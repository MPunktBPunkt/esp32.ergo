#pragma once

#include <stdint.h>

/**
 * Steuermodus — vorerst nur OFF und MANUAL_LEVEL.
 *
 * Arduino-frei. MANUAL_ERG / HR_HOLD / WORKOUT sind reserviert und lehnen
 * setMode ab, bis Kennflaeche bzw. Pulsfuehrung stehen (STATE.md §5).
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

    /** Nur Off ↔ ManualLevel. Andere Modi → false. */
    bool setMode(ControlMode m);

    /** Nur in ManualLevel. */
    bool setLevelTargetTenths(int16_t tenths);

    bool allowsLevelWrite() const { return mode_ == ControlMode::ManualLevel; }
    bool allowsAnyLoadWrite() const { return mode_ == ControlMode::ManualLevel; }

    /** Session-Stand-in: alles ausser Off gilt als „Session". */
    bool sessionActive() const { return mode_ != ControlMode::Off; }

private:
    ControlMode mode_ = ControlMode::Off;
    int16_t levelTargetTenths_ = -1;
};

}  // namespace ergo
