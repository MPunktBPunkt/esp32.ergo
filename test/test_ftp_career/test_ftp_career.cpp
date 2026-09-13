#include <unity.h>
#include <string.h>

#include "core/FtpCareer.h"
#include "core/SessionSummary.h"

using namespace ergo;

static SessionSummary makeDone(const char* wid) {
    SessionSummary s;
    s.valid = true;
    strncpy(s.mode, "WORKOUT", sizeof(s.mode) - 1);
    strncpy(s.workoutId, wid, sizeof(s.workoutId) - 1);
    strncpy(s.endReason, "done", sizeof(s.endReason) - 1);
    s.durationS = 600;
    s.avgPowerW = 100;
    s.avgDesiredW = 100;
    return s;
}

static void test_stages(void) {
    TEST_ASSERT_EQUAL_UINT8(8, ftpCareerStageCount());
    TEST_ASSERT_EQUAL_STRING("ftp_warm", ftpCareerStage(0)->id);
    TEST_ASSERT_EQUAL_STRING("test_ramp", ftpCareerStage(7)->id);
    TEST_ASSERT_EQUAL_UINT8(5, ftpCareerIndexOf("over_under"));
    TEST_ASSERT_EQUAL_UINT8(255, ftpCareerIndexOf("physio"));
}

static void test_unlock_flow(void) {
    FtpCareerState st;
    TEST_ASSERT_TRUE(ftpCareerIsUnlocked(st, 0));
    TEST_ASSERT_FALSE(ftpCareerIsUnlocked(st, 1));
    ftpCareerOnSessionEnd(st, makeDone("ftp_warm"));
    TEST_ASSERT_TRUE(st.offerPending);
    TEST_ASSERT_TRUE(st.offerClean);
    TEST_ASSERT_TRUE(ftpCareerAccept(st));
    TEST_ASSERT_EQUAL_UINT8(1, st.unlocked);
    TEST_ASSERT_TRUE(ftpCareerIsUnlocked(st, 1));
    TEST_ASSERT_FALSE(ftpCareerIsUnlocked(st, 2));
}

static void test_wrong_workout_no_offer(void) {
    FtpCareerState st;
    ftpCareerOnSessionEnd(st, makeDone("ss_3x12"));
    TEST_ASSERT_FALSE(st.offerPending);
}

static void test_json_roundtrip(void) {
    FtpCareerState a;
    a.unlocked = 3;
    char buf[48];
    TEST_ASSERT_TRUE(ftpCareerWriteJson(a, buf, sizeof(buf)) > 0);
    FtpCareerState b;
    TEST_ASSERT_TRUE(ftpCareerParseJson(buf, b));
    TEST_ASSERT_EQUAL_UINT8(3, b.unlocked);
}

static void test_set_unlocked(void) {
    FtpCareerState st;
    TEST_ASSERT_TRUE(ftpCareerSetUnlocked(st, 4));
    TEST_ASSERT_EQUAL_UINT8(4, st.unlocked);
    TEST_ASSERT_FALSE(ftpCareerSetUnlocked(st, 99));
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_stages);
    RUN_TEST(test_unlock_flow);
    RUN_TEST(test_wrong_workout_no_offer);
    RUN_TEST(test_json_roundtrip);
    RUN_TEST(test_set_unlocked);
    return UNITY_END();
}
