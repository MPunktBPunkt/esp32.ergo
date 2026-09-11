/**
 * Nativer Test des Steuer-Journals.
 *
 *   pio test -e native
 *
 * Zwei Fehler werden hier festgenagelt, beide echt passiert:
 *
 *   1. Die Sonde hat „wirkt" gesagt, weil die Leistung gestiegen war — obwohl
 *      nur schneller getreten wurde, und einmal bei Kadenz null.
 *   2. Die erste Hardware-Session hat aus Erfolgsquittungen geschlossen, der
 *      Schreibweg stehe. Es ging die falsche Byte-Breite hinaus.
 *
 * Beide muessen hier als NO_EFFECT beziehungsweise UNJUDGED herauskommen.
 */

#include <stdio.h>
#include <unity.h>

#include "control/ControlJournal.h"

using namespace ergo;

void setUp(void) {}
void tearDown(void) {}

static JournalConfig quickCfg() {
    JournalConfig c;
    c.settleMs = 1000;
    c.windowMs = 2000;
    c.minRpm = 18.0f;
    c.maxDriftPct = 25.0f;
    c.minEffectPct = 10.0f;
    return c;
}

/** Speist Messwerte im 250-ms-Raster, wie ein 4-Hz-Notify-Strom. */
struct Feeder {
    ControlJournal& j;
    uint32_t now = 0;
    explicit Feeder(ControlJournal& jj) : j(jj) {}
    void run(uint32_t ms, float rpm, float watt) {
        for (uint32_t i = 0; i < ms; i += 250) {
            j.addSample(rpm, watt, now);
            j.tick(now);
            now += 250;
        }
    }
    /** Wie `run`, aber ohne Messwerte — abgerissener Strom. */
    void silence(uint32_t ms) {
        for (uint32_t i = 0; i < ms; i += 250) {
            j.tick(now);
            now += 250;
        }
    }
    void write(int16_t from, int16_t to) {
        const uint8_t cmd[3] = {0x04, (uint8_t)(to & 0xFF), (uint8_t)(to >> 8)};
        j.noteWrite(cmd, 3, from, to, now);
    }
};

// ───────────────────────────────────── Der Fehler der letzten Session

/**
 * Success gemeldet, nichts passiert. Das ist der sint16-Fehler: Stufe 4 auf
 * Stufe 8 geschrieben, Bike quittiert mit `80 04 01`, Watt pro Kadenz bleibt
 * gleich. Das Journal muss NO_EFFECT sagen und den Widerspruch benennen.
 */
static void test_success_without_effect_is_contradiction(void) {
    ControlJournal j;
    j.begin(quickCfg());
    Feeder f(j);

    f.run(3000, 60.0f, 100.0f);
    f.write(40, 80);
    j.noteResponse(0x04, 0x01);  // Success
    f.run(4000, 60.0f, 100.0f);  // unveraendert

    TEST_ASSERT_EQUAL_UINT8(1, j.count());
    const JournalEntry* e = j.at(0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_INT((int)Effect::NoEffect, (int)e->effect);
    TEST_ASSERT_EQUAL_STRING("Watt pro Kadenz unveraendert", e->reason);
    TEST_ASSERT_TRUE(e->responseSeen);
    TEST_ASSERT_TRUE(e->contradictory());
    TEST_ASSERT_EQUAL_UINT16(1, j.contradictions());
    TEST_ASSERT_EQUAL_UINT16(1, j.noEffect());
    TEST_ASSERT_EQUAL_UINT16(0, j.worked());
}

// ────────────────────────────── Die Fehler der Sonden-Verdict-Logik

/**
 * Leistung verdoppelt, aber nur weil doppelt so schnell getreten wurde. Watt
 * pro Kadenz bleibt gleich — also keine Wirkung. Die Sonde hat hier „wirkt"
 * gesagt.
 */
static void test_more_power_from_more_cadence_is_no_effect(void) {
    ControlJournal j;
    JournalConfig c = quickCfg();
    c.maxDriftPct = 200.0f;  // Drift hier ausdruecklich zulassen
    j.begin(c);
    Feeder f(j);

    f.run(3000, 50.0f, 100.0f);  // 2,0 W/rpm
    f.write(40, 80);
    f.run(4000, 100.0f, 200.0f);  // ebenfalls 2,0 W/rpm

    const JournalEntry* e = j.at(0);
    TEST_ASSERT_EQUAL_INT((int)Effect::NoEffect, (int)e->effect);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.0f, e->prePerRpm);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.0f, e->postPerRpm);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, e->changePct);
}

/** Kadenz null: kein Urteil. Die Sonde hat hier ebenfalls „wirkt" gesagt. */
static void test_zero_cadence_is_unjudged(void) {
    ControlJournal j;
    j.begin(quickCfg());
    Feeder f(j);

    f.run(3000, 60.0f, 100.0f);
    f.write(40, 80);
    f.run(4000, 0.0f, 0.0f);

    const JournalEntry* e = j.at(0);
    TEST_ASSERT_EQUAL_INT((int)Effect::Unjudged, (int)e->effect);
    TEST_ASSERT_EQUAL_STRING("Kadenz zu niedrig fuer ein Urteil", e->reason);
    TEST_ASSERT_EQUAL_UINT16(1, j.unjudged());
    // Ein Widerspruch ist es nicht — es gibt ja kein Urteil.
    TEST_ASSERT_FALSE(e->contradictory());
}

