/**
 * @file PSRAMFrameRing.h
 * @brief Header-only PSRAM-resident framebuffer history ring (INF-03 substrate).
 *
 * Phase 5 Move 5.1 of the K1 visual-pipeline synergy-topology kill order
 * (INF-03). Companion to `PSRAMScalarRing` (INF-12) — where ScalarRing holds a
 * small history of scalar samples in DRAM, FrameRing holds a small history of
 * full LED framebuffers in PSRAM. Future consumers include any effect that
 * needs to read back N previous rendered frames (echo/delay-line persistence,
 * scrolling time-scopes, motion-trail composers).
 *
 * The storage is allocated ONCE in the constructor via
 * `heap_caps_malloc(MALLOC_CAP_SPIRAM)` on firmware, or plain `new[]` on
 * `NATIVE_BUILD`. Allocation may legitimately fail on a system without PSRAM
 * or when PSRAM is exhausted; consumers MUST guard with `isValid()` before
 * calling `push` / `atOffset`. A failed ring degrades to a no-op rather than
 * crashing the render path.
 *
 * ── Algorithm ────────────────────────────────────────────────────────────
 *
 *   push(frame): memcpy(slot[head_], frame, kStripLen·sizeof(CRGB));
 *                head_ = (head_ + 1) % kNumSlots;
 *                count_ = min(count_ + 1, kNumSlots);
 *
 *   atOffset(o, dest): bounds-check o < count_;
 *                      idx = (head_ + kNumSlots - 1 - o) % kNumSlots;
 *                      memcpy(dest, slot[idx], kStripLen·sizeof(CRGB));
 *
 * `atOffset(0)` returns the most recently pushed frame; `atOffset(N-1)`
 * returns the oldest frame still resident in the ring. `o >= count_` returns
 * false and leaves `dest` untouched.
 *
 * ── Constraints ──────────────────────────────────────────────────────────
 *
 *  - Allocation happens ONCE in the constructor. No heap touch in `push` or
 *    `atOffset` — both paths are pure memcpy + integer arithmetic.
 *  - Substrate-only — no ControlBus, no FastLED show, no global state.
 *  - PSRAM-resident on firmware, DRAM on native test. Same source, two
 *    allocators, identical behaviour.
 *  - nullptr-safe push: if the caller passes `nullptr` the ring is unchanged.
 *  - Per-slot row pointers (`slot_[]`) point into a single contiguous block,
 *    so wrap-around lookup is O(1) — no row-pointer indirection cost beyond
 *    the array index.
 *  - British English in comments (centre, colour, behaviour, initialise).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>  // std::nothrow

#include <FastLED.h>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos {
namespace effects {
namespace persistence {

/**
 * @brief Fixed-capacity ring of full-strip framebuffers held in PSRAM.
 *
 * @tparam kStripLen  LED count per stored frame (must be > 0).
 * @tparam kNumSlots  Number of historical frames retained (must be > 1; a
 *                    one-slot ring is just a single frame and would not
 *                    benefit from the wrap logic).
 */
template <size_t kStripLen, size_t kNumSlots>
class PSRAMFrameRing {
    static_assert(kStripLen > 0, "PSRAMFrameRing kStripLen must be > 0");
    static_assert(kNumSlots > 1, "PSRAMFrameRing kNumSlots must be > 1");

public:
    /// @brief Allocate the backing store. On firmware the block is requested
    /// from PSRAM (`MALLOC_CAP_SPIRAM`); on `NATIVE_BUILD` it is plain DRAM
    /// via `new[]`. If allocation fails, `isValid()` returns false and every
    /// subsequent `push` / `atOffset` call is a guarded no-op.
    PSRAMFrameRing()
        : storage_(nullptr), head_(0), count_(0) {
        const size_t totalBytes = kStripLen * kNumSlots * sizeof(CRGB);
#ifdef NATIVE_BUILD
        // Native unit-test build: plain DRAM allocator. We use new[] so the
        // CRGB default constructor zero-initialises the block — keeps the
        // ring in a defined state even before the first push.
        storage_ = new (std::nothrow) CRGB[kStripLen * kNumSlots];
        if (storage_ != nullptr) {
            // new[] already default-constructs CRGB to {0,0,0}; explicit
            // zero pass is redundant but cheap and matches the firmware
            // path where `heap_caps_malloc` returns uninitialised memory.
            std::memset(storage_, 0, totalBytes);
        }
#else
        // Firmware build: place the history in external PSRAM. Internal SRAM
        // remains free for the render path. Failure here is legitimate (no
        // PSRAM fitted, or PSRAM exhausted) — consumers must guard via
        // isValid().
        storage_ = static_cast<CRGB*>(
            heap_caps_malloc(totalBytes, MALLOC_CAP_SPIRAM));
        if (storage_ != nullptr) {
            std::memset(storage_, 0, totalBytes);
        }
#endif
    }

