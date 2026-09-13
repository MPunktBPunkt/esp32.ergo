#include <unity.h>

#include "control/WorkoutJson.h"

using namespace ergo;

static const char* kPhysioJson =
    "{"
    "\"name\":\"Physio Grundlage\","
    "\"id\":\"physio\","
    "\"steps\":["
    "{\"type\":\"steady\",\"duration_s\":120,\"target\":{\"power\":40},"
    "\"limit\":{\"hr_max\":120,\"hr_soft\":115},\"label\":\"Einfahren\"},"
    "{\"type\":\"steady\",\"duration_s\":600,\"target\":{\"power\":60},"
    "\"limit\":{\"hr_max\":120,\"hr_soft\":115},\"label\":\"Hauptteil\"},"
    "{\"type\":\"steady\",\"duration_s\":120,\"target\":{\"power\":35},"
    "\"limit\":{\"hr_max\":120},\"label\":\"Ausfahren\"}"
    "]}";

static void test_parse_physio(void) {
    WorkoutDoc d;
    char err[64];
    TEST_ASSERT_TRUE(workoutParseJson(kPhysioJson, d, err, sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("physio", d.id);
    TEST_ASSERT_EQUAL_UINT8(3, d.stepCount);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 60.0f, d.steps[1].powerW);
    TEST_ASSERT_EQUAL_UINT8(120, d.steps[1].hrMax);
    TEST_ASSERT_EQUAL_UINT8(115, d.steps[1].hrSoft);
}

static void test_roundtrip(void) {
    WorkoutDoc a, b;
    char err[64];
    char buf[1024];
    TEST_ASSERT_TRUE(workoutBuiltinById("easy20", a));
    TEST_ASSERT_TRUE(workoutWriteJson(a, buf, sizeof(buf)) > 0);
    TEST_ASSERT_TRUE(workoutParseJson(buf, b, err, sizeof(err)));
    TEST_ASSERT_EQUAL_UINT8(a.stepCount, b.stepCount);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, a.steps[1].powerW, b.steps[1].powerW);
}

static void test_ftp_pct(void) {
    WorkoutDoc d;
    char err[64];
    const char* j =
        "{\"name\":\"W\",\"steps\":[{\"duration_s\":60,\"target\":{\"ftp_pct\":55},"
        "\"limit\":{\"hr_max\":150}}]}";
    TEST_ASSERT_TRUE(workoutParseJson(j, d, err, sizeof(err)));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 55.0f, d.steps[0].ftpPct);
}

static void test_builtins(void) {
    TEST_ASSERT_TRUE(workoutBuiltinCount() >= 13);
    WorkoutDoc d;
    TEST_ASSERT_TRUE(workoutBuiltinById("reha_kurz", d));
    TEST_ASSERT_EQUAL_UINT8(3, d.stepCount);
    TEST_ASSERT_TRUE(workoutBuiltinById("ftp_warm", d));
    TEST_ASSERT_TRUE(d.steps[0].ftpPct > 0);
    TEST_ASSERT_TRUE(workoutBuiltinById("test_ramp", d));
    TEST_ASSERT_EQUAL_UINT8(WorkoutEngine::kMaxSteps, d.stepCount);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 200.0f, d.steps[7].powerW);
    TEST_ASSERT_TRUE(workoutBuiltinById("test_20min", d));
    TEST_ASSERT_EQUAL_UINT8(4, d.stepCount);
    TEST_ASSERT_TRUE(workoutBuiltinById("test_recovery", d));
    TEST_ASSERT_EQUAL_UINT32(60, d.steps[1].durationS);
}

static void test_ftp_builder_set(void) {
    WorkoutDoc d;
    TEST_ASSERT_TRUE(workoutBuiltinById("ss_3x12", d));
    TEST_ASSERT_EQUAL_UINT8(8, d.stepCount);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, d.steps[2].ftpPct);
    TEST_ASSERT_EQUAL_UINT32(720, d.steps[2].durationS);
    TEST_ASSERT_TRUE(workoutBuiltinById("ss_2x20", d));
    TEST_ASSERT_EQUAL_UINT8(6, d.stepCount);
    TEST_ASSERT_TRUE(workoutBuiltinById("th_4x8", d));
    TEST_ASSERT_EQUAL_UINT8(10, d.stepCount);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 102.0f, d.steps[2].ftpPct);
    TEST_ASSERT_TRUE(workoutBuiltinById("ftp_2x20", d));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 98.0f, d.steps[2].ftpPct);
    TEST_ASSERT_TRUE(workoutBuiltinById("over_under", d));
    TEST_ASSERT_EQUAL_UINT8(15, d.stepCount);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 95.0f, d.steps[2].ftpPct);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 105.0f, d.steps[3].ftpPct);
    TEST_ASSERT_TRUE(workoutBuiltinById("vo2_5x4", d));
    TEST_ASSERT_EQUAL_UINT8(12, d.stepCount);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 112.0f, d.steps[2].ftpPct);
    // JSON-Budget: Over/Under ist der groesste der Serie.
    char buf[kWorkoutJsonBuf];
    TEST_ASSERT_TRUE(workoutWriteJson(d, buf, sizeof(buf)) > 0);
    TEST_ASSERT_TRUE(workoutBuiltinById("over_under", d));
    TEST_ASSERT_TRUE(workoutWriteJson(d, buf, sizeof(buf)) > 0);
}

static void test_reject_empty(void) {
    WorkoutDoc d;
    char err[64];
    TEST_ASSERT_FALSE(workoutParseJson("{\"name\":\"x\",\"steps\":[]}", d, err, sizeof(err)));
}

