/**
 * @file PSRAMScalarRing.h
 * @brief Generic small-history ring buffer template (INF-12 substrate).
 *
 * Phase 2 Move 2.1 substrate per Topology_Reconciliation §6. A header-only,
 * fixed-capacity ring buffer with O(1) push and O(1) random-access read by
 * offset-from-head. Substrate for future consumers:
 *
 *   - LIN-06 RadialTimeScope     — 160-sample position ring
 *   - LIN-07 PrismHueDrift       — 60-sample hue ring (~240 B at float)
 *   - LIN-10 fx_dots motion-blur — 12-slot prev-position cache (~48 B)
 *   - AUD-16 OnsetHistory        — 160-sample event ring (~640 B)
 *
 * The `PSRAM` in the filename refers to the *intended placement domain* for
 * larger consumers (history rings can live in external PSRAM on ESP32-S3 to
 * keep internal SRAM free for the render path); the template itself is just a
 * fixed-size array, so smaller consumers can place the ring in DRAM or
 * directly inside an effect's state struct.
 *
 * ── Constraints ──────────────────────────────────────────────────────────
 *
 *  - NO heap allocation. Storage is a `T buffer_[N]` by-value member; the
 *    consumer chooses where the ring lives (stack frame, static, member).
 *  - O(1) push, O(1) random access. No loop, no modulo at read; read is
 *    `(head_ - 1 - offset + N) mod N` collapsed into a single conditional.
 *  - Cheap. Two integer ops per push, three per atOffset. Safe under the
 *    2.0 ms render ceiling for any reasonable N.
 *  - Portable. Pure C++ + <cstddef>; no platform intrinsics; same code path
 *    on native unit-test builds and on ESP32-S3 firmware builds.
 *
 * ── Semantics ────────────────────────────────────────────────────────────
 *
 *  - `push(v)` writes `v` at `head_`, advances `head_` (wrapping), and caps
 *    `count_` at N. The oldest value is silently evicted once full.
 *  - `atOffset(0)` is the value most recently pushed; `atOffset(N-1)` is the
 *    oldest still in the ring. `atOffset(k)` for any `k >= count_` returns
 *    a default-constructed `T{}` (so partial-fill reads do not return
 *    indeterminate stack memory).
 *  - `clear()` zeroes the buffer and resets head/count, so the ring behaves
 *    identically to a freshly constructed instance.
 *
 * ── British English ──────────────────────────────────────────────────────
 * Spelling in comments is British (behaviour, capacity, initialise).
 */

#pragma once

#include <cstddef>

namespace lightwaveos {
namespace effects {
namespace persistence {

/**
 * @brief Fixed-capacity ring buffer with offset-from-head random access.
 *
 * @tparam T  Stored value type (must be default-constructible and trivially
 *            assignable; scalars and small POD structs are the typical use).
 * @tparam N  Compile-time capacity (must be > 0; enforced by static_assert).
 */
template <typename T, size_t N>
class ScalarRing {
    static_assert(N > 0, "ScalarRing capacity must be greater than zero.");

public:
    /// Push the newest value; oldest evicted once full.
    void push(const T& value) {
        buffer_[head_] = value;
        head_ = (head_ + 1) % N;
        if (count_ < N) {
            ++count_;
        }
    }

    /**
     * Read the value at `offsetFromHead` slots back from the most recent
     * push. `offsetFromHead == 0` is the just-pushed value; `N - 1` is the
     * oldest. Returns `T{}` when the offset is at or beyond `count_` (the
     * partial-fill case before the ring has seen N pushes).
     */
    T atOffset(size_t offsetFromHead) const {
        if (offsetFromHead >= count_) {
            return T{};
        }
        // head_ points at the *next* write slot, so the most recent value
        // sits at (head_ - 1) mod N. Subtract a further `offsetFromHead`,
        // adding N before the modulus to keep the index non-negative.
        const size_t idx = (head_ + N - 1 - offsetFromHead) % N;
        return buffer_[idx];
    }

    /// Compile-time capacity.
    static constexpr size_t capacity() { return N; }

    /// Number of pushed values seen so far, capped at N.
    size_t count() const { return count_; }

    /// Reset to empty state (zero buffer + head + count).
    void clear() {
        for (size_t i = 0; i < N; ++i) {
            buffer_[i] = T{};
        }
        head_  = 0;
        count_ = 0;
    }

private:
    T      buffer_[N] = {};
    size_t head_      = 0;  // points at the next write slot
    size_t count_     = 0;  // number of valid entries, capped at N
};

}  // namespace persistence
}  // namespace effects
}  // namespace lightwaveos
