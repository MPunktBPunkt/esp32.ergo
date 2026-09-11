/**
 * Nativer Test fuer den Limiter.
 *
 *   pio test -e native
 *
 * Der Limiter ist der einzige Pfad zum Control Point. Er ist Arduino-frei
 * gehalten, damit genau der Code getestet wird, der spaeter auch faehrt —
 * inklusive Zeitverhalten, weil die Zeit hereingereicht statt gelesen wird.
 */

#include <stdio.h>
#include <unity.h>

#include "control/Limiter.h"

using namespace ergo;
using namespace ftms;

void setUp(void) {}
void tearDown(void) {}

// ------------------------------------------------------------- Vorlagen

/** Der Varon XTR II: 16 Stufen, kein belastbares Wattziel. */
static Capabilities varonCaps() {
    Capabilities c;
    c.valid = true;
    c.canTargetResistance = true;
    c.canTargetPower = false;
    c.canSimulate = true;
    c.reportsPower = true;
    c.hasResistanceRange = true;
    c.resistance.valid = true;
    c.resistance.minRaw = 10;
    c.resistance.maxRaw = 160;
    c.resistance.stepRaw = 10;
    c.hasPowerRange = false;
    c.powerTargetTrusted = false;
    return c;
}

/** Ein Geraet mit belegtem Wattziel. */
static Capabilities trainerCaps() {
    Capabilities c = varonCaps();
    c.canTargetPower = true;
    c.hasPowerRange = true;
    c.power.valid = true;
    c.power.minW = 0;
    c.power.maxW = 2000;
    c.power.stepW = 1;
    c.powerTargetTrusted = true;
    return c;
}

static Capabilities g_caps;

static Limiter makeLimiter(const Capabilities& caps, LimiterConfig cfg = LimiterConfig{}) {
    g_caps = caps;
    Limiter l;
    l.begin(cfg);
    l.setCapabilities(&g_caps);
    return l;
}

static void assertVerdict(const Limiter::Verdict& v, Decision want, const char* msg) {
    if (v.decision != want) {
        char buf[160];
        snprintf(buf, sizeof(buf), "%s: erwartet %s, bekam %s (%s)", msg, decisionName(want),
                 decisionName(v.decision), v.reason);
        TEST_FAIL_MESSAGE(buf);
    }
}

static void assertBytes(const Limiter::Verdict& v, const uint8_t* want, size_t n,
                        const char* msg) {
    TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)n, (uint32_t)v.len, msg);
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(want, v.data, n, msg);
}

// ------------------------------------------------------------ Whitelist

static void test_whitelist() {
    Limiter l = makeLimiter(varonCaps());
    const uint8_t request[] = {0x00};
    const uint8_t reset[] = {0x01};
    const uint8_t start[] = {0x07};
    const uint8_t speed[] = {0x02, 0x00, 0x00};
    const uint8_t incline[] = {0x03, 0x00, 0x00};
    const uint8_t unsinn[] = {0x99};

    assertVerdict(l.check(request, 1, 0), Decision::Allow, "00");
    assertVerdict(l.check(reset, 1, 0), Decision::Allow, "01");
    assertVerdict(l.check(start, 1, 0), Decision::Allow, "07");
    assertVerdict(l.check(speed, 3, 0), Decision::Deny, "02 Zielgeschwindigkeit");
    assertVerdict(l.check(incline, 3, 0), Decision::Deny, "03 Neigung");
    assertVerdict(l.check(unsinn, 1, 0), Decision::Deny, "unbekannter Opcode");
    assertVerdict(l.check(nullptr, 0, 0), Decision::Deny, "leer");
}

/** Pulsziel gibt es nicht — die Fuehrung laeuft als Kaskade ueber Watt. */
static void test_pulsziel_wird_abgelehnt() {
    Limiter l = makeLimiter(trainerCaps());
    const uint8_t hr[] = {0x06, 120};
    assertVerdict(l.check(hr, 2, 0), Decision::Deny, "06");
}

/**
 * Stop ist nie verhandelbar: kein Capability-Check, keine Rampe, keine
 * Klemme — auch dann nicht, wenn ueber das Geraet nichts bekannt ist.
 */
static void test_stop_geht_immer() {
    Limiter l;
    l.begin(LimiterConfig{});  // bewusst ohne setCapabilities
    const uint8_t stop[] = {0x08, 0x01};
    const uint8_t pause[] = {0x08, 0x02};
    const uint8_t murks[] = {0x08, 0x03};

    assertVerdict(l.check(stop, 2, 0), Decision::Allow, "08 01");
    assertVerdict(l.check(pause, 2, 0), Decision::Allow, "08 02");
    assertVerdict(l.check(murks, 2, 0), Decision::Deny, "08 03");
}

