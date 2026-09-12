#pragma once

#include <stdint.h>
#include <string.h>

/**
 * Letzte Session-Zusammenfassung (Stub) — RAM, optional LittleFS.
 * Arduino-frei.
 */
namespace ergo {

struct SessionSummary {
    char mode[16] = {};
    char workoutName[40] = {};
    char endReason[24] = {};
    uint32_t durationS = 0;
    uint8_t steps = 0;
    uint16_t interventions = 0;
    float avgDesiredW = 0.0f;
    uint32_t endedUnix = 0;
    bool valid = false;

    void clear() {
        *this = SessionSummary{};
    }
};

}  // namespace ergo
