#include "App.h"

#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_system.h>
#include <math.h>
#include <time.h>

#include "ble/FtmsCodec.h"
#include "core/Progression.h"
#include "core/NetUtil.h"
#include "core/Zone.h"
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
    // Alte Probe-Namen still auf Ergo ziehen (einmalig, NVS).
    if (config.deviceName == "FtmsProbe-S3" || config.deviceName == "FtmsProbe") {
        config.deviceName = DEVICE_NAME_DEFAULT;
        config.save();
        Serial.printf("[CFG] Geraetename → %s\n", config.deviceName.c_str());
    }
    beginFs();
    runCodecSelfTest();
    checkResetButton();
    setupWifi();

    if (config.enableMdns) {
        String mdns = "ergo-" + NetUtil::macNoColon().substring(6);
        if (MDNS.begin(mdns.c_str())) Serial.printf("[mDNS] %s.local\n", mdns.c_str());
    }

    loadProfiles();
    if (profiles.count() == 0) {
        seedDefaultProfiles();
        saveProfiles();
    } else {
        ensureKnownProfiles();
    }
    applyLimiterConfig();
    loadPowerMap();
    powerCtl.begin({});
    hrCtl.begin({});
    rehaCtl.begin({});
    rehaCtl.setDesiredW(60.0f);
    rehaCtl.setHrLimits(115, 120);
    rehaCtl.setDurationS(600);
    session_.begin({});
    loadSessionArchive_();
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
    // Vorlagen, keines aktiv — kein stilles Defaultprofil.
    ergo::Profile martin;
    ergo::profileCopyId(martin.id, sizeof(martin.id), "martin");
    ergo::profileCopyId(martin.name, sizeof(martin.name), "Martin");
    ergo::profileCopyId(martin.initial, sizeof(martin.initial), "M");
    martin.color = 0xE2802F;
    martin.ftpW = 180;
    martin.ftpOrigin = ergo::FtpOrigin::Estimate;
    martin.birthYear = 1981;
    martin.hrMax = ergo::ProfileStore::estimateHrMax(1981, 2026);
    martin.maxHr = martin.hrMax;
    martin.weightKg = 84;
    martin.goal = ergo::TrainingGoal::FatLoss;
    martin.maxPowerW = 280;
    martin.maxLevelTenths = 160;
    martin.targetCadenceRpm = 80;
    martin.onHrLoss = ergo::HrLossPolicy::Freeze;
    martin.leadingZone = ergo::ZoneLead::Power;
    profiles.put(martin);

    ergo::Profile reha;
    ergo::profileCopyId(reha.id, sizeof(reha.id), "reha");
    ergo::profileCopyId(reha.name, sizeof(reha.name), "Reha");
    ergo::profileCopyId(reha.initial, sizeof(reha.initial), "R");
    reha.color = 0xF0A13A;
    reha.ftpW = 80;
    reha.hrMax = 130;
    reha.maxPowerW = 100;
    reha.maxLevelTenths = 80;
    reha.maxHr = 120;
    reha.targetCadenceRpm = 60;
    reha.onHrLoss = ergo::HrLossPolicy::Stop;
    reha.leadingZone = ergo::ZoneLead::Hr;
    reha.goal = ergo::TrainingGoal::Reha;
    profiles.put(reha);
    Serial.printf("[PROFILE] %u Vorlagen (kein aktives Profil)\n", (unsigned)profiles.count());
}

void App::ensureKnownProfiles() {
    bool dirty = false;
    ergo::Profile existing;
    if (!profiles.get("martin", existing)) {
        if (profiles.count() < ergo::ProfileStore::kMaxProfiles) {
            ergo::Profile martin;
            ergo::profileCopyId(martin.id, sizeof(martin.id), "martin");
            ergo::profileCopyId(martin.name, sizeof(martin.name), "Martin");
            ergo::profileCopyId(martin.initial, sizeof(martin.initial), "M");
            martin.color = 0xE2802F;
            martin.ftpW = 180;
            martin.ftpOrigin = ergo::FtpOrigin::Estimate;
            martin.birthYear = 1981;
            martin.hrMax = ergo::ProfileStore::estimateHrMax(1981, 2026);
            martin.maxHr = martin.hrMax;
            martin.weightKg = 84;
            martin.goal = ergo::TrainingGoal::FatLoss;
            martin.maxPowerW = 280;
            martin.maxLevelTenths = 160;
            martin.targetCadenceRpm = 80;
            martin.onHrLoss = ergo::HrLossPolicy::Freeze;
            martin.leadingZone = ergo::ZoneLead::Power;
            if (profiles.put(martin)) {
                dirty = true;
                Serial.println("[PROFILE] Martin nachgetragen");
            }
        }
    } else {
        // Lücken in bekannten Feldern auffüllen, ohne gesetzte Werte zu überschreiben.
        bool touch = false;
        if (existing.weightKg == 0) {
            existing.weightKg = 84;
            touch = true;
        }
        if (existing.birthYear == 0) {
            existing.birthYear = 1981;
            touch = true;
        }
        if (existing.hrMax == 0 && existing.birthYear > 0) {
            existing.hrMax = ergo::ProfileStore::estimateHrMax(existing.birthYear, 2026);
            if (existing.maxHr == 0) existing.maxHr = existing.hrMax;
            touch = true;
        }
        if (existing.goal == ergo::TrainingGoal::None) {
            existing.goal = ergo::TrainingGoal::FatLoss;
            touch = true;
        }
        if (!existing.initial[0]) {
            ergo::profileCopyId(existing.initial, sizeof(existing.initial), "M");
            touch = true;
        }
        if (touch && profiles.put(existing)) {
            dirty = true;
            Serial.println("[PROFILE] Martin-Felder ergänzt");
        }
    }
    if (dirty) saveProfiles();
}

void App::loadProfiles() {
    Preferences p;
    if (!p.begin("ergoprofs", true)) return;
    const size_t len = p.getBytesLength("profs");
    if (len == 0 || len > ergo::ProfileStore::kMaxBytes) {
        p.end();
        return;
    }
    static uint8_t buf[ergo::ProfileStore::kMaxBytes];
    const size_t got = p.getBytes("profs", buf, len);
    p.end();
    if (got != len) return;
    if (profiles.load(buf, got)) {
        Serial.printf("[PROFILE] geladen: %u Profile%s%s\n", (unsigned)profiles.count(),
                      profiles.activeId() ? ", aktiv=" : "",
                      profiles.activeId() ? profiles.activeId() : "");
    } else {
        Serial.println("[PROFILE] gespeicherte Profile unlesbar — verworfen");
    }
}

