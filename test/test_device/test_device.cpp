/**
 * Nativer Test fuer DeviceStore (Geraeteprofil je MAC).
 *
 *   pio test -e native -f test_device
 */
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "core/DeviceStore.h"

using namespace ergo;
using namespace ftms;

void setUp(void) {}
void tearDown(void) {}

static void test_normalize_mac(void) {
    char out[18];
    TEST_ASSERT_TRUE(DeviceStore::normalizeMac("C2:32:A5:1E:BF:B5", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("c2:32:a5:1e:bf:b5", out);
    TEST_ASSERT_TRUE(DeviceStore::normalizeMac("c232a51ebfb5", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("c2:32:a5:1e:bf:b5", out);
    TEST_ASSERT_FALSE(DeviceStore::normalizeMac("zz", out, sizeof(out)));
    TEST_ASSERT_FALSE(DeviceStore::normalizeMac("", out, sizeof(out)));
}

static void test_remember_select_roundtrip(void) {
    DeviceStore s;
    TEST_ASSERT_TRUE(s.remember("c2:32:a5:1e:bf:b5", "TC174", 0, 1700000001UL));
    TEST_ASSERT_EQUAL_UINT8(1, s.count());
    TEST_ASSERT_NOT_NULL(s.active());
    TEST_ASSERT_EQUAL_STRING("c2:32:a5:1e:bf:b5", s.active()->mac);
    TEST_ASSERT_EQUAL_STRING("TC174", s.active()->name);
    // Varon-Defaults
    TEST_ASSERT_EQUAL((int)ResistanceFormat::Sint16, (int)s.active()->resistanceFormat);
    TEST_ASSERT_EQUAL_INT8(0, s.active()->powerTrusted);

    TEST_ASSERT_TRUE(s.remember("AA:BB:CC:DD:EE:FF", "Other", 1, 0));
    TEST_ASSERT_EQUAL_UINT8(2, s.count());
    TEST_ASSERT_EQUAL_STRING("aa:bb:cc:dd:ee:ff", s.active()->mac);
    TEST_ASSERT_TRUE(s.select("c2:32:a5:1e:bf:b5"));
    TEST_ASSERT_EQUAL_STRING("TC174", s.active()->name);
    TEST_ASSERT_EQUAL(0, s.mapSlot());

    uint8_t buf[DeviceStore::kMaxBytes];
    const size_t n = s.save(buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);

    DeviceStore b;
    TEST_ASSERT_TRUE(b.load(buf, n));
    TEST_ASSERT_EQUAL_UINT8(2, b.count());
    TEST_ASSERT_EQUAL_STRING("TC174", b.active()->name);
    TEST_ASSERT_EQUAL((int)ResistanceFormat::Sint16, (int)b.active()->resistanceFormat);
}

static void test_apply_overrides(void) {
    DeviceStore s;
    TEST_ASSERT_TRUE(s.remember("11:22:33:44:55:66", "X", 0));
    DeviceProfile* d = s.activeMutable();
    d->resistanceFormat = ResistanceFormat::Uint8;
    d->powerTrusted = 1;

    Capabilities caps;
    caps.valid = true;
    caps.resistanceFormat = ResistanceFormat::Sint16;
    caps.powerTargetTrusted = false;
    s.applyTo(caps);
    TEST_ASSERT_EQUAL((int)ResistanceFormat::Uint8, (int)caps.resistanceFormat);
    TEST_ASSERT_TRUE(caps.powerTargetTrusted);
}

static void test_full_rejects_fifth(void) {
    DeviceStore s;
    TEST_ASSERT_TRUE(s.remember("00:00:00:00:00:01", "a", 0));
    TEST_ASSERT_TRUE(s.remember("00:00:00:00:00:02", "b", 0));
    TEST_ASSERT_TRUE(s.remember("00:00:00:00:00:03", "c", 0));
    TEST_ASSERT_TRUE(s.remember("00:00:00:00:00:04", "d", 0));
    TEST_ASSERT_FALSE(s.remember("00:00:00:00:00:05", "e", 0));
    TEST_ASSERT_EQUAL_UINT8(4, s.count());
}

static void test_remove(void) {
    DeviceStore s;
    TEST_ASSERT_TRUE(s.remember("00:00:00:00:00:01", "a", 0));
    TEST_ASSERT_TRUE(s.remember("00:00:00:00:00:02", "b", 0));
    TEST_ASSERT_TRUE(s.remove("00:00:00:00:00:01"));
    TEST_ASSERT_EQUAL_UINT8(1, s.count());
    TEST_ASSERT_EQUAL_STRING("b", s.active()->name);
}

static void test_load_reseeds_varon_auto_power(void) {
    // Altbestand: powerTrusted noch „auto“ (−1) → nach load nie vertrauen.
    DeviceStore s;
    TEST_ASSERT_TRUE(s.remember("aa:bb:cc:dd:ee:ff", "Other", 0));
    DeviceProfile* o = s.activeMutable();
    o->powerTrusted = -1;
    o->resistanceFormat = ResistanceFormat::Unknown;
    // zweite Slot: Varon-MAC mit auto
    TEST_ASSERT_TRUE(s.remember("c2:32:a5:1e:bf:b5", "TC174", 0));
    DeviceProfile* v = s.activeMutable();
    v->powerTrusted = -1;
    v->resistanceFormat = ResistanceFormat::Unknown;

    uint8_t buf[DeviceStore::kMaxBytes];
    const size_t n = s.save(buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);

    DeviceStore b;
    TEST_ASSERT_TRUE(b.load(buf, n));
    TEST_ASSERT_TRUE(b.select("c2:32:a5:1e:bf:b5"));
    TEST_ASSERT_EQUAL_INT8(0, b.active()->powerTrusted);
    TEST_ASSERT_EQUAL((int)ResistanceFormat::Sint16, (int)b.active()->resistanceFormat);
    TEST_ASSERT_TRUE(b.select("aa:bb:cc:dd:ee:ff"));
    TEST_ASSERT_EQUAL_INT8(-1, b.active()->powerTrusted);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_normalize_mac);
    RUN_TEST(test_remember_select_roundtrip);
    RUN_TEST(test_apply_overrides);
    RUN_TEST(test_full_rejects_fifth);
    RUN_TEST(test_remove);
    RUN_TEST(test_load_reseeds_varon_auto_power);
    UNITY_END();
}

#ifndef ARDUINO
int main(int, char**) {
    setup();
    return 0;
}
#endif
