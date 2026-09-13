#pragma once

#include <stddef.h>
#include <stdint.h>

#include "control/WorkoutJson.h"

/**
 * Minimaler Zwift-.zwo-Import → kompaktes Workout-JSON.
 * Unterstuetzt: SteadyState, Warmup, Cooldown, Ramp, IntervalsT, FreeRide.
 * Arduino-frei / hosttestbar.
 */
namespace ergo {

/**
 * Konvertiert .zwo-XML in Workout-JSON (interval/ramp kompakt).
 * @return Bytes geschrieben (ohne NUL) oder 0 bei Fehler.
 */
size_t zwoToJson(const char* xml, char* buf, size_t bufLen, char* err, size_t errLen);

}  // namespace ergo
