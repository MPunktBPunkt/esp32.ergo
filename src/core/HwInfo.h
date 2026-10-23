#pragma once

/**
 * Modul-Identifikation ohne esptool: eFuse BLK1 (dieselben Felder, die
 * esptool als „Embedded PSRAM 2MB (AP_3v3)“ druckt) plus JEDEC-ID des
 * Flash-Chips. Funktioniert auch, wenn PSRAM im Build aus ist.
 *
 * 1:1 aus hu-probe `core/HwInfo` — dort am YD-ESP32-S3 als N8R2 gemessen.
 * `begin()` einmalig: Flash-ID-Lesen haelt die Caches kurz an.
 */

#include <ArduinoJson.h>

namespace HwInfo {

void begin();

/** Geschaetzte Variante, z. B. "N8R2"; "?" wenn die Flash-Groesse fehlt. */
const char* moduleGuess();

uint32_t psramBytesEfuse();
uint32_t flashBytesReal();

/**
 * Modul, Package, Flash, PSRAM und Roh-eFuse. Enthaelt configured vs. real
 * Flash und das noetige `memory_type` (`qio_qspi` / `qio_opi`).
 */
void toJson(JsonObject o);

}  // namespace HwInfo
