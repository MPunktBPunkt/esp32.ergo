#pragma once

#include <stdint.h>

/**
 * Steuermodus — OFF … REHA, WORKOUT, SIM.
 *
 * Arduino-frei. SIM nutzt FTMS 0x11 (Steigung), freigeschaltet nach Nachtest 4
 * und Config `allowSimulation`.
 */
namespace ergo {

enum class ControlMode : uint8_t {
    Off = 0,
    ManualLevel = 1,
    ManualErg = 2,
    HrHold = 3,
    Reha = 4,
    Workout = 5,
    Sim = 6,
};

const char* controlModeName(ControlMode m);
bool controlModeFromToken(const char* token, ControlMode& out);

class ControlState {
public:
    ControlMode mode() const { return mode_; }
    int16_t levelTargetTenths() const { return levelTargetTenths_; }
    float powerTargetW() const { return powerTargetW_; }
    uint8_t hrTargetBpm() const { return hrTargetBpm_; }
    /** Zielsteigung in 0,01 % (FTMS Grade). */
    int16_t gradeTargetHundredth() const { return gradeTargetHundredth_; }

    bool setMode(ControlMode m);
    bool setLevelTargetTenths(int16_t tenths);
    bool setPowerTargetW(float watt);
    bool setHrTargetBpm(uint8_t bpm);
    bool setGradeTargetHundredth(int16_t hundredth);

    bool allowsLevelWrite() const { return mode_ == ControlMode::ManualLevel; }
    bool allowsErg() const {
        return mode_ == ControlMode::ManualErg || mode_ == ControlMode::HrHold ||
               mode_ == ControlMode::Reha || mode_ == ControlMode::Workout;
    }
    bool allowsHrHold() const { return mode_ == ControlMode::HrHold; }
    bool allowsReha() const { return mode_ == ControlMode::Reha; }
    bool allowsWorkout() const { return mode_ == ControlMode::Workout; }
    bool allowsSim() const { return mode_ == ControlMode::Sim; }
    bool allowsAnyLoadWrite() const {
        return mode_ == ControlMode::ManualLevel || mode_ == ControlMode::ManualErg ||
               mode_ == ControlMode::HrHold || mode_ == ControlMode::Reha ||
               mode_ == ControlMode::Workout || mode_ == ControlMode::Sim;
    }
    bool sessionActive() const { return mode_ != ControlMode::Off; }

private:
    ControlMode mode_ = ControlMode::Off;
    int16_t levelTargetTenths_ = -1;
    float powerTargetW_ = 0.0f;
    uint8_t hrTargetBpm_ = 0;
    int16_t gradeTargetHundredth_ = 0;
};

}  // namespace ergo
