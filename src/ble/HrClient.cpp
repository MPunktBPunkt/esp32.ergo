#include "HrClient.h"

namespace ergo {

static constexpr uint16_t kSvcHeartRate = 0x180D;
static constexpr uint16_t kChrHeartRate = 0x2A37;
static constexpr uint16_t kSvcBattery = 0x180F;
static constexpr uint16_t kChrBatteryLevel = 0x2A19;

static HrClient* g_hr = nullptr;

static void hrNotifyCb(NimBLERemoteCharacteristic* chr, uint8_t* data, size_t len, bool) {
    if (g_hr) g_hr->onNotify(chr, data, len);
}

bool HrClient::attach(NimBLEClient* client) {
    detach();
    if (!client || !client->isConnected()) return false;
    g_hr = this;

    NimBLERemoteService* svc = client->getService(NimBLEUUID(kSvcHeartRate));
    if (!svc) {
        Serial.println("[HR] kein 0x180D auf diesem Geraet");
        return false;
    }
    client_ = client;
    hr_ = svc->getCharacteristic(NimBLEUUID(kChrHeartRate));
    if (!hr_) {
        Serial.println("[HR] kein 0x2A37");
        return false;
    }
    if (!hr_->subscribe(true, hrNotifyCb, true) && !hr_->subscribe(true, hrNotifyCb, false)) {
        Serial.println("[HR] Abo auf 0x2A37 fehlgeschlagen");
        return false;
    }

    // Batterie ist optional und wird einmalig gelesen. Ein Abo dafuer waere
    // Funkverkehr fuer einen Wert, der sich in Stunden bewegt.
    NimBLERemoteService* bat = client->getService(NimBLEUUID(kSvcBattery));
    if (bat) {
        NimBLERemoteCharacteristic* c = bat->getCharacteristic(NimBLEUUID(kChrBatteryLevel));
        if (c && c->canRead()) {
            std::string v = c->readValue();
            if (v.size() >= 1) battery_ = (uint8_t)v[0];
        }
    }

    Serial.printf("[HR] verbunden, Batterie %u%%\n", (unsigned)battery_);
    return true;
}

void HrClient::detach() {
    client_ = nullptr;
    hr_ = nullptr;
    sample_ = HrSample{};
    count_ = 0;
    battery_ = 0;
}

bool HrClient::stale(uint32_t nowMs) const {
    if (!count_) return true;
    return (nowMs - sample_.at) > ERGO_DATA_STALE_MS;
}

void HrClient::onNotify(NimBLERemoteCharacteristic* chr, const uint8_t* data, size_t len) {
    if (!chr || !data || len < 2) return;
    if (chr->getUUID() != NimBLEUUID(kChrHeartRate)) return;

    const uint8_t flags = data[0];
    size_t i = 1;

    HrSample s;
    if (flags & 0x01) {  // 16-Bit-Wert
        if (len < i + 2) return;
        const uint16_t v = (uint16_t)data[i] | ((uint16_t)data[i + 1] << 8);
        s.bpm = (uint8_t)(v > 255 ? 255 : v);
        i += 2;
    } else {
        s.bpm = data[i++];
    }

    // Bit 2 sagt, ob der Kontaktstatus ueberhaupt gemeldet wird. Fehlt er,
    // darf das nicht als "kein Kontakt" gelesen werden — der H9 meldet ihn nie.
    s.contactSupported = (flags & 0x04) != 0;
    s.contact = s.contactSupported ? ((flags & 0x02) ? 1 : 0) : 2;

    if (flags & 0x08) {  // Energy Expended
        if (len >= i + 2) {
            s.hasEnergy = true;
            s.energyKj = (uint16_t)data[i] | ((uint16_t)data[i + 1] << 8);
            i += 2;
        }
    }

    if (flags & 0x10) {  // RR-Intervalle, 1/1024 s
        while (len >= i + 2 && s.rrCount < (uint8_t)(sizeof(s.rr) / sizeof(s.rr[0]))) {
            s.rr[s.rrCount++] = (uint16_t)data[i] | ((uint16_t)data[i + 1] << 8);
            i += 2;
        }
    }

    s.at = millis();
    sample_ = s;
    count_++;
}

void HrClient::appendStatusJson(JsonObject obj) const {
    obj["attached"] = attached();
    obj["notifies"] = count_;
    obj["stale"] = stale(millis());
    if (battery_) obj["battery"] = battery_;
    if (count_) {
        obj["bpm"] = sample_.bpm;
        obj["rrCount"] = sample_.rrCount;
        obj["contact"] = sample_.contactSupported ? (sample_.contact ? "ok" : "kein") : "n/a";
    }
}

}  // namespace ergo
