#include "DebugRing.h"

namespace ergo {

static const char kHex[] = "0123456789ABCDEF";

/** Ohne stdio: `snprintf` zieht auf dem ESP32 die Float-Formatierung mit in
 *  den Flash, und hier werden nur Ganzzahlen und Hexbytes gebraucht. */
static size_t putStr(char* out, size_t cap, size_t at, const char* s) {
    while (*s) {
        if (at + 1 >= cap) return 0;
        out[at++] = *s++;
    }
    return at;
}

static size_t putU32(char* out, size_t cap, size_t at, uint32_t v) {
    char tmp[11];
    uint8_t n = 0;
    do {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    } while (v && n < sizeof(tmp));
    while (n) {
        if (at + 1 >= cap) return 0;
        out[at++] = tmp[--n];
    }
    return at;
}

static size_t putHex16(char* out, size_t cap, size_t at, uint16_t v) {
    if (at + 4 >= cap) return 0;
    out[at++] = kHex[(v >> 12) & 0xF];
    out[at++] = kHex[(v >> 8) & 0xF];
    out[at++] = kHex[(v >> 4) & 0xF];
    out[at++] = kHex[v & 0xF];
    return at;
}

void DebugRing::setEnabled(bool on) { enabled_ = on; }

void DebugRing::setIbdEvery(uint16_t n) { ibdEvery_ = n; }

void DebugRing::clear() {
    head_ = 0;
    count_ = 0;
    seen_ = 0;
    thinned_ = 0;
    overwritten_ = 0;
    truncated_ = 0;
    ibdSeen_ = 0;
    lastFlags_ = 0;
    haveFlags_ = false;
}

void DebugRing::add(uint16_t uuid, Dir dir, const uint8_t* data, size_t len, uint32_t nowMs) {
    seen_++;
    if (!enabled_ || !data || len == 0) return;

    if (uuid == 0x2AD2 && dir == Dir::Notify) {
        ibdSeen_++;
        const uint16_t flags = (len >= 2) ? (uint16_t)(data[0] | ((uint16_t)data[1] << 8)) : 0;
        const bool newLayout = !haveFlags_ || flags != lastFlags_;
        if (!newLayout && ibdEvery_ > 1 && (ibdSeen_ % ibdEvery_) != 0) {
            thinned_++;
            return;
        }
        lastFlags_ = flags;
        haveFlags_ = true;
    }

    if (count_ == kRingSlots) overwritten_++;

    Rec& r = slots_[head_];
    r.atMs = nowMs;
    r.uuid = uuid;
    r.dir = dir;
    size_t n = len;
    if (n > kRingPayload) {
        n = kRingPayload;
        truncated_++;
    }
    r.len = (uint8_t)n;
    for (size_t i = 0; i < n; i++) r.data[i] = data[i];
    for (size_t i = n; i < kRingPayload; i++) r.data[i] = 0;

    head_ = (uint16_t)((head_ + 1) % kRingSlots);
    if (count_ < kRingSlots) count_++;
}

const DebugRing::Rec* DebugRing::at(uint16_t i) const {
    if (i >= count_) return nullptr;
    // Bei vollem Ring beginnt der aelteste Datensatz auf `head_`.
    const uint16_t start = (count_ == kRingSlots) ? head_ : 0;
    return &slots_[(uint16_t)((start + i) % kRingSlots)];
}

size_t DebugRing::formatLine(const Rec& r, char* out, size_t cap) {
    if (!out || cap == 0) return 0;
    size_t at = 0;
    // Feldnamen und Reihenfolge wie in bike-data.jsonl der Sonde —
    // tools/make-fixtures.py liest uuid, dir und hex.
    at = putStr(out, cap, at, "{\"t\":");
    if (!at) return 0;
    at = putU32(out, cap, at, r.atMs);
    if (!at) return 0;
    at = putStr(out, cap, at, ",\"uuid\":\"");
    if (!at) return 0;
    at = putHex16(out, cap, at, r.uuid);
    if (!at) return 0;
    at = putStr(out, cap, at, "\",\"dir\":\"");
    if (!at) return 0;
    at = putStr(out, cap, at, r.dir == Dir::Write ? "write" : "notify");
    if (!at) return 0;
    at = putStr(out, cap, at, "\",\"hex\":\"");
    if (!at) return 0;
    for (uint8_t i = 0; i < r.len; i++) {
        if (at + 2 >= cap) return 0;
        out[at++] = kHex[(r.data[i] >> 4) & 0xF];
        out[at++] = kHex[r.data[i] & 0xF];
    }
    at = putStr(out, cap, at, "\"}");
    if (!at) return 0;
    out[at] = '\0';
    return at;
}

}  // namespace ergo
