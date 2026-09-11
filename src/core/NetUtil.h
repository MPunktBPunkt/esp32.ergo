#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>

#include "ConfigStore.h"

/** Uebernommen aus esp32.heartrate — bewusst identisch, damit Code zwischen
 *  den Projekten wandern kann. */
namespace NetUtil {

String macNoColon();
String localIp();
String fmtUptime(unsigned long seconds);
String chipModel();
String toHex(const uint8_t* data, size_t len);

void sendJson(WebServer& server, int code, const JsonDocument& doc);
void sendError(WebServer& server, int code, const char* message);
bool readJsonBody(WebServer& server, JsonDocument& doc);
void addCors(WebServer& server);
String jsonToString(const JsonDocument& doc);

/** Startet SNTP (nicht blockierend). Interne Zeitrechnung bleibt auf millis(). */
void configureNtp(const ConfigStore& cfg);
bool timeSynced();
uint32_t unixNow();
String localNowStr();
String formatUnixLocal(uint32_t unix);

}  // namespace NetUtil
