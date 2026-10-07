#!/usr/bin/env bash
# Genera los PDF de slides a partir de los HTML de esta carpeta.
# Requiere Chromium/Chrome. Uso:  ./gen_slides.sh [ruta-a-chromium]
set -e
cd "$(dirname "$0")"
CHROME="${1:-$(command -v chromium || command -v chromium-browser || command -v google-chrome)}"

"$CHROME" --headless --disable-gpu --no-sandbox --no-pdf-header-footer \
  --virtual-time-budget=15000 \
  --print-to-pdf="../01-ble/Slides_Logger_BLE.pdf" slides-01-ble.html
"$CHROME" --headless --disable-gpu --no-sandbox --no-pdf-header-footer \
  --virtual-time-budget=15000 \
  --print-to-pdf="../02-wifi/Slides_Logger_WiFi.pdf" slides-02-wifi.html

echo "PDFs generados en 01-ble/ y 02-wifi/"
