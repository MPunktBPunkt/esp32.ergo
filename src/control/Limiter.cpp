#include "Limiter.h"

namespace ergo {
namespace {

inline int16_t minOf(int16_t a, int16_t b) { return a < b ? a : b; }

inline int16_t clampI16(int32_t v, int32_t lo, int32_t hi) {
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (int16_t)v;
}

inline int16_t s16le(const uint8_t* p) {
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

inline void put16le(uint8_t* p, int16_t v) {
    p[0] = (uint8_t)((uint16_t)v & 0xFF);
    p[1] = (uint8_t)(((uint16_t)v >> 8) & 0xFF);
}

}  // namespace

const char* decisionName(Decision d) {
    switch (d) {
        case Decision::Allow: return "ALLOW";
        case Decision::Clamp: return "CLAMP";
        case Decision::Defer: return "DEFER";
        case Decision::Deny: return "DENY";
        default: return "UNKNOWN";
    }
}

// ------------------------------------------------------------- Grenzen

int16_t Limiter::effectiveMaxLevelTenths() const {
    int16_t m = cfg_.absMaxLevelTenths;
    if (caps_ && caps_->hasResistanceRange) m = minOf(m, caps_->levelMaxTenths());
    if (cfg_.profileMaxLevelTenths > 0) m = minOf(m, cfg_.profileMaxLevelTenths);
    return m;
}

int16_t Limiter::effectiveMinLevelTenths() const {
    if (caps_ && caps_->hasResistanceRange) return caps_->levelMinTenths();
    return 0;
}

int16_t Limiter::effectiveMaxPowerW() const {
    int16_t m = cfg_.absMaxPowerW;
    if (caps_ && caps_->hasPowerRange) m = minOf(m, caps_->power.maxW);
    if (cfg_.profileMaxPowerW > 0) m = minOf(m, cfg_.profileMaxPowerW);
    return m;
}

int16_t Limiter::snapLevel(int16_t tenths) const {
    const int16_t lo = effectiveMinLevelTenths();
    const int16_t hi = effectiveMaxLevelTenths();
    int32_t v = clampI16(tenths, lo, hi);

    const uint16_t step = caps_ ? caps_->levelStepTenths() : 0;
    if (step > 1) {
        const int32_t n = (v - lo + step / 2) / step;
        v = lo + n * (int32_t)step;
        if (v > hi) v -= step;  // Runden darf nicht ueber das Maximum tragen
        if (v < lo) v = lo;
    }
    return (int16_t)v;
}

// -------------------------------------------------------------- Bausteine

Limiter::Verdict Limiter::deny(const char* reason) const {
    ++denies_;
    Verdict v;
    v.decision = Decision::Deny;
    v.reason = reason;
    return v;
}

Limiter::Verdict Limiter::allow(const uint8_t* cmd, size_t len) const {
    Verdict v;
    v.decision = Decision::Allow;
    v.len = (uint8_t)len;
    for (size_t i = 0; i < len; ++i) v.data[i] = cmd[i];
    return v;
}

Limiter::Verdict Limiter::clampTo(const uint8_t* data, size_t len, const char* reason) const {
    Verdict v;
    v.decision = Decision::Clamp;
    v.len = (uint8_t)len;
    v.reason = reason;
    for (size_t i = 0; i < len; ++i) v.data[i] = data[i];
    return v;
}

// ---------------------------------------------------------------- Pruefung

Limiter::Verdict Limiter::check(const uint8_t* cmd, size_t len, uint32_t nowMs) const {
    if (!cmd || len < 1) return deny("leeres Kommando");

    switch ((ftms::Opcode)cmd[0]) {
        case ftms::Opcode::RequestControl:
        case ftms::Opcode::Reset:
        case ftms::Opcode::StartResume:
            if (len != 1) return deny("unerwartete Nutzlast");
            return allow(cmd, len);

        case ftms::Opcode::StopPause:
            // Last wegnehmen ist nie verhandelbar — kein Capability-Check,
            // keine Rampe, keine Klemme.
            if (len != 2) return deny("Stop braucht genau einen Parameter");
            if (cmd[1] != (uint8_t)ftms::StopParam::Stop &&
                cmd[1] != (uint8_t)ftms::StopParam::Pause)
                return deny("unbekannter Stop-Parameter");
            return allow(cmd, len);

        case ftms::Opcode::SetTargetResistance:
            return checkResistance(cmd, len, nowMs);

        case ftms::Opcode::SetTargetPower:
            return checkPower(cmd, len);

        case ftms::Opcode::SetTargetHeartRate:
            return deny("Pulsziel wird nicht gestellt — Kaskade ueber den Widerstand");

        case ftms::Opcode::SetIndoorBikeSimulation:
            return checkSimulation(cmd, len);

        default:
            return deny("Opcode nicht auf der Whitelist");
    }
}

Limiter::Verdict Limiter::checkResistance(const uint8_t* cmd, size_t len, uint32_t nowMs) const {
    if (!caps_ || !caps_->valid) return deny("Faehigkeiten des Geraets unbekannt");
    if (!caps_->canTargetResistance) return deny("Geraet kennt kein Stufenziel");
    if (!caps_->hasResistanceRange) return deny("kein Stufenbereich aus 0x2AD6");

    int32_t want;
    if (len == 2) {
        want = cmd[1];  // uint8 in Zehnteln
    } else if (len == 3) {
        want = s16le(cmd + 1);
    } else {
        return deny("unbekannte Nutzlast fuer 0x04");
    }

    int16_t target = snapLevel((int16_t)clampI16(want, -32768, 32767));
    const char* reason = (target != want) ? "auf Geraetebereich geklemmt" : "";

    // Rampe gilt nur nach oben. Last wegnehmen darf sofort passieren —
    // sonst waere jeder Pulsdeckel und jeder Not-Stop traege.
    if (haveLevel_ && target > level_) {
        uint16_t stepT = cfg_.rampStepTenths;
        if (stepT == 0) stepT = caps_->levelStepTenths();
        if (stepT == 0) stepT = 1;

        if (cfg_.rampMs > 0) {
            const uint32_t elapsed = nowMs - lastLevelWriteMs_;
            const uint32_t allowedSteps = elapsed / cfg_.rampMs;
            if (allowedSteps == 0) {
                Verdict v;
                v.decision = Decision::Defer;
                v.reason = "Rampe — zu frueh fuer die naechste Stufe";
                return v;
            }
            const int32_t maxDelta = (int32_t)allowedSteps * (int32_t)stepT;
            if (target - level_ > maxDelta) {
                target = snapLevel((int16_t)(level_ + maxDelta));
                reason = "Rampe nach oben begrenzt";
            }
        }
    }

    uint8_t out[3];
    size_t outLen;
    if (len == 2) {
        if (target < 0 || target > 255) return deny("Stufe passt nicht in die uint8-Form");
        out[0] = cmd[0];
        out[1] = (uint8_t)target;
        outLen = 2;
    } else {
        out[0] = cmd[0];
        put16le(out + 1, target);
        outLen = 3;
    }

    bool same = true;
    for (size_t i = 0; i < outLen; ++i) {
        if (out[i] != cmd[i]) same = false;
    }
    return same ? allow(cmd, len) : clampTo(out, outLen, reason);
}

Limiter::Verdict Limiter::checkPower(const uint8_t* cmd, size_t len) const {
    if (!caps_ || !caps_->valid) return deny("Faehigkeiten des Geraets unbekannt");
    // Der entscheidende Riegel: der Varon quittiert 0x05 mit Success, ohne
    // das Feature zu melden und ohne 0x2AD8. Ohne belegtes Wattziel geht
    // hier nichts durch — gesteuert wird dann ueber den Widerstand.
    // Ausnahme: allowUntrustedPower (Nachtest 3, raw&force) — bewusst und eng.
    if (!cfg_.allowUntrustedPower) {
        if (!caps_->canTargetPower) return deny("Geraet meldet kein Wattziel");
        if (!caps_->powerTargetTrusted) return deny("Wattziel ohne 0x2AD8 — nicht belastbar");
    }
    if (len != 3) return deny("unbekannte Nutzlast fuer 0x05");

    const int16_t want = s16le(cmd + 1);
    int16_t lo = 0;
    if (caps_->hasPowerRange && caps_->power.minW > 0) lo = caps_->power.minW;
    const int16_t target = clampI16(want, lo, effectiveMaxPowerW());
    if (target == want) {
        if (cfg_.allowUntrustedPower)
            return clampTo(cmd, len, "UNTRUSTED 0x05 — Nachtest/Diagnose");
        return allow(cmd, len);
    }

    uint8_t out[3] = {cmd[0], 0, 0};
    put16le(out + 1, target);
    return clampTo(out, 3,
                   cfg_.allowUntrustedPower ? "UNTRUSTED 0x05 geklemmt" : "auf zulaessigen Wattbereich geklemmt");
}

Limiter::Verdict Limiter::checkSimulation(const uint8_t* cmd, size_t len) const {
    if (!cfg_.allowSimulation) return deny("Simulation gesperrt — allowSimulation aus");
    if (!caps_ || !caps_->valid) return deny("Faehigkeiten des Geraets unbekannt");
    if (!caps_->canSimulate) return deny("Geraet kennt keine Simulation");
    if (len != 7) return deny("unbekannte Nutzlast fuer 0x11");

    const int16_t grade = s16le(cmd + 3);
    const int16_t lim = cfg_.maxGradeHundredth;
    const int16_t target = clampI16(grade, -lim, lim);
    if (target == grade) return allow(cmd, len);

    uint8_t out[7];
    for (size_t i = 0; i < 7; ++i) out[i] = cmd[i];
    put16le(out + 3, target);
    return clampTo(out, 7, "Steigung geklemmt");
}

// ------------------------------------------------------------- Buchfuehrung

void Limiter::noteWritten(const uint8_t* cmd, size_t len, uint32_t nowMs) {
    if (!cmd || len < 1) return;
    ++writes_;
    lastAlive_ = nowMs;

    switch ((ftms::Opcode)cmd[0]) {
        case ftms::Opcode::SetTargetResistance:
            if (len == 2) {
                level_ = cmd[1];
            } else if (len == 3) {
                level_ = s16le(cmd + 1);
            } else {
                break;
            }
            haveLevel_ = true;
            lastLevelWriteMs_ = nowMs;
            if (cfg_.deadmanMs > 0) armed_ = true;
            break;

        case ftms::Opcode::SetTargetPower:
        case ftms::Opcode::SetIndoorBikeSimulation:
            if (cfg_.deadmanMs > 0) armed_ = true;
            break;

        case ftms::Opcode::Reset:
        case ftms::Opcode::StopPause:
            // Nach Stop oder Reset ist unbekannt, wo das Geraet steht. Die
            // konservative Annahme ist das Minimum: dann rampt der naechste
            // Aufbau von unten hoch, statt aus einem veralteten Schattenwert
            // heraus zu springen.
            level_ = effectiveMinLevelTenths();
            haveLevel_ = true;
            lastLevelWriteMs_ = nowMs;
            armed_ = false;
            break;

        default:
            break;
    }
}

bool Limiter::expired(uint32_t nowMs) const {
    if (!armed_ || cfg_.deadmanMs == 0) return false;
    return (uint32_t)(nowMs - lastAlive_) > cfg_.deadmanMs;
}

uint32_t Limiter::remainingMs(uint32_t nowMs) const {
    if (!armed_ || cfg_.deadmanMs == 0) return 0;
    const uint32_t elapsed = nowMs - lastAlive_;
    return elapsed >= cfg_.deadmanMs ? 0 : cfg_.deadmanMs - elapsed;
}

void Limiter::reset() {
    level_ = 0;
    haveLevel_ = false;
    lastLevelWriteMs_ = 0;
    armed_ = false;
}

}  // namespace ergo
