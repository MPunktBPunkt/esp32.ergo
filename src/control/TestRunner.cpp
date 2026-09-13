#include "control/TestRunner.h"

#include <stdio.h>

namespace ergo {

void TestRunner::reset() {
    kind_ = TestKind::None;
    phase_ = TestPhase::Idle;
    result_ = TestResult{};
    startMs_ = 0;
    lastSampleMs_ = 0;
    ringPos_ = 0;
    ringCount_ = 0;
    ringSum_ = 0;
    bestMeanW_ = 0;
    for (uint8_t i = 0; i < kWindowS; i++) ring_[i] = 0;
    mainSumW_ = 0;
    mainSamples_ = 0;
    peakW_ = 0;
    hrAtRecoverStart_ = 0;
    hrAtRecoverEnd_ = 0;
    recoverArmed_ = false;
}

TestKind TestRunner::kindFromWorkoutId(const char* id) {
    if (!id) return TestKind::None;
    if (strcmp(id, "test_ramp") == 0) return TestKind::Ramp;
    if (strcmp(id, "test_20min") == 0) return TestKind::Ftp20;
    if (strcmp(id, "test_recovery") == 0) return TestKind::Recovery;
    return TestKind::None;
}

const char* TestRunner::kindName(TestKind k) {
    switch (k) {
        case TestKind::Ramp: return "Rampe";
        case TestKind::Ftp20: return "20 Minuten";
        case TestKind::Recovery: return "Recovery";
        default: return "";
    }
}

TestPhase TestRunner::phaseFromLabel(TestKind kind, const char* label) {
    if (!label) label = "";
    if (kind == TestKind::Ramp) return TestPhase::Main;
    if (kind == TestKind::Ftp20) {
        if (strncmp(label, "Haupt", 5) == 0) return TestPhase::Main;
        if (strncmp(label, "Cool", 4) == 0) return TestPhase::Cool;
        if (strncmp(label, "Warm", 4) == 0) return TestPhase::Warmup;
        return TestPhase::Warmup;
    }
    if (kind == TestKind::Recovery) {
        if (strncmp(label, "Erholung", 8) == 0 || strncmp(label, "Recover", 7) == 0)
            return TestPhase::Recover;
        return TestPhase::Load;
    }
    return TestPhase::Idle;
}

void TestRunner::start(TestKind kind, uint32_t nowMs) {
    reset();
    if (kind == TestKind::None) return;
    kind_ = kind;
    phase_ = TestPhase::Idle;
    startMs_ = nowMs;
    lastSampleMs_ = nowMs;
}

void TestRunner::onStep(uint8_t /*stepIndex*/, const char* label, uint32_t nowMs) {
    if (kind_ == TestKind::None) return;
    const TestPhase next = phaseFromLabel(kind_, label);
    if (kind_ == TestKind::Recovery && next == TestPhase::Recover && phase_ != TestPhase::Recover) {
        recoverArmed_ = true;
        hrAtRecoverStart_ = 0;
        hrAtRecoverEnd_ = 0;
    }
    phase_ = next;
    (void)nowMs;
}

void TestRunner::samplePower_(uint16_t watts, uint32_t nowMs) {
    // Hoechstens 1 Hz — gleiches Raster wie Session-Ticks.
    if (lastSampleMs_ != 0 && (nowMs - lastSampleMs_) < 900UL) return;
    lastSampleMs_ = nowMs;

    if (watts > peakW_) peakW_ = watts;

    if (ringCount_ == kWindowS) {
        ringSum_ -= ring_[ringPos_];
    } else {
        ringCount_++;
    }
    ring_[ringPos_] = watts;
    ringSum_ += watts;
    ringPos_ = (uint8_t)((ringPos_ + 1) % kWindowS);
    updateMap_();

    if (phase_ == TestPhase::Main) {
        mainSumW_ += watts;
        mainSamples_++;
    }
}

void TestRunner::updateMap_() {
    if (ringCount_ < kWindowS) return;
    const uint16_t mean = (uint16_t)(ringSum_ / kWindowS);
    if (mean > bestMeanW_) bestMeanW_ = mean;
}

void TestRunner::observe(uint16_t watts, uint8_t hr, uint32_t nowMs) {
    if (kind_ == TestKind::None || result_.valid) return;

    if (kind_ == TestKind::Ramp || kind_ == TestKind::Ftp20 || kind_ == TestKind::Recovery) {
        samplePower_(watts, nowMs);
    }

    if (kind_ == TestKind::Recovery && phase_ == TestPhase::Recover && hr > 0) {
        if (recoverArmed_ && hrAtRecoverStart_ == 0) hrAtRecoverStart_ = hr;
        hrAtRecoverEnd_ = hr;
    }
}

void TestRunner::score_() {
    result_.kind = kind_;
    result_.peakW = peakW_;
    result_.mapW = bestMeanW_;
    result_.avgMainW =
        mainSamples_ > 0 ? (uint16_t)(mainSumW_ / mainSamples_) : 0;
    result_.mainSamples = mainSamples_;
    result_.hrLoad = hrAtRecoverStart_;
    result_.hrRecover = hrAtRecoverEnd_;

    if (kind_ == TestKind::Ramp) {
        const uint16_t map = bestMeanW_ > 0 ? bestMeanW_ : peakW_;
        result_.mapW = map;
        result_.ftpPropose = map > 0 ? (uint16_t)((map * 75 + 50) / 100) : 0;
    } else if (kind_ == TestKind::Ftp20) {
        const uint16_t avg = result_.avgMainW;
        result_.ftpPropose = avg > 0 ? (uint16_t)((avg * 95 + 50) / 100) : 0;
    } else if (kind_ == TestKind::Recovery) {
        result_.ftpPropose = 0;
        if (hrAtRecoverStart_ > 0 && hrAtRecoverEnd_ > 0 &&
            hrAtRecoverStart_ >= hrAtRecoverEnd_) {
            const unsigned drop = (unsigned)(hrAtRecoverStart_ - hrAtRecoverEnd_);
            // Grobe Note 1..10: 0 bpm → 1, ≥40 bpm → 10 (Konsole-aehnlich).
            unsigned note = 1 + (drop * 9) / 40;
            if (note > 10) note = 10;
            result_.recoveryNote = (uint8_t)note;
        }
    }
}

void TestRunner::finalize(const char* reason, uint32_t nowMs) {
    if (kind_ == TestKind::None) return;
    // Letzte unvollstaendige Fenster: fuer Rampe auch Teilfenster als Peak-Hilfe
    // nicht als MAP ausgeben — bestMean braucht volle 60 s.
    if (kind_ == TestKind::Ramp && bestMeanW_ == 0 && ringCount_ > 0) {
        bestMeanW_ = (uint16_t)(ringSum_ / ringCount_);
    }
    score_();
    result_.valid = true;
    result_.durationS = startMs_ ? (nowMs - startMs_) / 1000UL : 0;
    strncpy(result_.endReason, reason ? reason : "", sizeof(result_.endReason) - 1);
    result_.endReason[sizeof(result_.endReason) - 1] = 0;
    kind_ = TestKind::None;
    phase_ = TestPhase::Idle;
}

}  // namespace ergo
