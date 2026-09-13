#include <unity.h>
#include <string.h>

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
    a.zoneCount = 7;
    a.zoneTimeS[2] = 120;
    a.zoneTimeS[3] = 80;
    char buf[480];
    TEST_ASSERT_TRUE(st.writeJsonLine(a, buf, sizeof(buf)) > 0);
    SessionSummary b;
    TEST_ASSERT_TRUE(st.parseJsonLine(buf, b));
    TEST_ASSERT_EQUAL_STRING("MANUAL_ERG", b.mode);
    TEST_ASSERT_EQUAL_UINT32(600, b.durationS);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 95.5f, b.avgPowerW);
    TEST_ASSERT_EQUAL_UINT32(120, b.zoneTimeS[2]);
    TEST_ASSERT_EQUAL_UINT32(80, b.zoneTimeS[3]);
}

static void test_rpe_note_roundtrip(void) {
    SessionStore st;
    SessionSummary a;
    a.valid = true;
    strncpy(a.mode, "WORKOUT", sizeof(a.mode) - 1);
    strncpy(a.endReason, "done", sizeof(a.endReason) - 1);
    a.durationS = 300;
    a.rpe = 7;
    strncpy(a.note, "gute Beine", sizeof(a.note) - 1);
    char buf[560];
    TEST_ASSERT_TRUE(st.writeJsonLine(a, buf, sizeof(buf)) > 0);
    SessionSummary b;
    TEST_ASSERT_TRUE(st.parseJsonLine(buf, b));
    TEST_ASSERT_EQUAL_UINT8(7, b.rpe);
    TEST_ASSERT_EQUAL_STRING("gute Beine", b.note);
    TEST_ASSERT_TRUE(st.append(a));
    a.rpe = 8;
    strncpy(a.note, "noch ok", sizeof(a.note) - 1);
    TEST_ASSERT_TRUE(st.replaceNewest(a));
    SessionSummary c;
    TEST_ASSERT_TRUE(st.at(0, c));
    TEST_ASSERT_EQUAL_UINT8(8, c.rpe);
    TEST_ASSERT_EQUAL_STRING("noch ok", c.note);
}

static void test_zone_accumulate(void) {
    SessionTracker s;
    s.begin({});
    s.setZoneBasis(false, 200, 180);
    s.start(1000, "MANUAL_ERG", "", "standard");
    // 100 W = 50 % FTP → Z1
    s.tick(1000, 60.0f, 100.0f, 100, true);
    TEST_ASSERT_EQUAL_UINT8(1, s.currentZone());
    s.tick(3000, 60.0f, 100.0f, 100, true);
    TEST_ASSERT_TRUE(s.peek().zoneTimeS[0] >= 1);
    // 200 W = 100 % → Z4
    s.tick(5000, 60.0f, 200.0f, 100, true);
    TEST_ASSERT_EQUAL_UINT8(4, s.currentZone());
}

static void test_set_auto_pause_override(void) {
    SessionTracker s;
    s.begin({});
    TEST_ASSERT_EQUAL_UINT32(10000, s.autoPauseAfterMs());
    s.setAutoPauseAfterMs(4000);
    TEST_ASSERT_EQUAL_UINT32(4000, s.autoPauseAfterMs());
    s.setAutoPauseAfterMs(500);  // clamp ≥ 2000
    TEST_ASSERT_EQUAL_UINT32(2000, s.autoPauseAfterMs());
    s.start(0, "WORKOUT", "T", "standard", "t1");
    s.tick(0, 60.0f, 80.0f, 120, true);
    s.tick(1000, 0.0f, 0.0f, 120, true);
    TEST_ASSERT_FALSE(s.paused());
    s.tick(3500, 0.0f, 0.0f, 120, true);
    TEST_ASSERT_TRUE(s.paused());
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_auto_pause_and_resume);
    RUN_TEST(test_freeze_timeout);
    RUN_TEST(test_store_ring);
    RUN_TEST(test_json_roundtrip);
    RUN_TEST(test_rpe_note_roundtrip);
    RUN_TEST(test_zone_accumulate);
    RUN_TEST(test_set_auto_pause_override);
    return UNITY_END();
}
