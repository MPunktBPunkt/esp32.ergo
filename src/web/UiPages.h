#pragma once

/**
 * UI-Quelle der Wahrheit: web/index.html
 * Gebaut via tools/pack_ui.py → include/UiPagesGz.h (gzip PROGMEM).
 *
 * OTA-Rueckweg: die gzip-Seite enthaelt weiterhin das native OTA-Formular;
 * ohne JS bleibt der Upload bedienbar (body.js / Formular).
 */
#include "UiPagesGz.h"
