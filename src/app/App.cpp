#include "App.h"

#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_system.h>
#include <time.h>

#include "ble/FtmsCodec.h"
#include "core/NetUtil.h"
#include "web/UiPages.h"

App& App::instance() {
    static App app;
    return app;
}

// ------------------------------------------------------------------ Start

void App::begin() {
    Serial.begin(115200);
    delay(300);
    Serial.printf("\n=== esp32.ergo v%s (%s) ===\n", FW_VERSION, ERGO_BOARD_LABEL);
    Serial.printf("[BOOT] reset reason %d\n", (int)esp_reset_reason());

    config.begin();
    runCodecSelfTest();
    checkResetButton();
    setupWifi();

    if (config.enableMdns) {
        String mdns = "ergo-" + NetUtil::macNoColon().substring(6);
        if (MDNS.begin(mdns.c_str())) Serial.printf("[mDNS] %s.local\n", mdns.c_str());
    }

    applyLimiterConfig();
    seedDefaultProfiles();
    loadPowerMap();
    journal.begin(ergo::JournalConfig{});
    // Der Ring bleibt nach dem Booten aus und wird bewusst nicht in der
    // Konfiguration gemerkt. Ein Mitschnitt, der einen Neustart ueberlebt und
    // dann monatelang unbemerkt mitlaeuft, ist kein Werkzeug, sondern ein Leck.
    ring.setEnabled(false);
    ftms.begin(&limiter);
    ftms.setDebugRing(&ring);
    ftms.setJournal(&journal);
    ble.begin(&config);
    ble.setLinkEvent([](ergo::Role role, bool up) { App::instance().onLink(role, up); });

    setupWeb();
    hub.begin(&config);
    hub.setPayloadBuilder([](JsonDocument& doc) { App::instance().buildHeartbeat(doc); });
    if (config.enableHub) hub.sendNow();

    // Nur auf ausdrueckliche Ansage. Ein Ergometer, das sich nach einem
    // Stromausfall unaufgefordert wieder ankoppelt, waehrend niemand daneben
    // steht, ist kein Komfortgewinn.
    if (config.autoConnect && config.bikeMac.length()) {
        char err[48] = {0};
        Serial.println("[BLE] autoConnect: verbinde gemerktes Bike");
        ble.reconnectRole(ergo::Role::Bike, err, sizeof(err));
    }
}

/**
 * Die absoluten Schranken kommen aus BuildFlags, der Betriebsbereich spaeter
 * aus 0x2AD6 und die Profilgrenzen aus dem Nutzerprofil. Hier steht bewusst
 * keine Geraeteeigenschaft.
 */
void App::applyLimiterConfig() {
    ergo::LimiterConfig lc;
    lc.absMaxLevelTenths = ERGO_LEVEL_ABS_MAX_TENTHS;
    lc.absMaxPowerW = ERGO_POWER_ABS_MAX_W;
    lc.rampMs = ERGO_LEVEL_RAMP_MS;
    lc.rampStepTenths = 0;  // 0 = Schrittweite des Geraets
    // Deadman bleibt aus, solange keine Regelschleife laeuft. Ein Wachhund,
    // der nichts zu bewachen hat, wuerde nur Stop-Kommandos erzeugen.
    lc.deadmanMs = 0;
    lc.allowSimulation = false;  // bis Nachtest 4
    profiles.applyTo(lc);
    limiter.begin(lc);
}

void App::seedDefaultProfiles() {
    // Zwei Vorlagen, keines aktiv — kein stilles Defaultprofil.
    ergo::Profile std;
    ergo::profileCopyId(std.id, sizeof(std.id), "standard");
    ergo::profileCopyId(std.name, sizeof(std.name), "Standard");
    std.color = 0x4EC9A5;
    std.ftpW = 200;
    std.hrMax = 180;
    std.maxPowerW = 300;
    std.maxLevelTenths = 160;
    std.maxHr = 180;
    std.targetCadenceRpm = 80;
    profiles.put(std);

    ergo::Profile reha;
    ergo::profileCopyId(reha.id, sizeof(reha.id), "reha");
    ergo::profileCopyId(reha.name, sizeof(reha.name), "Reha");
    reha.color = 0xF0A13A;
    reha.ftpW = 80;
    reha.hrMax = 130;
    reha.maxPowerW = 100;
    reha.maxLevelTenths = 80;  // Stufe 8,0
    reha.maxHr = 120;
    reha.targetCadenceRpm = 60;
    reha.onHrLoss = ergo::HrLossPolicy::Stop;
    reha.leadingZone = ergo::ZoneLead::Hr;
    profiles.put(reha);
    Serial.printf("[PROFILE] %u Vorlagen (kein aktives Profil)\n", (unsigned)profiles.count());
}

// ─────────────────────────────────────────────────────────────── BLE-Ereignis

void App::onLink(ergo::Role role, bool up) {
    if (role == ergo::Role::Bike) {
        if (up) {
            if (ftms.attach(ble.client(ergo::Role::Bike))) {
                ble.markReady(ergo::Role::Bike);
                // Steuerhoheit sofort anfragen: ohne sie lehnt ein
                // spec-treues Geraet jedes Lastkommando mit 0x05
                // ControlNotPermitted ab.
                const ergo::FtmsClient::Result r = ftms.requestControl(millis());
                Serial.printf("[FTMS] RequestControl: %s\n",
                              ergo::FtmsClient::resultName(r));

                // Den Stellweg erst jetzt in die Kennflaeche geben: vorher ist
                // er nicht bekannt. Passt er nicht zum Gespeicherten, verwirft
                // PowerMap die alte Flaeche — ein anderes Bike hat eine andere.
                const ftms::Capabilities& c = ftms.capabilities();
                const uint16_t n = c.levelCount();
                if (n > 0) {
                    const uint16_t before = powerMap.pointCount();
                    powerMap.begin((uint8_t)n, c.levelMinTenths(), c.levelStepTenths());
                    Serial.printf("[MAP] %u Stufen ab %d, Schritt %u — %u Stuetzstellen%s\n",
                                  (unsigned)n, (int)c.levelMinTenths(),
                                  (unsigned)c.levelStepTenths(),
                                  (unsigned)powerMap.pointCount(),
                                  (before && !powerMap.pointCount()) ? " (Flaeche verworfen,"
                                                                       " Stellweg passt nicht)"
                                                                     : "");
                }
            } else {
                Serial.println("[FTMS] attach fehlgeschlagen — kein FTMS-Geraet?");
            }
        } else {
            // Ein laufender Sweep ohne Bike stellt Stufen ins Leere. Der
            // Runner wuerde nach `abortAfterMs` selbst abbrechen; das hier
            // spart die Wartezeit und macht den Grund eindeutig.
            if (sweep.running()) {
                Serial.println("[SWEEP] Bike weg — Sweep abgebrochen");
                sweep.cancel(millis());
                harvestSweepPoints();
                savePowerMap();
            }
            ftms.detach();
        }
        return;
    }

    if (up) {
        if (hrc.attach(ble.client(ergo::Role::Hr))) ble.markReady(ergo::Role::Hr);
    } else {
        hrc.detach();
    }
}

// ───────────────────────────────────────────────────────────────── Pulsquelle

