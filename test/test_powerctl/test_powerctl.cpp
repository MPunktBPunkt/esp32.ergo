#include <unity.h>

#include "control/PowerController.h"
#include "control/PowerMap.h"

using namespace ergo;

static void fillMap(PowerMap& m) {
    m.begin(16, 10, 10);
    // Grobe Kennlinie bei ~60 rpm: Stufe n ≈ 10 + 10*n Watt (Stufe 1→20 … 16→170)
    const uint32_t t = 1000;
    for (uint8_t s = 1; s <= 16; s++) {
        const int16_t tenths = (int16_t)(10 + (s - 1) * 10);
        const float w = 10.0f + 10.0f * (float)s;
        TEST_ASSERT_TRUE(m.add(tenths, 60.0f, w, t, true));
    }
}

static void test_feedforward_picks_level(void) {
    PowerMap map;
    fillMap(map);
    PowerController pc;
    PowerControllerConfig cfg;
    cfg.periodMs = 1000;
    cfg.iGain = 0.0f;  // reine Vorsteuerung
    pc.begin(cfg);
    pc.setTargetW(85.0f);  // braucht Stufe ~7.5 → 8 (80 W map? Stufe 8 = 90)

    auto t = pc.tick(1000, 60.0f, 50.0f, true, map);
    TEST_ASSERT_TRUE(t.wantWrite);
    TEST_ASSERT_TRUE(t.mapReady);
    TEST_ASSERT_FALSE(t.ceiling);
    // Stufe 8 = 90 W ist erste >= 85
    TEST_ASSERT_EQUAL_INT16(80, t.levelTenths);
}

static void test_ceiling_when_unreachable(void) {
    PowerMap map;
    fillMap(map);
    PowerController pc;
    pc.begin({});
    pc.setTargetW(500.0f);
    auto t = pc.tick(1000, 60.0f, 100.0f, true, map);
    TEST_ASSERT_TRUE(t.wantWrite);
    TEST_ASSERT_TRUE(t.ceiling);
    TEST_ASSERT_EQUAL_INT16(160, t.levelTenths);
}

static void test_no_map_no_write(void) {
    PowerMap map;
    PowerController pc;
    pc.begin({});
    pc.setTargetW(100.0f);
    auto t = pc.tick(1000, 60.0f, 50.0f, true, map);
    TEST_ASSERT_FALSE(t.wantWrite);
    TEST_ASSERT_FALSE(t.mapReady);
}

static void test_period_limits_writes(void) {
    PowerMap map;
    fillMap(map);
    PowerController pc;
    PowerControllerConfig cfg;
    cfg.periodMs = 5000;
    cfg.iGain = 0.0f;
    pc.begin(cfg);
    pc.setTargetW(50.0f);
    auto a = pc.tick(1000, 60.0f, 40.0f, true, map);
    TEST_ASSERT_TRUE(a.wantWrite);
    auto b = pc.tick(2000, 60.0f, 40.0f, true, map);
    TEST_ASSERT_FALSE(b.wantWrite);  // zu frueh, gleiche Stufe
    auto c = pc.tick(7000, 60.0f, 40.0f, true, map);
    TEST_ASSERT_FALSE(c.wantWrite);  // faellig, aber Stufe unveraendert
}

static void test_integral_raises_level(void) {
    PowerMap map;
    fillMap(map);
    PowerController pc;
    PowerControllerConfig cfg;
    cfg.periodMs = 1000;
    cfg.iGain = 0.5f;
    cfg.iLimitW = 50.0f;
    cfg.deadbandW = 5.0f;
    cfg.smoothTauS = 0.5f;
    cfg.maxStepTenths = 0;  // freier Sprung — Integral-Test isoliert
    pc.begin(cfg);
    pc.setTargetW(90.0f);  // Stufe 8 = 90

    // Erstes Tick: Vorsteuerung Stufe 8
    auto t0 = pc.tick(0, 60.0f, 60.0f, true, map);
    TEST_ASSERT_TRUE(t0.wantWrite);
    TEST_ASSERT_EQUAL_INT16(80, t0.levelTenths);

    // Lange unter Ziel bleiben → Integral hebt effective target → hoehere Stufe
    for (uint32_t t = 500; t <= 20000; t += 500) {
        pc.tick(t, 60.0f, 60.0f, true, map);
    }
    auto t1 = pc.tick(21000, 60.0f, 60.0f, true, map);
    TEST_ASSERT_TRUE(t1.effectiveTargetW > 90.0f);
    TEST_ASSERT_TRUE(t1.levelTenths >= 80);
}

static void test_slew_limits_jump(void) {
    PowerMap map;
    fillMap(map);
    PowerController pc;
    PowerControllerConfig cfg;
    cfg.periodMs = 1000;
    cfg.iGain = 0.0f;
    cfg.maxStepTenths = 10;  // 1 Stufe
    cfg.retargetW = 20.0f;
    pc.begin(cfg);
    pc.setTargetW(50.0f);  // Stufe ~4 → 40 tenths
    auto a = pc.tick(1000, 60.0f, 40.0f, true, map);
    TEST_ASSERT_TRUE(a.wantWrite);
    TEST_ASSERT_EQUAL_INT16(40, a.levelTenths);

    pc.setTargetW(150.0f);  // Map will ~Stufe 14 — ohne Slew Sprung
    auto b = pc.tick(2000, 60.0f, 40.0f, true, map);
    TEST_ASSERT_TRUE(b.wantWrite);
    TEST_ASSERT_EQUAL_INT16(50, b.levelTenths);  // nur +1 Stufe
    TEST_ASSERT_TRUE(b.desiredTenths > 50);
}

static void test_small_retarget_no_immediate_write(void) {
    PowerMap map;
    fillMap(map);
    PowerController pc;
    PowerControllerConfig cfg;
    cfg.periodMs = 5000;
    cfg.iGain = 0.0f;
    cfg.retargetW = 20.0f;
    pc.begin(cfg);
    pc.setTargetW(80.0f);
    auto a = pc.tick(1000, 60.0f, 70.0f, true, map);
    TEST_ASSERT_TRUE(a.wantWrite);

    pc.setTargetW(85.0f);  // < retargetW — kein Sofort-Write
    auto b = pc.tick(1500, 60.0f, 70.0f, true, map);
    TEST_ASSERT_FALSE(b.wantWrite);

    pc.setTargetW(120.0f);  // >= retargetW — sofort faellig
    auto c = pc.tick(1600, 60.0f, 70.0f, true, map);
    TEST_ASSERT_TRUE(c.wantWrite);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_feedforward_picks_level);
    RUN_TEST(test_ceiling_when_unreachable);
    RUN_TEST(test_no_map_no_write);
    RUN_TEST(test_period_limits_writes);
    RUN_TEST(test_integral_raises_level);
    RUN_TEST(test_slew_limits_jump);
    RUN_TEST(test_small_retarget_no_immediate_write);
    return UNITY_END();
}
