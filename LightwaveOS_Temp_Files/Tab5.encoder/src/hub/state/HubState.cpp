/**
 * HubState singleton implementation
 */

#include "HubState.h"

HubState& HubState::getInstance() {
    static HubState instance;
    return instance;
}

HubState::HubState()
    : mux_(portMUX_INITIALIZER_UNLOCKED),
      global_(),
      globalDirtyMask_(0) {
    memset(perNodeZones_, 0, sizeof(perNodeZones_));
    memset(perNodeZoneDirtyMask_, 0, sizeof(perNodeZoneDirtyMask_));
    memset(nodeInitialised_, 0, sizeof(nodeInitialised_));
    resetToDefaults();
}

void HubState::resetToDefaults() {
    portENTER_CRITICAL(&mux_);

    global_ = GlobalParams{};
    globalDirtyMask_ = 0;
    zonesEnabled_ = false;

    // Default zones inherit basic defaults.
    for (uint8_t z = 0; z < MAX_ZONES; ++z) {
        defaultZones_[z] = ZoneSettings{};
    }

    // Clear per-node state (kept uninitialised until a node registers).
    memset(perNodeZones_, 0, sizeof(perNodeZones_));
    memset(perNodeZoneDirtyMask_, 0, sizeof(perNodeZoneDirtyMask_));
    memset(nodeInitialised_, 0, sizeof(nodeInitialised_));

    portEXIT_CRITICAL(&mux_);
}

void HubState::ensureNodeInitialised(uint8_t nodeId) {
    if (!validNodeId(nodeId)) return;
    if (nodeInitialised_[nodeId]) return;

    for (uint8_t z = 0; z < MAX_ZONES; ++z) {
        perNodeZones_[nodeId][z] = defaultZones_[z];
        perNodeZoneDirtyMask_[nodeId][z] = 0;
    }
    nodeInitialised_[nodeId] = true;
}

void HubState::onNodeRegistered(uint8_t nodeId) {
    if (!validNodeId(nodeId)) return;
    portENTER_CRITICAL(&mux_);
    ensureNodeInitialised(nodeId);
    portEXIT_CRITICAL(&mux_);
}

bool HubState::areZonesEnabled() const {
    portENTER_CRITICAL((portMUX_TYPE*)&mux_);
    const bool enabled = zonesEnabled_;
    portEXIT_CRITICAL((portMUX_TYPE*)&mux_);
    return enabled;
}

void HubState::setZonesEnabled(bool enabled) {
    portENTER_CRITICAL(&mux_);
    zonesEnabled_ = enabled;
    if (!zonesEnabled_) {
        // Ensure we stop emitting zone deltas immediately.
        memset(perNodeZoneDirtyMask_, 0, sizeof(perNodeZoneDirtyMask_));
    }
    portEXIT_CRITICAL(&mux_);
}

bool HubState::hasDirty() const {
    portENTER_CRITICAL((portMUX_TYPE*)&mux_);
    bool dirty = (globalDirtyMask_ != 0);
    if (!dirty) {
        for (uint8_t n = 1; n <= LW_MAX_NODES && !dirty; ++n) {
            if (!nodeInitialised_[n]) continue;
            for (uint8_t z = 0; z < MAX_ZONES; ++z) {
                if (perNodeZoneDirtyMask_[n][z] != 0) { dirty = true; break; }
            }
        }
    }
    portEXIT_CRITICAL((portMUX_TYPE*)&mux_);
    return dirty;
}

HubState::GlobalDelta HubState::consumeGlobalDelta() {
    GlobalDelta delta;
    portENTER_CRITICAL(&mux_);
    delta.dirtyMask = globalDirtyMask_;
    delta.values = global_;
    globalDirtyMask_ = 0;
    portEXIT_CRITICAL(&mux_);
    return delta;
}