// ------------------------------------------------------------- Klemmen

static void test_ohne_faehigkeiten_keine_last() {
    Limiter l;
    l.begin(LimiterConfig{});
    const uint8_t level[] = {0x04, 0x64, 0x00};
    assertVerdict(l.check(level, 3, 0), Decision::Deny, "Stufe ohne Capabilities");
}

static void test_stufe_wird_geklemmt() {
    Limiter l = makeLimiter(varonCaps());
    const uint8_t zuHoch[] = {0x04, 0xC8, 0x00};  // Stufe 20,0
    const uint8_t want[] = {0x04, 0xA0, 0x00};    // auf 16,0
    Limiter::Verdict v = l.check(zuHoch, 3, 0);
    assertVerdict(v, Decision::Clamp, "Stufe 20 auf 16");
    assertBytes(v, want, 3, "Stufe 20 auf 16");

    const uint8_t zuTief[] = {0x04, 0x00, 0x00};
    const uint8_t wantMin[] = {0x04, 0x0A, 0x00};
    v = l.check(zuTief, 3, 0);
    assertVerdict(v, Decision::Clamp, "Stufe 0 auf Minimum");
    assertBytes(v, wantMin, 3, "Stufe 0 auf Minimum");
}

/** Zwischenwerte rasten auf die Schrittweite des Geraets ein. */
static void test_stufe_rastet_ein() {
    Limiter l = makeLimiter(varonCaps());
    const uint8_t krumm[] = {0x04, 0x87, 0x00};  // 13,5
    const uint8_t want[] = {0x04, 0x8C, 0x00};   // 14,0
    Limiter::Verdict v = l.check(krumm, 3, 0);
    assertVerdict(v, Decision::Clamp, "13,5 auf 14,0");
    assertBytes(v, want, 3, "13,5 auf 14,0");
}

static void test_profilgrenze_sticht() {
    LimiterConfig cfg;
    cfg.profileMaxLevelTenths = 100;  // Reha-Profil: hoechstens Stufe 10
    Limiter l = makeLimiter(varonCaps(), cfg);

    TEST_ASSERT_EQUAL_INT16(100, l.effectiveMaxLevelTenths());
    const uint8_t voll[] = {0x04, 0xA0, 0x00};
    const uint8_t want[] = {0x04, 0x64, 0x00};
    Limiter::Verdict v = l.check(voll, 3, 0);
    assertVerdict(v, Decision::Clamp, "Profilgrenze");
    assertBytes(v, want, 3, "Profilgrenze");
}

/** Die schmale Form bleibt schmal, auch nach dem Klemmen. */
static void test_uint8_form_bleibt_uint8() {
    Limiter l = makeLimiter(varonCaps());
    const uint8_t schmal[] = {0x04, 0xC8};  // 20,0 als uint8
    const uint8_t want[] = {0x04, 0xA0};
    Limiter::Verdict v = l.check(schmal, 2, 0);
    assertVerdict(v, Decision::Clamp, "uint8 geklemmt");
    assertBytes(v, want, 2, "uint8 geklemmt");
}

// --------------------------------------------------------------- Rampe

static void test_rampe_nach_oben() {
    Limiter l = makeLimiter(varonCaps());
    const uint8_t auf10[] = {0x04, 0x64, 0x00};
    l.noteWritten(auf10, 3, 0);
    TEST_ASSERT_EQUAL_INT16(100, l.currentLevelTenths());

    const uint8_t auf16[] = {0x04, 0xA0, 0x00};

    // Noch keine zwei Sekunden vorbei: nichts senden, spaeter erneut fragen.
    assertVerdict(l.check(auf16, 3, 1000), Decision::Defer, "1 s nach dem Write");

    // Nach zwei Sekunden genau eine Stufe.
    Limiter::Verdict v = l.check(auf16, 3, 2000);
    const uint8_t erwartet11[] = {0x04, 0x6E, 0x00};
    assertVerdict(v, Decision::Clamp, "2 s -> eine Stufe");
    assertBytes(v, erwartet11, 3, "2 s -> eine Stufe");

    // Nach sechs Sekunden drei.
    v = l.check(auf16, 3, 6000);
    const uint8_t erwartet13[] = {0x04, 0x82, 0x00};
    assertVerdict(v, Decision::Clamp, "6 s -> drei Stufen");
    assertBytes(v, erwartet13, 3, "6 s -> drei Stufen");

    // Wenn genug Zeit vergangen ist, geht das volle Ziel unveraendert durch.
    assertVerdict(l.check(auf16, 3, 30000), Decision::Allow, "30 s -> ganzes Ziel");
}

