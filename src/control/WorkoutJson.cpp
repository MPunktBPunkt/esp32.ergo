#include "control/WorkoutJson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace ergo {

static void setErr(char* err, size_t errLen, const char* msg) {
    if (!err || errLen == 0) return;
    strncpy(err, msg, errLen - 1);
    err[errLen - 1] = 0;
}

static const char* skipWs(const char* p) {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    return p;
}

static bool parseString(const char*& p, char* out, size_t outLen) {
    p = skipWs(p);
    if (*p != '"') return false;
    p++;
    size_t i = 0;
    while (*p && *p != '"') {
        char c = *p++;
        if (c == '\\' && *p) {
            c = *p++;
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
        }
        if (i + 1 < outLen) out[i++] = c;
    }
    if (*p != '"') return false;
    p++;
    out[i] = 0;
    return true;
}

static bool parseNumber(const char*& p, double& v) {
    p = skipWs(p);
    char* end = nullptr;
    v = strtod(p, &end);
    if (end == p) return false;
    p = end;
    return true;
}

static bool skipToKey(const char*& p, const char* key) {
    // Sucht "key" in der aktuellen Objektebene (naive, aber fuer unsere Docs ok).
    const size_t klen = strlen(key);
    while (*p) {
        if (*p == '"') {
            const char* s = p + 1;
            if (strncmp(s, key, klen) == 0 && s[klen] == '"') {
                p = s + klen + 1;
                p = skipWs(p);
                if (*p == ':') {
                    p++;
                    return true;
                }
            }
        }
        p++;
    }
    return false;
}

static const char* findMatching(const char* start, char open, char close) {
    int depth = 0;
    for (const char* p = start; *p; p++) {
        if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) p++;
                p++;
            }
            if (!*p) return nullptr;
            continue;
        }
        if (*p == open) depth++;
        else if (*p == close) {
            depth--;
            if (depth == 0) return p;
        }
    }
    return nullptr;
}

static bool parseStepObject(const char* obj, size_t len, WorkoutStep& st) {
    char buf[512];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, obj, len);
    buf[len] = 0;
    const char* p = buf;
    st = WorkoutStep{};

    const char* save = p;
    if (skipToKey(p, "label")) {
        if (!parseString(p, st.label, sizeof(st.label))) return false;
    } else {
        p = save;
    }

    p = buf;
    if (skipToKey(p, "duration_s")) {
        double v = 0;
        if (!parseNumber(p, v) || v < 1) return false;
        st.durationS = (uint32_t)v;
    } else {
        return false;
    }

    p = buf;
    if (skipToKey(p, "target")) {
        p = skipWs(p);
        if (*p != '{') return false;
        const char* end = findMatching(p, '{', '}');
        if (!end) return false;
        char tbuf[256];
        size_t n = (size_t)(end - p + 1);
        if (n >= sizeof(tbuf)) n = sizeof(tbuf) - 1;
        memcpy(tbuf, p, n);
        tbuf[n] = 0;
        const char* tp = tbuf;
        if (skipToKey(tp, "power")) {
            double v = 0;
            if (parseNumber(tp, v)) st.powerW = (float)v;
        }
        tp = tbuf;
        if (skipToKey(tp, "ftp_pct")) {
            double v = 0;
            if (parseNumber(tp, v)) st.ftpPct = (float)v;
        }
    }

    p = buf;
    if (skipToKey(p, "limit")) {
        p = skipWs(p);
        if (*p == '{') {
            const char* end = findMatching(p, '{', '}');
            if (end) {
                char lbuf[128];
                size_t n = (size_t)(end - p + 1);
                if (n >= sizeof(lbuf)) n = sizeof(lbuf) - 1;
                memcpy(lbuf, p, n);
                lbuf[n] = 0;
                const char* lp = lbuf;
                if (skipToKey(lp, "hr_max")) {
                    double v = 0;
                    if (parseNumber(lp, v)) st.hrMax = (uint8_t)v;
                }
                lp = lbuf;
                if (skipToKey(lp, "hr_soft")) {
                    double v = 0;
                    if (parseNumber(lp, v)) st.hrSoft = (uint8_t)v;
                }
            }
        }
    }

    if (st.powerW <= 0.0f && st.ftpPct <= 0.0f) return false;
    if (st.hrMax > 0 && st.hrSoft == 0)
        st.hrSoft = (st.hrMax > 5) ? (uint8_t)(st.hrMax - 5) : st.hrMax;
    return true;
}