/** Kadenz laeuft im Beobachtungszeitraum weg: ebenfalls kein Urteil. */
static void test_cadence_drift_is_unjudged(void) {
    ControlJournal j;
    j.begin(quickCfg());  // maxDriftPct = 25
    Feeder f(j);

    f.run(3000, 60.0f, 100.0f);
    f.write(40, 80);
    f.run(4000, 90.0f, 220.0f);  // +50 % Kadenz

    const JournalEntry* e = j.at(0);
    TEST_ASSERT_EQUAL_INT((int)Effect::Unjudged, (int)e->effect);
    TEST_ASSERT_EQUAL_STRING("Kadenz zwischen den Fenstern weggelaufen", e->reason);
}

// ──────────────────────────────────────────────── Der gute Fall

/** Stufe hoch, Drehmoment steigt, Kadenz gehalten: WORKS. */
static void test_real_effect_is_recognised(void) {
    ControlJournal j;
    j.begin(quickCfg());
    Feeder f(j);

    f.run(3000, 60.0f, 100.0f);  // 1,67 W/rpm
    f.write(40, 80);
    j.noteResponse(0x04, 0x01);
    f.run(4000, 58.0f, 160.0f);  // 2,76 W/rpm, Kadenz fast gehalten

    const JournalEntry* e = j.at(0);
    TEST_ASSERT_EQUAL_INT((int)Effect::Works, (int)e->effect);
    TEST_ASSERT_TRUE(e->changePct > 50.0f);
    TEST_ASSERT_FALSE(e->contradictory());
    TEST_ASSERT_EQUAL_UINT16(1, j.worked());
}

/** Stufe runter, Drehmoment faellt: ebenfalls WORKS. */
static void test_downward_effect_is_recognised(void) {
    ControlJournal j;
    j.begin(quickCfg());
    Feeder f(j);

    f.run(3000, 60.0f, 160.0f);
    f.write(80, 40);
    f.run(4000, 60.0f, 100.0f);

    const JournalEntry* e = j.at(0);
    TEST_ASSERT_EQUAL_INT((int)Effect::Works, (int)e->effect);
    TEST_ASSERT_TRUE(e->changePct < 0.0f);
}

/** Stufe hoch, Drehmoment faellt: das ist keine Wirkung, sondern ein Befund. */
static void test_wrong_direction_is_no_effect(void) {
    ControlJournal j;
    j.begin(quickCfg());
    Feeder f(j);

    f.run(3000, 60.0f, 160.0f);
    f.write(40, 80);  // hoch
    f.run(4000, 60.0f, 100.0f);  // aber schwaecher

    const JournalEntry* e = j.at(0);
    TEST_ASSERT_EQUAL_INT((int)Effect::NoEffect, (int)e->effect);
    TEST_ASSERT_EQUAL_STRING("Aenderung in die falsche Richtung", e->reason);
}

/** Ohne bekannte Richtung zaehlt jede nennenswerte Aenderung. */
static void test_unknown_direction_accepts_any_change(void) {
    ControlJournal j;
    j.begin(quickCfg());
    Feeder f(j);

    f.run(3000, 60.0f, 100.0f);
    const uint8_t cmd[1] = {0x07};  // Start/Resume, keine Stufe im Spiel
    f.j.noteWrite(cmd, 1, -1, -1, f.now);
    f.run(4000, 60.0f, 160.0f);

    const JournalEntry* e = j.at(0);
    TEST_ASSERT_EQUAL_INT((int)Effect::Works, (int)e->effect);
    TEST_ASSERT_EQUAL_UINT8(1, e->cmdLen);
    TEST_ASSERT_EQUAL_UINT8(0x07, e->cmd[0]);
}

// ───────────────────────────────────────────── Zeitverhalten

/** Das Nachher-Fenster beginnt erst nach der Einschwingzeit. */
static void test_settle_is_excluded(void) {
    ControlJournal j;
    j.begin(quickCfg());  // settle 1000, window 2000
    Feeder f(j);

    f.run(3000, 60.0f, 100.0f);
    f.write(40, 80);
    // Waehrend des Einschwingens noch der alte Wert …
    f.run(1000, 60.0f, 100.0f);
    // … danach der neue. Waere das Einschwingen mitgemessen, laege das
    // Ergebnis zwischen beiden und die Wirkung waere zu klein.
    f.run(3000, 60.0f, 200.0f);

    const JournalEntry* e = j.at(0);
    TEST_ASSERT_EQUAL_INT((int)Effect::Works, (int)e->effect);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 200.0f / 60.0f, e->postPerRpm);
}

