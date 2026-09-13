#include <unity.h>

#include "control/TestRunner.h"

using namespace ergo;

static void feed(TestRunner& t, uint16_t w, uint8_t hr, uint32_t& now, unsigned n) {
    for (unsigned i = 0; i < n; i++) {
        now += 1000;
        t.observe(w, hr, now);
    }
}

static void test_kind_from_id(void) {
    TEST_ASSERT_EQUAL((int)TestKind::Ramp, (int)TestRunner::kindFromWorkoutId("test_ramp"));
    TEST_ASSERT_EQUAL((int)TestKind::Ftp20, (int)TestRunner::kindFromWorkoutId("test_20min"));
    TEST_ASSERT_EQUAL((int)TestKind::Recovery, (int)TestRunner::kindFromWorkoutId("test_recovery"));
    TEST_ASSERT_EQUAL((int)TestKind::None, (int)TestRunner::kindFromWorkoutId("physio"));
}

static void test_ramp_map_and_ftp(void) {
    TestRunner t;
    uint32_t now = 1000;
    t.start(TestKind::Ramp, now);
    t.onStep(0, "60 W", now);
    feed(t, 100, 120, now, 30);
    feed(t, 200, 150, now, 60);  // best 60s @ 200
    feed(t, 80, 140, now, 10);
    t.finalize("done", now);
    const TestResult& r = t.result();
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_UINT16(200, r.mapW);
    TEST_ASSERT_EQUAL_UINT16(150, r.ftpPropose);  // 0.75 × 200
    TEST_ASSERT_EQUAL_UINT16(200, r.peakW);
}

static void test_ftp20_main_only(void) {
    TestRunner t;
    uint32_t now = 0;
    t.start(TestKind::Ftp20, now);
    t.onStep(0, "Warm 50%", now);
    feed(t, 50, 100, now, 20);
    t.onStep(2, "Haupt 20 min", now);
    feed(t, 200, 140, now, 40);
    t.onStep(3, "Cool", now);
    feed(t, 40, 110, now, 10);
    t.finalize("done", now);
    const TestResult& r = t.result();
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_UINT16(200, r.avgMainW);
    TEST_ASSERT_EQUAL_UINT16(190, r.ftpPropose);  // 0.95 × 200
    TEST_ASSERT_EQUAL_UINT32(40, r.mainSamples);
}

static void test_recovery_note(void) {
    TestRunner t;
    uint32_t now = 0;
    t.start(TestKind::Recovery, now);
    t.onStep(0, "Belastung", now);
    feed(t, 180, 160, now, 5);
    t.onStep(1, "Erholung 60s", now);
    // observe sets hrAtRecoverStart on first HR in recover phase
    now += 1000;
    t.observe(40, 155, now);
    feed(t, 40, 120, now, 59);  // drop 35 bpm → note ~8
    t.finalize("done", now);
    const TestResult& r = t.result();
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_UINT8(155, r.hrLoad);
    TEST_ASSERT_EQUAL_UINT8(120, r.hrRecover);
    TEST_ASSERT_TRUE(r.recoveryNote >= 7 && r.recoveryNote <= 10);
    TEST_ASSERT_EQUAL_UINT16(0, r.ftpPropose);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_kind_from_id);
    RUN_TEST(test_ramp_map_and_ftp);
    RUN_TEST(test_ftp20_main_only);
    RUN_TEST(test_recovery_note);
    return UNITY_END();
}
