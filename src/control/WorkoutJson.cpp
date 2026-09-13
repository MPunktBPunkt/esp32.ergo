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
    if (skipToKey(p, "self_paced")) {
        p = skipWs(p);
        if (strncmp(p, "true", 4) == 0) st.selfPaced = true;
        else if (*p == '1') st.selfPaced = true;
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
        if (skipToKey(tp, "self_paced")) {
            tp = skipWs(tp);
            if (strncmp(tp, "true", 4) == 0 || *tp == '1') st.selfPaced = true;
        }
        tp = tbuf;
        if (skipToKey(tp, "open")) {
            tp = skipWs(tp);
            if (strncmp(tp, "true", 4) == 0 || *tp == '1') st.selfPaced = true;
        }
        tp = tbuf;
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

    if (!st.selfPaced && st.powerW <= 0.0f && st.ftpPct <= 0.0f) return false;
    if (st.selfPaced) {
        st.powerW = 0.0f;
        st.ftpPct = 0.0f;
    }
    if (st.hrMax > 0 && st.hrSoft == 0)
        st.hrSoft = (st.hrMax > 5) ? (uint8_t)(st.hrMax - 5) : st.hrMax;
    return true;
}

static bool objectTypeIs(const char* obj, size_t len, const char* type) {
    if (!obj || !type || len < 8) return false;
    // Eigenes Suchfenster: skipToKey braucht NUL-terminierten Puffer.
    char buf[384];
    size_t n = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
    memcpy(buf, obj, n);
    buf[n] = 0;
    const char* p = buf;
    if (!skipToKey(p, "type")) return false;
    p = skipWs(p);
    if (*p != '"') return false;
    p++;
    const size_t tlen = strlen(type);
    return strncmp(p, type, tlen) == 0 && p[tlen] == '"';
}

/** Intervallblock → flache Steady-Schritte (Work/Rest × repeat). */
static bool expandIntervalObject(const char* obj, size_t len, WorkoutDoc& out, char* err,
                                 size_t errLen) {
    char buf[1024];
    if (len >= sizeof(buf)) {
        setErr(err, errLen, "Intervall zu gross");
        return false;
    }
    memcpy(buf, obj, len);
    buf[len] = 0;

    unsigned repeat = 1;
    const char* p = buf;
    if (skipToKey(p, "repeat")) {
        double v = 0;
        if (parseNumber(p, v) && v >= 1) repeat = (unsigned)v;
    }
    if (repeat < 1) repeat = 1;
    if (repeat > 16) {
        setErr(err, errLen, "repeat > 16");
        return false;
    }

    char blockLabel[24] = {};
    p = buf;
    if (skipToKey(p, "label")) parseString(p, blockLabel, sizeof(blockLabel));

    p = buf;
    if (!skipToKey(p, "steps")) {
        setErr(err, errLen, "Intervall ohne steps");
        return false;
    }
    p = skipWs(p);
    if (*p != '[') {
        setErr(err, errLen, "Intervall-steps kein Array");
        return false;
    }
    const char* arrEnd = findMatching(p, '[', ']');
    if (!arrEnd) {
        setErr(err, errLen, "Intervall-steps ungeschlossen");
        return false;
    }

    WorkoutStep kids[4];
    uint8_t kidCount = 0;
    p++;
    while (p < arrEnd && kidCount < 4) {
        p = skipWs(p);
        if (*p == ']') break;
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p != '{') {
            setErr(err, errLen, "Intervall-Kind kein Objekt");
            return false;
        }
        const char* oend = findMatching(p, '{', '}');
        if (!oend || oend > arrEnd) {
            setErr(err, errLen, "Intervall-Kind ungeschlossen");
            return false;
        }
        if (objectTypeIs(p, (size_t)(oend - p + 1), "interval")) {
            setErr(err, errLen, "verschachtelte Intervalle");
            return false;
        }
        if (!parseStepObject(p, (size_t)(oend - p + 1), kids[kidCount])) {
            setErr(err, errLen, "Intervall-Kind ungueltig");
            return false;
        }
        kidCount++;
        p = oend + 1;
    }
    if (kidCount == 0) {
        setErr(err, errLen, "Intervall leer");
        return false;
    }

    const unsigned need = repeat * (unsigned)kidCount;
    if ((unsigned)out.stepCount + need > WorkoutEngine::kMaxSteps) {
        setErr(err, errLen, "Intervall sprengt max. Schritte");
        return false;
    }

    for (unsigned r = 0; r < repeat; r++) {
        for (uint8_t k = 0; k < kidCount; k++) {
            WorkoutStep st = kids[k];
            char lab[24];
            const char* base = kids[k].label[0] ? kids[k].label : "I";
            char shortBase[16];
            strncpy(shortBase, base, sizeof(shortBase) - 1);
            shortBase[sizeof(shortBase) - 1] = 0;
            snprintf(lab, sizeof(lab), "%s*%u", shortBase, r + 1);
            strncpy(st.label, lab, sizeof(st.label) - 1);
            st.label[sizeof(st.label) - 1] = 0;
            out.steps[out.stepCount++] = st;
        }
    }
    return true;
}