void App::saveProfiles() {
    static uint8_t buf[ergo::ProfileStore::kMaxBytes];
    const size_t n = profiles.save(buf, sizeof(buf));
    if (n == 0) return;
    Preferences p;
    if (!p.begin("ergoprofs", false)) return;
    const bool ok = p.putBytes("profs", buf, n) == n;
    p.end();
    Serial.printf("[PROFILE] %s (%u Byte, %u Profile)\n", ok ? "gesichert" : "Sicherung fehlgeschlagen",
                  (unsigned)n, (unsigned)profiles.count());
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
    doc["powerTargetW"] = control.powerTargetW();
    JsonObject erg = doc["erg"].to<JsonObject>();
    erg["targetW"] = powerCtl.targetW();
    erg["smoothedW"] = powerCtl.smoothedW();
    erg["ceiling"] = powerCtl.ceiling();
    erg["levelTenths"] = powerCtl.lastLevelTenths();
    erg["mapReady"] = powerMap.ready() && powerMap.pointCount() > 0;
    doc["hrTargetBpm"] = control.hrTargetBpm();
    JsonObject hrhold = doc["hrHold"].to<JsonObject>();
    hrhold["targetBpm"] = hrCtl.targetHr();
    hrhold["powerTargetW"] = hrCtl.powerTargetW();
    hrhold["lost"] = hrCtl.lost();
    hrhold["smoothedHr"] = hrCtl.smoothedHr();
    if (const ergo::Profile* ap = profiles.active()) {
        const char* loss = "reduce";
        if (ap->onHrLoss == ergo::HrLossPolicy::Freeze) loss = "freeze";
        else if (ap->onHrLoss == ergo::HrLossPolicy::Stop) loss = "stop";
        hrhold["onHrLoss"] = loss;
    }
    JsonObject reha = doc["reha"].to<JsonObject>();
    reha["desiredW"] = rehaCtl.desiredW();
    reha["effectiveW"] = rehaCtl.effectiveW();
    reha["hrMax"] = rehaCtl.hrMax();
    reha["hrSoft"] = rehaCtl.hrSoft();
    reha["capActive"] = rehaCtl.capActive();
    reha["interventions"] = rehaCtl.interventions();
    reha["durationS"] = rehaCtl.durationS();
    reha["elapsedS"] = rehaCtl.elapsedS();
    reha["remainingS"] = rehaCtl.remainingS();
    reha["lost"] = rehaCtl.lost();
    reha["finished"] = rehaCtl.finished();
    if (const ergo::Profile* ap = profiles.active()) {
        const char* loss = "reduce";
        if (ap->onHrLoss == ergo::HrLossPolicy::Freeze) loss = "freeze";
        else if (ap->onHrLoss == ergo::HrLossPolicy::Stop) loss = "stop";
        reha["onHrLoss"] = loss;
    }
    JsonObject wo = doc["workout"].to<JsonObject>();
    wo["name"] = workout.name();
    wo["state"] = ergo::WorkoutEngine::stateName(workout.state());
    wo["stepIndex"] = workout.stepIndex();
    wo["stepCount"] = workout.stepCount();
    wo["label"] = woSnap_.label ? woSnap_.label : "";
    wo["desiredW"] = woSnap_.desiredW;
    wo["hrMax"] = woSnap_.hrMax;
    wo["hrSoft"] = woSnap_.hrSoft;
    wo["stepRemainingS"] = woSnap_.stepRemainingS;
    wo["totalRemainingS"] = woSnap_.totalRemainingS;
    wo["elapsedS"] = woSnap_.elapsedS;
    if (lastSession_.valid) {
        JsonObject ls = doc["lastSession"].to<JsonObject>();
        ls["mode"] = lastSession_.mode;
        ls["workoutName"] = lastSession_.workoutName;
        ls["profileId"] = lastSession_.profileId;
        ls["endReason"] = lastSession_.endReason;
        ls["durationS"] = lastSession_.durationS;
        ls["pausedS"] = lastSession_.pausedS;
        ls["steps"] = lastSession_.steps;
        ls["interventions"] = lastSession_.interventions;
        ls["autoPauses"] = lastSession_.autoPauses;
        ls["avgPowerW"] = lastSession_.avgPowerW;
        ls["avgDesiredW"] = lastSession_.avgDesiredW;
        ls["workKj"] = lastSession_.workKj;
        ls["hrAvg"] = lastSession_.hrAvg;
        ls["hrMax"] = lastSession_.hrMax;
        ls["leadHr"] = lastSession_.leadHr;
        ls["zoneCount"] = lastSession_.zoneCount;
        JsonArray zta = ls["zoneTimeS"].to<JsonArray>();
        for (uint8_t i = 0; i < lastSession_.zoneCount && i < ergo::kPowerZones; i++)
            zta.add(lastSession_.zoneTimeS[i]);
    }
    {
        JsonObject sess = doc["session"].to<JsonObject>();
        sess["active"] = session_.active();
        sess["paused"] = session_.paused();
        sess["durationS"] = session_.elapsedActiveS(millis());
        sess["autoPauses"] = session_.peek().autoPauses;
        sess["interventions"] = session_.peek().interventions;
        sess["avgPowerW"] = session_.peek().avgPowerW;
        sess["workKj"] = session_.peek().workKj;
        sess["zone"] = session_.currentZone();
        sess["leadHr"] = session_.peek().leadHr;
        sess["zoneCount"] = session_.peek().zoneCount;
        JsonArray zta = sess["zoneTimeS"].to<JsonArray>();
        for (uint8_t i = 0; i < session_.peek().zoneCount && i < ergo::kPowerZones; i++)
            zta.add(session_.peek().zoneTimeS[i]);
    }
    doc["fsReady"] = fsReady_;
    doc["sessionCount"] = sessionStore_.count();

    {
        float watt = 0.0f;
        if (ftms.hasLive() && !ftms.stale(millis())) watt = (float)ftms.live().powerW;
        const uint8_t hr = effectiveHr();
        bool leadHr = false;
        uint16_t ftp = 0;
        uint8_t hrMax = 0;
        if (const ergo::Profile* ap = profiles.active()) {
            leadHr = (ap->leadingZone == ergo::ZoneLead::Hr);
            ftp = ap->ftpW;
            hrMax = ap->hrMax ? ap->hrMax : ap->maxHr;
        }
        const uint8_t raw = leadHr ? ergo::zoneFromHr(hr, hrMax)
                                   : ergo::zoneFromPowerW(watt, ftp);
        uint8_t idx = raw;
        if (session_.active() && session_.currentZone() > 0)
            idx = session_.currentZone();
        else if (zoneUiPrev_ > 0) {
            idx = leadHr ? ergo::zoneFromHr(hr, hrMax, zoneUiPrev_)
                         : ergo::zoneFromPowerW(watt, ftp, zoneUiPrev_);
        }
        zoneUiPrev_ = idx;
        const ergo::ZoneInfo zi =
            leadHr ? ergo::hrZoneInfo(idx) : ergo::powerZoneInfo(idx);
        JsonObject z = doc["zone"].to<JsonObject>();
        z["index"] = zi.index;
        z["code"] = zi.code;
        z["name"] = zi.name;
        z["color"] = zi.colorHex;
        z["lead"] = leadHr ? "hr" : "power";
        z["ftpW"] = ftp;
        z["hrMax"] = hrMax;
    }

    if (const ergo::Profile* ap = profiles.active()) {
        doc["profile"] = ap->id;
        JsonObject po = doc["profileInfo"].to<JsonObject>();
        po["id"] = ap->id;
        po["name"] = ap->name;
        po["color"] = ap->color;
        po["ftpW"] = ap->ftpW;
        po["hrMax"] = ap->hrMax;
        po["maxHr"] = ap->maxHr;
        po["maxPowerW"] = ap->maxPowerW;
        po["maxLevelTenths"] = ap->maxLevelTenths;
        po["targetCadenceRpm"] = ap->targetCadenceRpm;
        po["leadingZone"] = (ap->leadingZone == ergo::ZoneLead::Hr) ? "hr" : "power";
        po["weightKg"] = ap->weightKg;
        po["birthYear"] = ap->birthYear;
        po["initial"] = ap->initial;
        const char* goal = "none";
        if (ap->goal == ergo::TrainingGoal::Fitness) goal = "fitness";
        else if (ap->goal == ergo::TrainingGoal::FatLoss) goal = "fatloss";
        else if (ap->goal == ergo::TrainingGoal::Reha) goal = "reha";
        else if (ap->goal == ergo::TrainingGoal::Performance) goal = "performance";
        po["goal"] = goal;
        if (ap->ftpW > 0 && ap->weightKg > 0)
            po["wPerKg"] = (float)ap->ftpW / (float)ap->weightKg;
    } else {
        doc["profile"] = nullptr;
        doc["profileInfo"] = nullptr;
    }

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

    {
        JsonObject sa = ios["session_active"].to<JsonObject>();
        sa["type"] = "sensor";
        sa["value"] = control.sessionActive() ? 1 : 0;
        sa["unit"] = "";
        if (session_.active() || control.sessionActive()) {
            JsonObject sd = ios["session_duration"].to<JsonObject>();
            sd["type"] = "sensor";
            sd["value"] = (int)session_.elapsedActiveS(millis());
            sd["unit"] = "s";
            JsonObject sp = ios["session_paused"].to<JsonObject>();
            sp["type"] = "sensor";
            sp["value"] = session_.paused() ? 1 : 0;
            sp["unit"] = "";
            JsonObject wk = ios["work_kj"].to<JsonObject>();
            wk["type"] = "sensor";
            wk["value"] = session_.peek().workKj;
            wk["unit"] = "kJ";
        }
        if (control.powerTargetW() > 0.0f) {
            JsonObject pt = ios["power_target"].to<JsonObject>();
            pt["type"] = "sensor";
            pt["value"] = (int)lroundf(control.powerTargetW());
            pt["unit"] = "W";
        }
        if (control.levelTargetTenths() >= 0) {
            JsonObject lt = ios["level_target"].to<JsonObject>();
            lt["type"] = "sensor";
            lt["value"] = control.levelTargetTenths() / 10.0f;
            lt["unit"] = "";
        }
        if (control.allowsWorkout() && workout.running()) {
            JsonObject wn = ios["workout_name"].to<JsonObject>();
            wn["type"] = "sensor";
            wn["value"] = workout.name();
            wn["unit"] = "";
            JsonObject ws = ios["workout_step"].to<JsonObject>();
            ws["type"] = "sensor";
            ws["value"] = woSnap_.label ? woSnap_.label : "";
            ws["unit"] = "";
            JsonObject wr = ios["workout_remaining"].to<JsonObject>();
            wr["type"] = "sensor";
            wr["value"] = (int)woSnap_.totalRemainingS;
            wr["unit"] = "s";
        }
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
 * OFF, MANUAL_LEVEL, MANUAL_ERG. Last schreiben verlangt ein aktives Profil.
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
    auto requireProfile = [this]() -> bool {
        if (profiles.active()) return true;
        NetUtil::sendError(server, 409, "Profil wählen (Reiter Profile)");
        return false;
    };

    server.on("/api/control/stop", HTTP_POST, [this, reply]() {
        if (control.sessionActive()) recordSessionEnd("panic");
        const auto r = ftms.stop(millis());
        control.setMode(ergo::ControlMode::Off);
        powerCtl.reset();
        hrCtl.reset();
        rehaCtl.reset();
        workout.stop();
        reply(r);
    });
    server.on("/api/control/request", HTTP_POST,
              [this, reply]() { reply(ftms.requestControl(millis())); });
    server.on("/api/control/reset", HTTP_POST, [this, reply]() { reply(ftms.reset(millis())); });
    server.on("/api/control/start", HTTP_POST, [this, reply]() { reply(ftms.start(millis())); });

    server.on("/api/control/mode", HTTP_POST, [this, requireProfile]() {
        String token;
        int16_t tenths = -1;
        float watt = -1.0f;
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
            if (!body["watt"].isNull()) watt = body["watt"].as<float>();
        }
        if (!token.length() && server.hasArg("mode")) token = server.arg("mode");
        if (tenths < 0 && server.hasArg("tenths")) tenths = (int16_t)server.arg("tenths").toInt();
        if (tenths < 0 && (server.hasArg("value") || server.hasArg("level"))) {
            const float v = server.hasArg("value") ? server.arg("value").toFloat()
                                                   : server.arg("level").toFloat();
            tenths = (int16_t)lroundf(v * 10.0f);
        }
        if (watt < 0.0f && server.hasArg("watt")) watt = server.arg("watt").toFloat();

        ergo::ControlMode m;
        if (!ergo::controlModeFromToken(token.c_str(), m)) {
            NetUtil::sendError(server, 400, "mode fehlt oder unbekannt (off|level|erg|…)");
            return;
        }
        if (m != ergo::ControlMode::Off && m != ergo::ControlMode::ManualLevel &&
            m != ergo::ControlMode::ManualErg && m != ergo::ControlMode::HrHold &&
            m != ergo::ControlMode::Reha && m != ergo::ControlMode::Workout) {
            NetUtil::sendError(server, 501, "Modus noch nicht implementiert");
            return;
        }
        if (m != ergo::ControlMode::Off && !requireProfile()) return;
        if ((m == ergo::ControlMode::ManualErg || m == ergo::ControlMode::HrHold ||
             m == ergo::ControlMode::Reha || m == ergo::ControlMode::Workout) &&
            (!powerMap.ready() || powerMap.pointCount() == 0)) {
            NetUtil::sendError(server, 409, "Kennfläche leer — zuerst Kalibrierung");
            return;
        }
        if (!control.setMode(m)) {
            NetUtil::sendError(server, 409, "Modus abgelehnt");
            return;
        }

        if (m == ergo::ControlMode::Off) {
            if (session_.active()) recordSessionEnd("stop");
        } else if (m != ergo::ControlMode::Workout) {
            if (session_.active()) recordSessionEnd("switch");
            beginSession("");
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
        if (m == ergo::ControlMode::ManualErg) {
            powerCtl.reset();
            hrCtl.reset();
            rehaCtl.reset();
            if (watt > 0.0f) {
                control.setPowerTargetW(watt);
                powerCtl.setTargetW(watt);
            }
            doc["powerTargetW"] = control.powerTargetW();
            doc["mapPoints"] = powerMap.pointCount();
        }
        if (m == ergo::ControlMode::HrHold) {
            powerCtl.reset();
            hrCtl.reset();
            rehaCtl.reset();
            int hr = server.hasArg("hr") ? server.arg("hr").toInt() : 0;
            if (hasBody && !body["hr"].isNull()) hr = body["hr"].as<int>();
            if (hr <= 0 && profiles.active() && profiles.active()->maxHr > 0)
                hr = profiles.active()->maxHr > 10 ? profiles.active()->maxHr - 10 : 100;
            if (hr <= 0) hr = 130;
            control.setHrTargetBpm((uint8_t)hr);
            hrCtl.setTargetHr((uint8_t)hr);
            hrCtl.setLossPolicy(profiles.active() ? profiles.active()->onHrLoss
                                                  : ergo::HrLossPolicy::Reduce);
            float maxW = 180.0f;
            float minW = 25.0f;
            float base = 80.0f;
            if (const ergo::Profile* ap = profiles.active()) {
                if (ap->maxPowerW > 0) maxW = (float)ap->maxPowerW;
                if (ap->ftpW > 0) base = (float)ap->ftpW * 0.55f;
            }
            if (watt > 0.0f) base = watt;
            hrCtl.setPowerLimits(minW, maxW);
            hrCtl.setBasePowerW(base);
            powerCtl.setTargetW(base);
            control.setPowerTargetW(base);
            doc["hrTargetBpm"] = control.hrTargetBpm();
            doc["powerTargetW"] = base;
            doc["mapPoints"] = powerMap.pointCount();
        }
        if (m == ergo::ControlMode::Reha) {
            powerCtl.reset();
            hrCtl.reset();
            rehaCtl.reset();
            float w = watt;
            if (w <= 0.0f && hasBody && !body["watt"].isNull()) w = body["watt"].as<float>();
            if (w <= 0.0f) w = 60.0f;
            int hrMax = server.hasArg("hrMax") ? server.arg("hrMax").toInt() : 0;
            if (hasBody && !body["hrMax"].isNull()) hrMax = body["hrMax"].as<int>();
            if (hrMax <= 0 && profiles.active() && profiles.active()->maxHr > 0)
                hrMax = profiles.active()->maxHr;
            if (hrMax <= 0) hrMax = 120;
            int hrSoft = server.hasArg("hrSoft") ? server.arg("hrSoft").toInt() : 0;
            if (hasBody && !body["hrSoft"].isNull()) hrSoft = body["hrSoft"].as<int>();
            if (hrSoft <= 0) hrSoft = hrMax > 5 ? hrMax - 5 : hrMax;
            int dur = server.hasArg("durationS") ? server.arg("durationS").toInt() : -1;
            if (hasBody && !body["durationS"].isNull()) dur = body["durationS"].as<int>();
            if (dur < 0) dur = 600;
            if (const ergo::Profile* ap = profiles.active()) {
                if (ap->maxPowerW > 0 && w > (float)ap->maxPowerW) w = (float)ap->maxPowerW;
                rehaCtl.setLossPolicy(ap->onHrLoss);
            } else {
                rehaCtl.setLossPolicy(ergo::HrLossPolicy::Reduce);
            }
            rehaCtl.setDesiredW(w);
            rehaCtl.setHrLimits((uint8_t)hrSoft, (uint8_t)hrMax);
            rehaCtl.setDurationS((uint32_t)dur);
            rehaCtl.reset();
            control.setPowerTargetW(w);
            powerCtl.setTargetW(w);
            doc["powerTargetW"] = w;
            doc["hrMax"] = rehaCtl.hrMax();
            doc["hrSoft"] = rehaCtl.hrSoft();
            doc["durationS"] = rehaCtl.durationS();
            doc["mapPoints"] = powerMap.pointCount();
            workout.stop();
        }
        if (m == ergo::ControlMode::Workout) {
            powerCtl.reset();
            hrCtl.reset();
            rehaCtl.reset();
            float scale = server.hasArg("scale") ? server.arg("scale").toFloat() : 1.0f;
            if (hasBody && !body["scale"].isNull()) scale = body["scale"].as<float>();
            if (scale <= 0.0f) scale = 1.0f;
            String id = server.hasArg("id") ? server.arg("id") : "physio";
            if (hasBody && !body["id"].isNull()) id = String(body["id"].as<const char*>());
            if (const ergo::Profile* ap = profiles.active()) {
                workout.setFtpW(ap->ftpW);
                rehaCtl.setLossPolicy(ap->onHrLoss);
            } else {
                workout.setFtpW(0);
                rehaCtl.setLossPolicy(ergo::HrLossPolicy::Reduce);
            }
            ergo::WorkoutDoc docIn;
            char err[64] = {0};
            bool okDoc = ergo::workoutBuiltinById(id.c_str(), docIn);
            if (!okDoc && fsReady_) {
                String path = String("/workouts/") + id + ".json";
                File f = LittleFS.open(path, "r");
                if (f) {
                    String bodyStr = f.readString();
                    f.close();
                    okDoc = ergo::workoutParseJson(bodyStr.c_str(), docIn, err, sizeof(err));
                }
            }
            if (!okDoc || !loadWorkoutDoc(docIn, scale)) {
                NetUtil::sendError(server, 404, err[0] ? err : "Workout nicht geladen");
                return;
            }
            if (session_.active()) recordSessionEnd("switch");
            beginSession(workout.name());
            doc["workout"] = workout.name();
            doc["id"] = docIn.id;
            doc["scale"] = scale;
            doc["steps"] = workout.stepCount();
            doc["mapPoints"] = powerMap.pointCount();
        }
        if (m == ergo::ControlMode::ManualErg || m == ergo::ControlMode::HrHold) {
            workout.stop();
        }
        if (m == ergo::ControlMode::Off && ble.ready(ergo::Role::Bike)) {
            ftms.stop(millis());
            powerCtl.reset();
            hrCtl.reset();
            rehaCtl.reset();
            workout.stop();
            doc["stopSent"] = true;
        } else if (m == ergo::ControlMode::Off) {
            workout.stop();
        }
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/control/level", HTTP_POST, [this, reply, requireProfile]() {
        if (!requireProfile()) return;
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
        if (control.mode() != ergo::ControlMode::ManualLevel) {
            if (session_.active()) recordSessionEnd("switch");
            control.setMode(ergo::ControlMode::ManualLevel);
            beginSession("");
        } else if (!session_.active()) {
            beginSession("");
        }
        control.setLevelTargetTenths(tenths);
        reply(ftms.setLevelTenths(tenths, millis()));
    });

    server.on("/api/control/power", HTTP_POST, [this, reply, requireProfile]() {
        if (!requireProfile()) return;
        if (!server.hasArg("watt")) {
            NetUtil::sendError(server, 400, "watt fehlt");
            return;
        }
        const float wattIn = server.arg("watt").toFloat();
        float watt = wattIn;
        if (const ergo::Profile* ap = profiles.active()) {
            if (ap->maxPowerW > 0 && watt > (float)ap->maxPowerW) watt = (float)ap->maxPowerW;
        }
        // Emuliertes ERG-Ziel (nicht Opcode 0x05 — der ist am Varon tot).
        if (server.arg("raw") == "1") {
            reply(ftms.setPowerW((int16_t)wattIn, millis()));
            return;
        }
        if (control.allowsReha()) {
            rehaCtl.setDesiredW(watt);
            control.setPowerTargetW(rehaCtl.effectiveW());
            powerCtl.setTargetW(rehaCtl.effectiveW());
            JsonDocument doc;
            doc["ok"] = true;
            doc["mode"] = "REHA";
            doc["desiredW"] = rehaCtl.desiredW();
            doc["effectiveW"] = rehaCtl.effectiveW();
            NetUtil::sendJson(server, 200, doc);
            return;
        }
        if (control.mode() != ergo::ControlMode::ManualErg) {
            if (!powerMap.ready() || powerMap.pointCount() == 0) {
                NetUtil::sendError(server, 409, "Kennfläche leer — zuerst Kalibrierung");
                return;
            }
            if (session_.active()) recordSessionEnd("switch");
            control.setMode(ergo::ControlMode::ManualErg);
            beginSession("");
        } else if (!session_.active()) {
            beginSession("");
        }
        control.setPowerTargetW(watt);
        powerCtl.setTargetW(watt);
        JsonDocument doc;
        doc["ok"] = true;
        doc["mode"] = "MANUAL_ERG";
        doc["powerTargetW"] = watt;
        doc["ceiling"] = powerCtl.ceiling();
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/control/hr", HTTP_POST, [this, requireProfile]() {
        if (!requireProfile()) return;
        if (!control.allowsHrHold()) {
            NetUtil::sendError(server, 409, "nicht im Modus HR_HOLD");
            return;
        }
        int bpm = server.hasArg("bpm") ? server.arg("bpm").toInt()
                                       : (server.hasArg("hr") ? server.arg("hr").toInt() : 0);
        if (bpm <= 0) {
            NetUtil::sendError(server, 400, "bpm fehlt");
            return;
        }
        if (!control.setHrTargetBpm((uint8_t)bpm)) {
            NetUtil::sendError(server, 400, "bpm ungueltig (40..220)");
            return;
        }
        hrCtl.setTargetHr((uint8_t)bpm);
        JsonDocument doc;
        doc["ok"] = true;
        doc["mode"] = "HR_HOLD";
        doc["hrTargetBpm"] = control.hrTargetBpm();
        doc["powerTargetW"] = hrCtl.powerTargetW();
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/control/reha", HTTP_POST, [this, requireProfile]() {
        if (!requireProfile()) return;
        if (!control.allowsReha()) {
            NetUtil::sendError(server, 409, "nicht im Modus REHA");
            return;
        }
        if (server.hasArg("watt")) {
            float w = server.arg("watt").toFloat();
            if (const ergo::Profile* ap = profiles.active()) {
                if (ap->maxPowerW > 0 && w > (float)ap->maxPowerW) w = (float)ap->maxPowerW;
            }
            rehaCtl.setDesiredW(w);
            control.setPowerTargetW(rehaCtl.effectiveW());
            powerCtl.setTargetW(rehaCtl.effectiveW());
        }
        if (server.hasArg("hrMax") || server.hasArg("hrSoft")) {
            int hard = server.hasArg("hrMax") ? server.arg("hrMax").toInt() : rehaCtl.hrMax();
            int soft = server.hasArg("hrSoft") ? server.arg("hrSoft").toInt() : rehaCtl.hrSoft();
            rehaCtl.setHrLimits((uint8_t)soft, (uint8_t)hard);
        }
        if (server.hasArg("durationS")) {
            rehaCtl.setDurationS((uint32_t)server.arg("durationS").toInt());
        }
        JsonDocument doc;
        doc["ok"] = true;
        doc["mode"] = "REHA";
        doc["desiredW"] = rehaCtl.desiredW();
        doc["effectiveW"] = rehaCtl.effectiveW();
        doc["hrMax"] = rehaCtl.hrMax();
        doc["hrSoft"] = rehaCtl.hrSoft();
        doc["durationS"] = rehaCtl.durationS();
        NetUtil::sendJson(server, 200, doc);
    });

    auto workoutJson = [this](JsonDocument& doc) {
        doc["ok"] = true;
        doc["mode"] = ergo::controlModeName(control.mode());
        doc["name"] = workout.name();
        doc["state"] = ergo::WorkoutEngine::stateName(workout.state());
        doc["stepIndex"] = workout.stepIndex();
        doc["stepCount"] = workout.stepCount();
        doc["label"] = woSnap_.label ? woSnap_.label : "";
        doc["desiredW"] = woSnap_.desiredW;
        doc["stepRemainingS"] = woSnap_.stepRemainingS;
        doc["totalRemainingS"] = woSnap_.totalRemainingS;
    };

    server.on("/api/workout/start", HTTP_POST, [this, requireProfile, workoutJson]() {
        if (!requireProfile()) return;
        if (!powerMap.ready() || powerMap.pointCount() == 0) {
            NetUtil::sendError(server, 409, "Kennfläche leer — zuerst Kalibrierung");
            return;
        }
        float scale = server.hasArg("scale") ? server.arg("scale").toFloat() : 1.0f;
        if (scale <= 0.0f) scale = 1.0f;
        String id = server.hasArg("id") ? server.arg("id") : "physio";
        ergo::WorkoutDoc docIn;
        char err[64];
        bool ok = false;
        if (ergo::workoutBuiltinById(id.c_str(), docIn)) {
            ok = true;
        } else if (fsReady_) {
            String path = String("/workouts/") + id + ".json";
            File f = LittleFS.open(path, "r");
            if (f) {
                String body = f.readString();
                f.close();
                ok = ergo::workoutParseJson(body.c_str(), docIn, err, sizeof(err));
            } else {
                strncpy(err, "Datei nicht gefunden", sizeof(err) - 1);
            }
        } else {
            strncpy(err, "unbekanntes Programm", sizeof(err) - 1);
        }
        if (!ok) {
            NetUtil::sendError(server, 404, err[0] ? err : "Workout nicht geladen");
            return;
        }
        if (!control.setMode(ergo::ControlMode::Workout)) {
            NetUtil::sendError(server, 409, "Modus abgelehnt");
            return;
        }
        powerCtl.reset();
        hrCtl.reset();
        rehaCtl.reset();
        if (const ergo::Profile* ap = profiles.active()) {
            workout.setFtpW(ap->ftpW);
            rehaCtl.setLossPolicy(ap->onHrLoss);
        }
        if (!loadWorkoutDoc(docIn, scale)) {
            NetUtil::sendError(server, 500, "Workout-Start fehlgeschlagen");
            return;
        }
        if (session_.active()) recordSessionEnd("switch");
        beginSession(workout.name());
        JsonDocument doc;
        workoutJson(doc);
        doc["scale"] = scale;
        doc["id"] = docIn.id;
        NetUtil::sendJson(server, 200, doc);
    });
    server.on("/api/workout/skip", HTTP_POST, [this, workoutJson]() {
        if (!control.allowsWorkout()) {
            NetUtil::sendError(server, 409, "kein Workout aktiv");
            return;
        }
        workout.skip(millis());
        woSnap_ = workout.tick(millis());
        if (woSnap_.finished) {
            recordSessionEnd("done");
            if (ble.ready(ergo::Role::Bike)) ftms.stop(millis());
            control.setMode(ergo::ControlMode::Off);
            powerCtl.reset();
            rehaCtl.reset();
            workout.stop();
        } else {
            rehaCtl.setDesiredW(woSnap_.desiredW);
            rehaCtl.setHrLimits(woSnap_.hrSoft, woSnap_.hrMax ? woSnap_.hrMax : 120);
            control.setPowerTargetW(woSnap_.desiredW);
            powerCtl.setTargetW(woSnap_.desiredW);
        }
        JsonDocument doc;
        workoutJson(doc);
        NetUtil::sendJson(server, 200, doc);
    });
    server.on("/api/workout/pause", HTTP_POST, [this, workoutJson]() {
        if (!control.allowsWorkout()) {
            NetUtil::sendError(server, 409, "kein Workout aktiv");
            return;
        }
        workout.pause(millis());
        woSnap_.state = ergo::WorkoutState::Paused;
        JsonDocument doc;
        workoutJson(doc);
        NetUtil::sendJson(server, 200, doc);
    });
    server.on("/api/workout/resume", HTTP_POST, [this, workoutJson]() {
        if (!control.allowsWorkout()) {
            NetUtil::sendError(server, 409, "kein Workout aktiv");
            return;
        }
        workout.resume(millis());
        woSnap_ = workout.tick(millis());
        JsonDocument doc;
        workoutJson(doc);
        NetUtil::sendJson(server, 200, doc);
    });
    server.on("/api/workout/stop", HTTP_POST, [this, workoutJson]() {
        recordSessionEnd("stop");
        if (ble.ready(ergo::Role::Bike)) ftms.stop(millis());
        control.setMode(ergo::ControlMode::Off);
        powerCtl.reset();
        rehaCtl.reset();
        workout.stop();
        woSnap_ = {};
        JsonDocument doc;
        workoutJson(doc);
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/workout/list", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["ok"] = true;
        doc["fsReady"] = fsReady_;
        JsonArray builtins = doc["builtins"].to<JsonArray>();
        for (uint8_t i = 0; i < ergo::workoutBuiltinCount(); i++) {
            JsonObject o = builtins.add<JsonObject>();
            o["id"] = ergo::workoutBuiltinId(i);
            o["name"] = ergo::workoutBuiltinName(i);
            o["source"] = "builtin";
        }
        JsonArray files = doc["files"].to<JsonArray>();
        if (fsReady_) {
            File root = LittleFS.open("/workouts");
            if (root && root.isDirectory()) {
                File f = root.openNextFile();
                while (f) {
                    String name = f.name();
                    if (name.endsWith(".json")) {
                        JsonObject o = files.add<JsonObject>();
                        int slash = name.lastIndexOf('/');
                        String base = slash >= 0 ? name.substring(slash + 1) : name;
                        if (base.endsWith(".json")) base.remove(base.length() - 5);
                        o["id"] = base;
                        o["name"] = base;
                        o["source"] = "fs";
                        o["bytes"] = (int)f.size();
                    }
                    f = root.openNextFile();
                }
            }
        }
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/workout/validate", HTTP_POST, [this]() {
        if (!server.hasArg("plain")) {
            NetUtil::sendError(server, 400, "JSON-Body fehlt");
            return;
        }
        ergo::WorkoutDoc d;
        char err[80];
        const bool ok = ergo::workoutParseJson(server.arg("plain").c_str(), d, err, sizeof(err));
        JsonDocument doc;
        doc["ok"] = ok;
        if (ok) {
            doc["id"] = d.id;
            doc["name"] = d.name;
            doc["steps"] = d.stepCount;
            uint16_t ftp = 0;
            int16_t maxPw = 0;
            uint8_t maxHr = 0;
            const char* pid = nullptr;
            if (const ergo::Profile* ap = profiles.active()) {
                ftp = ap->ftpW;
                maxPw = ap->maxPowerW;
                maxHr = ap->maxHr ? ap->maxHr : ap->hrMax;
                pid = ap->id;
            }
            doc["profileId"] = pid ? pid : nullptr;
            doc["ftpW"] = ftp;
            uint32_t totalS = 0;
            float peakW = 0.0f;
            float workJ = 0.0f;
            bool needFtp = false;
            JsonArray arr = doc["timeline"].to<JsonArray>();
            for (uint8_t i = 0; i < d.stepCount; i++) {
                const ergo::WorkoutStep& st = d.steps[i];
                totalS += st.durationS;
                float w = st.powerW;
                if (w <= 0.0f && st.ftpPct > 0.0f) {
                    needFtp = true;
                    if (ftp > 0) w = (float)ftp * st.ftpPct / 100.0f;
                }
                if (w > peakW) peakW = w;
                if (w > 0.0f) workJ += w * (float)st.durationS;
                JsonObject o = arr.add<JsonObject>();
                o["i"] = i;
                o["label"] = st.label;
                o["durationS"] = st.durationS;
                if (st.powerW > 0.0f) o["powerW"] = st.powerW;
                if (st.ftpPct > 0.0f) o["ftpPct"] = st.ftpPct;
                if (w > 0.0f) o["resolvedW"] = w;
                o["hrMax"] = st.hrMax;
                o["hrSoft"] = st.hrSoft;
            }
            doc["durationS"] = totalS;
            doc["peakW"] = peakW;
            doc["avgW"] = totalS > 0 ? (workJ / (float)totalS) : 0.0f;
            doc["needFtp"] = needFtp;
            JsonArray warns = doc["warnings"].to<JsonArray>();
            if (needFtp && ftp == 0) warns.add("ftp_pct braucht FTP im aktiven Profil");
            if (maxPw > 0 && peakW > (float)maxPw) {
                String wmsg = "Spitze " + String((int)peakW) + " W über Profil-max " + String((int)maxPw) + " W";
                warns.add(wmsg);
            }
            if (maxHr > 0) {
                for (uint8_t i = 0; i < d.stepCount; i++) {
                    if (d.steps[i].hrMax > maxHr) {
                        warns.add("Schritt-Pulsdeckel über Profil-maxHr");
                        break;
                    }
                }
            }
            if (powerMap.ready() && powerMap.pointCount() > 0 && peakW > 0.0f) {
                bool ceil = false;
                int16_t lvl = -1;
                if (powerMap.bestLevel(peakW, 80.0f, lvl, ceil)) {
                    doc["mapLevelTenths"] = lvl;
                    doc["mapCeiling"] = ceil;
                    if (ceil) warns.add("Spitze über Kennfläche bei ~80 rpm");
                }
            }
            doc["feasible"] = warns.size() == 0;
        } else {
            doc["error"] = err;
        }
        NetUtil::sendJson(server, ok ? 200 : 400, doc);
    });

    server.on("/api/workout/put", HTTP_POST, [this, requireProfile]() {
        if (!requireProfile()) return;
        if (!fsReady_) {
            NetUtil::sendError(server, 503, "LittleFS nicht bereit");
            return;
        }
        if (!server.hasArg("plain")) {
            NetUtil::sendError(server, 400, "JSON-Body fehlt");
            return;
        }
        ergo::WorkoutDoc d;
        char err[80];
        if (!ergo::workoutParseJson(server.arg("plain").c_str(), d, err, sizeof(err))) {
            NetUtil::sendError(server, 400, err);
            return;
        }
        char path[48];
        snprintf(path, sizeof(path), "/workouts/%s.json", d.id);
        File f = LittleFS.open(path, "w");
        if (!f) {
            NetUtil::sendError(server, 500, "Schreiben fehlgeschlagen");
            return;
        }
        char buf[1536];
        const size_t n = ergo::workoutWriteJson(d, buf, sizeof(buf));
        if (n == 0 || f.write((const uint8_t*)buf, n) != n) {
            f.close();
            NetUtil::sendError(server, 500, "Write unvollstaendig");
            return;
        }
        f.close();
        JsonDocument doc;
        doc["ok"] = true;
        doc["id"] = d.id;
        doc["path"] = path;
        doc["bytes"] = (int)n;
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/workout/download", HTTP_GET, [this]() {
        if (!server.hasArg("id")) {
            NetUtil::sendError(server, 400, "id fehlt");
            return;
        }
        const String id = server.arg("id");
        ergo::WorkoutDoc d;
        char err[64];
        char buf[1536];
        if (ergo::workoutBuiltinById(id.c_str(), d)) {
            prepareWorkoutDoc(d);
            const size_t n = ergo::workoutWriteJson(d, buf, sizeof(buf));
            server.send(200, "application/json", n ? buf : "{}");
            return;
        }
        if (!fsReady_) {
            NetUtil::sendError(server, 404, "nicht gefunden");
            return;
        }
        String path = String("/workouts/") + id + ".json";
        File f = LittleFS.open(path, "r");
        if (!f) {
            NetUtil::sendError(server, 404, "nicht gefunden");
            return;
        }
        server.streamFile(f, "application/json");
        f.close();
        (void)err;
    });

    server.on("/api/session/last", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["ok"] = lastSession_.valid;
        if (lastSession_.valid) {
            doc["mode"] = lastSession_.mode;
            doc["workoutName"] = lastSession_.workoutName;
            doc["profileId"] = lastSession_.profileId;
            doc["endReason"] = lastSession_.endReason;
            doc["durationS"] = lastSession_.durationS;
            doc["pausedS"] = lastSession_.pausedS;
            doc["steps"] = lastSession_.steps;
            doc["interventions"] = lastSession_.interventions;
            doc["autoPauses"] = lastSession_.autoPauses;
            doc["avgPowerW"] = lastSession_.avgPowerW;
            doc["avgDesiredW"] = lastSession_.avgDesiredW;
            doc["workKj"] = lastSession_.workKj;
            doc["hrAvg"] = lastSession_.hrAvg;
            doc["hrMax"] = lastSession_.hrMax;
            doc["leadHr"] = lastSession_.leadHr;
            doc["zoneCount"] = lastSession_.zoneCount;
            JsonArray zta = doc["zoneTimeS"].to<JsonArray>();
            for (uint8_t i = 0; i < lastSession_.zoneCount && i < ergo::kPowerZones; i++)
                zta.add(lastSession_.zoneTimeS[i]);
            if (progOffer_.workoutId[0])
                appendProgressionOfferJson(doc["progression"].to<JsonObject>());
        }
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/progression/get", HTTP_GET, [this]() {
        String id = server.hasArg("id") ? server.arg("id") : "physio";
        ergo::WorkoutDoc d;
        if (!ergo::workoutBuiltinById(id.c_str(), d) && fsReady_) {
            String path = String("/workouts/") + id + ".json";
            File f = LittleFS.open(path, "r");
            char err[48];
            if (f) {
                String body = f.readString();
                f.close();
                ergo::workoutParseJson(body.c_str(), d, err, sizeof(err));
            }
        }
        JsonDocument doc;
        doc["ok"] = d.progression.enabled || progOffer_.workoutId[0];
        doc["id"] = id;
        doc["enabled"] = d.progression.enabled;
        doc["stepS"] = d.progression.stepS ? d.progression.stepS : 60;
        doc["maxS"] = d.progression.maxS ? d.progression.maxS : 1800;
        const uint32_t base = d.progression.baseDurationS
                                  ? d.progression.baseDurationS
                                  : (d.stepCount > 1 ? d.steps[1].durationS : 600);
        uint32_t cur = readProgressionMainS(id.c_str());
        if (cur == 0) cur = base;
        doc["baseMainS"] = base;
        doc["currentMainS"] = cur;
        doc["nextMainS"] = ergo::progressionNextMainS(cur, doc["stepS"], doc["maxS"]);
        if (progOffer_.pending || progOffer_.workoutId[0])
            appendProgressionOfferJson(doc["offer"].to<JsonObject>());
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/progression/accept", HTTP_POST, [this]() {
        String id = server.hasArg("id") ? server.arg("id") : String(progOffer_.workoutId);
        if (!id.length()) id = "physio";
        uint32_t cur = progOffer_.currentMainS;
        uint16_t step = progOffer_.stepS ? progOffer_.stepS : 60;
        uint32_t maxS = progOffer_.maxS ? progOffer_.maxS : 1800;
        if (!progOffer_.clean) {
            NetUtil::sendError(server, 409, progOffer_.reason[0] ? progOffer_.reason : "nicht sauber");
            return;
        }
        uint32_t next = progOffer_.nextMainS ? progOffer_.nextMainS
                                             : ergo::progressionNextMainS(cur, step, maxS);
        if (next <= cur) {
            NetUtil::sendError(server, 409, "Maximum erreicht");
            return;
        }
        if (!writeProgressionMainS(id.c_str(), next)) {
            NetUtil::sendError(server, 500, "Progression nicht speicherbar");
            return;
        }
        progOffer_.pending = false;
        progOffer_.currentMainS = next;
        progOffer_.nextMainS = ergo::progressionNextMainS(next, step, maxS);
        JsonDocument doc;
        doc["ok"] = true;
        doc["id"] = id;
        doc["mainS"] = next;
        NetUtil::sendJson(server, 200, doc);
    });

    server.on("/api/progression/decline", HTTP_POST, [this]() {
        progOffer_.pending = false;
        JsonDocument doc;
        doc["ok"] = true;
        NetUtil::sendJson(server, 200, doc);
    });
    server.on("/api/session/list", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["ok"] = true;
        doc["count"] = sessionStore_.count();
        JsonArray arr = doc["sessions"].to<JsonArray>();
        for (uint8_t i = 0; i < sessionStore_.count(); i++) {
            ergo::SessionSummary s;
            if (!sessionStore_.at(i, s)) continue;
            JsonObject o = arr.add<JsonObject>();
            o["mode"] = s.mode;
            o["workoutName"] = s.workoutName;
            o["profileId"] = s.profileId;
            o["endReason"] = s.endReason;
            o["durationS"] = s.durationS;
            o["pausedS"] = s.pausedS;
            o["steps"] = s.steps;
            o["interventions"] = s.interventions;
            o["autoPauses"] = s.autoPauses;
            o["avgPowerW"] = s.avgPowerW;
            o["avgDesiredW"] = s.avgDesiredW;
            o["workKj"] = s.workKj;
            o["hrAvg"] = s.hrAvg;
            o["hrMax"] = s.hrMax;
            o["leadHr"] = s.leadHr;
            o["zoneCount"] = s.zoneCount;
            JsonArray zta = o["zoneTimeS"].to<JsonArray>();
            for (uint8_t j = 0; j < s.zoneCount && j < ergo::kPowerZones; j++)
                zta.add(s.zoneTimeS[j]);
        }
        NetUtil::sendJson(server, 200, doc);
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
    const char* fo = "manual";
    if (p.ftpOrigin == ergo::FtpOrigin::Estimate) fo = "estimate";
    else if (p.ftpOrigin == ergo::FtpOrigin::Test) fo = "test";
    obj["ftpOrigin"] = fo;
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
    obj["birthYear"] = p.birthYear;
    const char* goal = "none";
    if (p.goal == ergo::TrainingGoal::Fitness) goal = "fitness";
    else if (p.goal == ergo::TrainingGoal::FatLoss) goal = "fatloss";
    else if (p.goal == ergo::TrainingGoal::Reha) goal = "reha";
    else if (p.goal == ergo::TrainingGoal::Performance) goal = "performance";
    obj["goal"] = goal;
    if (p.ftpW > 0 && p.weightKg > 0)
        obj["wPerKg"] = (float)p.ftpW / (float)p.weightKg;
    if (p.birthYear > 0) {
        obj["hrMaxEstimate"] = ergo::ProfileStore::estimateHrMax(p.birthYear, 2026);
    }
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
    if (!v["ftpOrigin"].isNull()) {
        const char* z = v["ftpOrigin"].as<const char*>();
        if (z && strcmp(z, "estimate") == 0) out.ftpOrigin = ergo::FtpOrigin::Estimate;
        else if (z && strcmp(z, "test") == 0) out.ftpOrigin = ergo::FtpOrigin::Test;
        else out.ftpOrigin = ergo::FtpOrigin::Manual;
    }
    if (!v["birthYear"].isNull()) out.birthYear = (uint16_t)v["birthYear"].as<int>();
    if (!v["goal"].isNull()) {
        const char* z = v["goal"].as<const char*>();
        if (z && strcmp(z, "fitness") == 0) out.goal = ergo::TrainingGoal::Fitness;
        else if (z && strcmp(z, "fatloss") == 0) out.goal = ergo::TrainingGoal::FatLoss;
        else if (z && strcmp(z, "reha") == 0) out.goal = ergo::TrainingGoal::Reha;
        else if (z && strcmp(z, "performance") == 0) out.goal = ergo::TrainingGoal::Performance;
        else out.goal = ergo::TrainingGoal::None;
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
        saveProfiles();
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
        saveProfiles();
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
        saveProfiles();
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

void App::loopErg(unsigned long now) {
    if (!control.allowsErg()) return;
    if (session_.holdLoad()) return;
    // HR verloren + Freeze: Stufe halten, keine Watt→Level-Anpassung.
    if (control.allowsHrHold() && hrCtl.lost() &&
        hrCtl.lossPolicy() == ergo::HrLossPolicy::Freeze) {
        return;
    }
    if (control.allowsReha() && rehaCtl.lost() &&
        rehaCtl.lossPolicy() == ergo::HrLossPolicy::Freeze) {
        return;
    }
    if (control.allowsWorkout() && rehaCtl.lost() &&
        rehaCtl.lossPolicy() == ergo::HrLossPolicy::Freeze) {
        return;
    }
    if (control.allowsWorkout() && workout.state() == ergo::WorkoutState::Paused) {
        return;
    }
    if (!ble.ready(ergo::Role::Bike) || !ftms.attached()) return;
    if (sweep.running()) return;

    const bool fresh = ftms.hasLive() && !ftms.stale(now);
    const float rpm = fresh ? ftms.live().cadenceRpm() : 0.0f;
    const float watt = fresh ? (float)ftms.live().powerW : 0.0f;

    if (control.powerTargetW() > 0.0f &&
        fabsf(control.powerTargetW() - powerCtl.targetW()) > 0.5f) {
        powerCtl.setTargetW(control.powerTargetW());
    }

    const ergo::PowerController::Tick t = powerCtl.tick(now, rpm, watt, fresh, powerMap);
    if (!t.wantWrite || t.levelTenths < 0) return;

    const ergo::FtmsClient::Result r = ftms.setLevelTenths(t.levelTenths, now);
    if (r == ergo::FtmsClient::Result::Ok) {
        const char* tag = "ERG";
        if (control.allowsHrHold()) tag = "HR";
        else if (control.allowsWorkout()) tag = "WO";
        else if (control.allowsReha()) tag = "REHA";
        Serial.printf("[%s] Ziel %.0f W → Stufe %d%s (Ist~%.0f W, rpm=%.0f)\n", tag, t.targetW,
                      (int)t.levelTenths, t.ceiling ? " CEILING" : "", t.smoothedW, rpm);
    } else if (r != ergo::FtmsClient::Result::Deferred) {
        Serial.printf("[ERG] Write abgelehnt: %s (%s)\n", ergo::FtmsClient::resultName(r),
                      ftms.lastDenyReason());
    }
}

void App::loopHr(unsigned long now) {
    if (!control.allowsHrHold()) return;
    if (sweep.running()) return;

    const uint8_t hr = effectiveHr();
    const bool hrFresh = (resolveHrSource() != ergo::HrSource::None) && hr > 0;
    uint8_t hardMax = 0;
    if (const ergo::Profile* ap = profiles.active()) {
        hardMax = ap->maxHr;
        hrCtl.setLossPolicy(ap->onHrLoss);
    }

    const ergo::HrController::Tick ht = hrCtl.tick(now, hr, hrFresh, hardMax);

    if (ht.lost) {
        static unsigned long lastLossLog = 0;
        if (now - lastLossLog > 2000) {
            lastLossLog = now;
            Serial.printf("[HR] Pulsverlust — Politik %d\n", (int)ht.lossPolicy);
        }
        if (ht.lossPolicy == ergo::HrLossPolicy::Stop) {
            ftms.stop(now);
            recordSessionEnd("hr_lost");
            control.setMode(ergo::ControlMode::Off);
            powerCtl.reset();
            hrCtl.reset();
            return;
        }
        if (ht.lossPolicy == ergo::HrLossPolicy::Freeze) {
            // Stufe einfrieren: kein neues Watt-Ziel, loopErg schreibt nur bei
            // Periodenwechsel derselben Stufe — lastLevel verhindert Writes.
            return;
        }
        // Reduce: Wattziel absenken
        float p = powerCtl.targetW();
        if (p < 1.0f) p = ht.powerTargetW;
        p *= 0.92f;
        if (p < 25.0f) p = 25.0f;
        control.setPowerTargetW(p);
        powerCtl.setTargetW(p);
        return;
    }

    control.setPowerTargetW(ht.powerTargetW);
    powerCtl.setTargetW(ht.powerTargetW);
}

void App::loopReha(unsigned long now) {
    if (!control.allowsReha()) return;
    if (sweep.running()) return;
    applyRehaCap(now, false);
}

void App::loopWorkout(unsigned long now) {
    if (!control.allowsWorkout()) return;
    if (sweep.running()) return;

    const ergo::WorkoutEngine::Tick wt = workout.tick(now);
    woSnap_ = wt;
    if (wt.finished || wt.state == ergo::WorkoutState::Done) {
        Serial.println("[WO] Programm fertig — STOP");
        recordSessionEnd("done");
        if (ble.ready(ergo::Role::Bike)) ftms.stop(now);
        control.setMode(ergo::ControlMode::Off);
        powerCtl.reset();
        rehaCtl.reset();
        workout.stop();
        return;
    }
    if (wt.state == ergo::WorkoutState::Paused) {
        // Keine neuen Wattziele; Stufe halten.
        return;
    }
    if (wt.justAdvanced || fabsf(rehaCtl.desiredW() - wt.desiredW) > 0.5f) {
        Serial.printf("[WO] Schritt %u/%u %s — %.0f W, Puls ≤ %u\n",
                      (unsigned)(wt.stepIndex + 1), (unsigned)wt.stepCount, wt.label,
                      wt.desiredW, (unsigned)wt.hrMax);
        rehaCtl.setDesiredW(wt.desiredW);
        rehaCtl.setHrLimits(wt.hrSoft ? wt.hrSoft : (wt.hrMax > 5 ? wt.hrMax - 5 : wt.hrMax),
                            wt.hrMax ? wt.hrMax : 220);
        rehaCtl.setDurationS(0);
        // Integral/Zustand behalten, nur Soll nachziehen — Interventionszaehler
        // bleibt ueber Schritte.
        if (wt.justAdvanced && wt.stepIndex == 0) rehaCtl.reset();
        else {
            // desired schon gesetzt; effective nicht ueber desired
            if (rehaCtl.effectiveW() > wt.desiredW) {
                /* setDesiredW klemmt schon */
            }
        }
        control.setPowerTargetW(wt.desiredW);
        powerCtl.setTargetW(wt.desiredW);
    }
    applyRehaCap(now, true);
}

void App::applyRehaCap(unsigned long now, bool fromWorkout) {
    const char* tag = fromWorkout ? "WO" : "REHA";
    const uint8_t hr = effectiveHr();
    const bool hrFresh = (resolveHrSource() != ergo::HrSource::None) && hr > 0;
    if (const ergo::Profile* ap = profiles.active()) {
        rehaCtl.setLossPolicy(ap->onHrLoss);
    }

    const ergo::RehaController::Tick rt = rehaCtl.tick(now, hr, hrFresh);

    if (!fromWorkout && rt.finished) {
        Serial.printf("[%s] Dauer erreicht — STOP\n", tag);
        recordSessionEnd("done");
        if (ble.ready(ergo::Role::Bike)) ftms.stop(now);
        control.setMode(ergo::ControlMode::Off);
        powerCtl.reset();
        rehaCtl.reset();
        workout.stop();
        return;
    }

    if (rt.lost) {
        static unsigned long lastLossLog = 0;
        if (now - lastLossLog > 2000) {
            lastLossLog = now;
            Serial.printf("[%s] Pulsverlust — Politik %d\n", tag, (int)rt.lossPolicy);
        }
        if (rt.lossPolicy == ergo::HrLossPolicy::Stop) {
            recordSessionEnd("hr_lost");
            if (ble.ready(ergo::Role::Bike)) ftms.stop(now);
            control.setMode(ergo::ControlMode::Off);
            powerCtl.reset();
            rehaCtl.reset();
            workout.stop();
            return;
        }
        if (rt.lossPolicy == ergo::HrLossPolicy::Freeze) return;
        float p = powerCtl.targetW();
        if (p < 1.0f) p = rt.effectiveW;
        p *= 0.92f;
        if (p < 25.0f) p = 25.0f;
        control.setPowerTargetW(p);
        powerCtl.setTargetW(p);
        return;
    }

    if (rt.capActive) {
        static unsigned long lastCapLog = 0;
        if (now - lastCapLog > 3000) {
            lastCapLog = now;
            Serial.printf("[%s] Deckel: %.0f → %.0f W (HR %u, Eingriffe %u)\n", tag, rt.desiredW,
                          rt.effectiveW, (unsigned)hr, (unsigned)rt.interventions);
        }
        if (rt.interventions > interventionsSeen_) {
            while (interventionsSeen_ < rt.interventions) {
                session_.noteIntervention();
                interventionsSeen_++;
            }
        }
    }

    control.setPowerTargetW(rt.effectiveW);
    powerCtl.setTargetW(rt.effectiveW);
    if (rt.desiredW > 0.0f) session_.noteDesiredW(rt.desiredW);
}

bool App::beginFs() {
    fsReady_ = LittleFS.begin(false);
    if (!fsReady_) fsReady_ = LittleFS.begin(true);
    if (fsReady_) {
        if (!LittleFS.exists("/workouts")) LittleFS.mkdir("/workouts");
        if (!LittleFS.exists("/sessions")) LittleFS.mkdir("/sessions");
        if (!LittleFS.exists("/progression")) LittleFS.mkdir("/progression");
        Serial.println("[FS] LittleFS bereit (/workouts, /sessions, /progression)");
    } else {
        Serial.println("[FS] LittleFS fehlt — nur Builtins");
    }
    return fsReady_;
}

bool App::loadWorkoutDoc(const ergo::WorkoutDoc& doc, float scale) {
    if (doc.stepCount == 0) return false;
    if (scale < 0.05f) scale = 0.05f;
    if (scale > 2.0f) scale = 2.0f;
    ergo::WorkoutDoc prepared = doc;
    prepareWorkoutDoc(prepared);
    ergo::WorkoutStep steps[ergo::WorkoutEngine::kMaxSteps];
    for (uint8_t i = 0; i < prepared.stepCount; i++) {
        steps[i] = prepared.steps[i];
        steps[i].durationS = (uint32_t)(steps[i].durationS * scale + 0.5f);
        if (steps[i].durationS < 1) steps[i].durationS = 1;
    }
    if (!workout.loadSteps(steps, prepared.stepCount, prepared.name[0] ? prepared.name : prepared.id))
        return false;
    strncpy(activeWorkoutId_, prepared.id, sizeof(activeWorkoutId_) - 1);
    activeWorkoutId_[sizeof(activeWorkoutId_) - 1] = 0;
    activeProg_ = prepared.progression;
    rehaCtl.setDurationS(0);
    if (!workout.start(millis())) return false;
    woSnap_ = workout.tick(millis());
    rehaCtl.setDesiredW(woSnap_.desiredW);
    rehaCtl.setHrLimits(woSnap_.hrSoft, woSnap_.hrMax ? woSnap_.hrMax : 120);
    rehaCtl.reset();
    control.setPowerTargetW(woSnap_.desiredW);
    powerCtl.setTargetW(woSnap_.desiredW);
    return true;
}

void App::prepareWorkoutDoc(ergo::WorkoutDoc& doc) {
    if (!doc.progression.enabled) return;
    doc.progression.stepIndex = ergo::progressionResolveStepIndex(doc);
    if (doc.progression.baseDurationS == 0)
        doc.progression.baseDurationS = doc.steps[doc.progression.stepIndex].durationS;
    const uint32_t stored = readProgressionMainS(doc.id);
    const uint32_t mainS = stored > 0 ? stored : doc.progression.baseDurationS;
    ergo::progressionApplyMain(doc, mainS);
}

uint32_t App::readProgressionMainS(const char* id) const {
    if (!fsReady_ || !id || !id[0]) return 0;
    String path = String("/progression/") + id + ".json";
    File f = LittleFS.open(path, "r");
    if (!f) return 0;
    String body = f.readString();
    f.close();
    const char* p = strstr(body.c_str(), "\"mainS\"");
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    return (uint32_t)strtoul(p + 1, nullptr, 10);
}

bool App::writeProgressionMainS(const char* id, uint32_t mainS) {
    if (!fsReady_ || !id || !id[0]) return false;
    if (!LittleFS.exists("/progression")) LittleFS.mkdir("/progression");
    String path = String("/progression/") + id + ".json";
    File f = LittleFS.open(path, "w");
    if (!f) return false;
    char buf[48];
    snprintf(buf, sizeof(buf), "{\"mainS\":%u}", (unsigned)mainS);
    const bool ok = f.print(buf) > 0;
    f.close();
    return ok;
}

void App::appendProgressionOfferJson(JsonObject obj) const {
    obj["pending"] = progOffer_.pending;
    obj["clean"] = progOffer_.clean;
    obj["reason"] = progOffer_.reason;
    obj["workoutId"] = progOffer_.workoutId;
    obj["workoutName"] = progOffer_.workoutName;
    obj["currentMainS"] = progOffer_.currentMainS;
    obj["nextMainS"] = progOffer_.nextMainS;
    obj["stepS"] = progOffer_.stepS;
    obj["maxS"] = progOffer_.maxS;
}

void App::maybeOfferProgression(const ergo::SessionSummary& s) {
    progOffer_ = ergo::ProgressionOffer{};
    if (!activeProg_.enabled || !activeWorkoutId_[0]) return;
    strncpy(progOffer_.workoutId, activeWorkoutId_, sizeof(progOffer_.workoutId) - 1);
    strncpy(progOffer_.workoutName, s.workoutName, sizeof(progOffer_.workoutName) - 1);
    progOffer_.stepS = activeProg_.stepS ? activeProg_.stepS : 60;
    progOffer_.maxS = activeProg_.maxS ? activeProg_.maxS : 1800;
    uint32_t cur = readProgressionMainS(activeWorkoutId_);
    if (cur == 0) cur = activeProg_.baseDurationS;
    if (cur == 0) cur = 600;
    progOffer_.currentMainS = cur;
    progOffer_.nextMainS = ergo::progressionNextMainS(cur, progOffer_.stepS, progOffer_.maxS);
    progOffer_.clean = ergo::progressionIsClean(s, progOffer_.reason, sizeof(progOffer_.reason));
    if (progOffer_.clean && progOffer_.nextMainS > progOffer_.currentMainS)
        progOffer_.pending = true;
    else if (!progOffer_.clean)
        progOffer_.pending = true;
    else {
        progOffer_.pending = false;
        strncpy(progOffer_.reason, "Maximum erreicht", sizeof(progOffer_.reason) - 1);
    }
}

void App::recordSessionEnd(const char* reason) {
    if (!session_.active()) return;
    ergo::SessionSummary s = session_.end(millis(), reason);
    if (workout.stepCount() > 0) s.steps = workout.stepCount();
    if (rehaCtl.interventions() > s.interventions) s.interventions = rehaCtl.interventions();
    lastSession_ = s;
    persistSession_(s);
    maybeOfferProgression(s);
    Serial.printf("[SESS] %s %s %u s (Pause %u), Ø %.0f W, %.1f kJ, Deckel %u×\n", s.mode,
                  s.endReason, (unsigned)s.durationS, (unsigned)s.pausedS, s.avgPowerW, s.workKj,
                  (unsigned)s.interventions);
}

void App::beginSession(const char* workoutName) {
    const char* mode = ergo::controlModeName(control.mode());
    const char* pid = profiles.activeId() ? profiles.activeId() : "";
    session_.start(millis(), mode, workoutName ? workoutName : "", pid);
    bool leadHr = false;
    uint16_t ftp = 0;
    uint8_t hrMax = 0;
    if (const ergo::Profile* ap = profiles.active()) {
        leadHr = (ap->leadingZone == ergo::ZoneLead::Hr);
        ftp = ap->ftpW;
        hrMax = ap->hrMax ? ap->hrMax : ap->maxHr;
    }
    session_.setZoneBasis(leadHr, ftp, hrMax);
    interventionsSeen_ = rehaCtl.interventions();
    Serial.printf("[SESS] start %s profile=%s zones=%s\n", mode, pid, leadHr ? "HR" : "PWR");
}

void App::loopSession(unsigned long now) {
    if (!session_.active()) return;

    if (const ergo::Profile* ap = profiles.active()) {
        session_.setZoneBasis(ap->leadingZone == ergo::ZoneLead::Hr, ap->ftpW,
                              ap->hrMax ? ap->hrMax : ap->maxHr);
    }

    const bool linked = ble.ready(ergo::Role::Bike);
    const bool fresh = linked && ftms.hasLive() && !ftms.stale(now);
    const float rpm = fresh ? ftms.live().cadenceRpm() : 0.0f;
    const float watt = fresh ? (float)ftms.live().powerW : 0.0f;
    const uint8_t hr = effectiveHr();

    if (!linked || !fresh) {
        session_.tick(now, 0.0f, 0.0f, hr, true);
    } else {
        session_.tick(now, rpm, watt, hr, true);
    }

    const bool freezing =
        (control.allowsHrHold() && hrCtl.lost() &&
         hrCtl.lossPolicy() == ergo::HrLossPolicy::Freeze) ||
        ((control.allowsReha() || control.allowsWorkout()) && rehaCtl.lost() &&
         rehaCtl.lossPolicy() == ergo::HrLossPolicy::Freeze);
    session_.noteHrLost(now, freezing);
    if (freezing) applyFreezeToLevel(now);
}

void App::applyFreezeToLevel(unsigned long now) {
    if (!session_.freezeTimedOut(now)) return;
    const int16_t tenths = limiter.currentLevelTenths();
    Serial.printf("[SESS] Freeze-Timeout → MANUAL_LEVEL Stufe %.1f\n", tenths / 10.0f);
    control.setMode(ergo::ControlMode::ManualLevel);
    if (tenths >= 0) {
        control.setLevelTargetTenths(tenths);
        if (ble.ready(ergo::Role::Bike)) ftms.setLevelTenths(tenths, now);
    }
    powerCtl.reset();
    hrCtl.reset();
    rehaCtl.reset();
    workout.stop();
    session_.noteHrLost(now, false);
    session_.retagMode(ergo::controlModeName(ergo::ControlMode::ManualLevel));
}

void App::persistSession_(const ergo::SessionSummary& s) {
    if (!s.valid) return;
    sessionStore_.append(s);

    if (fsReady_) {
        if (!LittleFS.exists("/sessions")) LittleFS.mkdir("/sessions");
        char line[480];
        const size_t n = sessionStore_.writeJsonLine(s, line, sizeof(line));
        if (n > 0) {
            File f = LittleFS.open("/sessions/log.jsonl", "a");
            if (f) {
                f.println(line);
                f.close();
            }
            File last = LittleFS.open("/sessions/last.json", "w");
            if (last) {
                last.print(line);
                last.close();
            }
        }
    }

    if (config.enableHub) {
        JsonDocument doc;
        doc["mac"] = NetUtil::macNoColon();
        doc["fwType"] = FW_TYPE;
        doc["name"] = config.deviceName;
        doc["mode"] = s.mode;
        doc["workoutName"] = s.workoutName;
        doc["profileId"] = s.profileId;
        doc["endReason"] = s.endReason;
        doc["durationS"] = s.durationS;
        doc["pausedS"] = s.pausedS;
        doc["steps"] = s.steps;
        doc["interventions"] = s.interventions;
        doc["autoPauses"] = s.autoPauses;
        doc["avgPowerW"] = s.avgPowerW;
        doc["avgDesiredW"] = s.avgDesiredW;
        doc["workKj"] = s.workKj;
        doc["hrAvg"] = s.hrAvg;
        doc["hrMax"] = s.hrMax;
        doc["leadHr"] = s.leadHr;
        doc["zoneCount"] = s.zoneCount;
        JsonArray zta = doc["zoneTimeS"].to<JsonArray>();
        for (uint8_t i = 0; i < s.zoneCount && i < ergo::kPowerZones; i++)
            zta.add(s.zoneTimeS[i]);
        String payload;
        serializeJson(doc, payload);
        const int code = hub.postJson("/api/session-export", payload, 15000);
        Serial.printf("[SESS] Hub-Export HTTP %d\n", code);
    }
}

void App::loadSessionArchive_() {
    sessionStore_.clear();
    if (!fsReady_) return;
    File f = LittleFS.open("/sessions/log.jsonl", "r");
    if (!f) {
        File last = LittleFS.open("/sessions/last.json", "r");
        if (last) {
            String body = last.readString();
            last.close();
            ergo::SessionSummary s;
            if (sessionStore_.parseJsonLine(body.c_str(), s)) {
                sessionStore_.append(s);
                lastSession_ = s;
            }
        }
        return;
    }
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (!line.length()) continue;
        ergo::SessionSummary s;
        if (sessionStore_.parseJsonLine(line.c_str(), s)) sessionStore_.append(s);
    }
    f.close();
    sessionStore_.at(0, lastSession_);
    Serial.printf("[SESS] Archiv: %u Eintraege\n", (unsigned)sessionStore_.count());
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
        if (!profiles.active()) {
            NetUtil::sendError(server, 409, "Profil wählen (Reiter Profile)");
            return;
        }
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
        const bool light = server.arg("light") == "1" || server.arg("plan") == "light";
        const char* kind = light ? "light" : (coarse ? "coarse" : "full");
        float rpm = server.hasArg("rpm") ? server.arg("rpm").toFloat()
                                         : (light || coarse ? 80.0f : 60.0f);
        if (rpm < 40.0f || rpm > 119.0f) {
            NetUtil::sendError(server, 400, "rpm ausserhalb 40..119");
            return;
        }
        ergo::SweepPlan plan = ergo::SweepRunner::planFor((uint8_t)c.levelCount(),
                                                          c.levelMinTenths(),
                                                          c.levelStepTenths(), rpm, kind);
        // Optional: nur bis Stufe N (1-basiert), z.B. maxLevel=8.
        if (server.hasArg("maxLevel")) {
            const int ml = server.arg("maxLevel").toInt();
            if (ml > 0) {
                const int16_t maxTenths =
                    (int16_t)(c.levelMinTenths() + (int32_t)(ml - 1) * c.levelStepTenths());
                ergo::SweepRunner::clipPlanToMax(plan, maxTenths);
            }
        }
        const int16_t maxLvl = limiter.effectiveMaxLevelTenths();
        const uint8_t beforeClip = plan.count;
        ergo::SweepRunner::clipPlanToMax(plan, maxLvl);
        if (plan.count == 0) {
            NetUtil::sendError(server, 409, "Profilgrenze: keine Sweep-Stufe übrig");
            return;
        }
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
        Serial.printf("[SWEEP] Start %s: %u Stufen, Ziel %.0f rpm\n", kind, (unsigned)plan.count,
                      plan.targetRpm);

        JsonDocument doc;
        doc["ok"] = true;
        doc["plan"] = kind;
        doc["levels"] = plan.count;
        doc["clipped"] = (plan.count < beforeClip);
        doc["maxLevelTenths"] = maxLvl;
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
    loopSession(now);
    loopHr(now);
    loopReha(now);
    loopWorkout(now);
    loopErg(now);

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
