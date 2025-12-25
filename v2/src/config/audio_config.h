/**
 * @file audio_config.h
 * @brief Audio pipeline configuration for LightwaveOS v2
 *
 * I2S configuration for SPH0645 MEMS microphone with TWO-RATE PIPELINE:
 *
 * FAST LANE (125 Hz / 8ms):
 *   - HOP_FAST = 128 samples
 *   - Quick texture updates (RMS, flux, bands)
 *   - Punchy, low-latency visual micro-motion
 *
 * BEAT LANE (62.5 Hz / 16ms - Tab5 parity):
 *   - HOP_BEAT = 256 samples
 *   - WINDOW_BEAT = 512 samples (32ms for bass coherence)
 *   - Stable tempo/beat tracking
 *
 * Critical constraints:
 * - Beat lane must stay at 256-hop for Tab5 parity
 * - ESP-IDF 5.x new I2S driver (driver/i2s_std.h) - currently using legacy for Arduino compat
 * - Emotiscope-proven sample conversion (see AudioCapture.cpp)
 */

#pragma once

#include "features.h"

#if FEATURE_AUDIO_SYNC

#include <driver/gpio.h>

namespace lightwaveos {
namespace audio {

// ============================================================================
// I2S Pin Configuration (SPH0645 Wiring)
// ============================================================================

/**
 * SPH0645 I2S MEMS Microphone pinout:
 * - SEL tied to GND = Left channel (mono)
 * - GPIO 12/13/14 chosen to avoid conflicts with:
 *   - GPIO 4/5: WS2812 LED strips
 *   - GPIO 17/18: I2C (M5ROTATE8 encoder)
 *   - GPIO 0/3/45/46: ESP32-S3 strapping pins
 */
constexpr gpio_num_t I2S_BCLK_PIN = GPIO_NUM_14;  // Bit Clock
constexpr gpio_num_t I2S_DOUT_PIN = GPIO_NUM_13;  // Data Out (mic output)
constexpr gpio_num_t I2S_LRCL_PIN = GPIO_NUM_12;  // Left/Right Clock (Word Select)

// ============================================================================
// Audio Processing Parameters (Two-Rate Pipeline)
// ============================================================================

/**
 * Two-rate pipeline architecture:
 *
 * FAST LANE: 128 samples @ 16kHz = 8ms = 125 Hz
 *   - Texture features (RMS, flux, bands)
 *   - Sliding 512-sample window for Goertzel (75% overlap)
 *   - Fast, punchy visual response
 *
 * BEAT LANE: 256 samples @ 16kHz = 16ms = 62.5 Hz (Tab5 parity)
 *   - Beat/tempo tracking with longer integration
 *   - 512-sample analysis window
 *   - Stable musical time grid
 *
 * DO NOT change beat hop size without updating:
 * - Filter constants in ControlBus
 * - Resonator Q values in beat tracker
 * - Attack/release envelope timing
 */
constexpr uint16_t SAMPLE_RATE = 16000;       // 16 kHz (Tab5 standard)

// Fast lane (texture) - 125 Hz updates
constexpr uint16_t HOP_FAST = 128;            // 8ms hop @ 16kHz = 125 Hz frames
constexpr float HOP_FAST_MS = (HOP_FAST * 1000.0f) / SAMPLE_RATE;  // 8ms
constexpr float HOP_FAST_HZ = SAMPLE_RATE / static_cast<float>(HOP_FAST);  // 125 Hz

// Beat lane (tempo/beat) - 62.5 Hz updates (Tab5 parity)
constexpr uint16_t HOP_BEAT = 256;            // 16ms hop @ 16kHz = 62.5 Hz frames
constexpr uint16_t WINDOW_BEAT = 512;         // 32ms window for beat analysis
constexpr float HOP_BEAT_MS = (HOP_BEAT * 1000.0f) / SAMPLE_RATE;  // 16ms
constexpr float HOP_BEAT_HZ = SAMPLE_RATE / static_cast<float>(HOP_BEAT);  // 62.5 Hz

// Goertzel analysis window (sliding, updated every HOP_FAST)
constexpr uint16_t GOERTZEL_WINDOW = 512;     // 32ms window for bass coherence

// Legacy aliases for backward compatibility
constexpr uint16_t HOP_SIZE = HOP_FAST;       // Actor ticks at fast rate
constexpr uint16_t FFT_SIZE = WINDOW_BEAT;    // For beat tracker spectral analysis
constexpr float HOP_DURATION_MS = HOP_FAST_MS;
constexpr float HOP_RATE_HZ = HOP_FAST_HZ;

// ============================================================================
// I2S DMA Configuration
// ============================================================================

/**
 * DMA buffer sizing:
 * - 4 buffers for smooth double-buffering with margin
 * - 512 samples per buffer (2 hops worth)
 * - Total DMA memory: 4 * 512 * 4 bytes = 8KB
 */
constexpr size_t DMA_BUFFER_COUNT = 4;
constexpr size_t DMA_BUFFER_SAMPLES = 512;

// ============================================================================
// SPH0645 Sample Format and I2S Configuration
// ============================================================================

/**
 * SPH0645 outputs 18-bit data, MSB-first, in 32-bit I2S slots.
 * 
 * I2S Configuration (ESP32-S3 legacy driver):
 * - I2S_CHANNEL_FMT_ONLY_RIGHT (SEL=GND wiring; ESP32-S3 expects right slot)
 * - 32-bit slots (I2S_BITS_PER_SAMPLE_32BIT)
 * - MSB shift enabled (REG_SET_BIT I2S_RX_MSB_SHIFT) - aligns SPH0645 data
 * - Timing delay enabled (REG_SET_BIT BIT(9) in I2S_RX_TIMING_REG)
 * - WS idle polarity inverted (REG_SET_BIT I2S_RX_WS_IDLE_POL)
 *
 * Sample conversion (see AudioCapture.cpp):
 *   1. Shift >> 14 to extract 18-bit value (validate via DMA dbg pk>>N output)
 *   2. Clip to ±131072 (18-bit signed range)
 *   3. Scale to 16-bit (>> 2)
 *   4. DC removal handled in AudioActor
 */
constexpr uint8_t I2S_BITS_PER_SAMPLE = 32;   // 32-bit slots for SPH0645

// ============================================================================
// ControlBus Band Configuration
// ============================================================================

/**
 * 8-band frequency analysis matching Tab5 ControlBus.
 * Goertzel target frequencies (approximately log-spaced):
 *   Band 0: ~60 Hz  (sub-bass / kick)
 *   Band 1: ~120 Hz (bass)
 *   Band 2: ~250 Hz (low-mid)
 *   Band 3: ~500 Hz (mid)
 *   Band 4: ~1000 Hz (upper-mid)
 *   Band 5: ~2000 Hz (presence)
 *   Band 6: ~4000 Hz (brilliance)
 *   Band 7: ~8000 Hz (air / hi-hats)
 */
constexpr uint8_t NUM_BANDS = 8;

constexpr uint16_t BAND_CENTER_FREQUENCIES[NUM_BANDS] = {
    60, 120, 250, 500, 1000, 2000, 4000, 7800
};

// ============================================================================
// Staleness Threshold
// ============================================================================

/**
 * Audio data is considered "fresh" if less than this many ms old.
 * When stale, effects should fall back to time-based animation.
 */
constexpr float STALENESS_THRESHOLD_MS = 100.0f;  // 100ms = 6 frames @ 62.5 Hz

// ============================================================================
// Beat Tracker Configuration
// ============================================================================

/**
 * Band-weighted spectral flux for transient detection.
 * Bass bands (0-1) are weighted heavier to detect kick drums.
 */
constexpr float BASS_WEIGHT = 2.0f;      // Weight for bands 0-1 (sub-bass, bass)
constexpr float MID_WEIGHT = 1.0f;       // Weight for bands 2-4 (low-mid to upper-mid)
constexpr float HIGH_WEIGHT = 0.5f;      // Weight for bands 5-7 (presence to air)

/**
 * Adaptive threshold parameters.
 * threshold = ema_mean + k * ema_std
 */
constexpr float ONSET_THRESHOLD_K = 1.5f;        // Std dev multiplier
constexpr float ONSET_EMA_ALPHA = 0.1f;          // EMA smoothing (higher = more reactive)
constexpr float RMS_FLOOR = 0.02f;               // Minimum RMS to consider (silence gate)

/**
 * BPM tracking range (prevents octave errors).
 */
constexpr float MIN_BPM = 60.0f;
constexpr float MAX_BPM = 180.0f;

// ============================================================================
// Actor Configuration
// ============================================================================

/**
 * AudioActor runs on Core 0 at priority 4 (below Renderer at 5).
 * 8ms tick interval matches fast hop size for texture updates.
 */
constexpr uint8_t AUDIO_ACTOR_PRIORITY = 4;
constexpr uint8_t AUDIO_ACTOR_CORE = 0;
constexpr uint16_t AUDIO_ACTOR_STACK_WORDS = 8192;  // 32KB stack
constexpr uint16_t AUDIO_ACTOR_TICK_MS = 8;         // Match HOP_FAST (125 Hz)

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
