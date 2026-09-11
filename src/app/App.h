#pragma once

#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFiClient.h>

#include "core/ConfigStore.h"
#include "core/HubClient.h"

/**
 * Connectivity-Shell: WiFi, Web, OTA, Hub. Noch kein BLE.
 *
 * Die Reihenfolge ist Absicht — eine Firmware ohne `/ota-upload` auf ein
 * Geraet zu flashen, an dem kein Kabel haengt, waere ein Remote-Brick.
 * BleCentral, FtmsClient und die Verdrahtung des Limiters kommen erst,
 * wenn dieser Weg nachweislich steht.
 */
class App {
public:
    static App& instance();

    ConfigStore config;
    HubClient hub;
    WebServer server{80};

    void begin();
    void loop();

    void buildStatusJson(JsonDocument& doc);
    void buildHeartbeat(JsonDocument& doc);
    String statusString();

private:
    App() {}

    void checkResetButton();
    void setupWifi();
    void setupWeb();
    void registerRoutes();
    void runCodecSelfTest();

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

    /** "ok" oder eine Fehlerbeschreibung — landet in /api/status. */
    const char* codecSelfTest_ = "nicht gelaufen";
};
