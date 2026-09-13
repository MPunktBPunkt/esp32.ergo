#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "BuildFlags.h"

/**
 * Persistenz in zwei NVS-Namespaces:
 *
 *   `esphub` — familienweit geteilt: `name`, `hub_host`, `hub_port`. Ein
 *              Firmwarewechsel zwischen Sonde, heartrate und ergo auf
 *              demselben Chip behaelt diese Felder.
 *   `ergo`   — alles Projektspezifische, versioniert ueber `cfg_ver`.
 *
 * Fehlende Schluessel behalten ihren Default. Neue Felder brauchen deshalb
 * keinen Factory-Reset, nur eine erhoehte `kConfigVersion`.
 */
class ConfigStore {
public:
    String deviceName = DEVICE_NAME_DEFAULT;
    String hubHost = HUB_HOST_DEFAULT;
    int hubPort = HUB_PORT_DEFAULT;

    bool enableHub = true;
    uint16_t heartbeatIntervalS = 30;
    bool enableMdns = true;

    /**
     * Neustart, wenn der Hub so lange schweigt. 0 schaltet ab.
     *
     * Achtung fuer spaeter: sobald ein Bike-Link steht, darf hier nicht mehr
     * einfach neu gestartet werden — erst `08 01` senden, sonst bleibt das
     * Ergometer unter Last stehen. Siehe Test 6 der Nachtests.
     */
    uint16_t watchdogS = 300;

    bool enableNtp = true;
    String ntpServer = NTP_SERVER_DEFAULT;
    String tz = TZ_DEFAULT;

    // ── gemerkte Geraete ────────────────────────────────────────────────────
    //
    // Der Adresstyp wird mitgespeichert, weil er die haeufigste Ursache fuer
    // einen fehlgeschlagenen Reconnect ist: dasselbe Geraet unter public statt
    // random angesprochen antwortet einfach nicht. -1 heisst "unbekannt, beide
    // Varianten probieren".
    String bikeMac;
    String bikeName;
    int8_t bikeAddrType = -1;

    String hrMac;
    String hrName;
    int8_t hrAddrType = -1;

    /**
     * Nach dem Booten selbstaendig verbinden.
     *
     * Default aus: ein Ergometer, das sich beim Stromausfall-Neustart
     * unaufgefordert wieder ankoppelt, waehrend niemand daneben steht, ist
     * kein Komfortgewinn. Der Nutzer startet die Fahrt.
     */
    bool autoConnect = false;

    /**
     * FTMS-Bridge (Peripheral fuer MyWhoosh u. a.).
     * Default aus: wer nicht bewusst bridged, soll nicht als Trainer erscheinen.
     */
    bool bridgeEnabled = false;
    String bridgeName;
    /** 100 = App-Watt unverändert; 50..150. */
    uint16_t bridgeDifficultyPct = 100;
    /** Soft-/Hard-Pulsdeckel; 0 = aus (Profil-maxHr greift nicht automatisch). */
    uint8_t bridgeHrSoft = 0;
    uint8_t bridgeHrMax = 0;

    /**
     * FTMS 0x11 Simulation freigeben (Nachtest 4).
     * Default aus — nur bewusst einschalten, Steigung kann stark wirken.
     */
    bool allowSimulation = false;

    void begin();
    void load();
    void save();
    void factoryReset();
    void applyDefaults();
    void toJson(JsonObject obj) const;
    bool fromJson(JsonVariantConst obj);

private:
    static constexpr uint8_t kConfigVersion = 5;  // 5: allowSimulation
};