uint8_t HubState::consumeZoneDeltas(ZoneDelta* out, uint8_t maxCount) {
    if (!out || maxCount == 0) return 0;

    uint8_t written = 0;
    portENTER_CRITICAL(&mux_);

    for (uint8_t n = 1; n <= LW_MAX_NODES && written < maxCount; ++n) {
        if (!nodeInitialised_[n]) continue;
        for (uint8_t z = 0; z < MAX_ZONES && written < maxCount; ++z) {
            uint8_t mask = perNodeZoneDirtyMask_[n][z];
            if (mask == 0) continue;
            out[written].nodeId = n;
            out[written].zoneId = z;
            out[written].dirtyMask = mask;
            out[written].values = perNodeZones_[n][z];
            perNodeZoneDirtyMask_[n][z] = 0;
            written++;
        }
    }

    portEXIT_CRITICAL(&mux_);
    return written;
}

HubState::FullSnapshot HubState::createFullSnapshot(uint8_t nodeId) {
    FullSnapshot snap;
    snap.nodeId = nodeId;

    portENTER_CRITICAL(&mux_);
    if (validNodeId(nodeId)) {
        ensureNodeInitialised(nodeId);
    }

    snap.global = global_;

    if (validNodeId(nodeId) && nodeInitialised_[nodeId]) {
        for (uint8_t z = 0; z < MAX_ZONES; ++z) {
            snap.zones[z] = perNodeZones_[nodeId][z];
        }
    } else {
        for (uint8_t z = 0; z < MAX_ZONES; ++z) {
            snap.zones[z] = defaultZones_[z];
        }
    }
    portEXIT_CRITICAL(&mux_);

    return snap;
}

HubState::GlobalParams HubState::getGlobalSnapshot() const {
    GlobalParams snap;
    portENTER_CRITICAL((portMUX_TYPE*)&mux_);
    snap = global_;
    portEXIT_CRITICAL((portMUX_TYPE*)&mux_);
    return snap;
}

// ===== Global setters =====

void HubState::setGlobalEffect(uint8_t effectId) {
    portENTER_CRITICAL(&mux_);
    if (global_.effectId != effectId) {
        global_.effectId = effectId;
        globalDirtyMask_ |= GF_EFFECT;
    }
    portEXIT_CRITICAL(&mux_);
}

void HubState::setGlobalBrightness(uint8_t brightness) {
    portENTER_CRITICAL(&mux_);
    if (global_.brightness != brightness) {
        global_.brightness = brightness;
        globalDirtyMask_ |= GF_BRIGHTNESS;
    }
    portEXIT_CRITICAL(&mux_);
}

void HubState::setGlobalSpeed(uint8_t speed) {
    portENTER_CRITICAL(&mux_);
    if (global_.speed != speed) {
        global_.speed = speed;
        globalDirtyMask_ |= GF_SPEED;
    }
    portEXIT_CRITICAL(&mux_);
}

void HubState::setGlobalPalette(uint8_t paletteId) {
    portENTER_CRITICAL(&mux_);
    if (global_.paletteId != paletteId) {
        global_.paletteId = paletteId;
        globalDirtyMask_ |= GF_PALETTE;
    }
    portEXIT_CRITICAL(&mux_);
}

void HubState::setGlobalHue(uint8_t hue) {
    portENTER_CRITICAL(&mux_);
    if (global_.hue != hue) {
        global_.hue = hue;
        globalDirtyMask_ |= GF_HUE;
    }
    portEXIT_CRITICAL(&mux_);
}

void HubState::setGlobalIntensity(uint8_t intensity) {
    portENTER_CRITICAL(&mux_);
    if (global_.intensity != intensity) {
        global_.intensity = intensity;
        globalDirtyMask_ |= GF_INTENSITY;
    }
    portEXIT_CRITICAL(&mux_);
}

void HubState::setGlobalSaturation(uint8_t saturation) {
    portENTER_CRITICAL(&mux_);
    if (global_.saturation != saturation) {
        global_.saturation = saturation;
        globalDirtyMask_ |= GF_SATURATION;
    }
    portEXIT_CRITICAL(&mux_);
}

void HubState::setGlobalComplexity(uint8_t complexity) {
    portENTER_CRITICAL(&mux_);
    if (global_.complexity != complexity) {
        global_.complexity = complexity;
        globalDirtyMask_ |= GF_COMPLEXITY;
    }
    portEXIT_CRITICAL(&mux_);
}

