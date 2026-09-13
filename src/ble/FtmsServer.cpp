#include "FtmsServer.h"

#include <string.h>

#include "BuildFlags.h"
#include "CyclingCodec.h"

static FtmsServer* g_ftmsSrv = nullptr;

class BridgeServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer*, ble_gap_conn_desc* desc) override {
        if (g_ftmsSrv && desc) g_ftmsSrv->onConnect(desc->conn_handle);
        else if (g_ftmsSrv) g_ftmsSrv->onConnect(0);
    }
    void onDisconnect(NimBLEServer*, ble_gap_conn_desc* desc) override {
        if (g_ftmsSrv && desc) g_ftmsSrv->onDisconnect(desc->conn_handle);
        else if (g_ftmsSrv) g_ftmsSrv->onDisconnect(0);
    }
};

class BridgeCpCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c, ble_gap_conn_desc*) override {
        if (g_ftmsSrv) g_ftmsSrv->onControlWrite(c);
    }
    void onWrite(NimBLECharacteristic* c) override {
        if (g_ftmsSrv) g_ftmsSrv->onControlWrite(c);
    }
};

static BridgeServerCallbacks g_srvCb;
static BridgeCpCallbacks g_cpCb;

// FTMS Service Data: Fitness Machine Type, Bit 5 = Indoor Bike.
static const char kIndoorBikeSvcData[] = {0x20, 0x00};

void FtmsServer::begin(ConfigStore* cfg) {
    cfg_ = cfg;
    g_ftmsSrv = this;
    String n = effectiveName();
    strncpy(name_, n.c_str(), sizeof(name_) - 1);
    name_[sizeof(name_) - 1] = 0;
    if (cfg_ && cfg_->bridgeEnabled) setEnabled(true);
}

String FtmsServer::effectiveName() const {
    if (cfg_ && cfg_->bridgeName.length()) return cfg_->bridgeName;
    if (cfg_ && cfg_->deviceName.length()) return cfg_->deviceName + "-Bridge";
    return String("Ergo-Bridge");
}

void FtmsServer::seedStaticChars() {
    uint8_t buf[8];
    size_t n = ftms::encodeFeature(ftms::bridgeFeatureSet(), buf, sizeof(buf));
    if (feature_ && n) feature_->setValue(buf, n);

    n = ftms::encodeResistanceRange(ftms::bridgeResistanceRange(), buf, sizeof(buf));
    if (resRange_ && n) resRange_->setValue(buf, n);

    n = ftms::encodePowerRange(ftms::bridgePowerRange(), buf, sizeof(buf));
    if (pwrRange_ && n) pwrRange_->setValue(buf, n);
}

