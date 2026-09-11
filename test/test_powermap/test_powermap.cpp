/**
 * Nativer Test der Kennflaeche.
 *
 *   pio test -e native
 *
 * PowerMap ist Arduino-frei, damit genau der Code geprueft wird, der spaeter
 * auch faehrt. Getestet wird vor allem das, was auf dem Geraet niemand mehr
 * sieht: Interpolation ueber leere Zellen, die Gewichtung von Sweep gegen
 * passives Lernen und der Byte-Roundtrip, mit dem die Flaeche einen Neustart
 * ueberlebt.
 */

#include <stdio.h>
#include <unity.h>

#include "control/PowerMap.h"

using namespace ergo;

/** Der Varon XTR II: 16 Stufen, 10 bis 160 in Zehnteln, Schritt 10. */
static PowerMap varonMap() {
    PowerMap m;
    m.begin(16, 10, 10);
    return m;
}

void setUp(void) {}
void tearDown(void) {}

// ─────────────────────────────────────────────────────── Grundrechnung

static void test_bands(void) {
    TEST_ASSERT_EQUAL_INT(-1, PowerMap::bandOf(39.9f));
    TEST_ASSERT_EQUAL_INT(0, PowerMap::bandOf(40.0f));
    TEST_ASSERT_EQUAL_INT(0, PowerMap::bandOf(49.9f));
    TEST_ASSERT_EQUAL_INT(2, PowerMap::bandOf(60.0f));
    TEST_ASSERT_EQUAL_INT(7, PowerMap::bandOf(119.0f));
    TEST_ASSERT_EQUAL_INT(-1, PowerMap::bandOf(120.0f));
    TEST_ASSERT_EQUAL_FLOAT(65.0f, PowerMap::bandCenter(2));
}

static void test_level_index(void) {
    PowerMap m = varonMap();
    TEST_ASSERT_EQUAL_INT(0, m.indexOf(10));
    TEST_ASSERT_EQUAL_INT(15, m.indexOf(160));
    TEST_ASSERT_EQUAL_INT(-1, m.indexOf(170));  // ueber dem Bereich
    TEST_ASSERT_EQUAL_INT(-1, m.indexOf(0));    // unter dem Bereich
    TEST_ASSERT_EQUAL_INT(-1, m.indexOf(15));   // nicht auf dem Raster
    TEST_ASSERT_EQUAL_INT(10, m.tenthsOf(0));
    TEST_ASSERT_EQUAL_INT(160, m.tenthsOf(15));
}

static void test_rejects_junk(void) {
    PowerMap m = varonMap();
    // Kadenz unter dem kleinsten Band: Freilauf, kein Messpunkt
    TEST_ASSERT_FALSE(m.add(100, 20.0f, 90.0f, 1, true));
    // Stufe nicht auf dem Raster
    TEST_ASSERT_FALSE(m.add(105, 60.0f, 90.0f, 1, true));
    // unsinnige Leistung
    TEST_ASSERT_FALSE(m.add(100, 60.0f, -5.0f, 1, true));
    TEST_ASSERT_EQUAL_UINT16(0, m.pointCount());
}

static void test_not_ready_without_begin(void) {
    PowerMap m;
    TEST_ASSERT_FALSE(m.ready());
    TEST_ASSERT_FALSE(m.add(100, 60.0f, 90.0f, 1, true));
    float w = 0.0f;
    TEST_ASSERT_FALSE(m.estimate(100, 60.0f, w));
}

// ────────────────────────────────────────────────────────── Schaetzen

static void test_exact_cell(void) {
    PowerMap m = varonMap();
    // Der einzige belegte Messwert aus dem ersten Sondenlauf: Stufe 10,
    // 59 rpm, 108 W. Siehe GERAETEPROFIL.md.
    TEST_ASSERT_TRUE(m.add(100, 59.0f, 108.0f, 1000, true));
    float w = 0.0f;
    TEST_ASSERT_TRUE(m.estimate(100, 59.0f, w));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 108.0f, w);
}

static void test_cadence_interpolation(void) {
    PowerMap m = varonMap();
    m.add(80, 55.0f, 80.0f, 1, true);   // Band 1, Mitte 55
    m.add(80, 85.0f, 140.0f, 1, true);  // Band 4, Mitte 85
    float w = 0.0f;
    // Genau dazwischen: Mitte 70 liegt auf halbem Weg zwischen 55 und 85
    TEST_ASSERT_TRUE(m.estimate(80, 70.0f, w));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 110.0f, w);
    // Ausserhalb der belegten Baender wird gehalten, nicht extrapoliert:
    // eine Gerade ueber den Messbereich hinaus zu verlaengern waere geraten.
    TEST_ASSERT_TRUE(m.estimate(80, 45.0f, w));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 80.0f, w);
    TEST_ASSERT_TRUE(m.estimate(80, 115.0f, w));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 140.0f, w);
}

