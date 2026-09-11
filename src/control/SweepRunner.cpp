#include "SweepRunner.h"

namespace ergo {

/** Nicht oefter als so oft einen Stufen-Write versuchen. Die Rampe des
 *  Limiters lehnt zwischendurch mit `Defer` ab; jeden Durchlauf erneut zu
 *  schreiben waere Funkverkehr ohne Wirkung. */
static constexpr uint32_t kSetRetryMs = 500;

const char* sweepStateName(SweepState s) {
    switch (s) {
        case SweepState::Idle: return "IDLE";
        case SweepState::Settle: return "SETTLE";
        case SweepState::Measure: return "MEASURE";
        case SweepState::Done: return "DONE";
        case SweepState::Aborted: return "ABORTED";
        default: return "UNKNOWN";
    }
}

// Stufennummern (1-basiert) der beiden Plaene aus NACHTESTS.md.
static const uint8_t kFull[] = {1, 2, 4, 6, 8, 10, 12, 14, 16};
static const uint8_t kCoarse[] = {4, 8, 12, 16};

SweepPlan SweepRunner::planFor(uint8_t levelCount, int16_t minTenths, uint16_t stepTenths,
                               float targetRpm, bool coarse) {
    SweepPlan p;
    p.targetRpm = targetRpm > 0.0f ? targetRpm : 60.0f;
    if (stepTenths == 0) stepTenths = 10;
    const uint8_t* list = coarse ? kCoarse : kFull;
    const uint8_t n = coarse ? (uint8_t)(sizeof(kCoarse) / sizeof(kCoarse[0]))
                             : (uint8_t)(sizeof(kFull) / sizeof(kFull[0]));
    for (uint8_t i = 0; i < n && p.count < kMapMaxLevels; i++) {
        // Stufen jenseits des Geraetebereichs fallen weg. Ein Geraet mit zwölf
        // Stufen soll einen kuerzeren Sweep fahren, keinen kaputten.
        if (list[i] > levelCount) continue;
        p.levels[p.count++] = (int16_t)(minTenths + (int32_t)(list[i] - 1) * stepTenths);
    }
    return p;
}

bool SweepRunner::start(const SweepPlan& plan, uint32_t nowMs) {
    if (plan.count == 0) return false;
    reset();
    plan_ = plan;
    if (plan_.settleMs < 1000) plan_.settleMs = 1000;
    if (plan_.windowMs < 1000) plan_.windowMs = 1000;
    if (plan_.minRpm < 1.0f) plan_.minRpm = 1.0f;
    if (plan_.maxDriftPct < 1.0f) plan_.maxDriftPct = 1.0f;
    state_ = SweepState::Settle;
    idx_ = 0;
    needSet_ = true;
    lastSetTryMs_ = nowMs - kSetRetryMs;
    phaseStartMs_ = nowMs;
    return true;
}

void SweepRunner::cancel(uint32_t nowMs) {
    (void)nowMs;
    if (state_ == SweepState::Settle || state_ == SweepState::Measure) {
        state_ = SweepState::Aborted;
        abortReason_ = "abgebrochen";
    }
    needSet_ = false;
}

void SweepRunner::reset() {
    plan_ = SweepPlan{};
    state_ = SweepState::Idle;
    idx_ = 0;
    needSet_ = false;
    lastSetTryMs_ = 0;
    phaseStartMs_ = 0;
    lowSinceMs_ = 0;
    low_ = false;
    abortReason_ = "";
    n_ = 0;
    sumRpm_ = sumWatt_ = 0.0f;
    minRpm_ = maxRpm_ = 0.0f;
    points_ = 0;
    for (uint8_t i = 0; i < kMapMaxLevels; i++) pts_[i] = SweepPoint{};
}

int16_t SweepRunner::currentLevelTenths() const {
    if (idx_ >= plan_.count) return -1;
    return plan_.levels[idx_];
}

const SweepPoint& SweepRunner::point(uint8_t i) const {
    static const SweepPoint empty;
    if (i >= points_) return empty;
    return pts_[i];
}

uint8_t SweepRunner::validCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < points_; i++) {
        if (pts_[i].valid) n++;
    }
    return n;
}

uint8_t SweepRunner::progressPct() const {
    if (plan_.count == 0) return 0;
    if (state_ == SweepState::Done) return 100;
    return (uint8_t)((uint16_t)points_ * 100u / plan_.count);
}

uint32_t SweepRunner::phaseRemainingMs(uint32_t nowMs) const {
    if (state_ != SweepState::Settle && state_ != SweepState::Measure) return 0;
    if (needSet_) return plan_.settleMs;
    const uint32_t len = (state_ == SweepState::Settle) ? plan_.settleMs : plan_.windowMs;
    const uint32_t gone = nowMs - phaseStartMs_;
    return gone >= len ? 0u : (len - gone);
}