bool workoutParseJson(const char* json, WorkoutDoc& out, char* err, size_t errLen) {
    out = WorkoutDoc{};
    if (!json || !json[0]) {
        setErr(err, errLen, "leer");
        return false;
    }
    const char* p = skipWs(json);
    if (*p != '{') {
        setErr(err, errLen, "Objekt erwartet");
        return false;
    }

    const char* root = p;
    p = root;
    if (skipToKey(p, "id")) {
        if (!parseString(p, out.id, sizeof(out.id))) {
            setErr(err, errLen, "id ungueltig");
            return false;
        }
    }
    p = root;
    if (skipToKey(p, "name")) {
        if (!parseString(p, out.name, sizeof(out.name))) {
            setErr(err, errLen, "name ungueltig");
            return false;
        }
    } else {
        setErr(err, errLen, "name fehlt");
        return false;
    }
    if (!out.id[0]) {
        // id aus name ableiten
        size_t j = 0;
        for (size_t i = 0; out.name[i] && j + 1 < sizeof(out.id); i++) {
            char c = out.name[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
                out.id[j++] = (char)((c >= 'A' && c <= 'Z') ? (c + 32) : c);
            else if (c == ' ' || c == '-' || c == '_')
                out.id[j++] = '_';
        }
        out.id[j] = 0;
        if (!out.id[0]) strncpy(out.id, "workout", sizeof(out.id) - 1);
    }

    p = root;
    if (skipToKey(p, "progression")) {
        p = skipWs(p);
        if (*p == '{') {
            const char* end = findMatching(p, '{', '}');
            if (end) {
                char pbuf[192];
                size_t n = (size_t)(end - p + 1);
                if (n >= sizeof(pbuf)) n = sizeof(pbuf) - 1;
                memcpy(pbuf, p, n);
                pbuf[n] = 0;
                out.progression.enabled = true;
                const char* pp = pbuf;
                if (skipToKey(pp, "step")) {
                    double v = 0;
                    if (parseNumber(pp, v) && v > 0) out.progression.stepS = (uint16_t)v;
                }
                pp = pbuf;
                if (skipToKey(pp, "max")) {
                    double v = 0;
                    if (parseNumber(pp, v) && v > 0) out.progression.maxS = (uint32_t)v;
                }
                pp = pbuf;
                if (skipToKey(pp, "step_index")) {
                    double v = 0;
                    if (parseNumber(pp, v)) out.progression.stepIndex = (uint8_t)v;
                }
            }
        }
    }

    p = root;
    if (!skipToKey(p, "steps")) {
        setErr(err, errLen, "steps fehlt");
        return false;
    }
    p = skipWs(p);
    if (*p != '[') {
        setErr(err, errLen, "steps kein Array");
        return false;
    }
    const char* arrEnd = findMatching(p, '[', ']');
    if (!arrEnd) {
        setErr(err, errLen, "steps ungeschlossen");
        return false;
    }
    p++;  // past [
    while (p < arrEnd && out.stepCount < WorkoutEngine::kMaxSteps) {
        p = skipWs(p);
        if (*p == ']') break;
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p != '{') {
            setErr(err, errLen, "Schritt kein Objekt");
            return false;
        }
        const char* oend = findMatching(p, '{', '}');
        if (!oend || oend > arrEnd) {
            setErr(err, errLen, "Schritt ungeschlossen");
            return false;
        }
        WorkoutStep st;
        if (!parseStepObject(p, (size_t)(oend - p + 1), st)) {
            setErr(err, errLen, "Schritt ungueltig (duration/target)");
            return false;
        }
        out.steps[out.stepCount++] = st;
        p = oend + 1;
    }
    if (out.stepCount == 0) {
        setErr(err, errLen, "keine Schritte");
        return false;
    }
    if (out.progression.enabled) {
        const uint8_t idx = out.progression.stepIndex < out.stepCount
                                ? out.progression.stepIndex
                                : (out.stepCount >= 3 ? (uint8_t)1 : (uint8_t)0);
        out.progression.stepIndex = idx;
        out.progression.baseDurationS = out.steps[idx].durationS;
    }
    return true;
}