static void test_level_interpolation(void) {
    PowerMap m = varonMap();
    m.add(40, 65.0f, 40.0f, 1, true);    // Stufe 4
    m.add(120, 65.0f, 120.0f, 1, true);  // Stufe 12
    float w = 0.0f;
    // Stufe 8 ist leer und liegt genau mittig
    TEST_ASSERT_TRUE(m.estimate(80, 65.0f, w));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 80.0f, w);
    // Unterhalb und oberhalb der belegten Stufen wird ebenfalls gehalten
    TEST_ASSERT_TRUE(m.estimate(10, 65.0f, w));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, w);
    TEST_ASSERT_TRUE(m.estimate(160, 65.0f, w));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 120.0f, w);
}

static void test_estimate_fails_on_empty_map(void) {
    PowerMap m = varonMap();
    float w = 0.0f;
    TEST_ASSERT_FALSE(m.estimate(100, 60.0f, w));
}

// ──────────────────────────────────────────── Gewichtung der Quellen

static void test_sweep_replaces_passive(void) {
    PowerMap m = varonMap();
    // Passiv gelernt, unruhig
    m.add(100, 62.0f, 60.0f, 1, false);
    m.add(100, 62.0f, 160.0f, 2, false);
    // Dann der gefuehrte Punkt: er ersetzt, statt sich einzumischen
    m.add(100, 62.0f, 108.0f, 3, true);
    float w = 0.0f;
    TEST_ASSERT_TRUE(m.estimate(100, 62.0f, w));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 108.0f, w);
    TEST_ASSERT_TRUE(m.cell(9, 2).sweep);
    TEST_ASSERT_EQUAL_UINT16(1, m.cell(9, 2).samples);
}

static void test_passive_nudges_sweep_only_slightly(void) {
    PowerMap m = varonMap();
    m.add(100, 62.0f, 100.0f, 1, true);
    // Ein einzelner passiver Punkt weit daneben darf die Stuetzstelle nicht
    // umwerfen: 1/32 von 200 W Differenz sind gut 3 W.
    m.add(100, 62.0f, 300.0f, 2, false);
    float w = 0.0f;
    m.estimate(100, 62.0f, w);
    TEST_ASSERT_TRUE(w > 100.0f);
    TEST_ASSERT_TRUE(w < 110.0f);
    // Die Zelle bleibt als Sweep-Punkt gekennzeichnet
    TEST_ASSERT_TRUE(m.cell(9, 2).sweep);
}

static void test_passive_mean_converges(void) {
    PowerMap m = varonMap();
    for (int i = 0; i < 50; i++) m.add(100, 62.0f, 100.0f, (uint32_t)i, false);
    float w = 0.0f;
    m.estimate(100, 62.0f, w);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 100.0f, w);
}

// ─────────────────────────────────────────────── bestLevel fuer ERG

static void test_best_level(void) {
    PowerMap m = varonMap();
    // Grobe Kennlinie bei 65 rpm: 10 W je Stufe
    for (uint8_t n = 1; n <= 16; n++) {
        m.add((int16_t)(n * 10), 65.0f, (float)(n * 10), 1, true);
    }
    int16_t tenths = 0;
    bool ceiling = true;
    TEST_ASSERT_TRUE(m.bestLevel(75.0f, 65.0f, tenths, ceiling));
    TEST_ASSERT_FALSE(ceiling);
    // Stufe 8 liefert 80 W, Stufe 7 nur 70 — also die kleinste, die reicht
    TEST_ASSERT_EQUAL_INT(80, tenths);
}

static void test_best_level_reports_ceiling(void) {
    PowerMap m = varonMap();
    for (uint8_t n = 1; n <= 16; n++) {
        m.add((int16_t)(n * 10), 65.0f, (float)(n * 8), 1, true);
    }
    int16_t tenths = 0;
    bool ceiling = false;
    // 300 W sind auf diesem Geraet nicht fahrbar — das muss gemeldet werden,
    // nicht still geklemmt. Abnahmekriterium 8.
    TEST_ASSERT_TRUE(m.bestLevel(300.0f, 65.0f, tenths, ceiling));
    TEST_ASSERT_TRUE(ceiling);
    TEST_ASSERT_EQUAL_INT(160, tenths);
}

static void test_best_level_fails_without_data(void) {
    PowerMap m = varonMap();
    int16_t tenths = 0;
    bool ceiling = false;
    TEST_ASSERT_FALSE(m.bestLevel(100.0f, 65.0f, tenths, ceiling));
}

// ──────────────────────────────────────────────────────────── Masse

