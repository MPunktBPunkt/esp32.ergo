#pragma once

#include "FtmsCodec.h"
#include "core/ConfigStore.h"
#include <ArduinoJson.h>
#include <NimBLEDevice.h>

/**
 * FTMS-Peripheral der Bridge (v0.3).
 *
 * Vorbild: HrServer in esp32.heartrate. Central bleibt BleCentral; dieser
 * Server legt zusaetzlich 0x1826 an und wirbt als Indoor Bike.
 *
 * Aufwertung gegenueber dem Varon: eigenes 0x2ACC mit kTgtPower und 0x2AD8.
 * Eingehende 0x05 werden NICHT ans Bike weitergereicht — App wendet sie ueber
 * PowerController/Limiter als Stufen an (STATE.md Regel 5).
 *
 * Zusaetzlich CPS (0x1818) und CSC (0x1816) fuer Apps, die darauf bestehen.
 *
 * Im NimBLE-Callback nur: Bytes lesen, Pending setzen, Response indikaten.
 */
class FtmsServer {
public:
    enum class PendingOp : uint8_t {
        None = 0,
        RequestControl,
        Reset,
        Start,
        Stop,
        Pause,
        SetPower,
        SetResistance,
        SetSimulation,
    };

    struct Pending {
        PendingOp op = PendingOp::None;
        int16_t watt = 0;
        int16_t resistanceTenths = 0;
        int16_t windMms = 0;
        int16_t gradeHundredth = 0;
        uint8_t crr10000 = 0;
        uint8_t cw100 = 0;
    };

    void begin(ConfigStore* cfg);
    void loop();

    void setEnabled(bool on);
    bool enabled() const { return enabled_; }
    bool advertising() const { return advertising_; }
    bool controlGranted() const { return controlGranted_; }
    uint8_t clients() const;
    uint8_t subscribedIbd() const;
    const char* name() const { return name_; }

    /** Live-Paket vom Bike (ggf. mit ueberlagertem Puls) an Abonnenten. */
    void notifyIndoorBike(const ftms::IndoorBikeData& d);
    /** CPS Instantaneous Power + CSC Crank aus dem Live-Strom. */
    void notifyCycling(int16_t watt, uint16_t cumCrank, uint16_t crankEvent1024);

    /** Wenn true: 0x11 wird gequeued statt NotSupported (Nachtest 4). */
    void setAllowSimulation(bool on) { allowSim_ = on; }
    bool allowSimulation() const { return allowSim_; }

    /** Ein Pending aus dem CP-Callback holen (App-loop). */
    bool takePending(Pending& out);

    void appendStatusJson(JsonObject obj) const;

    // NimBLE — nur Flags / Zaehler
    void onConnect(uint16_t connId);
    void onDisconnect(uint16_t connId);
    void onControlWrite(NimBLECharacteristic* c);

private:
    void ensureServer();
    void startAdvertising();
    void stopAdvertising();
    void disconnectAll();
    void refreshAdvertising();
    void seedStaticChars();
    void indicateControlResponse(ftms::Opcode request, ftms::ControlResult result);
    void notifyMachineStatus(uint8_t op, const uint8_t* param, size_t paramLen);
    String effectiveName() const;

    ConfigStore* cfg_ = nullptr;
    NimBLEServer* server_ = nullptr;
    NimBLECharacteristic* feature_ = nullptr;
    NimBLECharacteristic* ibd_ = nullptr;
    NimBLECharacteristic* resRange_ = nullptr;
    NimBLECharacteristic* pwrRange_ = nullptr;
    NimBLECharacteristic* cp_ = nullptr;
    NimBLECharacteristic* status_ = nullptr;
    NimBLECharacteristic* cpsMeas_ = nullptr;
    NimBLECharacteristic* cscMeas_ = nullptr;

    bool enabled_ = false;
    bool serverReady_ = false;
    bool advertising_ = false;
    bool controlGranted_ = false;
    bool allowSim_ = false;

    volatile bool pendingReady_ = false;
    Pending pending_{};

    uint32_t notifySent_ = 0;
    uint32_t indicateSent_ = 0;
    uint32_t cpWrites_ = 0;
    uint32_t cpsSent_ = 0;
    uint32_t cscSent_ = 0;
    unsigned long lastNotifyMs_ = 0;
    char name_[32] = {0};
};
