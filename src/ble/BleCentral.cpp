#include "BleCentral.h"

#include "FtmsTypes.h"

namespace ergo {

static BleCentral* g_central = nullptr;

class ErgoAdvCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* dev) override {
        if (g_central) g_central->onScanResult(dev);
    }
};

class ErgoClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient*) override {}
    void onDisconnect(NimBLEClient* c) override {
        if (g_central) g_central->onClientDisconnect(c);
    }
};

static ErgoAdvCallbacks g_advCb;
static ErgoClientCallbacks g_cliCb;

/** Backoff des Reconnects. Nach dem letzten Eintrag bleibt es bei 30 s —
 *  ein Bike, das aus ist, soll das Funkband nicht dauerhaft belegen. */
static const uint16_t kRetryDelaysS[] = {2, 5, 10, 20, 30};

// ──────────────────────────────────────────────────────────────── Start

void BleCentral::begin(ConfigStore* cfg) {
    cfg_ = cfg;
    g_central = this;
    NimBLEDevice::init("");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setSecurityAuth(false, false, false);
    Serial.println("[BLE] Central bereit");
}

// ───────────────────────────────────────────────────────────────── Scan

bool BleCentral::startScan(uint16_t seconds) {
    if (seconds < 2) seconds = 2;
    if (seconds > 30) seconds = 30;
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (scan->isScanning()) scan->stop();
    scan->setAdvertisedDeviceCallbacks(&g_advCb, false);
    // Aktiver Scan: ohne Scan Response fehlen bei vielen Geraeten die Namen,
    // und eine Liste aus nackten MAC-Adressen ist unbedienbar.
    scan->setActiveScan(true);
    scan->setInterval(160);
    scan->setWindow(80);
    if (!scan->start(seconds, nullptr, false)) return false;
    scanUntil_ = millis() + (uint32_t)seconds * 1000UL;
    return true;
}

void BleCentral::stopScan() {
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (scan->isScanning()) scan->stop();
    scanUntil_ = 0;
}

bool BleCentral::scanning() const {
    return NimBLEDevice::getScan()->isScanning();
}

void BleCentral::clearScan() {
    for (uint8_t i = 0; i < ERGO_MAX_SCAN; i++) scan_[i] = ScanEntry{};
    scanCount_ = 0;
}

int BleCentral::findScan(const char* mac) const {
    if (!mac || !*mac) return -1;
    for (uint8_t i = 0; i < scanCount_; i++) {
        if (strcasecmp(scan_[i].mac, mac) == 0) return (int)i;
    }
    return -1;
}

void BleCentral::onScanResult(NimBLEAdvertisedDevice* dev) {
    if (!dev) return;
    std::string addr = dev->getAddress().toString();
    int idx = findScan(addr.c_str());
    if (idx < 0) {
        if (scanCount_ >= ERGO_MAX_SCAN) return;
        idx = scanCount_++;
        scan_[idx] = ScanEntry{};
        strncpy(scan_[idx].mac, addr.c_str(), sizeof(scan_[idx].mac) - 1);
    }
    ScanEntry& e = scan_[idx];
    e.rssi = dev->getRSSI();
    e.addrType = dev->getAddress().getType();
    e.lastSeen = millis();
    if (e.seen < 0xFFFF) e.seen++;

    std::string name = dev->getName();
    if (!name.empty()) {
        strncpy(e.name, name.c_str(), sizeof(e.name) - 1);
        e.name[sizeof(e.name) - 1] = 0;
    }

    // Ueber alle Pakete hinweg sammeln: manche Geraete legen den Service nur in
    // die Scan Response, andere wechseln zwischen den Paketen.
    const size_t n = dev->getServiceUUIDCount();
    for (size_t i = 0; i < n; i++) {
        const NimBLEUUID u = dev->getServiceUUID(i);
        if (u == NimBLEUUID(ftms::kSvcFitnessMachine)) e.hasFtms = true;
        if (u == NimBLEUUID((uint16_t)0x180D)) e.hasHr = true;
    }
}

