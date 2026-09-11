#include <unity.h>

#include "control/ControlMode.h"

using namespace ergo;

static void test_boot_off(void) {
    ControlState c;
    TEST_ASSERT_EQUAL_INT((int)ControlMode::Off, (int)c.mode());
    TEST_ASSERT_FALSE(c.allowsLevelWrite());
    TEST_ASSERT_FALSE(c.sessionActive());
}

static void test_manual_level(void) {
    ControlState c;
    TEST_ASSERT_TRUE(c.setMode(ControlMode::ManualLevel));
    TEST_ASSERT_TRUE(c.allowsLevelWrite());
    TEST_ASSERT_TRUE(c.sessionActive());
    TEST_ASSERT_TRUE(c.setLevelTargetTenths(100));
    TEST_ASSERT_EQUAL_INT16(100, c.levelTargetTenths());
}

static void test_off_clears_target(void) {
    ControlState c;
    TEST_ASSERT_TRUE(c.setMode(ControlMode::ManualLevel));
    TEST_ASSERT_TRUE(c.setLevelTargetTenths(80));
    TEST_ASSERT_TRUE(c.setMode(ControlMode::Off));
    TEST_ASSERT_EQUAL_INT16(-1, c.levelTargetTenths());
    TEST_ASSERT_FALSE(c.allowsLevelWrite());
}

static void test_level_while_off_denied(void) {
    ControlState c;
    TEST_ASSERT_FALSE(c.setLevelTargetTenths(50));
}

static void test_manual_erg(void) {
    ControlState c;
    TEST_ASSERT_TRUE(c.setMode(ControlMode::ManualErg));
    TEST_ASSERT_TRUE(c.allowsErg());
    TEST_ASSERT_TRUE(c.allowsAnyLoadWrite());
    TEST_ASSERT_FALSE(c.allowsLevelWrite());
    TEST_ASSERT_TRUE(c.setPowerTargetW(100.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 100.0f, c.powerTargetW());
    TEST_ASSERT_FALSE(c.setLevelTargetTenths(80));
}

static void test_manual_hr(void) {
    ControlState c;
    TEST_ASSERT_TRUE(c.setMode(ControlMode::HrHold));
    TEST_ASSERT_TRUE(c.allowsHrHold());
    TEST_ASSERT_TRUE(c.allowsErg());
    TEST_ASSERT_TRUE(c.setHrTargetBpm(140));
    TEST_ASSERT_EQUAL_UINT8(140, c.hrTargetBpm());
    TEST_ASSERT_TRUE(c.setPowerTargetW(70.0f));
}

static void test_reha(void) {
    ControlState c;
    TEST_ASSERT_TRUE(c.setMode(ControlMode::Reha));
    TEST_ASSERT_TRUE(c.allowsReha());
    TEST_ASSERT_TRUE(c.allowsErg());
    TEST_ASSERT_FALSE(c.allowsHrHold());
    TEST_ASSERT_TRUE(c.setPowerTargetW(60.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 60.0f, c.powerTargetW());
    TEST_ASSERT_FALSE(c.setHrTargetBpm(120));
}

static void test_unimplemented_modes(void) {
    ControlState c;
    TEST_ASSERT_FALSE(c.setMode(ControlMode::Workout));
    TEST_ASSERT_FALSE(c.setMode(ControlMode::Sim));
    TEST_ASSERT_EQUAL_INT((int)ControlMode::Off, (int)c.mode());
}

static void test_token_parse(void) {
    ControlMode m;
    TEST_ASSERT_TRUE(controlModeFromToken("off", m));
    TEST_ASSERT_EQUAL_INT((int)ControlMode::Off, (int)m);
    TEST_ASSERT_TRUE(controlModeFromToken("level", m));
    TEST_ASSERT_EQUAL_INT((int)ControlMode::ManualLevel, (int)m);
    TEST_ASSERT_TRUE(controlModeFromToken("erg", m));
    TEST_ASSERT_EQUAL_INT((int)ControlMode::ManualErg, (int)m);
    TEST_ASSERT_TRUE(controlModeFromToken("hr", m));
    TEST_ASSERT_EQUAL_INT((int)ControlMode::HrHold, (int)m);
    TEST_ASSERT_TRUE(controlModeFromToken("reha", m));
    TEST_ASSERT_EQUAL_INT((int)ControlMode::Reha, (int)m);
    TEST_ASSERT_TRUE(controlModeFromToken("physio", m));
    TEST_ASSERT_EQUAL_INT((int)ControlMode::Reha, (int)m);
    TEST_ASSERT_FALSE(controlModeFromToken("nope", m));
}

static void test_names(void) {
    TEST_ASSERT_EQUAL_STRING("OFF", controlModeName(ControlMode::Off));
    TEST_ASSERT_EQUAL_STRING("MANUAL_LEVEL", controlModeName(ControlMode::ManualLevel));
    TEST_ASSERT_EQUAL_STRING("MANUAL_ERG", controlModeName(ControlMode::ManualErg));
    TEST_ASSERT_EQUAL_STRING("HR_HOLD", controlModeName(ControlMode::HrHold));
    TEST_ASSERT_EQUAL_STRING("REHA", controlModeName(ControlMode::Reha));
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_boot_off);
    RUN_TEST(test_manual_level);
    RUN_TEST(test_manual_erg);
    RUN_TEST(test_manual_hr);
    RUN_TEST(test_reha);
    RUN_TEST(test_off_clears_target);
    RUN_TEST(test_level_while_off_denied);
    RUN_TEST(test_unimplemented_modes);
    RUN_TEST(test_token_parse);
    RUN_TEST(test_names);
    return UNITY_END();
}
