#include "core/SessionStore.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace ergo {

void SessionStore::clear() {
    head_ = 0;
    count_ = 0;
    for (uint8_t i = 0; i < kMax; i++) ring_[i].clear();
}

bool SessionStore::append(const SessionSummary& s) {
    if (!s.valid) return false;
    ring_[head_] = s;
    head_ = (uint8_t)((head_ + 1) % kMax);
    if (count_ < kMax) count_++;
    return true;
}

bool SessionStore::at(uint8_t newestIndex, SessionSummary& out) const {
    if (newestIndex >= count_) return false;
    // head_ zeigt hinter dem neuesten
    int idx = (int)head_ - 1 - (int)newestIndex;
    while (idx < 0) idx += kMax;
    out = ring_[(uint8_t)idx];
    return out.valid;
}

size_t SessionStore::writeJsonLine(const SessionSummary& s, char* buf, size_t bufLen) {
    if (!buf || bufLen < 32) return 0;
    int n = snprintf(buf, bufLen,
                     "{\"mode\":\"%s\",\"workoutName\":\"%s\",\"profileId\":\"%s\","
                     "\"endReason\":\"%s\",\"durationS\":%u,\"pausedS\":%u,"
                     "\"steps\":%u,\"interventions\":%u,\"autoPauses\":%u,"
                     "\"avgPowerW\":%.1f,\"avgDesiredW\":%.1f,\"workKj\":%.1f,"
                     "\"hrAvg\":%u,\"hrMax\":%u}",
                     s.mode, s.workoutName, s.profileId, s.endReason, (unsigned)s.durationS,
                     (unsigned)s.pausedS, (unsigned)s.steps, (unsigned)s.interventions,
                     (unsigned)s.autoPauses, s.avgPowerW, s.avgDesiredW, s.workKj,
                     (unsigned)s.hrAvg, (unsigned)s.hrMax);
    if (n < 0 || (size_t)n >= bufLen) return 0;
    return (size_t)n;
}

static bool extractStr(const char* j, const char* key, char* out, size_t outLen) {
    char pat[40];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char* p = strstr(j, pat);
    if (!p) {
        out[0] = 0;
        return false;
    }
    p += strlen(pat);
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < outLen) out[i++] = *p++;
    out[i] = 0;
    return true;
}

static bool extractU32(const char* j, const char* key, uint32_t& v) {
    char pat[40];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char* p = strstr(j, pat);
    if (!p) return false;
    p += strlen(pat);
    v = (uint32_t)strtoul(p, nullptr, 10);
    return true;
}

static bool extractF(const char* j, const char* key, float& v) {
    char pat[40];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char* p = strstr(j, pat);
    if (!p) return false;
    p += strlen(pat);
    v = (float)strtod(p, nullptr);
    return true;
}

bool SessionStore::parseJsonLine(const char* line, SessionSummary& out) {
    out.clear();
    if (!line || !line[0]) return false;
    extractStr(line, "mode", out.mode, sizeof(out.mode));
    extractStr(line, "workoutName", out.workoutName, sizeof(out.workoutName));
    extractStr(line, "profileId", out.profileId, sizeof(out.profileId));
    extractStr(line, "endReason", out.endReason, sizeof(out.endReason));
    uint32_t u = 0;
    if (extractU32(line, "durationS", u)) out.durationS = u;
    if (extractU32(line, "pausedS", u)) out.pausedS = u;
    if (extractU32(line, "steps", u)) out.steps = (uint8_t)u;
    if (extractU32(line, "interventions", u)) out.interventions = (uint16_t)u;
    if (extractU32(line, "autoPauses", u)) out.autoPauses = (uint16_t)u;
    extractF(line, "avgPowerW", out.avgPowerW);
    extractF(line, "avgDesiredW", out.avgDesiredW);
    extractF(line, "workKj", out.workKj);
    if (extractU32(line, "hrAvg", u)) out.hrAvg = (uint8_t)u;
    if (extractU32(line, "hrMax", u)) out.hrMax = (uint8_t)u;
    out.valid = out.mode[0] != 0 || out.durationS > 0;
    return out.valid;
}

}  // namespace ergo