void BleCentral::scanToJson(JsonArray arr) const {
    const uint32_t now = millis();
    for (uint8_t i = 0; i < scanCount_; i++) {
        const ScanEntry& e = scan_[i];
        JsonObject o = arr.add<JsonObject>();
        o["mac"] = e.mac;
        o["name"] = e.name;
        o["rssi"] = e.rssi;
        o["addrType"] = e.addrType;
        o["ftms"] = e.hasFtms;
        o["hr"] = e.hasHr;
        o["ageMs"] = now - e.lastSeen;
        o["seen"] = e.seen;
        if (cfg_) {
            if (cfg_->bikeMac.length() && strcasecmp(e.mac, cfg_->bikeMac.c_str()) == 0)
                o["role"] = "bike";
            else if (cfg_->hrMac.length() && strcasecmp(e.mac, cfg_->hrMac.c_str()) == 0)
                o["role"] = "hr";
        }
    }
}

// ─────────────────────────────────────────────────────────── Verbindung

uint8_t BleCentral::resolveAddrType(const char* mac) const {
    const int idx = findScan(mac);
    if (idx >= 0) return scan_[idx].addrType;
    return BLE_ADDR_PUBLIC;
}

const char* BleCentral::memoMac(Role role) const {
    if (!cfg_) return "";
    return (role == Role::Bike) ? cfg_->bikeMac.c_str() : cfg_->hrMac.c_str();
}

int8_t BleCentral::memoAddrType(Role role) const {
    if (!cfg_) return -1;
    return (role == Role::Bike) ? cfg_->bikeAddrType : cfg_->hrAddrType;
}

void BleCentral::rememberRole(Role role, const Link& l) {
    if (!cfg_) return;
    if (role == Role::Bike) {
        cfg_->bikeMac = l.mac;
        if (l.name[0]) cfg_->bikeName = l.name;
        cfg_->bikeAddrType = (int8_t)l.addrType;
    } else {
        cfg_->hrMac = l.mac;
        if (l.name[0]) cfg_->hrName = l.name;
        cfg_->hrAddrType = (int8_t)l.addrType;
    }
    cfg_->save();
}

bool BleCentral::connectRole(Role role, const char* mac, int addrTypeHint, char* err,
                             size_t errLen) {
    const uint8_t r = (uint8_t)role;
    if (!mac || !*mac) {
        if (err) snprintf(err, errLen, "MAC fehlt");
        return false;
    }

    // Scannen und Verbinden gleichzeitig macht den Controller unnoetig fragil.
    if (scanning()) stopScan();

    if (links_[r].client) release(role);

    clearLink(role, mac);
    Link& l = links_[r];
    l.state = LinkState::Connecting;

    uint8_t addrType = (addrTypeHint >= 0) ? (uint8_t)addrTypeHint : resolveAddrType(mac);

    l.client = NimBLEDevice::createClient();
    if (!l.client) {
        l.state = LinkState::Idle;
        if (err) snprintf(err, errLen, "createClient fehlgeschlagen");
        return false;
    }
    l.client->setClientCallbacks(&g_cliCb, false);
    // Weiche Parameter: weniger Controller-Stress, laengeres Supervision-Timeout.
    // Bei zwei Links plus WiFi ist das der Unterschied zwischen stabil und nicht.
    l.client->setConnectionParams(40, 80, 0, 400, 80, 60);
    l.client->setConnectTimeout(12);

    bool ok = l.client->connect(NimBLEAddress(mac, addrType));
    if (!ok) {
        // Der Adresstyp ist die haeufigste Ursache. Zweiter Versuch mit dem anderen.
        const uint8_t alt = (addrType == BLE_ADDR_PUBLIC) ? BLE_ADDR_RANDOM : BLE_ADDR_PUBLIC;
        ok = l.client->connect(NimBLEAddress(mac, alt));
        if (ok) addrType = alt;
    }
    if (!ok) {
        NimBLEDevice::deleteClient(l.client);
        clearLink(role, mac);
        links_[r].state = LinkState::Lost;
        if (err) snprintf(err, errLen, "Verbindung fehlgeschlagen");
        Serial.printf("[BLE] %s connect %s fehlgeschlagen\n", roleName(role), mac);
        return false;
    }

    l.addrType = addrType;
    l.connectedAt = millis();
    l.rssi = (int8_t)l.client->getRssi();
    strncpy(l.mac, l.client->getPeerAddress().toString().c_str(), sizeof(l.mac) - 1);
    l.mac[sizeof(l.mac) - 1] = 0;
    const int si = findScan(l.mac);
    if (si >= 0 && scan_[si].name[0]) strncpy(l.name, scan_[si].name, sizeof(l.name) - 1);
    l.state = LinkState::Discovering;

    suppress_[r] = false;
    retries_[r] = 0;
    nextRetryAt_[r] = 0;
    rememberRole(role, l);

    Serial.printf("[BLE] %s verbunden: %s (%s) type=%u rssi=%d\n", roleName(role), l.mac,
                  l.name[0] ? l.name : "-", (unsigned)addrType, (int)l.rssi);

    // Die Protokollschicht darf erst aus loop() heraus arbeiten, nie aus einem
    // BLE-Callback: eine GATT-Operation im Callback-Kontext blockiert den
    // NimBLE-Host-Task und laeuft in den Watchdog.
    pendingUp_[r] = true;
    return true;
}

