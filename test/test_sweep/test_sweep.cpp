/**
 * Nativer Test des gefuehrten Sweeps.
 *
 *   pio test -e native
 *
 * Der SweepRunner trifft nur Entscheidungen, deshalb laesst sich ein
 * kompletter Neun-Stufen-Lauf hier in Millisekunden durchspielen — inklusive
 * der Faelle, die auf dem Rad Stunden kosten: Kadenz laeuft weg, Notify-Strom
 * reisst ab, Fahrer steigt ab.
 *
 * Getestet wird ausdruecklich auch, dass das Einschwingen erst nach dem
 * bestaetigten Write beginnt. Die Rampe des Limiters verzoegert einen Write um
 * Sekunden, und wuerde man die mitmessen, waere jeder erste Messwert zu klein.
 */

#include <stdio.h>
#include <unity.h>

#include "control/SweepRunner.h"

using namespace ergo;

void setUp(void) {}
void tearDown(void) {}

/** Kurze Zeiten, damit die Tests nicht zu Buchhaltung werden. */
static SweepPlan quickPlan(uint8_t count) {
    SweepPlan p = SweepRunner::planFor(16, 10, 10, 60.0f, count == 4);
    p.settleMs = 2000;
    p.windowMs = 4000;
    p.abortAfterMs = 3000;
    return p;
}

/**
 * Faehrt einen Punkt bis zum Ende seines Fensters.
 *
 * Bildet den Aufrufer nach: `SetLevel` wird als erfolgreich abgesetzt
 * quittiert, Stop wird gezaehlt. Schrittweite 100 ms, wie die Notify-Rate
 * des Bikes.
 */
struct Driver {
    SweepRunner& r;
    uint32_t now = 0;
    int setCalls = 0;
    int stopCalls = 0;
    int16_t lastLevel = -1;
    bool confirmSets = true;

    explicit Driver(SweepRunner& rr) : r(rr) {}

    void step(float rpm, float watt, bool fresh = true) {
        const SweepRunner::Tick t = r.tick(now, rpm, watt, fresh);
        if (t.action == SweepRunner::Tick::Do::SetLevel) {
            setCalls++;
            lastLevel = t.levelTenths;
            if (confirmSets) r.noteLevelSet(now);
        } else if (t.action == SweepRunner::Tick::Do::Stop) {
            stopCalls++;
        }
        now += 100;
    }

    void run(uint32_t ms, float rpm, float watt, bool fresh = true) {
        for (uint32_t i = 0; i < ms; i += 100) step(rpm, watt, fresh);
    }
};

// ─────────────────────────────────────────────────────────────── Plaene

static void test_plan_full(void) {
    const SweepPlan p = SweepRunner::planFor(16, 10, 10, 60.0f, false);
    TEST_ASSERT_EQUAL_UINT8(9, p.count);
    TEST_ASSERT_EQUAL_INT(10, p.levels[0]);    // Stufe 1
    TEST_ASSERT_EQUAL_INT(20, p.levels[1]);    // Stufe 2
    TEST_ASSERT_EQUAL_INT(40, p.levels[2]);    // Stufe 4
    TEST_ASSERT_EQUAL_INT(160, p.levels[8]);   // Stufe 16
    TEST_ASSERT_EQUAL_FLOAT(60.0f, p.targetRpm);
}

static void test_plan_coarse(void) {
    const SweepPlan p = SweepRunner::planFor(16, 10, 10, 80.0f, true);
    TEST_ASSERT_EQUAL_UINT8(4, p.count);
    TEST_ASSERT_EQUAL_INT(40, p.levels[0]);
    TEST_ASSERT_EQUAL_INT(160, p.levels[3]);
    TEST_ASSERT_EQUAL_FLOAT(80.0f, p.targetRpm);
}

static void test_plan_light(void) {
    const SweepPlan p = SweepRunner::planFor(16, 10, 10, 80.0f, "light");
    TEST_ASSERT_EQUAL_UINT8(2, p.count);
    TEST_ASSERT_EQUAL_INT(40, p.levels[0]);
    TEST_ASSERT_EQUAL_INT(80, p.levels[1]);
    TEST_ASSERT_EQUAL_FLOAT(80.0f, p.targetRpm);
}

