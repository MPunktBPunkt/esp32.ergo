/**
 * Nativer Test des Rohbyte-Rings.
 *
 *   pio test -e native
 *
 * Der eigentliche Punkt ist das Ausgabeformat: eine Zeile aus diesem Ring muss
 * `tools/make-fixtures.py` unveraendert als `bike-data.jsonl` schmecken, sonst
 * ist die Aufzeichnung einer echten Fahrt nicht als Fixture verwertbar. Der
 * Erzeuger liest `uuid`, `dir` und `hex`.
 */

#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "ble/DebugRing.h"

using namespace ergo;

void setUp(void) {}
void tearDown(void) {}

/** Ein echtes 0x2AD2 des Varon: flags 0x0B54, 19 Byte. */
static const uint8_t kIbd[19] = {0x54, 0x0B, 0x64, 0x00, 0x76, 0x00, 0x2C, 0x01, 0x00,
                                 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x00,
                                 0x00};

static void feedIbd(DebugRing& r, int n, uint32_t& t, uint16_t flags = 0x0B54) {
    uint8_t p[19];
    memcpy(p, kIbd, sizeof(p));
    p[0] = (uint8_t)(flags & 0xFF);
    p[1] = (uint8_t)(flags >> 8);
    for (int i = 0; i < n; i++) {
        r.add(0x2AD2, DebugRing::Dir::Notify, p, sizeof(p), t);
        t += 500;
    }
}

// ──────────────────────────────────────────────────── Aus ist Standard

static void test_disabled_records_nothing(void) {
    DebugRing r;
    uint32_t t = 1000;
    TEST_ASSERT_FALSE(r.enabled());
    feedIbd(r, 10, t);
    TEST_ASSERT_EQUAL_UINT16(0, r.count());
    // Gesehen wird trotzdem gezaehlt — sonst weiss niemand, was ihm fehlt.
    TEST_ASSERT_EQUAL_UINT32(10, r.seen());
}

// ──────────────────────────────────────────────────────── Ausduennung

static void test_thinning_keeps_every_nth(void) {
    DebugRing r;
    r.setEnabled(true);
    r.setIbdEvery(5);
    uint32_t t = 1000;
    feedIbd(r, 20, t);
    // Das erste Paket bringt ein neues Flagwort und wird immer behalten,
    // danach jedes fuenfte: 5, 10, 15, 20.
    TEST_ASSERT_EQUAL_UINT16(5, r.count());
    TEST_ASSERT_EQUAL_UINT32(15, r.thinned());
}

static void test_thinning_off_keeps_all(void) {
    DebugRing r;
    r.setEnabled(true);
    r.setIbdEvery(1);
    uint32_t t = 0;
    feedIbd(r, 12, t);
    TEST_ASSERT_EQUAL_UINT16(12, r.count());
    TEST_ASSERT_EQUAL_UINT32(0, r.thinned());
}

/**
 * Ein neues Flagwort wird immer behalten, auch mitten in der Ausduennung.
 * Fuer Fixtures ist genau das der wertvolle Datensatz — ein Paketlayout, das
 * der Decoder noch nicht gesehen hat.
 */
static void test_new_flags_always_kept(void) {
    DebugRing r;
    r.setEnabled(true);
    r.setIbdEvery(100);  // praktisch alles ausduennen
    uint32_t t = 0;
    feedIbd(r, 3, t, 0x0B54);
    TEST_ASSERT_EQUAL_UINT16(1, r.count());  // nur das erste
    feedIbd(r, 3, t, 0x0044);                // anderes Layout
    TEST_ASSERT_EQUAL_UINT16(2, r.count());
    feedIbd(r, 3, t, 0x0B54);                // und zurueck
    TEST_ASSERT_EQUAL_UINT16(3, r.count());
}

/** Steuerverkehr wird nie ausgeduennt — er ist selten und kostbar. */
static void test_control_traffic_never_thinned(void) {
    DebugRing r;
    r.setEnabled(true);
    r.setIbdEvery(100);
    uint32_t t = 0;
    const uint8_t w[3] = {0x04, 0x50, 0x00};
    const uint8_t resp[3] = {0x80, 0x04, 0x01};
    for (int i = 0; i < 6; i++) {
        r.add(0x2AD9, DebugRing::Dir::Write, w, sizeof(w), t++);
        r.add(0x2AD9, DebugRing::Dir::Notify, resp, sizeof(resp), t++);
    }
    TEST_ASSERT_EQUAL_UINT16(12, r.count());
    TEST_ASSERT_EQUAL_UINT32(0, r.thinned());
}

// ───────────────────────────────────────────────────────────── Ring