size_t workoutWriteJson(const WorkoutDoc& doc, char* buf, size_t bufLen) {
    if (!buf || bufLen < 32) return 0;
    int w;
    if (doc.progression.enabled) {
        w = snprintf(buf, bufLen,
                     "{\"id\":\"%s\",\"name\":\"%s\","
                     "\"progression\":{\"field\":\"duration_s\",\"step\":%u,\"max\":%u},"
                     "\"steps\":[",
                     doc.id, doc.name, (unsigned)doc.progression.stepS,
                     (unsigned)doc.progression.maxS);
    } else {
        w = snprintf(buf, bufLen, "{\"id\":\"%s\",\"name\":\"%s\",\"steps\":[", doc.id, doc.name);
    }
    if (w < 0 || (size_t)w >= bufLen) return 0;
    size_t n = (size_t)w;
    for (uint8_t i = 0; i < doc.stepCount; i++) {
        const WorkoutStep& s = doc.steps[i];
        char step[192];
        if (s.powerW > 0.0f) {
            snprintf(step, sizeof(step),
                     "%s{\"type\":\"steady\",\"duration_s\":%u,\"label\":\"%s\","
                     "\"target\":{\"power\":%.0f},\"limit\":{\"hr_max\":%u,\"hr_soft\":%u}}",
                     i ? "," : "", (unsigned)s.durationS, s.label, s.powerW, (unsigned)s.hrMax,
                     (unsigned)s.hrSoft);
        } else {
            snprintf(step, sizeof(step),
                     "%s{\"type\":\"steady\",\"duration_s\":%u,\"label\":\"%s\","
                     "\"target\":{\"ftp_pct\":%.0f},\"limit\":{\"hr_max\":%u,\"hr_soft\":%u}}",
                     i ? "," : "", (unsigned)s.durationS, s.label, s.ftpPct, (unsigned)s.hrMax,
                     (unsigned)s.hrSoft);
        }
        size_t L = strlen(step);
        if (n + L + 3 > bufLen) return 0;
        memcpy(buf + n, step, L);
        n += L;
    }
    if (n + 3 > bufLen) return 0;
    buf[n++] = ']';
    buf[n++] = '}';
    buf[n] = 0;
    return n;
}

static void fillPhysio(WorkoutDoc& d, float scale) {
    d = WorkoutDoc{};
    strncpy(d.id, "physio", sizeof(d.id) - 1);
    strncpy(d.name, "Physio Grundlage", sizeof(d.name) - 1);
    if (scale < 0.05f) scale = 0.05f;
    WorkoutStep* s = d.steps;
    strncpy(s[0].label, "Einfahren", sizeof(s[0].label) - 1);
    s[0].durationS = (uint32_t)(120 * scale + 0.5f);
    s[0].powerW = 40;
    s[0].hrMax = 120;
    s[0].hrSoft = 115;
    strncpy(s[1].label, "Hauptteil", sizeof(s[1].label) - 1);
    s[1].durationS = (uint32_t)(600 * scale + 0.5f);
    s[1].powerW = 60;
    s[1].hrMax = 120;
    s[1].hrSoft = 115;
    strncpy(s[2].label, "Ausfahren", sizeof(s[2].label) - 1);
    s[2].durationS = (uint32_t)(120 * scale + 0.5f);
    s[2].powerW = 35;
    s[2].hrMax = 120;
    s[2].hrSoft = 115;
    d.stepCount = 3;
    d.progression.enabled = true;
    d.progression.stepS = 60;
    d.progression.maxS = 1800;
    d.progression.stepIndex = 1;
    d.progression.baseDurationS = d.steps[1].durationS;
}

