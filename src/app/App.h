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
#include "control/HrController.h"
#include "control/Limiter.h"
#include "control/PowerController.h"
#include "control/PowerMap.h"
#include "control/RehaController.h"
#include "control/SweepRunner.h"
#include "control/WorkoutEngine.h"
#include "control/WorkoutJson.h"
#include "core/ConfigStore.h"
#include "core/HubClient.h"
#include "core/Profile.h"
#include "core/SessionStore.h"
#include "core/SessionSummary.h"
#include "core/SessionTracker.h"

/**
 * Shell, BLE, Kalibrierung, Profile, OFF / LEVEL / ERG / HR_HOLD / REHA / WORKOUT.
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
    ergo::PowerController powerCtl;
    ergo::HrController hrCtl;
    ergo::RehaController rehaCtl;
    ergo::WorkoutEngine workout;
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
    /** Legt fehlende Vorlagen nach (Martin), ohne bestehende zu überschreiben. */
    void ensureKnownProfiles();
    void loadProfiles();
    void saveProfiles();
    bool beginFs();
    void recordSessionEnd(const char* reason);
    void beginSession(const char* workoutName = "");
    void loopSession(unsigned long now);
    void persistSession_(const ergo::SessionSummary& s);
    void loadSessionArchive_();
    bool loadWorkoutDoc(const ergo::WorkoutDoc& doc, float scale);

    void profileToJson(const ergo::Profile& p, JsonObject obj) const;
    bool profileFromJson(JsonVariantConst v, ergo::Profile& out) const;

    /** Welcher Puls gilt gerade, und woher. */
    ergo::HrSource resolveHrSource() const;
    uint8_t effectiveHr() const;

    // ── Kalibrierung ────────────────────────────────────────────────────────
    void loopCalibration(unsigned long now);
    void loopErg(unsigned long now);
    void loopHr(unsigned long now);
    void loopReha(unsigned long now);
    void loopWorkout(unsigned long now);
    /** Gemeinsame Pulsdeckel-Last fuer REHA und WORKOUT. */
    void applyRehaCap(unsigned long now, bool fromWorkout);
    void applyFreezeToLevel(unsigned long now);
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
    ergo::WorkoutEngine::Tick woSnap_{};
    ergo::SessionSummary lastSession_{};
    ergo::SessionTracker session_;
    ergo::SessionStore sessionStore_;
    bool fsReady_ = false;
    uint16_t interventionsSeen_ = 0;
};
