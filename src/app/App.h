#pragma once

#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFiClient.h>

#include "ble/BleCentral.h"
#include "ble/FtmsClient.h"
#include "ble/HrClient.h"
#include "ble/DebugRing.h"
#include "control/ControlJournal.h"
#include "control/ControlMode.h"
#include "control/Limiter.h"
#include "control/PowerMap.h"
#include "control/SweepRunner.h"
#include "core/ConfigStore.h"
#include "core/HubClient.h"
#include "core/Profile.h"

/**
 * Shell, BLE, Kalibrierung, Profile und Steuermodus OFF/MANUAL_LEVEL.
 *
 * MANUAL_ERG und HR_HOLD warten auf den Hand-Beweis der Stufe und die
 * Kennflaeche (STATE.md §3 / §5).
 */
class App {
public:
    static App& instance();

    ConfigStore config;
    HubClient hub;
    WebServer server{80};

    ergo::BleCentral ble;
    ergo::FtmsClient ftms;
    ergo::HrClient hrc;
    ergo::Limiter limiter;
    ergo::PowerMap powerMap;
    ergo::SweepRunner sweep;
    ergo::DebugRing ring;
    ergo::ControlJournal journal;
    ergo::ProfileStore profiles;
    ergo::ControlState control;

    void begin();
    void loop();

    void buildStatusJson(JsonDocument& doc);
    void buildHeartbeat(JsonDocument& doc);
    String statusString();

    /** Von BleCentral aus loop() gerufen. */
    void onLink(ergo::Role role, bool up);

private:
    App() {}

    void checkResetButton();
    void setupWifi();
    void setupWeb();
    void registerRoutes();
    void registerBleRoutes();
    void registerControlRoutes();
    void registerProfileRoutes();
    void registerCalibRoutes();
    void registerDebugRoutes();
    void runCodecSelfTest();
    void applyLimiterConfig();
    void seedDefaultProfiles();

    void profileToJson(const ergo::Profile& p, JsonObject obj) const;
    bool profileFromJson(JsonVariantConst v, ergo::Profile& out) const;

    /** Welcher Puls gilt gerade, und woher. */
    ergo::HrSource resolveHrSource() const;
    uint8_t effectiveHr() const;

    // ── Kalibrierung ────────────────────────────────────────────────────────
    void loopCalibration(unsigned long now);
    /** Fertige Sweep-Punkte in die Kennflaeche uebernehmen und protokollieren. */
    void harvestSweepPoints();
    void appendCalibJson(JsonObject obj) const;
    void loadPowerMap();
    void savePowerMap();
    /** Zeitstempel fuer die Kennflaeche. Unixzeit wenn NTP steht, sonst
     *  Laufzeit — dann ist „Alter" relativ zum Boot, und das ist besser als
     *  ein erfundenes Datum. */
    static uint32_t mapNowS();

    void handleMain();
    void handleOtaUpload();
    void handleOtaUploadFinish();
    void handleEvents();
    void sseSend(const String& data);

    WiFiClient sseClient_;
    unsigned long lastSse_ = 0;
    unsigned long lastWifiCheck_ = 0;
    bool restartPending_ = false;
    unsigned long restartAt_ = 0;

    uint8_t sweepSeen_ = 0;
    ergo::SweepState sweepWas_ = ergo::SweepState::Idle;
    /** Fuer passives Lernen: seit wann steht die Stufe unveraendert. */
    int16_t levelWas_ = -32768;
    unsigned long levelStableSince_ = 0;
    unsigned long lastPassive_ = 0;
    bool mapDirty_ = false;
    unsigned long mapSaved_ = 0;

    // ── Debug-Modus und Steuer-Journal ──────────────────────────────────────
    /**
     * Beides wird aus `loop()` gefuettert, nicht aus dem NimBLE-Callback.
     *
     * Der Ring muss im Callback sitzen, weil die Rohbytes nur dort existieren —
     * das ist ein memcpy und sonst nichts. Das Journal rechnet und urteilt, und
     * das hat im Host-Task nichts zu suchen. Die beiden Zaehler sagen `loop()`,
     * ob seit dem letzten Durchlauf ein neuer Messwert oder eine neue Quittung
     * angekommen ist.
     */
    void loopDebug(unsigned long now);
    void appendDebugJson(JsonObject obj) const;
    uint32_t liveSeen_ = 0;
    uint32_t respSeen_ = 0;
    uint16_t judgedSeen_ = 0;

    const char* codecSelfTest_ = "nicht gelaufen";
};
