#pragma once

#include <stdint.h>

#include "core/Profile.h"

/**
 * Pulsregelung als Aeussenkreis: Zielpuls → Zielwatt.
 *
 * Der innere Kreis ist `PowerController` (Watt → Stufe). So bleibt die
 * Kadenzabhaengigkeit dort, wo die Kennflaeche sitzt (PFLICHTENHEFT §6).
 * Arduino-frei; Zeit und Puls kommen als Parameter.
 */
namespace ergo {

struct HrControllerConfig {
    float deadbandBpm = 3.0f;
    /** Watt pro (BPM-Fehler × Sekunde). Positiv: Puls unter Ziel → mehr Watt. */
    float iGain = 0.35f;
    float iLimitW = 50.0f;
    float basePowerW = 80.0f;
    float minPowerW = 25.0f;
    float maxPowerW = 180.0f;
    uint32_t lostAfterMs = 8000;
    float overCapGain = 2.0f;
};

class HrController {
public:
    struct Tick {
        float powerTargetW = 0.0f;
        bool hrOk = false;
        bool lost = false;
        uint8_t smoothedHr = 0;
        float errorBpm = 0.0f;
        HrLossPolicy lossPolicy = HrLossPolicy::Reduce;
    };

    void begin(const HrControllerConfig& cfg = {});
    void reset();

    void setTargetHr(uint8_t bpm);
    uint8_t targetHr() const { return targetHr_; }
    bool hasTarget() const { return targetHr_ >= 40; }

    void setPowerLimits(float minW, float maxW);
    void setBasePowerW(float w);
    void setLossPolicy(HrLossPolicy p) { lossPolicy_ = p; }

    float powerTargetW() const { return powerTargetW_; }
    bool lost() const { return lost_; }
    HrLossPolicy lossPolicy() const { return lossPolicy_; }
    uint8_t smoothedHr() const {
        return smoothInit_ ? static_cast<uint8_t>(smoothHr_ + 0.5f) : 0;
    }

    Tick tick(uint32_t nowMs, uint8_t hrBpm, bool hrFresh, uint8_t hardMaxHr = 0);

private:
    HrControllerConfig cfg_{};
    uint8_t targetHr_ = 0;
    float integralW_ = 0.0f;
    float powerTargetW_ = 0.0f;
    float smoothHr_ = 0.0f;
    bool smoothInit_ = false;
    bool lost_ = false;
    bool sawHr_ = false;
    uint32_t lastHrMs_ = 0;
    uint32_t armMs_ = 0;
    uint32_t lastTickMs_ = 0;
    HrLossPolicy lossPolicy_ = HrLossPolicy::Reduce;
};

}  // namespace ergo
