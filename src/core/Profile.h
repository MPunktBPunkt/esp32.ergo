#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "control/Limiter.h"

/**
 * Nutzerprofil — Grenzen, Zonenparameter, Verhalten bei Pulsverlust.
 *
 * Arduino-frei und hosttestbar. Persistenz als Byte-Blob (NVS via App),
 * Roundtrip ueber save()/load() — gleiche Idee wie PowerMap.
 *
 * Die Kennflaeche Stufe × Kadenz → Watt gehoert NICHT hierher.
 */
namespace ergo {

enum class ZoneLead : uint8_t { Power = 0, Hr = 1 };
enum class ZoneBasis : uint8_t { HrMax = 0, Lthr = 1 };
enum class FtpOrigin : uint8_t { Manual = 0, Estimate = 1, Test = 2 };
enum class HrLossPolicy : uint8_t { Freeze = 0, Reduce = 1, Stop = 2 };

struct Profile {
    char id[16] = {0};
    char name[24] = {0};
    uint32_t color = 0x4EC9A5;  // Default-Akzent wie UI
    char initial[4] = {0};

    uint16_t ftpW = 0;
    uint32_t ftpDateUnix = 0;
    FtpOrigin ftpOrigin = FtpOrigin::Manual;

    uint8_t hrMax = 0;
    uint8_t restingHr = 0;
    uint8_t lthr = 0;
    ZoneBasis zoneBasis = ZoneBasis::HrMax;

    uint8_t weightKg = 0;
    ZoneLead leadingZone = ZoneLead::Power;

    /** Harte Grenzen. 0 = keine Profilvorgabe (Limiter-Default). */
    int16_t maxPowerW = 0;
    uint8_t maxHr = 0;
    int16_t maxLevelTenths = 0;

    uint8_t targetCadenceRpm = 0;
    HrLossPolicy onHrLoss = HrLossPolicy::Reduce;

    bool hasId() const { return id[0] != '\0'; }
};

inline void profileCopyId(char* dst, size_t cap, const char* src) {
    if (!dst || cap == 0) return;
    dst[0] = '\0';
    if (!src) return;
    size_t i = 0;
    for (; src[i] && i + 1 < cap; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

class ProfileStore {
public:
    static constexpr uint8_t kMaxProfiles = 4;
    /** Absolute Pulsdeckel — auch ein fehlerhaftes Profil darf nicht hoeher. */
    static constexpr uint8_t kHrCeilingMax = 190;
    static constexpr size_t kMaxBytes = 320;

    uint8_t count() const { return count_; }
    const Profile* at(uint8_t i) const { return i < count_ ? &items_[i] : nullptr; }

    /** nullptr, wenn keines gewaehlt ist — kein stilles Defaultprofil. */
    const Profile* active() const;
    const char* activeId() const { return activeId_[0] ? activeId_ : nullptr; }

    bool put(const Profile& p);
    bool get(const char* id, Profile& out) const;
    bool remove(const char* id);

    /**
     * Profil aktivieren. `sessionLocked` steht fuer „Session laeuft" — dann
     * kein Wechsel (Pflichtenheft). Ohne aktives Profil und ohne Lock ist
     * Select immer erlaubt.
     */
    bool select(const char* id, bool sessionLocked = false);
    void clearActive();
    void clearAll();

    /** Schreibt Profilgrenzen in die Limiter-Config (0 bleibt 0). */
    void applyTo(LimiterConfig& lc) const;

    /** Klemmt maxHr auf kHrCeilingMax; leere id/name → false. */
    static bool sanitize(Profile& p);

    /** Byte-Blob fuer NVS. 0 = Puffer zu klein / leer. */
    size_t save(uint8_t* out, size_t cap) const;
    bool load(const uint8_t* in, size_t len);

private:
    int findIndex(const char* id) const;

    Profile items_[kMaxProfiles];
    uint8_t count_ = 0;
    char activeId_[16] = {0};
};

}  // namespace ergo
