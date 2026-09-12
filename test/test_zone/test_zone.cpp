#include <unity.h>

#include "core/Zone.h"

using namespace ergo;

static void test_power_boundaries(void) {
    TEST_ASSERT_EQUAL_UINT8(0, zoneFromPowerW(100, 0));
    TEST_ASSERT_EQUAL_UINT8(0, zoneFromPowerW(0, 200));
    TEST_ASSERT_EQUAL_UINT8(1, zoneFromPowerW(100, 200));   // 50 %
    TEST_ASSERT_EQUAL_UINT8(2, zoneFromPowerW(120, 200));   // 60 %
    TEST_ASSERT_EQUAL_UINT8(3, zoneFromPowerW(160, 200));   // 80 %
    TEST_ASSERT_EQUAL_UINT8(4, zoneFromPowerW(200, 200));   // 100 %
    TEST_ASSERT_EQUAL_UINT8(5, zoneFromPowerW(220, 200));   // 110 %
    TEST_ASSERT_EQUAL_UINT8(6, zoneFromPowerW(260, 200));   // 130 %
    TEST_ASSERT_EQUAL_UINT8(7, zoneFromPowerW(320, 200));   // 160 %
}

static void test_hr_boundaries(void) {
    TEST_ASSERT_EQUAL_UINT8(0, zoneFromHr(100, 0));
    TEST_ASSERT_EQUAL_UINT8(1, zoneFromHr(100, 200));  // 50 %
    TEST_ASSERT_EQUAL_UINT8(2, zoneFromHr(130, 200));  // 65 %
    TEST_ASSERT_EQUAL_UINT8(3, zoneFromHr(150, 200));  // 75 %
    TEST_ASSERT_EQUAL_UINT8(4, zoneFromHr(170, 200));  // 85 %
    TEST_ASSERT_EQUAL_UINT8(5, zoneFromHr(190, 200));  // 95 %
}

static void test_info_names(void) {
    ZoneInfo p = powerZoneInfo(3);
    TEST_ASSERT_EQUAL_UINT8(3, p.index);
    TEST_ASSERT_EQUAL_STRING("Z3", p.code);
    TEST_ASSERT_EQUAL_STRING("Tempo", p.name);
    ZoneInfo h = hrZoneInfo(5);
    TEST_ASSERT_EQUAL_STRING("Z5", h.code);
}

static void test_leading(void) {
    ZoneInfo a = leadingZoneInfo(false, 160, 200, 100, 180);
    TEST_ASSERT_EQUAL_UINT8(3, a.index);
    ZoneInfo b = leadingZoneInfo(true, 160, 200, 150, 180);
    // 150/180 ≈ 83 % → Z4
    TEST_ASSERT_EQUAL_UINT8(4, b.index);
}

static void test_power_hysteresis(void) {
    // An der Z2/Z3-Grenze (76 % von 200 = 152 W) nicht sofort hoch/runter.
    TEST_ASSERT_EQUAL_UINT8(2, zoneFromPowerW(150, 200, 2));  // 75 %
    TEST_ASSERT_EQUAL_UINT8(2, zoneFromPowerW(153, 200, 2));  // 76.5 % — noch Hyst
    TEST_ASSERT_EQUAL_UINT8(3, zoneFromPowerW(158, 200, 2));  // 79 % — klar drüber
    TEST_ASSERT_EQUAL_UINT8(3, zoneFromPowerW(153, 200, 3));  // bleibt in 3
    TEST_ASSERT_EQUAL_UINT8(2, zoneFromPowerW(145, 200, 3));  // unter 76−2.5
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_power_boundaries);
    RUN_TEST(test_hr_boundaries);
    RUN_TEST(test_info_names);
    RUN_TEST(test_leading);
    RUN_TEST(test_power_hysteresis);
    return UNITY_END();
}
