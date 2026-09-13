/**
 * Hosttests fuer BridgeAssist (Difficulty + HR-Deckel).
 */
#include <unity.h>

#include "control/BridgeAssist.h"

using namespace ergo;

void setUp(void) {}
void tearDown(void) {}

static void test_difficulty_neutral() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 200.0f, bridgeApplyDifficulty(200.0f, 100));
}

static void test_difficulty_easier() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 160.0f, bridgeApplyDifficulty(200.0f, 80));
}

static void test_difficulty_harder_clamped_by_max() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 250.0f, bridgeApplyDifficulty(200.0f, 150, 20.0f, 250.0f));
}

static void test_difficulty_pct_clamped() {
    // 40 → 50, 200 → 150
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, bridgeApplyDifficulty(200.0f, 40));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 300.0f, bridgeApplyDifficulty(200.0f, 200));
}

static void test_hr_cap_off() {
    BridgeHrCap cap;
    cap.begin({});
    cap.setLimits(0, 0);
    TEST_ASSERT_FALSE(cap.enabled());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 180.0f, cap.tick(1000, 180.0f, 190, true));
    TEST_ASSERT_FALSE(cap.capActive());
}

static void test_hr_cap_soft() {
    BridgeHrCap cap;
    cap.begin({});
    cap.setLimits(150, 170);
    // 160 bpm → 10 über Soft × 2.5 = −25 W
    const float e = cap.tick(1000, 200.0f, 160, true);
    TEST_ASSERT_TRUE(cap.capActive());
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 175.0f, e);
}

static void test_hr_cap_hard() {
    BridgeHrCap cap;
    cap.begin({});
    cap.setLimits(150, 170);
    // 180 → soft-band 20×2.5 + hard 10×6 = 50+60 = 110 → 90 W
    const float e = cap.tick(1000, 200.0f, 180, true);
    TEST_ASSERT_TRUE(cap.capActive());
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 90.0f, e);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_difficulty_neutral);
    RUN_TEST(test_difficulty_easier);
    RUN_TEST(test_difficulty_harder_clamped_by_max);
    RUN_TEST(test_difficulty_pct_clamped);
    RUN_TEST(test_hr_cap_off);
    RUN_TEST(test_hr_cap_soft);
    RUN_TEST(test_hr_cap_hard);
    return UNITY_END();
}
