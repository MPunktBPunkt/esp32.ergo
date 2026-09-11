/**
 * Nativer Test fuer ftms::FtmsCodec.
 *
 *   pio test -e native
 *
 * Prueft drei Dinge:
 *   1. Die aufgezeichneten Pakete des Varon XTR II werden korrekt zerlegt.
 *   2. Die Feldkombinationen, die dieses Geraet nie sendet, ebenso — sonst
 *      haette man einen Varon-Decoder statt eines FTMS-Decoders.
 *   3. Die belegten Eigenheiten des Geraets bleiben belegt: kein Wattziel,
 *      kein Pulsziel, 16 Stufen, Erfolgsquittung ohne Aussagekraft.
 */

#include <string.h>
#include <unity.h>

#include "ble/FtmsCapabilities.h"
#include "ble/FtmsCodec.h"
#include "fixtures_ibd.h"
#include "fixtures_synth.h"

using namespace ftms;

void setUp(void) {}
void tearDown(void) {}

// --------------------------------------------------------------- Helfer

static void checkFixture(const IbdFixture& f) {
    IndoorBikeData d;
    const IbdStatus st = decodeIndoorBikeData(f.data, f.len, d);

    TEST_ASSERT_EQUAL_INT_MESSAGE((int)IbdStatus::Ok, (int)st, f.note);
    TEST_ASSERT_EQUAL_HEX16_MESSAGE(f.flags, d.flags, f.note);
    TEST_ASSERT_EQUAL_HEX16_MESSAGE(f.presence, d.presence, f.note);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(f.consumed, d.consumed, f.note);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(f.trailing, d.trailing, f.note);

    TEST_ASSERT_EQUAL_UINT16_MESSAGE(f.speedRaw, d.speedRaw, f.note);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(f.avgSpeedRaw, d.avgSpeedRaw, f.note);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(f.cadenceRaw, d.cadenceRaw, f.note);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(f.avgCadenceRaw, d.avgCadenceRaw, f.note);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(f.distanceM, d.distanceM, f.note);
    TEST_ASSERT_EQUAL_INT16_MESSAGE(f.resistanceRaw, d.resistanceRaw, f.note);
    TEST_ASSERT_EQUAL_INT16_MESSAGE(f.powerW, d.powerW, f.note);
    TEST_ASSERT_EQUAL_INT16_MESSAGE(f.avgPowerW, d.avgPowerW, f.note);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(f.energyTotalKcal, d.energyTotalKcal, f.note);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(f.energyPerHourKcal, d.energyPerHourKcal, f.note);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(f.energyPerMinKcal, d.energyPerMinKcal, f.note);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(f.heartRateBpm, d.heartRateBpm, f.note);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(f.metRaw, d.metRaw, f.note);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(f.elapsedS, d.elapsedS, f.note);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(f.remainingS, d.remainingS, f.note);
}

// --------------------------------------------------------- Indoor Bike

static void test_device_packets() {
    for (size_t i = 0; i < kIbdFixtureCount; ++i) checkFixture(kIbdFixtures[i]);
}

static void test_synthetic_packets() {
    for (size_t i = 0; i < kSynthFixtureCount; ++i) checkFixture(kSynthFixtures[i]);
}

/** Die Falle: Bit 0 gesetzt heisst "kein Speed", nicht "Speed". */
static void test_speed_flag_is_inverted() {
    const uint8_t withSpeed[] = {0x00, 0x00, 0xE8, 0x03};
    const uint8_t without[] = {0x01, 0x00, 0xE8, 0x03};
    IndoorBikeData a, b;

    TEST_ASSERT_EQUAL_INT((int)IbdStatus::Ok, (int)decodeIndoorBikeData(withSpeed, sizeof(withSpeed), a));
    TEST_ASSERT_TRUE(a.has(kSpeed));
    TEST_ASSERT_EQUAL_UINT16(1000, a.speedRaw);

    TEST_ASSERT_EQUAL_INT((int)IbdStatus::Ok, (int)decodeIndoorBikeData(without, sizeof(without), b));
    TEST_ASSERT_FALSE(b.has(kSpeed));
    TEST_ASSERT_EQUAL_UINT16(0, b.speedRaw);
    TEST_ASSERT_EQUAL_UINT8(2, b.trailing);  // die zwei Bytes bleiben uebrig
}

