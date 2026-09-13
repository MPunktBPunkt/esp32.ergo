#pragma once

#include <stdint.h>
#include <string.h>

#include "core/Zone.h"

/**
 * Letzte Session-Zusammenfassung — RAM / JSON-Zeile.
 * Arduino-frei.
 */
namespace ergo {

struct SessionSummary {
    char mode[16] = {};
    char workoutName[40] = {};
    char workoutId[24] = {};
    char profileId[16] = {};
    char endReason[24] = {};
    /** 0 = keine Note; 1..10 empfundene Anstrengung. */
    uint8_t rpe = 0;
    char note[48] = {};
    uint32_t durationS = 0;
    uint32_t pausedS = 0;
    uint8_t steps = 0;
    uint16_t interventions = 0;
    uint16_t autoPauses = 0;
    float avgPowerW = 0.0f;
    float avgDesiredW = 0.0f;
    float workKj = 0.0f;
    uint8_t hrAvg = 0;
    uint8_t hrMax = 0;
    /** Sekunden je Zone (Z1…; max kPowerZones). */
    uint32_t zoneTimeS[kPowerZones] = {};
    uint8_t zoneCount = kPowerZones;
    bool leadHr = false;
    uint32_t endedUnix = 0;
    bool valid = false;

    void clear() { *this = SessionSummary{}; }
};

}  // namespace ergo
