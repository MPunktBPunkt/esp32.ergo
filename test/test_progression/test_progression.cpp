#include <unity.h>

#include "core/Progression.h"
#include "control/WorkoutJson.h"

using namespace ergo;

static SessionSummary makeClean() {
    SessionSummary s;
    s.valid = true;
    strncpy(s.endReason, "done", sizeof(s.endReason) - 1);
    strncpy(s.workoutName, "Physio Grundlage", sizeof(s.workoutName) - 1);
    s.interventions = 0;
    s.avgPowerW = 58.0f;
    s.avgDesiredW = 60.0f;
    return s;
}

static void test_clean_ok(void) {
    char r[56];
    TEST_ASSERT_TRUE(progressionIsClean(makeClean(), r, sizeof(r)));
}

static void test_clean_interventions(void) {
    char r[56];
    SessionSummary s = makeClean();
    s.interventions = 2;
    TEST_ASSERT_FALSE(progressionIsClean(s, r, sizeof(r)));
    TEST_ASSERT_TRUE(r[0] != 0);
}

static void test_clean_power(void) {
    char r[56];
    SessionSummary s = makeClean();
    s.avgPowerW = 40.0f;
    TEST_ASSERT_FALSE(progressionIsClean(s, r, sizeof(r)));
}

static void test_next_clamp(void) {
    TEST_ASSERT_EQUAL_UINT32(660, progressionNextMainS(600, 60, 1800));
    TEST_ASSERT_EQUAL_UINT32(1800, progressionNextMainS(1780, 60, 1800));
}

static void test_apply_physio(void) {
    WorkoutDoc d;
    TEST_ASSERT_TRUE(workoutBuiltinById("physio", d));
    TEST_ASSERT_TRUE(d.progression.enabled);
    TEST_ASSERT_EQUAL_UINT32(600, d.steps[1].durationS);
    progressionApplyMain(d, 720);
    TEST_ASSERT_EQUAL_UINT32(720, d.steps[1].durationS);
    TEST_ASSERT_EQUAL_UINT32(120, d.steps[0].durationS);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_clean_ok);
    RUN_TEST(test_clean_interventions);
    RUN_TEST(test_clean_power);
    RUN_TEST(test_next_clamp);
    RUN_TEST(test_apply_physio);
    return UNITY_END();
}
