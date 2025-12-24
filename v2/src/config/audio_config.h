/**
 * @file audio_config.h
 * @brief Audio pipeline configuration for LightwaveOS v2
 *
 * I2S configuration for SPH0645 MEMS microphone and Tab5-compatible
 * audio processing parameters.
 *
 * Critical constraints:
 * - Hop size = 256 (Tab5 parity for beat tracker)
 * - ESP-IDF 5.x new I2S driver (driver/i2s_std.h)
 * - 32-bit I2S slots, shift >>16 for 16-bit samples
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
// Audio Processing Parameters (Tab5 Parity)
// ============================================================================

/**
 * Tab5 beat tracker timing: 16kHz / FFT_N=512 / HOP_N=256 = 62.5 Hz frames
 *
 * DO NOT change hop size without updating:
 * - Filter constants in ControlBus
 * - Resonator Q values in beat tracker
 * - Attack/release envelope timing
 */
constexpr uint16_t SAMPLE_RATE = 16000;       // 16 kHz (Tab5 standard)
constexpr uint16_t HOP_SIZE = 256;            // 16ms hop @ 16kHz = 62.5 Hz frames
constexpr uint16_t FFT_SIZE = 512;            // For beat tracker spectral analysis
constexpr uint16_t GOERTZEL_WINDOW = 512;     // 32ms window for bass coherence

// Derived timing constants
constexpr float HOP_DURATION_MS = (HOP_SIZE * 1000.0f) / SAMPLE_RATE;  // 16ms
constexpr float HOP_RATE_HZ = SAMPLE_RATE / static_cast<float>(HOP_SIZE);  // 62.5 Hz

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
// SPH0645 Sample Format
// ============================================================================

/**
 * SPH0645 outputs 24-bit 2's complement, MSB-first, 18-bit precision.
 * Configure I2S for 32-bit slots, then extract:
 *
 *   int32_t raw = i2s_buffer[i];
 *   int16_t sample = (int16_t)(raw >> 16);  // Standard 16-bit
 *   // Or: raw >> 14 for 18-bit precision scaling
 */
constexpr uint8_t I2S_BITS_PER_SAMPLE = 32;   // 32-bit slots for SPH0645
constexpr uint8_t SAMPLE_SHIFT_BITS = 16;     // Shift to 16-bit signed

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
    60, 120, 250, 500, 1000, 2000, 4000, 8000
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
// Actor Configuration
// ============================================================================

/**
 * AudioActor runs on Core 0 at priority 4 (below Renderer at 5).
 * 16ms tick interval matches hop size for precise timing.
 */
constexpr uint8_t AUDIO_ACTOR_PRIORITY = 4;
constexpr uint8_t AUDIO_ACTOR_CORE = 0;
constexpr uint16_t AUDIO_ACTOR_STACK_WORDS = 3072;  // 12KB stack
constexpr uint16_t AUDIO_ACTOR_TICK_MS = 16;        // Match hop duration

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