ergo::HrSource App::resolveHrSource() const {
    const uint32_t now = millis();
    // Reihenfolge nach Verlaesslichkeit: eigener Gurt, dann das Bike-Feld.
    // Das Relay kommt, wenn die HTTP-Quelle implementiert ist.
    if (hrc.hasSample() && !hrc.stale(now)) return ergo::HrSource::Strap;
    if (ftms.hasLive() && !ftms.stale(now) && ftms.capabilities().ibdReportsHeartRate &&
        ftms.live().heartRateBpm > 0) {
        return ergo::HrSource::Machine;
    }
    return ergo::HrSource::None;
}

uint8_t App::effectiveHr() const {
    switch (resolveHrSource()) {
        case ergo::HrSource::Strap: return hrc.sample().bpm;
        case ergo::HrSource::Machine: return ftms.live().heartRateBpm;
        default: return 0;
    }
}

/**
 * Ein aufgezeichnetes Paket des Varon XTR II durch den Codec schicken.
 *
 * Die eigentliche Pruefung liegt in `pio test -e native`. Das hier beweist
 * etwas anderes: dass derselbe Code auch auf dem S3 dieselben Zahlen
 * liefert — Ausrichtung, Endianness, `int`-Breite.
 */
void App::runCodecSelfTest() {
    static const uint8_t kPacket[] = {0x54, 0x0B, 0x98, 0x08, 0x76, 0x00, 0x13,
                                      0x0A, 0x00, 0x6C, 0x00, 0x0C, 0x00, 0x00,
                                      0x00, 0x00, 0x51, 0xC3, 0x01};
    ftms::IndoorBikeData d;
    if (ftms::decodeIndoorBikeData(kPacket, sizeof(kPacket), d) != ftms::IbdStatus::Ok) {
        codecSelfTest_ = "decode fehlgeschlagen";
    } else if (d.powerW != 108 || d.cadenceRaw != 118 || d.speedRaw != 2200 ||
               d.heartRateBpm != 81 || d.distanceM != 2579u || d.elapsedS != 451) {
        codecSelfTest_ = "Werte weichen ab";
    } else if (d.has(ftms::kResistance)) {
        codecSelfTest_ = "unerwartetes Resistance-Feld";
    } else {
        codecSelfTest_ = "ok";
    }
    Serial.printf("[CODEC] Selbsttest: %s (%d W, %.1f rpm, %.2f km/h, %u bpm)\n", codecSelfTest_,
                  (int)d.powerW, (double)d.cadenceRpm(), (double)d.speedKmh(), d.heartRateBpm);
}

void App::checkResetButton() {
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    if (digitalRead(RESET_BUTTON_PIN) == HIGH) return;
    Serial.printf("[BOOT] Reset-Taste, halte %ds...\n", RESET_HOLD_SEC);
    unsigned long t = millis();
    while (digitalRead(RESET_BUTTON_PIN) == LOW) {
        if (millis() - t > (unsigned long)RESET_HOLD_SEC * 1000UL) {
            WiFiManager wm;
            wm.resetSettings();
            config.factoryReset();
            Serial.println("[BOOT] Werkszustand, Neustart");
            delay(400);
            ESP.restart();
        }
        delay(50);
    }
}

void App::setupWifi() {
    WiFi.mode(WIFI_STA);
    WiFiManager wm;
    WiFiManagerParameter pName("name", "Geraetename", config.deviceName.c_str(), 32);
    WiFiManagerParameter pHost("hub_host", "ESP-Hub IP", config.hubHost.c_str(), 40);
    WiFiManagerParameter pPort("hub_port", "Port", String(config.hubPort).c_str(), 6);
    wm.addParameter(&pName);
    wm.addParameter(&pHost);
    wm.addParameter(&pPort);
    wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);
    wm.setAPCallback([](WiFiManager*) { Serial.println("[WiFi] Portal: " WIFI_AP_NAME); });
    if (!wm.autoConnect(WIFI_AP_NAME)) {
        delay(800);
        ESP.restart();
    }
    config.deviceName = String(pName.getValue());
    config.hubHost = String(pHost.getValue());
    config.hubPort = String(pPort.getValue()).toInt();
    config.save();

    // Modem-Sleep gibt Sendezeit fuer BLE frei — die Koexistenz wird gebraucht,
    // sobald Bike und Gurt gleichzeitig haengen.
    WiFi.setSleep(true);
    Serial.printf("[WiFi] IP %s  Hub %s:%d (sleep on)\n", NetUtil::localIp().c_str(),
                  config.hubHost.c_str(), config.hubPort);
    NetUtil::configureNtp(config);
}

void App::setupWeb() {
    registerRoutes();
    server.onNotFound([this]() {
        server.sendHeader(F("Location"), F("/"), true);
        server.send(302, F("text/plain"), F(""));
    });
    server.begin();
    Serial.printf("[WEB] http://%s/\n", NetUtil::localIp().c_str());
}

// ------------------------------------------------------------------ JSON

void App::buildStatusJson(JsonDocument& doc) {
    doc["name"] = config.deviceName;
    doc["version"] = FW_VERSION;
    doc["fwType"] = FW_TYPE;
    doc["board"] = ERGO_BOARD_ID;
    doc["boardLabel"] = ERGO_BOARD_LABEL;
    doc["chip"] = NetUtil::chipModel();
    doc["ip"] = NetUtil::localIp();
    doc["mac"] = NetUtil::macNoColon();
    doc["rssi"] = WiFi.RSSI();
    doc["uptimeS"] = millis() / 1000UL;
    doc["uptime"] = NetUtil::fmtUptime(millis() / 1000UL);
    doc["heap"] = ESP.getFreeHeap();
    doc["freeSketch"] = ESP.getFreeSketchSpace();
    doc["hub"] = config.hubHost + ":" + String(config.hubPort);
    doc["hubOk"] = hub.lastOk();
    doc["codecSelfTest"] = codecSelfTest_;
    doc["time"] = NetUtil::localNowStr();
    doc["mode"] = ergo::controlModeName(control.mode());
    doc["levelTargetTenths"] = control.levelTargetTenths();
    if (profiles.activeId()) doc["profile"] = profiles.activeId();
    else doc["profile"] = nullptr;

    // tools/deploy.sh liest dieses Feld und verweigert den OTA-Flash, solange
    // ein Bike haengt. Ein Neustart unter Last laesst das Ergometer gebremst
    // stehen — siehe Nachtest 6.
    doc["bikeLink"] = ble.ready(ergo::Role::Bike);

    ble.appendStatusJson(doc["ble"].to<JsonObject>());
    ftms.appendStatusJson(doc["ftms"].to<JsonObject>());
    hrc.appendStatusJson(doc["hr"].to<JsonObject>());

    doc["hrSource"] = ergo::hrSourceName(resolveHrSource());
    doc["heartRate"] = effectiveHr();

    JsonObject lim = doc["limiter"].to<JsonObject>();
    lim["levelTenths"] = limiter.currentLevelTenths();
    lim["maxLevelTenths"] = limiter.effectiveMaxLevelTenths();
    lim["minLevelTenths"] = limiter.effectiveMinLevelTenths();
    lim["maxPowerW"] = limiter.effectiveMaxPowerW();
    lim["writes"] = limiter.writeCount();
    lim["denies"] = limiter.denyCount();
    lim["armed"] = limiter.armed();
    lim["profileMaxLevelTenths"] = limiter.config().profileMaxLevelTenths;
    lim["profileMaxPowerW"] = limiter.config().profileMaxPowerW;

    appendCalibJson(doc["calib"].to<JsonObject>());
    appendDebugJson(doc["debug"].to<JsonObject>());
}

