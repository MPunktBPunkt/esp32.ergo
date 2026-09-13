#include "control/ControlMode.h"

#include <string.h>

namespace ergo {

const char* controlModeName(ControlMode m) {
    switch (m) {
        case ControlMode::Off: return "OFF";
        case ControlMode::ManualLevel: return "MANUAL_LEVEL";
        case ControlMode::ManualErg: return "MANUAL_ERG";
        case ControlMode::HrHold: return "HR_HOLD";
        case ControlMode::Reha: return "REHA";
        case ControlMode::Workout: return "WORKOUT";
        case ControlMode::Sim: return "SIM";
        default: return "UNKNOWN";
    }
}

bool controlModeFromToken(const char* token, ControlMode& out) {
    if (!token || !token[0]) return false;
    if (strcmp(token, "off") == 0 || strcmp(token, "OFF") == 0) {
        out = ControlMode::Off;
        return true;
    }
    if (strcmp(token, "level") == 0 || strcmp(token, "LEVEL") == 0 ||
        strcmp(token, "manual_level") == 0 || strcmp(token, "MANUAL_LEVEL") == 0) {
        out = ControlMode::ManualLevel;
        return true;
    }
    if (strcmp(token, "erg") == 0 || strcmp(token, "ERG") == 0 ||
        strcmp(token, "manual_erg") == 0 || strcmp(token, "MANUAL_ERG") == 0) {
        out = ControlMode::ManualErg;
        return true;
    }
    if (strcmp(token, "hr") == 0 || strcmp(token, "HR") == 0 ||
        strcmp(token, "hr_hold") == 0 || strcmp(token, "HR_HOLD") == 0) {
        out = ControlMode::HrHold;
        return true;
    }
    if (strcmp(token, "reha") == 0 || strcmp(token, "REHA") == 0 ||
        strcmp(token, "physio") == 0 || strcmp(token, "PHYSIO") == 0) {
        out = ControlMode::Reha;
        return true;
    }
    if (strcmp(token, "workout") == 0 || strcmp(token, "WORKOUT") == 0) {
        out = ControlMode::Workout;
        return true;
    }
    if (strcmp(token, "sim") == 0 || strcmp(token, "SIM") == 0) {
        out = ControlMode::Sim;
        return true;
    }
    return false;
}

bool ControlState::setMode(ControlMode m) {
    if (m != ControlMode::Off && m != ControlMode::ManualLevel && m != ControlMode::ManualErg &&
        m != ControlMode::HrHold && m != ControlMode::Reha && m != ControlMode::Workout &&
        m != ControlMode::Sim)
        return false;
    mode_ = m;
    if (m == ControlMode::Off) {
        levelTargetTenths_ = -1;
        powerTargetW_ = 0.0f;
        hrTargetBpm_ = 0;
        gradeTargetHundredth_ = 0;
    } else if (m == ControlMode::ManualLevel) {
        powerTargetW_ = 0.0f;
        hrTargetBpm_ = 0;
        gradeTargetHundredth_ = 0;
    } else if (m == ControlMode::ManualErg) {
        levelTargetTenths_ = -1;
        hrTargetBpm_ = 0;
        gradeTargetHundredth_ = 0;
    } else if (m == ControlMode::HrHold) {
        levelTargetTenths_ = -1;
        gradeTargetHundredth_ = 0;
    } else if (m == ControlMode::Reha || m == ControlMode::Workout) {
        levelTargetTenths_ = -1;
        hrTargetBpm_ = 0;
        gradeTargetHundredth_ = 0;
    } else if (m == ControlMode::Sim) {
        levelTargetTenths_ = -1;
        powerTargetW_ = 0.0f;
        hrTargetBpm_ = 0;
        // grade bleibt / wird per setGrade gesetzt
    }
    return true;
}

bool ControlState::setLevelTargetTenths(int16_t tenths) {
    if (mode_ != ControlMode::ManualLevel) return false;
    if (tenths < 0) return false;
    levelTargetTenths_ = tenths;
    return true;
}

bool ControlState::setPowerTargetW(float watt) {
    if (mode_ != ControlMode::ManualErg && mode_ != ControlMode::HrHold &&
        mode_ != ControlMode::Reha && mode_ != ControlMode::Workout)
        return false;
    if (watt < 0.0f) return false;
    powerTargetW_ = watt;
    return true;
}

bool ControlState::setHrTargetBpm(uint8_t bpm) {
    if (mode_ != ControlMode::HrHold) return false;
    if (bpm < 40 || bpm > 220) return false;
    hrTargetBpm_ = bpm;
    return true;
}

bool ControlState::setGradeTargetHundredth(int16_t hundredth) {
    if (mode_ != ControlMode::Sim) return false;
    // Limiter klemmt weiter; hier nur grober Rahmen (±20 %).
    if (hundredth < -2000) hundredth = -2000;
    if (hundredth > 2000) hundredth = 2000;
    gradeTargetHundredth_ = hundredth;
    return true;
}

}  // namespace ergo