static void test_plan_shortens_on_small_device(void) {
    // Acht Stufen: die Stufen 10 bis 16 fallen weg, der Plan bleibt fahrbar
    const SweepPlan p = SweepRunner::planFor(8, 10, 10, 60.0f, false);
    TEST_ASSERT_EQUAL_UINT8(5, p.count);  // 1, 2, 4, 6, 8
    TEST_ASSERT_EQUAL_INT(80, p.levels[4]);
}

static void test_start_rejects_empty_plan(void) {
    SweepRunner r;
    SweepPlan p;
    TEST_ASSERT_FALSE(r.start(p, 0));
    TEST_ASSERT_EQUAL_INT((int)SweepState::Idle, (int)r.state());
}

// ───────────────────────────────────────────────────────── Normallauf

static void test_full_run_collects_every_level(void) {
    SweepRunner r;
    const SweepPlan p = quickPlan(9);
    TEST_ASSERT_TRUE(r.start(p, 0));

    Driver d(r);
    // 9 Stufen x (2 s einschwingen + 4 s messen), grosszuegig plus Reserve
    for (int i = 0; i < 9; i++) d.run(6200, 60.0f, 100.0f);

    TEST_ASSERT_EQUAL_INT((int)SweepState::Done, (int)r.state());
    TEST_ASSERT_EQUAL_UINT8(9, r.pointCount());
    TEST_ASSERT_EQUAL_UINT8(9, r.validCount());
    TEST_ASSERT_EQUAL_UINT8(100, r.progressPct());
    TEST_ASSERT_EQUAL_INT(9, d.setCalls);
    // Am Ende genau ein Stop: die Last darf nicht stehen bleiben
    TEST_ASSERT_EQUAL_INT(1, d.stopCalls);
    TEST_ASSERT_EQUAL_INT(160, r.point(8).levelTenths);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 100.0f, r.point(0).meanWatt);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 60.0f, r.point(0).meanRpm);
}

static void test_settle_excluded_from_window(void) {
    SweepRunner r;
    SweepPlan p = quickPlan(4);
    p.count = 1;  // nur eine Stufe, das genuegt fuer diese Frage
    TEST_ASSERT_TRUE(r.start(p, 0));

    Driver d(r);
    // Waehrend des Einschwingens liegen 40 W an, im Fenster dann 120 W.
    d.run(2000, 60.0f, 40.0f);
    TEST_ASSERT_EQUAL_INT((int)SweepState::Settle, (int)r.state());
    d.run(4200, 60.0f, 120.0f);

    TEST_ASSERT_EQUAL_UINT8(1, r.pointCount());
    TEST_ASSERT_TRUE(r.point(0).valid);
    // Waeren die Einschwingwerte mitgemittelt, laege das Ergebnis bei ~93 W
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 120.0f, r.point(0).meanWatt);
}

static void test_window_starts_after_confirmed_write(void) {
    SweepRunner r;
    SweepPlan p = quickPlan(4);
    p.count = 1;
    r.start(p, 0);

    Driver d(r);
    d.confirmSets = false;  // der Limiter defert, der Write kommt nicht durch
    d.run(5000, 60.0f, 100.0f);

    // Ohne bestaetigten Write laeuft die Uhr nicht los
    TEST_ASSERT_EQUAL_INT((int)SweepState::Settle, (int)r.state());
    TEST_ASSERT_EQUAL_UINT8(0, r.pointCount());
    // Und es wird nachgefasst, aber nicht in jedem Durchlauf: 5 s bei 500 ms
    TEST_ASSERT_TRUE(d.setCalls >= 9);
    TEST_ASSERT_TRUE(d.setCalls <= 11);

    // Jetzt geht er durch, ab hier zaehlt die Zeit
    d.confirmSets = true;
    d.run(6200, 60.0f, 100.0f);
    TEST_ASSERT_EQUAL_INT((int)SweepState::Done, (int)r.state());
    TEST_ASSERT_EQUAL_UINT8(1, r.validCount());
}

// ───────────────────────────────────────────── Verwerfungsregeln

