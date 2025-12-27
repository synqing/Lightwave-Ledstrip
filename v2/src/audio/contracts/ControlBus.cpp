#include "ControlBus.h"
#include <cstring>  // for memcpy

namespace lightwaveos::audio {

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

ControlBus::ControlBus() { Reset(); }

void ControlBus::Reset() {
    m_frame = ControlBusFrame{};
    m_rms_s = 0.0f;
    m_flux_s = 0.0f;
    for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        m_bands_s[i] = 0.0f;
        m_heavy_bands_s[i] = 0.0f;
        m_bands_despiked[i] = 0.0f;
    }
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        m_chroma_s[i] = 0.0f;
        m_heavy_chroma_s[i] = 0.0f;
        m_chroma_despiked[i] = 0.0f;
    }
    // Reset lookahead buffers
    m_lookahead_bands.init(CONTROLBUS_NUM_BANDS);
    m_lookahead_chroma.init(CONTROLBUS_NUM_CHROMA);
    // Waveform array is zero-initialized by ControlBusFrame{} constructor
}

void ControlBus::setSmoothing(float alphaFast, float alphaSlow) {
    if (alphaFast < 0.0f) alphaFast = 0.0f;
    if (alphaFast > 1.0f) alphaFast = 1.0f;
    if (alphaSlow < 0.0f) alphaSlow = 0.0f;
    if (alphaSlow > 1.0f) alphaSlow = 1.0f;
    m_alpha_fast = alphaFast;
    m_alpha_slow = alphaSlow;
}

void ControlBus::setAttackRelease(float bandAttack, float bandRelease,
                                   float heavyBandAttack, float heavyBandRelease) {
    m_band_attack = clamp01(bandAttack);
    m_band_release = clamp01(bandRelease);
    m_heavy_band_attack = clamp01(heavyBandAttack);
    m_heavy_band_release = clamp01(heavyBandRelease);
}

/**
 * @brief Detect and remove single-frame spikes using 2-frame lookahead.
 *
 * Implements Sensory Bridge's spike detection algorithm:
 * - Maintains a 3-frame ring buffer (oldest, middle, newest)
 * - When middle frame changes direction from both neighbors (spike), replace with average
 * - Output is delayed by 2 frames to allow lookahead
 *
 * @param buffer   Ring buffer for history tracking
 * @param input    Current frame's raw values (clamped 0..1)
 * @param output   Despiked output values (2 frames delayed)
 * @param num_bands Number of bands to process
 */
