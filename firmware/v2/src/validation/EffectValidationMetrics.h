/**
 * @file EffectValidationMetrics.h
 * @brief Core data structures for audio-reactive effect validation framework
 *
 * Provides a lock-free ring buffer and sample structures for capturing
 * effect state during audio-reactive rendering. Designed for single-producer
 * (render thread) and single-consumer (WebSocket thread) operation.
 *
 * Memory layout:
 * - EffectValidationSample: 128 bytes (cache-line aligned for ESP32)
 * - EffectValidationRing<128>: 16KB (default configuration)
 *
 * @copyright 2024 LightwaveOS Project
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "../config/effect_ids.h"

// ESP32 atomic operations
#ifdef ESP_PLATFORM
#include <esp_attr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#else
// Fallback for non-ESP32 builds (testing)
#define IRAM_ATTR
#endif

namespace lightwaveos {
namespace validation {

/**
 * @brief Single validation sample captured during effect rendering
 *
 * This structure captures a snapshot of effect state at a specific point
 * in time, including phase accumulators, speed scaling factors, and
 * audio-derived metrics. Used for debugging jog-dial behavior and
 * validating audio-reactive smoothness.
 *
 * Size: 128 bytes (padded for cache alignment and efficient DMA)
 *
 * Memory layout (offsets in bytes):
 *   0-3:   timestamp_us
 *   4-7:   hop_seq
 *   8:     effect_id
 *   9:     reversal_count
 *   10-11: frame_seq
 *   12-15: phase
 *   16-19: phase_delta
 *   20-23: speed_scale_raw
 *   24-27: speed_scale_smooth
 *   28-31: dominant_freq_bin
 *   32-35: energy_avg
 *   36-39: energy_delta
 *   40-43: scroll_phase
 *   48-127: reserved (padding to 128 bytes)
 */
struct EffectValidationSample {
    // Timing and sequencing (8 bytes)
    uint32_t timestamp_us;      ///< Microseconds since boot (esp_timer_get_time)
    uint32_t hop_seq;           ///< Audio hop sequence number for correlation

    // Effect identification (8 bytes with padding)
    EffectId effect_id;         ///< Current effect ID (stable namespaced)
    uint8_t reversal_count;     ///< Jog-dial detection: direction reversals this frame
    uint16_t frame_seq;         ///< Per-effect frame sequence counter

    // Phase accumulator state (16 bytes)
    float phase;                ///< Normalized phase accumulator (0.0 - 1.0)
    float phase_delta;          ///< Rate of change per frame (signed)
    float speed_scale_raw;      ///< Raw speed scale before slew limiting
    float speed_scale_smooth;   ///< Smoothed speed scale after slew limiting

    // Audio metrics (16 bytes)
    float dominant_freq_bin;    ///< Dominant frequency bin index (0.0 - 7.0)
    float energy_avg;           ///< Average energy across bands (0.0 - 1.0)
    float energy_delta;         ///< Change in energy from previous frame
    float scroll_phase;         ///< AudioBloom scroll phase (0.0 - 1.0)

    // Reserved for future expansion (80 bytes padding to reach 128)
    uint8_t reserved[80];

    /**
     * @brief Default constructor - zero-initialize all fields
     */
    EffectValidationSample() :
        timestamp_us(0),
        hop_seq(0),
        effect_id(INVALID_EFFECT_ID),
        reversal_count(0),
        frame_seq(0),
        phase(0.0f),
        phase_delta(0.0f),
        speed_scale_raw(0.0f),
        speed_scale_smooth(0.0f),
        dominant_freq_bin(0.0f),
        energy_avg(0.0f),
        energy_delta(0.0f),
        scroll_phase(0.0f),
        reserved{}  // Brace init for zero (no loop overhead)
    {}
};

// Static assertion to verify structure size
static_assert(sizeof(EffectValidationSample) == 128,
    "EffectValidationSample must be exactly 128 bytes");

/**
 * @brief Lock-free ring buffer for validation samples
 *
 * Single-producer, single-consumer (SPSC) ring buffer designed for
 * real-time audio-reactive effect validation. The render thread pushes
 * samples, and the WebSocket thread drains them for transmission.
 *
 * Thread safety:
 * - push() is called only from the render thread (producer)
 * - drain() is called only from the WebSocket thread (consumer)
 * - No mutex required due to SPSC design with atomic index
 *
 * Memory model:
 * - Write index is atomically updated after data is written
 * - Consumer reads write index, then reads data up to that point
 * - Memory barriers ensure proper ordering on ESP32
 *
 * @tparam N Number of samples in the ring (default 128 = 16KB)
 */