void App::buildHeartbeat(JsonDocument& doc) {
    doc["mac"] = NetUtil::macNoColon();
    doc["name"] = config.deviceName;
    doc["hwType"] = ERGO_HW_TYPE;
    doc["chipModel"] = NetUtil::chipModel();
    doc["version"] = FW_VERSION;
    doc["ip"] = NetUtil::localIp();
    doc["rssi"] = WiFi.RSSI();
    doc["uptime"] = millis() / 1000UL;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["freeSketch"] = ESP.getFreeSketchSpace();
    doc["fwType"] = FW_TYPE;
    doc["board"] = ERGO_BOARD_ID;

    JsonObject ios = doc["ios"].to<JsonObject>();
    JsonObject st = ios["ergo_state"].to<JsonObject>();
    st["type"] = "sensor";
    st["value"] = ergo::linkStateName(ble.link(ergo::Role::Bike).state);
    st["unit"] = "";

    JsonObject cm = ios["control_mode"].to<JsonObject>();
    cm["type"] = "sensor";
    cm["value"] = ergo::controlModeName(control.mode());
    cm["unit"] = "";
    if (profiles.activeId()) {
        JsonObject pr = ios["profile"].to<JsonObject>();
        pr["type"] = "sensor";
        pr["value"] = profiles.activeId();
        pr["unit"] = "";
    }

    ftms.appendIoValues(ios);

    const ergo::HrSource src = resolveHrSource();
    if (src != ergo::HrSource::None) {
        JsonObject hr = ios["heart_rate"].to<JsonObject>();
        hr["type"] = "sensor";
        hr["value"] = effectiveHr();
        hr["unit"] = "bpm";
        JsonObject hs = ios["hr_source"].to<JsonObject>();
        hs["type"] = "sensor";
        hs["value"] = ergo::hrSourceName(src);
        hs["unit"] = "";
    }
}

String App::statusString() {
    JsonDocument doc;
    buildStatusJson(doc);
    return NetUtil::jsonToString(doc);
}

// ------------------------------------------------------------------- Web

void App::handleMain() {
    server.sendHeader(F("Cache-Control"), F("no-store, no-cache, must-revalidate, max-age=0"));
    server.sendHeader(F("Pragma"), F("no-cache"));
    server.sendHeader(F("Expires"), F("0"));
    server.send_P(200, "text/html", PAGE_MAIN);
}

void App::handleOtaUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[OTA] Start %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) Serial.printf("[OTA] OK %u Bytes\n", upload.totalSize);
        else Update.printError(Serial);
    }
}

void App::handleOtaUploadFinish() {
    if (Update.hasError()) server.send(500, F("text/plain"), F("OTA fehlgeschlagen!"));
    else server.send(200, F("text/plain"), F("OK - Neustart..."));
    delay(400);
    ESP.restart();
}

void App::sseSend(const String& data) {
    if (!sseClient_ || !sseClient_.connected()) return;
    sseClient_.print("data: ");
    sseClient_.print(data);
    sseClient_.print("\n\n");
    sseClient_.flush();
}

void App::handleEvents() {
    if (sseClient_ && sseClient_.connected()) sseClient_.stop();
    sseClient_ = server.client();
    sseClient_.print(F("HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/event-stream\r\n"
                       "Cache-Control: no-cache\r\n"
                       "Connection: keep-alive\r\n"
                       "Access-Control-Allow-Origin: *\r\n\r\n"));
    sseClient_.flush();
    sseSend(statusString());
}

void App::registerRoutes() {
    server.on("/", HTTP_GET, [this]() { handleMain(); });
    server.on("/ota", HTTP_GET, [this]() { handleMain(); });
    server.on("/events", HTTP_GET, [this]() { handleEvents(); });
    server.on(
        "/ota-upload", HTTP_POST, [this]() { handleOtaUploadFinish(); },
        [this]() { handleOtaUpload(); });

    server.on("/api/status", HTTP_GET, [this]() {
        JsonDocument doc;
        buildStatusJson(doc);
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/config/get", HTTP_GET, [this]() {
        JsonDocument doc;
        config.toJson(doc.to<JsonObject>());
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/config/save", HTTP_POST, [this]() {
        JsonDocument doc;
        if (!NetUtil::readJsonBody(server, doc)) return;
        if (!config.fromJson(doc.as<JsonVariantConst>())) {
            NetUtil::sendError(server, 400, "Konfiguration nicht lesbar");
            return;
        }
        config.save();
        JsonDocument out;
        out["ok"] = true;
        config.toJson(out["config"].to<JsonObject>());
        NetUtil::sendJson(server, 200, out);
        // Neuer Name sofort an den Hub, statt bis zum naechsten Intervall
        // zu warten — Familienverhalten seit heartrate 0.3.
        if (config.enableHub) hub.sendNow();
    });

    server.on("/api/system/restart", HTTP_POST, [this]() {
        JsonDocument doc;
        doc["ok"] = true;
        // Steht ein Bike unter Last, erst Stop senden. Sonst bleibt das
        // Ergometer gebremst stehen, bis jemand am Rad zieht.
        if (ble.ready(ergo::Role::Bike)) {
            ftms.stop(millis());
            doc["stopSent"] = true;
        }
        NetUtil::sendJson(server, 200, doc);
        restartPending_ = true;
        restartAt_ = millis() + 600;
    });

    registerBleRoutes();
    registerControlRoutes();
    registerProfileRoutes();
    registerCalibRoutes();
    registerDebugRoutes();
}

// ────────────────────────────────────────────────────────────── BLE-Routen

/** Liest `role` aus Query oder Body. Default ist das Bike. */
static ergo::Role roleFromArg(const String& v) {
    return (v == "hr") ? ergo::Role::Hr : ergo::Role::Bike;
}