static void test_overwrites_oldest(void) {
    DebugRing r;
    r.setEnabled(true);
    r.setIbdEvery(1);
    uint32_t t = 0;
    feedIbd(r, kRingSlots + 10, t);
    TEST_ASSERT_EQUAL_UINT16(kRingSlots, r.count());
    TEST_ASSERT_EQUAL_UINT32(10, r.overwritten());
    // Der aelteste ueberlebende ist der elfte, also t = 10 * 500.
    const DebugRing::Rec* first = r.at(0);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_EQUAL_UINT32(5000, first->atMs);
    const DebugRing::Rec* last = r.at((uint16_t)(kRingSlots - 1));
    TEST_ASSERT_NOT_NULL(last);
    TEST_ASSERT_EQUAL_UINT32((kRingSlots + 9) * 500, last->atMs);
    TEST_ASSERT_NULL(r.at(kRingSlots));
}

static void test_order_before_wrap(void) {
    DebugRing r;
    r.setEnabled(true);
    r.setIbdEvery(1);
    uint32_t t = 100;
    feedIbd(r, 3, t);
    TEST_ASSERT_EQUAL_UINT32(100, r.at(0)->atMs);
    TEST_ASSERT_EQUAL_UINT32(600, r.at(1)->atMs);
    TEST_ASSERT_EQUAL_UINT32(1100, r.at(2)->atMs);
}

static void test_long_payload_is_truncated_not_dropped(void) {
    DebugRing r;
    r.setEnabled(true);
    uint8_t big[40];
    for (int i = 0; i < 40; i++) big[i] = (uint8_t)i;
    r.add(0x2AD2, DebugRing::Dir::Notify, big, sizeof(big), 7);
    TEST_ASSERT_EQUAL_UINT16(1, r.count());
    TEST_ASSERT_EQUAL_UINT8(kRingPayload, r.at(0)->len);
    TEST_ASSERT_EQUAL_UINT32(1, r.truncated());
}

static void test_clear_resets_counters(void) {
    DebugRing r;
    r.setEnabled(true);
    uint32_t t = 0;
    feedIbd(r, 30, t);
    r.clear();
    TEST_ASSERT_EQUAL_UINT16(0, r.count());
    TEST_ASSERT_EQUAL_UINT32(0, r.seen());
    TEST_ASSERT_EQUAL_UINT32(0, r.thinned());
    TEST_ASSERT_NULL(r.at(0));
    // Nach dem Leeren ist das naechste Paket wieder ein neues Layout.
    feedIbd(r, 1, t);
    TEST_ASSERT_EQUAL_UINT16(1, r.count());
}

// ────────────────────────────────────────────── Das Ausgabeformat

/**
 * Die Zeile, die `tools/make-fixtures.py` lesen muss. Reihenfolge und
 * Schreibweise sind nicht kosmetisch: der Erzeuger filtert auf
 * `uuid == "2AD2"` und `dir == "notify"` und nimmt `hex` in Grossbuchstaben.
 */
static void test_jsonl_line(void) {
    DebugRing r;
    r.setEnabled(true);
    const uint8_t p[4] = {0x54, 0x0B, 0x00, 0xFF};
    r.add(0x2AD2, DebugRing::Dir::Notify, p, sizeof(p), 123456);

    char line[256];
    const size_t n = DebugRing::formatLine(*r.at(0), line, sizeof(line));
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_STRING("{\"t\":123456,\"uuid\":\"2AD2\",\"dir\":\"notify\",\"hex\":\"540B00FF\"}",
                             line);
    TEST_ASSERT_EQUAL_UINT32(strlen(line), n);
}

static void test_jsonl_write_direction(void) {
    DebugRing r;
    r.setEnabled(true);
    const uint8_t w[3] = {0x04, 0x50, 0x00};
    r.add(0x2AD9, DebugRing::Dir::Write, w, sizeof(w), 42);
    char line[256];
    DebugRing::formatLine(*r.at(0), line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("{\"t\":42,\"uuid\":\"2AD9\",\"dir\":\"write\",\"hex\":\"045000\"}",
                             line);
}

/** Zu kleiner Puffer liefert 0 und schreibt nichts Halbes hinaus. */
static void test_jsonl_needs_room(void) {
    DebugRing r;
    r.setEnabled(true);
    r.add(0x2AD2, DebugRing::Dir::Notify, kIbd, sizeof(kIbd), 1);
    char small[20];
    TEST_ASSERT_EQUAL_UINT32(0, DebugRing::formatLine(*r.at(0), small, sizeof(small)));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_disabled_records_nothing);
    RUN_TEST(test_thinning_keeps_every_nth);
    RUN_TEST(test_thinning_off_keeps_all);
    RUN_TEST(test_new_flags_always_kept);
    RUN_TEST(test_control_traffic_never_thinned);
    RUN_TEST(test_overwrites_oldest);
    RUN_TEST(test_order_before_wrap);
    RUN_TEST(test_long_payload_is_truncated_not_dropped);
    RUN_TEST(test_clear_resets_counters);
    RUN_TEST(test_jsonl_line);
    RUN_TEST(test_jsonl_write_direction);
    RUN_TEST(test_jsonl_needs_room);
    return UNITY_END();
}
