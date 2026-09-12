#include "core/SessionTracker.h"

#include "core/Zone.h"

namespace ergo {

void SessionTracker::begin(const SessionTrackerConfig& cfg) {
    cfg_ = cfg;
    if (cfg_.autoPauseAfterMs < 2000) cfg_.autoPauseAfterMs = 2000;
    if (cfg_.freezeToLevelAfterMs < 5000) cfg_.freezeToLevelAfterMs = 5000;
    reset();
}

void SessionTracker::reset() {
    active_ = false;
    paused_ = false;
    startMs_ = 0;
    lastTickMs_ = 0;
    zeroSinceMs_ = 0;
    pauseAccumMs_ = 0;
    pauseAtMs_ = 0;
    hrLostSinceMs_ = 0;
    powerSum_ = 0.0;
    powerN_ = 0;
    workJ_ = 0.0;
    desiredSum_ = 0.0;
    desiredN_ = 0;
    hrSum_ = 0;
    hrN_ = 0;
    leadHr_ = false;
    ftpW_ = 0;
    hrMax_ = 0;
    curZone_ = 0;
    for (uint8_t i = 0; i < kPowerZones; i++) zoneMs_[i] = 0;
    cur_.clear();
}

void SessionTracker::setZoneBasis(bool leadHr, uint16_t ftpW, uint8_t hrMax) {
    leadHr_ = leadHr;
    ftpW_ = ftpW;
    hrMax_ = hrMax;
    if (active_) {
        cur_.leadHr = leadHr_;
        cur_.zoneCount = leadHr_ ? kHrZones : kPowerZones;
    }
}

void SessionTracker::start(uint32_t nowMs, const char* mode, const char* workoutName,
                           const char* profileId) {
    const bool lead = leadHr_;
    const uint16_t ftp = ftpW_;
    const uint8_t hrm = hrMax_;
    reset();
    leadHr_ = lead;
    ftpW_ = ftp;
    hrMax_ = hrm;
    active_ = true;
    startMs_ = nowMs ? nowMs : 1;
    lastTickMs_ = startMs_;
    cur_.valid = true;
    cur_.leadHr = leadHr_;
    cur_.zoneCount = leadHr_ ? kHrZones : kPowerZones;
    if (mode) {
        strncpy(cur_.mode, mode, sizeof(cur_.mode) - 1);
        cur_.mode[sizeof(cur_.mode) - 1] = 0;
    }
    if (workoutName) {
        strncpy(cur_.workoutName, workoutName, sizeof(cur_.workoutName) - 1);
        cur_.workoutName[sizeof(cur_.workoutName) - 1] = 0;
    }
    if (profileId) {
        strncpy(cur_.profileId, profileId, sizeof(cur_.profileId) - 1);
        cur_.profileId[sizeof(cur_.profileId) - 1] = 0;
    }
}

SessionSummary SessionTracker::end(uint32_t nowMs, const char* reason) {
    if (!active_) {
        return cur_;
    }
    if (paused_ && pauseAtMs_ > 0 && nowMs >= pauseAtMs_) {
        pauseAccumMs_ += (nowMs - pauseAtMs_);
        pauseAtMs_ = 0;
    }
    accumulate_(nowMs, 0.0f, 0, false);
    cur_.durationS = elapsedActiveS(nowMs);
    cur_.pausedS = pauseAccumMs_ / 1000UL;
    cur_.endReason[0] = 0;
    if (reason) {
        strncpy(cur_.endReason, reason, sizeof(cur_.endReason) - 1);
        cur_.endReason[sizeof(cur_.endReason) - 1] = 0;
    }
    if (powerN_ > 0) cur_.avgPowerW = (float)(powerSum_ / (double)powerN_);
    if (desiredN_ > 0) cur_.avgDesiredW = (float)(desiredSum_ / (double)desiredN_);
    cur_.workKj = (float)(workJ_ / 1000.0);
    if (hrN_ > 0) cur_.hrAvg = (uint8_t)(hrSum_ / hrN_);
    cur_.leadHr = leadHr_;
    cur_.zoneCount = leadHr_ ? kHrZones : kPowerZones;
    cur_.valid = true;
    active_ = false;
    paused_ = false;
    return cur_;
}

uint32_t SessionTracker::elapsedActiveS(uint32_t nowMs) const {
    if (!active_ || startMs_ == 0) return cur_.durationS;
    uint32_t wall = (nowMs >= startMs_) ? (nowMs - startMs_) : 0;
    uint32_t paused = pauseAccumMs_;
    if (paused_ && pauseAtMs_ > 0 && nowMs >= pauseAtMs_) paused += (nowMs - pauseAtMs_);
    if (wall > paused) return (wall - paused) / 1000UL;
    return 0;
}

void SessionTracker::noteIntervention() {
    if (active_) cur_.interventions++;
}

void SessionTracker::noteDesiredW(float desiredW) {
    if (!active_ || paused_ || desiredW <= 0.0f) return;
    desiredSum_ += desiredW;
    desiredN_++;
    cur_.avgDesiredW = (float)(desiredSum_ / (double)desiredN_);
}

void SessionTracker::retagMode(const char* mode) {
    if (!active_ || !mode) return;
    strncpy(cur_.mode, mode, sizeof(cur_.mode) - 1);
    cur_.mode[sizeof(cur_.mode) - 1] = 0;
}

void SessionTracker::noteHrLost(uint32_t nowMs, bool lost) {
    if (!lost) {
        hrLostSinceMs_ = 0;
        return;
    }
    if (hrLostSinceMs_ == 0) hrLostSinceMs_ = nowMs ? nowMs : 1;
}

bool SessionTracker::freezeTimedOut(uint32_t nowMs) const {
    if (!active_ || hrLostSinceMs_ == 0) return false;
    return nowMs >= hrLostSinceMs_ && (nowMs - hrLostSinceMs_) >= cfg_.freezeToLevelAfterMs;
}

void SessionTracker::accumulate_(uint32_t nowMs, float watt, uint8_t hr, bool liveData) {
    float dtS = 0.0f;
    if (lastTickMs_ > 0 && nowMs >= lastTickMs_) {
        dtS = (float)(nowMs - lastTickMs_) / 1000.0f;
        if (dtS > 3.0f) dtS = 3.0f;
    }
    lastTickMs_ = nowMs ? nowMs : 1;
    if (!active_ || paused_) return;

    if (liveData) {
        uint8_t z = 0;
        if (leadHr_) {
            z = zoneFromHr(hr, hrMax_);
        } else if (watt > 0.0f) {
            z = zoneFromPowerW(watt, ftpW_);
        }
        curZone_ = z;
    }

    if (dtS <= 0.0f || !liveData) return;

    if (watt > 0.0f) {
        powerSum_ += watt;
        powerN_++;
        workJ_ += (double)watt * (double)dtS;
    }
    if (hr > 0) {
        hrSum_ += hr;
        hrN_++;
        if (hr > cur_.hrMax) cur_.hrMax = hr;
    }
    if (curZone_ >= 1 && curZone_ <= cur_.zoneCount) {
        const uint32_t addMs = (uint32_t)(dtS * 1000.0f + 0.5f);
        zoneMs_[curZone_ - 1] += addMs;
        cur_.zoneTimeS[curZone_ - 1] = zoneMs_[curZone_ - 1] / 1000UL;
    }
}

void SessionTracker::tick(uint32_t nowMs, float rpm, float watt, uint8_t hr, bool liveData) {
    if (!active_) return;

    const bool moving = liveData && rpm >= cfg_.cadenceMinRpm;
    if (moving) {
        zeroSinceMs_ = 0;
        if (paused_) {
            if (pauseAtMs_ > 0 && nowMs >= pauseAtMs_) pauseAccumMs_ += (nowMs - pauseAtMs_);
            pauseAtMs_ = 0;
            paused_ = false;
        }
    } else if (liveData) {
        if (zeroSinceMs_ == 0) zeroSinceMs_ = nowMs ? nowMs : 1;
        if (!paused_ && nowMs >= zeroSinceMs_ &&
            (nowMs - zeroSinceMs_) >= cfg_.autoPauseAfterMs) {
            paused_ = true;
            pauseAtMs_ = nowMs ? nowMs : 1;
            cur_.autoPauses++;
        }
    }

    accumulate_(nowMs, watt, hr, liveData && !paused_);
    cur_.durationS = elapsedActiveS(nowMs);
    cur_.pausedS = pauseAccumMs_ / 1000UL;
    if (paused_ && pauseAtMs_ > 0 && nowMs >= pauseAtMs_)
        cur_.pausedS = (pauseAccumMs_ + (nowMs - pauseAtMs_)) / 1000UL;
    if (powerN_ > 0) cur_.avgPowerW = (float)(powerSum_ / (double)powerN_);
    cur_.workKj = (float)(workJ_ / 1000.0);
    if (hrN_ > 0) cur_.hrAvg = (uint8_t)(hrSum_ / hrN_);
}

}  // namespace ergo