void FtmsServer::ensureServer() {
    if (serverReady_) return;

    server_ = NimBLEDevice::createServer();
    server_->setCallbacks(&g_srvCb, false);

    NimBLEService* svc = server_->createService(NimBLEUUID((uint16_t)ftms::kSvcFitnessMachine));

    feature_ = svc->createCharacteristic(NimBLEUUID((uint16_t)ftms::kChrFeature),
                                         NIMBLE_PROPERTY::READ);

    ibd_ = svc->createCharacteristic(NimBLEUUID((uint16_t)ftms::kChrIndoorBikeData),
                                     NIMBLE_PROPERTY::NOTIFY);

    resRange_ = svc->createCharacteristic(NimBLEUUID((uint16_t)ftms::kChrResistanceRange),
                                          NIMBLE_PROPERTY::READ);

    pwrRange_ = svc->createCharacteristic(NimBLEUUID((uint16_t)ftms::kChrPowerRange),
                                          NIMBLE_PROPERTY::READ);

    // Spec: Write + Indicate (nicht die Notify-Eigenheit des Varon nachbauen).
    cp_ = svc->createCharacteristic(
        NimBLEUUID((uint16_t)ftms::kChrControlPoint),
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE);
    cp_->setCallbacks(&g_cpCb);

    status_ = svc->createCharacteristic(NimBLEUUID((uint16_t)ftms::kChrStatus),
                                        NIMBLE_PROPERTY::NOTIFY);

    seedStaticChars();
    svc->start();

    // CPS / CSC — manche Apps bestehen darauf, neben FTMS.
    NimBLEService* cps = server_->createService(NimBLEUUID((uint16_t)cycling::kSvcCyclingPower));
    NimBLECharacteristic* cpsFeat = cps->createCharacteristic(
        NimBLEUUID((uint16_t)cycling::kChrCyclingPowerFeature), NIMBLE_PROPERTY::READ);
    uint8_t cpsFeatBuf[4];
    size_t cpn = cycling::encodeCyclingPowerFeature(cycling::kCpsFeatCrank, cpsFeatBuf,
                                                    sizeof(cpsFeatBuf));
    if (cpn) cpsFeat->setValue(cpsFeatBuf, cpn);
    cpsMeas_ = cps->createCharacteristic(NimBLEUUID((uint16_t)cycling::kChrCyclingPowerMeasurement),
                                         NIMBLE_PROPERTY::NOTIFY);
    cps->start();

    NimBLEService* csc = server_->createService(NimBLEUUID((uint16_t)cycling::kSvcCsc));
    NimBLECharacteristic* cscFeat = csc->createCharacteristic(
        NimBLEUUID((uint16_t)cycling::kChrCscFeature), NIMBLE_PROPERTY::READ);
    uint8_t cscFeatBuf[2];
    size_t csn = cycling::encodeCscFeature(cycling::kCscFeatCrank, cscFeatBuf, sizeof(cscFeatBuf));
    if (csn) cscFeat->setValue(cscFeatBuf, csn);
    cscMeas_ = csc->createCharacteristic(NimBLEUUID((uint16_t)cycling::kChrCscMeasurement),
                                         NIMBLE_PROPERTY::NOTIFY);
    csc->start();

    NimBLEService* dis = server_->createService(NimBLEUUID((uint16_t)0x180A));
    auto setStr = [&](uint16_t uuid, const char* v) {
        NimBLECharacteristic* c =
            dis->createCharacteristic(NimBLEUUID(uuid), NIMBLE_PROPERTY::READ);
        c->setValue(v);
    };
    setStr(0x2A29, "MPunktBPunkt");
    setStr(0x2A24, "esp32.ergo");
    setStr(0x2A26, FW_VERSION);
    dis->start();

    serverReady_ = true;
    Serial.println("[BRIDGE] GATT server ready (FTMS+CPS+CSC)");
}

void FtmsServer::startAdvertising() {
    if (!serverReady_) return;
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->stop();
    adv->reset();
    adv->addServiceUUID(NimBLEUUID((uint16_t)ftms::kSvcFitnessMachine));
    adv->setAppearance(0x0480);  // Generic Cycling
    adv->setName(name_);
    adv->setServiceData(NimBLEUUID((uint16_t)ftms::kSvcFitnessMachine),
                        std::string(kIndoorBikeSvcData, sizeof(kIndoorBikeSvcData)));
    adv->setScanResponse(true);
    NimBLEDevice::startAdvertising();
    advertising_ = true;
    Serial.printf("[BRIDGE] advertising as %s\n", name_);
}

void FtmsServer::stopAdvertising() {
    if (!serverReady_) {
        advertising_ = false;
        return;
    }
    NimBLEDevice::stopAdvertising();
    advertising_ = false;
    Serial.println("[BRIDGE] advertising stopped");
}

void FtmsServer::disconnectAll() {
    if (!server_) return;
    std::vector<uint16_t> peers = server_->getPeerDevices();
    for (uint16_t h : peers) server_->disconnect(h);
}

void FtmsServer::refreshAdvertising() {
    if (!enabled_) return;
    // Eine App (MyWhoosh) reicht fuer MVP.
    if (clients() >= 1) {
        if (advertising_) stopAdvertising();
    } else if (!advertising_) {
        startAdvertising();
    }
}

void FtmsServer::setEnabled(bool on) {
    if (cfg_) cfg_->bridgeEnabled = on;
    if (on) {
        if (enabled_ && serverReady_) {
            if (!advertising_ && clients() < 1) startAdvertising();
            return;
        }
        enabled_ = true;
        String n = effectiveName();
        strncpy(name_, n.c_str(), sizeof(name_) - 1);
        name_[sizeof(name_) - 1] = 0;
        ensureServer();
        startAdvertising();
    } else {
        if (!enabled_ && !serverReady_) return;
        enabled_ = false;
        controlGranted_ = false;
        clearController_();
        pendingReady_ = false;
        pending_ = Pending{};
        disconnectAll();
        stopAdvertising();
    }
}

void FtmsServer::noteLoadCommand_() {
    controlling_ = true;
    if (loadCmds_ < 0xFFFFFFFFu) loadCmds_++;
}

void FtmsServer::clearController_() {
    controlling_ = false;
    loadCmds_ = 0;
}