void BleCentral::markReady(Role role) {
    Link& l = links_[(uint8_t)role];
    if (l.state == LinkState::Discovering) l.state = LinkState::Ready;
}

void BleCentral::release(Role role) {
    Link& l = links_[(uint8_t)role];
    if (l.client) {
        if (l.client->isConnected()) l.client->disconnect();
        NimBLEDevice::deleteClient(l.client);
        l.client = nullptr;
    }
}

void BleCentral::clearLink(Role role, const char* keepMac) {
    char mac[18] = {0};
    if (keepMac) strncpy(mac, keepMac, sizeof(mac) - 1);
    Link& l = links_[(uint8_t)role];
    memset((void*)&l, 0, sizeof(Link));
    l.state = LinkState::Idle;
    if (mac[0]) strncpy(l.mac, mac, sizeof(l.mac) - 1);
}

void BleCentral::disconnectRole(Role role) {
    const uint8_t r = (uint8_t)role;
    const bool wasUp = links_[r].state == LinkState::Ready ||
                       links_[r].state == LinkState::Discovering;
    suppress_[r] = true;
    release(role);
    char mac[18] = {0};
    strncpy(mac, links_[r].mac, sizeof(mac) - 1);
    clearLink(role, mac);
    if (wasUp && event_) event_(role, false);
    Serial.printf("[BLE] %s getrennt (beabsichtigt)\n", roleName(role));
}

void BleCentral::forgetRole(Role role) {
    disconnectRole(role);
    if (cfg_) {
        if (role == Role::Bike) {
            cfg_->bikeMac = "";
            cfg_->bikeName = "";
            cfg_->bikeAddrType = -1;
        } else {
            cfg_->hrMac = "";
            cfg_->hrName = "";
            cfg_->hrAddrType = -1;
        }
        cfg_->save();
    }
    clearLink(role);
}

bool BleCentral::reconnectRole(Role role, char* err, size_t errLen) {
    const char* mac = memoMac(role);
    if (!mac || !*mac) {
        if (err) snprintf(err, errLen, "kein Geraet gemerkt");
        return false;
    }
    return connectRole(role, mac, memoAddrType(role), err, errLen);
}

void BleCentral::disconnectAll() {
    disconnectRole(Role::Bike);
    disconnectRole(Role::Hr);
}

void BleCentral::onClientDisconnect(NimBLEClient* c) {
    // Nur markieren. Aufraeumen und Ereignis liefern macht loop().
    for (uint8_t r = 0; r < kRoleCount; r++) {
        if (links_[r].client == c) links_[r].dropped = true;
    }
}

uint8_t BleCentral::linkCount() const {
    uint8_t n = 0;
    for (uint8_t r = 0; r < kRoleCount; r++) {
        if (links_[r].state == LinkState::Ready || links_[r].state == LinkState::Discovering) n++;
    }
    return n;
}

