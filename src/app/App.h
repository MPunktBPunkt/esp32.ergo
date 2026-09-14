#pragma once

#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFiClient.h>

#include "ble/BleCentral.h"
#include "ble/FtmsClient.h"
#include "ble/FtmsServer.h"
#include "ble/HrClient.h"
#include "ble/DebugRing.h"
#include "control/BridgeAssist.h"
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
#include "control/TestRunner.h"
#include "core/ConfigStore.h"
#include "core/DeviceStore.h"
#include "core/HubClient.h"
#include "core/FtpCareer.h"
#include "core/Progression.h"
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
    FtmsServer bridge;
    ergo::HrClient hrc;
    ergo::Limiter limiter;
    ergo::PowerMap powerMap;
    ergo::PowerController powerCtl;
    ergo::HrController hrCtl;
    ergo::RehaController rehaCtl;
    ergo::BridgeHrCap bridgeHrCap;
    ergo::WorkoutEngine workout;
    ergo::TestRunner testRunner;
    ergo::SweepRunner sweep;
    ergo::DebugRing ring;
    ergo::ControlJournal journal;
    ergo::ProfileStore profiles;
    ergo::DeviceStore devices;
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
    void registerDeviceRoutes();
    void registerDebugRoutes();
    void registerTestRoutes();
    void registerBridgeRoutes();
    void loopBridge(unsigned long now);
    void applyBridgePending(const FtmsServer::Pending& p, unsigned long now);
    void syncBridgeHrLimits();
    float bridgeScaleAppWatt(float appWatt) const;
    /** Bridge-App hat Lastkommandos gesendet — Coach darf nicht mitregeln. */
    bool bridgeExclusiveControl() const;
    void appendBridgeAppJson_(JsonObject obj) const;
    void requestBridgeLevel_(int16_t tenths, unsigned long now);
    void clearBridgeLevelWant_();
    void loopBridgeLevel_(unsigned long now);
    void runCodecSelfTest();
    void applyLimiterConfig();
    void seedDefaultProfiles();
    /** Legt fehlende Vorlagen nach (Martin), ohne bestehende zu überschreiben. */
    void ensureKnownProfiles();
    void loadProfiles();
    void saveProfiles();
    void loadDevices();
    void saveDevices();
    void syncDeviceFromConfig_();
    void applyActiveDeviceOverrides_();
    bool beginFs();
    void recordSessionEnd(const char* reason);
    void beginSession(const char* workoutName = "", const char* workoutId = "");
    void loopSession(unsigned long now);
    void persistSession_(const ergo::SessionSummary& s);
    void loadSessionArchive_();
    bool loadWorkoutDoc(const ergo::WorkoutDoc& doc, float scale);
    void prepareWorkoutDoc(ergo::WorkoutDoc& doc);
    void loadWorkoutMeta_();
    void saveWorkoutMeta_();
    bool isWorkoutFavorite_(const char* id) const;
    void setWorkoutFavorite_(const char* id, bool on);
    uint8_t workoutTagsOf_(const char* id, char out[][ergo::WorkoutDoc::kTagLen],
                           uint8_t maxOut) const;
    void setWorkoutTags_(const char* id, const char tags[][ergo::WorkoutDoc::kTagLen],
                         uint8_t n);
    void appendTagsJson_(JsonArray arr, const char* id,
                         const ergo::WorkoutDoc* fsDoc = nullptr) const;
    void applyAutoPauseForDoc_(const ergo::WorkoutDoc& doc);
    void restoreDefaultAutoPause_();
    void refreshGhost_(const char* workoutId, const char* profileId, const char* mode);
    void clearGhost_();
    void appendProbeJson_(JsonObject obj) const;
    void captureProbeMark_(const char* label);
    uint32_t readProgressionMainS(const char* id) const;
    bool writeProgressionMainS(const char* id, uint32_t mainS);
    void maybeOfferProgression(const ergo::SessionSummary& s);
    void appendProgressionOfferJson(JsonObject obj) const;
    void loadFtpCareer_();
    void saveFtpCareer_();
    void maybeOfferFtpCareer_(const ergo::SessionSummary& s);
    void appendFtpCareerJson_(JsonObject obj) const;

    void profileToJson(const ergo::Profile& p, JsonObject obj) const;
    bool profileFromJson(JsonVariantConst v, ergo::Profile& out) const;

    /** Welcher Puls gilt gerade, und woher. */
    ergo::HrSource resolveHrSource() const;
    /** HTTP 409 mit reason=hr_source_not_trusted. */
    void rejectHrSourceNotTrusted_();
    bool hrControlOk_() const {
        return ergo::hrUsableForControl(resolveHrSource());
    }
    uint8_t effectiveHr() const;

    // ── Kalibrierung ────────────────────────────────────────────────────────
    void loopCalibration(unsigned long now);
    void loopErg(unsigned long now);
    void loopSim(unsigned long now);
    void loopHr(unsigned long now);
    void loopReha(unsigned long now);
    void loopWorkout(unsigned long now);
    /** Gemeinsame Pulsdeckel-Last fuer REHA und WORKOUT. */
    void applyRehaCap(unsigned long now, bool fromWorkout);
    void applyFreezeToLevel(unsigned long now);
    /** Fertige Sweep-Punkte in die Kennflaeche uebernehmen und protokollieren. */
    void harvestSweepPoints();
    void appendCalibJson(JsonObject obj);
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
    bool simDirty_ = false;
    unsigned long lastSimWriteMs_ = 0;
    int16_t lastErgAssistGrade_ = 0;
    unsigned long lastErgAssistWriteMs_ = 0;

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
    uint32_t bridgeLiveSeen_ = 0;
    float bridgeAppWatt_ = 0.0f;   // vor Difficulty
    float bridgeDesiredW_ = 0.0f;  // nach Difficulty, vor HR-Deckel
    bool bridgeDriving_ = false;
    /** Letztes Bridge-SetPower — Resistance kurz danach ignorieren (ERG-Spam). */
    unsigned long lastBridgePowerMs_ = 0;
    uint32_t bridgeResistIgnored_ = 0;
    int16_t lastBridgeResistTenths_ = -1;
    const char* lastBridgeOp_ = "none";
    /** Pending Bridge-Stufe (Zehntel); Retry mit schneller Rampe. */
    int16_t bridgeLevelWant_ = -1;
    unsigned long lastBridgeLevelTryMs_ = 0;
    uint16_t crankRevs_ = 0;
    uint16_t crankEvent_ = 0;
    unsigned long crankLastMs_ = 0;

    const char* codecSelfTest_ = "nicht gelaufen";
    ergo::WorkoutEngine::Tick woSnap_{};
    ergo::SessionSummary lastSession_{};
    ergo::SessionTracker session_;
    ergo::SessionStore sessionStore_;
    uint8_t     zoneUiPrev_ = 0;
    bool sessWasPaused_ = false;
    bool fsReady_ = false;
    uint16_t interventionsSeen_ = 0;
    char activeWorkoutId_[24] = {};
    ergo::WorkoutDoc::Progression activeProg_{};
    ergo::ProgressionOffer progOffer_{};
    ergo::FtpCareerState ftpCareer_{};
    /** Favoriten-IDs (Builtins + Dateien), persistiert in /workouts/meta.json. */
    char woFavIds_[12][24] = {};
    uint8_t woFavCount_ = 0;
    /** Tags für Builtins (FS-Dateien speichern Tags im JSON). */
    struct WoTagRow {
        char id[24] = {};
        char tags[ergo::WorkoutDoc::kMaxTags][ergo::WorkoutDoc::kTagLen] = {};
        uint8_t n = 0;
    };
    WoTagRow woTagRows_[24] = {};
    uint8_t woTagRowCount_ = 0;
    uint16_t pendingAutoPauseS_ = 0;
    ergo::SessionSummary ghost_{};
    bool ghostOk_ = false;

    struct ProbeMark {
        char label[24] = {};
        uint32_t atMs = 0;
        float watt = 0.0f;
        float rpm = 0.0f;
        uint8_t hrBike = 0;
        uint8_t hrStrap = 0;
        uint8_t hrEff = 0;
        int16_t levelTenths = 0;
        bool valid = false;
    };
    static constexpr uint8_t kProbeMarks = 8;
    ProbeMark probeMarks_[kProbeMarks] = {};
    uint8_t probeMarkCount_ = 0;
};
