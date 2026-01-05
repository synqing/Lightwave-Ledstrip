#pragma once
// ============================================================================
// PresetData - Full State Preset Structure for Tab5.encoder
// ============================================================================
// Captures a complete snapshot of LightwaveOS visual state for preset recall.
// Designed for efficient NVS blob storage (packed struct, 64 bytes total).
//
// Preset Scope (Full State):
//   - Global parameters: Effect, Brightness, Palette, Speed, Mood, Fade,
//     Complexity, Variation
//   - Zone state: Zone mode enabled, zone count, per-zone configs
//   - Additional settings: Gamma, Brown guardrail, Auto-exposure
//   - Metadata: Occupied flag, timestamp, checksum
//
// Storage:
//   - 8 preset slots (one per Unit-B encoder)
//   - Stored as binary blobs in NVS namespace "tab5prst"
//   - Keys: "slot0", "slot1", ..., "slot7"
//
// Usage:
//   PresetData preset;
//   preset.captureFrom(paramHandler, wsClient);  // Snapshot current state
//   preset.applyTo(wsClient);                     // Restore state
// ============================================================================

#ifndef PRESET_DATA_H
#define PRESET_DATA_H

#include <Arduino.h>

// Ensure struct is packed for consistent binary storage
#pragma pack(push, 1)

// Per-zone configuration (4 bytes per zone)
struct ZonePresetConfig {
    uint8_t effectId;      // Zone effect index
    uint8_t speed;         // Zone speed (1-100)
    uint8_t brightness;    // Zone brightness (0-255)
    uint8_t enabled : 1;   // Zone enabled flag
    uint8_t paletteId : 7; // Zone palette (0-74, 7 bits is enough for 75 palettes)
};

// Full State Preset (64 bytes total - efficient for NVS blob storage)
struct PresetData {
    // Magic number for validation (2 bytes)
    static constexpr uint16_t MAGIC = 0x5450;  // "PT" for PreseT
    uint16_t magic;

    // Version for forward compatibility (1 byte)
    uint8_t version;
    static constexpr uint8_t CURRENT_VERSION = 1;

    // ========================================================================
    // Global Parameters (8 bytes) - matches Unit-A encoder mapping
    // ========================================================================
    uint8_t effectId;      // Current effect index (0-95)
    uint8_t brightness;    // Global brightness (0-255)
    uint8_t paletteId;     // Current palette index (0-74, matches v2 MASTER_PALETTE_COUNT=75)
    uint8_t speed;         // Animation speed (1-100)
    uint8_t mood;          // Mood parameter (0-255)
    uint8_t fade;          // Fade amount (0-255)
    uint8_t complexity;    // Effect complexity (0-255)
    uint8_t variation;     // Effect variation (0-255)

    // ========================================================================
    // Zone State (18 bytes)
    // ========================================================================
    uint8_t zoneModeEnabled : 1;  // Zone Composer active
    uint8_t zoneCount : 3;        // Number of active zones (1-4)
    uint8_t reserved1 : 4;        // Reserved for future use

    ZonePresetConfig zones[4];    // 4 zones × 4 bytes = 16 bytes

    uint8_t reserved2;            // Padding for alignment

    // ========================================================================
    // Additional Settings (3 bytes)
    // ========================================================================
    uint8_t gamma;                // Gamma correction value
    uint8_t brownGuardrail : 1;   // Brown guardrail enabled
    uint8_t autoExposure : 1;     // Auto-exposure enabled
    uint8_t reserved3 : 6;        // Reserved for future use
    uint8_t reserved4;            // Padding

    // ========================================================================
    // Metadata (8 bytes)
    // ========================================================================
    uint8_t occupied;             // Slot has valid preset data
    uint8_t reserved5[3];         // Padding for alignment
    uint32_t timestamp;           // millis() when saved

    // ========================================================================
    // Checksum (2 bytes) - CRC16 of all preceding bytes
    // ========================================================================
    uint16_t checksum;

