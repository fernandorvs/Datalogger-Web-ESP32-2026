/*
    ─────────────────────────────────────────────────────────────────────

    MATERIA: EL831 Instrumentación Electrónica

    ─────────────────────────────────────────────────────────────────────

    MÓDULO:      Datalogger Web para ESP32
    PROYECTO:    01 - Logger BLE (Web Bluetooth) · 2 canales
    HARDWARE:    Familia ESP32
                 DS18B20: DATA en GPIO 4, pull-up 4,7 kΩ a 3V3
                 Potenciómetro: extremos a 3V3 y GND, cursor a GPIO 34 (clásico) / GPIO 3 (C3) — ADC1
    FRAMEWORK:   Arduino / PlatformIO

    DESCRIPCIÓN:
    El ESP32 publica DOS canales por Bluetooth Low Energy, en el servicio
    estándar "Environmental Sensing" (0x181A), uno por característica:

        0x2A6E Temperature   → sint16, centésimas de °C, little-endian
        0x2B04 Percentage 8  → uint8, resolución 0,5 % (valor = % × 2)

    La app web (docs/ del repo, servida por GitHub Pages) se suscribe a
    las notificaciones de ambas y registra los pares. Agregar un canal =
    agregar una característica: esa es la gracia de GATT.

    El potenciómetro se lee con analogReadMilliVolts() (ADC1: GPIO 34 no
    pelea con la radio). La misma medición sale por serie, como testigo.
    Con SIMULAR = true no hace falta ningún sensor.

    ─────────────────────────────────────────────────────────────────────
*/

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---------- identidad BLE ----------
const char* NOMBRE_BLE = "EL831-Logger";         // cambiar por grupo: EL831-G3
static BLEUUID UUID_SERVICIO((uint16_t)0x181A);     // Environmental Sensing
static BLEUUID UUID_TEMPERATURA((uint16_t)0x2A6E);  // Temperature (sint16, 0,01 °C)
static BLEUUID UUID_PORCENTAJE((uint16_t)0x2B04);   // Percentage 8 (uint8, 0,5 %)

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

BLECharacteristic* caracTemp = nullptr;
BLECharacteristic* caracPote = nullptr;
volatile bool hayCliente = false;

class Conexiones : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    hayCliente = true;
    Serial.println("# cliente BLE conectado");
  }
  void onDisconnect(BLEServer* srv) override {
    hayCliente = false;
    Serial.println("# cliente BLE desconectado — vuelvo a anunciar");
    srv->getAdvertising()->start();   // volver a ser visible
  }
};

float medirTemperaturaC() {
  if (SIMULAR) {
    // temperatura que oscila 25 ± 3 °C en ~1 minuto, con ruido
    float fase = (millis() % 60000UL) / 60000.0 * TWO_PI;
    return 25.0 + 3.0 * sin(fase) + random(-10, 11) * 0.01;
  }
  sensores.requestTemperatures();
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

void setup() {
  Serial.begin(115200);
  sensores.begin();
  analogSetPinAttenuation(PIN_POTE, ADC_11db);   // rango completo del ADC

  BLEDevice::init(NOMBRE_BLE);
  BLEServer* servidor = BLEDevice::createServer();
  servidor->setCallbacks(new Conexiones());

  BLEService* servicio = servidor->createService(UUID_SERVICIO);
  caracTemp = servicio->createCharacteristic(
      UUID_TEMPERATURA,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  caracTemp->addDescriptor(new BLE2902());   // CCCD: habilita suscripciones
  caracPote = servicio->createCharacteristic(
      UUID_PORCENTAJE,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  caracPote->addDescriptor(new BLE2902());
  servicio->start();

  BLEAdvertising* anuncio = BLEDevice::getAdvertising();
  anuncio->addServiceUUID(UUID_SERVICIO);    // la app filtra por este servicio
  anuncio->setScanResponse(true);
  anuncio->start();

  Serial.print("Anunciando por BLE como: ");
  Serial.println(NOMBRE_BLE);
}

void loop() {
  float t    = medirTemperaturaC();
  float pote = medirPotePct();

  if (isnan(t)) {
    Serial.println("ERROR: DS18B20 no detectado — revisar conexion y pull-up");
  } else {
    // testigo por serie: temp_c, pote_pct
    Serial.print(t, 2);
    Serial.print(", ");
    Serial.println(pote, 1);

    // 0x2B04: uint8 con resolución 0,5 % → valor = % × 2
    uint8_t medios = (uint8_t)lroundf(pote * 2.0f);
    caracPote->setValue(&medios, 1);
    if (hayCliente) caracPote->notify();

    // 0x2A6E: sint16, centésimas de °C, little-endian
    // (se notifica último: la app registra el par al llegar la temperatura)
    int16_t centesimas = (int16_t)lroundf(t * 100.0f);
    caracTemp->setValue((uint8_t*)&centesimas, sizeof(centesimas));
    if (hayCliente) caracTemp->notify();
  }
  delay(1000);
}