template<size_t N = 128>
class EffectValidationRing {
public:
    static_assert((N & (N - 1)) == 0, "Ring size must be a power of 2");
    static_assert(N >= 8 && N <= 1024, "Ring size must be between 8 and 1024");

    /**
     * @brief Default constructor - initialize empty ring
     * Uses memset for zero-init to avoid loop overhead during static init
     */
    EffectValidationRing() : m_write_idx(0), m_read_idx(0), m_buffer{} {
        // Brace initialization zeros the buffer (no loop overhead)
    }

    /**
     * @brief Push a sample into the ring buffer (producer only)
     *
     * Called from the render thread. If the buffer is full, the oldest
     * sample is overwritten (lossy behavior for real-time systems).
     *
     * @param sample The validation sample to push
     * @return true if sample was written, false if buffer is full (overwrites)
     */
    IRAM_ATTR bool push(const EffectValidationSample& sample) {
        uint32_t write_pos = m_write_idx;
        uint32_t next_pos = (write_pos + 1) & (N - 1);

        // Write the sample to the current position
        m_buffer[write_pos] = sample;

        // Memory barrier: ensure data is written before index update
#ifdef ESP_PLATFORM
        __asm__ __volatile__("" ::: "memory");
#endif

        // Update write index atomically
        m_write_idx = next_pos;

        // Check if we overwrote unread data
        return (next_pos != m_read_idx);
    }

    /**
     * @brief Drain available samples from the ring buffer (consumer only)
     *
     * Called from the WebSocket thread. Copies available samples to the
     * output buffer and updates the read index.
     *
     * @param out Output buffer to receive samples
     * @param max_count Maximum number of samples to drain
     * @return Number of samples actually drained
     */
    size_t drain(EffectValidationSample* out, size_t max_count) {
        if (out == nullptr || max_count == 0) {
            return 0;
        }

        // Read write index (atomic on ESP32 for uint32_t)
        uint32_t write_pos = m_write_idx;
        uint32_t read_pos = m_read_idx;

        // Calculate available samples
        size_t available = (write_pos - read_pos) & (N - 1);
        size_t to_drain = (available < max_count) ? available : max_count;

        // Memory barrier: ensure we read index before data
#ifdef ESP_PLATFORM
        __asm__ __volatile__("" ::: "memory");
#endif

        // Copy samples to output buffer
        for (size_t i = 0; i < to_drain; ++i) {
            uint32_t idx = (read_pos + i) & (N - 1);
            out[i] = m_buffer[idx];
        }

        // Memory barrier: ensure data is read before index update
#ifdef ESP_PLATFORM
        __asm__ __volatile__("" ::: "memory");
#endif

        // Update read index
        m_read_idx = (read_pos + to_drain) & (N - 1);

        return to_drain;
    }

    /**
     * @brief Get the number of samples currently in the buffer
     * @return Number of unread samples
     */
    size_t available() const {
        return (m_write_idx - m_read_idx) & (N - 1);
    }

    /**
     * @brief Check if the buffer is empty
     * @return true if no samples available
     */
    bool empty() const {
        return m_write_idx == m_read_idx;
    }

    /**
     * @brief Get the ring buffer capacity
     * @return Maximum number of samples the buffer can hold
     */
    static constexpr size_t capacity() {
        return N - 1;  // One slot reserved to distinguish full from empty
    }

    /**
     * @brief Clear all samples from the buffer
     *
     * Should only be called when both producer and consumer are idle.
     */
    void clear() {
        m_read_idx = m_write_idx;
    }

private:
    // Ring buffer storage
    EffectValidationSample m_buffer[N];

    // Indices (volatile for cross-thread visibility)
    volatile uint32_t m_write_idx;
    volatile uint32_t m_read_idx;
};

/**
 * @brief Type alias for backward compatibility with existing code
 *
 * The existing EffectValidationMacros.cpp uses ValidationRingBuffer<N>.
 * This alias maps to EffectValidationRing<N>.
 */
template<size_t N = 128>
using ValidationRingBuffer = EffectValidationRing<N>;

/**
 * @brief Global validation ring buffer pointer (lazy initialized)
 *
 * Pointer to ring buffer, allocated on first use via initValidationRing().
 * This avoids stack overflow during ESP32 static initialization by
 * deferring construction until after FreeRTOS is running.
 *
 * Call initValidationRing() from setup() before using validation macros.
 */