    // Reserved space for future expansion (22 bytes)
    // Total: 2+1+8+18+3+8+2+22 = 64 bytes
    // Zone segments (16 bytes) stored in reservedFuture[0-15] when zone mode is enabled
    // Format: 4 zones × 4 bytes each (s1LeftStart, s1LeftEnd, s1RightStart, s1RightEnd)
    uint8_t reservedFuture[22];

    // ========================================================================
    // Helper methods for zone segment access (stored in reservedFuture[0-15])
    // ========================================================================
    
    /**
     * Get zone segment data (stored in reservedFuture)
     * @param zoneId Zone ID (0-3)
     * @param s1LeftStart Output
     * @param s1LeftEnd Output
     * @param s1RightStart Output
     * @param s1RightEnd Output
     */
    void getZoneSegment(uint8_t zoneId, uint8_t& s1LeftStart, uint8_t& s1LeftEnd, 
                       uint8_t& s1RightStart, uint8_t& s1RightEnd) const {
        if (zoneId >= 4) return;
        uint8_t offset = zoneId * 4;
        s1LeftStart = reservedFuture[offset];
        s1LeftEnd = reservedFuture[offset + 1];
        s1RightStart = reservedFuture[offset + 2];
        s1RightEnd = reservedFuture[offset + 3];
    }

    /**
     * Set zone segment data (stored in reservedFuture)
     * @param zoneId Zone ID (0-3)
     * @param s1LeftStart
     * @param s1LeftEnd
     * @param s1RightStart
     * @param s1RightEnd
     */
    void setZoneSegment(uint8_t zoneId, uint8_t s1LeftStart, uint8_t s1LeftEnd,
                       uint8_t s1RightStart, uint8_t s1RightEnd) {
        if (zoneId >= 4) return;
        uint8_t offset = zoneId * 4;
        reservedFuture[offset] = s1LeftStart;
        reservedFuture[offset + 1] = s1LeftEnd;
        reservedFuture[offset + 2] = s1RightStart;
        reservedFuture[offset + 3] = s1RightEnd;
    }

    // ========================================================================
    // Methods
    // ========================================================================

    // Initialize to empty/default state
    void clear() {
        memset(this, 0, sizeof(PresetData));
        magic = MAGIC;
        version = CURRENT_VERSION;
        occupied = 0;

        // Set sensible defaults for when preset is applied
        brightness = 128;
        speed = 25;
        gamma = 22;  // Default gamma 2.2 (stored as 22)
        zoneCount = 1;
    }

    // Validate preset data integrity
    bool isValid() const {
        if (magic != MAGIC) return false;
        if (version == 0 || version > CURRENT_VERSION) return false;
        if (!occupied) return false;
        return checksum == calculateChecksum();
    }

    // Check if slot is empty
    bool isEmpty() const {
        return !occupied || magic != MAGIC;
    }

    // Calculate CRC16 checksum of preset data
    uint16_t calculateChecksum() const {
        // CRC16-CCITT implementation
        uint16_t crc = 0xFFFF;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(this);

        // Calculate CRC over all bytes except the checksum field itself
        // checksum is at offset 40 (2 bytes before reservedFuture)
        constexpr size_t checksumOffset = offsetof(PresetData, checksum);

        for (size_t i = 0; i < checksumOffset; i++) {
            crc ^= static_cast<uint16_t>(data[i]) << 8;
            for (int j = 0; j < 8; j++) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ 0x1021;
                } else {
                    crc <<= 1;
                }
            }
        }
        return crc;
    }

    // Update checksum after modifying preset data
    void updateChecksum() {
        checksum = calculateChecksum();
    }

    // Mark preset as occupied with current timestamp
    void markOccupied() {
        occupied = 1;
        timestamp = millis();
        updateChecksum();
    }
};

#pragma pack(pop)

// Verify struct size at compile time
static_assert(sizeof(PresetData) == 64, "PresetData must be exactly 64 bytes");
static_assert(sizeof(ZonePresetConfig) == 4, "ZonePresetConfig must be exactly 4 bytes");

// Number of preset slots (one per Unit-B encoder)
constexpr uint8_t PRESET_SLOT_COUNT = 8;

#endif // PRESET_DATA_H
