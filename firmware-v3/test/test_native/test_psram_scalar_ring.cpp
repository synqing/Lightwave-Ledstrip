// Phase 2 Move 2.1 — PSRAMScalarRing substrate test (INF-12).
//
// Per Topology_Reconciliation §6: a generic small-history ring buffer
// template that future consumers depend on, including:
//   - LIN-06 RadialTimeScope (160-sample position ring)
//   - LIN-07 PrismHueDrift (60-sample hue ring, 240 B)
//   - LIN-10 fx_dots motion-blur cache (12-slot prev-position cache, 48 B)
//   - AUD-16 OnsetHistory (160-sample event ring, 640 B)
//
// Contract:
//   - NO heap allocation (template instantiation produces fixed-size storage)
//   - O(1) push, O(1) atOffset
//   - atOffset(0) returns the just-pushed value; atOffset(N-1) the oldest
//   - atOffset() with offset >= count returns T{} (default-constructed)
//   - clear() resets the buffer to empty
//   - Header-only, native + ESP32-S3 portable
//
// British spelling in comments where applicable; this is a maths primitive,
// no UI or log strings to localise.

#include <unity.h>
#include <cstddef>
#include <cstdint>

#include "effects/persistence/PSRAMScalarRing.h"

using lightwaveos::effects::persistence::ScalarRing;

namespace {

// A small POD struct used to verify template-genericity beyond scalars.
struct Point2 {
    float x;
    float y;

    bool operator==(const Point2& other) const {
        return x == other.x && y == other.y;
    }
};

// ─── Default state ─────────────────────────────────────────────────────────

// 1 — A freshly constructed ring is empty and reads back T{} for any offset.
void test_scalarring_default_state_is_empty() {
    ScalarRing<int, 4> ring;
    TEST_ASSERT_EQUAL_size_t(0, ring.count());
    TEST_ASSERT_EQUAL_size_t(4, ScalarRing<int, 4>::capacity());
    TEST_ASSERT_EQUAL_INT(0, ring.atOffset(0));
    TEST_ASSERT_EQUAL_INT(0, ring.atOffset(1));
    TEST_ASSERT_EQUAL_INT(0, ring.atOffset(3));
    // Out-of-bounds offsets also return T{}.
    TEST_ASSERT_EQUAL_INT(0, ring.atOffset(99));
}

// ─── Single push ───────────────────────────────────────────────────────────

// 2 — Pushing one value: count is 1, atOffset(0) is the just-pushed value,
//     and any other offset still reads back T{}.
void test_scalarring_single_push() {
    ScalarRing<int, 4> ring;
    ring.push(42);
    TEST_ASSERT_EQUAL_size_t(1, ring.count());
    TEST_ASSERT_EQUAL_INT(42, ring.atOffset(0));
    TEST_ASSERT_EQUAL_INT(0,  ring.atOffset(1));
    TEST_ASSERT_EQUAL_INT(0,  ring.atOffset(3));
}

// ─── Fill without wrap ─────────────────────────────────────────────────────

// 3 — Filling exactly N values: count saturates at N, ordering from newest
//     (offset 0) to oldest (offset N-1) is correct.
void test_scalarring_fill_to_capacity_orders_correctly() {
    ScalarRing<int, 4> ring;
    ring.push(1);
    ring.push(2);
    ring.push(3);
    ring.push(4);
    TEST_ASSERT_EQUAL_size_t(4, ring.count());
    TEST_ASSERT_EQUAL_INT(4, ring.atOffset(0));  // newest
    TEST_ASSERT_EQUAL_INT(3, ring.atOffset(1));
    TEST_ASSERT_EQUAL_INT(2, ring.atOffset(2));
    TEST_ASSERT_EQUAL_INT(1, ring.atOffset(3));  // oldest
}

// ─── Wrap behaviour ────────────────────────────────────────────────────────

// 4 — Pushing N+1 values: count caps at N, the very first value is evicted,
//     atOffset(0) is the most recent push, atOffset(N-1) is the second push.
void test_scalarring_wrap_evicts_oldest() {
    ScalarRing<int, 4> ring;
    ring.push(1);
    ring.push(2);
    ring.push(3);
    ring.push(4);
    ring.push(5);  // evicts 1
    TEST_ASSERT_EQUAL_size_t(4, ring.count());
    TEST_ASSERT_EQUAL_INT(5, ring.atOffset(0));
    TEST_ASSERT_EQUAL_INT(4, ring.atOffset(1));
    TEST_ASSERT_EQUAL_INT(3, ring.atOffset(2));
    TEST_ASSERT_EQUAL_INT(2, ring.atOffset(3));
}

// 5 — Wrapping multiple times stays consistent (push 2N+1 values, count == N).
void test_scalarring_wrap_multiple_times() {
    ScalarRing<int, 4> ring;
    for (int i = 1; i <= 9; ++i) {  // pushes 1..9, last 4 retained
        ring.push(i);
    }
    TEST_ASSERT_EQUAL_size_t(4, ring.count());
    TEST_ASSERT_EQUAL_INT(9, ring.atOffset(0));
    TEST_ASSERT_EQUAL_INT(8, ring.atOffset(1));
    TEST_ASSERT_EQUAL_INT(7, ring.atOffset(2));
    TEST_ASSERT_EQUAL_INT(6, ring.atOffset(3));
}

// ─── Out-of-bounds offset ──────────────────────────────────────────────────

// 6 — Reading at an offset >= count returns T{}, even after partial fill.
void test_scalarring_offset_beyond_count_returns_default() {
    ScalarRing<int, 4> ring;
    ring.push(10);
    ring.push(20);
    TEST_ASSERT_EQUAL_INT(20, ring.atOffset(0));
    TEST_ASSERT_EQUAL_INT(10, ring.atOffset(1));
    // count() == 2; offsets 2, 3, and beyond are out of bounds.
    TEST_ASSERT_EQUAL_INT(0, ring.atOffset(2));
    TEST_ASSERT_EQUAL_INT(0, ring.atOffset(3));
    TEST_ASSERT_EQUAL_INT(0, ring.atOffset(100));
}

// ─── clear() ───────────────────────────────────────────────────────────────

// 7 — clear() resets count to zero, all reads return T{} again, and pushes
//     after a clear behave as if from a fresh ring.
void test_scalarring_clear_resets_state() {
    ScalarRing<int, 4> ring;
    ring.push(7);
    ring.push(8);
    ring.push(9);
    ring.clear();
    TEST_ASSERT_EQUAL_size_t(0, ring.count());
    TEST_ASSERT_EQUAL_INT(0, ring.atOffset(0));
    TEST_ASSERT_EQUAL_INT(0, ring.atOffset(1));

    ring.push(100);
    TEST_ASSERT_EQUAL_size_t(1, ring.count());
    TEST_ASSERT_EQUAL_INT(100, ring.atOffset(0));
    TEST_ASSERT_EQUAL_INT(0,   ring.atOffset(1));
}

// ─── Template genericity ───────────────────────────────────────────────────

// 8 — float specialisation: same semantics, exact-equal float comparisons
//     valid because no arithmetic touches the values.
void test_scalarring_float_specialisation() {
    ScalarRing<float, 3> ring;
    ring.push(1.5f);
    ring.push(2.5f);
    ring.push(3.5f);
    ring.push(4.5f);  // evicts 1.5
    TEST_ASSERT_EQUAL_size_t(3, ring.count());
    TEST_ASSERT_EQUAL_FLOAT(4.5f, ring.atOffset(0));
    TEST_ASSERT_EQUAL_FLOAT(3.5f, ring.atOffset(1));
    TEST_ASSERT_EQUAL_FLOAT(2.5f, ring.atOffset(2));
}

// 9 — POD struct specialisation: arbitrary trivially-copyable T works,
//     including reading back the default-constructed value via T{}.
void test_scalarring_pod_struct_specialisation() {
    ScalarRing<Point2, 3> ring;
    const Point2 a{1.0f, 2.0f};
    const Point2 b{3.0f, 4.0f};
    ring.push(a);
    ring.push(b);

    TEST_ASSERT_EQUAL_size_t(2, ring.count());
    Point2 newest = ring.atOffset(0);
    Point2 prev   = ring.atOffset(1);
    TEST_ASSERT_TRUE(newest == b);
    TEST_ASSERT_TRUE(prev   == a);

    // Out-of-bounds returns Point2{} (zero-initialised).
    Point2 oob = ring.atOffset(2);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, oob.x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, oob.y);
}

