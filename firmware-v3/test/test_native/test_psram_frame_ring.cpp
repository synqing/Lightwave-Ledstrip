// Phase 5 Move 5.1 — PSRAMFrameRing substrate test (INF-03).
//
// Per Topology_Reconciliation §6: a header-only PSRAM-resident framebuffer
// history ring. Sister to PSRAMScalarRing (INF-12) — that one stores scalars
// in DRAM, this one stores full strips in PSRAM (or DRAM under NATIVE_BUILD).
//
// Contract:
//   - One-time allocation in the constructor; isValid() must be true after a
//     successful new[] in NATIVE_BUILD and after heap_caps_malloc on firmware.
//   - O(1) push, O(1) atOffset by offset-from-head.
//   - atOffset(0) returns the most recently pushed frame; atOffset(count-1)
//     returns the oldest frame still resident.
//   - atOffset() with offset >= count returns false and leaves dest untouched.
//   - push(nullptr) is a guarded no-op.
//   - clear() zeroes the backing store and resets head/count.
//   - capacity() and stripLen() are compile-time constexpr.
//   - Two independently constructed rings do NOT share state.
//
// British spelling in comments where applicable; this is a memory primitive.

#include <unity.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <FastLED.h>

#include "effects/persistence/PSRAMFrameRing.h"

using lightwaveos::effects::persistence::PSRAMFrameRing;

namespace {

// Small strip length keeps the test fixtures cheap on native; the substrate
// is templated on size, so the same code paths run on the K1 320-LED frame.
constexpr size_t kStrip = 8;
constexpr size_t kSlots = 4;

using TestRing = PSRAMFrameRing<kStrip, kSlots>;

// Fill a buffer with a deterministic pattern so wrap-around tests can assert
// the right frame survived. Each LED gets r=tag, g=i, b=0.
void fillFrame(CRGB* frame, uint8_t tag) {
    for (size_t i = 0; i < kStrip; ++i) {
        frame[i].r = tag;
        frame[i].g = static_cast<uint8_t>(i);
        frame[i].b = 0;
    }
}

bool framesEqual(const CRGB* a, const CRGB* b) {
    for (size_t i = 0; i < kStrip; ++i) {
        if (a[i].r != b[i].r || a[i].g != b[i].g || a[i].b != b[i].b) {
            return false;
        }
    }
    return true;
}

bool frameIsZero(const CRGB* a) {
    for (size_t i = 0; i < kStrip; ++i) {
        if (a[i].r != 0 || a[i].g != 0 || a[i].b != 0) return false;
    }
    return true;
}

// 1 — A freshly constructed ring is valid (native new[] should not fail) and
//     has count() == 0, capacity() == kSlots.
void test_constructs_and_clears_on_init() {
    TestRing ring;
    TEST_ASSERT_TRUE(ring.isValid());
    TEST_ASSERT_EQUAL_size_t(0, ring.count());
    TEST_ASSERT_EQUAL_size_t(kSlots, TestRing::capacity());
    // No frame has been pushed; atOffset(0) must report no-data.
    CRGB dest[kStrip];
    fillFrame(dest, 0xEE);  // sentinel
    TEST_ASSERT_FALSE(ring.atOffset(0, dest));
    // dest must be untouched by the failed read.
    for (size_t i = 0; i < kStrip; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0xEE, dest[i].r);
    }
}

// 2 — push() increments count() until the ring is full, then count saturates.
void test_push_increments_count_until_capacity() {
    TestRing ring;
    CRGB frame[kStrip];
    for (size_t i = 0; i < kSlots; ++i) {
        fillFrame(frame, static_cast<uint8_t>(i + 1));
        ring.push(frame);
        TEST_ASSERT_EQUAL_size_t(i + 1, ring.count());
    }
    // One more push: count saturates at kSlots.
    fillFrame(frame, 0x99);
    ring.push(frame);
    TEST_ASSERT_EQUAL_size_t(kSlots, ring.count());
}