void SweepRunner::noteLevelSet(uint32_t nowMs) {
    if (!needSet_) return;
    needSet_ = false;
    phaseStartMs_ = nowMs;
    state_ = SweepState::Settle;
}

// ──────────────────────────────────────────────────────────────── Ablauf

void SweepRunner::finishWindow(uint32_t nowMs) {
    SweepPoint p;
    p.levelTenths = plan_.levels[idx_];
    p.samples = n_;
    if (n_ == 0) {
        p.valid = false;
        p.reason = "keine Daten";
    } else {
        p.meanRpm = sumRpm_ / (float)n_;
        p.meanWatt = sumWatt_ / (float)n_;
        p.rpmMin = minRpm_;
        p.rpmMax = maxRpm_;
        const float span = maxRpm_ - minRpm_;
        const float driftPct = (p.meanRpm > 0.0f) ? (span / p.meanRpm * 100.0f) : 999.0f;
        if (p.meanRpm < plan_.minRpm) {
            p.valid = false;
            p.reason = "Kadenz zu niedrig";
        } else if (driftPct > plan_.maxDriftPct) {
            p.valid = false;
            p.reason = "Kadenz nicht gehalten";
        } else {
            p.valid = true;
        }
    }
    if (points_ < kMapMaxLevels) pts_[points_++] = p;
    advance(nowMs);
}

void SweepRunner::advance(uint32_t nowMs) {
    n_ = 0;
    sumRpm_ = sumWatt_ = 0.0f;
    minRpm_ = maxRpm_ = 0.0f;
    idx_++;
    if (idx_ >= plan_.count) {
        state_ = SweepState::Done;
        needSet_ = false;
        return;
    }
    state_ = SweepState::Settle;
    needSet_ = true;
    lastSetTryMs_ = nowMs - kSetRetryMs;
    phaseStartMs_ = nowMs;
}

SweepRunner::Tick SweepRunner::tick(uint32_t nowMs, float rpm, float watt, bool fresh) {
    Tick t;
    // Stop wird genau beim Uebergang nach Done oder Aborted gemeldet, nicht
    // hier. Danach ist der Runner still — ein zweites Stop je Durchlauf waere
    // harmlos, aber es wuerde das Steuer-Journal mit Rauschen fuellen.
    if (state_ == SweepState::Idle || state_ == SweepState::Done ||
        state_ == SweepState::Aborted) {
        return t;
    }

    // Abbruch, wenn nicht mehr getreten wird. Das ist kein Messfehler, das ist
    // ein Mensch, der aufgehoert hat; ein weiterlaufender Sweep wuerde dann
    // Stufen stellen, die niemand erwartet.
    const bool lowNow = !fresh || rpm < plan_.minRpm;
    if (lowNow) {
        if (!low_) {
            low_ = true;
            lowSinceMs_ = nowMs;
        } else if (nowMs - lowSinceMs_ >= plan_.abortAfterMs) {
            state_ = SweepState::Aborted;
            abortReason_ = fresh ? "Kadenz weg" : "keine Daten vom Bike";
            needSet_ = false;
            t.action = Tick::Do::Stop;
            return t;
        }
    } else {
        low_ = false;
    }

    if (needSet_) {
        if (nowMs - lastSetTryMs_ >= kSetRetryMs) {
            lastSetTryMs_ = nowMs;
            t.action = Tick::Do::SetLevel;
            t.levelTenths = plan_.levels[idx_];
        }
        return t;
    }

    if (state_ == SweepState::Settle) {
        if (nowMs - phaseStartMs_ >= plan_.settleMs) {
            state_ = SweepState::Measure;
            phaseStartMs_ = nowMs;
            n_ = 0;
            sumRpm_ = sumWatt_ = 0.0f;
            minRpm_ = maxRpm_ = 0.0f;
        }
        return t;
    }

    // Messfenster
    if (fresh) {
        if (n_ == 0) {
            minRpm_ = maxRpm_ = rpm;
        } else {
            if (rpm < minRpm_) minRpm_ = rpm;
            if (rpm > maxRpm_) maxRpm_ = rpm;
        }
        sumRpm_ += rpm;
        sumWatt_ += watt;
        if (n_ < 0xFFFF) n_++;
    }
    if (nowMs - phaseStartMs_ >= plan_.windowMs) {
        finishWindow(nowMs);
        if (state_ == SweepState::Done) {
            t.action = Tick::Do::Stop;
        }
    }
    return t;
}

}  // namespace ergo