    /// @brief Free the backing store.
    ~PSRAMFrameRing() {
        if (storage_ == nullptr) {
            return;
        }
#ifdef NATIVE_BUILD
        delete[] storage_;
#else
        heap_caps_free(storage_);
#endif
        storage_ = nullptr;
    }

    // Non-copyable, non-movable: the backing store is a raw pointer to a
    // single allocation; copy/move semantics would require deep-copy or
    // careful ownership transfer that the substrate does not need.
    PSRAMFrameRing(const PSRAMFrameRing&)            = delete;
    PSRAMFrameRing& operator=(const PSRAMFrameRing&) = delete;
    PSRAMFrameRing(PSRAMFrameRing&&)                 = delete;
    PSRAMFrameRing& operator=(PSRAMFrameRing&&)      = delete;

    /// @brief True once the backing store is allocated; consumers must guard.
    bool isValid() const { return storage_ != nullptr; }

    /// @brief Compile-time slot count.
    static constexpr size_t capacity() { return kNumSlots; }

    /// @brief Compile-time per-frame strip length.
    static constexpr size_t stripLen() { return kStripLen; }

    /// @brief Number of valid frames pushed so far, capped at `kNumSlots`.
    size_t count() const { return count_; }

    /**
     * @brief Copy `kStripLen` CRGBs from `frame` into the ring's head slot,
     * advance head, and cap count at `kNumSlots`.
     *
     * `frame == nullptr` is a guarded no-op (the ring is unchanged). A ring
     * whose constructor failed (`!isValid()`) silently ignores all pushes.
     */
    void push(const CRGB* frame) {
        if (storage_ == nullptr || frame == nullptr) {
            return;
        }
        CRGB* slot = slotPtr(head_);
        std::memcpy(slot, frame, kStripLen * sizeof(CRGB));
        head_ = (head_ + 1) % kNumSlots;
        if (count_ < kNumSlots) {
            ++count_;
        }
    }

    /**
     * @brief Copy the frame at `offsetFromHead` slots back from the most
     * recent push into `dest[0 .. kStripLen)`.
     *
     * `offsetFromHead == 0` returns the most recently pushed frame;
     * `offsetFromHead == count_ - 1` returns the oldest frame still in the
     * ring. Returns `false` and leaves `dest` untouched when:
     *   - the ring failed to allocate (`!isValid()`),
     *   - `dest == nullptr`,
     *   - `offsetFromHead >= count_` (out of populated range).
     */
    bool atOffset(size_t offsetFromHead, CRGB* dest) const {
        if (storage_ == nullptr || dest == nullptr) {
            return false;
        }
        if (offsetFromHead >= count_) {
            return false;
        }
        // head_ points at the next write slot, so the most recent frame sits
        // at (head_ - 1) mod N. Subtract a further `offsetFromHead`, adding
        // kNumSlots before the modulus to keep the index non-negative.
        const size_t idx = (head_ + kNumSlots - 1 - offsetFromHead) % kNumSlots;
        std::memcpy(dest, slotPtr(idx), kStripLen * sizeof(CRGB));
        return true;
    }

    /// @brief Zero every slot and reset head/count to behave like a freshly
    /// constructed ring. Safe to call when `!isValid()` (no-op).
    void clear() {
        if (storage_ != nullptr) {
            std::memset(storage_, 0, kStripLen * kNumSlots * sizeof(CRGB));
        }
        head_  = 0;
        count_ = 0;
    }

private:
    /// @brief Address of slot `i` inside the contiguous backing store.
    CRGB* slotPtr(size_t i) const {
        return storage_ + (i * kStripLen);
    }

    CRGB*  storage_;  ///< Single contiguous block of `kStripLen·kNumSlots` CRGBs.
    size_t head_;     ///< Index of the next write slot.
    size_t count_;    ///< Valid frames pushed, capped at `kNumSlots`.
};

}  // namespace persistence
}  // namespace effects
}  // namespace lightwaveos