/** Total Distance ist uint24, nicht uint32 — der zweite Klassiker. */
static void test_distance_is_uint24() {
    const uint8_t p[] = {0x11, 0x00, 0xFF, 0xFF, 0xFF};  // Bit0 gesetzt, Bit4 Distance
    IndoorBikeData d;
    TEST_ASSERT_EQUAL_INT((int)IbdStatus::Ok, (int)decodeIndoorBikeData(p, sizeof(p), d));
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFu, d.distanceM);
    TEST_ASSERT_EQUAL_UINT8(5, d.consumed);
    TEST_ASSERT_EQUAL_UINT8(0, d.trailing);
}

static void test_skalierung() {
    IndoorBikeData d;
    TEST_ASSERT_EQUAL_INT(
        (int)IbdStatus::Ok,
        (int)decodeIndoorBikeData(kIbdFixtures[4].data, kIbdFixtures[4].len, d));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 22.00f, d.speedKmh());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 59.0f, d.cadenceRpm());
    TEST_ASSERT_EQUAL_INT16(108, d.powerW);
}

static void test_zu_kurz() {
    IndoorBikeData d;
    const uint8_t one[] = {0x00};
    TEST_ASSERT_EQUAL_INT((int)IbdStatus::TooShort, (int)decodeIndoorBikeData(nullptr, 0, d));
    TEST_ASSERT_EQUAL_INT((int)IbdStatus::TooShort, (int)decodeIndoorBikeData(one, 1, d));
}

static void test_abgeschnittenes_feld() {
    // Flags versprechen Cadence und Power, das Paket liefert nur Cadence.
    const uint8_t p[] = {0x45, 0x00, 0xB4, 0x00};
    IndoorBikeData d;
    TEST_ASSERT_EQUAL_INT((int)IbdStatus::FieldTruncated, (int)decodeIndoorBikeData(p, sizeof(p), d));
    TEST_ASSERT_EQUAL_HEX16(kPower, d.truncatedField);
    TEST_ASSERT_EQUAL_UINT8(4, d.consumed);
    // Was vor dem Abbruch gelesen wurde, bleibt gueltig und markiert.
    TEST_ASSERT_TRUE(d.has(kCadence));
    TEST_ASSERT_EQUAL_UINT16(180, d.cadenceRaw);
    TEST_ASSERT_FALSE(d.has(kPower));
}

// ------------------------------------------------------ Geraeteprofil

/**
 * 0x2ACC des Varon XTR II, gelesen am 2026-09-10: A646000004200000.
 * Dieser Test haelt die vier Befunde fest, auf denen die gesamte
 * Steuerungsarchitektur steht.
 */
static void test_feature_des_geraets() {
    const uint8_t raw[] = {0xA6, 0x46, 0x00, 0x00, 0x04, 0x20, 0x00, 0x00};
    FeatureSet f;
    TEST_ASSERT_TRUE(decodeFeature(raw, sizeof(raw), f));
    TEST_ASSERT_EQUAL_HEX32(0x000046A6u, f.machine);
    TEST_ASSERT_EQUAL_HEX32(0x00002004u, f.target);

    TEST_ASSERT_TRUE(f.hasMachine(kFeatCadence));
    TEST_ASSERT_TRUE(f.hasMachine(kFeatTotalDistance));
    TEST_ASSERT_TRUE(f.hasMachine(kFeatResistanceLevel));
    TEST_ASSERT_TRUE(f.hasMachine(kFeatHeartRate));
    TEST_ASSERT_TRUE(f.hasMachine(kFeatPowerMeasurement));

    // Der Befund, der ERG zur Emulation macht:
    TEST_ASSERT_TRUE(f.hasTarget(kTgtResistance));
    TEST_ASSERT_FALSE(f.hasTarget(kTgtPower));
    TEST_ASSERT_FALSE(f.hasTarget(kTgtHeartRate));
    TEST_ASSERT_TRUE(f.hasTarget(kTgtIndoorBikeSimulation));
}

/** 0x2AD6 des Geraets: 0A00A0000A00 — Stufe 1,0 bis 16,0 in Zehnteln. */
static void test_stufenbereich_des_geraets() {
    const uint8_t raw[] = {0x0A, 0x00, 0xA0, 0x00, 0x0A, 0x00};
    ResistanceRange r;
    TEST_ASSERT_TRUE(decodeResistanceRange(raw, sizeof(raw), r));
    TEST_ASSERT_EQUAL_INT16(10, r.minRaw);
    TEST_ASSERT_EQUAL_INT16(160, r.maxRaw);
    TEST_ASSERT_EQUAL_UINT16(10, r.stepRaw);
    TEST_ASSERT_EQUAL_UINT16(16, r.levelCount());
}

