// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
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
// Version history:
//   v1 - Initial layout. Global effectId stored low byte only + reservedFuture[0]
//        for high byte (but high byte was outside CRC coverage). Zone effectId
//        was bare uint8_t — values > 255 silently truncated.
//   v2 - 16-bit zone effect IDs: effectIdHigh bytes stored in reservedFuture[1..4].
//        Checksum moved to end of struct (offset 62) so it covers all bytes
//        including zone high bytes and global effectId high byte.
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
//
// NOTE: effectIdLow stores bits [7:0] of the zone's 16-bit effect ID.
// The corresponding high byte (bits [15:8]) lives in
// PresetData::reservedFuture[1 + zoneIndex] so that it is covered by the
// CRC16 checksum.  Use setZoneEffectId() / getZoneEffectId() on the parent
// PresetData struct rather than accessing effectIdLow directly.
struct ZonePresetConfig {
    uint8_t effectIdLow;   // Zone effect ID low byte (bits [7:0])
    uint8_t speed;         // Zone speed (1-100)
    uint8_t brightness;    // Zone brightness (0-255)
    uint8_t enabled : 1;   // Zone enabled flag
    uint8_t paletteId : 7; // Zone palette (0-63, 7 bits is enough)
};

// Full State Preset (64 bytes total - efficient for NVS blob storage)
struct PresetData {
    // Magic number for validation (2 bytes)
    static constexpr uint16_t MAGIC = 0x5450;  // "PT" for PreseT
    uint16_t magic;

    // Version for forward compatibility (1 byte)
    uint8_t version;
    static constexpr uint8_t CURRENT_VERSION = 2;
    static constexpr uint8_t V1_VERSION      = 1;  // Used for migration

    // ========================================================================
    // Global Parameters (8 bytes) - matches Unit-A encoder mapping
    // ========================================================================
    uint8_t effectId;      // Current effect low byte (bits [7:0])
    uint8_t brightness;    // Global brightness (0-255)
    uint8_t paletteId;     // Current palette index (0-63)
    uint8_t speed;         // Animation speed (1-100)
    uint8_t mood;          // Mood parameter (0-255)
    uint8_t fade;          // Fade amount (0-255)
    uint8_t complexity;    // Effect complexity (0-255)
    uint8_t variation;     // Effect variation (0-255)

    // ========================================================================
    // Zone State (18 bytes)
    // ========================================================================
    uint8_t zoneModeEnabled : 1;  // Zone Composer active
    uint8_t zoneCount : 3;        // Number of active zones (1-3)
    uint8_t reserved1 : 4;        // Reserved for future use

    ZonePresetConfig zones[4];    // 3 active zones + 1 reserved (4 × 4 bytes = 16 bytes for binary compat)

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
    // Reserved / overflow space (22 bytes)
    // Layout (v2+):
    //   [0]    Global effectId high byte (bits [15:8])
    //   [1..4] Zone[0..3] effectId high bytes (bits [15:8] per zone)
    //   [5..21] Available for future expansion
    // ========================================================================
    uint8_t reservedFuture[22];

    // ========================================================================
    // Checksum (2 bytes) — CRC16 of ALL preceding bytes (offset 0..61)
    // Placed at end so it covers reservedFuture, including effectId high bytes.
    // ========================================================================
    uint16_t checksum;

    // ========================================================================
    // Methods
    // ========================================================================

    // Initialise to empty/default state
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

    // Store / retrieve the full 16-bit global effect ID.
    // Low byte goes into effectId; high byte into reservedFuture[0].
    void setEffectId16(uint16_t effectId16) {
        effectId = static_cast<uint8_t>(effectId16 & 0xFF);
        reservedFuture[0] = static_cast<uint8_t>((effectId16 >> 8) & 0xFF);
    }

    uint16_t getEffectId16() const {
        return static_cast<uint16_t>(effectId) |
               (static_cast<uint16_t>(reservedFuture[0]) << 8);
    }

    // Store / retrieve the full 16-bit zone effect ID for zone index z (0-3).
    // Low byte goes into zones[z].effectIdLow; high byte into reservedFuture[1+z].
    void setZoneEffectId(uint8_t z, uint16_t effectId16) {
        if (z >= 4) return;
        zones[z].effectIdLow = static_cast<uint8_t>(effectId16 & 0xFF);
        reservedFuture[1 + z] = static_cast<uint8_t>((effectId16 >> 8) & 0xFF);
    }

    uint16_t getZoneEffectId(uint8_t z) const {
        if (z >= 4) return 0;
        return static_cast<uint16_t>(zones[z].effectIdLow) |
               (static_cast<uint16_t>(reservedFuture[1 + z]) << 8);
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

    // Calculate CRC16-CCITT checksum covering all bytes before the checksum
    // field (i.e. bytes 0 through offsetof(PresetData, checksum) - 1).
    // With checksum at the end of the struct this now covers reservedFuture,
    // protecting both the global and zone effectId high bytes.
    uint16_t calculateChecksum() const {
        uint16_t crc = 0xFFFF;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(this);

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