extern EffectValidationRing<32>* g_validationRing;

/**
 * @brief Initialize the global validation ring buffer
 *
 * Must be called from setup() or after FreeRTOS is running.
 * Safe to call multiple times (idempotent).
 */
void initValidationRing();

/**
 * @brief Check if validation ring is initialized
 */
inline bool isValidationRingReady() { return g_validationRing != nullptr; }

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Detect direction reversal in phase delta
 *
 * Returns true if the sign of phase_delta changed between frames,
 * indicating a potential jog-dial artifact where the effect direction
 * reversed due to noisy audio input.
 *
 * @param prev_delta Previous frame's phase delta
 * @param curr_delta Current frame's phase delta
 * @return true if direction reversed
 */
static inline bool detectReversal(float prev_delta, float curr_delta) {
    // Check for sign change (XOR of sign bits)
    // Also trigger if either delta is zero (transition through zero)
    if (prev_delta == 0.0f || curr_delta == 0.0f) {
        return false;  // No reversal if either is at rest
    }
    return (prev_delta > 0.0f) != (curr_delta > 0.0f);
}

/**
 * @brief Compute jerk (rate of change of acceleration)
 *
 * Jerk is the third derivative of position, computed from three
 * consecutive phase delta values. High jerk indicates jerky motion
 * that may be perceptually disturbing.
 *
 * Formula: jerk = (delta2 - 2*delta1 + delta0) / dt^2
 * Since dt is constant (one frame), we normalize to unit time.
 *
 * @param delta0 Phase delta from 2 frames ago
 * @param delta1 Phase delta from 1 frame ago
 * @param delta2 Current phase delta
 * @return Normalized jerk value (units: phase/frame^3)
 */
static inline float computeJerk(float delta0, float delta1, float delta2) {
    // Second difference approximates jerk when dt=1
    // jerk = d^2(velocity)/dt^2 = (v2 - 2*v1 + v0)
    return delta2 - 2.0f * delta1 + delta0;
}

/**
 * @brief Compute absolute jerk magnitude
 *
 * Same as computeJerk but returns absolute value for threshold comparisons.
 *
 * @param delta0 Phase delta from 2 frames ago
 * @param delta1 Phase delta from 1 frame ago
 * @param delta2 Current phase delta
 * @return Absolute jerk magnitude
 */
static inline float computeJerkMagnitude(float delta0, float delta1, float delta2) {
    float jerk = computeJerk(delta0, delta1, delta2);
    return (jerk >= 0.0f) ? jerk : -jerk;
}

/**
 * @brief Compute smoothness metric from delta history
 *
 * Returns a value between 0.0 (very jerky) and 1.0 (perfectly smooth).
 * Based on the coefficient of variation of consecutive deltas.
 *
 * @param delta0 Phase delta from 2 frames ago
 * @param delta1 Phase delta from 1 frame ago
 * @param delta2 Current phase delta
 * @return Smoothness metric (0.0 - 1.0)
 */
static inline float computeSmoothness(float delta0, float delta1, float delta2) {
    // Compute mean
    float mean = (delta0 + delta1 + delta2) / 3.0f;

    // Handle near-zero mean (avoid division by zero)
    float abs_mean = (mean >= 0.0f) ? mean : -mean;
    if (abs_mean < 0.0001f) {
        return 1.0f;  // Consider it smooth if not moving
    }

    // Compute variance
    float d0 = delta0 - mean;
    float d1 = delta1 - mean;
    float d2 = delta2 - mean;
    float variance = (d0*d0 + d1*d1 + d2*d2) / 3.0f;

    // Standard deviation
    // Approximation: sqrt via Newton-Raphson (1 iteration for embedded)
    float std_dev = variance;
    if (std_dev > 0.0f) {
        float x = std_dev;
        x = 0.5f * (x + variance / x);  // One Newton iteration
        std_dev = x;
    }

    // Coefficient of variation
    float cv = std_dev / abs_mean;

    // Map to smoothness (CV of 0 = smooth, CV >= 1 = jerky)
    float smoothness = 1.0f - cv;
    if (smoothness < 0.0f) smoothness = 0.0f;
    if (smoothness > 1.0f) smoothness = 1.0f;

    return smoothness;
}

} // namespace validation
} // namespace lightwaveos