static void test_kurze_ranges_werden_abgelehnt() {
    const uint8_t kurz[] = {0x0A, 0x00, 0xA0};
    ResistanceRange r;
    PowerRange p;
    TEST_ASSERT_FALSE(decodeResistanceRange(kurz, sizeof(kurz), r));
    TEST_ASSERT_FALSE(r.valid);
    TEST_ASSERT_EQUAL_UINT16(0, r.levelCount());
    // 0x2AD8 fehlt bei diesem Geraet ganz — der Aufrufer bekommt nie Bytes.
    TEST_ASSERT_FALSE(decodePowerRange(nullptr, 0, p));
    TEST_ASSERT_FALSE(p.valid);
}

// ------------------------------------------------------- Control Point

static void assertBytes(const uint8_t* got, size_t n, const uint8_t* want, size_t wantN,
                        const char* msg) {
    TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)wantN, (uint32_t)n, msg);
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(want, got, wantN, msg);
}

static void test_encoder_gegen_aufzeichnung() {
    uint8_t buf[kMaxControlLen];

    const uint8_t wantRequest[] = {0x00};
    assertBytes(buf, encodeRequestControl(buf, sizeof(buf)), wantRequest, 1, "00");

    const uint8_t wantStart[] = {0x07};
    assertBytes(buf, encodeStartResume(buf, sizeof(buf)), wantStart, 1, "07");

    const uint8_t wantStop[] = {0x08, 0x01};
    assertBytes(buf, encodeStop(buf, sizeof(buf)), wantStop, 2, "0801");

    const uint8_t wantPause[] = {0x08, 0x02};
    assertBytes(buf, encodePause(buf, sizeof(buf)), wantPause, 2, "0802");

    // Stufe 10,0 in der Form, die gewirkt hat.
    const uint8_t wantLevelWide[] = {0x04, 0x64, 0x00};
    assertBytes(buf, encodeSetTargetResistance(buf, sizeof(buf), 100), wantLevelWide, 3,
                "046400");

    // Und in der Form, die nur quittiert wurde.
    const uint8_t wantLevelNarrow[] = {0x04, 0x0A};
    assertBytes(buf, encodeSetTargetResistance(buf, sizeof(buf), 10, false), wantLevelNarrow, 2,
                "040A");

    const uint8_t wantPower[] = {0x05, 0x64, 0x00};
    assertBytes(buf, encodeSetTargetPower(buf, sizeof(buf), 100), wantPower, 3, "056400");
}

static void test_negative_stufe_bleibt_sint16() {
    uint8_t buf[kMaxControlLen];
    const uint8_t want[] = {0x04, 0xF6, 0xFF};  // -10
    assertBytes(buf, encodeSetTargetResistance(buf, sizeof(buf), -10), want, 3, "-1.0");
    // Die schmale Form kann das nicht und verweigert, statt still zu kappen.
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)encodeSetTargetResistance(buf, sizeof(buf), -10, false));
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)encodeSetTargetResistance(buf, sizeof(buf), 300, false));
}

static void test_simulation() {
    uint8_t buf[kMaxControlLen];
    // 0 mm/s Wind, 2,50 % Steigung, Crr 0,0040, Cw 0,51
    const uint8_t want[] = {0x11, 0x00, 0x00, 0xFA, 0x00, 0x28, 0x33};
    assertBytes(buf, encodeIndoorBikeSimulation(buf, sizeof(buf), 0, 250, 40, 51), want, 7,
                "0x11");
}

static void test_zu_kleiner_puffer() {
    uint8_t buf[2];
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)encodeSetTargetResistance(buf, sizeof(buf), 100));
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)encodeIndoorBikeSimulation(buf, sizeof(buf), 0, 0, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)encodeStop(buf, sizeof(buf)));
}

/**
 * Erfolgsquittung ohne Aussagekraft: das Geraet antwortet auf `05 64 00`
 * mit Success, obwohl Target-Bit 3 geloescht ist und 0x2AD8 fehlt. Der
 * Decoder gibt genau das wieder — die Bewertung gehoert nicht hierher.
 */
static void test_control_response() {
    const uint8_t okRequest[] = {0x80, 0x00, 0x01};
    const uint8_t okPower[] = {0x80, 0x05, 0x01};
    const uint8_t notSupported[] = {0x80, 0x11, 0x02};
    ControlResponse r;

    TEST_ASSERT_TRUE(decodeControlResponse(okRequest, sizeof(okRequest), r));
    TEST_ASSERT_EQUAL_INT((int)Opcode::RequestControl, (int)r.request);
    TEST_ASSERT_TRUE(r.ok());

    TEST_ASSERT_TRUE(decodeControlResponse(okPower, sizeof(okPower), r));
    TEST_ASSERT_EQUAL_INT((int)Opcode::SetTargetPower, (int)r.request);
    TEST_ASSERT_TRUE(r.ok());

    TEST_ASSERT_TRUE(decodeControlResponse(notSupported, sizeof(notSupported), r));
    TEST_ASSERT_EQUAL_INT((int)Opcode::SetIndoorBikeSimulation, (int)r.request);
    TEST_ASSERT_EQUAL_INT((int)ControlResult::NotSupported, (int)r.result);
    TEST_ASSERT_FALSE(r.ok());
}

