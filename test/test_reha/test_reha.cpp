#include <unity.h>

#include "control/RehaController.h"

using namespace ergo;

static void test_holds_desired_under_soft(void) {
    RehaController r;
    r.begin({});
    r.setDesiredW(60.0f);
    r.setHrLimits(115, 120);
    r.setDurationS(0);
    for (uint32_t t = 0; t <= 5000; t += 500) {
        r.tick(t, 100, true);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 60.0f, r.effectiveW());
    TEST_ASSERT_FALSE(r.capActive());
    TEST_ASSERT_EQUAL_UINT16(0, r.interventions());
}

static void test_soft_cuts_before_hard(void) {
    RehaController r;
    r.begin({});
    r.setDesiredW(60.0f);
    r.setHrLimits(115, 120);
    r.setDurationS(0);
    r.tick(0, 100, true);
    r.tick(1000, 117, true);  // 2 bpm über Soft → ~5 W cut
    TEST_ASSERT_TRUE(r.effectiveW() < 60.0f);
    TEST_ASSERT_TRUE(r.capActive());
    TEST_ASSERT_EQUAL_UINT16(1, r.interventions());
    TEST_ASSERT_TRUE(r.effectiveW() > 40.0f);
}

static void test_hard_cuts_harder(void) {
    RehaController r;
    r.begin({});
    r.setDesiredW(60.0f);
    r.setHrLimits(115, 120);
    r.setDurationS(0);
    r.tick(0, 100, true);
    r.tick(1000, 125, true);
    TEST_ASSERT_TRUE(r.effectiveW() < 50.0f);
    TEST_ASSERT_TRUE(r.capActive());
}

static void test_never_above_desired(void) {
    RehaController r;
    r.begin({});
    r.setDesiredW(60.0f);
    r.setHrLimits(115, 120);
    r.setDurationS(0);
    r.tick(0, 90, true);
    for (uint32_t t = 500; t <= 20000; t += 500) {
        r.tick(t, 90, true);
        TEST_ASSERT_TRUE(r.effectiveW() <= 60.0f + 0.01f);
    }
}

static void test_restores_after_cap(void) {
    RehaController r;
    RehaControllerConfig cfg;
    cfg.restorePerS = 2.0f;
    r.begin(cfg);
    r.setDesiredW(60.0f);
    r.setHrLimits(115, 120);
    r.setDurationS(0);
    r.tick(0, 122, true);
    r.tick(1000, 122, true);
    const float low = r.effectiveW();
    TEST_ASSERT_TRUE(low < 55.0f);
    for (uint32_t t = 1500; t <= 30000; t += 500) {
        r.tick(t, 100, true);
    }
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 60.0f, r.effectiveW());
    TEST_ASSERT_FALSE(r.capActive());
}

static void test_duration_finishes(void) {
    RehaController r;
    r.begin({});
    r.setDesiredW(60.0f);
    r.setHrLimits(115, 120);
    r.setDurationS(10);
    r.tick(0, 100, true);
    TEST_ASSERT_FALSE(r.tick(5000, 100, true).finished);
    TEST_ASSERT_TRUE(r.tick(11000, 100, true).finished);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, r.effectiveW());
}

static void test_lost_timeout(void) {
    RehaController r;
    RehaControllerConfig cfg;
    cfg.lostAfterMs = 3000;
    r.begin(cfg);
    r.setDesiredW(60.0f);
    r.setHrLimits(115, 120);
    r.setDurationS(0);
    r.tick(0, 100, true);
    TEST_ASSERT_FALSE(r.tick(1000, 0, false).lost);
    TEST_ASSERT_TRUE(r.tick(5000, 0, false).lost);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_holds_desired_under_soft);
    RUN_TEST(test_soft_cuts_before_hard);
    RUN_TEST(test_hard_cuts_harder);
    RUN_TEST(test_never_above_desired);
    RUN_TEST(test_restores_after_cap);
    RUN_TEST(test_duration_finishes);
    RUN_TEST(test_lost_timeout);
    return UNITY_END();
}
