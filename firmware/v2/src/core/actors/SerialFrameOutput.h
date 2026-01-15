/**
 * @file SerialFrameOutput.h
 * @brief Serial output handler for frame capture system
 * 
 * Sends captured frames via Serial for external analysis.
 * 
 * Frame Format (Binary):
 * ----------------------
 * [SYNC_1][SYNC_2][TAP_ID][RESERVED][FRAME_COUNT_32][LED_COUNT_16][METADATA_6][RGB_DATA...]
 * 
 * Header (16 bytes):
 *   - Sync bytes: 0xFF 0xFE (2 bytes)
 *   - Tap ID: 0=TAP_A, 1=TAP_B, 2=TAP_C (1 byte)
 *   - Reserved: 0x00 (1 byte)
 *   - Frame count: uint32_t little-endian (4 bytes)
 *   - LED count: uint16_t little-endian (2 bytes)
 *   - Metadata: effect_id, palette_id, brightness, speed (4 bytes)
 *   - Timestamp: uint32_t microseconds (4 bytes) [offset 12-15]
 * 
 * RGB Data (led_count × 3 bytes):
 *   - Sequential RGB triplets: R0 G0 B0 R1 G1 B1 ...
 * 
 * Serial Commands:
 * ----------------
 *   capture on [tap_mask]  - Enable capture (default: all taps)
 *   capture off            - Disable capture
 *   capture status         - Show capture state
 *   capture stream [tap]   - Stream single tap continuously
 * 
 * Python Receiver:
 * ----------------
 *   tools/dither_bench/serial_frame_capture.py
 * 
 * @author LightwaveOS Team
 * @version 1.0.0
 */

#pragma once

#include <Arduino.h>
#include "RendererNode.h"

namespace lightwaveos {
namespace nodes {

class SerialFrameOutput {
public:
    /**
     * @brief Send captured frame via Serial
     * 
     * @param tap Tap point identifier
     * @param buffer LED buffer (TOTAL_LEDS size)
     * @param metadata Frame metadata (effect, palette, etc)
     */
    static void sendFrame(RendererNode::CaptureTap tap,
                         const CRGB* buffer,
                         const RendererNode::CaptureMetadata& metadata) {
        if (buffer == nullptr) return;
        
        const uint16_t ledCount = LedConfig::TOTAL_LEDS;
        
        // Header
        Serial.write(0xFF);  // Sync byte 1
        Serial.write(0xFE);  // Sync byte 2
        Serial.write(static_cast<uint8_t>(tap));
        Serial.write(0x00);  // Reserved
        
        // Frame count (32-bit little-endian)
        Serial.write((uint8_t*)&metadata.frameIndex, 4);
        
        // LED count (16-bit little-endian)
        Serial.write((uint8_t*)&ledCount, 2);
        
        // Metadata (6 bytes)
        Serial.write(metadata.effectId);
        Serial.write(metadata.paletteId);
        Serial.write(metadata.brightness);
        Serial.write(metadata.speed);
        
        // Timestamp (32-bit little-endian)
        Serial.write((uint8_t*)&metadata.timestampUs, 4);
        
        // RGB data (led_count × 3 bytes)
        for (uint16_t i = 0; i < ledCount; i++) {
            Serial.write(buffer[i].r);
            Serial.write(buffer[i].g);
            Serial.write(buffer[i].b);
        }
        
        // Optional: Add checksum footer
        // uint8_t checksum = computeChecksum(buffer, ledCount);
        // Serial.write(checksum);
    }
    
    /**
     * @brief Send captured frame with automatic tap buffer retrieval
     * 
     * @param renderer RendererNode instance
     * @param tap Tap to send
     */
    static void sendCapturedFrame(RendererNode* renderer, 
                                  RendererNode::CaptureTap tap) {
        if (renderer == nullptr) return;
        
        CRGB buffer[LedConfig::TOTAL_LEDS];
        
        if (renderer->getCapturedFrame(tap, buffer)) {
            auto metadata = renderer->getCaptureMetadata();
            sendFrame(tap, buffer, metadata);
        }
    }
    
    /**
     * @brief Stream frames continuously (blocking)
     * 
     * @param renderer RendererNode instance
     * @param tap Tap to stream
     * @param durationMs Duration in milliseconds (0 = infinite)
     */
    static void streamFrames(RendererNode* renderer,
                            RendererNode::CaptureTap tap,
                            uint32_t durationMs = 0) {
        uint32_t startTime = millis();
        uint32_t framesSent = 0;
        
        Serial.println("# Frame stream started");
        Serial.printf("# Tap: %d, Duration: %u ms\n", 
                     static_cast<uint8_t>(tap), durationMs);
        
        while (true) {
            // Check duration
            if (durationMs > 0 && (millis() - startTime) >= durationMs) {
                break;
            }
            
            // Check for stop command
            if (Serial.available()) {
                char c = Serial.read();
                if (c == 'q' || c == 'Q' || c == 27) {  // ESC
                    break;
                }
            }
            
            // Send frame if available
            sendCapturedFrame(renderer, tap);
            framesSent++;
            
            // Brief delay to allow processing
            delay(1);
        }
        
        Serial.println();
        Serial.printf("# Stream ended: %u frames sent\n", framesSent);
    }
};

} // namespace nodes
} // namespace lightwaveos
