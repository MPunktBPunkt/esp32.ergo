#include "FtmsClient.h"

#include "core/NetUtil.h"

namespace ergo {

static FtmsClient* g_ftms = nullptr;

static void ftmsNotifyCb(NimBLERemoteCharacteristic* chr, uint8_t* data, size_t len, bool) {
    if (g_ftms) g_ftms->onNotify(chr, data, len);
}

const char* FtmsClient::resultName(Result r) {
    switch (r) {
        case Result::Ok: return "ok";
        case Result::NoLink: return "no-link";
        case Result::Denied: return "denied";
        case Result::Deferred: return "deferred";
        case Result::EncodeFailed: return "encode-failed";
        case Result::WriteFailed: return "write-failed";
        default: return "?";
    }
}

void FtmsClient::begin(Limiter* limiter) {
    limiter_ = limiter;
    g_ftms = this;
}

// ─────────────────────────────────────────────────────────────── Anbinden

NimBLERemoteCharacteristic* FtmsClient::find(NimBLERemoteService* svc, uint16_t uuid) {
    if (!svc) return nullptr;
    return svc->getCharacteristic(NimBLEUUID(uuid));
}

bool FtmsClient::readInto(NimBLERemoteCharacteristic* c, uint8_t* buf, size_t cap,
                          size_t& outLen) {
    outLen = 0;
    if (!c || !c->canRead()) return false;
    std::string v = c->readValue();
    outLen = v.size() > cap ? cap : v.size();
    if (outLen) memcpy(buf, v.data(), outLen);
    return outLen > 0;
}

bool FtmsClient::subscribeTo(NimBLERemoteCharacteristic* c) {
    if (!c) return false;
    // Nach Spec ist der Control Point Indicate. Der Varon liefert ihn als
    // Notify. Deshalb nicht raten, sondern die Properties fragen — genau das
    // macht den Unterschied, wenn spaeter ein anderes Geraet dranhaengt.
    const bool useNotify = c->canNotify();
    if (!useNotify && !c->canIndicate()) return false;
    // Manche Peripherals beantworten den CCCD-Write nicht, obwohl das Abo
    // greift. Dann ohne Write-Response nachziehen.
    if (c->subscribe(useNotify, ftmsNotifyCb, true)) return true;
    return c->subscribe(useNotify, ftmsNotifyCb, false);
}

bool FtmsClient::attach(NimBLEClient* client) {
    detach();
    if (!client || !client->isConnected()) return false;

    NimBLERemoteService* svc = client->getService(NimBLEUUID(ftms::kSvcFitnessMachine));
    if (!svc) {
        Serial.println("[FTMS] kein 0x1826 auf diesem Geraet");
        return false;
    }
    client_ = client;

    uint8_t buf[8];
    size_t len = 0;

    ftms::FeatureSet feat;
    if (readInto(find(svc, ftms::kChrFeature), buf, sizeof(buf), len)) {
        ftms::decodeFeature(buf, len, feat);
        String hx = NetUtil::toHex(buf, len);
        strncpy(featureHex_, hx.c_str(), sizeof(featureHex_) - 1);
    }

    ftms::ResistanceRange res;
    bool haveRes = false;
    if (readInto(find(svc, ftms::kChrResistanceRange), buf, sizeof(buf), len)) {
        haveRes = ftms::decodeResistanceRange(buf, len, res);
        String hx = NetUtil::toHex(buf, len);
        strncpy(resistanceHex_, hx.c_str(), sizeof(resistanceHex_) - 1);
    }

    ftms::PowerRange pow;
    bool havePow = false;
    if (readInto(find(svc, ftms::kChrPowerRange), buf, sizeof(buf), len)) {
        havePow = ftms::decodePowerRange(buf, len, pow);
        String hx = NetUtil::toHex(buf, len);
        strncpy(powerHex_, hx.c_str(), sizeof(powerHex_) - 1);
    }

    caps_ = ftms::deriveCapabilities(feat, haveRes ? &res : nullptr, havePow ? &pow : nullptr);

    ibd_ = find(svc, ftms::kChrIndoorBikeData);
    cp_ = find(svc, ftms::kChrControlPoint);
    caps_.controlPointNotify = cp_ && cp_->canNotify();

    if (ibd_ && !subscribeTo(ibd_)) Serial.println("[FTMS] Abo auf 0x2AD2 fehlgeschlagen");
    if (cp_ && !subscribeTo(cp_)) Serial.println("[FTMS] Abo auf 0x2AD9 fehlgeschlagen");

    if (limiter_) {
        limiter_->reset();
        limiter_->setCapabilities(&caps_);
    }

    Serial.printf("[FTMS] Feature=%s Stellweg=%s Watt=%s\n",
                  featureHex_[0] ? featureHex_ : "-",
                  resistanceHex_[0] ? resistanceHex_ : "fehlt",
                  powerHex_[0] ? powerHex_ : "fehlt");
    Serial.printf("[FTMS] Strategie=%s Stufen=%u Wattziel=%s\n",
                  ftms::powerStrategyName(caps_.powerStrategy()), (unsigned)caps_.levelCount(),
                  caps_.powerTargetTrusted ? "vertrauenswuerdig" : "nein");
    return true;
}

void FtmsClient::detach() {
    // Die Characteristic-Zeiger gehoeren dem Client; nach dessen Freigabe
    // sind sie ungueltig. Nur verwerfen, nichts abmelden.
    client_ = nullptr;
    ibd_ = nullptr;
    cp_ = nullptr;
    caps_ = ftms::Capabilities{};
    live_ = ftms::IndoorBikeData{};
    lastResp_ = ftms::ControlResponse{};
    liveCount_ = 0;
    respCount_ = 0;
    lastData_ = 0;
    controlGranted_ = false;
    featureHex_[0] = 0;
    resistanceHex_[0] = 0;
    powerHex_[0] = 0;
    if (limiter_) {
        limiter_->setCapabilities(nullptr);
        limiter_->reset();
    }
}

bool FtmsClient::stale(uint32_t nowMs) const {
    if (!liveCount_) return true;
    return (nowMs - lastData_) > ERGO_DATA_STALE_MS;
}

// ───────────────────────────────────────────────────────────────── Notify

void FtmsClient::onNotify(NimBLERemoteCharacteristic* chr, const uint8_t* data, size_t len) {
    if (!chr || !data || !len) return;
    const NimBLEUUID u = chr->getUUID();

    if (u == NimBLEUUID(ftms::kChrIndoorBikeData)) {
        ftms::IndoorBikeData d;
        if (ftms::decodeIndoorBikeData(data, len, d) == ftms::IbdStatus::Ok) {
            live_ = d;
            liveCount_++;
            lastData_ = millis();
            // Was wirklich im Datenstrom steht, ist belastbarer als das, was
            // 0x2ACC behauptet — beim Varon fehlt dort das Resistance-Feld.
            ftms::noteIndoorBikeData(caps_, d);
        }
        return;
    }

    if (u == NimBLEUUID(ftms::kChrControlPoint)) {
        ftms::ControlResponse r;
        if (ftms::decodeControlResponse(data, len, r)) {
            lastResp_ = r;
            respCount_++;
            if (r.request == ftms::Opcode::RequestControl) controlGranted_ = r.ok();
            if (r.request == ftms::Opcode::Reset && r.ok()) controlGranted_ = false;
        }
    }
}

// ─────────────────────────────────────────────────────────────── Schreiben

FtmsClient::Result FtmsClient::send(const uint8_t* cmd, size_t len, uint32_t nowMs) {
    if (!cp_ || !client_ || !client_->isConnected()) return Result::NoLink;
    if (!limiter_) return Result::Denied;  // ohne Limiter geht hier nichts raus

    const Limiter::Verdict v = limiter_->check(cmd, len, nowMs);
    if (v.decision == Decision::Defer) {
        lastDeny_ = v.reason;
        return Result::Deferred;
    }
    if (!v.sendable()) {
        lastDeny_ = v.reason;
        Serial.printf("[FTMS] abgelehnt: %s\n", v.reason);
        return Result::Denied;
    }

    // Write mit Response: ohne Quittung wissen wir nicht, ob das Kommando
    // ueberhaupt im Geraet angekommen ist. Bei Lastkommandos ist das der
    // Unterschied zwischen "gestellt" und "gehofft".
    if (!cp_->writeValue(v.data, v.len, true)) {
        Serial.printf("[FTMS] Write fehlgeschlagen (%s)\n", v.reason);
        return Result::WriteFailed;
    }
    limiter_->noteWritten(v.data, v.len, nowMs);
    return Result::Ok;
}

FtmsClient::Result FtmsClient::requestControl(uint32_t nowMs) {
    uint8_t c[ftms::kMaxControlLen];
    const size_t n = ftms::encodeRequestControl(c, sizeof(c));
    if (!n) return Result::EncodeFailed;
    return send(c, n, nowMs);
}

FtmsClient::Result FtmsClient::reset(uint32_t nowMs) {
    uint8_t c[ftms::kMaxControlLen];
    const size_t n = ftms::encodeReset(c, sizeof(c));
    if (!n) return Result::EncodeFailed;
    return send(c, n, nowMs);
}

FtmsClient::Result FtmsClient::start(uint32_t nowMs) {
    uint8_t c[ftms::kMaxControlLen];
    const size_t n = ftms::encodeStartResume(c, sizeof(c));
    if (!n) return Result::EncodeFailed;
    return send(c, n, nowMs);
}

FtmsClient::Result FtmsClient::stop(uint32_t nowMs) {
    uint8_t c[ftms::kMaxControlLen];
    const size_t n = ftms::encodeStop(c, sizeof(c));
    if (!n) return Result::EncodeFailed;
    return send(c, n, nowMs);
}

FtmsClient::Result FtmsClient::pause(uint32_t nowMs) {
    uint8_t c[ftms::kMaxControlLen];
    const size_t n = ftms::encodePause(c, sizeof(c));
    if (!n) return Result::EncodeFailed;
    return send(c, n, nowMs);
}

FtmsClient::Result FtmsClient::setLevelTenths(int16_t tenths, uint32_t nowMs) {
    uint8_t c[ftms::kMaxControlLen];
    // Breite aus den Faehigkeiten, nicht aus einer Konstante: der Varon nimmt
    // nur die sint16-Form an, ein Geraet mit 0..100-Skala kann die uint8-Form
    // brauchen. Die Entscheidung liegt in Capabilities, nicht hier.
    const size_t n =
        ftms::encodeSetTargetResistance(c, sizeof(c), tenths, caps_.needsWideResistance());
    if (!n) return Result::EncodeFailed;
    return send(c, n, nowMs);
}

FtmsClient::Result FtmsClient::setPowerW(int16_t watt, uint32_t nowMs) {
    uint8_t c[ftms::kMaxControlLen];
    const size_t n = ftms::encodeSetTargetPower(c, sizeof(c), watt);
    if (!n) return Result::EncodeFailed;
    // Der Limiter lehnt das ab, solange `powerTargetTrusted` nicht gilt. Hier
    // wird bewusst nicht vorgefiltert — die Ablehnung soll mit Begruendung in
    // der UI landen, statt still zu verschwinden.
    return send(c, n, nowMs);
}

// ───────────────────────────────────────────────────────────────── JSON

void FtmsClient::appendStatusJson(JsonObject obj) const {
    obj["attached"] = attached();
    obj["controlPoint"] = hasControlPoint();
    obj["controlGranted"] = controlGranted_;
    obj["notifies"] = liveCount_;
    obj["responses"] = respCount_;
    obj["stale"] = stale(millis());

    JsonObject c = obj["caps"].to<JsonObject>();
    c["valid"] = caps_.valid;
    c["strategy"] = ftms::powerStrategyName(caps_.powerStrategy());
    c["targetResistance"] = caps_.canTargetResistance;
    c["targetPower"] = caps_.canTargetPower;
    c["powerTrusted"] = caps_.powerTargetTrusted;
    c["simulate"] = caps_.canSimulate;
    c["levels"] = caps_.levelCount();
    c["levelMinTenths"] = caps_.levelMinTenths();
    c["levelMaxTenths"] = caps_.levelMaxTenths();
    c["levelStepTenths"] = caps_.levelStepTenths();
    c["wide"] = caps_.needsWideResistance();
    c["reportsPower"] = caps_.ibdReportsPower;
    c["reportsCadence"] = caps_.ibdReportsCadence;
    c["reportsResistance"] = caps_.ibdReportsResistance;
    c["reportsHeartRate"] = caps_.ibdReportsHeartRate;
    if (featureHex_[0]) c["featureHex"] = featureHex_;
    if (resistanceHex_[0]) c["resistanceHex"] = resistanceHex_;
    c["powerRangeHex"] = powerHex_[0] ? powerHex_ : "fehlt";

    if (liveCount_) {
        JsonObject d = obj["data"].to<JsonObject>();
        d["powerW"] = live_.powerW;
        d["cadenceRpm"] = live_.cadenceRpm();
        d["speedKmh"] = live_.speedKmh();
        d["distanceM"] = live_.distanceM;
        d["energyKcal"] = live_.energyTotalKcal;
        d["elapsedS"] = live_.elapsedS;
        if (caps_.ibdReportsHeartRate) d["heartRate"] = live_.heartRateBpm;
        d["flags"] = live_.flags;
    }

    if (lastResp_.valid) {
        JsonObject r = obj["lastResponse"].to<JsonObject>();
        r["opcode"] = ftms::opcodeName(lastResp_.request);
        r["result"] = ftms::controlResultName(lastResp_.result);
        r["ok"] = lastResp_.ok();
    }
    if (lastDeny_ && lastDeny_[0]) obj["lastDeny"] = lastDeny_;
}

void FtmsClient::appendIoValues(JsonObject ios) const {
    auto add = [&](const char* key, float value, const char* unit) {
        JsonObject o = ios[key].to<JsonObject>();
        o["type"] = "sensor";
        o["value"] = value;
        o["unit"] = unit;
    };
    if (!liveCount_) return;
    add("power", (float)live_.powerW, "W");
    add("cadence", live_.cadenceRpm(), "rpm");
    add("speed", live_.speedKmh(), "km/h");
    add("distance", (float)live_.distanceM, "m");
    add("calories", (float)live_.energyTotalKcal, "kcal");
    if (caps_.ibdReportsHeartRate && live_.heartRateBpm) {
        add("heart_rate", (float)live_.heartRateBpm, "bpm");
    }
    if (limiter_) {
        const int16_t lvl = limiter_->currentLevelTenths();
        if (lvl >= 0) add("level", lvl / 10.0f, "");
    }
}

}  // namespace ergo
