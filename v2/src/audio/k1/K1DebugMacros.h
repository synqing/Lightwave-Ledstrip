/**
 * @file K1DebugMacros.h
 * @brief Zero-Overhead Debug Instrumentation Macros
 *
 * When FEATURE_K1_DEBUG is disabled, all macros expand to nothing.
 * When enabled, they capture pipeline state to a K1DebugSample for
 * cross-core transfer via the lock-free ring buffer.
 *
 * Usage in K1Pipeline::processNovelty():
 *
 *   bool K1Pipeline::processNovelty(...) {
 *       K1_DEBUG_DECL();
 *       K1_DEBUG_START(t_ms);
 *
 *       // ... existing pipeline code ...
 *
 *       K1_DEBUG_CAPTURE_RESONATORS(last_resonator_frame_);
 *       K1_DEBUG_CAPTURE_TACTUS(last_tactus_frame_);
 *       K1_DEBUG_CAPTURE_CLOCK(last_beat_clock_);
 *       K1_DEBUG_NOVELTY(novelty_z);
 *       K1_DEBUG_END(debug_ring_, update_count_);
 *
 *       return lock_changed;
 *   }
 */

#pragma once

#include "../../config/features.h"

#if FEATURE_K1_DEBUG

#include "K1DebugMetrics.h"
#include "K1Types.h"

// ============================================================================
// Debug-Enabled Macros
// ============================================================================

/**
 * Declare a local debug sample variable
 */
#define K1_DEBUG_DECL() \
    ::lightwaveos::audio::k1::K1DebugSample _k1_dbg = {}

/**
 * Start capturing - set timestamp
 */
#define K1_DEBUG_START(t_ms) \
    do { \
        _k1_dbg.timestamp_ms = (t_ms); \
        _k1_dbg.flags = 0; \
    } while(0)

/**
 * Capture top-3 resonator candidates
 */
#define K1_DEBUG_CAPTURE_RESONATORS(rf) \
    do { \
        using namespace ::lightwaveos::audio::k1::debug_conv; \
        const auto& _rf = (rf); \
        int k = (_rf.k > 3) ? 3 : _rf.k; \
        for (int i = 0; i < k; ++i) { \
            _k1_dbg.top3[i].bpm_x10 = bpm_to_x10(_rf.candidates[i].bpm); \
            _k1_dbg.top3[i].magnitude_x1k = float01_to_x1k(_rf.candidates[i].magnitude); \
            _k1_dbg.top3[i].phase_x100 = rad_to_x100(_rf.candidates[i].phase); \
        } \
    } while(0)

/**
 * Capture tactus resolver state
 */
#define K1_DEBUG_CAPTURE_TACTUS(tf) \
    do { \
        using namespace ::lightwaveos::audio::k1::debug_conv; \
        const auto& _tf = (tf); \
        _k1_dbg.locked_bpm_x10 = bpm_to_x10(_tf.bpm); \
        _k1_dbg.challenger_bpm_x10 = 0; /* TODO: expose challenger BPM */ \
        _k1_dbg.challenger_frames = static_cast<uint8_t>(_tf.challenger_frames); \
        _k1_dbg.winning_bin = static_cast<uint8_t>(_tf.winning_bin >= 0 ? _tf.winning_bin : 0); \
        _k1_dbg.density_conf_x1k = float01_to_x1k(_tf.density_conf); \
        _k1_dbg.family_score_x1k = float01_to_x1k(_tf.family_score); \
        _k1_dbg.confidence_x1k = float01_to_x1k(_tf.confidence); \
        _k1_dbg.tracker_state = _tf.locked ? \
            ::lightwaveos::audio::k1::debug_state::LOCKED : \
            ::lightwaveos::audio::k1::debug_state::COAST; \
    } while(0)

/**
 * Capture beat clock (PLL) state
 */
#define K1_DEBUG_CAPTURE_CLOCK(cs) \
    do { \
        using namespace ::lightwaveos::audio::k1::debug_conv; \
        using namespace ::lightwaveos::audio::k1::debug_flags; \
        const auto& _cs = (cs); \
        _k1_dbg.phase01_x1k = float01_to_x1k(_cs.phase01); \
        _k1_dbg.phase_error_x100 = rad_to_x100(_cs.phase_error); \
        _k1_dbg.freq_error_x100 = rad_to_x100(_cs.freq_error); \
        if (_cs.beat_tick) _k1_dbg.flags |= BEAT_TICK; \
    } while(0)

/**
 * Capture novelty z-score
 */
#define K1_DEBUG_NOVELTY(z) \
    do { \
        using namespace ::lightwaveos::audio::k1::debug_conv; \
        _k1_dbg.novelty_z_x100 = zscore_to_x100(z); \
    } while(0)

/**
 * Set lock changed flag
 */
#define K1_DEBUG_FLAG_LOCK_CHANGED() \
    do { \
        _k1_dbg.flags |= ::lightwaveos::audio::k1::debug_flags::LOCK_CHANGED; \
    } while(0)

/**
 * Set tempo changed flag
 */
#define K1_DEBUG_FLAG_TEMPO_CHANGED() \
    do { \
        _k1_dbg.flags |= ::lightwaveos::audio::k1::debug_flags::TEMPO_CHANGED; \
    } while(0)

/**
 * End capture - set update count and push to ring
 * @param ring Pointer to K1DebugRing
 * @param count Update counter value
 */
#define K1_DEBUG_END(ring, count) \
    do { \
        _k1_dbg.update_count = static_cast<uint16_t>(count); \
        if ((ring) != nullptr) { \
            (ring)->push(_k1_dbg); \
        } \
    } while(0)

#else // !FEATURE_K1_DEBUG

// ============================================================================
// Release-Mode: All Macros Expand to Nothing
// ============================================================================

#define K1_DEBUG_DECL()                         do {} while(0)
#define K1_DEBUG_START(t_ms)                    do {} while(0)
#define K1_DEBUG_CAPTURE_RESONATORS(rf)         do {} while(0)
#define K1_DEBUG_CAPTURE_TACTUS(tf)             do {} while(0)
#define K1_DEBUG_CAPTURE_CLOCK(cs)              do {} while(0)
#define K1_DEBUG_NOVELTY(z)                     do {} while(0)
#define K1_DEBUG_FLAG_LOCK_CHANGED()            do {} while(0)
#define K1_DEBUG_FLAG_TEMPO_CHANGED()           do {} while(0)
#define K1_DEBUG_END(ring, count)               do {} while(0)

#endif // FEATURE_K1_DEBUG
