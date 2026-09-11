#pragma once

#include <stdint.h>

/**
 * Steuermodus — OFF, MANUAL_LEVEL, MANUAL_ERG.
 *
 * Arduino-frei. HR_HOLD / WORKOUT / SIM bleiben reserviert und lehnen
 * setMode ab, bis Pulsfuehrung bzw. Workout stehen.
 */
namespace ergo {

enum class ControlMode : uint8_t {
    Off = 0,
    ManualLevel = 1,
    ManualErg = 2,
    HrHold = 3,
    Workout = 4,
    Sim = 5,
};

const char* controlModeName(ControlMode m);
/** "off" / "level" / "erg" / "hr" / "workout" / "sim" — nullptr bei Muell. */
bool controlModeFromToken(const char* token, ControlMode& out);

class ControlState {
public:
    ControlMode mode() const { return mode_; }
    int16_t levelTargetTenths() const { return levelTargetTenths_; }
    float powerTargetW() const { return powerTargetW_; }

    /** Off, ManualLevel, ManualErg. Andere → false. */
    bool setMode(ControlMode m);

    /** Nur in ManualLevel. */
    bool setLevelTargetTenths(int16_t tenths);

    /** Nur in ManualErg. watt<=0 loescht das Ziel nicht — setMode(Off) schon. */
    bool setPowerTargetW(float watt);

    bool allowsLevelWrite() const { return mode_ == ControlMode::ManualLevel; }
    bool allowsErg() const { return mode_ == ControlMode::ManualErg; }
    bool allowsAnyLoadWrite() const {
        return mode_ == ControlMode::ManualLevel || mode_ == ControlMode::ManualErg;
    }

    /** Session-Stand-in: alles ausser Off gilt als „Session". */
    bool sessionActive() const { return mode_ != ControlMode::Off; }

private:
    ControlMode mode_ = ControlMode::Off;
    int16_t levelTargetTenths_ = -1;
    float powerTargetW_ = 0.0f;
};

}  // namespace ergo