// ───────────────────────────────────────────────────────────────── Loop

void BleCentral::loop() {
    const uint32_t now = millis();

    for (uint8_t r = 0; r < kRoleCount; r++) {
        const Role role = (Role)r;
        Link& l = links_[r];

        // 1. Verlust verarbeiten
        if (l.dropped) {
            l.dropped = false;
            const bool wasUp = l.state == LinkState::Ready || l.state == LinkState::Discovering;
            release(role);
            l.state = LinkState::Lost;
            l.lostAt = now;
            if (l.losses < 0xFFFF) l.losses++;
            Serial.printf("[BLE] %s Verbindung verloren (%u.)\n", roleName(role),
                          (unsigned)l.losses);
            if (wasUp && event_) event_(role, false);
            retries_[r] = 0;
            nextRetryAt_[r] = now + 1500UL;  // dem Controller Luft lassen
        }

        // 2. Aufstieg melden — im loop()-Kontext, nicht im Callback
        if (pendingUp_[r]) {
            pendingUp_[r] = false;
            if (event_) event_(role, true);
        }

        // 3. Reconnect
        if (l.state != LinkState::Lost || suppress_[r]) continue;
        const char* mac = memoMac(role);
        if (!mac || !*mac) {
            l.state = LinkState::Idle;
            continue;
        }
        if (now < nextRetryAt_[r]) continue;
        // Waehrend eines laufenden Scans nicht dazwischenfunken.
        if (scanning()) {
            nextRetryAt_[r] = now + 1000UL;
            continue;
        }

        const uint8_t step = retries_[r] < (uint8_t)(sizeof(kRetryDelaysS) / sizeof(kRetryDelaysS[0]))
                                 ? retries_[r]
                                 : (uint8_t)(sizeof(kRetryDelaysS) / sizeof(kRetryDelaysS[0]) - 1);
        if (retries_[r] < 250) retries_[r]++;
        nextRetryAt_[r] = now + (uint32_t)kRetryDelaysS[step] * 1000UL;

        char err[48] = {0};
        Serial.printf("[BLE] %s Reconnect-Versuch %u\n", roleName(role), (unsigned)retries_[r]);
        if (connectRole(role, mac, memoAddrType(role), err, sizeof(err))) {
            if (l.reconnects < 0xFFFF) l.reconnects++;
        }
    }

    // Scanfenster abgelaufen: Callbacks abmelden, damit die Liste stabil bleibt
    if (scanUntil_ && now > scanUntil_ && !scanning()) scanUntil_ = 0;

    // RSSI der offenen Links nachziehen — billig und in der UI sichtbar
    static uint32_t lastRssi = 0;
    if (now - lastRssi > 3000UL) {
        lastRssi = now;
        for (uint8_t r = 0; r < kRoleCount; r++) {
            Link& l = links_[r];
            if (l.client && l.client->isConnected()) l.rssi = (int8_t)l.client->getRssi();
        }
    }
}

// ───────────────────────────────────────────────────────────────── JSON

void BleCentral::linksToJson(JsonObject obj) const {
    for (uint8_t r = 0; r < kRoleCount; r++) {
        const Link& l = links_[r];
        JsonObject o = obj[roleName((Role)r)].to<JsonObject>();
        o["state"] = linkStateName(l.state);
        o["mac"] = l.mac;
        o["name"] = l.name;
        o["rssi"] = l.rssi;
        o["losses"] = l.losses;
        o["reconnects"] = l.reconnects;
        if (l.connectedAt) o["upS"] = (millis() - l.connectedAt) / 1000UL;
        if (cfg_) {
            o["rememberedMac"] = (r == 0) ? cfg_->bikeMac : cfg_->hrMac;
            o["rememberedName"] = (r == 0) ? cfg_->bikeName : cfg_->hrName;
        }
    }
}

void BleCentral::appendStatusJson(JsonObject obj) const {
    obj["scanning"] = scanning();
    obj["scanCount"] = scanCount_;
    obj["linkCount"] = linkCount();
    linksToJson(obj["links"].to<JsonObject>());
}

}  // namespace ergo
