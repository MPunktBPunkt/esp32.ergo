#pragma once

#include <stddef.h>
#include <stdint.h>

#include "control/WorkoutEngine.h"

/**
 * Steady-Workout als kompaktes JSON (Pflichtenheft-Form, nur type=steady).
 * Arduino-frei — Hosttests ohne ArduinoJson.
 */
namespace ergo {

struct WorkoutDoc {
    char id[24] = {};
    char name[40] = {};
    WorkoutStep steps[WorkoutEngine::kMaxSteps];
    uint8_t stepCount = 0;
    struct Progression {
        bool enabled = false;
        uint16_t stepS = 60;
        uint32_t maxS = 1800;
        uint8_t stepIndex = 1;
        uint32_t baseDurationS = 0;
    } progression;
};

/** true wenn JSON gelesen und mindestens ein Schritt vorhanden. */
bool workoutParseJson(const char* json, WorkoutDoc& out, char* err, size_t errLen);

/** Schreibt JSON; return Bytes geschrieben (ohne NUL) oder 0 bei zu klein. */
size_t workoutWriteJson(const WorkoutDoc& doc, char* buf, size_t bufLen);

/** Builtin-Katalog: id → Doc. */
bool workoutBuiltinById(const char* id, WorkoutDoc& out);
/** Anzahl Builtins. */
uint8_t workoutBuiltinCount();
/** Builtin-ID an Index. */
const char* workoutBuiltinId(uint8_t index);
const char* workoutBuiltinName(uint8_t index);

}  // namespace ergo
