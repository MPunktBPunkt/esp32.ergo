#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Das Steuer-Journal: hat ein Write gewirkt? (Abnahmekriterium 6b)
 *
 * Der Anlass steht in NACHTESTS.md unter „Bug in der Wirkungsauswertung". Die
 * Verdict-Logik der Sonde urteilte „wirkt" allein aus dem Leistungsdelta und
 * rechnete die Kadenz nicht heraus. Im ersten Lauf hat sie deshalb zweimal
 * einen Effekt bescheinigt, wo keiner war — einmal bei Kadenz null, einmal bei
 * mehr als verdoppelter Kadenz.
 *
 * Und der umgekehrte Fall ist gerade passiert: die erste Hardware-Session sah
 * lauter Erfolgsquittungen und hat daraus geschlossen, der Schreibweg stehe.
 * Er stand nicht — es ging die falsche Byte-Breite hinaus. Ein Journal, das
 * Quittung und Wirkung getrennt fuehrt, haette das gemeldet.
 *
 * Deshalb zwei Regeln:
 *
 *   1. Beurteilt wird **Watt pro Kadenz**, nicht Watt. Bei fester Stufe ist die
 *      Leistung ungefaehr Drehmoment mal Kadenz; Watt durch rpm ist damit ein
 *      Mass fuer das Drehmoment — und genau das ist es, was die Stufe stellt.
 *      Schneller treten erhoeht die Leistung, nicht die Stufe.
 *   2. Bei zu niedriger Kadenz oder weggelaufener Kadenz wird **kein Urteil**
 *      gefaellt. „Weiss nicht" ist ein Ergebnis, „wirkt" auf Basis von
 *      Rauschen ist keines.
 *
 * Arduino-frei, Zeit und Messwerte kommen herein.
 */
namespace ergo {

enum class Effect : uint8_t {
    Pending = 0,  // Fenster laeuft noch
    Works,        // Wirkung in der erwarteten Richtung
    NoEffect,     // keine nennenswerte Aenderung, oder die falsche Richtung
    Unjudged,     // Kadenz gab kein Urteil her
};

const char* effectName(Effect e);

struct JournalConfig {
    /** Wartezeit nach dem Write, bevor gemessen wird. Der Widerstand braucht
     *  einen Moment, und die Rampe des Limiters laeuft noch. */
    uint32_t settleMs = 5000;
    /** Laenge des Fensters vor und nach dem Write. */
    uint32_t windowMs = 6000;
    /** Darunter kein Urteil. Bewusst niedrig: von Hand gedrehte Kurbel liegt
     *  bei etwa 20 rpm, und dieser Fall soll beurteilbar bleiben. */
    float minRpm = 18.0f;
    /** Zulaessige Kadenzaenderung zwischen den Fenstern, in Prozent. Die
     *  Normierung faengt einiges auf, aber nicht beliebig viel. */
    float maxDriftPct = 25.0f;
    /** Kleinere Aenderung von Watt pro Kadenz gilt als keine. */
    float minEffectPct = 10.0f;
};

/** Ein beurteilter Schreibvorgang. */
struct JournalEntry {
    uint8_t cmd[8] = {0};  // 0x11 Simulation braucht 7 Byte
    uint8_t cmdLen = 0;
    int16_t fromTenths = -1;
    int16_t toTenths = -1;
    uint32_t atMs = 0;

    float preRpm = 0.0f, preWatt = 0.0f;
    float postRpm = 0.0f, postWatt = 0.0f;
    /** Watt pro rpm — die normierte Groesse, auf der das Urteil beruht. */
    float prePerRpm = 0.0f, postPerRpm = 0.0f;
    float changePct = 0.0f;
    uint16_t preSamples = 0, postSamples = 0;

    Effect effect = Effect::Pending;
    const char* reason = "";

    bool responseSeen = false;
    uint8_t responseOpcode = 0;
    uint8_t responseResult = 0;  // 0x01 = Success

    /**
     * Das Geraet hat Erfolg gemeldet, die Messung sagt nichts passiert.
     *
     * Dieser eine Ausdruck ist der Grund fuer die ganze Klasse. Genau dieser
     * Zustand lag in der ersten Hardware-Session vor, und niemand konnte ihn
     * benennen.
     */
    bool contradictory() const {
        return responseSeen && responseResult == 0x01 && effect == Effect::NoEffect;
    }
};

constexpr uint8_t kJournalSlots = 8;
constexpr uint8_t kJournalSamples = 64;

class ControlJournal {
public:
    void begin(const JournalConfig& cfg);
    const JournalConfig& config() const { return cfg_; }
    void reset();

    /** Jeden Live-Messwert hereingeben, auch wenn gerade nichts beurteilt wird —
     *  das Vorher-Fenster muss schon da sein, wenn der Write passiert. */
    void addSample(float rpm, float watt, uint32_t nowMs);

    /**
     * Nach einem tatsaechlich abgesetzten Write aufrufen.
     *
     * `fromTenths` und `toTenths` sind Schattenwerte des Limiters und dienen
     * nur der erwarteten Richtung. Ist sie unbekannt (beide gleich oder -1),
     * wird nur auf „irgendeine Aenderung" geprueft.
     */
    void noteWrite(const uint8_t* cmd, size_t len, int16_t fromTenths, int16_t toTenths,
                   uint32_t nowMs);

    /** Antwort vom Control Point. Wird dem offenen Eintrag zugeordnet. */
    void noteResponse(uint8_t opcode, uint8_t result);

    /** Aus loop() aufrufen: schliesst faellige Fenster ab. */
    void tick(uint32_t nowMs);

    uint8_t count() const { return count_; }
    /** Eintrag `i`, 0 ist der neueste. */
    const JournalEntry* at(uint8_t i) const;
    /** Der noch offene Eintrag, oder `nullptr`. */
    const JournalEntry* open() const;
    uint16_t judged() const { return judged_; }
    uint16_t worked() const { return worked_; }
    uint16_t noEffect() const { return noEffect_; }
    uint16_t unjudged() const { return unjudged_; }
    /** Wie oft Erfolg gemeldet und nichts passiert ist. */
    uint16_t contradictions() const { return contradictions_; }

private:
    struct Sample {
        uint32_t at;
        float rpm;
        float watt;
    };
    struct Stats {
        uint16_t n = 0;
        float rpm = 0.0f;
        float watt = 0.0f;
    };
    /** Mittelwerte ueber [fromMs, toMs]. */
    Stats window(uint32_t fromMs, uint32_t toMs) const;
    void finish(uint32_t nowMs);
    void push(const JournalEntry& e);

    JournalConfig cfg_;
    Sample samples_[kJournalSamples];
    uint8_t sHead_ = 0;
    uint8_t sCount_ = 0;

    JournalEntry slots_[kJournalSlots];
    uint8_t count_ = 0;

    JournalEntry pend_;
    bool pending_ = false;
    uint32_t writeAt_ = 0;

    uint16_t judged_ = 0, worked_ = 0, noEffect_ = 0, unjudged_ = 0, contradictions_ = 0;
};

}  // namespace ergo
