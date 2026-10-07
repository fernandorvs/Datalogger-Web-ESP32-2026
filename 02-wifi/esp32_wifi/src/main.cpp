/*
    ─────────────────────────────────────────────────────────────────────

    MATERIA: EL831 Instrumentación Electrónica

    ─────────────────────────────────────────────────────────────────────

    MÓDULO:      Datalogger Web para ESP32
    PROYECTO:    02 - Logger WiFi (el instrumento es el servidor) · 2 canales
    HARDWARE:    Familia ESP32
                 DS18B20: DATA en GPIO 4, pull-up 4,7 kΩ a 3V3
                 Potenciómetro: extremos a 3V3 y GND, cursor a GPIO 34 (clásico) / GPIO 3 (C3) — ADC1
    FRAMEWORK:   Arduino / PlatformIO

    DESCRIPCIÓN:
    El ESP32 crea su PROPIA red WiFi (modo Access Point) y sirve la app
    de registro en http://192.168.4.1. La página (embebida en pagina.h)
    hace polling de:

        GET /medicion → {"seq":123,"temp_c":24.31,"pote_pct":57.5}

    Dos canales: la temperatura del DS18B20 y la posición del
    potenciómetro leída por el ADC. En JSON, agregar un canal = agregar
    un par nombre-valor: los clientes viejos ni se enteran.

    IMPORTANTE: con la radio WiFi encendida solo funciona ADC1
    (GPIO 32-39 en el ESP32 clásico) — por eso el pote va en GPIO 34.

    La misma medición sale por el puerto serie, como testigo.
    Con SIMULAR = true no hace falta ningún sensor.

    ─────────────────────────────────────────────────────────────────────
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
// la pagina web (pagina.html) la embebe PlatformIO en el build:
// board_build.embed_txtfiles la convierte en este simbolo, ya en flash
extern const char PAGINA[] asm("_binary_pagina_html_start");

// ---------- red propia (Access Point) ----------
const char* AP_SSID = "EL831-Logger";   // cambiar por grupo: EL831-G3
const char* AP_PASS = "el831logger";    // minimo 8 caracteres

// ---------- modo de trabajo ----------
const bool SIMULAR = false;        // true: señales simuladas sin hardware

// ---------- hardware ----------
const int PIN_DS18B20 = 4;         // DATA del DS18B20 (pull-up 4,7 kΩ a 3V3)
// cursor del pote — SIEMPRE un pin de ADC1 (con la radio encendida ADC2 no anda)
#ifdef CONFIG_IDF_TARGET_ESP32C3
const int PIN_POTE = 3;            // ESP32-C3: ADC1_CH3 (GPIO 34 no existe aca)
#else
const int PIN_POTE = 34;           // ESP32 clasico: ADC1_CH6
#endif
const int N_PROM_ADC  = 16;        // muestras promediadas del ADC

OneWire oneWire(PIN_DS18B20);
DallasTemperature sensores(&oneWire);

WebServer servidor(80);

// última medición disponible para servir
unsigned long g_seq = 0;
float g_temp_c = NAN;
float g_pote_pct = 0;
unsigned long g_proximaMedicionMs = 0;

float medirTemperaturaC() {
  if (SIMULAR) {
    // temperatura que oscila 25 ± 3 °C en ~1 minuto, con ruido
    float fase = (millis() % 60000UL) / 60000.0 * TWO_PI;
    return 25.0 + 3.0 * sin(fase) + random(-10, 11) * 0.01;
  }
  sensores.requestTemperatures();  // bloquea ~750 ms (conversión a 12 bits)
  float t = sensores.getTempCByIndex(0);
  if (t < -100) return NAN;        // -127: sensor no detectado
  return t;
}

float medirPotePct() {
  if (SIMULAR) {
    // rampa triangular 0..100 % en ~90 s, con un poco de ruido
    unsigned long f = millis() % 90000UL;
    float pct = (f < 45000) ? f / 450.0 : (90000 - f) / 450.0;
    return constrain(pct + random(-10, 11) * 0.05, 0.0, 100.0);
  }
  float suma_mv = 0;
  for (int i = 0; i < N_PROM_ADC; i++) suma_mv += analogReadMilliVolts(PIN_POTE);
  float pct = (suma_mv / N_PROM_ADC) / 3300.0 * 100.0;
  return constrain(pct, 0.0, 100.0);
}

// manifest + icono: con esto "Agregar a pantalla de inicio" crea un acceso
// directo con nombre e icono propios. (La instalacion PWA completa requiere
// HTTPS — contexto seguro, el mismo limite de Web Bluetooth y mixed content.)
const char MANIFEST[] PROGMEM = R"deloro({
  "name": "Logger WiFi \u00b7 EL831",
  "short_name": "Logger WiFi",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#f9f9f7",
  "theme_color": "#16213a",
  "icons": [{ "src": "/icono.svg", "sizes": "any", "type": "image/svg+xml" }]
})deloro";

const char ICONO[] PROGMEM = R"deloro(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512">
<rect width="512" height="512" fill="#16213A"/>
<polyline points="88,312 168,312 208,178 264,372 314,244 354,286 424,286" fill="none" stroke="#D95926" stroke-width="34" stroke-linecap="round" stroke-linejoin="round"/>
<circle cx="424" cy="286" r="27" fill="#3987E5"/>
</svg>)deloro";

void atenderRaiz() {
  servidor.send_P(200, "text/html", PAGINA);
}

void atenderMedicion() {
  if (isnan(g_temp_c)) {
    servidor.send(503, "application/json",
                  "{\"error\":\"sin medicion valida (DS18B20 no detectado)\"}");
    return;
  }
  char json[96];
  snprintf(json, sizeof(json),
           "{\"seq\":%lu,\"temp_c\":%.2f,\"pote_pct\":%.1f}",
           g_seq, g_temp_c, g_pote_pct);
  servidor.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  sensores.begin();
  analogSetPinAttenuation(PIN_POTE, ADC_11db);   // rango completo del ADC

  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("Red propia creada: ");
  Serial.println(AP_SSID);
  Serial.print("Conectarse a esa red y abrir: http://");
  Serial.println(WiFi.softAPIP());   // 192.168.4.1

  servidor.on("/", atenderRaiz);
  servidor.on("/medicion", atenderMedicion);
  servidor.on("/manifest.json", []() {
    servidor.send_P(200, "application/manifest+json", MANIFEST);
  });
  servidor.on("/icono.svg", []() {
    servidor.send_P(200, "image/svg+xml", ICONO);
  });
  servidor.begin();
}

void loop() {
  servidor.handleClient();

  // medir una vez por segundo sin frenar el servidor con delay()
  if (millis() >= g_proximaMedicionMs) {
    g_proximaMedicionMs = millis() + 1000;
    float t = medirTemperaturaC();
    g_pote_pct = medirPotePct();
    if (isnan(t)) {
      g_temp_c = NAN;
      Serial.println("ERROR: DS18B20 no detectado — revisar conexion y pull-up");
    } else {
      g_seq++;
      g_temp_c = t;
      // testigo por serie: temp_c, pote_pct
      Serial.print(t, 2);
      Serial.print(", ");
      Serial.println(g_pote_pct, 1);
    }
  }
}