void App::registerBleRoutes() {
    server.on("/api/ble/scan/start", HTTP_POST, [this]() {
        const uint16_t s = server.hasArg("seconds") ? (uint16_t)server.arg("seconds").toInt() : 8;
        ble.clearScan();
        const bool ok = ble.startScan(s);
        JsonDocument doc;
        doc["ok"] = ok;
        doc["seconds"] = s;
        NetUtil::sendJson(server, ok ? 200 : 500, doc);
    });

    server.on("/api/ble/scan/stop", HTTP_POST, [this]() {
        ble.stopScan();
        JsonDocument doc;
        doc["ok"] = true;
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/ble/devices", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["scanning"] = ble.scanning();
        ble.scanToJson(doc["devices"].to<JsonArray>());
        ble.linksToJson(doc["links"].to<JsonObject>());
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/ble/connect", HTTP_POST, [this]() {
        JsonDocument body;
        if (!NetUtil::readJsonBody(server, body)) return;
        const String mac = body["mac"].as<String>();
        if (mac.length() < 11) {
            NetUtil::sendError(server, 400, "mac fehlt");
            return;
        }
        const ergo::Role role = roleFromArg(body["role"].as<String>());
        const int hint = body["addrType"].isNull() ? -1 : body["addrType"].as<int>();
        char err[64] = {0};
        const bool ok = ble.connectRole(role, mac.c_str(), hint, err, sizeof(err));
        JsonDocument doc;
        doc["ok"] = ok;
        doc["role"] = ergo::roleName(role);
        if (!ok) doc["error"] = err;
        NetUtil::sendJson(server, ok ? 200 : 500, doc);
    });

    server.on("/api/ble/disconnect", HTTP_POST, [this]() {
        const ergo::Role role = roleFromArg(server.arg("role"));
        // Vor dem Trennen Stop: ein Bike, dem man den Link unter der Last
        // wegzieht, haelt den letzten Widerstand.
        if (role == ergo::Role::Bike && ble.ready(role)) ftms.stop(millis());
        ble.disconnectRole(role);
        JsonDocument doc;
        doc["ok"] = true;
        doc["role"] = ergo::roleName(role);
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/ble/forget", HTTP_POST, [this]() {
        const ergo::Role role = roleFromArg(server.arg("role"));
        if (role == ergo::Role::Bike && ble.ready(role)) ftms.stop(millis());
        ble.forgetRole(role);
        JsonDocument doc;
        doc["ok"] = true;
        doc["role"] = ergo::roleName(role);
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/ble/reconnect", HTTP_POST, [this]() {
        const ergo::Role role = roleFromArg(server.arg("role"));
        char err[64] = {0};
        const bool ok = ble.reconnectRole(role, err, sizeof(err));
        JsonDocument doc;
        doc["ok"] = ok;
        if (!ok) doc["error"] = err;
        NetUtil::sendJson(server, ok ? 200 : 500, doc);
    });
}

// ───────────────────────────────────────────────────────── Steuer-Routen

/**
 * OFF und MANUAL_LEVEL. Level-Schreiben verlangt MANUAL_LEVEL (oder setzt ihn
 * beim Hand-Endpunkt, damit der Kurbel-Beweis ohne Extra-Schritt geht).
 * Stop schaltet zurueck auf OFF.
 */
void App::registerControlRoutes() {
    auto reply = [this](ergo::FtmsClient::Result r) {
        JsonDocument doc;
        doc["ok"] = (r == ergo::FtmsClient::Result::Ok);
        doc["result"] = ergo::FtmsClient::resultName(r);
        if (r == ergo::FtmsClient::Result::Denied ||
            r == ergo::FtmsClient::Result::Deferred) {
            doc["reason"] = ftms.lastDenyReason();
        }
        doc["levelTenths"] = limiter.currentLevelTenths();
        doc["mode"] = ergo::controlModeName(control.mode());
        const int code = (r == ergo::FtmsClient::Result::Ok) ? 200
                         : (r == ergo::FtmsClient::Result::Denied) ? 409
                         : (r == ergo::FtmsClient::Result::Deferred) ? 202
                                                                     : 500;
        NetUtil::sendJson(server, code, doc);
    };

    server.on("/api/control/stop", HTTP_POST, [this, reply]() {
        const auto r = ftms.stop(millis());
        control.setMode(ergo::ControlMode::Off);
        reply(r);
    });
    server.on("/api/control/request", HTTP_POST,
              [this, reply]() { reply(ftms.requestControl(millis())); });
    server.on("/api/control/reset", HTTP_POST, [this, reply]() { reply(ftms.reset(millis())); });
    server.on("/api/control/start", HTTP_POST, [this, reply]() { reply(ftms.start(millis())); });

    server.on("/api/control/mode", HTTP_POST, [this]() {
        String token;
        int16_t tenths = -1;
        JsonDocument body;
        const bool hasBody = server.hasArg("plain") && server.arg("plain").length() > 0;
        if (hasBody) {
            if (!NetUtil::readJsonBody(server, body)) return;
            if (!body["mode"].isNull()) token = String(body["mode"].as<const char*>());
            if (!body["tenths"].isNull()) tenths = (int16_t)body["tenths"].as<int>();
            else if (!body["value"].isNull())
                tenths = (int16_t)lroundf(body["value"].as<float>() * 10.0f);
            else if (!body["level"].isNull())
                tenths = (int16_t)lroundf(body["level"].as<float>() * 10.0f);
        }
        if (!token.length() && server.hasArg("mode")) token = server.arg("mode");
        if (tenths < 0 && server.hasArg("tenths")) tenths = (int16_t)server.arg("tenths").toInt();
        if (tenths < 0 && (server.hasArg("value") || server.hasArg("level"))) {
            const float v = server.hasArg("value") ? server.arg("value").toFloat()
                                                   : server.arg("level").toFloat();
            tenths = (int16_t)lroundf(v * 10.0f);
        }

        ergo::ControlMode m;
        if (!ergo::controlModeFromToken(token.c_str(), m)) {
            NetUtil::sendError(server, 400, "mode fehlt oder unbekannt (off|level|…)");
            return;
        }
        if (m != ergo::ControlMode::Off && m != ergo::ControlMode::ManualLevel) {
            NetUtil::sendError(server, 501, "Modus noch nicht implementiert");
            return;
        }
        if (!control.setMode(m)) {
            NetUtil::sendError(server, 409, "Modus abgelehnt");
            return;
        }

        JsonDocument doc;
        doc["ok"] = true;
        doc["mode"] = ergo::controlModeName(control.mode());
        if (m == ergo::ControlMode::ManualLevel && tenths >= 0) {
            control.setLevelTargetTenths(tenths);
            doc["levelTargetTenths"] = tenths;
            if (ble.ready(ergo::Role::Bike)) {
                const auto r = ftms.setLevelTenths(tenths, millis());
                doc["write"] = ergo::FtmsClient::resultName(r);
                if (r == ergo::FtmsClient::Result::Denied ||
                    r == ergo::FtmsClient::Result::Deferred)
                    doc["reason"] = ftms.lastDenyReason();
            }
        }
        if (m == ergo::ControlMode::Off && ble.ready(ergo::Role::Bike)) {
            ftms.stop(millis());
            doc["stopSent"] = true;
        }
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/control/level", HTTP_POST, [this, reply]() {
        if (!server.hasArg("tenths") && !server.hasArg("level")) {
            NetUtil::sendError(server, 400, "tenths oder level fehlt");
            return;
        }
        int16_t tenths;
        if (server.hasArg("tenths")) {
            tenths = (int16_t)server.arg("tenths").toInt();
        } else {
            tenths = (int16_t)lroundf(server.arg("level").toFloat() * 10.0f);
        }
        // Hand-Beweis / UI: Level-Endpunkt schaltet bei Bedarf auf MANUAL_LEVEL.
        if (control.mode() != ergo::ControlMode::ManualLevel) {
            control.setMode(ergo::ControlMode::ManualLevel);
        }
        control.setLevelTargetTenths(tenths);
        reply(ftms.setLevelTenths(tenths, millis()));
    });

    server.on("/api/control/power", HTTP_POST, [this, reply]() {
        if (!server.hasArg("watt")) {
            NetUtil::sendError(server, 400, "watt fehlt");
            return;
        }
        reply(ftms.setPowerW((int16_t)server.arg("watt").toInt(), millis()));
    });
}

// ───────────────────────────────────────────────────────── Profile

