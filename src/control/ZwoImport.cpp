#include "control/ZwoImport.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ergo {
namespace {

bool ieq(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

void setErr(char* err, size_t errLen, const char* msg) {
    if (!err || errLen == 0) return;
    strncpy(err, msg, errLen - 1);
    err[errLen - 1] = 0;
}

const char* findCi(const char* hay, const char* needle) {
    if (!hay || !needle || !*needle) return nullptr;
    const size_t n = strlen(needle);
    for (const char* p = hay; *p; p++) {
        size_t i = 0;
        while (i < n && p[i] &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))
            i++;
        if (i == n) return p;
    }
    return nullptr;
}

bool attrStr(const char* attrs, const char* key, char* out, size_t outLen) {
    if (!attrs || !key || !out || outLen == 0) return false;
    char pat[40];
    snprintf(pat, sizeof(pat), "%s=\"", key);
    const char* p = findCi(attrs, pat);
    if (!p) {
        snprintf(pat, sizeof(pat), "%s='", key);
        p = findCi(attrs, pat);
        if (!p) {
            out[0] = 0;
            return false;
        }
    }
    p += strlen(key) + 2;
    size_t i = 0;
    const char quote = *(p - 1);
    while (*p && *p != quote && i + 1 < outLen) out[i++] = *p++;
    out[i] = 0;
    return i > 0;
}

bool attrF(const char* attrs, const char* key, float& v) {
    char tmp[32];
    if (!attrStr(attrs, key, tmp, sizeof(tmp))) return false;
    v = (float)strtod(tmp, nullptr);
    return true;
}

bool attrU(const char* attrs, const char* key, uint32_t& v) {
    char tmp[24];
    if (!attrStr(attrs, key, tmp, sizeof(tmp))) return false;
    v = (uint32_t)strtoul(tmp, nullptr, 10);
    return true;
}

/** FTP-Fraktion (0.85) oder schon Prozent (>1.5) → Prozent 1..200. */
float fracToPct(float f) {
    if (f <= 0.0f) return 0.0f;
    if (f <= 1.5f) return f * 100.0f;
    if (f > 200.0f) return 200.0f;
    return f;
}

float powerPct(const char* attrs, const char* single, const char* lowKey, const char* highKey) {
    float v = 0.0f;
    if (attrF(attrs, single, v) && v > 0.0f) return fracToPct(v);
    float lo = 0.0f, hi = 0.0f;
    const bool a = attrF(attrs, lowKey, lo);
    const bool b = attrF(attrs, highKey, hi);
    if (a && b && lo > 0.0f && hi > 0.0f) return fracToPct((lo + hi) * 0.5f);
    if (a && lo > 0.0f) return fracToPct(lo);
    if (b && hi > 0.0f) return fracToPct(hi);
    return 0.0f;
}

void slugify(const char* name, char* id, size_t idLen) {
    if (!id || idLen == 0) return;
    size_t o = 0;
    bool sep = false;
    for (const char* p = name ? name : ""; *p && o + 1 < idLen; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            if (sep && o > 0 && o + 1 < idLen) id[o++] = '_';
            sep = false;
            id[o++] = c;
        } else {
            sep = true;
        }
    }
    if (o == 0) {
        strncpy(id, "zwo_import", idLen - 1);
        id[idLen - 1] = 0;
        return;
    }
    id[o] = 0;
}

void extractTagText(const char* xml, const char* tag, char* out, size_t outLen) {
    out[0] = 0;
    char open[24], close[24];
    snprintf(open, sizeof(open), "<%s>", tag);
    snprintf(close, sizeof(close), "</%s>", tag);
    const char* a = findCi(xml, open);
    if (!a) return;
    a += strlen(open);
    const char* b = findCi(a, close);
    if (!b || b <= a) return;
    size_t n = (size_t)(b - a);
    if (n + 1 > outLen) n = outLen - 1;
    // Trim.
    while (n && (*a == ' ' || *a == '\n' || *a == '\r' || *a == '\t')) {
        a++;
        n--;
    }
    while (n && (a[n - 1] == ' ' || a[n - 1] == '\n' || a[n - 1] == '\r' || a[n - 1] == '\t'))
        n--;
    memcpy(out, a, n);
    out[n] = 0;
}

