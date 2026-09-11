#pragma once

#include <Arduino.h>

#include "BuildFlags.h"

namespace ergo {

/**
 * Zwei Rollen, feste Slots. Bewusst keine dynamische Linkliste wie in der
 * Sonde: dort war das Ziel, beliebige Geraete zu erkunden. Hier ist die Frage
 * immer dieselbe — welches Geraet ist das Bike, welches der Gurt.
 */
enum class Role : uint8_t { Bike = 0, Hr = 1 };
constexpr uint8_t kRoleCount = 2;

inline const char* roleName(Role r) {
    switch (r) {
        case Role::Bike: return "bike";
        case Role::Hr: return "hr";
        default: return "?";
    }
}

enum class LinkState : uint8_t {
    Idle = 0,     // nichts gemerkt oder bewusst getrennt
    Connecting,   // Verbindungsversuch laeuft
    Discovering,  // verbunden, Attributsuche laeuft
    Ready,        // verbunden und abonniert
    Lost,         // Verbindung weg, Reconnect wartet
};

inline const char* linkStateName(LinkState s) {
    switch (s) {
        case LinkState::Idle: return "IDLE";
        case LinkState::Connecting: return "CONNECTING";
        case LinkState::Discovering: return "DISCOVERING";
        case LinkState::Ready: return "READY";
        case LinkState::Lost: return "LOST";
        default: return "UNKNOWN";
    }
}

struct ScanEntry {
    char mac[18] = {0};
    char name[32] = {0};
    int8_t rssi = 0;
    uint8_t addrType = 0;
    bool hasFtms = false;
    bool hasHr = false;
    uint32_t lastSeen = 0;
    uint16_t seen = 0;
};

/**
 * Pulsmuster aus 0x2A37. Herstellerunabhaengig gehalten: `contactSupported`
 * trennt "kein Kontakt" von "meldet keinen Kontaktstatus". Der Polar H9 faellt
 * in den zweiten Fall, und die UI darf das nicht als Fehler anzeigen.
 */
struct HrSample {
    uint8_t bpm = 0;
    bool contactSupported = false;
    uint8_t contact = 0;  // 0 = kein Kontakt, 1 = Kontakt, 2 = unbekannt
    uint16_t rr[8] = {0};
    uint8_t rrCount = 0;
    bool hasEnergy = false;
    uint16_t energyKj = 0;
    uint32_t at = 0;  // millis der Notify
};

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

}  // namespace ergo