void App::profileToJson(const ergo::Profile& p, JsonObject obj) const {
    obj["id"] = p.id;
    obj["name"] = p.name;
    obj["color"] = p.color;
    obj["initial"] = p.initial;
    obj["ftpW"] = p.ftpW;
    obj["ftpDateUnix"] = p.ftpDateUnix;
    obj["hrMax"] = p.hrMax;
    obj["restingHr"] = p.restingHr;
    obj["lthr"] = p.lthr;
    obj["weightKg"] = p.weightKg;
    obj["maxPowerW"] = p.maxPowerW;
    obj["maxHr"] = p.maxHr;
    obj["maxLevelTenths"] = p.maxLevelTenths;
    obj["targetCadenceRpm"] = p.targetCadenceRpm;
    obj["leadingZone"] = (p.leadingZone == ergo::ZoneLead::Hr) ? "hr" : "power";
    obj["zoneBasis"] = (p.zoneBasis == ergo::ZoneBasis::Lthr) ? "lthr" : "hrmax";
    const char* loss = "reduce";
    if (p.onHrLoss == ergo::HrLossPolicy::Freeze) loss = "freeze";
    else if (p.onHrLoss == ergo::HrLossPolicy::Stop) loss = "stop";
    obj["onHrLoss"] = loss;
}

bool App::profileFromJson(JsonVariantConst v, ergo::Profile& out) const {
    if (v["id"].isNull() || v["name"].isNull()) return false;
    out = ergo::Profile{};
    ergo::profileCopyId(out.id, sizeof(out.id), v["id"].as<const char*>());
    ergo::profileCopyId(out.name, sizeof(out.name), v["name"].as<const char*>());
    if (!v["initial"].isNull())
        ergo::profileCopyId(out.initial, sizeof(out.initial), v["initial"].as<const char*>());
    if (!v["color"].isNull()) out.color = v["color"].as<uint32_t>();
    if (!v["ftpW"].isNull()) out.ftpW = (uint16_t)v["ftpW"].as<int>();
    if (!v["ftpDateUnix"].isNull()) out.ftpDateUnix = v["ftpDateUnix"].as<uint32_t>();
    if (!v["hrMax"].isNull()) out.hrMax = (uint8_t)v["hrMax"].as<int>();
    if (!v["restingHr"].isNull()) out.restingHr = (uint8_t)v["restingHr"].as<int>();
    if (!v["lthr"].isNull()) out.lthr = (uint8_t)v["lthr"].as<int>();
    if (!v["weightKg"].isNull()) out.weightKg = (uint8_t)v["weightKg"].as<int>();
    if (!v["maxPowerW"].isNull()) out.maxPowerW = (int16_t)v["maxPowerW"].as<int>();
    if (!v["maxHr"].isNull()) out.maxHr = (uint8_t)v["maxHr"].as<int>();
    if (!v["maxLevelTenths"].isNull()) out.maxLevelTenths = (int16_t)v["maxLevelTenths"].as<int>();
    if (!v["targetCadenceRpm"].isNull())
        out.targetCadenceRpm = (uint8_t)v["targetCadenceRpm"].as<int>();
    if (!v["leadingZone"].isNull()) {
        const char* z = v["leadingZone"].as<const char*>();
        out.leadingZone = (z && strcmp(z, "hr") == 0) ? ergo::ZoneLead::Hr : ergo::ZoneLead::Power;
    }
    if (!v["zoneBasis"].isNull()) {
        const char* z = v["zoneBasis"].as<const char*>();
        out.zoneBasis = (z && strcmp(z, "lthr") == 0) ? ergo::ZoneBasis::Lthr : ergo::ZoneBasis::HrMax;
    }
    if (!v["onHrLoss"].isNull()) {
        const char* z = v["onHrLoss"].as<const char*>();
        if (z && strcmp(z, "freeze") == 0) out.onHrLoss = ergo::HrLossPolicy::Freeze;
        else if (z && strcmp(z, "stop") == 0) out.onHrLoss = ergo::HrLossPolicy::Stop;
        else out.onHrLoss = ergo::HrLossPolicy::Reduce;
    }
    return ergo::ProfileStore::sanitize(out);
}

void App::registerProfileRoutes() {
    server.on("/api/profile/list", HTTP_GET, [this]() {
        JsonDocument doc;
        JsonArray arr = doc["profiles"].to<JsonArray>();
        for (uint8_t i = 0; i < profiles.count(); ++i) {
            const ergo::Profile* p = profiles.at(i);
            if (!p) continue;
            profileToJson(*p, arr.add<JsonObject>());
        }
        if (profiles.activeId()) doc["active"] = profiles.activeId();
        else doc["active"] = nullptr;
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/profile/get", HTTP_GET, [this]() {
        const String id = server.arg("id");
        ergo::Profile p;
        if (!profiles.get(id.c_str(), p)) {
            NetUtil::sendError(server, 404, "Profil nicht gefunden");
            return;
        }
        JsonDocument doc;
        profileToJson(p, doc.to<JsonObject>());
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/profile/put", HTTP_POST, [this]() {
        JsonDocument body;
        if (!NetUtil::readJsonBody(server, body)) {
            NetUtil::sendError(server, 400, "JSON erwartet");
            return;
        }
        ergo::Profile p;
        if (!profileFromJson(body.as<JsonVariantConst>(), p)) {
            NetUtil::sendError(server, 400, "Profil ungueltig");
            return;
        }
        if (!profiles.put(p)) {
            NetUtil::sendError(server, 409, "Profil nicht speicherbar (voll?)");
            return;
        }
        if (profiles.activeId() && strcmp(profiles.activeId(), p.id) == 0) applyLimiterConfig();
        JsonDocument doc;
        doc["ok"] = true;
        profileToJson(p, doc["profile"].to<JsonObject>());
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/profile/delete", HTTP_POST, [this]() {
        String id = server.arg("id");
        if (server.hasArg("plain") && server.arg("plain").length()) {
            JsonDocument body;
            if (NetUtil::readJsonBody(server, body) && !body["id"].isNull())
                id = String(body["id"].as<const char*>());
            else if (!id.length()) return;
        }
        if (!id.length()) {
            NetUtil::sendError(server, 400, "id fehlt");
            return;
        }
        if (!profiles.remove(id.c_str())) {
            NetUtil::sendError(server, 404, "Profil nicht gefunden");
            return;
        }
        applyLimiterConfig();
        JsonDocument doc;
        doc["ok"] = true;
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/profile/select", HTTP_POST, [this]() {
        String id = server.arg("id");
        if (server.hasArg("plain") && server.arg("plain").length()) {
            JsonDocument body;
            if (NetUtil::readJsonBody(server, body) && !body["id"].isNull())
                id = String(body["id"].as<const char*>());
            else if (!id.length() && !server.hasArg("id")) {
                /* empty body already errored */
            }
        }
        const bool locked = control.sessionActive();
        if (!profiles.select(id.length() ? id.c_str() : "", locked)) {
            NetUtil::sendError(server, locked ? 409 : 404,
                               locked ? "Profilwechsel nur im Modus OFF" : "Profil nicht gefunden");
            return;
        }
        applyLimiterConfig();
        JsonDocument doc;
        doc["ok"] = true;
        if (profiles.activeId()) doc["active"] = profiles.activeId();
        else doc["active"] = nullptr;
        doc["limiter"]["profileMaxLevelTenths"] = limiter.config().profileMaxLevelTenths;
        doc["limiter"]["profileMaxPowerW"] = limiter.config().profileMaxPowerW;
        NetUtil::sendJson(server, 200, doc);
    });
}