/**
 * Der wichtigste Fall: nach unten gibt es keine Rampe. Ein Pulsdeckel, der
 * erst in zwei Sekunden greifen darf, waere kein Pulsdeckel.
 */
static void test_runter_geht_sofort() {
    Limiter l = makeLimiter(varonCaps());
    const uint8_t auf16[] = {0x04, 0xA0, 0x00};
    l.noteWritten(auf16, 3, 0);

    const uint8_t auf1[] = {0x04, 0x0A, 0x00};
    assertVerdict(l.check(auf1, 3, 1), Decision::Allow, "sofort runter");

    const uint8_t auf8[] = {0x04, 0x50, 0x00};
    assertVerdict(l.check(auf8, 3, 1), Decision::Allow, "sofort auf 8");
}

// ---------------------------------------------------------------- Watt

/** Am Varon geht kein Wattziel durch — auch wenn er es quittieren wuerde. */
static void test_wattziel_am_varon_abgelehnt() {
    Limiter l = makeLimiter(varonCaps());
    const uint8_t watt[] = {0x05, 0x64, 0x00};
    assertVerdict(l.check(watt, 3, 0), Decision::Deny, "05 am Varon");
}

/** Behauptetes Wattziel ohne 0x2AD8 bekommt ebenfalls keinen Durchlass. */
static void test_wattziel_ohne_bereich_abgelehnt() {
    Capabilities c = varonCaps();
    c.canTargetPower = true;      // gemeldet
    c.powerTargetTrusted = false; // aber ohne 0x2AD8
    Limiter l = makeLimiter(c);
    const uint8_t watt[] = {0x05, 0x64, 0x00};
    assertVerdict(l.check(watt, 3, 0), Decision::Deny, "05 ohne 0x2AD8");
}

static void test_wattziel_am_trainer() {
    Limiter l = makeLimiter(trainerCaps());
    const uint8_t w200[] = {0x05, 0xC8, 0x00};
    assertVerdict(l.check(w200, 3, 0), Decision::Allow, "200 W");

    const uint8_t w800[] = {0x05, 0x20, 0x03};  // 800 W
    const uint8_t want[] = {0x05, 0x58, 0x02};  // auf 600 W
    Limiter::Verdict v = l.check(w800, 3, 0);
    assertVerdict(v, Decision::Clamp, "800 W auf absolute Schranke");
    assertBytes(v, want, 3, "800 W auf absolute Schranke");
}

static void test_wattziel_profilgrenze() {
    LimiterConfig cfg;
    cfg.profileMaxPowerW = 150;
    Limiter l = makeLimiter(trainerCaps(), cfg);
    TEST_ASSERT_EQUAL_INT16(150, l.effectiveMaxPowerW());

    const uint8_t w200[] = {0x05, 0xC8, 0x00};
    const uint8_t want[] = {0x05, 0x96, 0x00};
    Limiter::Verdict v = l.check(w200, 3, 0);
    assertVerdict(v, Decision::Clamp, "Profilgrenze Watt");
    assertBytes(v, want, 3, "Profilgrenze Watt");
}

// --------------------------------------------------------- Simulation

static void test_simulation_ist_gesperrt() {
    Limiter l = makeLimiter(varonCaps());
    const uint8_t sim[] = {0x11, 0x00, 0x00, 0xFA, 0x00, 0x28, 0x33};
    assertVerdict(l.check(sim, 7, 0), Decision::Deny, "0x11 gesperrt");
}

static void test_simulation_mit_steigungsklemme() {
    LimiterConfig cfg;
    cfg.allowSimulation = true;
    cfg.maxGradeHundredth = 800;
    Limiter l = makeLimiter(varonCaps(), cfg);

    const uint8_t sanft[] = {0x11, 0x00, 0x00, 0xFA, 0x00, 0x28, 0x33};  // 2,50 %
    assertVerdict(l.check(sanft, 7, 0), Decision::Allow, "2,5 %");

    const uint8_t steil[] = {0x11, 0x00, 0x00, 0xDC, 0x05, 0x28, 0x33};  // 15,00 %
    const uint8_t want[] = {0x11, 0x00, 0x00, 0x20, 0x03, 0x28, 0x33};   // auf 8,00 %
    Limiter::Verdict v = l.check(steil, 7, 0);
    assertVerdict(v, Decision::Clamp, "15 % auf 8 %");
    assertBytes(v, want, 7, "15 % auf 8 %");
}

// ------------------------------------------------------------- Deadman

