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
    bikeMac = "";
    bikeName = "";
    bikeAddrType = -1;
    hrMac = "";
    hrName = "";
    hrAddrType = -1;
    autoConnect = false;
    bridgeEnabled = false;
    bridgeName = "";
    bridgeDifficultyPct = 100;
    bridgeHrSoft = 0;
    bridgeHrMax = 0;
    allowSimulation = false;
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
    // v2: fehlende Schluessel behalten ihren Default, deshalb braucht der
    // Sprung von v1 keinen Factory-Reset.
    bikeMac = prefs.getString("bk_mac", bikeMac);
    bikeName = prefs.getString("bk_name", bikeName);
    bikeAddrType = (int8_t)prefs.getChar("bk_at", bikeAddrType);
    hrMac = prefs.getString("hr_mac", hrMac);
    hrName = prefs.getString("hr_name", hrName);
    hrAddrType = (int8_t)prefs.getChar("hr_at", hrAddrType);
    autoConnect = prefs.getBool("auto_c", false);  // fehlt der Schluessel: aus
    bridgeEnabled = prefs.getBool("br_en", false);
    bridgeName = prefs.getString("br_name", bridgeName);
    bridgeDifficultyPct = prefs.getUShort("br_diff", 100);
    bridgeHrSoft = prefs.getUChar("br_hrs", 0);
    bridgeHrMax = prefs.getUChar("br_hrm", 0);
    allowSimulation = prefs.getBool("allow_sim", false);
    prefs.end();

    if (heartbeatIntervalS < 5) heartbeatIntervalS = 5;
    if (ntpServer.length() == 0) ntpServer = NTP_SERVER_DEFAULT;
    if (tz.length() == 0) tz = TZ_DEFAULT;
    if (bridgeDifficultyPct < 50) bridgeDifficultyPct = 50;
    if (bridgeDifficultyPct > 150) bridgeDifficultyPct = 150;
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
    prefs.putString("bk_mac", bikeMac);
    prefs.putString("bk_name", bikeName);
    prefs.putChar("bk_at", (int8_t)bikeAddrType);
    prefs.putString("hr_mac", hrMac);
    prefs.putString("hr_name", hrName);
    prefs.putChar("hr_at", (int8_t)hrAddrType);
    prefs.putBool("auto_c", autoConnect);
    prefs.putBool("br_en", bridgeEnabled);
    prefs.putString("br_name", bridgeName);
    prefs.putUShort("br_diff", bridgeDifficultyPct);
    prefs.putUChar("br_hrs", bridgeHrSoft);
    prefs.putUChar("br_hrm", bridgeHrMax);
    prefs.putBool("allow_sim", allowSimulation);
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
    obj["bikeMac"] = bikeMac;
    obj["bikeName"] = bikeName;
    obj["bikeAddrType"] = bikeAddrType;
    obj["hrMac"] = hrMac;
    obj["hrName"] = hrName;
    obj["hrAddrType"] = hrAddrType;
    obj["autoConnect"] = autoConnect;
    obj["bridgeEnabled"] = bridgeEnabled;
    obj["bridgeName"] = bridgeName;
    obj["bridgeDifficultyPct"] = bridgeDifficultyPct;
    obj["bridgeHrSoft"] = bridgeHrSoft;
    obj["bridgeHrMax"] = bridgeHrMax;
    obj["allowSimulation"] = allowSimulation;
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
    bikeMac = jsonString(obj["bikeMac"], bikeMac);
    bikeName = jsonString(obj["bikeName"], bikeName);
    if (!obj["bikeAddrType"].isNull()) bikeAddrType = obj["bikeAddrType"].as<int8_t>();
    hrMac = jsonString(obj["hrMac"], hrMac);
    hrName = jsonString(obj["hrName"], hrName);
    if (!obj["hrAddrType"].isNull()) hrAddrType = obj["hrAddrType"].as<int8_t>();
    if (!obj["autoConnect"].isNull()) autoConnect = obj["autoConnect"].as<bool>();
    if (!obj["bridgeEnabled"].isNull()) bridgeEnabled = obj["bridgeEnabled"].as<bool>();
    bridgeName = jsonString(obj["bridgeName"], bridgeName);
    if (!obj["bridgeDifficultyPct"].isNull())
        bridgeDifficultyPct = obj["bridgeDifficultyPct"].as<uint16_t>();
    if (!obj["bridgeHrSoft"].isNull()) bridgeHrSoft = obj["bridgeHrSoft"].as<uint8_t>();
    if (!obj["bridgeHrMax"].isNull()) bridgeHrMax = obj["bridgeHrMax"].as<uint8_t>();
    if (!obj["allowSimulation"].isNull()) allowSimulation = obj["allowSimulation"].as<bool>();

    if (heartbeatIntervalS < 5) heartbeatIntervalS = 5;
    if (hubPort < 1 || hubPort > 65535) hubPort = HUB_PORT_DEFAULT;
    if (ntpServer.length() == 0) ntpServer = NTP_SERVER_DEFAULT;
    if (tz.length() == 0) tz = TZ_DEFAULT;
    if (bridgeDifficultyPct < 50) bridgeDifficultyPct = 50;
    if (bridgeDifficultyPct > 150) bridgeDifficultyPct = 150;
    return true;
}