void ControlBus::detectAndRemoveSpikes(LookaheadBuffer& buffer,
                                        const float* input,
                                        float* output,
                                        size_t num_bands) {
    // Handle disabled state - passthrough with no delay
    if (!buffer.enabled) {
        memcpy(output, input, num_bands * sizeof(float));
        return;
    }

    // Ring buffer indices (Sensory Bridge naming: past_index, look_ahead_1, look_ahead_2)
    // We write to 'newest' and output from 'oldest' (2-frame delay)
    const size_t newest = buffer.current_frame;
    const size_t middle = (buffer.current_frame + LOOKAHEAD_FRAMES - 1) % LOOKAHEAD_FRAMES;
    const size_t oldest = (buffer.current_frame + LOOKAHEAD_FRAMES - 2) % LOOKAHEAD_FRAMES;

    // Store current frame into newest slot
    for (size_t i = 0; i < num_bands; ++i) {
        buffer.history[newest][i] = input[i];
    }

    // Track warmup period - need 3 frames before spike detection is valid
    if (buffer.frames_filled < LOOKAHEAD_FRAMES) {
        buffer.frames_filled++;
        // During warmup, output zeros to avoid transient artifacts
        for (size_t i = 0; i < num_bands; ++i) {
            output[i] = 0.0f;
        }
        // Advance ring buffer
        buffer.current_frame = (buffer.current_frame + 1) % LOOKAHEAD_FRAMES;
        return;
    }

    // Spike detection: check if middle frame is a spike relative to neighbors
    // A spike occurs when middle has opposite direction from both transitions
    for (size_t i = 0; i < num_bands; ++i) {
        const float oldest_val = buffer.history[oldest][i];
        const float middle_val = buffer.history[middle][i];
        const float newest_val = buffer.history[newest][i];

        // Direction detection (Sensory Bridge pattern)
        // look_ahead_1_rising: middle > oldest (rising into middle)
        // look_ahead_2_rising: newest > middle (rising out of middle)
        const bool rising_into_middle = middle_val > oldest_val;
        const bool rising_out_of_middle = newest_val > middle_val;

        // Spike detection: direction changes at middle frame
        // Peak:   rising_into AND falling_out  (middle is local maximum)
        // Trough: falling_into AND rising_out  (middle is local minimum)
        const bool is_direction_change = (rising_into_middle != rising_out_of_middle);

        if (is_direction_change) {
            // Calculate expected value (average of neighbors)
            const float expected = (oldest_val + newest_val) * 0.5f;

            // Only correct if deviation is significant
            // This prevents over-smoothing gradual changes
            const float deviation = (middle_val > expected) ?
                                    (middle_val - expected) : (expected - middle_val);

            // Threshold: 15% of expected value, with floor to handle near-zero values
            const float threshold_floor = 0.02f;  // Minimum threshold
            const float threshold = (expected * 0.15f > threshold_floor) ?
                                    expected * 0.15f : threshold_floor;

            if (deviation > threshold) {
                // Replace spike with average of neighbors
                buffer.history[middle][i] = expected;
            }
        }
    }

    // Output the oldest frame (2-frame delay) after any spike corrections propagate
    // Note: corrections are applied to middle, which becomes oldest in 1 more frame
    // So we output oldest which has already been through spike detection
    for (size_t i = 0; i < num_bands; ++i) {
        output[i] = buffer.history[oldest][i];
    }

    // Advance ring buffer index
    buffer.current_frame = (buffer.current_frame + 1) % LOOKAHEAD_FRAMES;
}

void ControlBus::UpdateFromHop(const AudioTime& now, const ControlBusRawInput& raw) {
    m_frame.t = now;
    m_frame.hop_seq++;

    m_frame.fast_rms = clamp01(raw.rms);
    m_rms_s = lerp(m_rms_s, m_frame.fast_rms, m_alpha_fast);
    m_frame.rms = m_rms_s;

    m_frame.fast_flux = clamp01(raw.flux);
    m_flux_s = lerp(m_flux_s, m_frame.fast_flux, m_alpha_slow);
    m_frame.flux = m_flux_s;

    // LGP_SMOOTH: Asymmetric attack/release for bands
    for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        float target = clamp01(raw.bands[i]);
        // Fast attack, slow release for normal bands
        float alpha = (target > m_bands_s[i]) ? m_band_attack : m_band_release;
        m_bands_s[i] = lerp(m_bands_s[i], target, alpha);
        m_frame.bands[i] = m_bands_s[i];

        // Extra-smoothed heavy bands for ambient effects
        float heavyAlpha = (target > m_heavy_bands_s[i]) ? m_heavy_band_attack : m_heavy_band_release;
        m_heavy_bands_s[i] = lerp(m_heavy_bands_s[i], target, heavyAlpha);
        m_frame.heavy_bands[i] = m_heavy_bands_s[i];
    }

    // LGP_SMOOTH: Asymmetric attack/release for chroma
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        float target = clamp01(raw.chroma[i]);
        float alpha = (target > m_chroma_s[i]) ? m_band_attack : m_band_release;
        m_chroma_s[i] = lerp(m_chroma_s[i], target, alpha);
        m_frame.chroma[i] = m_chroma_s[i];

        float heavyAlpha = (target > m_heavy_chroma_s[i]) ? m_heavy_band_attack : m_heavy_band_release;
        m_heavy_chroma_s[i] = lerp(m_heavy_chroma_s[i], target, heavyAlpha);
        m_frame.heavy_chroma[i] = m_heavy_chroma_s[i];
    }

    for (uint8_t i = 0; i < CONTROLBUS_WAVEFORM_N; ++i) {
        m_frame.waveform[i] = raw.waveform[i];
    }
}

} // namespace lightwaveos::audio
