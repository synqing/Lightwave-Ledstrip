/**
 * @file HubState.h
 * @brief HubState singleton for Tab5 Hub (authoritative desired state)
 *
 * Stores global parameters and per-node zone settings, and exposes dirty flags for 50ms batching.
 *
 * Notes:
 * - Avoids heap allocation by using fixed-size arrays indexed by nodeId (1..LW_MAX_NODES).
 * - Thread-safety: guarded by a FreeRTOS critical section (HubMain loop vs. encoder/UI tasks).
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

#include "../../common/proto/proto_constants.h"

class HubState {
public:
    static constexpr uint8_t MAX_ZONES = 4;

    struct GlobalParams {
        uint8_t effectId = 0;
        uint8_t brightness = 128;
        uint8_t speed = 25;
        uint8_t paletteId = 0;
        uint8_t hue = 0;
        uint8_t intensity = 128;
        uint8_t saturation = 255;
        uint8_t complexity = 128;
        uint8_t variation = 0;
    };

    struct ZoneSettings {
        uint8_t effectId = 0;
        uint8_t brightness = 128;
        uint8_t speed = 25;
        uint8_t paletteId = 0;
        uint8_t blendMode = 0;
    };

    struct GlobalDelta {
        // Bitmask for changed fields in GlobalParams (bit position documented in GlobalField).
        uint16_t dirtyMask = 0;
        GlobalParams values;
    };

    struct ZoneDelta {
        uint8_t nodeId = 0;
        uint8_t zoneId = 0;
        uint8_t dirtyMask = 0; // bitmask for ZoneField
        ZoneSettings values;
    };

    struct FullSnapshot {
        uint8_t nodeId = 0;
        GlobalParams global;
        ZoneSettings zones[MAX_ZONES];
    };

    enum GlobalField : uint16_t {
        GF_EFFECT      = 1u << 0,
        GF_BRIGHTNESS  = 1u << 1,
        GF_SPEED       = 1u << 2,
        GF_PALETTE     = 1u << 3,
        GF_HUE         = 1u << 4,
        GF_INTENSITY   = 1u << 5,
        GF_SATURATION  = 1u << 6,
        GF_COMPLEXITY  = 1u << 7,
        GF_VARIATION   = 1u << 8,
    };

    enum ZoneField : uint8_t {
        ZF_EFFECT      = 1u << 0,
        ZF_BRIGHTNESS  = 1u << 1,
        ZF_SPEED       = 1u << 2,
        ZF_PALETTE     = 1u << 3,
        ZF_BLEND       = 1u << 4,
    };

    static HubState& getInstance();

    void resetToDefaults();

    // Node lifecycle hooks
    void onNodeRegistered(uint8_t nodeId);

    // Zone mode state (hub authoritative). Zones are opt-in; keep disabled unless the user
    // explicitly touches zone controls.
    bool areZonesEnabled() const;
    void setZonesEnabled(bool enabled);

    // Global setters (mark dirty)
    void setGlobalEffect(uint8_t effectId);
    void setGlobalBrightness(uint8_t brightness);
    void setGlobalSpeed(uint8_t speed);
    void setGlobalPalette(uint8_t paletteId);
    void setGlobalHue(uint8_t hue);
    void setGlobalIntensity(uint8_t intensity);
    void setGlobalSaturation(uint8_t saturation);
    void setGlobalComplexity(uint8_t complexity);
    void setGlobalVariation(uint8_t variation);

    // Zone setters for all nodes (updates defaults + any initialised nodes)
    void setZoneEffectAll(uint8_t zoneId, uint8_t effectId);
    void setZoneBrightnessAll(uint8_t zoneId, uint8_t brightness);
    void setZoneSpeedAll(uint8_t zoneId, uint8_t speed);
    void setZonePaletteAll(uint8_t zoneId, uint8_t paletteId);
    void setZoneBlendAll(uint8_t zoneId, uint8_t blendMode);

    // Snapshot/delta creation (consumes dirty flags)
    bool hasDirty() const;
    GlobalDelta consumeGlobalDelta();
    uint8_t consumeZoneDeltas(ZoneDelta* out, uint8_t maxCount);
    FullSnapshot createFullSnapshot(uint8_t nodeId);

    // Snapshot helpers (do not consume dirty flags)
    GlobalParams getGlobalSnapshot() const;

private:
    HubState();

    void ensureNodeInitialised(uint8_t nodeId);
    static inline bool validNodeId(uint8_t nodeId) { return nodeId > 0 && nodeId <= LW_MAX_NODES; }
    static inline bool validZoneId(uint8_t zoneId) { return zoneId < MAX_ZONES; }

    mutable portMUX_TYPE mux_;

    GlobalParams global_;
    uint16_t globalDirtyMask_;

    // Per-node zone settings (indexed by nodeId, zoneId)
    ZoneSettings perNodeZones_[LW_MAX_NODES + 1][MAX_ZONES];
    uint8_t perNodeZoneDirtyMask_[LW_MAX_NODES + 1][MAX_ZONES];
    bool nodeInitialised_[LW_MAX_NODES + 1];

    // Default zone settings applied when a node first registers
    ZoneSettings defaultZones_[MAX_ZONES];

    bool zonesEnabled_ = false;
};
