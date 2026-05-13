/**
 * @file InterStripPhaseDelay.h
 * @brief Phase 3 Move 3.3 dual-strip delay-line substrate.
 *
 * Stores a short history for each physical K1 strip so future DUAL_CHANNEL
 * effects can sample strip A and strip B at different frame offsets without
 * allocating inside render(). This is infrastructure only; it does not alter
 * renderer output until an effect explicitly consumes it.
 */

#pragma once

#include <cstddef>

#include <FastLED.h>

#include "PSRAMFrameRing.h"

namespace lightwaveos {
namespace effects {
namespace persistence {

/**
 * @brief Paired frame rings for strip-local phase delay.
 *
 * @tparam kStripLen  LED count per physical strip.
 * @tparam kNumSlots  Historical frames retained per strip.
 */
template <size_t kStripLen, size_t kNumSlots>
class InterStripPhaseDelay {
public:
    /// Both backing rings must allocate successfully before consumers use the
    /// delay line. A failed allocation degrades every operation to a no-op.
    bool isValid() const {
        return stripA_.isValid() && stripB_.isValid();
    }

    static constexpr size_t stripLen() { return kStripLen; }
    static constexpr size_t capacity() { return kNumSlots; }

    /// Number of complete A/B frame pairs pushed so far.
    size_t count() const {
        const size_t a = stripA_.count();
        const size_t b = stripB_.count();
        return (a < b) ? a : b;
    }

    /// Store the current strip pair. Null input leaves both rings unchanged so
    /// pair alignment cannot be corrupted by a partial push.
    void pushPair(const CRGB* stripA, const CRGB* stripB) {
        if (!isValid() || stripA == nullptr || stripB == nullptr) {
            return;
        }
        stripA_.push(stripA);
        stripB_.push(stripB);
    }

    /// Copy a delayed strip pair into caller-owned output buffers.
    bool atOffset(size_t offsetFromHead, CRGB* outA, CRGB* outB) const {
        if (!isValid() || outA == nullptr || outB == nullptr) {
            return false;
        }
        if (offsetFromHead >= count()) {
            return false;
        }
        return stripA_.atOffset(offsetFromHead, outA)
            && stripB_.atOffset(offsetFromHead, outB);
    }

    /// Copy one delayed strip. stripIdx: 0 = A, 1 = B.
    bool stripAtOffset(uint8_t stripIdx, size_t offsetFromHead, CRGB* out) const {
        if (!isValid() || out == nullptr || offsetFromHead >= count()) {
            return false;
        }
        if (stripIdx == 0) {
            return stripA_.atOffset(offsetFromHead, out);
        }
        if (stripIdx == 1) {
            return stripB_.atOffset(offsetFromHead, out);
        }
        return false;
    }

    void clear() {
        stripA_.clear();
        stripB_.clear();
    }

private:
    PSRAMFrameRing<kStripLen, kNumSlots> stripA_;
    PSRAMFrameRing<kStripLen, kNumSlots> stripB_;
};

}  // namespace persistence
}  // namespace effects
}  // namespace lightwaveos
