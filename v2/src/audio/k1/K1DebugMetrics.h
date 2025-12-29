/**
 * @file K1DebugMetrics.h
 * @brief K1 Debug Sample Structure for Cross-Core Capture
 *
 * Compact 64-byte debug sample designed for lock-free SPSC queue transfer
 * from Core 0 (AudioActor) to Core 1 (RendererActor/WebSocket).
 *
 * Uses fixed-point integers to pack more data while staying cache-aligned.
 */

#pragma once

#include "../../config/features.h"

#if FEATURE_K1_DEBUG

#include <stdint.h>
#include "K1Config.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

// ============================================================================
// Debug Candidate (6 bytes each)
// ============================================================================
struct K1DebugCandidate {
    uint16_t bpm_x10;       // BPM * 10 (600-1800 for 60-180 BPM)
    uint16_t magnitude_x1k; // Magnitude * 1000 (0-1000)
    int16_t  phase_x100;    // Phase * 100 radians (-314 to +314)
};

// ============================================================================
// K1 Debug Sample (64 bytes, cache-aligned)
// ============================================================================
struct __attribute__((aligned(64))) K1DebugSample {
    // ========== Timing & Flags (8 bytes) ==========
    uint32_t timestamp_ms;      // Capture timestamp
    uint16_t update_count;      // Pipeline update counter
    uint8_t  tracker_state;     // 0=LOST, 1=COAST, 2=LOCKED
    uint8_t  flags;             // Bit flags:
                                //   bit 0: beat_tick
                                //   bit 1: tempo_changed
                                //   bit 2: lock_changed
                                //   bit 3-7: reserved

    // ========== Stage 3: Tactus (12 bytes) ==========
    uint16_t locked_bpm_x10;    // Locked BPM * 10
    uint16_t challenger_bpm_x10;// Challenger BPM * 10 (0 if none)
    uint8_t  challenger_frames; // Frames challenger has sustained
    uint8_t  winning_bin;       // Current winning resonator bin
    uint16_t density_conf_x1k;  // density_conf * 1000
    uint16_t family_score_x1k;  // family_score * 1000
    uint16_t confidence_x1k;    // Overall confidence * 1000

    // ========== Stage 4: PLL (8 bytes) ==========
    uint16_t phase01_x1k;       // phase01 * 1000 (0-999)
    int16_t  phase_error_x100;  // Phase error * 100 radians
    int16_t  freq_error_x100;   // Frequency error * 100 rad/s
    uint16_t _pll_reserved;     // Alignment padding

    // ========== Top-3 Candidates (18 bytes) ==========
    K1DebugCandidate top3[3];

    // ========== Novelty (4 bytes) ==========
    int16_t  novelty_z_x100;    // Current novelty z-score * 100
    uint16_t novelty_rms_x1k;   // Novelty RMS * 1000 (for noise detection)

    // ========== Reserved (14 bytes) ==========
    uint8_t  _reserved[14];     // Future expansion, keeps struct at 64 bytes
};

// Static assertion to verify size
static_assert(sizeof(K1DebugSample) == 64, "K1DebugSample must be 64 bytes");

// ============================================================================
// Conversion Helpers
// ============================================================================
namespace debug_conv {

inline uint16_t bpm_to_x10(float bpm) {
    if (bpm < 0.0f) return 0;
    if (bpm > 6500.0f) return 65000;  // Max for uint16
    return static_cast<uint16_t>(bpm * 10.0f + 0.5f);
}

inline float x10_to_bpm(uint16_t x10) {
    return static_cast<float>(x10) / 10.0f;
}

inline uint16_t float01_to_x1k(float f) {
    if (f < 0.0f) return 0;
    if (f > 1.0f) return 1000;
    return static_cast<uint16_t>(f * 1000.0f + 0.5f);
}

inline float x1k_to_float01(uint16_t x1k) {
    return static_cast<float>(x1k) / 1000.0f;
}

inline int16_t rad_to_x100(float rad) {
    int32_t val = static_cast<int32_t>(rad * 100.0f);
    if (val < -32768) return -32768;
    if (val > 32767) return 32767;
    return static_cast<int16_t>(val);
}

inline float x100_to_rad(int16_t x100) {
    return static_cast<float>(x100) / 100.0f;
}

inline int16_t zscore_to_x100(float z) {
    int32_t val = static_cast<int32_t>(z * 100.0f);
    if (val < -32768) return -32768;
    if (val > 32767) return 32767;
    return static_cast<int16_t>(val);
}

inline float x100_to_zscore(int16_t x100) {
    return static_cast<float>(x100) / 100.0f;
}

} // namespace debug_conv

// ============================================================================
// Flag Bit Definitions
// ============================================================================
namespace debug_flags {
    constexpr uint8_t BEAT_TICK     = 0x01;
    constexpr uint8_t TEMPO_CHANGED = 0x02;
    constexpr uint8_t LOCK_CHANGED  = 0x04;
}

// ============================================================================
// Tracker State Values
// ============================================================================
namespace debug_state {
    constexpr uint8_t LOST   = 0;
    constexpr uint8_t COAST  = 1;
    constexpr uint8_t LOCKED = 2;
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_K1_DEBUG
