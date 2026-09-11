#include "ConfigStore.h"

#include <Preferences.h>

static Preferences prefs;

void ConfigStore::applyDefaults() {
    deviceName = DEVICE_NAME_DEFAULT;
    hubHost = HUB_HOST_DEFAULT;
    hubPort = HUB_PORT_DEFAULT;
    enableHub = true;
    heartbeatIntervalS = 30;
    enableMdns = true;
    watchdogS = 300;
    enableNtp = true;
    ntpServer = NTP_SERVER_DEFAULT;
    tz = TZ_DEFAULT;
}

void ConfigStore::begin() {
    applyDefaults();
    load();
}

void ConfigStore::load() {
    prefs.begin("esphub", true);
    deviceName = prefs.getString("name", deviceName);
    hubHost = prefs.getString("hub_host", hubHost);
    hubPort = prefs.getInt("hub_port", hubPort);
    prefs.end();

    prefs.begin("ergo", true);
    const uint8_t ver = prefs.getUChar("cfg_ver", 0);
    if (ver == 0) {
        prefs.end();
        return;
    }
    enableHub = prefs.getBool("en_hub", enableHub);
    heartbeatIntervalS = prefs.getUShort("hb_s", heartbeatIntervalS);
    enableMdns = prefs.getBool("en_mdns", enableMdns);
    watchdogS = prefs.getUShort("wdt_s", watchdogS);
    enableNtp = prefs.getBool("en_ntp", enableNtp);
    ntpServer = prefs.getString("ntp", ntpServer);
    tz = prefs.getString("tz", tz);
    prefs.end();

    if (heartbeatIntervalS < 5) heartbeatIntervalS = 5;
    if (ntpServer.length() == 0) ntpServer = NTP_SERVER_DEFAULT;
    if (tz.length() == 0) tz = TZ_DEFAULT;
}

void ConfigStore::save() {
    prefs.begin("esphub", false);
    prefs.putString("name", deviceName);
    prefs.putString("hub_host", hubHost);
    prefs.putInt("hub_port", hubPort);
    prefs.end();

    prefs.begin("ergo", false);
    prefs.putUChar("cfg_ver", kConfigVersion);
    prefs.putBool("en_hub", enableHub);
    prefs.putUShort("hb_s", heartbeatIntervalS);
    prefs.putBool("en_mdns", enableMdns);
    prefs.putUShort("wdt_s", watchdogS);
    prefs.putBool("en_ntp", enableNtp);
    prefs.putString("ntp", ntpServer);
    prefs.putString("tz", tz);
    prefs.end();
}

void ConfigStore::factoryReset() {
    prefs.begin("esphub", false);
    prefs.clear();
    prefs.end();
    prefs.begin("ergo", false);
    prefs.clear();
    prefs.end();
    applyDefaults();
}

void ConfigStore::toJson(JsonObject obj) const {
    obj["deviceName"] = deviceName;
    obj["hubHost"] = hubHost;
    obj["hubPort"] = hubPort;
    obj["enableHub"] = enableHub;
    obj["heartbeatIntervalS"] = heartbeatIntervalS;
    obj["enableMdns"] = enableMdns;
    obj["watchdogS"] = watchdogS;
    obj["enableNtp"] = enableNtp;
    obj["ntpServer"] = ntpServer;
    obj["tz"] = tz;
    obj["board"] = ERGO_BOARD_ID;
    obj["boardLabel"] = ERGO_BOARD_LABEL;
}

static String jsonString(JsonVariantConst v, const String& fallback) {
    if (v.isNull()) return fallback;
    return v.as<String>();
}

bool ConfigStore::fromJson(JsonVariantConst obj) {
    if (!obj.is<JsonObjectConst>()) return false;
    deviceName = jsonString(obj["deviceName"], deviceName);
    hubHost = jsonString(obj["hubHost"], hubHost);
    if (!obj["hubPort"].isNull()) hubPort = obj["hubPort"].as<int>();
    if (!obj["enableHub"].isNull()) enableHub = obj["enableHub"].as<bool>();
    if (!obj["heartbeatIntervalS"].isNull())
        heartbeatIntervalS = obj["heartbeatIntervalS"].as<uint16_t>();
    if (!obj["enableMdns"].isNull()) enableMdns = obj["enableMdns"].as<bool>();
    if (!obj["watchdogS"].isNull()) watchdogS = obj["watchdogS"].as<uint16_t>();
    if (!obj["enableNtp"].isNull()) enableNtp = obj["enableNtp"].as<bool>();
    ntpServer = jsonString(obj["ntpServer"], ntpServer);
    tz = jsonString(obj["tz"], tz);

    if (heartbeatIntervalS < 5) heartbeatIntervalS = 5;
    if (hubPort < 1 || hubPort > 65535) hubPort = HUB_PORT_DEFAULT;
    if (ntpServer.length() == 0) ntpServer = NTP_SERVER_DEFAULT;
    if (tz.length() == 0) tz = TZ_DEFAULT;
    return true;
}