void HubState::setGlobalVariation(uint8_t variation) {
    portENTER_CRITICAL(&mux_);
    if (global_.variation != variation) {
        global_.variation = variation;
        globalDirtyMask_ |= GF_VARIATION;
    }
    portEXIT_CRITICAL(&mux_);
}

// ===== Zone setters (apply to defaults + all initialised nodes) =====

void HubState::setZoneEffectAll(uint8_t zoneId, uint8_t effectId) {
    if (!validZoneId(zoneId)) return;
    portENTER_CRITICAL(&mux_);
    zonesEnabled_ = true;
    defaultZones_[zoneId].effectId = effectId;
    for (uint8_t n = 1; n <= LW_MAX_NODES; ++n) {
        if (!nodeInitialised_[n]) continue;
        if (perNodeZones_[n][zoneId].effectId != effectId) {
            perNodeZones_[n][zoneId].effectId = effectId;
            perNodeZoneDirtyMask_[n][zoneId] |= ZF_EFFECT;
        }
    }
    portEXIT_CRITICAL(&mux_);
}

void HubState::setZoneBrightnessAll(uint8_t zoneId, uint8_t brightness) {
    if (!validZoneId(zoneId)) return;
    portENTER_CRITICAL(&mux_);
    zonesEnabled_ = true;
    defaultZones_[zoneId].brightness = brightness;
    for (uint8_t n = 1; n <= LW_MAX_NODES; ++n) {
        if (!nodeInitialised_[n]) continue;
        if (perNodeZones_[n][zoneId].brightness != brightness) {
            perNodeZones_[n][zoneId].brightness = brightness;
            perNodeZoneDirtyMask_[n][zoneId] |= ZF_BRIGHTNESS;
        }
    }
    portEXIT_CRITICAL(&mux_);
}

void HubState::setZoneSpeedAll(uint8_t zoneId, uint8_t speed) {
    if (!validZoneId(zoneId)) return;
    portENTER_CRITICAL(&mux_);
    zonesEnabled_ = true;
    defaultZones_[zoneId].speed = speed;
    for (uint8_t n = 1; n <= LW_MAX_NODES; ++n) {
        if (!nodeInitialised_[n]) continue;
        if (perNodeZones_[n][zoneId].speed != speed) {
            perNodeZones_[n][zoneId].speed = speed;
            perNodeZoneDirtyMask_[n][zoneId] |= ZF_SPEED;
        }
    }
    portEXIT_CRITICAL(&mux_);
}

void HubState::setZonePaletteAll(uint8_t zoneId, uint8_t paletteId) {
    if (!validZoneId(zoneId)) return;
    portENTER_CRITICAL(&mux_);
    zonesEnabled_ = true;
    defaultZones_[zoneId].paletteId = paletteId;
    for (uint8_t n = 1; n <= LW_MAX_NODES; ++n) {
        if (!nodeInitialised_[n]) continue;
        if (perNodeZones_[n][zoneId].paletteId != paletteId) {
            perNodeZones_[n][zoneId].paletteId = paletteId;
            perNodeZoneDirtyMask_[n][zoneId] |= ZF_PALETTE;
        }
    }
    portEXIT_CRITICAL(&mux_);
}

void HubState::setZoneBlendAll(uint8_t zoneId, uint8_t blendMode) {
    if (!validZoneId(zoneId)) return;
    portENTER_CRITICAL(&mux_);
    zonesEnabled_ = true;
    defaultZones_[zoneId].blendMode = blendMode;
    for (uint8_t n = 1; n <= LW_MAX_NODES; ++n) {
        if (!nodeInitialised_[n]) continue;
        if (perNodeZones_[n][zoneId].blendMode != blendMode) {
            perNodeZones_[n][zoneId].blendMode = blendMode;
            perNodeZoneDirtyMask_[n][zoneId] |= ZF_BLEND;
        }
    }
    portEXIT_CRITICAL(&mux_);
}