// ───────────────────────────────────────────────────────── Kalibrierung (marker)

// ───────────────────────────────────────────────────────── Kalibrierung

/**
 * Wie lange die Stufe stehen muss, bevor passiv gelernt wird.
 *
 * Der gefuehrte Sweep laesst 20 s einschwingen. Passiv darf es nicht knapper
 * sein, denn ein Punkt kurz nach einem Stufenwechsel beschreibt einen
 * Uebergang und nicht den Beharrungszustand — und wuerde die Kennflaeche
 * systematisch zu niedrig ziehen.
 */
static constexpr unsigned long kPassiveSettleMs = 20000;
/** Abstand zwischen passiven Punkten. Haeufiger waere kein Informations-
 *  gewinn, nur eine schnellere Mittelung ueber dasselbe Fenster. */
static constexpr unsigned long kPassivePeriodMs = 5000;
/** Wie oft die Flaeche hoechstens ins NVS geht. Flash hat endliche
 *  Schreibzyklen; eine 45-Minuten-Session ist mit neun Schreibvorgaengen
 *  ausreichend gesichert. */
static constexpr unsigned long kMapSaveMs = 300000;

uint32_t App::mapNowS() {
    const time_t t = time(nullptr);
    return (t > 1700000000) ? (uint32_t)t : (uint32_t)(millis() / 1000UL);
}

void App::harvestSweepPoints() {
    while (sweepSeen_ < sweep.pointCount()) {
        const ergo::SweepPoint& p = sweep.point(sweepSeen_++);
        if (p.valid) {
            if (powerMap.add(p.levelTenths, p.meanRpm, p.meanWatt, mapNowS(), true)) {
                mapDirty_ = true;
            }
            Serial.printf("[SWEEP] Stufe %d: %.0f W bei %.1f rpm (%.0f..%.0f, n=%u)\n",
                          (int)p.levelTenths, p.meanWatt, p.meanRpm, p.rpmMin, p.rpmMax,
                          (unsigned)p.samples);
        } else {
            // Verworfene Punkte werden genannt, nicht verschluckt. Genau das
            // ist im ersten Sondenlauf schiefgegangen.
            Serial.printf("[SWEEP] Stufe %d VERWORFEN: %s\n", (int)p.levelTenths, p.reason);
        }
    }
}

void App::loopCalibration(unsigned long now) {
    const bool bike = ble.ready(ergo::Role::Bike);
    const bool fresh = bike && ftms.hasLive() && !ftms.stale(now);
    const float rpm = fresh ? ftms.live().cadenceRpm() : 0.0f;
    const float watt = fresh ? (float)ftms.live().powerW : 0.0f;

    const int16_t lvl = limiter.currentLevelTenths();
    if (lvl != levelWas_) {
        levelWas_ = lvl;
        levelStableSince_ = now;
    }

    if (sweep.running()) {
        const ergo::SweepRunner::Tick t = sweep.tick(now, rpm, watt, fresh);
        if (t.action == ergo::SweepRunner::Tick::Do::SetLevel) {
            // Durch den Limiter wie jeder andere Schreibweg. Lehnt er mit
            // `Deferred` ab, laeuft die Rampe noch — der Runner fasst nach,
            // und das Messfenster beginnt erst mit dem echten Write.
            const ergo::FtmsClient::Result r = ftms.setLevelTenths(t.levelTenths, now);
            if (r == ergo::FtmsClient::Result::Ok) {
                sweep.noteLevelSet(now);
            } else if (r != ergo::FtmsClient::Result::Deferred) {
                Serial.printf("[SWEEP] Stufe %d abgelehnt: %s (%s) — Abbruch\n",
                              (int)t.levelTenths, ergo::FtmsClient::resultName(r),
                              ftms.lastDenyReason());
                sweep.cancel(now);
                ftms.stop(now);
            }
        } else if (t.action == ergo::SweepRunner::Tick::Do::Stop) {
            ftms.stop(now);
        }
        harvestSweepPoints();
    } else if (fresh && rpm >= ergo::kCadMin && watt > 0.0f && lvl >= 0 &&
               now - levelStableSince_ >= kPassiveSettleMs &&
               now - lastPassive_ >= kPassivePeriodMs) {
        // Passives Lernen. Es fuellt die Flaeche genau dort, wo tatsaechlich
        // gefahren wird — und das sind andere Zellen als die des Sweeps, weil
        // niemand auf 60 rpm bleibt.
        lastPassive_ = now;
        if (powerMap.add(lvl, rpm, watt, mapNowS(), false)) mapDirty_ = true;
    }

    const ergo::SweepState st = sweep.state();
    if (st != sweepWas_) {
        sweepWas_ = st;
        if (st == ergo::SweepState::Done || st == ergo::SweepState::Aborted) {
            Serial.printf("[SWEEP] %s — %u von %u Punkten gueltig\n", ergo::sweepStateName(st),
                          (unsigned)sweep.validCount(), (unsigned)sweep.pointCount());
            // Sofort sichern: dahinter stecken bis zu neun Minuten Treten.
            savePowerMap();
        }
    }

    if (mapDirty_ && now - mapSaved_ >= kMapSaveMs) savePowerMap();
}

void App::loadPowerMap() {
    Preferences p;
    if (!p.begin("ergomap", true)) return;
    const size_t len = p.getBytesLength("pmap");
    if (len == 0 || len > ergo::PowerMap::kMaxBytes) {
        p.end();
        return;
    }
    static uint8_t buf[ergo::PowerMap::kMaxBytes];
    const size_t got = p.getBytes("pmap", buf, len);
    p.end();
    if (got != len) return;
    if (powerMap.load(buf, got)) {
        Serial.printf("[MAP] geladen: %u Stufen, %u Stuetzstellen, %u aus Sweeps\n",
                      (unsigned)powerMap.levelCount(), (unsigned)powerMap.pointCount(),
                      (unsigned)powerMap.sweepCells());
    } else {
        Serial.println("[MAP] gespeicherte Kennflaeche unlesbar — verworfen");
    }
}

void App::savePowerMap() {
    if (!mapDirty_) return;
    mapSaved_ = millis();
    if (!powerMap.ready()) return;
    static uint8_t buf[ergo::PowerMap::kMaxBytes];
    const size_t n = powerMap.save(buf, sizeof(buf));
    if (n == 0) return;
    Preferences p;
    if (!p.begin("ergomap", false)) return;
    const bool ok = p.putBytes("pmap", buf, n) == n;
    p.end();
    if (ok) mapDirty_ = false;
    Serial.printf("[MAP] %s (%u Byte, %u Stuetzstellen)\n", ok ? "gesichert" : "Sicherung fehlgeschlagen",
                  (unsigned)n, (unsigned)powerMap.pointCount());
}

