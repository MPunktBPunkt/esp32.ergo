#include "core/Profile.h"

namespace ergo {
namespace {

constexpr uint32_t kMagic = 0x45524750u;  // 'ERGP'
constexpr uint8_t kVersion = 1;

inline void w8(uint8_t*& p, uint8_t v) { *p++ = v; }
inline void w16(uint8_t*& p, uint16_t v) {
    *p++ = static_cast<uint8_t>(v & 0xff);
    *p++ = static_cast<uint8_t>((v >> 8) & 0xff);
}
inline void w32(uint8_t*& p, uint32_t v) {
    w16(p, static_cast<uint16_t>(v & 0xffff));
    w16(p, static_cast<uint16_t>((v >> 16) & 0xffff));
}
inline void wbytes(uint8_t*& p, const void* src, size_t n) {
    memcpy(p, src, n);
    p += n;
}

inline bool need(const uint8_t*& p, const uint8_t* end, size_t n) {
    return static_cast<size_t>(end - p) >= n;
}
inline uint8_t r8(const uint8_t*& p) { return *p++; }
inline uint16_t r16(const uint8_t*& p) {
    const uint16_t v = static_cast<uint16_t>(p[0] | (p[1] << 8));
    p += 2;
    return v;
}
inline uint32_t r32(const uint8_t*& p) {
    const uint32_t lo = r16(p);
    const uint32_t hi = r16(p);
    return lo | (hi << 16);
}
inline void rbytes(const uint8_t*& p, void* dst, size_t n) {
    memcpy(dst, p, n);
    p += n;
}

void writeProfile(uint8_t*& p, const Profile& pr) {
    wbytes(p, pr.id, 16);
    wbytes(p, pr.name, 24);
    w32(p, pr.color);
    wbytes(p, pr.initial, 4);
    w16(p, pr.ftpW);
    w32(p, pr.ftpDateUnix);
    w8(p, static_cast<uint8_t>(pr.ftpOrigin));
    w8(p, pr.hrMax);
    w8(p, pr.restingHr);
    w8(p, pr.lthr);
    w8(p, static_cast<uint8_t>(pr.zoneBasis));
    w8(p, pr.weightKg);
    w8(p, static_cast<uint8_t>(pr.leadingZone));
    w16(p, static_cast<uint16_t>(pr.maxPowerW));
    w8(p, pr.maxHr);
    w16(p, static_cast<uint16_t>(pr.maxLevelTenths));
    w8(p, pr.targetCadenceRpm);
    w8(p, static_cast<uint8_t>(pr.onHrLoss));
}

bool readProfile(const uint8_t*& p, const uint8_t* end, Profile& pr) {
    // 16+24+4+4+2+4+1+1+1+1+1+1+1+2+1+2+1+1 = 68
    if (!need(p, end, 68)) return false;
    pr = Profile{};
    rbytes(p, pr.id, 16);
    rbytes(p, pr.name, 24);
    pr.color = r32(p);
    rbytes(p, pr.initial, 4);
    pr.ftpW = r16(p);
    pr.ftpDateUnix = r32(p);
    pr.ftpOrigin = static_cast<FtpOrigin>(r8(p));
    pr.hrMax = r8(p);
    pr.restingHr = r8(p);
    pr.lthr = r8(p);
    pr.zoneBasis = static_cast<ZoneBasis>(r8(p));
    pr.weightKg = r8(p);
    pr.leadingZone = static_cast<ZoneLead>(r8(p));
    pr.maxPowerW = static_cast<int16_t>(r16(p));
    pr.maxHr = r8(p);
    pr.maxLevelTenths = static_cast<int16_t>(r16(p));
    pr.targetCadenceRpm = r8(p);
    pr.onHrLoss = static_cast<HrLossPolicy>(r8(p));
    pr.id[15] = '\0';
    pr.name[23] = '\0';
    pr.initial[3] = '\0';
    return ProfileStore::sanitize(pr);
}

}  // namespace

const Profile* ProfileStore::active() const {
    if (!activeId_[0]) return nullptr;
    const int i = findIndex(activeId_);
    return i >= 0 ? &items_[static_cast<uint8_t>(i)] : nullptr;
}

int ProfileStore::findIndex(const char* id) const {
    if (!id || !id[0]) return -1;
    for (uint8_t i = 0; i < count_; ++i) {
        if (strcmp(items_[i].id, id) == 0) return static_cast<int>(i);
    }
    return -1;
}

bool ProfileStore::sanitize(Profile& p) {
    if (!p.id[0] || !p.name[0]) return false;
    for (char* c = p.id; *c; ++c) {
        if (*c < 33 || *c > 126) return false;
    }
    if (p.maxHr > kHrCeilingMax) p.maxHr = kHrCeilingMax;
    if (p.hrMax > kHrCeilingMax) p.hrMax = kHrCeilingMax;
    if (p.maxPowerW < 0) p.maxPowerW = 0;
    if (p.maxLevelTenths < 0) p.maxLevelTenths = 0;
    return true;
}

bool ProfileStore::put(const Profile& in) {
    Profile p = in;
    if (!sanitize(p)) return false;

    const int i = findIndex(p.id);
    if (i >= 0) {
        items_[static_cast<uint8_t>(i)] = p;
        return true;
    }
    if (count_ >= kMaxProfiles) return false;
    items_[count_++] = p;
    return true;
}

bool ProfileStore::get(const char* id, Profile& out) const {
    const int i = findIndex(id);
    if (i < 0) return false;
    out = items_[static_cast<uint8_t>(i)];
    return true;
}

bool ProfileStore::remove(const char* id) {
    const int i = findIndex(id);
    if (i < 0) return false;
    if (activeId_[0] && strcmp(activeId_, id) == 0) activeId_[0] = '\0';
    for (uint8_t j = static_cast<uint8_t>(i); j + 1 < count_; ++j) items_[j] = items_[j + 1];
    --count_;
    return true;
}

bool ProfileStore::select(const char* id, bool sessionLocked) {
    if (sessionLocked) return false;
    if (!id || !id[0]) {
        activeId_[0] = '\0';
        return true;
    }
    if (findIndex(id) < 0) return false;
    profileCopyId(activeId_, sizeof(activeId_), id);
    return true;
}

void ProfileStore::clearActive() { activeId_[0] = '\0'; }

void ProfileStore::clearAll() {
    count_ = 0;
    activeId_[0] = '\0';
}

void ProfileStore::applyTo(LimiterConfig& lc) const {
    lc.profileMaxLevelTenths = 0;
    lc.profileMaxPowerW = 0;
    const Profile* a = active();
    if (!a) return;
    lc.profileMaxLevelTenths = a->maxLevelTenths;
    lc.profileMaxPowerW = a->maxPowerW;
}

size_t ProfileStore::save(uint8_t* out, size_t cap) const {
    // Header 22 + 68 * count
    const size_t needBytes = 22u + 68u * static_cast<size_t>(count_);
    if (!out || cap < needBytes) return 0;
    uint8_t* p = out;
    w32(p, kMagic);
    w8(p, kVersion);
    w8(p, count_);
    wbytes(p, activeId_, 16);
    for (uint8_t i = 0; i < count_; ++i) writeProfile(p, items_[i]);
    return static_cast<size_t>(p - out);
}

bool ProfileStore::load(const uint8_t* in, size_t len) {
    if (!in || len < 22) return false;
    const uint8_t* p = in;
    const uint8_t* end = in + len;
    if (r32(p) != kMagic) return false;
    if (r8(p) != kVersion) return false;
    const uint8_t n = r8(p);
    if (n > kMaxProfiles) return false;
    char aid[16] = {0};
    rbytes(p, aid, 16);
    aid[15] = '\0';

    Profile tmp[kMaxProfiles];
    for (uint8_t i = 0; i < n; ++i) {
        if (!readProfile(p, end, tmp[i])) return false;
    }
    // Rest darf Padding sein — nicht verlangen, dass p == end.

    clearAll();
    for (uint8_t i = 0; i < n; ++i) items_[count_++] = tmp[i];
    if (aid[0] && findIndex(aid) >= 0) profileCopyId(activeId_, sizeof(activeId_), aid);
    return true;
}

}  // namespace ergo