static void test_control_response_muell() {
    const uint8_t kurz[] = {0x80, 0x05};
    const uint8_t keinResponse[] = {0x05, 0x64, 0x00};
    ControlResponse r;
    TEST_ASSERT_FALSE(decodeControlResponse(kurz, sizeof(kurz), r));
    TEST_ASSERT_FALSE(decodeControlResponse(keinResponse, sizeof(keinResponse), r));
    TEST_ASSERT_FALSE(r.valid);
}

// ------------------------------------------------------- Faehigkeiten
//
// Diese Gruppe haelt fest, dass nichts oberhalb des Clients weiss, welches
// Ergometer angeschlossen ist. Jeder Fall unten ist ein Geraetetyp, der ohne
// Reflash laufen muss.

static FeatureSet featureOf(uint32_t machine, uint32_t target) {
    uint8_t raw[8] = {(uint8_t)(machine), (uint8_t)(machine >> 8), (uint8_t)(machine >> 16),
                      (uint8_t)(machine >> 24), (uint8_t)(target), (uint8_t)(target >> 8),
                      (uint8_t)(target >> 16), (uint8_t)(target >> 24)};
    FeatureSet f;
    decodeFeature(raw, sizeof(raw), f);
    return f;
}

static ResistanceRange rangeOf(int16_t lo, int16_t hi, uint16_t step) {
    ResistanceRange r;
    r.valid = true;
    r.minRaw = lo;
    r.maxRaw = hi;
    r.stepRaw = step;
    return r;
}

/** Der Varon XTR II: meldet kein Wattziel und liefert keine 0x2AD8. */
static void test_caps_varon() {
    const FeatureSet f = featureOf(0x000046A6u, 0x00002004u);
    const ResistanceRange r = rangeOf(10, 160, 10);
    const Capabilities c = deriveCapabilities(f, &r, nullptr);

    TEST_ASSERT_TRUE(c.valid);
    TEST_ASSERT_TRUE(c.canTargetResistance);
    TEST_ASSERT_FALSE(c.canTargetPower);
    TEST_ASSERT_FALSE(c.powerTargetTrusted);
    TEST_ASSERT_TRUE(c.canSimulate);
    TEST_ASSERT_EQUAL_INT((int)PowerStrategy::EmulateResistance, (int)c.powerStrategy());
    TEST_ASSERT_EQUAL_UINT16(16, c.levelCount());
    // Spec-Treue: sint16 ist Standard. Der Bereich passt in uint8 — genau
    // deshalb ging frueher die wirkungslose schmale Form hinaus (siehe
    // test_caps / UPDATE_CAPS_FIX.md).
    TEST_ASSERT_TRUE(c.needsWideResistance());
    TEST_ASSERT_EQUAL_INT((int)ResistanceFormat::Sint16, (int)c.resistanceFormat);
}

/** Ein echter Smarttrainer: Wattziel gemeldet UND Bereich veroeffentlicht. */
static void test_caps_smarttrainer() {
    const FeatureSet f = featureOf(kFeatCadence | kFeatPowerMeasurement,
                                   kTgtResistance | kTgtPower | kTgtIndoorBikeSimulation);
    const ResistanceRange r = rangeOf(0, 100, 1);
    PowerRange p;
    p.valid = true;
    p.minW = 0;
    p.maxW = 2000;
    p.stepW = 1;

    const Capabilities c = deriveCapabilities(f, &r, &p);
    TEST_ASSERT_TRUE(c.powerTargetTrusted);
    TEST_ASSERT_EQUAL_INT((int)PowerStrategy::DirectTarget, (int)c.powerStrategy());
}

/**
 * Der gefaehrliche Zwischenfall: ein Geraet behauptet das Wattziel, liefert
 * aber keinen Wattbereich. Ohne 0x2AD8 gibt es keinen Vertrauensvorschuss —
 * es faellt auf Emulation zurueck statt blind Watt zu schreiben.
 */
