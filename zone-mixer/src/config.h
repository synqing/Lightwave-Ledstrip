/**
 * Zone Mixer — Configuration Constants
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstdint>

// ============================================================================
// Hardware — AtomS3 + PaHub v2.1
// ============================================================================

namespace hw {
    // AtomS3 Grove I2C (NOT the ESP32 defaults!)
    constexpr uint8_t I2C_SDA = 2;
    constexpr uint8_t I2C_SCL = 1;
    constexpr uint32_t I2C_FREQ = 400000;  // 400kHz Fast Mode

    // PaHub v2.1 (PCA9548A)
    constexpr uint8_t PAHUB_ADDR = 0x70;

    // PaHub channel assignments (matches physical wiring)
    constexpr uint8_t CH_ENC_A    = 0;  // 8Encoder at 0x42
    constexpr uint8_t CH_ENC_B    = 1;  // 8Encoder at 0x41
    constexpr uint8_t CH_SCROLL_1 = 2;  // Zone 1 brightness
    constexpr uint8_t CH_SCROLL_2 = 3;  // Zone 2 brightness
    constexpr uint8_t CH_SCROLL_3 = 4;  // Zone 3 brightness
    constexpr uint8_t CH_SCROLL_M = 5;  // Master brightness

    // Device I2C addresses
    constexpr uint8_t ENCODER_A_ADDR = 0x42;  // Non-default (was Tab5 Unit A)
    constexpr uint8_t ENCODER_B_ADDR = 0x41;  // Default
    constexpr uint8_t SCROLL_ADDR    = 0x40;  // Default (all Scroll units)

    // Unit-Scroll registers
    constexpr uint8_t SCROLL_REG_ENCODER = 0x10;
    constexpr uint8_t SCROLL_REG_BUTTON  = 0x20;
    constexpr uint8_t SCROLL_REG_LED     = 0x30;
    constexpr uint8_t SCROLL_REG_INC     = 0x50;

    // Zone colours (1-indexed: Zone 1, Zone 2, Zone 3)
    constexpr uint32_t ZONE_COLOUR[3] = {
        0x00FFFF,  // Zone 1: Cyan (innermost)
        0x00FF99,  // Zone 2: Green (middle)
        0xFF6600,  // Zone 3: Orange (outermost)
    };
    constexpr uint32_t MASTER_COLOUR = 0xFFFFFF;

    // Polling
    constexpr uint32_t POLL_INTERVAL_US = 10000;  // 100Hz target
    constexpr uint32_t INTER_TXN_DELAY_US = 100;  // STM32F030 breathing room
}

// ============================================================================
// Network — K1 Connection
// ============================================================================

namespace net {
    // K1 AP (always at this address — AP-only architecture)
    constexpr const char* K1_SSID     = "LightwaveOS-AP";
    constexpr const char* K1_PASSWORD = "";  // Open network
    constexpr const char* K1_IP       = "192.168.4.1";
    constexpr uint16_t    K1_PORT     = 80;
    constexpr const char* K1_WS_PATH  = "/ws";

    // WiFi timeouts
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
    constexpr uint32_t WIFI_RECONNECT_DELAY_MS = 5000;
    constexpr uint32_t WIFI_MAX_RECONNECT_MS   = 30000;

    // WebSocket timeouts
    constexpr uint32_t WS_INITIAL_RECONNECT_MS  = 1000;
    constexpr uint32_t WS_MAX_RECONNECT_MS      = 30000;
    constexpr uint32_t WS_CONNECTION_TIMEOUT_MS  = 5000;   // Reduced from 20s — must stay under WDT (5s)

    // Rate limiting
    constexpr uint32_t PARAM_THROTTLE_MS         = 50;
    constexpr uint32_t SEND_QUEUE_STALE_MS       = 500;
    constexpr uint32_t SEND_MUTEX_TIMEOUT_MS     = 10;
    constexpr uint32_t SEND_WARN_THRESHOLD_MS    = 50;
    constexpr uint8_t  MAX_CONSECUTIVE_FAILURES  = 3;
    constexpr uint8_t  SEND_QUEUE_SIZE           = 14;

    // JSON buffer
    constexpr size_t JSON_BUFFER_SIZE = 512;
}

// ============================================================================
// Zones — HARD RULE: 3 zones, 1-indexed
// ============================================================================

namespace zones {
    constexpr uint8_t MAX_ZONES = 3;
}
