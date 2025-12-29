/**
 * @file K1Types.h
 * @brief K1-Lightwave Beat Tracker v2 Data Types
 *
 * Ported from Tab5.DSP for ESP32-S3 Lightwave v2 integration.
 */

#pragma once

#include <stdint.h>
#include <cmath>
#include "K1Config.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

// ============================================================================
// Fast Float Validation
// ============================================================================
#define K1_IS_VALID_FLOAT(x) (((*(uint32_t*)&(x)) & 0x7F800000u) != 0x7F800000u)

// ============================================================================
// Math Helpers
// ============================================================================
inline float k1_clampf(float x, float a, float b) {
    if (x < a) return a;
    if (x > b) return b;
    return x;
}

inline float k1_clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

inline float k1_wrap01(float x) {
    while (x >= 1.0f) x -= 1.0f;
    while (x <  0.0f) x += 1.0f;
    return x;
}

inline float k1_absf(float x) { return (x < 0.0f) ? -x : x; }

// ============================================================================
// Tracker State (internal use)
// ============================================================================
enum class K1TrackerState {
    LOST,    // No reliable beat detected
    COAST,   // Coasting on last known tempo
    LOCKED   // Confident beat lock
};

// ============================================================================
// Ring Buffer Template
// ============================================================================
template<typename T, int N>
struct K1RingBuffer {
    T     data[N];
    int   head    = 0;
    int   count   = 0;

    void push(T val) {
        data[head] = val;
        head = (head + 1) % N;
        if (count < N) count++;
    }

    T get(int back) const {
        if (back >= count) back = count - 1;
        if (back < 0) back = 0;
        int idx = (head - 1 - back + N) % N;
        return data[idx];
    }

    void clear() {
        head = 0;
        count = 0;
        for (int i = 0; i < N; i++) data[i] = T{};
    }

    bool full() const { return count >= N; }
    int  size() const { return count; }
};

// ============================================================================
// Stage 2 Output: Resonator Frame
// ============================================================================
struct K1ResonatorCandidate {
    float bpm           = 0.0f;
    float magnitude     = 0.0f;   // Normalized [0,1]
    float phase         = 0.0f;   // Radians [-π, +π]
    float raw_mag       = 0.0f;
};

struct K1ResonatorFrame {
    uint32_t t_ms       = 0;
    int   k             = 0;
    K1ResonatorCandidate candidates[ST2_TOPK];
    float spectrum[ST2_BPM_BINS];
};

// ============================================================================
// Stage 3 Output: Tactus Frame
// ============================================================================
struct K1TactusFrame {
    uint32_t t_ms       = 0;
    float bpm           = 0.0f;
    float confidence    = 0.0f;
    float density_conf  = 0.0f;
    float phase_hint    = 0.0f;
    bool  locked        = false;
    int   winning_bin   = -1;
    int   challenger_frames = 0;
    float family_score  = 0.0f;
};

// ============================================================================
// Stage 4 Output: Beat Clock
// ============================================================================
struct K1BeatClockState {
    uint32_t t_ms       = 0;
    float phase01       = 0.0f;   // 0 = beat, wraps at 1
    bool  beat_tick     = false;
    float bpm           = 0.0f;
    float confidence    = 0.0f;
    bool  locked        = false;
    float phase_error   = 0.0f;
    float freq_error    = 0.0f;
};

// ============================================================================
// Internal: Goertzel Resonator State
// ============================================================================
struct K1GoertzelBin {
    float target_hz     = 0.0f;
    int   block_size    = 0;
    float coeff         = 0.0f;
    float cosine        = 0.0f;
    float sine          = 0.0f;
    float window_step   = 0.0f;
    float magnitude_raw = 0.0f;
    float magnitude_smooth = 0.0f;
    float phase         = 0.0f;
};

// ============================================================================
// Novelty Input (received from AudioActor via ControlBusFrame)
// ============================================================================
struct K1NoveltyInput {
    uint32_t t_ms       = 0;
    float novelty_z     = 0.0f;   // Scaled from Lightwave flux [0,1] → z-score [-6,+6]
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos
