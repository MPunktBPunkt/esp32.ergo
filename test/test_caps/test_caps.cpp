/**
 * Nativer Test der Capability-Ableitung.
 *
 *   pio test -e native
 *
 * Diese Suite existiert wegen eines Fehlers, der genau hier hingehoert hätte
 * und nicht da war: `needsWideResistance()` entschied das Drahtformat von
 * `0x04` aus dem Stellbereich. Beim Varon passt der Bereich in ein uint8,
 * also ging die schmale Form hinaus — das Gerät quittierte sie mit Success
 * und tat nichts. Der Hardware-Test sah lauter Erfolgsquittungen und keine
 * Wirkung.
 *
 * Die Lehre steht im Pflichtenheft und ist hier festgenagelt: eine
 * Erfolgsquittung beweist nichts, und ein Stellbereich beantwortet keine
 * Formatfrage. Wer diese Datei ändert, ändert das Verhalten am Gerät.
 */

#include <stdio.h>
#include <unity.h>

#include "ble/FtmsCapabilities.h"

using namespace ftms;

void setUp(void) {}
void tearDown(void) {}

// ───────────────────────────────────────────────────────────── Vorlagen

/** 0x2ACC des Varon XTR II, gemessen: `A646000004200000`. */
static FeatureSet varonFeatures() {
    FeatureSet f;
    f.valid = true;
    f.machine = 0x000046A6;  // Kadenz, Distanz, Energie, Puls, Zeit, Leistung
    f.target = 0x00002004;   // Resistance + IndoorBikeSimulation, kein Power
    return f;
}

/** 0x2AD6 des Varon, gemessen: `0A00A0000A00` — 10..160, Schritt 10. */
static ResistanceRange varonResistance() {
    ResistanceRange r;
    r.valid = true;
    r.minRaw = 10;
    r.maxRaw = 160;
    r.stepRaw = 10;
    return r;
}

static Capabilities varon() {
    const FeatureSet f = varonFeatures();
    const ResistanceRange r = varonResistance();
    // Kein 0x2AD8 — das ist der Normalfall bei diesem Gerät.
    return deriveCapabilities(f, &r, nullptr);
}

// ─────────────────────────────────── Der Fehler, der das hier ausgelöst hat

/**
 * Der Kern: ein Stellbereich, der in ein uint8 passt, darf die schmale Form
 * NICHT auslösen. 10 bis 160 passt bequem — und trotzdem muss sint16 hinaus.
 */
static void test_varon_needs_wide_despite_small_range(void) {
    const Capabilities c = varon();
    TEST_ASSERT_TRUE(c.valid);
    TEST_ASSERT_TRUE(c.canTargetResistance);
    // Der Bereich passt in ein uint8 …
    TEST_ASSERT_TRUE(c.resistance.maxRaw <= 255);
    TEST_ASSERT_TRUE(c.resistance.minRaw >= 0);
    // … und genau deshalb ging früher die wirkungslose Form hinaus.
    TEST_ASSERT_TRUE(c.needsWideResistance());
    TEST_ASSERT_EQUAL_INT((int)ResistanceFormat::Sint16, (int)c.resistanceFormat);
}

/** Spec-Treue ist der Standard, auch ohne jedes Vorwissen über das Gerät. */
static void test_unknown_format_defaults_to_wide(void) {
    Capabilities c;
    c.valid = true;
    c.canTargetResistance = true;
    c.hasResistanceRange = true;
    c.resistance = varonResistance();
    c.resistanceFormat = ResistanceFormat::Unknown;
    TEST_ASSERT_TRUE(c.needsWideResistance());
}

/** Die schmale Form gibt es nur auf ausdrückliche Ansage aus dem Profil. */
static void test_uint8_profile_uses_narrow(void) {
    Capabilities c = varon();
    c.resistanceFormat = ResistanceFormat::Uint8;
    TEST_ASSERT_FALSE(c.needsWideResistance());
}

/** Und selbst dann nicht, wenn der Wert gar nicht hineinpasst. */
static void test_uint8_profile_overridden_by_range(void) {
    Capabilities c = varon();
    c.resistanceFormat = ResistanceFormat::Uint8;
    c.resistance.maxRaw = 400;  // Stufe 40,0 passt nicht in ein uint8
    TEST_ASSERT_TRUE(c.needsWideResistance());

    c.resistance.maxRaw = 160;
    c.resistance.minRaw = -50;  // negativ geht in uint8 ebenfalls nicht
    TEST_ASSERT_TRUE(c.needsWideResistance());
}

// ────────────────────────────────────────────────── Wattziel und Vertrauen

/**
 * Der Varon meldet kein Wattziel und veröffentlicht keinen Wattbereich —
 * obwohl er `05` mit Success quittiert. Also kein Vertrauen.
 */
static void test_varon_power_target_not_trusted(void) {
    const Capabilities c = varon();
    TEST_ASSERT_FALSE(c.canTargetPower);
    TEST_ASSERT_FALSE(c.hasPowerRange);
    TEST_ASSERT_FALSE(c.powerTargetTrusted);
    TEST_ASSERT_EQUAL_INT((int)PowerStrategy::EmulateResistance, (int)c.powerStrategy());
}

/** Feature-Bit allein reicht nicht: ohne 0x2AD8 kein Vertrauensvorschuss. */
static void test_power_feature_without_range_is_not_trusted(void) {
    FeatureSet f = varonFeatures();
    f.target |= kTgtPower;
    const ResistanceRange r = varonResistance();
    const Capabilities c = deriveCapabilities(f, &r, nullptr);
    TEST_ASSERT_TRUE(c.canTargetPower);
    TEST_ASSERT_FALSE(c.powerTargetTrusted);
    TEST_ASSERT_EQUAL_INT((int)PowerStrategy::EmulateResistance, (int)c.powerStrategy());
}

