#pragma once

#include <stdint.h>
#include <string.h>

#include "core/SessionSummary.h"

/**
 * Session-Zeit und Auto-Pause (Kadenz = 0).
 * Arduino-frei; Messwerte und Zeit kommen als Parameter.
 */
namespace ergo {

struct SessionTrackerConfig {
    /** Nach so vielen ms ohne Trittfrequenz → Pause. */
    uint32_t autoPauseAfterMs = 10000;
    /** Unter dieser rpm gilt „Stillstand". */
    float cadenceMinRpm = 5.0f;
    /** Freeze-Pulsverlust: nach Timeout auf MANUAL_LEVEL. */
    uint32_t freezeToLevelAfterMs = 30000;
};

class SessionTracker {
public:
    void begin(const SessionTrackerConfig& cfg = {});
    void reset();

    void start(uint32_t nowMs, const char* mode, const char* workoutName, const char* profileId);
    /** Beendet; Summary bleibt bis zum nächsten start lesbar. */
    SessionSummary end(uint32_t nowMs, const char* reason);

    void tick(uint32_t nowMs, float rpm, float watt, uint8_t hr, bool liveData);
    void noteIntervention();
    void noteHrLost(uint32_t nowMs, bool lost);
    void noteDesiredW(float desiredW);
    void retagMode(const char* mode);

    bool active() const { return active_; }
    bool paused() const { return paused_; }
    /** Während Auto-Pause keine neuen Last-Writes. */
    bool holdLoad() const { return active_ && paused_; }
    bool freezeTimedOut(uint32_t nowMs) const;

    const SessionSummary& peek() const { return cur_; }
    uint32_t elapsedActiveS(uint32_t nowMs) const;

private:
    void accumulate_(uint32_t nowMs, float watt, uint8_t hr, bool liveData);

    SessionTrackerConfig cfg_{};
    bool active_ = false;
    bool paused_ = false;
    uint32_t startMs_ = 0;
    uint32_t lastTickMs_ = 0;
    uint32_t zeroSinceMs_ = 0;
    uint32_t pauseAccumMs_ = 0;
    uint32_t pauseAtMs_ = 0;
    uint32_t hrLostSinceMs_ = 0;
    double powerSum_ = 0.0;
    uint32_t powerN_ = 0;
    double workJ_ = 0.0;
    double desiredSum_ = 0.0;
    uint32_t desiredN_ = 0;
    uint32_t hrSum_ = 0;
    uint32_t hrN_ = 0;
    SessionSummary cur_{};
};

}  // namespace ergo
