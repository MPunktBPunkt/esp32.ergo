#include "App.h"

#include <ESPmDNS.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_system.h>

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

    setupWeb();
    hub.begin(&config);
    hub.setPayloadBuilder([](JsonDocument& doc) { App::instance().buildHeartbeat(doc); });
    if (config.enableHub) hub.sendNow();
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
    // In der Shell gibt es noch keine Messwerte. Der Zustand selbst ist aber
    // schon eine Information, die der Hub anzeigen kann.
    JsonObject st = ios["ergo_state"].to<JsonObject>();
    st["type"] = "sensor";
    st["value"] = "SHELL";
    st["unit"] = "";
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
        NetUtil::sendJson(server, 200, doc);
        restartPending_ = true;
        restartAt_ = millis() + 400;
    });
}

// ------------------------------------------------------------------ Loop

void App::loop() {
    server.handleClient();
    hub.loop();

    const unsigned long now = millis();

    if (sseClient_ && sseClient_.connected() && now - lastSse_ >= 2000UL) {
        lastSse_ = now;
        sseSend(statusString());
    }

    if (now - lastWifiCheck_ > 5000UL) {
        lastWifiCheck_ = now;
        if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
    }

    if (config.enableHub && config.watchdogS > 0) {
        if (now - hub.lastSuccessMs() > (unsigned long)config.watchdogS * 1000UL) {
            // Sobald ein Bike-Link besteht, darf hier nicht mehr blind neu
            // gestartet werden — erst `08 01` senden. Siehe Test 6.
            Serial.println("[WD] Hub schweigt, Neustart");
            delay(200);
            ESP.restart();
        }
    }

    if (restartPending_ && now >= restartAt_) ESP.restart();
    delay(2);
}
