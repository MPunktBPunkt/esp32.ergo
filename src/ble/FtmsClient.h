#pragma once

#include <ArduinoJson.h>
#include <NimBLEDevice.h>

#include "DebugRing.h"
#include "FtmsCapabilities.h"
#include "FtmsCodec.h"
#include "control/ControlJournal.h"
#include "control/Limiter.h"

namespace ergo {

/**
 * FTMS-Protokollschicht auf einem bereits verbundenen Client.
 *
 * Der entscheidende Entwurfspunkt: es gibt **keine** oeffentliche Methode, die
 * rohe Bytes an den Control Point schreibt. Jeder Weg nach draussen laeuft
 * durch `send()`, und `send()` fragt zuerst den Limiter. Rule 4 des
 * Pflichtenhefts ist damit nicht Disziplin, sondern Bauweise — ein Bypass
 * muesste diese Klasse aendern, nicht nur sie falsch benutzen.
 *
 * Was das Geraet kann, steht nicht im Code, sondern kommt aus 0x2ACC, 0x2AD6,
 * 0x2AD8 und den beobachteten 0x2AD2-Flags. Ein anderes Ergometer ist deshalb
 * ein Kalibrierlauf, kein Reflash.
 */
class FtmsClient {
public:
    enum class Result : uint8_t {
        Ok = 0,
        NoLink,        // kein Control Point verbunden
        Denied,        // der Limiter hat abgelehnt
        Deferred,      // Rampe: spaeter erneut versuchen
        EncodeFailed,  // Codec hat nichts geliefert
        WriteFailed,   // GATT-Write fehlgeschlagen
    };
    static const char* resultName(Result r);

    void begin(Limiter* limiter);

    /**
     * Optionale Mitschreiber.
     *
     * Beide haengen hier und nicht in `App`, weil nur diese Klasse die
     * tatsaechlich abgesetzten Bytes kennt — nach dem Limiter, der klemmen
     * darf. Ein Mitschnitt aus der Absicht statt aus der Wirklichkeit waere
     * genau die Sorte Beweis, die in der letzten Session nichts wert war.
     */
    void setDebugRing(DebugRing* ring) { ring_ = ring; }
    void setJournal(ControlJournal* journal) { journal_ = journal; }

    /** Nach dem Connect aus loop() aufrufen. Entdeckt 0x1826, liest die
     *  Faehigkeiten und abonniert 0x2AD2 sowie 0x2AD9. */
    bool attach(NimBLEClient* client);
    /** Nach Verbindungsverlust. Verwirft Zeiger, Capabilities und Schattenwert. */
    void detach();

    bool attached() const { return client_ != nullptr; }
    bool hasControlPoint() const { return cp_ != nullptr; }
    bool controlGranted() const { return controlGranted_; }

    const ftms::Capabilities& capabilities() const { return caps_; }
    /**
     * Geraeteprofil-Overrides nach attach (Format / Watt-Vertrauen).
     * Ruft der App-Layer, sobald DeviceStore das aktive Bike kennt.
     */
    void applyDeviceOverrides(ftms::ResistanceFormat format, int8_t powerTrusted);
    const ftms::IndoorBikeData& live() const { return live_; }
    bool hasLive() const { return liveCount_ > 0; }
    uint32_t liveCount() const { return liveCount_; }
    uint32_t lastDataMs() const { return lastData_; }
    /** Kommen keine 0x2AD2 mehr, ist der Link faktisch tot, auch wenn die
     *  Verbindung formal steht. */
    bool stale(uint32_t nowMs) const;

    // ── Steuerung (alles durch den Limiter) ─────────────────────────────────
    Result requestControl(uint32_t nowMs);
    Result reset(uint32_t nowMs);
    Result start(uint32_t nowMs);
    /** Not-Stop. Der Limiter laesst 0x08 immer durch, auch ohne Capabilities. */
    Result stop(uint32_t nowMs);
    Result pause(uint32_t nowMs);
    Result setLevelTenths(int16_t tenths, uint32_t nowMs);
    Result setPowerW(int16_t watt, uint32_t nowMs);
    /** 0x11 — Limiter sperrt bis allowSimulation. */
    Result setSimulation(int16_t windMms, int16_t gradeHundredth, uint8_t crr10000, uint8_t cw100,
                         uint32_t nowMs);

    /** Zuletzt vom Geraet quittierte Antwort auf 0x2AD9. */
    const ftms::ControlResponse& lastResponse() const { return lastResp_; }
    /** Zaehlt hoch, sobald eine neue Antwort da ist — so erkennt `loop()` eine
     *  Quittung, ohne im NimBLE-Callback arbeiten zu muessen. */
    uint32_t respCount() const { return respCount_; }
    const char* lastDenyReason() const { return lastDeny_; }

    void appendStatusJson(JsonObject obj) const;
    void appendIoValues(JsonObject ios) const;

    /** NimBLE-Callback, nicht selbst aufrufen. */
    void onNotify(NimBLERemoteCharacteristic* chr, const uint8_t* data, size_t len);

private:
    Result send(const uint8_t* cmd, size_t len, uint32_t nowMs);
    NimBLERemoteCharacteristic* find(NimBLERemoteService* svc, uint16_t uuid);
    bool readInto(NimBLERemoteCharacteristic* c, uint8_t* buf, size_t cap, size_t& outLen);
    bool subscribeTo(NimBLERemoteCharacteristic* c);

    NimBLEClient* client_ = nullptr;
    NimBLERemoteCharacteristic* ibd_ = nullptr;
    NimBLERemoteCharacteristic* cp_ = nullptr;
    Limiter* limiter_ = nullptr;
    DebugRing* ring_ = nullptr;
    ControlJournal* journal_ = nullptr;

    ftms::Capabilities caps_;
    ftms::IndoorBikeData live_;
    ftms::ControlResponse lastResp_;
    const char* lastDeny_ = "";

    uint32_t liveCount_ = 0;
    uint32_t lastData_ = 0;
    uint32_t respCount_ = 0;
    bool controlGranted_ = false;
    /** Rohbytes der gelesenen Kennungen — fuer Doku und Fehlersuche. */
    char featureHex_[20] = {0};
    char resistanceHex_[16] = {0};
    char powerHex_[16] = {0};
};

}  // namespace ergo
