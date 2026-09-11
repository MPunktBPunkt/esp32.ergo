#include "ControlJournal.h"

namespace ergo {

const char* effectName(Effect e) {
    switch (e) {
        case Effect::Pending: return "PENDING";
        case Effect::Works: return "WORKS";
        case Effect::NoEffect: return "NO_EFFECT";
        case Effect::Unjudged: return "UNJUDGED";
        default: return "UNKNOWN";
    }
}

void ControlJournal::begin(const JournalConfig& cfg) {
    cfg_ = cfg;
    if (cfg_.windowMs < 500) cfg_.windowMs = 500;
    if (cfg_.minRpm < 1.0f) cfg_.minRpm = 1.0f;
    if (cfg_.maxDriftPct < 1.0f) cfg_.maxDriftPct = 1.0f;
    if (cfg_.minEffectPct < 0.1f) cfg_.minEffectPct = 0.1f;
    reset();
}

void ControlJournal::reset() {
    sHead_ = 0;
    sCount_ = 0;
    count_ = 0;
    pending_ = false;
    writeAt_ = 0;
    pend_ = JournalEntry{};
    judged_ = worked_ = noEffect_ = unjudged_ = contradictions_ = 0;
    for (uint8_t i = 0; i < kJournalSlots; i++) slots_[i] = JournalEntry{};
}

void ControlJournal::addSample(float rpm, float watt, uint32_t nowMs) {
    if (rpm < 0.0f || watt < 0.0f) return;
    samples_[sHead_] = Sample{nowMs, rpm, watt};
    sHead_ = (uint8_t)((sHead_ + 1) % kJournalSamples);
    if (sCount_ < kJournalSamples) sCount_++;
}

ControlJournal::Stats ControlJournal::window(uint32_t fromMs, uint32_t toMs) const {
    Stats s;
    for (uint8_t i = 0; i < sCount_; i++) {
        const Sample& x = samples_[i];
        // Vorzeichenbehaftet vergleichen, damit der Millis-Ueberlauf nicht
        // zufaellig ein leeres Fenster erzeugt.
        if ((int32_t)(x.at - fromMs) < 0) continue;
        if ((int32_t)(x.at - toMs) > 0) continue;
        s.rpm += x.rpm;
        s.watt += x.watt;
        s.n++;
    }
    if (s.n) {
        s.rpm /= (float)s.n;
        s.watt /= (float)s.n;
    }
    return s;
}

void ControlJournal::noteWrite(const uint8_t* cmd, size_t len, int16_t fromTenths,
                               int16_t toTenths, uint32_t nowMs) {
    // Ein neuer Write beendet einen offenen Eintrag ohne Urteil. Zwei Writes
    // in einem Fenster sind nicht trennbar, und ein Urteil aus vermischten
    // Ursachen waere schlechter als keines.
    if (pending_) {
        pend_.effect = Effect::Unjudged;
        pend_.reason = "naechster Write kam zu schnell";
        unjudged_++;
        judged_++;
        push(pend_);
    }

    pend_ = JournalEntry{};
    pend_.cmdLen = (uint8_t)(len > sizeof(pend_.cmd) ? sizeof(pend_.cmd) : len);
    for (uint8_t i = 0; i < pend_.cmdLen; i++) pend_.cmd[i] = cmd[i];
    pend_.fromTenths = fromTenths;
    pend_.toTenths = toTenths;
    pend_.atMs = nowMs;

    // Das Vorher-Fenster liegt schon vor: es endet beim Write.
    const Stats pre = window(nowMs - cfg_.windowMs, nowMs);
    pend_.preSamples = pre.n;
    pend_.preRpm = pre.rpm;
    pend_.preWatt = pre.watt;
    pend_.prePerRpm = (pre.rpm > 0.0f) ? (pre.watt / pre.rpm) : 0.0f;

    pend_.effect = Effect::Pending;
    pending_ = true;
    writeAt_ = nowMs;
}

void ControlJournal::noteResponse(uint8_t opcode, uint8_t result) {
    if (!pending_) return;
    pend_.responseSeen = true;
    pend_.responseOpcode = opcode;
    pend_.responseResult = result;
}

void ControlJournal::tick(uint32_t nowMs) {
    if (!pending_) return;
    if ((int32_t)(nowMs - (writeAt_ + cfg_.settleMs + cfg_.windowMs)) < 0) return;
    finish(nowMs);
}

void ControlJournal::finish(uint32_t nowMs) {
    (void)nowMs;
    const uint32_t from = writeAt_ + cfg_.settleMs;
    const Stats post = window(from, from + cfg_.windowMs);
    pend_.postSamples = post.n;
    pend_.postRpm = post.rpm;
    pend_.postWatt = post.watt;
    pend_.postPerRpm = (post.rpm > 0.0f) ? (post.watt / post.rpm) : 0.0f;

    if (pend_.preSamples == 0 || pend_.postSamples == 0) {
        pend_.effect = Effect::Unjudged;
        pend_.reason = "keine Daten in einem der Fenster";
    } else if (pend_.preRpm < cfg_.minRpm || pend_.postRpm < cfg_.minRpm) {
        // Genau der Fall, in dem die Sonde „wirkt" gesagt hat: Kadenz null.
        pend_.effect = Effect::Unjudged;
        pend_.reason = "Kadenz zu niedrig fuer ein Urteil";
    } else {
        const float drift = (pend_.postRpm - pend_.preRpm) / pend_.preRpm * 100.0f;
        const float absDrift = drift < 0.0f ? -drift : drift;
        if (absDrift > cfg_.maxDriftPct) {
            pend_.effect = Effect::Unjudged;
            pend_.reason = "Kadenz zwischen den Fenstern weggelaufen";
        } else if (pend_.prePerRpm <= 0.0f) {
            pend_.effect = Effect::Unjudged;
            pend_.reason = "kein Bezugswert vor dem Write";
        } else {
            pend_.changePct =
                (pend_.postPerRpm - pend_.prePerRpm) / pend_.prePerRpm * 100.0f;
            const float absChange = pend_.changePct < 0.0f ? -pend_.changePct : pend_.changePct;

            // Erwartete Richtung aus den Schattenwerten. Sind sie unbekannt,
            // zaehlt jede nennenswerte Aenderung.
            int8_t want = 0;
            if (pend_.fromTenths >= 0 && pend_.toTenths >= 0) {
                if (pend_.toTenths > pend_.fromTenths) want = 1;
                else if (pend_.toTenths < pend_.fromTenths) want = -1;
            }

            if (absChange < cfg_.minEffectPct) {
                pend_.effect = Effect::NoEffect;
                pend_.reason = "Watt pro Kadenz unveraendert";
            } else if (want != 0 &&
                       ((want > 0 && pend_.changePct < 0.0f) ||
                        (want < 0 && pend_.changePct > 0.0f))) {
                pend_.effect = Effect::NoEffect;
                pend_.reason = "Aenderung in die falsche Richtung";
            } else {
                pend_.effect = Effect::Works;
                pend_.reason = "";
            }
        }
    }

    judged_++;
    switch (pend_.effect) {
        case Effect::Works: worked_++; break;
        case Effect::NoEffect: noEffect_++; break;
        default: unjudged_++; break;
    }
    if (pend_.contradictory()) contradictions_++;

    push(pend_);
    pending_ = false;
}

void ControlJournal::push(const JournalEntry& e) {
    // Neueste zuerst: in der UI ist der letzte Write der interessante.
    for (uint8_t i = kJournalSlots - 1; i > 0; i--) slots_[i] = slots_[i - 1];
    slots_[0] = e;
    if (count_ < kJournalSlots) count_++;
}

const JournalEntry* ControlJournal::at(uint8_t i) const {
    if (i >= count_) return nullptr;
    return &slots_[i];
}

const JournalEntry* ControlJournal::open() const { return pending_ ? &pend_ : nullptr; }

}  // namespace ergo
