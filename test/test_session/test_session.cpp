#include <unity.h>

#include "core/SessionTracker.h"
#include "core/SessionStore.h"

using namespace ergo;

static void test_auto_pause_and_resume(void) {
    SessionTracker s;
    SessionTrackerConfig cfg;
    cfg.autoPauseAfterMs = 3000;
    cfg.cadenceMinRpm = 5.0f;
    s.begin(cfg);
    s.start(0, "MANUAL_ERG", "", "standard");
    s.tick(0, 60.0f, 80.0f, 120, true);
    s.tick(1000, 60.0f, 80.0f, 120, true);
    TEST_ASSERT_FALSE(s.paused());
    s.tick(2000, 0.0f, 0.0f, 120, true);
    TEST_ASSERT_FALSE(s.paused());
    s.tick(5500, 0.0f, 0.0f, 120, true);
    TEST_ASSERT_TRUE(s.paused());
    TEST_ASSERT_TRUE(s.holdLoad());
    s.tick(6000, 70.0f, 90.0f, 121, true);
    TEST_ASSERT_FALSE(s.paused());
    auto sum = s.end(8000, "stop");
    TEST_ASSERT_TRUE(sum.valid);
    TEST_ASSERT_TRUE(sum.autoPauses >= 1);
    TEST_ASSERT_TRUE(sum.avgPowerW > 0.0f);
}

static void test_freeze_timeout(void) {
    SessionTracker s;
    SessionTrackerConfig cfg;
    cfg.freezeToLevelAfterMs = 5000;
    s.begin(cfg);
    s.start(0, "HR_HOLD", "", "reha");
    s.noteHrLost(1000, true);
    TEST_ASSERT_FALSE(s.freezeTimedOut(3000));
    TEST_ASSERT_TRUE(s.freezeTimedOut(7000));
    s.noteHrLost(7000, false);
    TEST_ASSERT_FALSE(s.freezeTimedOut(9000));
}

static void test_store_ring(void) {
    SessionStore st;
    st.clear();
    SessionSummary a;
    a.valid = true;
    strncpy(a.mode, "REHA", sizeof(a.mode) - 1);
    a.durationS = 100;
    st.append(a);
    a.durationS = 200;
    strncpy(a.mode, "WORKOUT", sizeof(a.mode) - 1);
    st.append(a);
    SessionSummary out;
    TEST_ASSERT_TRUE(st.at(0, out));
    TEST_ASSERT_EQUAL_STRING("WORKOUT", out.mode);
    TEST_ASSERT_EQUAL_UINT32(200, out.durationS);
    TEST_ASSERT_TRUE(st.at(1, out));
    TEST_ASSERT_EQUAL_STRING("REHA", out.mode);
}

static void test_json_roundtrip(void) {
    SessionStore st;
    SessionSummary a;
    a.valid = true;
    strncpy(a.mode, "MANUAL_ERG", sizeof(a.mode) - 1);
    strncpy(a.workoutName, "x", sizeof(a.workoutName) - 1);
    a.durationS = 600;
    a.avgPowerW = 95.5f;
    a.workKj = 57.3f;
    a.hrMax = 145;
    char buf[320];
    TEST_ASSERT_TRUE(st.writeJsonLine(a, buf, sizeof(buf)) > 0);
    SessionSummary b;
    TEST_ASSERT_TRUE(st.parseJsonLine(buf, b));
    TEST_ASSERT_EQUAL_STRING("MANUAL_ERG", b.mode);
    TEST_ASSERT_EQUAL_UINT32(600, b.durationS);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 95.5f, b.avgPowerW);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_auto_pause_and_resume);
    RUN_TEST(test_freeze_timeout);
    RUN_TEST(test_store_ring);
    RUN_TEST(test_json_roundtrip);
    return UNITY_END();
}
