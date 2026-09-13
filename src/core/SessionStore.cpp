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

bool SessionStore::replaceNewest(const SessionSummary& s) {
    if (count_ == 0 || !s.valid) return false;
    int idx = (int)head_ - 1;
    while (idx < 0) idx += kMax;
    ring_[(uint8_t)idx] = s;
    return true;
}

bool SessionStore::at(uint8_t newestIndex, SessionSummary& out) const {
    if (newestIndex >= count_) return false;
    int idx = (int)head_ - 1 - (int)newestIndex;
    while (idx < 0) idx += kMax;
    out = ring_[(uint8_t)idx];
    return out.valid;
}

static void escapeJsonStr(const char* in, char* out, size_t outLen) {
    if (!out || outLen == 0) return;
    size_t o = 0;
    if (!in) {
        out[0] = 0;
        return;
    }
    for (const char* p = in; *p && o + 1 < outLen; p++) {
        if ((*p == '"' || *p == '\\') && o + 2 < outLen) {
            out[o++] = '\\';
            out[o++] = *p;
        } else if ((unsigned char)*p < 0x20) {
            continue;
        } else {
            out[o++] = *p;
        }
    }
    out[o] = 0;
}

size_t SessionStore::writeJsonLine(const SessionSummary& s, char* buf, size_t bufLen) {
    if (!buf || bufLen < 64) return 0;
    char zones[96];
    zones[0] = 0;
    size_t zp = 0;
    const uint8_t n = s.zoneCount ? s.zoneCount : kPowerZones;
    for (uint8_t i = 0; i < n && i < kPowerZones; i++) {
        int w = snprintf(zones + zp, sizeof(zones) - zp, "%s%u", i ? "," : "",
                         (unsigned)s.zoneTimeS[i]);
        if (w < 0) break;
        zp += (size_t)w;
        if (zp >= sizeof(zones)) break;
    }
    char noteEsc[96];
    escapeJsonStr(s.note, noteEsc, sizeof(noteEsc));
    int m = snprintf(buf, bufLen,
                     "{\"mode\":\"%s\",\"workoutName\":\"%s\",\"workoutId\":\"%s\",\"profileId\":\"%s\","
                     "\"endReason\":\"%s\",\"rpe\":%u,\"note\":\"%s\",\"durationS\":%u,\"pausedS\":%u,"
                     "\"steps\":%u,\"interventions\":%u,\"autoPauses\":%u,"
                     "\"avgPowerW\":%.1f,\"avgDesiredW\":%.1f,\"workKj\":%.1f,"
                     "\"hrAvg\":%u,\"hrMax\":%u,\"leadHr\":%s,\"zoneCount\":%u,"
                     "\"zoneTimeS\":[%s]}",
                     s.mode, s.workoutName, s.workoutId, s.profileId, s.endReason, (unsigned)s.rpe,
                     noteEsc, (unsigned)s.durationS, (unsigned)s.pausedS, (unsigned)s.steps,
                     (unsigned)s.interventions, (unsigned)s.autoPauses, s.avgPowerW, s.avgDesiredW,
                     s.workKj, (unsigned)s.hrAvg, (unsigned)s.hrMax, s.leadHr ? "true" : "false",
                     (unsigned)n, zones);
    if (m < 0 || (size_t)m >= bufLen) return 0;
    return (size_t)m;
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

static bool extractBool(const char* j, const char* key, bool& v) {
    char pat[40];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char* p = strstr(j, pat);
    if (!p) return false;
    p += strlen(pat);
    while (*p == ' ') p++;
    v = (strncmp(p, "true", 4) == 0);
    return true;
}

bool SessionStore::parseJsonLine(const char* line, SessionSummary& out) {
    out.clear();
    if (!line || !line[0]) return false;
    extractStr(line, "mode", out.mode, sizeof(out.mode));
    extractStr(line, "workoutName", out.workoutName, sizeof(out.workoutName));
    extractStr(line, "workoutId", out.workoutId, sizeof(out.workoutId));
    extractStr(line, "profileId", out.profileId, sizeof(out.profileId));
    extractStr(line, "endReason", out.endReason, sizeof(out.endReason));
    extractStr(line, "note", out.note, sizeof(out.note));
    uint32_t u = 0;
    if (extractU32(line, "rpe", u)) out.rpe = (uint8_t)((u > 10) ? 10 : u);
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
    bool b = false;
    if (extractBool(line, "leadHr", b)) out.leadHr = b;
    if (extractU32(line, "zoneCount", u)) out.zoneCount = (uint8_t)u;
    if (out.zoneCount == 0 || out.zoneCount > kPowerZones)
        out.zoneCount = out.leadHr ? kHrZones : kPowerZones;
    const char* zt = strstr(line, "\"zoneTimeS\":[");
    if (zt) {
        zt = strchr(zt, '[');
        if (zt) {
            zt++;
            for (uint8_t i = 0; i < out.zoneCount && i < kPowerZones; i++) {
                while (*zt == ' ' || *zt == ',') zt++;
                if (*zt == ']' || !*zt) break;
                char* endp = nullptr;
                out.zoneTimeS[i] = (uint32_t)strtoul(zt, &endp, 10);
                zt = endp ? endp : zt + 1;
            }
        }
    }
    out.valid = out.mode[0] != 0 || out.durationS > 0;
    return out.valid;
}

}  // namespace ergo
