#include "control/RehaController.h"

namespace ergo {

void RehaController::begin(const RehaControllerConfig& cfg) {
    cfg_ = cfg;
    if (cfg_.minPowerW < 0.0f) cfg_.minPowerW = 0.0f;
    if (cfg_.softCutGain < 0.0f) cfg_.softCutGain = 0.0f;
    if (cfg_.hardCutGain < 0.0f) cfg_.hardCutGain = 0.0f;
    if (cfg_.restorePerS < 0.0f) cfg_.restorePerS = 0.0f;
    if (cfg_.lostAfterMs < 1000) cfg_.lostAfterMs = 1000;
    reset();
}

void RehaController::reset() {
    effectiveW_ = desiredW_;
    if (effectiveW_ < cfg_.minPowerW) effectiveW_ = cfg_.minPowerW;
    armMs_ = 0;
    lastTickMs_ = 0;
    lastHrMs_ = 0;
    elapsedS_ = 0;
    remainingS_ = durationS_;
    sawHr_ = false;
    lost_ = false;
    finished_ = false;
    capActive_ = false;
    interventions_ = 0;
}

void RehaController::setDesiredW(float watt) {
    if (watt < 0.0f) watt = 0.0f;
    desiredW_ = watt;
    if (effectiveW_ > desiredW_) effectiveW_ = desiredW_;
    if (effectiveW_ < cfg_.minPowerW && desiredW_ >= cfg_.minPowerW)
        effectiveW_ = cfg_.minPowerW;
}

void RehaController::setHrLimits(uint8_t softBpm, uint8_t hardBpm) {
    if (hardBpm < 40) hardBpm = 40;
    if (hardBpm > 220) hardBpm = 220;
    if (softBpm == 0 || softBpm > hardBpm) softBpm = (hardBpm > 5) ? (uint8_t)(hardBpm - 5) : hardBpm;
    if (softBpm < 40) softBpm = 40;
    hrSoft_ = softBpm;
    hrMax_ = hardBpm;
}

void RehaController::setDurationS(uint32_t sec) {
    durationS_ = sec;
}

RehaController::Tick RehaController::tick(uint32_t nowMs, uint8_t hrBpm, bool hrFresh) {
    Tick t;
    t.desiredW = desiredW_;
    t.effectiveW = effectiveW_;
    t.capActive = capActive_;
    t.interventions = interventions_;
    t.lossPolicy = lossPolicy_;
    t.finished = finished_;
    t.lost = lost_;

    if (armMs_ == 0) armMs_ = nowMs ? nowMs : 1;

    float dtS = 0.0f;
    if (lastTickMs_ > 0 && nowMs >= lastTickMs_) {
        dtS = (float)(nowMs - lastTickMs_) / 1000.0f;
        if (dtS > 3.0f) dtS = 3.0f;
    }
    lastTickMs_ = nowMs ? nowMs : 1;

    const uint32_t elapsedMs = (nowMs >= armMs_) ? (nowMs - armMs_) : 0;
    elapsedS_ = elapsedMs / 1000;
    t.elapsedS = elapsedS_;
    if (durationS_ > 0) {
        if (elapsedS_ >= durationS_) {
            finished_ = true;
            remainingS_ = 0;
            t.finished = true;
            t.remainingS = 0;
            effectiveW_ = 0.0f;
            t.effectiveW = 0.0f;
            return t;
        }
        remainingS_ = durationS_ - elapsedS_;
        t.remainingS = remainingS_;
    } else {
        remainingS_ = 0;
        t.remainingS = 0;
    }

    if (hrFresh && hrBpm > 0) {
        lastHrMs_ = nowMs ? nowMs : 1;
        sawHr_ = true;
        lost_ = false;
    } else if (sawHr_ && nowMs >= lastHrMs_ && (nowMs - lastHrMs_) >= cfg_.lostAfterMs) {
        lost_ = true;
    } else if (!sawHr_ && nowMs >= armMs_ && (nowMs - armMs_) >= cfg_.lostAfterMs) {
        lost_ = true;
    }
    t.lost = lost_;
    if (lost_ || dtS <= 0.0f) {
        if (lost_) {
            capActive_ = false;
            t.capActive = false;
        }
        t.effectiveW = effectiveW_;
        return t;
    }

    // Ohne frische vertrauenswuerdige Quelle nicht regeln (kein Cap auf altem BPM).
    if (!hrFresh) {
        t.effectiveW = effectiveW_;
        t.capActive = capActive_;
        t.interventions = interventions_;
        return t;
    }

    float ceilW = desiredW_;
    bool capping = false;
    if (hrBpm >= hrMax_) {
        capping = true;
        const float overSoft = (float)(hrMax_ > hrSoft_ ? (hrMax_ - hrSoft_) : 0);
        const float overHard = (float)(hrBpm - hrMax_);
        ceilW = desiredW_ - cfg_.softCutGain * overSoft - cfg_.hardCutGain * overHard;
    } else if (hrBpm >= hrSoft_) {
        capping = true;
        ceilW = desiredW_ - cfg_.softCutGain * (float)(hrBpm - hrSoft_);
    }

    if (ceilW < cfg_.minPowerW) ceilW = cfg_.minPowerW;
    if (ceilW > desiredW_) ceilW = desiredW_;

    if (capping) {
        if (!capActive_) {
            capActive_ = true;
            interventions_++;
        }
        if (effectiveW_ > ceilW) effectiveW_ = ceilW;
        else if (effectiveW_ < ceilW) {
            // Unter dem Soft-Ceiling darf langsam steigen, nie ueber ceilW.
            effectiveW_ += cfg_.restorePerS * dtS;
            if (effectiveW_ > ceilW) effectiveW_ = ceilW;
        }
    } else {
        capActive_ = false;
        if (effectiveW_ < desiredW_) {
            effectiveW_ += cfg_.restorePerS * dtS;
            if (effectiveW_ > desiredW_) effectiveW_ = desiredW_;
        } else if (effectiveW_ > desiredW_) {
            effectiveW_ = desiredW_;
        }
    }

    t.effectiveW = effectiveW_;
    t.capActive = capActive_;
    t.interventions = interventions_;
    return t;
}

}  // namespace ergo