static void test_deadman() {
    LimiterConfig cfg;
    cfg.deadmanMs = 5000;
    Limiter l = makeLimiter(varonCaps(), cfg);

    TEST_ASSERT_FALSE(l.armed());
    TEST_ASSERT_FALSE(l.expired(999999));

    const uint8_t level[] = {0x04, 0x64, 0x00};
    l.noteWritten(level, 3, 1000);
    TEST_ASSERT_TRUE(l.armed());
    TEST_ASSERT_FALSE(l.expired(5000));
    TEST_ASSERT_EQUAL_UINT32(3000, l.remainingMs(3000));

    TEST_ASSERT_TRUE(l.expired(6001));

    l.keepalive(6000);
    TEST_ASSERT_FALSE(l.expired(6001));
    TEST_ASSERT_TRUE(l.expired(11001));
}

/** Ein reines Lesekommando schaerft den Deadman nicht. */
static void test_deadman_nur_bei_last() {
    LimiterConfig cfg;
    cfg.deadmanMs = 5000;
    Limiter l = makeLimiter(varonCaps(), cfg);

    const uint8_t request[] = {0x00};
    l.noteWritten(request, 1, 0);
    TEST_ASSERT_FALSE(l.armed());

    const uint8_t start[] = {0x07};
    l.noteWritten(start, 1, 0);
    TEST_ASSERT_FALSE(l.armed());
}

/**
 * Nach Stop ist unbekannt, wo das Geraet steht. Der Schattenwert faellt auf
 * das Minimum, damit der naechste Aufbau von unten rampt statt zu springen.
 */
static void test_stop_setzt_schattenwert_zurueck() {
    LimiterConfig cfg;
    cfg.deadmanMs = 5000;
    Limiter l = makeLimiter(varonCaps(), cfg);

    const uint8_t auf16[] = {0x04, 0xA0, 0x00};
    l.noteWritten(auf16, 3, 0);
    TEST_ASSERT_EQUAL_INT16(160, l.currentLevelTenths());
    TEST_ASSERT_TRUE(l.armed());

    const uint8_t stop[] = {0x08, 0x01};
    l.noteWritten(stop, 2, 1000);
    TEST_ASSERT_EQUAL_INT16(10, l.currentLevelTenths());
    TEST_ASSERT_FALSE(l.armed());

    // Und der Wiederaufbau rampt, statt sofort auf 16 zu springen.
    assertVerdict(l.check(auf16, 3, 1500), Decision::Defer, "nach Stop gerampt");
}

static void test_reset_nach_linkverlust() {
    Limiter l = makeLimiter(varonCaps());
    const uint8_t auf16[] = {0x04, 0xA0, 0x00};
    l.noteWritten(auf16, 3, 0);
    TEST_ASSERT_EQUAL_INT16(160, l.currentLevelTenths());

    l.reset();
    TEST_ASSERT_EQUAL_INT16(-1, l.currentLevelTenths());
    // Ohne Schattenwert greift keine Rampe — das erste Kommando nach einem
    // Reconnect ist die Aufgabe des Controllers, nicht des Limiters.
    assertVerdict(l.check(auf16, 3, 1), Decision::Allow, "erstes Kommando nach Reset");
}

static void test_zaehler() {
    Limiter l = makeLimiter(varonCaps());
    const uint8_t murks[] = {0x99};
    l.check(murks, 1, 0);
    l.check(murks, 1, 0);
    TEST_ASSERT_EQUAL_UINT16(2, l.denyCount());

    const uint8_t start[] = {0x07};
    l.noteWritten(start, 1, 0);
    TEST_ASSERT_EQUAL_UINT16(1, l.writeCount());
}

// ------------------------------------------------------------------ main

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_whitelist);
    RUN_TEST(test_pulsziel_wird_abgelehnt);
    RUN_TEST(test_stop_geht_immer);
    RUN_TEST(test_ohne_faehigkeiten_keine_last);
    RUN_TEST(test_stufe_wird_geklemmt);
    RUN_TEST(test_stufe_rastet_ein);
    RUN_TEST(test_profilgrenze_sticht);
    RUN_TEST(test_uint8_form_bleibt_uint8);
    RUN_TEST(test_rampe_nach_oben);
    RUN_TEST(test_runter_geht_sofort);
    RUN_TEST(test_wattziel_am_varon_abgelehnt);
    RUN_TEST(test_wattziel_ohne_bereich_abgelehnt);
    RUN_TEST(test_wattziel_am_trainer);
    RUN_TEST(test_wattziel_profilgrenze);
    RUN_TEST(test_simulation_ist_gesperrt);
    RUN_TEST(test_simulation_mit_steigungsklemme);
    RUN_TEST(test_deadman);
    RUN_TEST(test_deadman_nur_bei_last);
    RUN_TEST(test_stop_setzt_schattenwert_zurueck);
    RUN_TEST(test_reset_nach_linkverlust);
    RUN_TEST(test_zaehler);
    return UNITY_END();
}
