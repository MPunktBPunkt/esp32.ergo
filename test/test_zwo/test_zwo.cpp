#include <unity.h>
#include <string.h>

#include "control/ZwoImport.h"
#include "control/WorkoutJson.h"

using namespace ergo;

static const char* kDemo =
    "<?xml version=\"1.0\"?>\n"
    "<workout_file>\n"
    "<author>Test</author>\n"
    "<name>4x2 Demo</name>\n"
    "<description>hosttest</description>\n"
    "<sportType>bike</sportType>\n"
    "<workout>\n"
    "<Warmup Duration=\"300\" PowerLow=\"0.50\" PowerHigh=\"0.70\"/>\n"
    "<IntervalsT Repeat=\"4\" OnDuration=\"120\" OffDuration=\"60\" "
    "OnPower=\"1.05\" OffPower=\"0.55\"/>\n"
    "<SteadyState Duration=\"180\" Power=\"0.60\"/>\n"
    "<FreeRide Duration=\"120\"/>\n"
    "<Cooldown Duration=\"300\" PowerLow=\"0.40\" PowerHigh=\"0.55\"/>\n"
    "</workout>\n"
    "</workout_file>\n";

static void test_zwo_demo(void) {
    char buf[kWorkoutJsonBuf];
    char err[80];
    const size_t n = zwoToJson(kDemo, buf, sizeof(buf), err, sizeof(err));
    TEST_ASSERT_TRUE_MESSAGE(n > 0, err);
    TEST_ASSERT_TRUE(strstr(buf, "\"id\":\"4x2_demo\"") != nullptr);
    TEST_ASSERT_TRUE(strstr(buf, "\"type\":\"interval\"") != nullptr);
    TEST_ASSERT_TRUE(strstr(buf, "\"repeat\":4") != nullptr);
    TEST_ASSERT_TRUE(strstr(buf, "self_paced") != nullptr);
    TEST_ASSERT_TRUE(strstr(buf, "Warmup") != nullptr);
    TEST_ASSERT_TRUE(strstr(buf, "Cooldown") != nullptr);
    WorkoutDoc d;
    char perr[64];
    TEST_ASSERT_TRUE_MESSAGE(workoutParseJson(buf, d, perr, sizeof(perr)), perr);
    // 2 warmup + 8 interval + 1 steady + 1 free + 2 cooldown = 14
    TEST_ASSERT_EQUAL_UINT8(14, d.stepCount);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 50.0f, d.steps[0].ftpPct);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 70.0f, d.steps[1].ftpPct);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 105.0f, d.steps[2].ftpPct);
    TEST_ASSERT_TRUE(d.steps[11].selfPaced);
}

static void test_zwo_cooldown_direction(void) {
    const char* xml =
        "<workout_file><name>Cool</name><workout>"
        "<Cooldown Duration=\"300\" PowerLow=\"0.40\" PowerHigh=\"0.60\"/>"
        "</workout></workout_file>";
    char buf[1024];
    char err[64];
    TEST_ASSERT_TRUE(zwoToJson(xml, buf, sizeof(buf), err, sizeof(err)) > 0);
    WorkoutDoc d;
    TEST_ASSERT_TRUE(workoutParseJson(buf, d, err, sizeof(err)));
    TEST_ASSERT_EQUAL_UINT8(2, d.stepCount);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 60.0f, d.steps[0].ftpPct);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 40.0f, d.steps[1].ftpPct);
}

static void test_zwo_reject_empty(void) {
    char buf[256];
    char err[64];
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)zwoToJson("<workout></workout>", buf, sizeof(buf), err,
                                                     sizeof(err)));
    TEST_ASSERT_TRUE(err[0] != 0);
}

static void test_zwo_powerlow_steady(void) {
    const char* xml =
        "<workout_file><name>PL</name><workout>"
        "<SteadyState Duration=\"60\" PowerLow=\"0.80\" PowerHigh=\"0.80\"/>"
        "</workout></workout_file>";
    char buf[512];
    char err[64];
    TEST_ASSERT_TRUE(zwoToJson(xml, buf, sizeof(buf), err, sizeof(err)) > 0);
    WorkoutDoc d;
    TEST_ASSERT_TRUE(workoutParseJson(buf, d, err, sizeof(err)));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 80.0f, d.steps[0].ftpPct);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_zwo_demo);
    RUN_TEST(test_zwo_cooldown_direction);
    RUN_TEST(test_zwo_reject_empty);
    RUN_TEST(test_zwo_powerlow_steady);
    return UNITY_END();
}