static void test_discards_drifting_cadence(void) {
    SweepRunner r;
    SweepPlan p = quickPlan(4);
    p.count = 1;
    r.start(p, 0);

    Driver d(r);
    d.run(2000, 60.0f, 100.0f);  // einschwingen
    // Im Fenster von 60 auf 75 rpm: Spanne 15 von Mittel ~67, also gut 22 %
    d.run(2000, 60.0f, 100.0f);
    d.run(2200, 75.0f, 140.0f);

    TEST_ASSERT_EQUAL_UINT8(1, r.pointCount());
    TEST_ASSERT_FALSE(r.point(0).valid);
    TEST_ASSERT_EQUAL_STRING("Kadenz nicht gehalten", r.point(0).reason);
    TEST_ASSERT_EQUAL_UINT8(0, r.validCount());
    // Die Rohwerte bleiben trotzdem am Punkt haengen — ein verworfener Punkt
    // soll nachvollziehbar sein, nicht verschwinden
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 60.0f, r.point(0).rpmMin);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 75.0f, r.point(0).rpmMax);
}

static void test_discards_low_cadence(void) {
    SweepRunner r;
    SweepPlan p = quickPlan(4);
    p.count = 1;
    p.minRpm = 45.0f;
    p.abortAfterMs = 60000;  // hier interessiert die Verwerfung, nicht der Abbruch
    r.start(p, 0);

    Driver d(r);
    d.run(2000, 60.0f, 100.0f);
    // Gleichmaessig, aber zu langsam: 42 rpm liegt unter der Schwelle
    d.run(4200, 42.0f, 55.0f);

    TEST_ASSERT_EQUAL_UINT8(1, r.pointCount());
    TEST_ASSERT_FALSE(r.point(0).valid);
    TEST_ASSERT_EQUAL_STRING("Kadenz zu niedrig", r.point(0).reason);
}

static void test_stale_data_is_not_averaged(void) {
    SweepRunner r;
    SweepPlan p = quickPlan(4);
    p.count = 1;
    p.abortAfterMs = 60000;
    r.start(p, 0);

    Driver d(r);
    d.run(2000, 60.0f, 100.0f);
    // Halbes Fenster echte Daten, halbes Fenster abgerissener Strom.
    // Der alte Wert wuerde eine ruhige Fahrt vortaeuschen.
    d.run(2000, 60.0f, 100.0f, true);
    d.run(2200, 60.0f, 100.0f, false);

    TEST_ASSERT_EQUAL_UINT8(1, r.pointCount());
    TEST_ASSERT_TRUE(r.point(0).samples <= 21);
    TEST_ASSERT_TRUE(r.point(0).samples >= 19);
}

static void test_no_data_at_all_is_no_point(void) {
    SweepRunner r;
    SweepPlan p = quickPlan(4);
    p.count = 1;
    p.abortAfterMs = 60000;
    r.start(p, 0);

    Driver d(r);
    d.run(6200, 60.0f, 100.0f, false);
    TEST_ASSERT_EQUAL_UINT8(1, r.pointCount());
    TEST_ASSERT_FALSE(r.point(0).valid);
    TEST_ASSERT_EQUAL_STRING("keine Daten", r.point(0).reason);
}

// ─────────────────────────────────────────────────────────── Abbruch

