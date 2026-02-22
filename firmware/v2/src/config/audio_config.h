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
 * - Proven sample conversion (see AudioCapture.cpp)
 */

#pragma once

#include "features.h"
#include "chip_config.h"

#if FEATURE_AUDIO_SYNC

#include <driver/gpio.h>

namespace lightwaveos {
namespace audio {

// ============================================================================
// Microphone Type Selection
// ============================================================================

/**
 * Select which I2S MEMS microphone is wired to the board.
 * Each mic type has different bit depth, channel slot, and I2S register needs.
 *
 * SPH0645:  18-bit, RIGHT channel on ESP32-S3 legacy driver, >>10 shift
 * IM69D130: 24-bit via ADAU7002, LEFT channel, >>8 shift, MSB_SHIFT set
 */
enum class MicType : uint8_t {
    SPH0645,    // Sparkfun SPH0645LM4H (original K1 dev board)
    IM69D130    // Infineon IM69D130 + ADAU7002 Shield2Go (K1 v2)
};

constexpr MicType MICROPHONE_TYPE = MicType::SPH0645;

// ============================================================================
// I2S Pin Configuration
// ============================================================================

/**
 * I2S MEMS Microphone pinout (board-dependent):
 *
 * SPH0645 (legacy):
 *   GPIO 12/13/14 — avoids LED (4/5), I2C (17/18), strapping (0/3/45/46)
 *
 * IM69D130 + ADAU7002 Shield2Go (PipelineCore):
 *   GPIO 36/38/39 — proven in im69d130_i2s_headless bringup
 *   Frees GPIO 14 for WS2812 LED data (Waveshare ESP32-S3-Matrix)
 */
#if CHIP_ESP32_P4
constexpr gpio_num_t I2S_BCLK_PIN = static_cast<gpio_num_t>(chip::gpio::I2S_BCLK);
constexpr gpio_num_t I2S_DIN_PIN = static_cast<gpio_num_t>(chip::gpio::I2S_DIN);
constexpr gpio_num_t I2S_DOUT_PIN = static_cast<gpio_num_t>(chip::gpio::I2S_DOUT);
constexpr gpio_num_t I2S_LRCL_PIN = static_cast<gpio_num_t>(chip::gpio::I2S_LRCL);
constexpr uint16_t I2S_MCLK_MULTIPLE = 384;
#elif FEATURE_AUDIO_BACKEND_PIPELINECORE
// PipelineCore DSP with SPH0645 mic — same I2S pins as default backend.
// GPIO 38 is reserved for StatusStrip (WS2812 RMT); do NOT use for I2S.
constexpr gpio_num_t I2S_BCLK_PIN = GPIO_NUM_14;  // Bit Clock
constexpr gpio_num_t I2S_DIN_PIN  = GPIO_NUM_13;  // Data In (mic -> ESP)
constexpr gpio_num_t I2S_DOUT_PIN = GPIO_NUM_13;  // (alias for legacy driver compatibility)
constexpr gpio_num_t I2S_LRCL_PIN = GPIO_NUM_12;  // Word Select (L/R Clock)
#else
// SPH0645 on K1 dev board
// GPIO 12/13/14 — avoids LED (4/5), I2C (17/18), strapping (0/3/45/46)
constexpr gpio_num_t I2S_BCLK_PIN = GPIO_NUM_14;  // Bit Clock
constexpr gpio_num_t I2S_DIN_PIN  = GPIO_NUM_13;  // Data In (mic -> ESP)
constexpr gpio_num_t I2S_DOUT_PIN = GPIO_NUM_13;  // (alias for legacy driver compatibility)
constexpr gpio_num_t I2S_LRCL_PIN = GPIO_NUM_12;  // Word Select (L/R Clock)
#endif

// ============================================================================
// Audio Processing Parameters (Tab5 Parity)
// ============================================================================

/**
 * Audio timing:
 * - ESP32-S3: 12.8kHz / HOP_N=256 = 50 Hz frames (Tab5 parity)
 * - ESP32-P4: 16kHz / HOP_N=128 = 125 Hz frames (P4 front end parity)
 *
 * DO NOT change hop size without updating:
 * - Filter constants in ControlBus
 * - Resonator Q values in beat tracker (TempoTracker SPECTRAL_LOG_HZ, VU_LOG_HZ)
 * - Attack/release envelope timing
 */
// Prevent vendor SAMPLE_RATE macro (from -include shim or global_defines.h)
// from colliding with our C++ constexpr below.
#ifdef SAMPLE_RATE
#undef SAMPLE_RATE
#endif

#if CHIP_ESP32_P4
// P4 target: align hop to scheduler reality (100 Hz) while preserving 16 kHz audio
// FreeRTOS tick = 100 Hz (10ms), so we use 160 samples @ 16kHz = 10ms = 100 Hz hops
// This eliminates timing drift and multi-hop compensation hacks
constexpr uint16_t SAMPLE_RATE = 16000;       // 16 kHz (P4 onboard front end)
constexpr uint16_t HOP_SIZE = 160;            // 10ms hop @ 16kHz = 100 Hz frames
#elif FEATURE_AUDIO_BACKEND_PIPELINECORE
// PipelineCore: 32kHz / 256-hop = 125 Hz frames (proven at 15% CPU on ESP32-S3)
constexpr uint16_t SAMPLE_RATE = 32000;       // 32 kHz (SPH0645 overclocked)
constexpr uint16_t HOP_SIZE = 256;            // 8ms hop @ 32kHz = 125 Hz frames
#elif FEATURE_AUDIO_BACKEND_ESV11_32KHZ
// ESV11 at 32kHz: SPH0645 overclocked, 256-hop, 125 Hz frame rate
// All vendor constants (SAMPLE_RATE, CHUNK_SIZE, etc.) overridden in EsV11_32kHz_Shim.h
constexpr uint16_t SAMPLE_RATE = 32000;       // 32 kHz (SPH0645 overclocked)
constexpr uint16_t HOP_SIZE = 256;            // 8ms hop @ 32kHz = 125 Hz frames
constexpr uint16_t ESV11_CHUNK_SIZE = 128;    // 4ms @ 32kHz (matches shim override)
#elif FEATURE_AUDIO_BACKEND_ESV11
// ESV11 at 12.8kHz (default): matches vendor global_defines.h
constexpr uint16_t SAMPLE_RATE = 12800;       // 12.8 kHz (Emotiscope match)
constexpr uint16_t HOP_SIZE = 256;            // 20ms hop @ 12.8kHz = 50 Hz frames
constexpr uint16_t ESV11_CHUNK_SIZE = 64;     // 5ms @ 12.8kHz (vendor default)
#else
constexpr uint16_t SAMPLE_RATE = 12800;       // 12.8 kHz (Emotiscope match)
constexpr uint16_t HOP_SIZE = 256;            // 20ms hop @ 12.8kHz = 50 Hz frames
#endif
constexpr uint16_t FFT_SIZE = 512;            // For beat tracker spectral analysis
constexpr uint16_t GOERTZEL_WINDOW = 512;     // 32ms window for bass coherence

// Derived timing constants
constexpr float HOP_DURATION_MS = (HOP_SIZE * 1000.0f) / SAMPLE_RATE;  // 20ms @ 12.8kHz
constexpr float HOP_RATE_HZ = SAMPLE_RATE / static_cast<float>(HOP_SIZE);  // 50 Hz @ 12.8kHz

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
// Sample Format and I2S Configuration
// ============================================================================

/**
 * Both SPH0645 and IM69D130 use 32-bit I2S slots.
 *
 * SPH0645:  18-bit data, RIGHT channel, >>10 shift, MSB_SHIFT cleared
 * IM69D130: 24-bit data, LEFT channel, >>8 shift, MSB_SHIFT set
 *
 * Channel selection and bit shift are handled at runtime in AudioCapture.cpp
 * based on MICROPHONE_TYPE above.
 */
constexpr uint8_t I2S_BITS_PER_SAMPLE = 32;   // 32-bit slots for both mic types

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
constexpr float STALENESS_THRESHOLD_MS = 100.0f;  // 100ms = 5 frames @ 50 Hz

// ============================================================================
// Actor Configuration
// ============================================================================

/**
 * AudioActor runs on Core 0 at priority 4 (below Renderer at 5).
 * 20ms tick interval matches hop size for precise timing.
 */
constexpr uint8_t AUDIO_ACTOR_PRIORITY = 4;
constexpr uint8_t AUDIO_ACTOR_CORE = 0;
constexpr uint16_t AUDIO_ACTOR_STACK_WORDS = 4096;  // 16KB stack (reverted from 32KB - original was 3072)
constexpr uint16_t AUDIO_ACTOR_TICK_MS = static_cast<uint16_t>(HOP_DURATION_MS + 0.5f);

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