const char* FtmsServer::clientRole() const {
    if (clients() == 0) return "none";
    if (controlling_) return "controller";
    if (subscribedIbd() > 0 || controlGranted_) return "observer";
    return "connected";
}

uint8_t FtmsServer::clients() const {
    if (!server_) return 0;
    return (uint8_t)server_->getConnectedCount();
}

uint8_t FtmsServer::subscribedIbd() const {
    if (!ibd_) return 0;
    return (uint8_t)ibd_->getSubscribedCount();
}

void FtmsServer::onConnect(uint16_t) {
    refreshAdvertising();
}

void FtmsServer::onDisconnect(uint16_t) {
    controlGranted_ = false;
    clearController_();
    refreshAdvertising();
}

void FtmsServer::indicateControlResponse(ftms::Opcode request, ftms::ControlResult result) {
    if (!cp_) return;
    uint8_t buf[3];
    const size_t n = ftms::encodeControlResponse(request, result, buf, sizeof(buf));
    if (!n) return;
    cp_->setValue(buf, n);
    cp_->indicate();
    indicateSent_++;
}

void FtmsServer::notifyMachineStatus(uint8_t op, const uint8_t* param, size_t paramLen) {
    if (!status_ || status_->getSubscribedCount() == 0) return;
    uint8_t buf[8];
    if (1 + paramLen > sizeof(buf)) return;
    buf[0] = op;
    if (param && paramLen) memcpy(buf + 1, param, paramLen);
    status_->setValue(buf, 1 + paramLen);
    status_->notify();
}

void FtmsServer::onControlWrite(NimBLECharacteristic* c) {
    if (!enabled_ || !c) return;
    std::string v = c->getValue();
    const uint8_t* data = (const uint8_t*)v.data();
    const size_t len = v.size();
    cpWrites_++;

    ftms::ControlWrite w;
    if (!ftms::decodeControlWrite(data, len, w) || !w.valid) {
        indicateControlResponse(ftms::Opcode::RequestControl, ftms::ControlResult::InvalidParameter);
        return;
    }

    auto queue = [&](PendingOp op, int16_t a = 0, int16_t b = 0) {
        pending_.op = op;
        pending_.watt = a;
        pending_.resistanceTenths = b;
        pendingReady_ = true;
    };

    switch (w.op) {
        case ftms::Opcode::RequestControl:
            controlGranted_ = true;
            queue(PendingOp::RequestControl);
            indicateControlResponse(w.op, ftms::ControlResult::Success);
            return;

        case ftms::Opcode::Reset:
            if (!controlGranted_) {
                indicateControlResponse(w.op, ftms::ControlResult::ControlNotPermitted);
                return;
            }
            clearController_();
            queue(PendingOp::Reset);
            indicateControlResponse(w.op, ftms::ControlResult::Success);
            notifyMachineStatus(0x01, nullptr, 0);
            return;

        case ftms::Opcode::StartResume:
            if (!controlGranted_) {
                indicateControlResponse(w.op, ftms::ControlResult::ControlNotPermitted);
                return;
            }
            queue(PendingOp::Start);
            indicateControlResponse(w.op, ftms::ControlResult::Success);
            notifyMachineStatus(0x04, nullptr, 0);
            return;

        case ftms::Opcode::StopPause: {
            if (!controlGranted_) {
                indicateControlResponse(w.op, ftms::ControlResult::ControlNotPermitted);
                return;
            }
            const bool pause = (w.stopParam == (uint8_t)ftms::StopParam::Pause);
            if (!pause) clearController_();
            queue(pause ? PendingOp::Pause : PendingOp::Stop);
            indicateControlResponse(w.op, ftms::ControlResult::Success);
            notifyMachineStatus(0x02, &w.stopParam, 1);
            return;
        }

        case ftms::Opcode::SetTargetPower: {
            if (!controlGranted_) {
                indicateControlResponse(w.op, ftms::ControlResult::ControlNotPermitted);
                return;
            }
            const ftms::PowerRange pr = ftms::bridgePowerRange();
            if (w.watt < pr.minW || w.watt > pr.maxW) {
                indicateControlResponse(w.op, ftms::ControlResult::InvalidParameter);
                return;
            }
            noteLoadCommand_();
            queue(PendingOp::SetPower, w.watt, 0);
            indicateControlResponse(w.op, ftms::ControlResult::Success);
            uint8_t p[2] = {(uint8_t)(w.watt & 0xFF), (uint8_t)((w.watt >> 8) & 0xFF)};
            notifyMachineStatus(0x08, p, 2);
            return;
        }

        case ftms::Opcode::SetTargetResistance: {
            if (!controlGranted_) {
                indicateControlResponse(w.op, ftms::ControlResult::ControlNotPermitted);
                return;
            }
            const ftms::ResistanceRange rr = ftms::bridgeResistanceRange();
            if (w.resistanceTenths < rr.minRaw || w.resistanceTenths > rr.maxRaw) {
                indicateControlResponse(w.op, ftms::ControlResult::InvalidParameter);
                return;
            }
            noteLoadCommand_();
            queue(PendingOp::SetResistance, 0, w.resistanceTenths);
            indicateControlResponse(w.op, ftms::ControlResult::Success);
            uint8_t p[2] = {(uint8_t)(w.resistanceTenths & 0xFF),
                            (uint8_t)((w.resistanceTenths >> 8) & 0xFF)};
            notifyMachineStatus(0x07, p, 2);
            return;
        }

        case ftms::Opcode::SetIndoorBikeSimulation: {
            if (!controlGranted_) {
                indicateControlResponse(w.op, ftms::ControlResult::ControlNotPermitted);
                return;
            }
            if (!allowSim_) {
                indicateControlResponse(w.op, ftms::ControlResult::NotSupported);
                return;
            }
            noteLoadCommand_();
            pending_.op = PendingOp::SetSimulation;
            pending_.watt = 0;
            pending_.resistanceTenths = 0;
            pending_.windMms = w.windMms;
            pending_.gradeHundredth = w.gradeHundredth;
            pending_.crr10000 = w.crr10000;
            pending_.cw100 = w.cw100;
            pendingReady_ = true;
            indicateControlResponse(w.op, ftms::ControlResult::Success);
            return;
        }

        default:
            indicateControlResponse(w.op, ftms::ControlResult::NotSupported);
            return;
    }
}

