#include "core/DeviceStore.h"

#include <ctype.h>
#include <stdio.h>

namespace ergo {
namespace {

constexpr uint32_t kMagic = 0x45524456u;  // 'ERDV'
constexpr uint8_t kVersion = 1;
/** mac[18]+name[24]+addr+fmt+trusted+reqCtrl+ceil u16+lastSeen u32 = 52 */
constexpr size_t kRecBytes = 52;

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

}  // namespace

void DeviceStore::clear() {
    for (uint8_t i = 0; i < kMaxDevices; ++i) items_[i] = DeviceProfile{};
    count_ = 0;
    active_ = -1;
}

const DeviceProfile* DeviceStore::active() const {
    if (active_ < 0 || static_cast<uint8_t>(active_) >= count_) return nullptr;
    return &items_[static_cast<uint8_t>(active_)];
}

DeviceProfile* DeviceStore::activeMutable() {
    if (active_ < 0 || static_cast<uint8_t>(active_) >= count_) return nullptr;
    return &items_[static_cast<uint8_t>(active_)];
}

const DeviceProfile* DeviceStore::at(uint8_t i) const {
    return i < count_ ? &items_[i] : nullptr;
}

bool DeviceStore::normalizeMac(const char* in, char* out, size_t outCap) {
    if (!in || !out || outCap < kDeviceMacLen) return false;
    char raw[12];
    size_t n = 0;
    for (const char* p = in; *p && n < sizeof(raw); ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c == ':' || c == '-' || c == ' ') continue;
        if (!isxdigit(c)) return false;
        raw[n++] = static_cast<char>(tolower(c));
    }
    if (n != 12) return false;
    // aa:bb:cc:dd:ee:ff
    for (size_t i = 0; i < 6; ++i) {
        out[i * 3] = raw[i * 2];
        out[i * 3 + 1] = raw[i * 2 + 1];
        out[i * 3 + 2] = (i < 5) ? ':' : '\0';
    }
    return true;
}

bool DeviceStore::macEqual(const char* a, const char* b) {
    char na[kDeviceMacLen], nb[kDeviceMacLen];
    if (!normalizeMac(a, na, sizeof(na)) || !normalizeMac(b, nb, sizeof(nb))) return false;
    return strcmp(na, nb) == 0;
}

int DeviceStore::findIndex(const char* macNorm) const {
    if (!macNorm || !macNorm[0]) return -1;
    for (uint8_t i = 0; i < count_; ++i) {
        if (macEqual(items_[i].mac, macNorm)) return static_cast<int>(i);
    }
    return -1;
}

void DeviceStore::seedVaronDefaults(DeviceProfile& d) {
    // Labor: sint16 wirkt, uint8 nicht; Wattziel untrusted.
    if (d.resistanceFormat == ftms::ResistanceFormat::Unknown)
        d.resistanceFormat = ftms::ResistanceFormat::Sint16;
    if (d.powerTrusted < 0) d.powerTrusted = 0;
    d.requestControlOnReconnect = true;
}

bool DeviceStore::remember(const char* mac, const char* name, uint8_t addrType, uint32_t nowUnix) {
    char norm[kDeviceMacLen];
    if (!normalizeMac(mac, norm, sizeof(norm))) return false;

    int idx = findIndex(norm);
    if (idx < 0) {
        if (count_ >= kMaxDevices) return false;
        idx = count_++;
        items_[static_cast<uint8_t>(idx)] = DeviceProfile{};
        memcpy(items_[static_cast<uint8_t>(idx)].mac, norm, kDeviceMacLen);
        // Varon TC174 / bekannte Labor-MAC: Defaults ohne manuelle Messung.
        if (strcmp(norm, "c2:32:a5:1e:bf:b5") == 0) seedVaronDefaults(items_[static_cast<uint8_t>(idx)]);
    }
    DeviceProfile& d = items_[static_cast<uint8_t>(idx)];
    if (name && name[0]) {
        strncpy(d.name, name, kDeviceNameLen - 1);
        d.name[kDeviceNameLen - 1] = '\0';
    }
    d.addrType = addrType;
    if (nowUnix) d.lastSeenUnix = nowUnix;
    active_ = static_cast<int8_t>(idx);
    return true;
}

