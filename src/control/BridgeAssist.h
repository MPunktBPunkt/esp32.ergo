#pragma once

#include <stdint.h>

/**
 * Bridge-Hilfen ohne Arduino: Difficulty und Pulsdeckel auf App-Wattziele.
 *
 * Difficulty: Prozent (100 = unveraendert). 80 = App-Watt × 0,8 ans Bike.
 * HR-Deckel: gleiche Soft/Hard-Logik wie Reha, aber nur fuer Bridge-ERG.
 */
namespace ergo {

/** appWatt × (pct/100), geklemmt auf [minW, maxW]. pct typisch 50..150. */
float bridgeApplyDifficulty(float appWatt, uint16_t difficultyPct, float minW = 20.0f,
                            float maxW = 400.0f);

struct BridgeHrCapConfig {
    float minPowerW = 25.0f;
    float softCutGain = 2.5f;
    float hardCutGain = 6.0f;
    float restorePerS = 0.5f;
};

class BridgeHrCap {
public:
    void begin(const BridgeHrCapConfig& cfg = {});
    void reset();

    /** soft/hard = 0 → Deckel aus. soft > 0 und hard == 0 → hard = soft. */
    void setLimits(uint8_t softBpm, uint8_t hardBpm);
    uint8_t softBpm() const { return soft_; }
    uint8_t hardBpm() const { return hard_; }
    bool enabled() const { return soft_ > 0; }

    float tick(uint32_t nowMs, float desiredW, uint8_t hrBpm, bool hrFresh);

    float effectiveW() const { return effective_; }
    bool capActive() const { return capActive_; }
    uint16_t interventions() const { return interventions_; }

private:
    BridgeHrCapConfig cfg_{};
    uint8_t soft_ = 0;
    uint8_t hard_ = 0;
    float effective_ = 0.0f;
    bool capActive_ = false;
    uint16_t interventions_ = 0;
    uint32_t lastMs_ = 0;
    bool armed_ = false;
};

}  // namespace ergo