void App::appendCalibJson(JsonObject obj) const {
    JsonObject m = obj["map"].to<JsonObject>();
    m["ready"] = powerMap.ready();
    m["levels"] = powerMap.levelCount();
    m["minTenths"] = powerMap.levelMinTenths();
    m["stepTenths"] = powerMap.levelStepTenths();
    m["points"] = powerMap.pointCount();
    m["sweepCells"] = powerMap.sweepCells();
    m["levelsCovered"] = powerMap.levelsCovered();
    m["bandsCovered"] = powerMap.bandsCovered();
    m["truncated"] = powerMap.truncated();

    JsonObject s = obj["sweep"].to<JsonObject>();
    s["state"] = ergo::sweepStateName(sweep.state());
    s["running"] = sweep.running();
    s["index"] = sweep.index();
    s["total"] = sweep.plan().count;
    s["progress"] = sweep.progressPct();
    s["levelTenths"] = sweep.currentLevelTenths();
    s["targetRpm"] = sweep.plan().targetRpm;
    s["minRpm"] = sweep.plan().minRpm;
    s["settleS"] = sweep.plan().settleMs / 1000UL;
    s["windowS"] = sweep.plan().windowMs / 1000UL;
    s["remainingMs"] = sweep.phaseRemainingMs(millis());
    s["valid"] = sweep.validCount();
    if (sweep.state() == ergo::SweepState::Aborted) s["abortReason"] = sweep.abortReason();

    JsonArray pts = s["points"].to<JsonArray>();
    for (uint8_t i = 0; i < sweep.pointCount(); i++) {
        const ergo::SweepPoint& p = sweep.point(i);
        JsonObject o = pts.add<JsonObject>();
        o["levelTenths"] = p.levelTenths;
        o["watt"] = p.meanWatt;
        o["rpm"] = p.meanRpm;
        o["rpmMin"] = p.rpmMin;
        o["rpmMax"] = p.rpmMax;
        o["valid"] = p.valid;
        if (!p.valid) o["reason"] = p.reason;
    }
}

/**
 * Der Sweep ist der einzige Teil dieser Firmware, der von sich aus Last
 * stellt. Deshalb hat er eine eigene Routengruppe und eine eigene Pruefung
 * der Voraussetzungen, statt still zu starten und im Leeren zu laufen.
 */
void App::registerCalibRoutes() {
    server.on("/api/calib/sweep/start", HTTP_POST, [this]() {
        if (!ble.ready(ergo::Role::Bike) || !ftms.attached()) {
            NetUtil::sendError(server, 409, "kein Bike verbunden");
            return;
        }
        if (sweep.running()) {
            NetUtil::sendError(server, 409, "Sweep laeuft schon");
            return;
        }
        const ftms::Capabilities& c = ftms.capabilities();
        if (c.levelCount() == 0) {
            NetUtil::sendError(server, 409, "kein Stellweg gemeldet");
            return;
        }
        const bool coarse = server.arg("coarse") == "1";
        float rpm = server.hasArg("rpm") ? server.arg("rpm").toFloat() : (coarse ? 80.0f : 60.0f);
        if (rpm < 40.0f || rpm > 119.0f) {
            NetUtil::sendError(server, 400, "rpm ausserhalb 40..119");
            return;
        }
        ergo::SweepPlan plan = ergo::SweepRunner::planFor((uint8_t)c.levelCount(),
                                                          c.levelMinTenths(),
                                                          c.levelStepTenths(), rpm, coarse);
        if (server.hasArg("settleS")) {
            plan.settleMs = (uint32_t)server.arg("settleS").toInt() * 1000UL;
        }
        if (server.hasArg("windowS")) {
            plan.windowMs = (uint32_t)server.arg("windowS").toInt() * 1000UL;
        }
        if (!sweep.start(plan, millis())) {
            NetUtil::sendError(server, 500, "Plan leer");
            return;
        }
        sweepSeen_ = 0;
        sweepWas_ = sweep.state();
        Serial.printf("[SWEEP] Start: %u Stufen, Ziel %.0f rpm, %lu s + %lu s je Stufe\n",
                      (unsigned)plan.count, plan.targetRpm, plan.settleMs / 1000UL,
                      plan.windowMs / 1000UL);

        JsonDocument doc;
        doc["ok"] = true;
        doc["levels"] = plan.count;
        doc["targetRpm"] = plan.targetRpm;
        doc["estimateS"] = (uint32_t)plan.count * (plan.settleMs + plan.windowMs) / 1000UL;
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/calib/sweep/stop", HTTP_POST, [this]() {
        const bool was = sweep.running();
        sweep.cancel(millis());
        harvestSweepPoints();
        // Last runter, unabhaengig davon, ob ueberhaupt etwas lief.
        const ergo::FtmsClient::Result r = ftms.stop(millis());
        savePowerMap();
        JsonDocument doc;
        doc["ok"] = true;
        doc["wasRunning"] = was;
        doc["stop"] = ergo::FtmsClient::resultName(r);
        NetUtil::sendJson(server, 200, doc);
    });

    // Die Flaeche als flache Arrays: 128 verschachtelte Objekte kosten auf dem
    // S3 mehr Heap als der ganze Rest der Antwort, und die UI rechnet den
    // Index ohnehin selbst aus.
    server.on("/api/calib/map", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["levels"] = powerMap.levelCount();
        doc["bands"] = ergo::kCadBands;
        doc["minTenths"] = powerMap.levelMinTenths();
        doc["stepTenths"] = powerMap.levelStepTenths();
        doc["cadMin"] = ergo::kCadMin;
        doc["cadStep"] = ergo::kCadStep;
        doc["nowS"] = mapNowS();
        JsonArray w = doc["w"].to<JsonArray>();
        JsonArray n = doc["n"].to<JsonArray>();
        JsonArray sw = doc["s"].to<JsonArray>();
        JsonArray age = doc["t"].to<JsonArray>();
        for (uint8_t l = 0; l < powerMap.levelCount(); l++) {
            for (uint8_t b = 0; b < ergo::kCadBands; b++) {
                const ergo::MapCell& c = powerMap.cell(l, b);
                w.add((int)(c.watt + 0.5f));
                n.add(c.samples);
                sw.add(c.sweep ? 1 : 0);
                age.add(c.lastS);
            }
        }
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/calib/clear", HTTP_POST, [this]() {
        if (sweep.running()) {
            NetUtil::sendError(server, 409, "Sweep laeuft");
            return;
        }
        powerMap.clear();
        sweep.reset();
        sweepSeen_ = 0;
        sweepWas_ = ergo::SweepState::Idle;
        mapDirty_ = true;
        mapSaved_ = 0;
        savePowerMap();
        JsonDocument doc;
        doc["ok"] = true;
        NetUtil::sendJson(server, 200, doc);
    });
}

// ─────────────────────────────────────── Debug-Modus und Steuer-Journal

/**
 * Beides wird hier bedient und nicht im NimBLE-Callback.
 *
 * Der Ring haengt notgedrungen im Callback, weil die Rohbytes nur dort
 * existieren — aber er tut dort auch nichts als kopieren. Das Journal rechnet,
 * urteilt und schiebt Eintraege um; das gehoert in den Haupttask, sonst
 * bezahlt man eine Fehlersuche mit einem Watchdog-Reset.
 */