// 10 — Capacity-1 ring: edge case where every push immediately evicts the
//      previous value. atOffset(0) is always the most-recent push.
void test_scalarring_capacity_one_edge_case() {
    ScalarRing<int, 1> ring;
    TEST_ASSERT_EQUAL_size_t(0, ring.count());
    ring.push(11);
    TEST_ASSERT_EQUAL_size_t(1, ring.count());
    TEST_ASSERT_EQUAL_INT(11, ring.atOffset(0));
    ring.push(22);
    TEST_ASSERT_EQUAL_size_t(1, ring.count());
    TEST_ASSERT_EQUAL_INT(22, ring.atOffset(0));
    TEST_ASSERT_EQUAL_INT(0,  ring.atOffset(1));  // out of bounds
}

}  // namespace

void run_psram_scalar_ring_tests() {
    RUN_TEST(test_scalarring_default_state_is_empty);
    RUN_TEST(test_scalarring_single_push);
    RUN_TEST(test_scalarring_fill_to_capacity_orders_correctly);
    RUN_TEST(test_scalarring_wrap_evicts_oldest);
    RUN_TEST(test_scalarring_wrap_multiple_times);
    RUN_TEST(test_scalarring_offset_beyond_count_returns_default);
    RUN_TEST(test_scalarring_clear_resets_state);
    RUN_TEST(test_scalarring_float_specialisation);
    RUN_TEST(test_scalarring_pod_struct_specialisation);
    RUN_TEST(test_scalarring_capacity_one_edge_case);
}
