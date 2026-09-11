#pragma once

#include <stdint.h>

#include "core/Profile.h"

/**
 * Reha / Physio: festes Wattziel mit Pulsdeckel.
 *
 * Kein Zielpuls (das ist HrController). Hier bleibt desiredW stehen und wird
 * nur abgesenkt, wenn der Puls ins Soft-/Hard-Band laeuft. Arduino-frei.
 */
namespace ergo {

struct RehaControllerConfig {
    float minPowerW = 25.0f;
    /** Watt pro BPM ueber Soft-Grenze. */
    float softCutGain = 2.5f;
    /** Zusaetzliche Watt pro BPM ueber Hard-Grenze. */
    float hardCutGain = 6.0f;
    /** Rueckkehr zum Soll, Watt pro Sekunde, wenn Puls unter Soft. */
    float restorePerS = 0.4f;
    uint32_t lostAfterMs = 8000;
};

class RehaController {
public:
    struct Tick {
        float effectiveW = 0.0f;
        float desiredW = 0.0f;
        bool capActive = false;
        uint16_t interventions = 0;
        bool finished = false;
        bool lost = false;
        uint32_t elapsedS = 0;
        uint32_t remainingS = 0;  // 0 wenn unbegrenzt oder fertig
        HrLossPolicy lossPolicy = HrLossPolicy::Reduce;
    };

    void begin(const RehaControllerConfig& cfg = {});
    void reset();

    void setDesiredW(float watt);
    float desiredW() const { return desiredW_; }

    void setHrLimits(uint8_t softBpm, uint8_t hardBpm);
    uint8_t hrSoft() const { return hrSoft_; }
    uint8_t hrMax() const { return hrMax_; }

    /** 0 = ohne Zeitlimit. */
    void setDurationS(uint32_t sec);
    uint32_t durationS() const { return durationS_; }

    void setLossPolicy(HrLossPolicy p) { lossPolicy_ = p; }
    HrLossPolicy lossPolicy() const { return lossPolicy_; }

    float effectiveW() const { return effectiveW_; }
    bool capActive() const { return capActive_; }
    uint16_t interventions() const { return interventions_; }
    bool lost() const { return lost_; }
    bool finished() const { return finished_; }
    uint32_t elapsedS() const { return elapsedS_; }
    uint32_t remainingS() const { return remainingS_; }

    Tick tick(uint32_t nowMs, uint8_t hrBpm, bool hrFresh);

private:
    RehaControllerConfig cfg_{};
    float desiredW_ = 60.0f;
    float effectiveW_ = 60.0f;
    uint8_t hrSoft_ = 115;
    uint8_t hrMax_ = 120;
    uint32_t durationS_ = 600;
    uint32_t armMs_ = 0;
    uint32_t lastTickMs_ = 0;
    uint32_t lastHrMs_ = 0;
    uint32_t elapsedS_ = 0;
    uint32_t remainingS_ = 0;
    bool sawHr_ = false;
    bool lost_ = false;
    bool finished_ = false;
    bool capActive_ = false;
    uint16_t interventions_ = 0;
    HrLossPolicy lossPolicy_ = HrLossPolicy::Reduce;
};

}  // namespace ergo