void App::loopDebug(unsigned long now) {
    const uint32_t lc = ftms.liveCount();
    if (lc != liveSeen_) {
        liveSeen_ = lc;
        if (!ftms.stale(now)) {
            journal.addSample(ftms.live().cadenceRpm(), (float)ftms.live().powerW, now);
        }
    }

    const uint32_t rc = ftms.respCount();
    if (rc != respSeen_) {
        respSeen_ = rc;
        const ftms::ControlResponse& r = ftms.lastResponse();
        journal.noteResponse((uint8_t)r.request, (uint8_t)r.result);
    }

    journal.tick(now);

    if (journal.judged() != judgedSeen_) {
        judgedSeen_ = journal.judged();
        const ergo::JournalEntry* e = journal.at(0);
        if (e) {
            Serial.printf("[JRN] %02X len=%u  %.2f -> %.2f W/rpm (%+.0f %%)  %s%s%s\n",
                          e->cmd[0], (unsigned)e->cmdLen, e->prePerRpm, e->postPerRpm,
                          e->changePct, ergo::effectName(e->effect),
                          e->reason[0] ? " — " : "", e->reason);
            // Der eine Satz, der in der letzten Hardware-Session gefehlt hat.
            if (e->contradictory()) {
                Serial.println(
                    "[JRN] WIDERSPRUCH: Geraet meldet Success, die Messung sieht "
                    "keine Wirkung");
            }
        }
    }
}

void App::appendDebugJson(JsonObject obj) const {
    JsonObject r = obj["ring"].to<JsonObject>();
    r["on"] = ring.enabled();
    r["count"] = ring.count();
    r["slots"] = ergo::kRingSlots;
    r["seen"] = ring.seen();
    r["thinned"] = ring.thinned();
    r["overwritten"] = ring.overwritten();
    r["every"] = ring.ibdEvery();

    JsonObject j = obj["journal"].to<JsonObject>();
    j["judged"] = journal.judged();
    j["worked"] = journal.worked();
    j["noEffect"] = journal.noEffect();
    j["unjudged"] = journal.unjudged();
    j["contradictions"] = journal.contradictions();
    j["pending"] = journal.open() != nullptr;

    JsonArray a = j["entries"].to<JsonArray>();
    for (uint8_t i = 0; i < journal.count(); i++) {
        const ergo::JournalEntry* e = journal.at(i);
        if (!e) continue;
        JsonObject o = a.add<JsonObject>();
        char hex[9] = {0};
        static const char* kHex = "0123456789ABCDEF";
        for (uint8_t k = 0; k < e->cmdLen && k < 4; k++) {
            hex[k * 2] = kHex[(e->cmd[k] >> 4) & 0xF];
            hex[k * 2 + 1] = kHex[e->cmd[k] & 0xF];
        }
        o["cmd"] = hex;
        o["atMs"] = e->atMs;
        o["from"] = e->fromTenths;
        o["to"] = e->toTenths;
        o["preRpm"] = roundf(e->preRpm * 10.0f) / 10.0f;
        o["preWatt"] = roundf(e->preWatt);
        o["postRpm"] = roundf(e->postRpm * 10.0f) / 10.0f;
        o["postWatt"] = roundf(e->postWatt);
        o["prePerRpm"] = roundf(e->prePerRpm * 100.0f) / 100.0f;
        o["postPerRpm"] = roundf(e->postPerRpm * 100.0f) / 100.0f;
        o["changePct"] = roundf(e->changePct);
        o["effect"] = ergo::effectName(e->effect);
        o["reason"] = e->reason;
        o["acked"] = e->responseSeen;
        o["result"] = e->responseResult;
        o["contradictory"] = e->contradictory();
    }
}

void App::registerDebugRoutes() {
    server.on("/api/debug/ring", HTTP_POST, [this]() {
        const String on = server.arg("on");
        if (on.length()) ring.setEnabled(on == "1" || on == "true");
        const String every = server.arg("every");
        if (every.length()) {
            const long n = every.toInt();
            if (n < 0 || n > 1000) {
                JsonDocument d;
                d["ok"] = false;
                d["error"] = "every ausserhalb 0..1000";
                NetUtil::sendJson(server, 400, d);
                return;
            }
            ring.setIbdEvery((uint16_t)n);
        }
        JsonDocument doc;
        doc["ok"] = true;
        // In ein Unterobjekt, nicht in die Wurzel: `to<JsonObject>()` auf dem
        // Dokument leert es und wuerde `ok` gleich wieder wegwerfen.
        appendDebugJson(doc["debug"].to<JsonObject>());
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/debug/clear", HTTP_POST, [this]() {
        ring.clear();
        journal.reset();
        judgedSeen_ = 0;
        JsonDocument doc;
        doc["ok"] = true;
        NetUtil::sendJson(server, 200, doc);
    });

    /**
     * Der Mitschnitt als JSONL — genau das Format, das
     * `tools/make-fixtures.py` als `bike-data.jsonl` liest.
     *
     * Zeilenweise gestreamt statt in einen Puffer gebaut: 256 Datensaetze
     * ergeben ueber 20 kB Text, und so viel zusammenhaengenden Heap gibt der
     * S3 waehrend eines laufenden BLE-Links nicht gern her.
     */
    server.on("/api/debug/export", HTTP_GET, [this]() {
        server.sendHeader("Content-Disposition", "attachment; filename=\"bike-data.jsonl\"");
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, "application/x-ndjson", "");
        char line[160];
        String chunk;
        chunk.reserve(1024);
        for (uint16_t i = 0; i < ring.count(); i++) {
            const ergo::DebugRing::Rec* rec = ring.at(i);
            if (!rec) continue;
            if (!ergo::DebugRing::formatLine(*rec, line, sizeof(line))) continue;
            chunk += line;
            chunk += '\n';
            if (chunk.length() >= 768) {
                server.sendContent(chunk);
                chunk = "";
            }
        }
        if (chunk.length()) server.sendContent(chunk);
        server.sendContent("");
    });
}

// ------------------------------------------------------------------ Loop

void App::loop() {
    server.handleClient();
    ble.loop();
    hub.loop();

    const unsigned long now = millis();
    loopDebug(now);
    loopCalibration(now);

    // Waehrend eines aktiven Links haeufiger senden — beim Fahren sind 2 s
    // eine Ewigkeit, im Leerlauf waere 1 Hz reine Verschwendung.
    const unsigned long sseInterval = ble.ready(ergo::Role::Bike) ? 1000UL : 3000UL;
    if (sseClient_ && sseClient_.connected() && now - lastSse_ >= sseInterval) {
        lastSse_ = now;
        sseSend(statusString());
    }

    if (now - lastWifiCheck_ > 5000UL) {
        lastWifiCheck_ = now;
        if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
    }

    if (config.enableHub && config.watchdogS > 0) {
        if (now - hub.lastSuccessMs() > (unsigned long)config.watchdogS * 1000UL) {
            // Kein Blind-Restart unter Last: erst Stop, dann neu starten.
            // Ein Ergometer, das mit Stufe 14 stehen bleibt, waere der
            // teuerste Weg, einen Hub-Ausfall zu melden.
            if (ble.ready(ergo::Role::Bike)) {
                Serial.println("[WD] Hub schweigt — Stop an das Bike, dann Neustart");
                ftms.stop(now);
                delay(300);
            } else {
                Serial.println("[WD] Hub schweigt, Neustart");
            }
            delay(200);
            ESP.restart();
        }
    }

    if (restartPending_ && now >= restartAt_) ESP.restart();
    delay(2);
}