static void fillEasy(WorkoutDoc& d) {
    d = WorkoutDoc{};
    strncpy(d.id, "easy20", sizeof(d.id) - 1);
    strncpy(d.name, "Locker 20 min", sizeof(d.name) - 1);
    strncpy(d.steps[0].label, "Warm", sizeof(d.steps[0].label) - 1);
    d.steps[0].durationS = 300;
    d.steps[0].powerW = 50;
    d.steps[0].hrMax = 130;
    d.steps[0].hrSoft = 125;
    strncpy(d.steps[1].label, "Locker", sizeof(d.steps[1].label) - 1);
    d.steps[1].durationS = 900;
    d.steps[1].powerW = 70;
    d.steps[1].hrMax = 140;
    d.steps[1].hrSoft = 135;
    strncpy(d.steps[2].label, "Cool", sizeof(d.steps[2].label) - 1);
    d.steps[2].durationS = 300;
    d.steps[2].powerW = 45;
    d.steps[2].hrMax = 130;
    d.steps[2].hrSoft = 125;
    d.stepCount = 3;
}

static void fillFtpWarm(WorkoutDoc& d) {
    d = WorkoutDoc{};
    strncpy(d.id, "ftp_warm", sizeof(d.id) - 1);
    strncpy(d.name, "FTP-Warmup", sizeof(d.name) - 1);
    strncpy(d.steps[0].label, "50% FTP", sizeof(d.steps[0].label) - 1);
    d.steps[0].durationS = 300;
    d.steps[0].ftpPct = 50;
    d.steps[0].hrMax = 150;
    d.steps[0].hrSoft = 145;
    strncpy(d.steps[1].label, "60% FTP", sizeof(d.steps[1].label) - 1);
    d.steps[1].durationS = 300;
    d.steps[1].ftpPct = 60;
    d.steps[1].hrMax = 155;
    d.steps[1].hrSoft = 150;
    strncpy(d.steps[2].label, "70% FTP", sizeof(d.steps[2].label) - 1);
    d.steps[2].durationS = 300;
    d.steps[2].ftpPct = 70;
    d.steps[2].hrMax = 160;
    d.steps[2].hrSoft = 155;
    d.stepCount = 3;
}

static void fillRehaKurz(WorkoutDoc& d) {
    d = WorkoutDoc{};
    strncpy(d.id, "reha_kurz", sizeof(d.id) - 1);
    strncpy(d.name, "Reha kurz", sizeof(d.name) - 1);
    strncpy(d.steps[0].label, "Ein", sizeof(d.steps[0].label) - 1);
    d.steps[0].durationS = 60;
    d.steps[0].powerW = 40;
    d.steps[0].hrMax = 120;
    d.steps[0].hrSoft = 115;
    strncpy(d.steps[1].label, "Halt", sizeof(d.steps[1].label) - 1);
    d.steps[1].durationS = 180;
    d.steps[1].powerW = 60;
    d.steps[1].hrMax = 120;
    d.steps[1].hrSoft = 115;
    strncpy(d.steps[2].label, "Aus", sizeof(d.steps[2].label) - 1);
    d.steps[2].durationS = 60;
    d.steps[2].powerW = 35;
    d.steps[2].hrMax = 120;
    d.steps[2].hrSoft = 115;
    d.stepCount = 3;
}

