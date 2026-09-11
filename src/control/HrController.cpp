#include "control/HrController.h"

namespace ergo {

void HrController::begin(const HrControllerConfig& cfg) {
    cfg_ = cfg;
    if (cfg_.deadbandBpm < 0.0f) cfg_.deadbandBpm = 0.0f;
    if (cfg_.iGain < 0.0f) cfg_.iGain = 0.0f;
    if (cfg_.iLimitW < 0.0f) cfg_.iLimitW = 0.0f;
    if (cfg_.minPowerW < 0.0f) cfg_.minPowerW = 0.0f;
    if (cfg_.maxPowerW < cfg_.minPowerW) cfg_.maxPowerW = cfg_.minPowerW;
    if (cfg_.lostAfterMs < 1000) cfg_.lostAfterMs = 1000;
    reset();
}

void HrController::reset() {
    integralW_ = 0.0f;
    powerTargetW_ = cfg_.basePowerW;
    if (powerTargetW_ < cfg_.minPowerW) powerTargetW_ = cfg_.minPowerW;
    if (powerTargetW_ > cfg_.maxPowerW) powerTargetW_ = cfg_.maxPowerW;
    smoothHr_ = 0.0f;
    smoothInit_ = false;
    lost_ = false;
    sawHr_ = false;
    lastHrMs_ = 0;
    armMs_ = 0;
    lastTickMs_ = 0;
}

void HrController::setTargetHr(uint8_t bpm) {
    if (bpm > 0 && bpm < 40) bpm = 40;
    if (bpm > 220) bpm = 220;
    if (bpm != targetHr_) {
        targetHr_ = bpm;
        integralW_ = 0.0f;
        armMs_ = 0;
        lost_ = false;
    }
}

void HrController::setPowerLimits(float minW, float maxW) {
    if (minW < 0.0f) minW = 0.0f;
    if (maxW < minW) maxW = minW;
    cfg_.minPowerW = minW;
    cfg_.maxPowerW = maxW;
    if (powerTargetW_ < minW) powerTargetW_ = minW;
    if (powerTargetW_ > maxW) powerTargetW_ = maxW;
}

void HrController::setBasePowerW(float w) {
    cfg_.basePowerW = w;
}

HrController::Tick HrController::tick(uint32_t nowMs, uint8_t hrBpm, bool hrFresh,
                                      uint8_t hardMaxHr) {
    Tick t;
    t.lossPolicy = lossPolicy_;
    t.powerTargetW = powerTargetW_;

    if (!hasTarget()) return t;

    if (armMs_ == 0) armMs_ = nowMs ? nowMs : 1;

    float dtS = 0.0f;
    if (lastTickMs_ > 0 && nowMs >= lastTickMs_) {
        dtS = (float)(nowMs - lastTickMs_) / 1000.0f;
        if (dtS > 3.0f) dtS = 3.0f;
    }
    lastTickMs_ = nowMs;

    if (hrFresh && hrBpm > 0) {
        lastHrMs_ = nowMs ? nowMs : 1;
        sawHr_ = true;
        lost_ = false;
        if (!smoothInit_) {
            smoothHr_ = (float)hrBpm;
            smoothInit_ = true;
        } else if (dtS > 0.0f) {
            const float tau = 4.0f;
            const float a = dtS / (tau + dtS);
            smoothHr_ = smoothHr_ + a * ((float)hrBpm - smoothHr_);
        }
    } else if (sawHr_ && nowMs >= lastHrMs_ && (nowMs - lastHrMs_) >= cfg_.lostAfterMs) {
        lost_ = true;
    } else if (!sawHr_ && armMs_ > 0 && nowMs >= armMs_ &&
               (nowMs - armMs_) >= cfg_.lostAfterMs) {
        lost_ = true;
    }

    t.smoothedHr = smoothInit_ ? (uint8_t)(smoothHr_ + 0.5f) : 0;
    t.lost = lost_;
    t.hrOk = hrFresh && hrBpm > 0 && !lost_;

    if (lost_ || !t.hrOk || dtS <= 0.0f) {
        t.powerTargetW = powerTargetW_;
        return t;
    }

    const float hr = smoothHr_;
    const float err = (float)targetHr_ - hr;
    t.errorBpm = err;

    if (err > cfg_.deadbandBpm || err < -cfg_.deadbandBpm) {
        integralW_ += cfg_.iGain * err * dtS;
    }
    if (hardMaxHr > 0 && hr > (float)hardMaxHr) {
        integralW_ -= cfg_.overCapGain * (hr - (float)hardMaxHr) * dtS;
    }
    if (integralW_ > cfg_.iLimitW) integralW_ = cfg_.iLimitW;
    if (integralW_ < -cfg_.iLimitW) integralW_ = -cfg_.iLimitW;

    float p = cfg_.basePowerW + integralW_;
    if (p < cfg_.minPowerW) p = cfg_.minPowerW;
    if (p > cfg_.maxPowerW) p = cfg_.maxPowerW;
    powerTargetW_ = p;
    t.powerTargetW = p;
    return t;
}

}  // namespace ergo
