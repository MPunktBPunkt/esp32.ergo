#pragma once

#include <stdint.h>
#include <string.h>

/**
 * Auswertung gefuehrter Tests (Rampe / 20 min / Recovery).
 *
 * Arduino-frei: Zeit und Messwerte kommen als Parameter. Die Lastkurve bleibt
 * beim WorkoutEngine; dieser Runner beobachtet Watt/Puls und liefert den
 * Vorschlag (MAP, FTP, Erholungsnote).
 */
namespace ergo {

enum class TestKind : uint8_t {
    None = 0,
    Ramp = 1,
    Ftp20 = 2,
    Recovery = 3,
};

enum class TestPhase : uint8_t {
    Idle = 0,
    Warmup = 1,
    Main = 2,
    Cool = 3,
    Load = 4,
    Recover = 5,
};

struct TestResult {
    bool valid = false;
    TestKind kind = TestKind::None;
    char endReason[16] = {};
    /** Bestes gleitendes 60-s-Mittel (Rampe). */
    uint16_t mapW = 0;
    /** Nur Hauptblock (20 min) bzw. Session-Fallback. */
    uint16_t avgMainW = 0;
    uint16_t peakW = 0;
    /** 0.75×MAP (Rampe) oder 0.95×avgMain (20 min); 0 wenn unbrauchbar. */
    uint16_t ftpPropose = 0;
    /** 1..10 aus 60-s-Pulsabfall; 0 = keine Note. */
    uint8_t recoveryNote = 0;
    uint8_t hrLoad = 0;
    uint8_t hrRecover = 0;
    uint32_t mainSamples = 0;
    uint32_t durationS = 0;
};

class TestRunner {
public:
    static constexpr uint8_t kWindowS = 60;

    void reset();
    void start(TestKind kind, uint32_t nowMs);
    /** Schrittwechsel aus dem Workout (Label bestimmt die Phase). */
    void onStep(uint8_t stepIndex, const char* label, uint32_t nowMs);
    void observe(uint16_t watts, uint8_t hr, uint32_t nowMs);
    void finalize(const char* reason, uint32_t nowMs);

    bool active() const { return kind_ != TestKind::None && !result_.valid; }
    TestKind kind() const { return kind_; }
    TestPhase phase() const { return phase_; }
    const TestResult& result() const { return result_; }

    static TestKind kindFromWorkoutId(const char* id);
    static const char* kindName(TestKind k);
    static TestPhase phaseFromLabel(TestKind kind, const char* label);

private:
    void samplePower_(uint16_t watts, uint32_t nowMs);
    void updateMap_();
    void score_();

    TestKind kind_ = TestKind::None;
    TestPhase phase_ = TestPhase::Idle;
    TestResult result_{};
    uint32_t startMs_ = 0;
    uint32_t lastSampleMs_ = 0;

    uint16_t ring_[kWindowS] = {};
    uint8_t ringPos_ = 0;
    uint8_t ringCount_ = 0;
    uint32_t ringSum_ = 0;
    uint16_t bestMeanW_ = 0;

    uint64_t mainSumW_ = 0;
    uint32_t mainSamples_ = 0;
    uint16_t peakW_ = 0;

    uint8_t hrAtRecoverStart_ = 0;
    uint8_t hrAtRecoverEnd_ = 0;
    bool recoverArmed_ = false;
};

}  // namespace ergo