/** Rampe WEBINTERFACE §6: 60 W, +20 W / 60 s, 8 Stufen (Engine-Limit). Abbruch = Stop. */
static void fillTestRamp(WorkoutDoc& d) {
    d = WorkoutDoc{};
    strncpy(d.id, "test_ramp", sizeof(d.id) - 1);
    strncpy(d.name, "Rampe", sizeof(d.name) - 1);
    for (uint8_t i = 0; i < WorkoutEngine::kMaxSteps; i++) {
        const int w = 60 + (int)i * 20;
        snprintf(d.steps[i].label, sizeof(d.steps[i].label), "%d W", w);
        d.steps[i].durationS = 60;
        d.steps[i].powerW = (float)w;
    }
    d.stepCount = WorkoutEngine::kMaxSteps;
}

/** 20-Min-Test: Warmup + 20 min bei 100 % FTP (ERG-Halter; Ø-Leistung → FTP-Vorschlag). */
static void fillTest20(WorkoutDoc& d) {
    d = WorkoutDoc{};
    strncpy(d.id, "test_20min", sizeof(d.id) - 1);
    strncpy(d.name, "20 Minuten", sizeof(d.name) - 1);
    strncpy(d.steps[0].label, "Warm 50%", sizeof(d.steps[0].label) - 1);
    d.steps[0].durationS = 300;
    d.steps[0].ftpPct = 50;
    strncpy(d.steps[1].label, "Warm 70%", sizeof(d.steps[1].label) - 1);
    d.steps[1].durationS = 180;
    d.steps[1].ftpPct = 70;
    strncpy(d.steps[2].label, "Haupt 20 min", sizeof(d.steps[2].label) - 1);
    d.steps[2].durationS = 1200;
    d.steps[2].ftpPct = 100;
    strncpy(d.steps[3].label, "Cool", sizeof(d.steps[3].label) - 1);
    d.steps[3].durationS = 180;
    d.steps[3].ftpPct = 40;
    d.stepCount = 4;
}

/** Recovery-Stub: Belastung + 60 s leicht (volle Erholungsnote braucht spaeter HR-Serie). */
static void fillTestRecovery(WorkoutDoc& d) {
    d = WorkoutDoc{};
    strncpy(d.id, "test_recovery", sizeof(d.id) - 1);
    strncpy(d.name, "Recovery", sizeof(d.name) - 1);
    strncpy(d.steps[0].label, "Belastung", sizeof(d.steps[0].label) - 1);
    d.steps[0].durationS = 180;
    d.steps[0].ftpPct = 90;
    strncpy(d.steps[1].label, "Erholung 60s", sizeof(d.steps[1].label) - 1);
    d.steps[1].durationS = 60;
    d.steps[1].ftpPct = 35;
    d.stepCount = 2;
}

struct Builtin {
    const char* id;
    const char* name;
    void (*fill)(WorkoutDoc&);
};

static void fillPhysio1(WorkoutDoc& d) { fillPhysio(d, 1.0f); }

static const Builtin kBuiltins[] = {
    {"physio", "Physio Grundlage", fillPhysio1},
    {"reha_kurz", "Reha kurz", fillRehaKurz},
    {"easy20", "Locker 20 min", fillEasy},
    {"ftp_warm", "FTP-Warmup", fillFtpWarm},
    {"test_ramp", "Rampe", fillTestRamp},
    {"test_20min", "20 Minuten", fillTest20},
    {"test_recovery", "Recovery", fillTestRecovery},
};

uint8_t workoutBuiltinCount() {
    return (uint8_t)(sizeof(kBuiltins) / sizeof(kBuiltins[0]));
}

const char* workoutBuiltinId(uint8_t index) {
    return index < workoutBuiltinCount() ? kBuiltins[index].id : "";
}

const char* workoutBuiltinName(uint8_t index) {
    return index < workoutBuiltinCount() ? kBuiltins[index].name : "";
}

bool workoutBuiltinById(const char* id, WorkoutDoc& out) {
    if (!id) return false;
    for (uint8_t i = 0; i < workoutBuiltinCount(); i++) {
        if (strcmp(id, kBuiltins[i].id) == 0) {
            kBuiltins[i].fill(out);
            return true;
        }
    }
    if (strcmp(id, "physio") == 0) {
        fillPhysio(out, 1.0f);
        return true;
    }
    return false;
}

}  // namespace ergo
