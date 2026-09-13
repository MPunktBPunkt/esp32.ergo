#include "BridgeAssist.h"

#include <math.h>

namespace ergo {

float bridgeApplyDifficulty(float appWatt, uint16_t difficultyPct, float minW, float maxW) {
    if (!(appWatt > 0.0f) || !isfinite(appWatt)) return minW;
    uint16_t pct = difficultyPct;
    if (pct < 50) pct = 50;
    if (pct > 150) pct = 150;
    float w = appWatt * ((float)pct / 100.0f);
    if (w < minW) w = minW;
    if (w > maxW) w = maxW;
    return w;
}

void BridgeHrCap::begin(const BridgeHrCapConfig& cfg) {
    cfg_ = cfg;
    reset();
}

void BridgeHrCap::reset() {
    effective_ = 0.0f;
    capActive_ = false;
    interventions_ = 0;
    lastMs_ = 0;
    armed_ = false;
}

void BridgeHrCap::setLimits(uint8_t softBpm, uint8_t hardBpm) {
    soft_ = softBpm;
    hard_ = hardBpm;
    if (soft_ > 0 && hard_ == 0) hard_ = soft_;
    if (hard_ > 0 && soft_ > hard_) soft_ = hard_;
}

float BridgeHrCap::tick(uint32_t nowMs, float desiredW, uint8_t hrBpm, bool hrFresh) {
    if (!(desiredW > 0.0f)) {
        effective_ = 0.0f;
        capActive_ = false;
        return 0.0f;
    }
    if (!enabled()) {
        effective_ = desiredW;
        capActive_ = false;
        return effective_;
    }

    float dt = 0.0f;
    if (armed_ && lastMs_ > 0 && nowMs > lastMs_) {
        dt = (float)(nowMs - lastMs_) / 1000.0f;
        if (dt > 2.0f) dt = 2.0f;
    }
    lastMs_ = nowMs;
    armed_ = true;

    if (!hrFresh || hrBpm == 0) {
        // Ohne Puls: Soll halten (App nicht stoeren).
        effective_ = desiredW;
        capActive_ = false;
        return effective_;
    }

    float target = desiredW;
    if (hrBpm > hard_) {
        const float over = (float)(hrBpm - hard_);
        target = desiredW - over * cfg_.hardCutGain - (float)(hard_ - soft_) * cfg_.softCutGain;
        capActive_ = true;
    } else if (hrBpm > soft_) {
        const float over = (float)(hrBpm - soft_);
        target = desiredW - over * cfg_.softCutGain;
        capActive_ = true;
    } else {
        capActive_ = false;
    }
    if (target < cfg_.minPowerW) target = cfg_.minPowerW;
    if (target > desiredW) target = desiredW;

    if (!capActive_ && effective_ < desiredW && dt > 0.0f) {
        effective_ += cfg_.restorePerS * dt;
        if (effective_ > desiredW) effective_ = desiredW;
    } else if (capActive_) {
        if (effective_ <= 0.0f) effective_ = desiredW;
        if (target < effective_) {
            if (interventions_ < 0xFFFF) interventions_++;
            effective_ = target;
        } else if (dt > 0.0f) {
            // Langsam nachziehen, wenn Puls sinkt aber noch ueber Soft.
            effective_ += cfg_.restorePerS * dt * 0.5f;
            if (effective_ > target) effective_ = target;
        }
    } else {
        effective_ = desiredW;
    }

    return effective_;
}

}  // namespace ergo
