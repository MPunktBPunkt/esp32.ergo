#include <unity.h>

#include "control/SimAssist.h"

using namespace ergo;

void setUp(void) {}
void tearDown(void) {}

static void test_no_assist_without_ceiling(void) {
    TEST_ASSERT_EQUAL_INT16(0, simAssistGradeHundredth(200.0f, 150.0f, false));
}

static void test_no_assist_small_deficit(void) {
    TEST_ASSERT_EQUAL_INT16(0, simAssistGradeHundredth(160.0f, 150.0f, true));
}

static void test_assist_scales_and_clamps(void) {
    // 48 W Defizit / 8 = 6 % → 600 hundredth
    TEST_ASSERT_EQUAL_INT16(600, simAssistGradeHundredth(200.0f, 152.0f, true));
    // 80 W → geklemmt auf 6 %
    TEST_ASSERT_EQUAL_INT16(600, simAssistGradeHundredth(250.0f, 170.0f, true));
}

static void test_min_effective_grade(void) {
    // 16 W / 8 = 2 % genau
    TEST_ASSERT_EQUAL_INT16(200, simAssistGradeHundredth(166.0f, 150.0f, true));
    // 15 W = minDeficit genau, 15/8 = 1.875 → auf 2 % angehoben
    TEST_ASSERT_EQUAL_INT16(200, simAssistGradeHundredth(165.0f, 150.0f, true));
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_no_assist_without_ceiling);
    RUN_TEST(test_no_assist_small_deficit);
    RUN_TEST(test_assist_scales_and_clamps);
    RUN_TEST(test_min_effective_grade);
    UNITY_END();
}

#ifndef ARDUINO
int main(int, char**) {
    setup();
    return 0;
}
#endif