bool append(char* buf, size_t bufLen, size_t& n, const char* chunk) {
    const size_t L = strlen(chunk);
    if (n + L + 1 > bufLen) return false;
    memcpy(buf + n, chunk, L);
    n += L;
    buf[n] = 0;
    return true;
}

bool appendf(char* buf, size_t bufLen, size_t& n, const char* fmt, ...) {
    char tmp[280];
    va_list ap;
    va_start(ap, fmt);
    int w = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (w < 0 || (size_t)w >= sizeof(tmp)) return false;
    return append(buf, bufLen, n, tmp);
}

}  // namespace

void sanitizeJsonStr(char* s) {
    if (!s) return;
    for (char* p = s; *p; p++) {
        if (*p == '"' || *p == '\\' || (unsigned char)*p < 0x20) *p = ' ';
    }
}

size_t zwoToJson(const char* xml, char* buf, size_t bufLen, char* err, size_t errLen) {
    if (!xml || !buf || bufLen < 64) {
        setErr(err, errLen, "Puffer fehlt");
        return 0;
    }
    if (!findCi(xml, "<workout")) {
        setErr(err, errLen, "kein <workout>");
        return 0;
    }

    char name[40] = {};
    extractTagText(xml, "name", name, sizeof(name));
    if (!name[0]) strncpy(name, "ZWO Import", sizeof(name) - 1);
    sanitizeJsonStr(name);
    char id[24] = {};
    slugify(name, id, sizeof(id));

    size_t n = 0;
    if (!appendf(buf, bufLen, n,
                 "{\"id\":\"%s\",\"name\":\"%s\",\"goal\":\"\",\"favorite\":false,\"steps\":[", id,
                 name)) {
        setErr(err, errLen, "JSON zu klein");
        return 0;
    }

    const char* p = findCi(xml, "<workout");
    if (!p) {
        setErr(err, errLen, "kein <workout>");
        return 0;
    }
    p = strchr(p, '>');
    if (!p) {
        setErr(err, errLen, "workout kaputt");
        return 0;
    }
    p++;
    const char* end = findCi(p, "</workout>");
    if (!end) end = p + strlen(p);

    uint8_t editorRows = 0;
    uint8_t flatLeft = WorkoutEngine::kMaxSteps;
    uint8_t stepN = 0;

    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
        if (p >= end || *p != '<') break;
        if (p[1] == '/' || p[1] == '!' || p[1] == '?') {
            const char* gt = strchr(p, '>');
            if (!gt || gt >= end) break;
            p = gt + 1;
            continue;
        }

        const char* tagStart = p + 1;
        const char* tagEnd = tagStart;
        while (tagEnd < end && (isalnum((unsigned char)*tagEnd) || *tagEnd == '_')) tagEnd++;
        char tag[24];
        size_t tlen = (size_t)(tagEnd - tagStart);
        if (tlen == 0 || tlen >= sizeof(tag)) {
            setErr(err, errLen, "Tag ungueltig");
            return 0;
        }
        memcpy(tag, tagStart, tlen);
        tag[tlen] = 0;

        const char* gt = strchr(tagEnd, '>');
        if (!gt || gt >= end) {
            setErr(err, errLen, "Tag nicht geschlossen");
            return 0;
        }
        char attrs[384];
        size_t alen = (size_t)(gt - tagEnd);
        if (alen >= sizeof(attrs)) alen = sizeof(attrs) - 1;
        memcpy(attrs, tagEnd, alen);
        attrs[alen] = 0;
        // Strip trailing '/' for self-closing.
        for (int i = (int)alen - 1; i >= 0; i--) {
            if (attrs[i] == ' ' || attrs[i] == '\t' || attrs[i] == '/')
                attrs[i] = 0;
            else
                break;
        }

        const bool selfClose = (gt > tagStart && *(gt - 1) == '/');
        p = gt + 1;
        if (!selfClose) {
            char close[28];
            snprintf(close, sizeof(close), "</%s>", tag);
            const char* c = findCi(p, close);
            if (c && c < end) p = c + strlen(close);
        }

        // Cosmetik / unbekannt: ueberspringen.
        if (ieq(tag, "textevent") || ieq(tag, "author") || ieq(tag, "description") ||
            ieq(tag, "tags") || ieq(tag, "tag") || ieq(tag, "sportType")) {
            continue;
        }

        if (editorRows >= 8) {
            setErr(err, errLen, "zu viele Bloecke (>8)");
            return 0;
        }

        if (ieq(tag, "SteadyState")) {
            uint32_t dur = 0;
            if (!attrU(attrs, "Duration", dur) || dur < 1) {
                setErr(err, errLen, "SteadyState ohne Duration");
                return 0;
            }
            float pct = powerPct(attrs, "Power", "PowerLow", "PowerHigh");
            if (pct <= 0.0f) {
                setErr(err, errLen, "SteadyState ohne Power");
                return 0;
            }
            if (flatLeft < 1) {
                setErr(err, errLen, "zu viele Schritte");
                return 0;
            }
            if (!appendf(buf, bufLen, n,
                         "%s{\"type\":\"steady\",\"duration_s\":%u,\"label\":\"Steady\","
                         "\"target\":{\"ftp_pct\":%.0f}}",
                         stepN ? "," : "", (unsigned)dur, pct)) {
                setErr(err, errLen, "JSON zu klein");
                return 0;
            }
            editorRows++;
            flatLeft--;
            stepN++;
        } else if (ieq(tag, "FreeRide") || ieq(tag, "Freeride") || ieq(tag, "MaxEffort")) {
            uint32_t dur = 0;
            if (!attrU(attrs, "Duration", dur) || dur < 1) {
                setErr(err, errLen, "FreeRide ohne Duration");
                return 0;
            }
            if (flatLeft < 1) {
                setErr(err, errLen, "zu viele Schritte");
                return 0;
            }
            if (!appendf(buf, bufLen, n,
                         "%s{\"type\":\"steady\",\"duration_s\":%u,\"label\":\"Free\","
                         "\"target\":{\"self_paced\":true}}",
                         stepN ? "," : "", (unsigned)dur)) {
                setErr(err, errLen, "JSON zu klein");
                return 0;
            }
            editorRows++;
            flatLeft--;
            stepN++;
        } else if (ieq(tag, "Warmup") || ieq(tag, "Cooldown") || ieq(tag, "Ramp")) {
            uint32_t dur = 0;
            if (!attrU(attrs, "Duration", dur) || dur < 1) {
                setErr(err, errLen, "Ramp ohne Duration");
                return 0;
            }
            float lo = 0.0f, hi = 0.0f;
            attrF(attrs, "PowerLow", lo);
            attrF(attrs, "PowerHigh", hi);
            float single = 0.0f;
            if (attrF(attrs, "Power", single) && single > 0.0f && lo <= 0.0f && hi <= 0.0f) {
                lo = single;
                hi = single;
            }
            lo = fracToPct(lo);
            hi = fracToPct(hi);
            if (lo <= 0.0f && hi <= 0.0f) {
                setErr(err, errLen, "Ramp ohne Power");
                return 0;
            }
            if (lo <= 0.0f) lo = hi;
            if (hi <= 0.0f) hi = lo;
            float from = lo, to = hi;
            if (ieq(tag, "Cooldown")) {
                from = hi;
                to = lo;
            }
            // Zwei Steadies statt type:ramp — sonst fressen lange Warmups das 16er-Budget.
            if (flatLeft < 2 || editorRows + 2 > 8) {
                setErr(err, errLen, "zu viele Schritte (Ramp)");
                return 0;
            }
            const char* label = "Ramp";
            if (ieq(tag, "Warmup")) label = "Warmup";
            else if (ieq(tag, "Cooldown")) label = "Cooldown";
            uint32_t d1 = dur / 2;
            uint32_t d2 = dur - d1;
            if (d1 < 1) d1 = 1;
            if (d2 < 1) d2 = 1;
            if (!appendf(buf, bufLen, n,
                         "%s{\"type\":\"steady\",\"duration_s\":%u,\"label\":\"%s\","
                         "\"target\":{\"ftp_pct\":%.0f}},"
                         "{\"type\":\"steady\",\"duration_s\":%u,\"label\":\"%s\","
                         "\"target\":{\"ftp_pct\":%.0f}}",
                         stepN ? "," : "", (unsigned)d1, label, from, (unsigned)d2, label, to)) {
                setErr(err, errLen, "JSON zu klein");
                return 0;
            }
            editorRows = (uint8_t)(editorRows + 2);
            flatLeft = (uint8_t)(flatLeft - 2);
            stepN = (uint8_t)(stepN + 2);
        } else if (ieq(tag, "IntervalsT")) {
            uint32_t rep = 0, onD = 0, offD = 0;
            if (!attrU(attrs, "Repeat", rep) || rep < 1) {
                setErr(err, errLen, "IntervalsT ohne Repeat");
                return 0;
            }
            if (!attrU(attrs, "OnDuration", onD) || onD < 1) {
                setErr(err, errLen, "IntervalsT ohne OnDuration");
                return 0;
            }
            if (!attrU(attrs, "OffDuration", offD) || offD < 1) {
                setErr(err, errLen, "IntervalsT ohne OffDuration");
                return 0;
            }
            float onP = powerPct(attrs, "OnPower", "PowerOnLow", "PowerOnHigh");
            float offP = powerPct(attrs, "OffPower", "PowerOffLow", "PowerOffHigh");
            if (onP <= 0.0f) onP = powerPct(attrs, "Power", "PowerLow", "PowerHigh");
            if (onP <= 0.0f || offP <= 0.0f) {
                setErr(err, errLen, "IntervalsT ohne Power");
                return 0;
            }
            if (rep > 16) {
                setErr(err, errLen, "IntervalsT Repeat zu gross");
                return 0;
            }
            const uint16_t need = (uint16_t)(rep * 2);
            if (need > flatLeft) {
                setErr(err, errLen, "zu viele Schritte (Interval)");
                return 0;
            }
            if (!appendf(buf, bufLen, n,
                         "%s{\"type\":\"interval\",\"repeat\":%u,\"label\":\"Interval\","
                         "\"steps\":["
                         "{\"type\":\"steady\",\"duration_s\":%u,\"label\":\"Work\","
                         "\"target\":{\"ftp_pct\":%.0f}},"
                         "{\"type\":\"steady\",\"duration_s\":%u,\"label\":\"Rest\","
                         "\"target\":{\"ftp_pct\":%.0f}}"
                         "]}",
                         stepN ? "," : "", (unsigned)rep, (unsigned)onD, onP, (unsigned)offD,
                         offP)) {
                setErr(err, errLen, "JSON zu klein");
                return 0;
            }
            editorRows++;
            flatLeft = (uint8_t)(flatLeft - need);
            stepN++;
        } else {
            // Unbekannte Bloecke (z. B. SolidState) stillschweigend ueberspringen.
            continue;
        }
    }

    if (stepN == 0) {
        setErr(err, errLen, "keine unterstuetzten Bloecke");
        return 0;
    }
    if (!append(buf, bufLen, n, "]}")) {
        setErr(err, errLen, "JSON zu klein");
        return 0;
    }
    if (err && errLen) err[0] = 0;
    return n;
}

}  // namespace ergo
