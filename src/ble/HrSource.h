#pragma once

#include <stdint.h>

namespace ergo {

/** Woher der aktuell gueltige Puls kommt. */
enum class HrSource : uint8_t {
    None = 0,
    Strap,   // eigener BLE-Link auf 0x180D
    Relay,   // esp32.heartrate als Zwischenstation
    Machine, // 0x2AD2-Feld des Bikes (beim Varon aus dem 5-kHz-Sender)
};

inline const char* hrSourceName(HrSource s) {
    switch (s) {
        case HrSource::Strap: return "strap";
        case HrSource::Relay: return "relay";
        case HrSource::Machine: return "machine";
        default: return "none";
    }
}

/**
 * Ob die Quelle fuer HR_HOLD / Reha-Deckel taugt.
 *
 * Nur Strap und Relay (echtes 0x180D). Machine (Bike 2AD2 / 5-kHz-GymLink)
 * liegt im Dual-Link-Lauf systematisch ~+25 bpm ueber dem Gurt und darf die
 * Regelung nicht speisen — siehe ENTWICKLERDOKU §5 / Nachtest 5.
 */
inline bool hrUsableForControl(HrSource s) {
    return s == HrSource::Strap || s == HrSource::Relay;
}

}  // namespace ergo
