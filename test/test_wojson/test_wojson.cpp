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
    TEST_ASSERT_TRUE(workoutBuiltinCount() >= 7);
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

static void test_reject_empty(void) {
    WorkoutDoc d;
    char err[64];
    TEST_ASSERT_FALSE(workoutParseJson("{\"name\":\"x\",\"steps\":[]}", d, err, sizeof(err)));
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_physio);
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_ftp_pct);
    RUN_TEST(test_builtins);
    RUN_TEST(test_reject_empty);
    return UNITY_END();
}
