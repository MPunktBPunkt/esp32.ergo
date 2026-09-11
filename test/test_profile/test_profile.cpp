#include <unity.h>

#include "control/Limiter.h"
#include "core/Profile.h"

using namespace ergo;

static Profile make(const char* id, const char* name, int16_t maxLvl, int16_t maxW, uint8_t maxHr = 0) {
    Profile p;
    profileCopyId(p.id, sizeof(p.id), id);
    profileCopyId(p.name, sizeof(p.name), name);
    p.maxLevelTenths = maxLvl;
    p.maxPowerW = maxW;
    p.maxHr = maxHr;
    return p;
}

static void test_empty_has_no_active(void) {
    ProfileStore s;
    TEST_ASSERT_EQUAL_UINT8(0, s.count());
    TEST_ASSERT_NULL(s.active());
    TEST_ASSERT_NULL(s.activeId());
}

static void test_put_get_list(void) {
    ProfileStore s;
    TEST_ASSERT_TRUE(s.put(make("anna", "Anna", 100, 150)));
    TEST_ASSERT_TRUE(s.put(make("ben", "Ben", 120, 200)));
    TEST_ASSERT_EQUAL_UINT8(2, s.count());
    Profile out;
    TEST_ASSERT_TRUE(s.get("anna", out));
    TEST_ASSERT_EQUAL_STRING("Anna", out.name);
    TEST_ASSERT_EQUAL_INT16(100, out.maxLevelTenths);
}

static void test_put_rejects_bad(void) {
    ProfileStore s;
    Profile p = make("", "X", 0, 0);
    TEST_ASSERT_FALSE(s.put(p));
    p = make("ok", "", 0, 0);
    TEST_ASSERT_FALSE(s.put(p));
}

static void test_select_and_apply(void) {
    ProfileStore s;
    TEST_ASSERT_TRUE(s.put(make("anna", "Anna", 100, 150)));
    TEST_ASSERT_TRUE(s.select("anna"));
    TEST_ASSERT_NOT_NULL(s.active());
    TEST_ASSERT_EQUAL_STRING("anna", s.activeId());

    LimiterConfig lc;
    s.applyTo(lc);
    TEST_ASSERT_EQUAL_INT16(100, lc.profileMaxLevelTenths);
    TEST_ASSERT_EQUAL_INT16(150, lc.profileMaxPowerW);
}

static void test_select_unknown(void) {
    ProfileStore s;
    TEST_ASSERT_TRUE(s.put(make("anna", "Anna", 100, 150)));
    TEST_ASSERT_TRUE(s.select("anna"));
    TEST_ASSERT_FALSE(s.select("ghost"));
    TEST_ASSERT_EQUAL_STRING("anna", s.activeId());
}

static void test_select_locked_while_session(void) {
    ProfileStore s;
    TEST_ASSERT_TRUE(s.put(make("anna", "Anna", 100, 150)));
    TEST_ASSERT_TRUE(s.put(make("ben", "Ben", 80, 100)));
    TEST_ASSERT_TRUE(s.select("anna"));
    TEST_ASSERT_FALSE(s.select("ben", /*sessionLocked=*/true));
    TEST_ASSERT_EQUAL_STRING("anna", s.activeId());
}

static void test_hr_ceiling_clamped(void) {
    ProfileStore s;
    Profile p = make("reha", "Reha", 60, 80, 220);
    TEST_ASSERT_TRUE(s.put(p));
    Profile out;
    TEST_ASSERT_TRUE(s.get("reha", out));
    TEST_ASSERT_EQUAL_UINT8(ProfileStore::kHrCeilingMax, out.maxHr);
}

static void test_remove_clears_active(void) {
    ProfileStore s;
    TEST_ASSERT_TRUE(s.put(make("anna", "Anna", 100, 150)));
    TEST_ASSERT_TRUE(s.select("anna"));
    TEST_ASSERT_TRUE(s.remove("anna"));
    TEST_ASSERT_NULL(s.active());
    LimiterConfig lc;
    lc.profileMaxLevelTenths = 999;
    s.applyTo(lc);
    TEST_ASSERT_EQUAL_INT16(0, lc.profileMaxLevelTenths);
}

static void test_full_rejects_fifth(void) {
    ProfileStore s;
    TEST_ASSERT_TRUE(s.put(make("a", "A", 0, 0)));
    TEST_ASSERT_TRUE(s.put(make("b", "B", 0, 0)));
    TEST_ASSERT_TRUE(s.put(make("c", "C", 0, 0)));
    TEST_ASSERT_TRUE(s.put(make("d", "D", 0, 0)));
    TEST_ASSERT_FALSE(s.put(make("e", "E", 0, 0)));
}

static void test_update_same_id(void) {
    ProfileStore s;
    TEST_ASSERT_TRUE(s.put(make("anna", "Anna", 100, 150)));
    TEST_ASSERT_TRUE(s.put(make("anna", "Anna2", 80, 120)));
    TEST_ASSERT_EQUAL_UINT8(1, s.count());
    Profile out;
    TEST_ASSERT_TRUE(s.get("anna", out));
    TEST_ASSERT_EQUAL_STRING("Anna2", out.name);
    TEST_ASSERT_EQUAL_INT16(80, out.maxLevelTenths);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_has_no_active);
    RUN_TEST(test_put_get_list);
    RUN_TEST(test_put_rejects_bad);
    RUN_TEST(test_select_and_apply);
    RUN_TEST(test_select_unknown);
    RUN_TEST(test_select_locked_while_session);
    RUN_TEST(test_hr_ceiling_clamped);
    RUN_TEST(test_remove_clears_active);
    RUN_TEST(test_full_rejects_fifth);
    RUN_TEST(test_update_same_id);
    return UNITY_END();
}