bool FtmsServer::takePending(Pending& out) {
    if (!pendingReady_) return false;
    out = pending_;
    pending_ = Pending{};
    pendingReady_ = false;
    return out.op != PendingOp::None;
}

void FtmsServer::notifyIndoorBike(const ftms::IndoorBikeData& d) {
    if (!enabled_ || !ibd_ || ibd_->getSubscribedCount() == 0) return;
    uint8_t buf[ftms::kMaxIndoorBikeLen];
    const size_t n = ftms::encodeIndoorBikeData(d, buf, sizeof(buf));
    if (!n) return;
    ibd_->setValue(buf, n);
    ibd_->notify();
    notifySent_++;
    lastNotifyMs_ = millis();
}

void FtmsServer::notifyCycling(int16_t watt, uint16_t cumCrank, uint16_t crankEvent1024) {
    if (!enabled_) return;
    if (cpsMeas_ && cpsMeas_->getSubscribedCount() > 0) {
        uint8_t buf[4];
        const size_t n = cycling::encodeCyclingPower(watt, buf, sizeof(buf));
        if (n) {
            cpsMeas_->setValue(buf, n);
            cpsMeas_->notify();
            cpsSent_++;
        }
    }
    if (cscMeas_ && cscMeas_->getSubscribedCount() > 0) {
        uint8_t buf[5];
        const size_t n = cycling::encodeCscCrank(cumCrank, crankEvent1024, buf, sizeof(buf));
        if (n) {
            cscMeas_->setValue(buf, n);
            cscMeas_->notify();
            cscSent_++;
        }
    }
}

void FtmsServer::loop() {
    if (!enabled_ || !serverReady_) return;
    refreshAdvertising();
}

void FtmsServer::appendStatusJson(JsonObject obj) const {
    obj["enabled"] = enabled_;
    obj["advertising"] = advertising_;
    obj["clients"] = clients();
    obj["ibdSubs"] = subscribedIbd();
    obj["controlGranted"] = controlGranted_;
    obj["controlling"] = controlling_;
    obj["clientRole"] = clientRole();
    obj["loadCommands"] = loadCmds_;
    obj["allowSim"] = allowSim_;
    obj["name"] = name_;
    obj["notifySent"] = notifySent_;
    obj["indicateSent"] = indicateSent_;
    obj["cpWrites"] = cpWrites_;
    obj["cpsSent"] = cpsSent_;
    obj["cscSent"] = cscSent_;
    if (lastNotifyMs_) obj["lastNotifyMs"] = lastNotifyMs_;
}
