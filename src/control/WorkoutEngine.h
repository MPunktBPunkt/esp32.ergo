#pragma once

#include <stdint.h>
#include <string.h>

/**
 * Mehrschritt-Programm: steady-Schritte mit Wattziel und optionalem Pulsdeckel.
 *
 * Kein JSON/LittleFS in diesem Schnitt — eingebaute Programme + programmierbare
 * Schritte. Arduino-frei; Zeit kommt als Parameter.
 */
namespace ergo {

enum class WorkoutState : uint8_t {
    Idle = 0,
    Running = 1,
    Paused = 2,
    Done = 3,
};

struct WorkoutStep {
    char label[24] = {};
    uint32_t durationS = 0;
    float powerW = 0.0f;
    /** Wenn >0 und powerW==0: power = ftpW * ftpPct / 100. */
    float ftpPct = 0.0f;
    uint8_t hrMax = 0;
    uint8_t hrSoft = 0;
    /** Kein Wattziel — Fahrer stellt die Stufe (z. B. 20-Min-Test). */
    bool selfPaced = false;
};

class WorkoutEngine {
public:
    /** 16 reicht fuer Rampe 60…360 W (+20/min); Editor bleibt bei max. 8. */
    static constexpr uint8_t kMaxSteps = 16;

    struct Tick {
        WorkoutState state = WorkoutState::Idle;
        uint8_t stepIndex = 0;
        uint8_t stepCount = 0;
        const char* label = "";
        float desiredW = 0.0f;
        uint8_t hrMax = 0;
        uint8_t hrSoft = 0;
        bool selfPaced = false;
        uint32_t stepRemainingS = 0;
        uint32_t totalRemainingS = 0;
        uint32_t elapsedS = 0;
        bool justAdvanced = false;
        bool finished = false;
    };

    void reset();

    /** Eingebautes Physio: Einfahren 40 W / Haupt 60 W / Ausfahren 35 W. */
    bool loadBuiltinPhysio(float scale = 1.0f);
    bool loadSteps(const WorkoutStep* steps, uint8_t count, const char* name = "custom");

    /** FTP fuer relative Schritte; 0 laesst ftp_pct-Schritte bei 0 W. */
    void setFtpW(uint16_t ftpW) { ftpW_ = ftpW; }

    bool start(uint32_t nowMs);
    void pause(uint32_t nowMs);
    void resume(uint32_t nowMs);
    bool skip(uint32_t nowMs);
    void stop();

    bool running() const {
        return state_ == WorkoutState::Running || state_ == WorkoutState::Paused;
    }
    WorkoutState state() const { return state_; }
    const char* name() const { return name_; }
    uint8_t stepCount() const { return stepCount_; }
    uint8_t stepIndex() const { return stepIndex_; }

    Tick tick(uint32_t nowMs);

    static const char* stateName(WorkoutState s);

private:
    float resolvePower(const WorkoutStep& st) const;
    uint32_t remainingFrom(uint8_t fromStep, uint32_t stepElapsedS) const;
    void enterStep(uint8_t idx, uint32_t nowMs);

    WorkoutStep steps_[kMaxSteps];
    uint8_t stepCount_ = 0;
    uint8_t stepIndex_ = 0;
    WorkoutState state_ = WorkoutState::Idle;
    char name_[24] = {};
    uint16_t ftpW_ = 0;
    uint32_t stepArmMs_ = 0;
    uint32_t pauseAccumMs_ = 0;
    uint32_t pauseAtMs_ = 0;
    uint32_t sessionArmMs_ = 0;
    bool justAdvanced_ = false;
};

}  // namespace ergo