static void test_coverage_measures(void) {
    PowerMap m = varonMap();
    TEST_ASSERT_EQUAL_UINT8(0, m.levelsCovered());
    m.add(10, 65.0f, 20.0f, 1, true);
    m.add(100, 65.0f, 108.0f, 1, true);
    m.add(100, 85.0f, 150.0f, 1, true);
    TEST_ASSERT_EQUAL_UINT8(2, m.levelsCovered());
    TEST_ASSERT_EQUAL_UINT8(2, m.bandsCovered());
    TEST_ASSERT_EQUAL_UINT16(3, m.pointCount());
    TEST_ASSERT_EQUAL_UINT16(3, m.sweepCells());
    // 3 von 16*8 Zellen
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f / 128.0f, m.coverage());
}

// ────────────────────────────────────────────────── Serialisierung

static void test_roundtrip(void) {
    PowerMap m = varonMap();
    m.add(100, 59.0f, 108.4f, 123456, true);
    m.add(40, 85.0f, 51.0f, 123460, false);

    uint8_t buf[PowerMap::kMaxBytes];
    const size_t n = m.save(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT32(m.byteSize(), n);

    PowerMap b;
    TEST_ASSERT_TRUE(b.load(buf, n));
    TEST_ASSERT_EQUAL_UINT8(16, b.levelCount());
    TEST_ASSERT_EQUAL_INT(10, b.levelMinTenths());
    TEST_ASSERT_EQUAL_UINT16(10, b.levelStepTenths());

    float w = 0.0f;
    TEST_ASSERT_TRUE(b.estimate(100, 59.0f, w));
    // Zehntelwatt-Raster der Serialisierung
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 108.4f, w);
    TEST_ASSERT_TRUE(b.cell(9, 1).sweep);
    TEST_ASSERT_FALSE(b.cell(3, 4).sweep);
    TEST_ASSERT_EQUAL_UINT32(123456, b.cell(9, 1).lastS);
}

static void test_save_needs_room(void) {
    PowerMap m = varonMap();
    uint8_t small[16];
    TEST_ASSERT_EQUAL_UINT32(0, m.save(small, sizeof(small)));
}

static void test_load_rejects_garbage(void) {
    PowerMap m = varonMap();
    m.add(100, 59.0f, 108.0f, 1, true);
    uint8_t buf[PowerMap::kMaxBytes];
    const size_t n = m.save(buf, sizeof(buf));

    PowerMap b;
    b.begin(16, 10, 10);
    uint8_t bad[PowerMap::kMaxBytes];
    for (size_t i = 0; i < n; i++) bad[i] = buf[i];

    bad[0] = 0x00;  // falsches Magic
    TEST_ASSERT_FALSE(b.load(bad, n));
    bad[0] = 0x45;
    bad[2] = 99;  // falsche Version
    TEST_ASSERT_FALSE(b.load(bad, n));
    bad[2] = 1;
    TEST_ASSERT_FALSE(b.load(bad, n - 1));  // zu kurz
    TEST_ASSERT_FALSE(b.load(buf, 4));
    TEST_ASSERT_TRUE(b.load(bad, n));  // und dann doch
}

static void test_begin_discards_on_range_change(void) {
    PowerMap m = varonMap();
    m.add(100, 59.0f, 108.0f, 1, true);
    TEST_ASSERT_EQUAL_UINT16(1, m.pointCount());
    // Gleicher Bereich: die Flaeche bleibt
    m.begin(16, 10, 10);
    TEST_ASSERT_EQUAL_UINT16(1, m.pointCount());
    // Anderer Bereich: Stufe 8 von 16 ist nicht Stufe 8 von 24
    m.begin(24, 10, 10);
    TEST_ASSERT_EQUAL_UINT16(0, m.pointCount());
    TEST_ASSERT_EQUAL_UINT8(16, m.levelCount());  // auf kMapMaxLevels geklemmt
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_bands);
    RUN_TEST(test_level_index);
    RUN_TEST(test_rejects_junk);
    RUN_TEST(test_not_ready_without_begin);
    RUN_TEST(test_exact_cell);
    RUN_TEST(test_cadence_interpolation);
    RUN_TEST(test_level_interpolation);
    RUN_TEST(test_estimate_fails_on_empty_map);
    RUN_TEST(test_sweep_replaces_passive);
    RUN_TEST(test_passive_nudges_sweep_only_slightly);
    RUN_TEST(test_passive_mean_converges);
    RUN_TEST(test_best_level);
    RUN_TEST(test_best_level_reports_ceiling);
    RUN_TEST(test_best_level_fails_without_data);
    RUN_TEST(test_coverage_measures);
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_save_needs_room);
    RUN_TEST(test_load_rejects_garbage);
    RUN_TEST(test_begin_discards_on_range_change);
    return UNITY_END();
}
