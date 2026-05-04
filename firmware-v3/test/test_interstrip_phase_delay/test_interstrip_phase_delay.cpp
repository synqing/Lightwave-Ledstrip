// Phase 3 Move 3.3 — InterStripPhaseDelay substrate tests.

#include <unity.h>
#include <cstddef>
#include <cstdint>

#include <FastLED.h>

#include "effects/persistence/InterStripPhaseDelay.h"

using lightwaveos::effects::persistence::InterStripPhaseDelay;

namespace {

constexpr size_t kStrip = 6;
constexpr size_t kSlots = 3;

using TestDelay = InterStripPhaseDelay<kStrip, kSlots>;

void fillFrame(CRGB* frame, uint8_t tag) {
    for (size_t i = 0; i < kStrip; ++i) {
        frame[i].r = tag;
        frame[i].g = static_cast<uint8_t>(i);
        frame[i].b = static_cast<uint8_t>(tag + i);
    }
}

bool frameMatches(const CRGB* frame, uint8_t tag) {
    for (size_t i = 0; i < kStrip; ++i) {
        if (frame[i].r != tag ||
            frame[i].g != static_cast<uint8_t>(i) ||
            frame[i].b != static_cast<uint8_t>(tag + i)) {
            return false;
        }
    }
    return true;
}

void test_constructs_valid_empty_delay_line() {
    TestDelay delay;
    TEST_ASSERT_TRUE(delay.isValid());
    TEST_ASSERT_EQUAL_size_t(0, delay.count());
    TEST_ASSERT_EQUAL_size_t(kStrip, TestDelay::stripLen());
    TEST_ASSERT_EQUAL_size_t(kSlots, TestDelay::capacity());
}

void test_push_pair_preserves_newest_strip_frames() {
    TestDelay delay;
    CRGB a[kStrip], b[kStrip], outA[kStrip], outB[kStrip];

    fillFrame(a, 0x11);
    fillFrame(b, 0x21);
    delay.pushPair(a, b);

    TEST_ASSERT_EQUAL_size_t(1, delay.count());
    TEST_ASSERT_TRUE(delay.atOffset(0, outA, outB));
    TEST_ASSERT_TRUE(frameMatches(outA, 0x11));
    TEST_ASSERT_TRUE(frameMatches(outB, 0x21));
}

void test_offset_reads_walk_back_through_paired_history() {
    TestDelay delay;
    CRGB a[kStrip], b[kStrip], outA[kStrip], outB[kStrip];

    for (uint8_t i = 1; i <= 3; ++i) {
        fillFrame(a, static_cast<uint8_t>(0x10 + i));
        fillFrame(b, static_cast<uint8_t>(0x40 + i));
        delay.pushPair(a, b);
    }

    TEST_ASSERT_TRUE(delay.atOffset(0, outA, outB));
    TEST_ASSERT_TRUE(frameMatches(outA, 0x13));
    TEST_ASSERT_TRUE(frameMatches(outB, 0x43));

    TEST_ASSERT_TRUE(delay.atOffset(2, outA, outB));
    TEST_ASSERT_TRUE(frameMatches(outA, 0x11));
    TEST_ASSERT_TRUE(frameMatches(outB, 0x41));
}

void test_wraparound_evicts_oldest_pair() {
    TestDelay delay;
    CRGB a[kStrip], b[kStrip], outA[kStrip], outB[kStrip];

    for (uint8_t i = 1; i <= 5; ++i) {
        fillFrame(a, static_cast<uint8_t>(0x20 + i));
        fillFrame(b, static_cast<uint8_t>(0x60 + i));
        delay.pushPair(a, b);
    }

    TEST_ASSERT_EQUAL_size_t(kSlots, delay.count());
    TEST_ASSERT_TRUE(delay.atOffset(kSlots - 1, outA, outB));
    TEST_ASSERT_TRUE(frameMatches(outA, 0x23));
    TEST_ASSERT_TRUE(frameMatches(outB, 0x63));
}

void test_null_pair_push_is_noop() {
    TestDelay delay;
    CRGB a[kStrip], b[kStrip], outA[kStrip], outB[kStrip];

    fillFrame(a, 0x31);
    fillFrame(b, 0x71);
    delay.pushPair(a, b);
    delay.pushPair(nullptr, b);
    delay.pushPair(a, nullptr);

    TEST_ASSERT_EQUAL_size_t(1, delay.count());
    TEST_ASSERT_TRUE(delay.atOffset(0, outA, outB));
    TEST_ASSERT_TRUE(frameMatches(outA, 0x31));
    TEST_ASSERT_TRUE(frameMatches(outB, 0x71));
}

void test_invalid_reads_leave_outputs_untouched() {
    TestDelay delay;
    CRGB a[kStrip], b[kStrip], outA[kStrip], outB[kStrip];

    fillFrame(a, 0x44);
    fillFrame(b, 0x84);
    fillFrame(outA, 0xAA);
    fillFrame(outB, 0xBB);

    TEST_ASSERT_FALSE(delay.atOffset(0, outA, outB));
    TEST_ASSERT_TRUE(frameMatches(outA, 0xAA));
    TEST_ASSERT_TRUE(frameMatches(outB, 0xBB));

    delay.pushPair(a, b);
    TEST_ASSERT_FALSE(delay.atOffset(1, outA, outB));
    TEST_ASSERT_FALSE(delay.atOffset(0, nullptr, outB));
    TEST_ASSERT_FALSE(delay.atOffset(0, outA, nullptr));
}

void test_strip_specific_reads_and_invalid_strip_index() {
    TestDelay delay;
    CRGB a[kStrip], b[kStrip], out[kStrip];

    fillFrame(a, 0x55);
    fillFrame(b, 0x95);
    delay.pushPair(a, b);

    TEST_ASSERT_TRUE(delay.stripAtOffset(0, 0, out));
    TEST_ASSERT_TRUE(frameMatches(out, 0x55));
    TEST_ASSERT_TRUE(delay.stripAtOffset(1, 0, out));
    TEST_ASSERT_TRUE(frameMatches(out, 0x95));
    TEST_ASSERT_FALSE(delay.stripAtOffset(2, 0, out));
}

void test_clear_resets_both_strip_rings() {
    TestDelay delay;
    CRGB a[kStrip], b[kStrip], outA[kStrip], outB[kStrip];

    fillFrame(a, 0x66);
    fillFrame(b, 0xA6);
    delay.pushPair(a, b);
    delay.clear();

    TEST_ASSERT_EQUAL_size_t(0, delay.count());
    TEST_ASSERT_FALSE(delay.atOffset(0, outA, outB));
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_constructs_valid_empty_delay_line);
    RUN_TEST(test_push_pair_preserves_newest_strip_frames);
    RUN_TEST(test_offset_reads_walk_back_through_paired_history);
    RUN_TEST(test_wraparound_evicts_oldest_pair);
    RUN_TEST(test_null_pair_push_is_noop);
    RUN_TEST(test_invalid_reads_leave_outputs_untouched);
    RUN_TEST(test_strip_specific_reads_and_invalid_strip_index);
    RUN_TEST(test_clear_resets_both_strip_rings);
    return UNITY_END();
}