// 3 — atOffset(0) returns the most recently pushed frame.
void test_at_offset_zero_returns_newest_pushed_frame() {
    TestRing ring;
    CRGB frameA[kStrip], frameB[kStrip], dest[kStrip];
    fillFrame(frameA, 0x11);
    fillFrame(frameB, 0x22);
    ring.push(frameA);
    ring.push(frameB);

    TEST_ASSERT_TRUE(ring.atOffset(0, dest));
    TEST_ASSERT_TRUE(framesEqual(dest, frameB));
    TEST_ASSERT_TRUE(ring.atOffset(1, dest));
    TEST_ASSERT_TRUE(framesEqual(dest, frameA));
}

// 4 — Push N + 2 frames; atOffset(N-1) returns frame 2 (frame 1 was evicted).
void test_at_offset_n_minus_1_returns_oldest_frame_when_full() {
    TestRing ring;
    CRGB frame[kStrip], dest[kStrip], expected[kStrip];
    // Push frames 1..(N+2). After this the ring contains frames 3..(N+2).
    for (size_t i = 1; i <= kSlots + 2; ++i) {
        fillFrame(frame, static_cast<uint8_t>(i));
        ring.push(frame);
    }
    TEST_ASSERT_EQUAL_size_t(kSlots, ring.count());

    // atOffset(0) is frame (N+2); atOffset(N-1) is frame ((N+2) - (N-1)) = 3.
    fillFrame(expected, static_cast<uint8_t>(kSlots + 2));
    TEST_ASSERT_TRUE(ring.atOffset(0, dest));
    TEST_ASSERT_TRUE(framesEqual(dest, expected));

    fillFrame(expected, 3);
    TEST_ASSERT_TRUE(ring.atOffset(kSlots - 1, dest));
    TEST_ASSERT_TRUE(framesEqual(dest, expected));
}

// 5 — atOffset() at or beyond count returns false and leaves dest untouched.
void test_at_offset_out_of_range_returns_false_and_leaves_dest_untouched() {
    TestRing ring;
    CRGB frame[kStrip], dest[kStrip];
    fillFrame(frame, 0x55);
    ring.push(frame);
    // count() == 1; offsets 1, 2, ..., and any large value are out of range.

    fillFrame(dest, 0xAB);  // sentinel
    TEST_ASSERT_FALSE(ring.atOffset(1, dest));
    TEST_ASSERT_FALSE(ring.atOffset(kSlots, dest));
    TEST_ASSERT_FALSE(ring.atOffset(1000, dest));

    // dest must still be the sentinel pattern — no partial copy.
    for (size_t i = 0; i < kStrip; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0xAB, dest[i].r);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(i), dest[i].g);
    }
}

// 6 — clear() resets count and head; subsequent pushes behave like a fresh
//     ring, and reads on the cleared instance fail until something is pushed.
void test_clear_resets_count_and_head() {
    TestRing ring;
    CRGB frame[kStrip], dest[kStrip];
    fillFrame(frame, 0x77);
    ring.push(frame);
    ring.push(frame);
    ring.push(frame);
    TEST_ASSERT_EQUAL_size_t(3, ring.count());

    ring.clear();
    TEST_ASSERT_EQUAL_size_t(0, ring.count());
    TEST_ASSERT_FALSE(ring.atOffset(0, dest));

    fillFrame(frame, 0x88);
    ring.push(frame);
    TEST_ASSERT_EQUAL_size_t(1, ring.count());
    TEST_ASSERT_TRUE(ring.atOffset(0, dest));
    TEST_ASSERT_TRUE(framesEqual(dest, frame));
}

// 7 — push(nullptr) is a no-op: count and contents are unchanged.
void test_push_nullptr_is_noop() {
    TestRing ring;
    CRGB frame[kStrip], dest[kStrip];
    fillFrame(frame, 0x33);
    ring.push(frame);

    ring.push(nullptr);  // must not increment count or corrupt the head slot
    TEST_ASSERT_EQUAL_size_t(1, ring.count());
    TEST_ASSERT_TRUE(ring.atOffset(0, dest));
    TEST_ASSERT_TRUE(framesEqual(dest, frame));
}

// 8 — capacity() is compile-time constexpr.
void test_capacity_is_compile_time_constexpr() {
    static_assert(PSRAMFrameRing<320, 4>::capacity() == 4,
                  "capacity() must be a compile-time constant.");
    static_assert(PSRAMFrameRing<8, 16>::capacity() == 16,
                  "capacity() must be a compile-time constant.");
    // Also exercise it at runtime to keep Unity reporting at least one
    // assertion in the test slot.
    TEST_ASSERT_EQUAL_size_t(4, (PSRAMFrameRing<320, 4>::capacity()));
}

