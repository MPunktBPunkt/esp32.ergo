#pragma once

#include <stddef.h>
#include <stdint.h>

#include "control/WorkoutEngine.h"

/**
 * Steady-Workout als kompaktes JSON (Pflichtenheft-Form).
 * `type:steady`, expandierende `type:interval` und `type:ramp`.
 * Arduino-frei — Hosttests ohne ArduinoJson.
 */
namespace ergo {

struct WorkoutDoc {
    static constexpr uint8_t kMaxTags = 4;
    static constexpr uint8_t kTagLen = 17;

    char id[24] = {};
    char name[40] = {};
    /**
     * Ziel-Tag, an Profile.TrainingGoal angelehnt:
     * "", "fitness", "fatloss", "reha", "performance".
     */
    char goal[16] = {};
    /** Favorit — bei Builtins ueber Meta-Datei, bei FS-Dateien im JSON. */
    bool favorite = false;
    /** Freie Tags (max. 4 × 16 Zeichen). Builtins: meta.json, FS: JSON. */
    char tags[kMaxTags][kTagLen] = {};
    uint8_t tagCount = 0;
    /**
     * Auto-Pause nach N Sekunden ohne Kadenz.
     * 0 = Session-Default (10 s).
     */
    uint16_t autoPauseS = 0;
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

/** Builtin-Ziel-Tag (kann leer sein). */
const char* workoutBuiltinGoal(uint8_t index);

/** Puffer fuer Serialisierung (16 Schritte + Header). */
static constexpr size_t kWorkoutJsonBuf = 4096;

/** Builtin-Katalog: id → Doc. */
bool workoutBuiltinById(const char* id, WorkoutDoc& out);
/** Anzahl Builtins. */
uint8_t workoutBuiltinCount();
/** Builtin-ID an Index. */
const char* workoutBuiltinId(uint8_t index);
const char* workoutBuiltinName(uint8_t index);

}  // namespace ergo
