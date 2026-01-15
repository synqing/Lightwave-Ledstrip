/**
 * @file AudioCanonicalConfig.h
 * @brief Shared constants for canonical audio pipeline
 * 
 * FROM SENSORY BRIDGE 4.1.1 + EMOTISCOPE 2.0 - DO NOT MODIFY
 * 
 * This header defines constants shared across multiple audio modules
 * to prevent duplication and ensure consistency.
 * 
 * @version 1.0.0
 * @date 2026-01-13
 */

#ifndef AUDIO_CANONICAL_CONFIG_H
#define AUDIO_CANONICAL_CONFIG_H

#include <cstdint>

namespace LightwaveOS {
namespace Audio {

// FROM SENSORY BRIDGE 4.1.1 - DO NOT MODIFY
constexpr uint16_t LWOS_SAMPLE_RATE = 16000;  // 16 kHz (I2S mic native rate)
constexpr uint16_t LWOS_CHUNK_SIZE  = 128;    // Matches NATIVE_RESOLUTION
constexpr uint8_t  LWOS_NUM_FREQS   = 64;     // MUST match Sensory Bridge

// FROM EMOTISCOPE 2.0 - DO NOT MODIFY  
constexpr uint8_t  NOVELTY_LOG_HZ   = 50;     // Novelty curve sampling rate
constexpr uint16_t NOVELTY_HISTORY_LENGTH = 1024; // 50 FPS for 20.48 seconds

} // namespace Audio
} // namespace LightwaveOS

#endif // AUDIO_CANONICAL_CONFIG_H
