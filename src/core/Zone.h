#pragma once

#include <stdint.h>

/**
 * Leistungs- und Pulszonen — Arduino-frei.
 *
 * Leistung: 7 Zonen nach % FTP (WEBINTERFACE.md §2).
 * Puls: 5 Zonen nach % HRmax (gleiche Farben Z1–Z5).
 * Führende Zone kommt aus dem Profil (Power vs HR).
 */
namespace ergo {

static constexpr uint8_t kPowerZones = 7;
static constexpr uint8_t kHrZones = 5;

struct ZoneInfo {
    uint8_t index = 0;  // 1..7 bzw. 1..5; 0 = unbekannt / keine Basis
    const char* code = "";
    const char* name = "";
    /** CSS-Hex ohne #, z. B. "3FB8B0". */
    const char* colorHex = "8A94A6";
};

/** Power-Zone 1..7 aus Watt und FTP. FTP 0 → 0. */
uint8_t zoneFromPowerW(float watt, uint16_t ftpW);

/** HR-Zone 1..5 aus BPM und HRmax. HRmax 0 → 0. */
uint8_t zoneFromHr(uint8_t bpm, uint8_t hrMax);

ZoneInfo powerZoneInfo(uint8_t z);
ZoneInfo hrZoneInfo(uint8_t z);

/** Führende Zone: Power oder HR laut leadHr. */
ZoneInfo leadingZoneInfo(bool leadHr, float watt, uint16_t ftpW, uint8_t bpm, uint8_t hrMax);

}  // namespace ergo
