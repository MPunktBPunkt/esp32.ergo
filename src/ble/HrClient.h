#pragma once

#include <ArduinoJson.h>
#include <NimBLEDevice.h>

#include "BleTypes.h"

namespace ergo {

/**
 * Pulsgurt ueber Standard-Heart-Rate-Service.
 *
 * Bewusst ohne jede Polar-Annahme: gelesen wird 0x2A37 nach Spec, und was
 * nicht kommt, fehlt einfach. Die vier Eigenheiten, die im Pflichtenheft als
 * H9-spezifisch markiert sind — ein BLE-Client, 5-kHz-Parallelsender, immer
 * RR-Intervalle, kein Sensor Contact — tauchen hier nirgends als Bedingung
 * auf. Ein anderer Gurt ist ein anderer Scan-Eintrag, kein Codeeingriff.
 */
class HrClient {
public:
    bool attach(NimBLEClient* client);
    void detach();

    bool attached() const { return client_ != nullptr; }
    const HrSample& sample() const { return sample_; }
    bool hasSample() const { return count_ > 0; }
    uint32_t count() const { return count_; }
    bool stale(uint32_t nowMs) const;
    uint8_t battery() const { return battery_; }

    void appendStatusJson(JsonObject obj) const;

    /** NimBLE-Callback, nicht selbst aufrufen. */
    void onNotify(NimBLERemoteCharacteristic* chr, const uint8_t* data, size_t len);

private:
    NimBLEClient* client_ = nullptr;
    NimBLERemoteCharacteristic* hr_ = nullptr;

    HrSample sample_;
    uint32_t count_ = 0;
    uint8_t battery_ = 0;
};

}  // namespace ergo