/** Vor Ablauf des Fensters gibt es kein Urteil, nur einen offenen Eintrag. */
static void test_pending_until_window_done(void) {
    ControlJournal j;
    j.begin(quickCfg());
    Feeder f(j);

    f.run(3000, 60.0f, 100.0f);
    f.write(40, 80);
    f.run(1500, 60.0f, 160.0f);

    TEST_ASSERT_EQUAL_UINT8(0, j.count());
    const JournalEntry* o = j.open();
    TEST_ASSERT_NOT_NULL(o);
    TEST_ASSERT_EQUAL_INT((int)Effect::Pending, (int)o->effect);
    TEST_ASSERT_EQUAL_INT(80, o->toTenths);

    f.run(2500, 60.0f, 160.0f);
    TEST_ASSERT_EQUAL_UINT8(1, j.count());
    TEST_ASSERT_NULL(j.open());
}

/** Abgerissener Notify-Strom nach dem Write: kein Urteil. */
static void test_no_data_after_write(void) {
    ControlJournal j;
    j.begin(quickCfg());
    Feeder f(j);

    f.run(3000, 60.0f, 100.0f);
    f.write(40, 80);
    f.silence(4000);

    const JournalEntry* e = j.at(0);
    TEST_ASSERT_EQUAL_INT((int)Effect::Unjudged, (int)e->effect);
    TEST_ASSERT_EQUAL_STRING("keine Daten in einem der Fenster", e->reason);
}

/** Ein Write ohne Vorgeschichte ist nicht beurteilbar. */
static void test_no_data_before_write(void) {
    ControlJournal j;
    j.begin(quickCfg());
    Feeder f(j);

    f.write(40, 80);
    f.run(4000, 60.0f, 160.0f);

    const JournalEntry* e = j.at(0);
    TEST_ASSERT_EQUAL_INT((int)Effect::Unjudged, (int)e->effect);
    TEST_ASSERT_EQUAL_UINT16(0, e->preSamples);
}

/**
 * Zwei Writes dicht hintereinander — genau das macht die Rampe des Limiters.
 * Der erste Eintrag wird ohne Urteil geschlossen, statt eine vermischte
 * Ursache zu beurteilen.
 */
static void test_second_write_closes_first_unjudged(void) {
    ControlJournal j;
    j.begin(quickCfg());
    Feeder f(j);

    f.run(3000, 60.0f, 100.0f);
    f.write(40, 50);
    f.run(1000, 60.0f, 110.0f);
    f.write(50, 60);
    f.run(4000, 60.0f, 200.0f);

    TEST_ASSERT_EQUAL_UINT8(2, j.count());
    // Neueste zuerst: at(0) ist der zweite Write und beurteilt.
    TEST_ASSERT_EQUAL_INT(60, j.at(0)->toTenths);
    TEST_ASSERT_EQUAL_INT((int)Effect::Works, (int)j.at(0)->effect);
    TEST_ASSERT_EQUAL_INT(50, j.at(1)->toTenths);
    TEST_ASSERT_EQUAL_INT((int)Effect::Unjudged, (int)j.at(1)->effect);
    TEST_ASSERT_EQUAL_STRING("naechster Write kam zu schnell", j.at(1)->reason);
}

/** Der Ring haelt die letzten acht und zaehlt weiter. */
static void test_ring_keeps_last_eight(void) {
    ControlJournal j;
    j.begin(quickCfg());
    Feeder f(j);
    f.run(3000, 60.0f, 100.0f);
    for (int i = 0; i < 12; i++) {
        f.write(40, 80);
        f.run(4000, 60.0f, 100.0f);
    }
    TEST_ASSERT_EQUAL_UINT8(kJournalSlots, j.count());
    TEST_ASSERT_EQUAL_UINT16(12, j.judged());
    TEST_ASSERT_NULL(j.at(kJournalSlots));
}

static void test_effect_names(void) {
    TEST_ASSERT_EQUAL_STRING("WORKS", effectName(Effect::Works));
    TEST_ASSERT_EQUAL_STRING("NO_EFFECT", effectName(Effect::NoEffect));
    TEST_ASSERT_EQUAL_STRING("UNJUDGED", effectName(Effect::Unjudged));
    TEST_ASSERT_EQUAL_STRING("PENDING", effectName(Effect::Pending));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_success_without_effect_is_contradiction);
    RUN_TEST(test_more_power_from_more_cadence_is_no_effect);
    RUN_TEST(test_zero_cadence_is_unjudged);
    RUN_TEST(test_cadence_drift_is_unjudged);
    RUN_TEST(test_real_effect_is_recognised);
    RUN_TEST(test_downward_effect_is_recognised);
    RUN_TEST(test_wrong_direction_is_no_effect);
    RUN_TEST(test_unknown_direction_accepts_any_change);
    RUN_TEST(test_settle_is_excluded);
    RUN_TEST(test_pending_until_window_done);
    RUN_TEST(test_no_data_after_write);
    RUN_TEST(test_no_data_before_write);
    RUN_TEST(test_second_write_closes_first_unjudged);
    RUN_TEST(test_ring_keeps_last_eight);
    RUN_TEST(test_effect_names);
    return UNITY_END();
}