static void test_caps_behauptetes_wattziel() {
    const FeatureSet f = featureOf(kFeatPowerMeasurement, kTgtResistance | kTgtPower);
    const ResistanceRange r = rangeOf(10, 160, 10);
    const Capabilities c = deriveCapabilities(f, &r, nullptr);

    TEST_ASSERT_TRUE(c.canTargetPower);
    TEST_ASSERT_FALSE(c.powerTargetTrusted);
    TEST_ASSERT_EQUAL_INT((int)PowerStrategy::EmulateResistance, (int)c.powerStrategy());
}

/** Feinere Skala als 25,5: die schmale Form scheidet rechnerisch aus. */
static void test_caps_grosse_skala() {
    const FeatureSet f = featureOf(kFeatPowerMeasurement, kTgtResistance);
    const ResistanceRange r = rangeOf(0, 1000, 5);
    const Capabilities c = deriveCapabilities(f, &r, nullptr);

    TEST_ASSERT_TRUE(c.needsWideResistance());
    TEST_ASSERT_EQUAL_INT((int)ResistanceFormat::Sint16, (int)c.resistanceFormat);
    TEST_ASSERT_EQUAL_UINT16(201, c.levelCount());
}

/** Weder Wattziel noch Stufenbereich: reines Dashboard, kein Stellweg. */
static void test_caps_ohne_stellweg() {
    const FeatureSet f = featureOf(kFeatCadence | kFeatPowerMeasurement, 0);
    const Capabilities c = deriveCapabilities(f, nullptr, nullptr);
    TEST_ASSERT_TRUE(c.valid);
    TEST_ASSERT_EQUAL_INT((int)PowerStrategy::None, (int)c.powerStrategy());
    TEST_ASSERT_EQUAL_UINT16(0, c.levelCount());

    FeatureSet kaputt;  // valid == false
    const Capabilities none = deriveCapabilities(kaputt, nullptr, nullptr);
    TEST_ASSERT_FALSE(none.valid);
    TEST_ASSERT_EQUAL_INT((int)PowerStrategy::None, (int)none.powerStrategy());
}

/** Was erst am Datenstrom sichtbar wird. */
static void test_caps_aus_datenstrom() {
    const FeatureSet f = featureOf(0x000046A6u, 0x00002004u);
    const ResistanceRange r = rangeOf(10, 160, 10);
    Capabilities c = deriveCapabilities(f, &r, nullptr);
    IndoorBikeData d;

    // Varon-Paket: kein Resistance-Feld, die Stufe bleibt ein Schattenwert.
    decodeIndoorBikeData(kIbdFixtures[0].data, kIbdFixtures[0].len, d);
    noteIndoorBikeData(c, d);
    TEST_ASSERT_TRUE(c.sawIndoorBikeData);
    TEST_ASSERT_EQUAL_HEX16(0x0B54, c.observedIbdFlags);
    TEST_ASSERT_FALSE(c.ibdReportsResistance);
    TEST_ASSERT_TRUE(c.ibdReportsPower);
    TEST_ASSERT_TRUE(c.ibdReportsCadence);

    // Ein Geraet, das die Stufe zurueckmeldet, macht aus dem Schattenwert
    // eine Messung — kSynthFixtures[2] enthaelt das Feld.
    decodeIndoorBikeData(kSynthFixtures[2].data, kSynthFixtures[2].len, d);
    noteIndoorBikeData(c, d);
    TEST_ASSERT_TRUE(c.ibdReportsResistance);
    TEST_ASSERT_FALSE(c.ibdReportsCadence);
}

// ------------------------------------------------------------------ main

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_device_packets);
    RUN_TEST(test_synthetic_packets);
    RUN_TEST(test_speed_flag_is_inverted);
    RUN_TEST(test_distance_is_uint24);
    RUN_TEST(test_skalierung);
    RUN_TEST(test_zu_kurz);
    RUN_TEST(test_abgeschnittenes_feld);
    RUN_TEST(test_feature_des_geraets);
    RUN_TEST(test_stufenbereich_des_geraets);
    RUN_TEST(test_kurze_ranges_werden_abgelehnt);
    RUN_TEST(test_encoder_gegen_aufzeichnung);
    RUN_TEST(test_negative_stufe_bleibt_sint16);
    RUN_TEST(test_simulation);
    RUN_TEST(test_zu_kleiner_puffer);
    RUN_TEST(test_control_response);
    RUN_TEST(test_control_response_muell);
    RUN_TEST(test_caps_varon);
    RUN_TEST(test_caps_smarttrainer);
    RUN_TEST(test_caps_behauptetes_wattziel);
    RUN_TEST(test_caps_grosse_skala);
    RUN_TEST(test_caps_ohne_stellweg);
    RUN_TEST(test_caps_aus_datenstrom);
    return UNITY_END();
}
