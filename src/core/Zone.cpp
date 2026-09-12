#include "core/Zone.h"

namespace ergo {

uint8_t zoneFromPowerW(float watt, uint16_t ftpW) {
    if (ftpW == 0 || watt <= 0.0f) return 0;
    const float pct = 100.0f * watt / (float)ftpW;
    if (pct < 55.0f) return 1;
    if (pct < 76.0f) return 2;
    if (pct < 91.0f) return 3;
    if (pct < 106.0f) return 4;
    if (pct < 121.0f) return 5;
    if (pct < 151.0f) return 6;
    return 7;
}

uint8_t zoneFromPowerW(float watt, uint16_t ftpW, uint8_t prevZone) {
    const uint8_t raw = zoneFromPowerW(watt, ftpW);
    if (prevZone == 0 || raw == 0 || raw == prevZone) return raw;
    if (ftpW == 0) return raw;
    const float pct = 100.0f * watt / (float)ftpW;
    /* Untere Grenzen der Zonen 2..7 (Zone 1 startet bei 0). */
    static const float enter[] = {0.f, 0.f, 55.f, 76.f, 91.f, 106.f, 121.f, 151.f};
    constexpr float kH = 2.5f;
    if (raw > prevZone) {
        return (pct >= enter[raw] + kH) ? raw : prevZone;
    }
    return (pct < enter[prevZone] - kH) ? raw : prevZone;
}

uint8_t zoneFromHr(uint8_t bpm, uint8_t hrMax) {
    if (hrMax == 0 || bpm == 0) return 0;
    const float pct = 100.0f * (float)bpm / (float)hrMax;
    if (pct < 60.0f) return 1;
    if (pct < 70.0f) return 2;
    if (pct < 80.0f) return 3;
    if (pct < 90.0f) return 4;
    return 5;
}

uint8_t zoneFromHr(uint8_t bpm, uint8_t hrMax, uint8_t prevZone) {
    const uint8_t raw = zoneFromHr(bpm, hrMax);
    if (prevZone == 0 || raw == 0 || raw == prevZone) return raw;
    if (hrMax == 0) return raw;
    const float pct = 100.0f * (float)bpm / (float)hrMax;
    static const float enter[] = {0.f, 0.f, 60.f, 70.f, 80.f, 90.f};
    constexpr float kH = 2.0f;
    if (raw > prevZone) {
        return (pct >= enter[raw] + kH) ? raw : prevZone;
    }
    return (pct < enter[prevZone] - kH) ? raw : prevZone;
}

static ZoneInfo makeZone(uint8_t index, const char* code, const char* name, const char* color) {
    ZoneInfo z;
    z.index = index;
    z.code = code;
    z.name = name;
    z.colorHex = color;
    return z;
}

ZoneInfo powerZoneInfo(uint8_t z) {
    switch (z) {
        case 1:
            return makeZone(1, "Z1", "Aktive Erholung", "3FB8B0");
        case 2:
            return makeZone(2, "Z2", "Grundlage", "4CAF63");
        case 3:
            return makeZone(3, "Z3", "Tempo", "D8B23A");
        case 4:
            return makeZone(4, "Z4", "Schwelle", "E2802F");
        case 5:
            return makeZone(5, "Z5", "VO2max", "DE5334");
        case 6:
            return makeZone(6, "Z6", "Anaerob", "C9304A");
        case 7:
            return makeZone(7, "Z7", "Neuromuskulaer", "A63FB0");
        default:
            return makeZone(0, "", "", "8A94A6");
    }
}

ZoneInfo hrZoneInfo(uint8_t z) {
    switch (z) {
        case 1:
            return makeZone(1, "Z1", "Erholung", "3FB8B0");
        case 2:
            return makeZone(2, "Z2", "Grundlage", "4CAF63");
        case 3:
            return makeZone(3, "Z3", "Tempo", "D8B23A");
        case 4:
            return makeZone(4, "Z4", "Schwelle", "E2802F");
        case 5:
            return makeZone(5, "Z5", "Maximum", "DE5334");
        default:
            return makeZone(0, "", "", "8A94A6");
    }
}

ZoneInfo leadingZoneInfo(bool leadHr, float watt, uint16_t ftpW, uint8_t bpm, uint8_t hrMax) {
    if (leadHr) {
        return hrZoneInfo(zoneFromHr(bpm, hrMax));
    }
    return powerZoneInfo(zoneFromPowerW(watt, ftpW));
}

}  // namespace ergo
