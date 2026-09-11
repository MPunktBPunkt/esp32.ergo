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

    void begin();
    void load();
    void save();
    void factoryReset();
    void applyDefaults();
    void toJson(JsonObject obj) const;
    bool fromJson(JsonVariantConst obj);

private:
    static constexpr uint8_t kConfigVersion = 1;
};