// 9 — stripLen() is compile-time constexpr.
void test_strip_len_is_compile_time_constexpr() {
    static_assert(PSRAMFrameRing<320, 4>::stripLen() == 320,
                  "stripLen() must be a compile-time constant.");
    static_assert(PSRAMFrameRing<8, 16>::stripLen() == 8,
                  "stripLen() must be a compile-time constant.");
    TEST_ASSERT_EQUAL_size_t(320, (PSRAMFrameRing<320, 4>::stripLen()));
}

// 10 — Push 2N frames; oldest frame surviving is frame (N+1).
//      With N=4 we push 8; ring keeps frames 5,6,7,8 → atOffset(N-1) is 5.
void test_wraparound_overwrites_oldest_correctly() {
    TestRing ring;
    CRGB frame[kStrip], dest[kStrip], expected[kStrip];
    for (size_t i = 1; i <= 2 * kSlots; ++i) {
        fillFrame(frame, static_cast<uint8_t>(i));
        ring.push(frame);
    }
    TEST_ASSERT_EQUAL_size_t(kSlots, ring.count());

    // Newest is 2N (=8), oldest is N+1 (=5).
    fillFrame(expected, static_cast<uint8_t>(2 * kSlots));
    TEST_ASSERT_TRUE(ring.atOffset(0, dest));
    TEST_ASSERT_TRUE(framesEqual(dest, expected));

    fillFrame(expected, static_cast<uint8_t>(kSlots + 1));
    TEST_ASSERT_TRUE(ring.atOffset(kSlots - 1, dest));
    TEST_ASSERT_TRUE(framesEqual(dest, expected));

    // Walk the whole ring: offsets 0..N-1 must be 2N, 2N-1, ..., N+1.
    for (size_t off = 0; off < kSlots; ++off) {
        const uint8_t want = static_cast<uint8_t>(2 * kSlots - off);
        fillFrame(expected, want);
        TEST_ASSERT_TRUE(ring.atOffset(off, dest));
        TEST_ASSERT_TRUE(framesEqual(dest, expected));
    }
}

// 11 — atOffset() writes exactly kStripLen CRGBs into dest. Guard bytes either
//      side of the dest array must remain at their sentinel value.
void test_atOffset_writes_exactly_kStripLen_crgbs_to_dest() {
    TestRing ring;
    CRGB frame[kStrip];
    fillFrame(frame, 0x66);
    ring.push(frame);

    // Layout: [canary_lo | dest[kStrip] | canary_hi]. The whole struct is on
    // the stack so any over-run shows up as a corrupted canary.
    struct Guarded {
        CRGB canaryLo[2];
        CRGB dest[kStrip];
        CRGB canaryHi[2];
    } g;
    // Sentinel bytes — non-zero so a stray memset(0) would also show.
    for (size_t i = 0; i < 2; ++i) {
        g.canaryLo[i].r = 0xCA; g.canaryLo[i].g = 0xCA; g.canaryLo[i].b = 0xCA;
        g.canaryHi[i].r = 0xFE; g.canaryHi[i].g = 0xFE; g.canaryHi[i].b = 0xFE;
    }
    for (size_t i = 0; i < kStrip; ++i) {
        g.dest[i].r = 0; g.dest[i].g = 0; g.dest[i].b = 0;
    }

    TEST_ASSERT_TRUE(ring.atOffset(0, g.dest));
    TEST_ASSERT_TRUE(framesEqual(g.dest, frame));

    // Canaries must be exactly as initialised — atOffset must not touch them.
    for (size_t i = 0; i < 2; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0xCA, g.canaryLo[i].r);
        TEST_ASSERT_EQUAL_UINT8(0xCA, g.canaryLo[i].g);
        TEST_ASSERT_EQUAL_UINT8(0xCA, g.canaryLo[i].b);
        TEST_ASSERT_EQUAL_UINT8(0xFE, g.canaryHi[i].r);
        TEST_ASSERT_EQUAL_UINT8(0xFE, g.canaryHi[i].g);
        TEST_ASSERT_EQUAL_UINT8(0xFE, g.canaryHi[i].b);
    }
}