static void test_ramp_json_fits(void) {
    WorkoutDoc d;
    TEST_ASSERT_TRUE(workoutBuiltinById("test_ramp", d));
    char small[1536];
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)workoutWriteJson(d, small, sizeof(small)));
    char buf[kWorkoutJsonBuf];
    const size_t n = workoutWriteJson(d, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_TRUE(n < kWorkoutJsonBuf);
    WorkoutDoc b;
    char err[64];
    TEST_ASSERT_TRUE(workoutParseJson(buf, b, err, sizeof(err)));
    TEST_ASSERT_EQUAL_UINT8(d.stepCount, b.stepCount);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, d.steps[d.stepCount - 1].powerW, b.steps[b.stepCount - 1].powerW);
}

static void test_self_paced_json(void) {
    WorkoutDoc d;
    TEST_ASSERT_TRUE(workoutBuiltinById("test_20min", d));
    TEST_ASSERT_TRUE(d.steps[2].selfPaced);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, d.steps[2].ftpPct);
    char buf[kWorkoutJsonBuf];
    TEST_ASSERT_TRUE(workoutWriteJson(d, buf, sizeof(buf)) > 0);
    TEST_ASSERT_TRUE(strstr(buf, "self_paced") != nullptr);
    WorkoutDoc b;
    char err[64];
    TEST_ASSERT_TRUE(workoutParseJson(buf, b, err, sizeof(err)));
    TEST_ASSERT_TRUE(b.steps[2].selfPaced);
}

static void test_interval_expand(void) {
    const char* j =
        "{\"name\":\"4x\",\"id\":\"iv4\",\"steps\":["
        "{\"type\":\"steady\",\"duration_s\":60,\"label\":\"Warm\",\"target\":{\"power\":50}},"
        "{\"type\":\"interval\",\"repeat\":4,\"label\":\"Block\",\"steps\":["
        "{\"type\":\"steady\",\"duration_s\":120,\"label\":\"Work\",\"target\":{\"ftp_pct\":90}},"
        "{\"type\":\"steady\",\"duration_s\":60,\"label\":\"Rest\",\"target\":{\"ftp_pct\":50}}"
        "]},"
        "{\"type\":\"steady\",\"duration_s\":60,\"label\":\"Cool\",\"target\":{\"power\":40}}"
        "]}";
    WorkoutDoc d;
    char err[64];
    TEST_ASSERT_TRUE(workoutParseJson(j, d, err, sizeof(err)));
    TEST_ASSERT_EQUAL_UINT8(10, d.stepCount);  // 1 + 8 + 1
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, d.steps[1].ftpPct);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, d.steps[2].ftpPct);
    TEST_ASSERT_EQUAL_UINT32(120, d.steps[1].durationS);
    TEST_ASSERT_EQUAL_UINT32(60, d.steps[8].durationS);  // last rest of 4th
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 40.0f, d.steps[9].powerW);
}

static void test_interval_overflow(void) {
    const char* j =
        "{\"name\":\"big\",\"steps\":["
        "{\"type\":\"interval\",\"repeat\":9,\"steps\":["
        "{\"duration_s\":60,\"target\":{\"power\":100}},"
        "{\"duration_s\":60,\"target\":{\"power\":50}}"
        "]}]}";
    WorkoutDoc d;
    char err[64];
    TEST_ASSERT_FALSE(workoutParseJson(j, d, err, sizeof(err)));
}

static void test_ramp_expand(void) {
    const char* j =
        "{\"name\":\"cool\",\"id\":\"cool\",\"steps\":["
        "{\"type\":\"ramp\",\"duration_s\":300,\"label\":\"Aus\","
        "\"target\":{\"ftp_pct_from\":52,\"ftp_pct_to\":35}}"
        "]}";
    WorkoutDoc d;
    char err[64];
    TEST_ASSERT_TRUE(workoutParseJson(j, d, err, sizeof(err)));
    TEST_ASSERT_TRUE(d.stepCount >= 2);
    TEST_ASSERT_TRUE(d.stepCount <= 10);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 52.0f, d.steps[0].ftpPct);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 35.0f, d.steps[d.stepCount - 1].ftpPct);
    uint32_t sum = 0;
    for (uint8_t i = 0; i < d.stepCount; i++) sum += d.steps[i].durationS;
    TEST_ASSERT_EQUAL_UINT32(300, sum);
}

static void test_goal_favorite(void) {
    WorkoutDoc d;
    char err[64];
    const char* j =
        "{\"name\":\"X\",\"id\":\"x1\",\"goal\":\"fatloss\",\"favorite\":true,"
        "\"steps\":[{\"duration_s\":60,\"target\":{\"power\":50}}]}";
    TEST_ASSERT_TRUE(workoutParseJson(j, d, err, sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("fatloss", d.goal);
    TEST_ASSERT_TRUE(d.favorite);
    char buf[512];
    TEST_ASSERT_TRUE(workoutWriteJson(d, buf, sizeof(buf)) > 0);
    WorkoutDoc b;
    TEST_ASSERT_TRUE(workoutParseJson(buf, b, err, sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("fatloss", b.goal);
    TEST_ASSERT_TRUE(b.favorite);
    TEST_ASSERT_EQUAL_STRING("reha", workoutBuiltinGoal(0));
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_physio);
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_ftp_pct);
    RUN_TEST(test_builtins);
    RUN_TEST(test_ftp_builder_set);
    RUN_TEST(test_reject_empty);
    RUN_TEST(test_ramp_json_fits);
    RUN_TEST(test_self_paced_json);
    RUN_TEST(test_interval_expand);
    RUN_TEST(test_interval_overflow);
    RUN_TEST(test_ramp_expand);
    RUN_TEST(test_goal_favorite);
    return UNITY_END();
}