/** Rampe from→to über duration_s → mehrere Steady-Scheiben. */
static bool expandRampObject(const char* obj, size_t len, WorkoutDoc& out, char* err, size_t errLen) {
    char buf[768];
    if (len >= sizeof(buf)) {
        setErr(err, errLen, "Rampe zu gross");
        return false;
    }
    memcpy(buf, obj, len);
    buf[len] = 0;

    uint32_t durationS = 0;
    const char* p = buf;
    if (skipToKey(p, "duration_s")) {
        double v = 0;
        if (!parseNumber(p, v) || v < 1) {
            setErr(err, errLen, "Rampe ohne duration_s");
            return false;
        }
        durationS = (uint32_t)v;
    } else {
        setErr(err, errLen, "Rampe ohne duration_s");
        return false;
    }

    char label[24] = {};
    p = buf;
    if (skipToKey(p, "label")) parseString(p, label, sizeof(label));

    float fromW = 0, toW = 0, fromPct = 0, toPct = 0;
    bool usePct = false;
    p = buf;
    if (skipToKey(p, "target")) {
        p = skipWs(p);
        if (*p == '{') {
            const char* end = findMatching(p, '{', '}');
            if (end) {
                char tbuf[256];
                size_t n = (size_t)(end - p + 1);
                if (n >= sizeof(tbuf)) n = sizeof(tbuf) - 1;
                memcpy(tbuf, p, n);
                tbuf[n] = 0;
                const char* tp = tbuf;
                if (skipToKey(tp, "ftp_pct_from")) {
                    double v = 0;
                    if (parseNumber(tp, v)) {
                        fromPct = (float)v;
                        usePct = true;
                    }
                }
                tp = tbuf;
                if (skipToKey(tp, "ftp_pct_to")) {
                    double v = 0;
                    if (parseNumber(tp, v)) {
                        toPct = (float)v;
                        usePct = true;
                    }
                }
                tp = tbuf;
                if (skipToKey(tp, "power_from")) {
                    double v = 0;
                    if (parseNumber(tp, v)) fromW = (float)v;
                }
                tp = tbuf;
                if (skipToKey(tp, "power_to")) {
                    double v = 0;
                    if (parseNumber(tp, v)) toW = (float)v;
                }
                // Kurzform: power / ftp_pct als Start, power_to / ftp_pct_to als Ende
                tp = tbuf;
                if (!usePct && fromW <= 0.0f && skipToKey(tp, "power")) {
                    double v = 0;
                    if (parseNumber(tp, v)) fromW = (float)v;
                }
                tp = tbuf;
                if (!usePct && fromPct <= 0.0f && skipToKey(tp, "ftp_pct")) {
                    double v = 0;
                    if (parseNumber(tp, v)) {
                        fromPct = (float)v;
                        usePct = true;
                    }
                }
            }
        }
    }
    if (usePct) {
        if (toPct <= 0.0f) toPct = fromPct;
        if (fromPct <= 0.0f) {
            setErr(err, errLen, "Rampe ftp_pct ungueltig");
            return false;
        }
    } else {
        if (toW <= 0.0f) toW = fromW;
        if (fromW <= 0.0f) {
            setErr(err, errLen, "Rampe power ungueltig");
            return false;
        }
    }

    uint8_t hrMax = 0, hrSoft = 0;
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
                    if (parseNumber(lp, v)) hrMax = (uint8_t)v;
                }
                lp = lbuf;
                if (skipToKey(lp, "hr_soft")) {
                    double v = 0;
                    if (parseNumber(lp, v)) hrSoft = (uint8_t)v;
                }
            }
        }
    }

    // ~30 s pro Scheibe, mind. 2, max. 10, und Restplatz im Doc.
    unsigned slices = durationS / 30U;
    if (slices < 2) slices = 2;
    if (slices > 10) slices = 10;
    const unsigned room = WorkoutEngine::kMaxSteps - out.stepCount;
    if (room < 2) {
        setErr(err, errLen, "Rampe sprengt max. Schritte");
        return false;
    }
    if (slices > room) slices = room;

    const uint32_t base = durationS / slices;
    uint32_t rem = durationS - base * slices;
    for (unsigned i = 0; i < slices; i++) {
        WorkoutStep st;
        st.durationS = base + (i < rem ? 1U : 0U);
        if (st.durationS < 1) st.durationS = 1;
        const float t = slices <= 1 ? 0.0f : (float)i / (float)(slices - 1);
        if (usePct) {
            st.ftpPct = fromPct + (toPct - fromPct) * t;
        } else {
            st.powerW = fromW + (toW - fromW) * t;
        }
        st.hrMax = hrMax;
        st.hrSoft = hrSoft;
        if (st.hrMax > 0 && st.hrSoft == 0)
            st.hrSoft = (st.hrMax > 5) ? (uint8_t)(st.hrMax - 5) : st.hrMax;
        char lab[24];
        const char* baseLab = label[0] ? label : "Rampe";
        char shortLab[14];
        strncpy(shortLab, baseLab, sizeof(shortLab) - 1);
        shortLab[sizeof(shortLab) - 1] = 0;
        snprintf(lab, sizeof(lab), "%s*%u", shortLab, i + 1);
        strncpy(st.label, lab, sizeof(st.label) - 1);
        out.steps[out.stepCount++] = st;
    }
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
    p = root;
    if (skipToKey(p, "goal")) {
        if (!parseString(p, out.goal, sizeof(out.goal))) {
            setErr(err, errLen, "goal ungueltig");
            return false;
        }
    }
    p = root;
    if (skipToKey(p, "favorite")) {
        p = skipWs(p);
        out.favorite = (strncmp(p, "true", 4) == 0 || *p == '1');
    }
    p = root;
    if (skipToKey(p, "autoPauseS")) {
        double v = 0;
        if (parseNumber(p, v) && v > 0) {
            if (v > 600) v = 600;
            out.autoPauseS = (uint16_t)v;
        }
    }
    p = root;
    if (skipToKey(p, "tags")) {
        p = skipWs(p);
        if (*p == '[') {
            const char* end = findMatching(p, '[', ']');
            if (end) {
                p++;
                while (p < end && out.tagCount < WorkoutDoc::kMaxTags) {
                    p = skipWs(p);
                    if (*p == ']') break;
                    if (*p == ',') {
                        p++;
                        continue;
                    }
                    if (!parseString(p, out.tags[out.tagCount], WorkoutDoc::kTagLen)) break;
                    if (out.tags[out.tagCount][0]) out.tagCount++;
                }
            }
        }
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
        const size_t olen = (size_t)(oend - p + 1);
        if (objectTypeIs(p, olen, "interval")) {
            if (!expandIntervalObject(p, olen, out, err, errLen)) return false;
        } else if (objectTypeIs(p, olen, "ramp")) {
            if (!expandRampObject(p, olen, out, err, errLen)) return false;
        } else {
            WorkoutStep st;
            if (!parseStepObject(p, olen, st)) {
                setErr(err, errLen, "Schritt ungueltig (duration/target)");
                return false;
            }
            if (out.stepCount >= WorkoutEngine::kMaxSteps) {
                setErr(err, errLen, "zu viele Schritte");
                return false;
            }
            out.steps[out.stepCount++] = st;
        }
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
    const char* fav = doc.favorite ? "true" : "false";
    char tagsPart[128] = {};
    if (doc.tagCount > 0) {
        size_t tn = 0;
        tagsPart[tn++] = ',';
        tn += (size_t)snprintf(tagsPart + tn, sizeof(tagsPart) - tn, "\"tags\":[");
        for (uint8_t i = 0; i < doc.tagCount && tn + 24 < sizeof(tagsPart); i++) {
            tn += (size_t)snprintf(tagsPart + tn, sizeof(tagsPart) - tn, "%s\"%s\"", i ? "," : "",
                                   doc.tags[i]);
        }
        if (tn + 2 < sizeof(tagsPart)) {
            tagsPart[tn++] = ']';
            tagsPart[tn] = 0;
        } else {
            tagsPart[0] = 0;
        }
    }
    char pausePart[32] = {};
    if (doc.autoPauseS > 0) {
        snprintf(pausePart, sizeof(pausePart), ",\"autoPauseS\":%u", (unsigned)doc.autoPauseS);
    }
    if (doc.progression.enabled) {
        w = snprintf(buf, bufLen,
                     "{\"id\":\"%s\",\"name\":\"%s\",\"goal\":\"%s\",\"favorite\":%s%s%s,"
                     "\"progression\":{\"field\":\"duration_s\",\"step\":%u,\"max\":%u},"
                     "\"steps\":[",
                     doc.id, doc.name, doc.goal, fav, tagsPart, pausePart,
                     (unsigned)doc.progression.stepS, (unsigned)doc.progression.maxS);
    } else {
        w = snprintf(buf, bufLen,
                     "{\"id\":\"%s\",\"name\":\"%s\",\"goal\":\"%s\",\"favorite\":%s%s%s,\"steps\":[",
                     doc.id, doc.name, doc.goal, fav, tagsPart, pausePart);
    }
    if (w < 0 || (size_t)w >= bufLen) return 0;
    size_t n = (size_t)w;
    for (uint8_t i = 0; i < doc.stepCount; i++) {
        const WorkoutStep& s = doc.steps[i];
        char step[220];
        if (s.selfPaced) {
            snprintf(step, sizeof(step),
                     "%s{\"type\":\"steady\",\"duration_s\":%u,\"label\":\"%s\","
                     "\"target\":{\"self_paced\":true},\"limit\":{\"hr_max\":%u,\"hr_soft\":%u}}",
                     i ? "," : "", (unsigned)s.durationS, s.label, (unsigned)s.hrMax,
                     (unsigned)s.hrSoft);
        } else if (s.powerW > 0.0f) {
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

/** Rampe WEBINTERFACE §6: 60 W, +20 W / 60 s, 16 Stufen (60…360 W). Abbruch = Stop. */
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

/** 20-Min-Test: Warmup ERG, Haupt self-paced (Stufe), Cool ERG. */
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
    d.steps[2].selfPaced = true;
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

static bool addFtpStep(WorkoutDoc& d, const char* label, uint32_t durationS, float ftpPct) {
    if (d.stepCount >= WorkoutEngine::kMaxSteps) return false;
    WorkoutStep& s = d.steps[d.stepCount++];
    s = WorkoutStep{};
    strncpy(s.label, label, sizeof(s.label) - 1);
    s.durationS = durationS;
    s.ftpPct = ftpPct;
    return true;
}

/** Warm 5′@55 + 3′@75, n×(Arbeit/Pause), Cool — Pause nach dem letzten Intervall entfällt. */
static void fillFtpRepeats(WorkoutDoc& d, const char* id, const char* name, uint8_t reps,
                           uint32_t workS, float workPct, uint32_t restS, float restPct,
                           uint32_t coolS, float coolPct) {
    d = WorkoutDoc{};
    strncpy(d.id, id, sizeof(d.id) - 1);
    strncpy(d.name, name, sizeof(d.name) - 1);
    addFtpStep(d, "Warm 55%", 300, 55);
    addFtpStep(d, "Warm 75%", 180, 75);
    for (uint8_t i = 0; i < reps; i++) {
        char lab[24];
        snprintf(lab, sizeof(lab), "Work %u", (unsigned)(i + 1));
        addFtpStep(d, lab, workS, workPct);
        if (i + 1 < reps) {
            snprintf(lab, sizeof(lab), "Rest %u", (unsigned)(i + 1));
            addFtpStep(d, lab, restS, restPct);
        }
    }
    addFtpStep(d, "Cool", coolS, coolPct);
}

static void fillSs3x12(WorkoutDoc& d) {
    fillFtpRepeats(d, "ss_3x12", "Sweet Spot 3x12", 3, 720, 90, 300, 50, 300, 45);
}
static void fillSs2x20(WorkoutDoc& d) {
    fillFtpRepeats(d, "ss_2x20", "Sweet Spot 2x20", 2, 1200, 92, 480, 50, 300, 45);
}
static void fillTh4x8(WorkoutDoc& d) {
    fillFtpRepeats(d, "th_4x8", "Threshold 4x8", 4, 480, 102, 240, 50, 300, 45);
}
static void fillFtp2x20(WorkoutDoc& d) {
    fillFtpRepeats(d, "ftp_2x20", "FTP 2x20", 2, 1200, 98, 540, 50, 300, 45);
}
/** Over/Under: 6×(2′ @95 / 1′ @105) — Schwelle ständig kreuzen. */
static void fillOverUnder(WorkoutDoc& d) {
    d = WorkoutDoc{};
    strncpy(d.id, "over_under", sizeof(d.id) - 1);
    strncpy(d.name, "Over/Under 6x(2+1)", sizeof(d.name) - 1);
    addFtpStep(d, "Warm 55%", 300, 55);
    addFtpStep(d, "Warm 75%", 180, 75);
    for (uint8_t i = 0; i < 6; i++) {
        char lab[24];
        snprintf(lab, sizeof(lab), "Under %u", (unsigned)(i + 1));
        addFtpStep(d, lab, 120, 95);
        snprintf(lab, sizeof(lab), "Over %u", (unsigned)(i + 1));
        addFtpStep(d, lab, 60, 105);
    }
    addFtpStep(d, "Cool", 480, 45);
}
static void fillVo25x4(WorkoutDoc& d) {
    fillFtpRepeats(d, "vo2_5x4", "VO2 5x4", 5, 240, 112, 240, 50, 300, 45);
}

struct Builtin {
    const char* id;
    const char* name;
    const char* goal;
    void (*fill)(WorkoutDoc&);
};

static void fillPhysio1(WorkoutDoc& d) { fillPhysio(d, 1.0f); }

static const Builtin kBuiltins[] = {
    {"physio", "Physio Grundlage", "reha", fillPhysio1},
    {"reha_kurz", "Reha kurz", "reha", fillRehaKurz},
    {"easy20", "Locker 20 min", "fatloss", fillEasy},
    {"ftp_warm", "FTP-Warmup", "performance", fillFtpWarm},
    {"ss_3x12", "Sweet Spot 3x12", "performance", fillSs3x12},
    {"ss_2x20", "Sweet Spot 2x20", "performance", fillSs2x20},
    {"th_4x8", "Threshold 4x8", "performance", fillTh4x8},
    {"ftp_2x20", "FTP 2x20", "performance", fillFtp2x20},
    {"over_under", "Over/Under 6x(2+1)", "performance", fillOverUnder},
    {"vo2_5x4", "VO2 5x4", "performance", fillVo25x4},
    {"test_ramp", "Rampe", "performance", fillTestRamp},
    {"test_20min", "20 Minuten", "performance", fillTest20},
    {"test_recovery", "Recovery", "performance", fillTestRecovery},
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

const char* workoutBuiltinGoal(uint8_t index) {
    return index < workoutBuiltinCount() ? kBuiltins[index].goal : "";
}

bool workoutBuiltinById(const char* id, WorkoutDoc& out) {
    if (!id) return false;
    for (uint8_t i = 0; i < workoutBuiltinCount(); i++) {
        if (strcmp(id, kBuiltins[i].id) == 0) {
            kBuiltins[i].fill(out);
            strncpy(out.goal, kBuiltins[i].goal, sizeof(out.goal) - 1);
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
