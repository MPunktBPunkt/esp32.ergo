#include "control/PowerController.h"

#include <math.h>

namespace ergo {

void PowerController::begin(const PowerControllerConfig& cfg) {
    cfg_ = cfg;
    if (cfg_.periodMs < 1000) cfg_.periodMs = 1000;
    if (cfg_.smoothTauS < 0.5f) cfg_.smoothTauS = 0.5f;
    if (cfg_.iGain < 0.0f) cfg_.iGain = 0.0f;
    if (cfg_.iLimitW < 0.0f) cfg_.iLimitW = 0.0f;
    if (cfg_.deadbandW < 0.0f) cfg_.deadbandW = 0.0f;
    if (cfg_.retargetW < 0.0f) cfg_.retargetW = 0.0f;
    reset();
}

void PowerController::reset() {
    integralW_ = 0.0f;
    smoothW_ = 0.0f;
    smoothInit_ = false;
    lastLevel_ = -1;
    ceiling_ = false;
    lastWriteMs_ = 0;
    lastTickMs_ = 0;
}

void PowerController::setTargetW(float watt) {
    if (watt < 0.0f) watt = 0.0f;
    const float delta = fabsf(watt - targetW_);
    if (watt == targetW_) return;
    targetW_ = watt;
    // Kleine Ziel-Updates (MyWhoosh ERG-Spam): nur Ziel, kein I-Reset,
    // kein Sofort-Write und vor allem kein lastLevel_=-1 (Stufenjagd).
    if (delta >= cfg_.retargetW) {
        integralW_ = 0.0f;
        lastWriteMs_ = 0;
    }
}

PowerController::Tick PowerController::tick(uint32_t nowMs, float rpm, float watt, bool fresh,
                                            const PowerMap& map) {
    Tick t;
    t.targetW = targetW_;
    t.mapReady = map.ready();

    if (targetW_ <= 0.0f || !map.ready()) {
        t.ceiling = false;
        ceiling_ = false;
        return t;
    }

    float dtS = 0.0f;
    if (lastTickMs_ > 0 && nowMs >= lastTickMs_) {
        dtS = (float)(nowMs - lastTickMs_) / 1000.0f;
        if (dtS > 2.0f) dtS = 2.0f;  // nach Pause nicht explosiv integrieren
    }
    lastTickMs_ = nowMs;

    if (fresh && watt >= 0.0f) {
        if (!smoothInit_) {
            smoothW_ = watt;
            smoothInit_ = true;
        } else if (dtS > 0.0f) {
            const float a = dtS / (cfg_.smoothTauS + dtS);
            smoothW_ = smoothW_ + a * (watt - smoothW_);
        }
    }
    t.smoothedW = smoothW_;

    if (fresh && smoothInit_ && dtS > 0.0f && rpm >= kCadMin) {
        const float err = targetW_ - smoothW_;
        if (err > cfg_.deadbandW || err < -cfg_.deadbandW) {
            integralW_ += cfg_.iGain * err * dtS;
            if (integralW_ > cfg_.iLimitW) integralW_ = cfg_.iLimitW;
            if (integralW_ < -cfg_.iLimitW) integralW_ = -cfg_.iLimitW;
        }
    }

    const float effective = targetW_ + integralW_;
    t.effectiveTargetW = effective;

    int16_t desired = -1;
    bool ceiling = false;
    if (!map.bestLevel(effective, rpm, desired, ceiling)) {
        // Keine Stuetzstelle fuer diese Kadenz — kein Write.
        ceiling_ = true;
        t.ceiling = true;
        return t;
    }
    ceiling_ = ceiling;
    t.ceiling = ceiling;
    t.desiredTenths = desired;

    int16_t tenths = desired;
    // Slew: hoechstens maxStepTenths Richtung Map-Wunsch (0 = freier Sprung).
    if (cfg_.maxStepTenths > 0 && lastLevel_ >= 0) {
        const int16_t step = (int16_t)cfg_.maxStepTenths;
        if (tenths > lastLevel_ + step) tenths = (int16_t)(lastLevel_ + step);
        else if (tenths < lastLevel_ - step) tenths = (int16_t)(lastLevel_ - step);
    }
    t.levelTenths = tenths;

    float est = 0.0f;
    if (map.estimate(tenths, rpm, est)) t.estimatedW = est;

    const bool due = (lastWriteMs_ == 0) || (nowMs - lastWriteMs_ >= cfg_.periodMs);
    const bool changed = (tenths != lastLevel_);
    if (due && (changed || lastWriteMs_ == 0)) {
        t.wantWrite = true;
        lastLevel_ = tenths;
        lastWriteMs_ = nowMs;
    }
    return t;
}

}  // namespace ergo
