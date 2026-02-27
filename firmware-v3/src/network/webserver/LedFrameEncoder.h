/**
 * @file LedFrameEncoder.h
 * @brief LED frame encoder for WebSocket binary streaming
 *
 * Encodes LED buffer data into binary frame format for real-time streaming.
 * Frame format v1: [MAGIC][VERSION][NUM_STRIPS][LEDS_PER_STRIP][STRIP0_ID][RGB×N][STRIP1_ID][RGB×N]
 *
 * Extracted from WebServer for better separation of concerns and testability.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <FastLED.h>

namespace lightwaveos {
namespace network {
namespace webserver {

// ============================================================================
// LED Stream Configuration
// ============================================================================

namespace LedStreamConfig {
    // Dual-strip configuration
    constexpr uint16_t LEDS_PER_STRIP = 160;          // LEDs per strip (top/bottom edges)
    constexpr uint8_t NUM_STRIPS = 2;                 // Number of independent strips
    constexpr uint16_t TOTAL_LEDS = LEDS_PER_STRIP * NUM_STRIPS;  // Total LEDs (320)
    
    // Frame format version 1: explicit dual-strip format
    constexpr uint8_t FRAME_VERSION = 1;              // Frame format version
    constexpr uint8_t MAGIC_BYTE = 0xFE;              // Frame header magic byte
    
    // Frame structure: [MAGIC][VERSION][NUM_STRIPS][LEDS_PER_STRIP][STRIP0_ID][RGB×160][STRIP1_ID][RGB×160]
    constexpr uint8_t FRAME_HEADER_SIZE = 4;          // Magic + Version + NumStrips + LEDsPerStrip
    constexpr uint16_t FRAME_SIZE_PER_STRIP = 1 + (LEDS_PER_STRIP * 3);  // StripID + RGB data (481 bytes)
    constexpr uint16_t FRAME_PAYLOAD_SIZE = NUM_STRIPS * FRAME_SIZE_PER_STRIP;  // Both strips (962 bytes)
    constexpr uint16_t FRAME_SIZE = FRAME_HEADER_SIZE + FRAME_PAYLOAD_SIZE;  // Total frame size (966 bytes)
    
    // Legacy format (v0): [MAGIC][RGB×320] = 961 bytes
    constexpr uint16_t LEGACY_FRAME_SIZE = 1 + (TOTAL_LEDS * 3);  // 961 bytes
    
    constexpr uint8_t TARGET_FPS = 10;                // Max streaming FPS (throttled for WiFi headroom)
    constexpr uint32_t FRAME_INTERVAL_MS = 1000 / TARGET_FPS;  // ~100ms between frames
}

// ============================================================================
// LED Frame Encoder
// ============================================================================

/**
 * @brief Encodes LED buffer into binary frame format
 *
 * Thread-safe: caller must ensure buffer is not modified during encoding.
 * Buffer must be at least LedStreamConfig::FRAME_SIZE bytes.
 */
class LedFrameEncoder {
public:
    /**
     * @brief Encode LED buffer into frame format
     * @param leds Source LED buffer (must have TOTAL_LEDS entries)
     * @param outputBuffer Output buffer (must be at least FRAME_SIZE bytes)
     * @param bufferSize Size of output buffer
     * @return Number of bytes written, or 0 on error
     */
    static size_t encode(const CRGB* leds, uint8_t* outputBuffer, size_t bufferSize) {
        if (!leds || !outputBuffer) return 0;
        if (bufferSize < LedStreamConfig::FRAME_SIZE) return 0;
        
        uint8_t* dst = outputBuffer;
        
        // Header: [MAGIC][VERSION][NUM_STRIPS][LEDS_PER_STRIP]
        *dst++ = LedStreamConfig::MAGIC_BYTE;
        *dst++ = LedStreamConfig::FRAME_VERSION;
        *dst++ = LedStreamConfig::NUM_STRIPS;
        *dst++ = (uint8_t)LedStreamConfig::LEDS_PER_STRIP;  // Cast to uint8_t (160 fits)
        
        // Strip 0 (TOP edge, GPIO4): indices 0..159
        *dst++ = 0;  // Strip ID
        for (uint16_t i = 0; i < LedStreamConfig::LEDS_PER_STRIP; i++) {
            *dst++ = leds[i].r;
            *dst++ = leds[i].g;
            *dst++ = leds[i].b;
        }
        
        // Strip 1 (BOTTOM edge, GPIO5): indices 160..319
        *dst++ = 1;  // Strip ID
        for (uint16_t i = 0; i < LedStreamConfig::LEDS_PER_STRIP; i++) {
            uint16_t unifiedIdx = LedStreamConfig::LEDS_PER_STRIP + i;
            *dst++ = leds[unifiedIdx].r;
            *dst++ = leds[unifiedIdx].g;
            *dst++ = leds[unifiedIdx].b;
        }
        
        return LedStreamConfig::FRAME_SIZE;
    }
    
    /**
     * @brief Validate frame format
     * @param frame Frame buffer
     * @param frameSize Size of frame buffer
     * @return true if valid, false otherwise
     */
    static bool validate(const uint8_t* frame, size_t frameSize) {
        if (!frame || frameSize < LedStreamConfig::FRAME_HEADER_SIZE) return false;
        
        // Check magic byte
        if (frame[0] != LedStreamConfig::MAGIC_BYTE) return false;
        
        // Check version
        if (frame[1] != LedStreamConfig::FRAME_VERSION) return false;
        
        // Check size
        if (frameSize != LedStreamConfig::FRAME_SIZE) return false;
        
        return true;
    }
    
    /**
     * @brief Get frame size for current format
     */
    static constexpr size_t getFrameSize() {
        return LedStreamConfig::FRAME_SIZE;
    }
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

