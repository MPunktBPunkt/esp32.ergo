#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ble/FtmsCapabilities.h"

/**
 * Geraeteprofil je MAC — Stufenformat, Watt-Vertrauen, Kennflaeche, Eigenarten.
 *
 * Arduino-frei. Die Kennflaeche bleibt in PowerMap; dieser Store haelt die
 * Metadaten und waehlt den Slot, unter dem die App die Flaeche speichert.
 * Nutzerprofile (ProfileStore) und Geraeteprofile sind bewusst getrennt.
 */
namespace ergo {

constexpr uint8_t kDeviceMacLen = 18;   // "aa:bb:cc:dd:ee:ff" + NUL
constexpr uint8_t kDeviceNameLen = 24;
constexpr uint8_t kMaxDevices = 4;

/**
 * powerTrusted: -1 = aus GATT ableiten, 0 = nie vertrauen, 1 = vertrauen.
 * resistanceFormat: Unknown = aus GATT / Bereich ableiten.
 */
struct DeviceProfile {
    char mac[kDeviceMacLen] = {0};
    char name[kDeviceNameLen] = {0};
    uint8_t addrType = 0;
    ftms::ResistanceFormat resistanceFormat = ftms::ResistanceFormat::Unknown;
    int8_t powerTrusted = -1;
    bool requestControlOnReconnect = true;
    uint16_t measuredCeilingW = 0;  // 0 = unbekannt
    uint32_t lastSeenUnix = 0;

    bool hasMac() const { return mac[0] != '\0'; }
};

class DeviceStore {
public:
    static constexpr size_t kMaxBytes = 256;  // Meta-Blob (ohne Kennflaeche)

    void clear();
    uint8_t count() const { return count_; }
    int activeIndex() const { return active_; }
    const DeviceProfile* active() const;
    DeviceProfile* activeMutable();
    const DeviceProfile* at(uint8_t i) const;

    /** MAC normalisieren (klein, Doppelpunkt). false bei Muell. */
    static bool normalizeMac(const char* in, char* out, size_t outCap);

    /**
     * Geraet anlegen oder aktualisieren und aktiv setzen.
     * false wenn voll und MAC unbekannt.
     */
    bool remember(const char* mac, const char* name, uint8_t addrType, uint32_t nowUnix = 0);

    /** Aktives Geraet wechseln. false wenn unbekannt. */
    bool select(const char* mac);

    bool remove(const char* mac);

    /** Caps nach GATT-Ableitung ueberschreiben (Format / Watt-Vertrauen). */
    void applyTo(ftms::Capabilities& caps) const;

    /** Bekannte Varon-Eigenarten setzen, wenn Format noch Unknown. */
    static void seedVaronDefaults(DeviceProfile& d);

    size_t save(uint8_t* buf, size_t cap) const;
    bool load(const uint8_t* buf, size_t len);

    /** Slot-Index fuer NVS-Map-Key `m0`…`m3`. -1 ohne aktives Geraet. */
    int mapSlot() const { return active_; }

private:
    int findIndex(const char* macNorm) const;
    static bool macEqual(const char* a, const char* b);

    DeviceProfile items_[kMaxDevices];
    uint8_t count_ = 0;
    int8_t active_ = -1;
};

}  // namespace ergo