static void test_aborts_when_rider_stops(void) {
    SweepRunner r;
    const SweepPlan p = quickPlan(9);
    r.start(p, 0);

    Driver d(r);
    d.run(3000, 60.0f, 100.0f);
    TEST_ASSERT_TRUE(r.running());
    // Kadenz auf null, laenger als abortAfterMs
    d.run(3500, 0.0f, 0.0f);

    TEST_ASSERT_EQUAL_INT((int)SweepState::Aborted, (int)r.state());
    TEST_ASSERT_FALSE(r.running());
    TEST_ASSERT_EQUAL_STRING("Kadenz weg", r.abortReason());
    // Genau ein Stop, und danach Ruhe
    TEST_ASSERT_EQUAL_INT(1, d.stopCalls);
    const int before = d.setCalls;
    d.run(5000, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT(1, d.stopCalls);
    TEST_ASSERT_EQUAL_INT(before, d.setCalls);
}

static void test_aborts_when_link_dies(void) {
    SweepRunner r;
    const SweepPlan p = quickPlan(9);
    r.start(p, 0);

    Driver d(r);
    d.run(3000, 60.0f, 100.0f);
    d.run(3500, 60.0f, 100.0f, false);  // Werte da, aber veraltet

    TEST_ASSERT_EQUAL_INT((int)SweepState::Aborted, (int)r.state());
    TEST_ASSERT_EQUAL_STRING("keine Daten vom Bike", r.abortReason());
    TEST_ASSERT_EQUAL_INT(1, d.stopCalls);
}

static void test_brief_dip_does_not_abort(void) {
    SweepRunner r;
    const SweepPlan p = quickPlan(9);
    r.start(p, 0);

    Driver d(r);
    d.run(2500, 60.0f, 100.0f);
    d.run(1000, 10.0f, 5.0f);  // kurz durchhaengen, unter abortAfterMs
    d.run(1000, 60.0f, 100.0f);
    TEST_ASSERT_TRUE(r.running());
    TEST_ASSERT_EQUAL_INT(0, d.stopCalls);
}

static void test_cancel_stops(void) {
    SweepRunner r;
    const SweepPlan p = quickPlan(9);
    r.start(p, 0);

    Driver d(r);
    d.run(3000, 60.0f, 100.0f);
    r.cancel(d.now);
    TEST_ASSERT_EQUAL_INT((int)SweepState::Aborted, (int)r.state());
    TEST_ASSERT_EQUAL_STRING("abgebrochen", r.abortReason());
    // Nach dem Abbruch stellt der Runner keine Stufen mehr
    const int before = d.setCalls;
    d.run(5000, 60.0f, 100.0f);
    TEST_ASSERT_EQUAL_INT(before, d.setCalls);
}

static void test_reset_clears(void) {
    SweepRunner r;
    r.start(quickPlan(4), 0);
    Driver d(r);
    d.run(6200, 60.0f, 100.0f);
    TEST_ASSERT_EQUAL_UINT8(1, r.pointCount());
    r.reset();
    TEST_ASSERT_EQUAL_INT((int)SweepState::Idle, (int)r.state());
    TEST_ASSERT_EQUAL_UINT8(0, r.pointCount());
    TEST_ASSERT_EQUAL_INT(-1, r.currentLevelTenths());
}

static void test_progress_counts_levels(void) {
    SweepRunner r;
    r.start(quickPlan(4), 0);
    Driver d(r);
    TEST_ASSERT_EQUAL_UINT8(0, r.progressPct());
    d.run(6200, 60.0f, 100.0f);
    TEST_ASSERT_EQUAL_UINT8(25, r.progressPct());
    d.run(6200, 60.0f, 100.0f);
    TEST_ASSERT_EQUAL_UINT8(50, r.progressPct());
}

static void test_clip_plan_to_profile_max(void) {
    SweepPlan p = SweepRunner::planFor(16, 10, 10, 60.0f, false);
    TEST_ASSERT_EQUAL_UINT8(9, p.count);  // 1..16 step 2-ish full list
    SweepRunner::clipPlanToMax(p, 80);     // Stufe 8.0
    TEST_ASSERT_EQUAL_UINT8(5, p.count);  // 1,2,4,6,8
    TEST_ASSERT_EQUAL_INT16(80, p.levels[p.count - 1]);
    SweepRunner::clipPlanToMax(p, 0);  // no-op
    TEST_ASSERT_EQUAL_UINT8(5, p.count);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_plan_full);
    RUN_TEST(test_plan_coarse);
    RUN_TEST(test_plan_light);
    RUN_TEST(test_plan_shortens_on_small_device);
    RUN_TEST(test_start_rejects_empty_plan);
    RUN_TEST(test_full_run_collects_every_level);
    RUN_TEST(test_settle_excluded_from_window);
    RUN_TEST(test_window_starts_after_confirmed_write);
    RUN_TEST(test_discards_drifting_cadence);
    RUN_TEST(test_discards_low_cadence);
    RUN_TEST(test_stale_data_is_not_averaged);
    RUN_TEST(test_no_data_at_all_is_no_point);
    RUN_TEST(test_aborts_when_rider_stops);
    RUN_TEST(test_aborts_when_link_dies);
    RUN_TEST(test_brief_dip_does_not_abort);
    RUN_TEST(test_cancel_stops);
    RUN_TEST(test_reset_clears);
    RUN_TEST(test_progress_counts_levels);
    RUN_TEST(test_clip_plan_to_profile_max);
    return UNITY_END();
}