bool DeviceStore::select(const char* mac) {
    char norm[kDeviceMacLen];
    if (!normalizeMac(mac, norm, sizeof(norm))) return false;
    const int idx = findIndex(norm);
    if (idx < 0) return false;
    active_ = static_cast<int8_t>(idx);
    return true;
}

bool DeviceStore::remove(const char* mac) {
    char norm[kDeviceMacLen];
    if (!normalizeMac(mac, norm, sizeof(norm))) return false;
    const int idx = findIndex(norm);
    if (idx < 0) return false;
    for (uint8_t i = static_cast<uint8_t>(idx); i + 1 < count_; ++i) items_[i] = items_[i + 1];
    --count_;
    items_[count_] = DeviceProfile{};
    if (active_ == idx) active_ = count_ > 0 ? 0 : -1;
    else if (active_ > idx) --active_;
    return true;
}

void DeviceStore::applyTo(ftms::Capabilities& caps) const {
    const DeviceProfile* d = active();
    if (!d) return;
    if (d->resistanceFormat != ftms::ResistanceFormat::Unknown)
        caps.resistanceFormat = d->resistanceFormat;
    if (d->powerTrusted >= 0) caps.powerTargetTrusted = (d->powerTrusted != 0);
}

size_t DeviceStore::save(uint8_t* buf, size_t cap) const {
    const size_t needBytes = 8 + static_cast<size_t>(count_) * kRecBytes;
    if (!buf || cap < needBytes) return 0;
    uint8_t* p = buf;
    w32(p, kMagic);
    w8(p, kVersion);
    w8(p, count_);
    w8(p, active_ < 0 ? 0xff : static_cast<uint8_t>(active_));
    w8(p, 0);  // pad
    for (uint8_t i = 0; i < count_; ++i) {
        const DeviceProfile& d = items_[i];
        wbytes(p, d.mac, kDeviceMacLen);
        wbytes(p, d.name, kDeviceNameLen);
        w8(p, d.addrType);
        w8(p, static_cast<uint8_t>(d.resistanceFormat));
        w8(p, static_cast<uint8_t>(d.powerTrusted));
        w8(p, d.requestControlOnReconnect ? 1 : 0);
        w16(p, d.measuredCeilingW);
        w32(p, d.lastSeenUnix);
    }
    return static_cast<size_t>(p - buf);
}

bool DeviceStore::load(const uint8_t* buf, size_t len) {
    if (!buf || len < 8) return false;
    const uint8_t* p = buf;
    const uint8_t* end = buf + len;
    if (r32(p) != kMagic) return false;
    const uint8_t ver = r8(p);
    if (ver != kVersion) return false;
    const uint8_t n = r8(p);
    const uint8_t act = r8(p);
    r8(p);  // pad
    if (n > kMaxDevices) return false;
    if (!need(p, end, static_cast<size_t>(n) * kRecBytes)) return false;

    clear();
    for (uint8_t i = 0; i < n; ++i) {
        DeviceProfile d;
        rbytes(p, d.mac, kDeviceMacLen);
        rbytes(p, d.name, kDeviceNameLen);
        d.mac[kDeviceMacLen - 1] = '\0';
        d.name[kDeviceNameLen - 1] = '\0';
        d.addrType = r8(p);
        d.resistanceFormat = static_cast<ftms::ResistanceFormat>(r8(p));
        d.powerTrusted = static_cast<int8_t>(r8(p));
        d.requestControlOnReconnect = r8(p) != 0;
        d.measuredCeilingW = r16(p);
        d.lastSeenUnix = r32(p);
        if (!d.hasMac()) continue;
        items_[count_++] = d;
    }
    if (act == 0xff || act >= count_) active_ = count_ > 0 ? 0 : -1;
    else active_ = static_cast<int8_t>(act);
    return true;
}

}  // namespace ergo