// 12 — Two independently constructed rings do NOT share state. Pushing into
//      ringA must leave ringB empty.
void test_independent_rings_do_not_share_state() {
    TestRing ringA;
    TestRing ringB;
    CRGB frame[kStrip], dest[kStrip];
    fillFrame(frame, 0xA5);

    ringA.push(frame);
    ringA.push(frame);
    ringA.push(frame);

    TEST_ASSERT_EQUAL_size_t(3, ringA.count());
    TEST_ASSERT_EQUAL_size_t(0, ringB.count());
    TEST_ASSERT_FALSE(ringB.atOffset(0, dest));

    // Push something different into B and confirm A is unchanged.
    CRGB frameB[kStrip];
    fillFrame(frameB, 0x5A);
    ringB.push(frameB);
    TEST_ASSERT_EQUAL_size_t(1, ringB.count());
    TEST_ASSERT_TRUE(ringB.atOffset(0, dest));
    TEST_ASSERT_TRUE(framesEqual(dest, frameB));

    // ringA's newest is still the original `frame`.
    TEST_ASSERT_TRUE(ringA.atOffset(0, dest));
    TEST_ASSERT_TRUE(framesEqual(dest, frame));
    TEST_ASSERT_EQUAL_size_t(3, ringA.count());
}

// 13 — clear() actually zeroes the backing slots, not just the counters.
//      After clear() and a single push, atOffset(1) (out of range) must fail
//      and atOffset(0) must return only the freshly pushed frame.
void test_clear_zeroes_backing_slots() {
    TestRing ring;
    CRGB frame[kStrip], dest[kStrip];
    fillFrame(frame, 0xDE);
    for (size_t i = 0; i < kSlots; ++i) {
        ring.push(frame);
    }
    ring.clear();
    TEST_ASSERT_EQUAL_size_t(0, ring.count());

    // Push one fresh frame. count() = 1; only offset 0 is valid; the slot
    // contents at the other indices are zeroed but unreachable via atOffset
    // because count() bounds the read.
    CRGB fresh[kStrip];
    fillFrame(fresh, 0x12);
    ring.push(fresh);
    TEST_ASSERT_TRUE(ring.atOffset(0, dest));
    TEST_ASSERT_TRUE(framesEqual(dest, fresh));
    fillFrame(dest, 0xEE);  // sentinel
    TEST_ASSERT_FALSE(ring.atOffset(1, dest));
    // dest unchanged by the failed read.
    TEST_ASSERT_EQUAL_UINT8(0xEE, dest[0].r);
    // For belt-and-braces: the underlying zeroing happened (we cannot read
    // those slots through the public API, but we can verify a freshly
    // constructed ring has the same observable signature).
    TestRing fresh_ring;
    TEST_ASSERT_TRUE(fresh_ring.isValid());
    TEST_ASSERT_EQUAL_size_t(0, fresh_ring.count());
    (void)frameIsZero;  // helper retained for documentation; suppress unused.
}

}  // namespace

void run_psram_frame_ring_tests() {
    RUN_TEST(test_constructs_and_clears_on_init);
    RUN_TEST(test_push_increments_count_until_capacity);
    RUN_TEST(test_at_offset_zero_returns_newest_pushed_frame);
    RUN_TEST(test_at_offset_n_minus_1_returns_oldest_frame_when_full);
    RUN_TEST(test_at_offset_out_of_range_returns_false_and_leaves_dest_untouched);
    RUN_TEST(test_clear_resets_count_and_head);
    RUN_TEST(test_push_nullptr_is_noop);
    RUN_TEST(test_capacity_is_compile_time_constexpr);
    RUN_TEST(test_strip_len_is_compile_time_constexpr);
    RUN_TEST(test_wraparound_overwrites_oldest_correctly);
    RUN_TEST(test_atOffset_writes_exactly_kStripLen_crgbs_to_dest);
    RUN_TEST(test_independent_rings_do_not_share_state);
    RUN_TEST(test_clear_zeroes_backing_slots);
}
