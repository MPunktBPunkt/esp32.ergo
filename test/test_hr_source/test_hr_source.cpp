#include <unity.h>

#include "ble/HrSource.h"
#include "control/HrController.h"
#include "control/RehaController.h"

using namespace ergo;

static void test_usable_strap_and_relay(void) {
    TEST_ASSERT_TRUE(hrUsableForControl(HrSource::Strap));
    TEST_ASSERT_TRUE(hrUsableForControl(HrSource::Relay));
}

static void test_machine_and_none_rejected(void) {
    TEST_ASSERT_FALSE(hrUsableForControl(HrSource::Machine));
    TEST_ASSERT_FALSE(hrUsableForControl(HrSource::None));
}

/** Reha: Strap → Machine (hrFresh false) → lost, kein neuer Cap-Eingriff. */
static void test_reha_machine_is_loss_not_intervention(void) {
    RehaController r;
    RehaControllerConfig cfg;
    cfg.lostAfterMs = 2000;
    r.begin(cfg);
    r.setDesiredW(60.0f);
    r.setHrLimits(115, 120);
    r.setDurationS(0);
    r.tick(0, 100, true);  // Strap ok
    TEST_ASSERT_FALSE(r.lost());
    TEST_ASSERT_FALSE(r.capActive());
    TEST_ASSERT_EQUAL_UINT16(0, r.interventions());
    // Quelle wechselt auf Machine → App setzt hrFresh=false (BPM egal)
    TEST_ASSERT_FALSE(r.tick(1000, 0, false).lost);
    const auto t = r.tick(3500, 0, false);
    TEST_ASSERT_TRUE(t.lost);
    TEST_ASSERT_FALSE(t.capActive);
    TEST_ASSERT_EQUAL_UINT16(0, t.interventions);
}

/** HR_HOLD: frischer Relay-Puls (hrFresh) hält lost=false. */
static void test_hrhold_relay_fresh_ok(void) {
    HrController h;
    HrControllerConfig cfg;
    cfg.lostAfterMs = 3000;
    cfg.basePowerW = 80.0f;
    h.begin(cfg);
    h.setTargetHr(130);
    TEST_ASSERT_FALSE(h.tick(0, 128, true, 0).lost);
    TEST_ASSERT_FALSE(h.tick(1000, 130, true, 0).lost);
}

/** Nach Verlust und Rückkehr: lost endet, Zielwatt ohne Sprung-Reset. */
static void test_hrhold_recover_after_loss(void) {
    HrController h;
    HrControllerConfig cfg;
    cfg.lostAfterMs = 2000;
    cfg.basePowerW = 80.0f;
    cfg.iGain = 0.0f;  // kein I → stabile Basis
    h.begin(cfg);
    h.setTargetHr(130);
    h.tick(0, 130, true, 0);
    const float before = h.powerTargetW();
    TEST_ASSERT_TRUE(h.tick(3000, 0, false, 0).lost);
    const auto back = h.tick(3500, 130, true, 0);
    TEST_ASSERT_FALSE(back.lost);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, before, h.powerTargetW());
}

void setUp(void) {}
void tearDown(void) {}

int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_usable_strap_and_relay);
    RUN_TEST(test_machine_and_none_rejected);
    RUN_TEST(test_reha_machine_is_loss_not_intervention);
    RUN_TEST(test_hrhold_relay_fresh_ok);
    RUN_TEST(test_hrhold_recover_after_loss);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    runUnityTests();
}
void loop() {}
#else
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    return runUnityTests();
}
#endif
