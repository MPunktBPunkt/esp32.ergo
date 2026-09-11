#include <unity.h>

#include "control/WorkoutEngine.h"

using namespace ergo;

static void test_builtin_three_steps(void) {
    WorkoutEngine w;
    TEST_ASSERT_TRUE(w.loadBuiltinPhysio(1.0f));
    TEST_ASSERT_EQUAL_UINT8(3, w.stepCount());
    TEST_ASSERT_EQUAL_STRING("Physio Grundlage", w.name());
}

static void test_runs_and_advances(void) {
    WorkoutEngine w;
    WorkoutStep s[2];
    strncpy(s[0].label, "A", sizeof(s[0].label) - 1);
    s[0].durationS = 5;
    s[0].powerW = 40;
    s[0].hrMax = 120;
    strncpy(s[1].label, "B", sizeof(s[1].label) - 1);
    s[1].durationS = 5;
    s[1].powerW = 60;
    s[1].hrMax = 120;
    TEST_ASSERT_TRUE(w.loadSteps(s, 2, "t"));
    TEST_ASSERT_TRUE(w.start(1000));
    auto t = w.tick(1000);
    TEST_ASSERT_EQUAL_STRING("A", t.label);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 40.0f, t.desiredW);
    TEST_ASSERT_FALSE(t.finished);
    t = w.tick(7000);  // 6s later → step B
    TEST_ASSERT_EQUAL_STRING("B", t.label);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 60.0f, t.desiredW);
    TEST_ASSERT_TRUE(t.justAdvanced);
    t = w.tick(13000);
    TEST_ASSERT_TRUE(t.finished);
    TEST_ASSERT_EQUAL_INT((int)WorkoutState::Done, (int)w.state());
}

static void test_skip(void) {
    WorkoutEngine w;
    TEST_ASSERT_TRUE(w.loadBuiltinPhysio(0.1f));
    TEST_ASSERT_TRUE(w.start(0));
    TEST_ASSERT_TRUE(w.skip(100));
    TEST_ASSERT_EQUAL_UINT8(1, w.stepIndex());
    TEST_ASSERT_EQUAL_STRING("Hauptteil", w.tick(100).label);
}

static void test_pause_freezes_clock(void) {
    WorkoutEngine w;
    WorkoutStep s[1];
    strncpy(s[0].label, "X", sizeof(s[0].label) - 1);
    s[0].durationS = 10;
    s[0].powerW = 50;
    w.loadSteps(s, 1, "p");
    w.start(1000);
    w.tick(1000);
    w.pause(4000);  // 3 s gelaufen
    auto t = w.tick(9000);
    TEST_ASSERT_EQUAL_INT((int)WorkoutState::Paused, (int)t.state);
    TEST_ASSERT_EQUAL_UINT32(7, t.stepRemainingS);
    w.resume(9000);
    t = w.tick(16000);  // +7 s → fertig
    TEST_ASSERT_TRUE(t.finished);
}

static void test_ftp_pct(void) {
    WorkoutEngine w;
    WorkoutStep s[1];
    strncpy(s[0].label, "ftp", sizeof(s[0].label) - 1);
    s[0].durationS = 30;
    s[0].ftpPct = 50.0f;
    w.setFtpW(200);
    w.loadSteps(s, 1, "f");
    w.start(0);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 100.0f, w.tick(0).desiredW);
}

static void test_scale_shortens(void) {
    WorkoutEngine w;
    TEST_ASSERT_TRUE(w.loadBuiltinPhysio(0.1f));
    // 12 + 60 + 12
    w.start(0);
    auto t = w.tick(0);
    TEST_ASSERT_TRUE(t.totalRemainingS <= 90);
    TEST_ASSERT_TRUE(t.totalRemainingS >= 80);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_builtin_three_steps);
    RUN_TEST(test_runs_and_advances);
    RUN_TEST(test_skip);
    RUN_TEST(test_pause_freezes_clock);
    RUN_TEST(test_ftp_pct);
    RUN_TEST(test_scale_shortens);
    return UNITY_END();
}