/** Melden UND veröffentlichen: dann wird direkt gestellt. */
static void test_power_feature_with_range_is_trusted(void) {
    FeatureSet f = varonFeatures();
    f.target |= kTgtPower;
    const ResistanceRange r = varonResistance();
    PowerRange p;
    p.valid = true;
    p.minW = 0;
    p.maxW = 2000;
    p.stepW = 1;
    const Capabilities c = deriveCapabilities(f, &r, &p);
    TEST_ASSERT_TRUE(c.powerTargetTrusted);
    TEST_ASSERT_EQUAL_INT((int)PowerStrategy::DirectTarget, (int)c.powerStrategy());
}

/** Ohne Stellweg ist das Gerät ein Dashboard, kein Ergometer. */
static void test_no_ranges_is_dashboard(void) {
    FeatureSet f = varonFeatures();
    f.target = 0;
    const Capabilities c = deriveCapabilities(f, nullptr, nullptr);
    TEST_ASSERT_TRUE(c.valid);
    TEST_ASSERT_FALSE(c.hasResistanceRange);
    TEST_ASSERT_EQUAL_UINT16(0, c.levelCount());
    TEST_ASSERT_EQUAL_INT((int)PowerStrategy::None, (int)c.powerStrategy());
    TEST_ASSERT_EQUAL_INT((int)ResistanceFormat::Unknown, (int)c.resistanceFormat);
}

static void test_invalid_feature_yields_nothing(void) {
    FeatureSet f;  // valid = false
    const ResistanceRange r = varonResistance();
    const Capabilities c = deriveCapabilities(f, &r, nullptr);
    TEST_ASSERT_FALSE(c.valid);
    TEST_ASSERT_EQUAL_INT((int)PowerStrategy::None, (int)c.powerStrategy());
}

// ───────────────────────────────────────────────────────────── Stellweg

static void test_level_arithmetic(void) {
    const Capabilities c = varon();
    TEST_ASSERT_EQUAL_UINT16(16, c.levelCount());
    TEST_ASSERT_EQUAL_INT(10, c.levelMinTenths());
    TEST_ASSERT_EQUAL_INT(160, c.levelMaxTenths());
    TEST_ASSERT_EQUAL_UINT16(10, c.levelStepTenths());
}

static void test_level_count_handles_junk_range(void) {
    ResistanceRange r;
    r.valid = true;
    r.minRaw = 10;
    r.maxRaw = 160;
    r.stepRaw = 0;  // Schrittweite null wäre eine Division durch null
    const FeatureSet f = varonFeatures();
    const Capabilities c = deriveCapabilities(f, &r, nullptr);
    TEST_ASSERT_EQUAL_UINT16(0, c.levelCount());

    ResistanceRange bad = varonResistance();
    bad.maxRaw = 5;  // Maximum unter dem Minimum
    const Capabilities c2 = deriveCapabilities(f, &bad, nullptr);
    TEST_ASSERT_EQUAL_UINT16(0, c2.levelCount());
}

/** Ein ungültiger Bereich darf nicht als Bereich durchgehen. */
static void test_invalid_range_is_ignored(void) {
    ResistanceRange r = varonResistance();
    r.valid = false;
    const FeatureSet f = varonFeatures();
    const Capabilities c = deriveCapabilities(f, &r, nullptr);
    TEST_ASSERT_FALSE(c.hasResistanceRange);
    TEST_ASSERT_EQUAL_INT((int)PowerStrategy::None, (int)c.powerStrategy());
}

// ──────────────────────────────────────────── Was erst der Strom zeigt

static void test_observed_flags(void) {
    Capabilities c = varon();
    TEST_ASSERT_FALSE(c.sawIndoorBikeData);

    IndoorBikeData d;
    d.flags = 0x0044;
    d.presence = kCadence | kPower;
    d.powerW = 108;
    d.cadenceRaw = 118;
    noteIndoorBikeData(c, d);

    TEST_ASSERT_TRUE(c.sawIndoorBikeData);
    TEST_ASSERT_EQUAL_UINT16(0x0044, c.observedIbdFlags);
    TEST_ASSERT_TRUE(c.ibdReportsCadence);
    TEST_ASSERT_TRUE(c.ibdReportsPower);
    // Die Stufe meldet der Varon nicht zurück — deshalb ist sie in der UI ein
    // Schattenwert und wird mit `~` gekennzeichnet.
    TEST_ASSERT_FALSE(c.ibdReportsResistance);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_varon_needs_wide_despite_small_range);
    RUN_TEST(test_unknown_format_defaults_to_wide);
    RUN_TEST(test_uint8_profile_uses_narrow);
    RUN_TEST(test_uint8_profile_overridden_by_range);
    RUN_TEST(test_varon_power_target_not_trusted);
    RUN_TEST(test_power_feature_without_range_is_not_trusted);
    RUN_TEST(test_power_feature_with_range_is_trusted);
    RUN_TEST(test_no_ranges_is_dashboard);
    RUN_TEST(test_invalid_feature_yields_nothing);
    RUN_TEST(test_level_arithmetic);
    RUN_TEST(test_level_count_handles_junk_range);
    RUN_TEST(test_invalid_range_is_ignored);
    RUN_TEST(test_observed_flags);
    return UNITY_END();
}
