#pragma once

#include <stdint.h>
#include <string.h>

/**
 * Letzte Session-Zusammenfassung — RAM / JSON-Zeile.
 * Arduino-frei.
 */
namespace ergo {

struct SessionSummary {
    char mode[16] = {};
    char workoutName[40] = {};
    char profileId[16] = {};
    char endReason[24] = {};
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
    uint32_t endedUnix = 0;
    bool valid = false;

    void clear() { *this = SessionSummary{}; }
};

}  // namespace ergo
