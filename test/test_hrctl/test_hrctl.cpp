#include <unity.h>

#include "control/HrController.h"

using namespace ergo;

static void test_raises_power_when_hr_low(void) {
    HrController h;
    HrControllerConfig cfg;
    cfg.basePowerW = 80.0f;
    cfg.iGain = 1.0f;
    cfg.deadbandBpm = 2.0f;
    cfg.iLimitW = 60.0f;
    cfg.lostAfterMs = 60000;
    h.begin(cfg);
    h.setTargetHr(140);

    float last = h.powerTargetW();
    for (uint32_t t = 0; t <= 20000; t += 500) {
        h.tick(t, 120, true, 0);  // 20 bpm unter Ziel
    }
    TEST_ASSERT_TRUE(h.powerTargetW() > last);
    TEST_ASSERT_TRUE(h.powerTargetW() > 80.0f);
}

static void test_lowers_power_when_hr_high(void) {
    HrController h;
    HrControllerConfig cfg;
    cfg.basePowerW = 100.0f;
    cfg.iGain = 1.0f;
    cfg.deadbandBpm = 2.0f;
    cfg.lostAfterMs = 60000;
    h.begin(cfg);
    h.setTargetHr(120);

    for (uint32_t t = 0; t <= 15000; t += 500) {
        h.tick(t, 145, true, 0);
    }
    TEST_ASSERT_TRUE(h.powerTargetW() < 100.0f);
}

static void test_lost_after_timeout(void) {
    HrController h;
    HrControllerConfig cfg;
    cfg.lostAfterMs = 3000;
    h.begin(cfg);
    h.setTargetHr(130);
    h.tick(0, 130, true, 0);
    TEST_ASSERT_FALSE(h.tick(1000, 0, false, 0).lost);
    TEST_ASSERT_TRUE(h.tick(5000, 0, false, 0).lost);
}

static void test_hard_cap_pulls_down(void) {
    HrController h;
    HrControllerConfig cfg;
    cfg.basePowerW = 120.0f;
    cfg.iGain = 0.1f;
    cfg.overCapGain = 5.0f;
    cfg.deadbandBpm = 5.0f;
    cfg.lostAfterMs = 60000;
    h.begin(cfg);
    h.setTargetHr(140);  // Ziel ok, aber hard max 120
    for (uint32_t t = 0; t <= 10000; t += 500) {
        h.tick(t, 135, true, 120);
    }
    TEST_ASSERT_TRUE(h.powerTargetW() < 120.0f);
}

static void test_deadband_holds(void) {
    HrController h;
    HrControllerConfig cfg;
    cfg.basePowerW = 90.0f;
    cfg.iGain = 2.0f;
    cfg.deadbandBpm = 5.0f;
    cfg.lostAfterMs = 60000;
    h.begin(cfg);
    h.setTargetHr(140);
    h.tick(0, 140, true, 0);
    const float a = h.powerTargetW();
    h.tick(5000, 142, true, 0);  // innerhalb Totband
    TEST_ASSERT_FLOAT_WITHIN(0.5f, a, h.powerTargetW());
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_raises_power_when_hr_low);
    RUN_TEST(test_lowers_power_when_hr_high);
    RUN_TEST(test_lost_after_timeout);
    RUN_TEST(test_hard_cap_pulls_down);
    RUN_TEST(test_deadband_holds);
    return UNITY_END();
}
