// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// PresetManager - Core Preset Logic Implementation
// ============================================================================

#include "PresetManager.h"
#include "../parameters/ParameterHandler.h"
#include "../parameters/ParameterMap.h"
#include "../network/WebSocketClient.h"
#include "../ui/ZoneComposerUI.h"

// ============================================================================
// Constructor
// ============================================================================

PresetManager::PresetManager(ParameterHandler* paramHandler, WebSocketClient* wsClient)
    : _paramHandler(paramHandler)
    , _wsClient(wsClient)
    , _feedbackCallback(nullptr)
    , _cachedOccupancy(0)
    , _occupancyCacheValid(false)
{
}

// ============================================================================
// Initialization
// ============================================================================

bool PresetManager::init() {
    if (!PresetStorage::init()) {
        Serial.println("[PresetManager] Storage init failed");
        return false;
    }

    // Cache initial occupancy
    updateOccupancyCache();

    Serial.printf("[PresetManager] Initialized, %d presets stored\n", getOccupiedCount());
    return true;
}

// ============================================================================
// Preset Operations
// ============================================================================

bool PresetManager::savePreset(uint8_t slot) {
    if (slot >= PRESET_SLOT_COUNT) {
        Serial.printf("[PresetManager] Save failed: invalid slot %d\n", slot);
        sendFeedback(slot, PresetAction::ERROR, false);
        return false;
    }

    // Capture current state
    PresetData preset;
    captureCurrentState(preset);

    // Save to storage
    bool success = PresetStorage::save(slot, preset);

    if (success) {
        _occupancyCacheValid = false;  // Invalidate cache
        Serial.printf("[PresetManager] Saved preset to slot %d\n", slot);
    } else {
        Serial.printf("[PresetManager] Failed to save preset to slot %d\n", slot);
    }

    sendFeedback(slot, PresetAction::SAVE, success);
    return success;
}

bool PresetManager::recallPreset(uint8_t slot) {
    if (slot >= PRESET_SLOT_COUNT) {
        Serial.printf("[PresetManager] Recall failed: invalid slot %d\n", slot);
        sendFeedback(slot, PresetAction::ERROR, false);
        return false;
    }

    // Load from storage
    PresetData preset;
    if (!PresetStorage::load(slot, preset)) {
        Serial.printf("[PresetManager] Recall failed: slot %d empty or invalid\n", slot);
        sendFeedback(slot, PresetAction::RECALL, false);
        return false;
    }

    // Apply to device
    bool success = applyPresetState(preset);

    if (success) {
        Serial.printf("[PresetManager] Recalled preset from slot %d\n", slot);
    } else {
        Serial.printf("[PresetManager] Failed to apply preset from slot %d\n", slot);
    }

    sendFeedback(slot, PresetAction::RECALL, success);
    return success;
}

bool PresetManager::deletePreset(uint8_t slot) {
    if (slot >= PRESET_SLOT_COUNT) {
        Serial.printf("[PresetManager] Delete failed: invalid slot %d\n", slot);
        sendFeedback(slot, PresetAction::ERROR, false);
        return false;
    }

    bool success = PresetStorage::clear(slot);

    if (success) {
        _occupancyCacheValid = false;  // Invalidate cache
        Serial.printf("[PresetManager] Deleted preset from slot %d\n", slot);
    } else {
        Serial.printf("[PresetManager] Failed to delete preset from slot %d\n", slot);
    }

    sendFeedback(slot, PresetAction::DELETE, success);
    return success;
}

// ============================================================================
// State Query
// ============================================================================

bool PresetManager::isSlotOccupied(uint8_t slot) const {
    if (slot >= PRESET_SLOT_COUNT) return false;

    if (!_occupancyCacheValid) {
        updateOccupancyCache();
    }

    return (_cachedOccupancy & (1 << slot)) != 0;
}

uint8_t PresetManager::getOccupancyMask() const {
    if (!_occupancyCacheValid) {
        updateOccupancyCache();
    }
    return _cachedOccupancy;
}

uint8_t PresetManager::getOccupiedCount() const {
    uint8_t mask = getOccupancyMask();
    uint8_t count = 0;

    while (mask) {
        count += (mask & 1);
        mask >>= 1;
    }

    return count;
}

bool PresetManager::getPreset(uint8_t slot, PresetData& preset) const {
    if (slot >= PRESET_SLOT_COUNT) return false;
    return PresetStorage::load(slot, preset);
}

// ============================================================================
// State Capture
// ============================================================================

void PresetManager::captureCurrentState(PresetData& preset) {
    // Initialize preset
    preset.clear();
    preset.magic = PresetData::MAGIC;
    preset.version = PresetData::CURRENT_VERSION;

    // Capture global parameters from ParameterHandler
    if (_paramHandler) {
        uint8_t values[PARAMETER_COUNT];
        _paramHandler->getAllValues(values);

        // Unit A parameters (0-7)
        preset.effectId = values[static_cast<uint8_t>(ParameterId::EffectId)];
        preset.brightness = values[static_cast<uint8_t>(ParameterId::Brightness)];
        preset.paletteId = values[static_cast<uint8_t>(ParameterId::PaletteId)];
        preset.speed = values[static_cast<uint8_t>(ParameterId::Speed)];
        preset.mood = values[static_cast<uint8_t>(ParameterId::Mood)];
        preset.fade = values[static_cast<uint8_t>(ParameterId::FadeAmount)];
        preset.complexity = values[static_cast<uint8_t>(ParameterId::Complexity)];
        preset.variation = values[static_cast<uint8_t>(ParameterId::Variation)];

        // Unit B zone parameters from encoder values (fallback)
        for (int z = 0; z < 4; z++) {
            uint8_t effectIdx = 8 + (z * 2);   // Zone effect indices: 8, 10, 12, 14
            uint8_t speedIdx = 9 + (z * 2);    // Zone speed indices: 9, 11, 13, 15

            preset.zones[z].effectId = values[effectIdx];
            preset.zones[z].speed = values[speedIdx];
            preset.zones[z].brightness = 255;  // Default
            preset.zones[z].enabled = true;
            preset.zones[z].paletteId = 0;
        }
    }

    // Zone mode state from ZoneComposerUI (authoritative source)
    if (_zoneUI) {
        preset.zoneModeEnabled = _zoneUI->isZoneModeEnabled();
        preset.zoneCount = _zoneUI->getZoneCount();

        // Override zone configs with actual state from UI
        for (uint8_t z = 0; z < 4; z++) {
            const ZoneState& zs = _zoneUI->getZoneState(z);
            preset.zones[z].effectId = zs.effectId;
            preset.zones[z].speed = zs.speed;
            preset.zones[z].paletteId = zs.paletteId;
            preset.zones[z].enabled = zs.enabled;
            // Note: ZoneState doesn't have brightness, use LED count as proxy or default
            preset.zones[z].brightness = 255;
        }
    } else {
        // Fallback if ZoneComposerUI not set
        preset.zoneModeEnabled = false;
        preset.zoneCount = 1;
    }

    // Color correction from cached WebSocket state
    if (_wsClient) {
        const ColorCorrectionState& cc = _wsClient->getColorCorrectionState();
        if (cc.valid) {
            // Store gamma as uint8 (value * 10, so 2.2 = 22)
            preset.gamma = cc.gammaEnabled ? static_cast<uint8_t>(cc.gammaValue * 10.0f) : 0;
            preset.brownGuardrail = cc.brownGuardrailEnabled;
            preset.autoExposure = cc.autoExposureEnabled;
        } else {
            // Fallback defaults if not yet synced
            preset.gamma = 22;
            preset.brownGuardrail = false;
            preset.autoExposure = false;
            Serial.println("[PresetManager] Warning: ColorCorrection not synced, using defaults");
        }
    } else {
        preset.gamma = 22;
        preset.brownGuardrail = false;
        preset.autoExposure = false;
    }

    // Log complete captured state
    Serial.printf("[PresetManager] Captured: E=%d B=%d P=%d S=%d M=%d F=%d C=%d V=%d\n",
                  preset.effectId, preset.brightness, preset.paletteId, preset.speed,
                  preset.mood, preset.fade, preset.complexity, preset.variation);
    Serial.printf("[PresetManager]   Zones: enabled=%d count=%d gamma=%d ae=%d brown=%d\n",
                  preset.zoneModeEnabled, preset.zoneCount, preset.gamma,
                  preset.autoExposure, preset.brownGuardrail);
    if (preset.zoneModeEnabled && preset.zoneCount > 0) {
        for (uint8_t z = 0; z < preset.zoneCount && z < 4; z++) {
            Serial.printf("[PresetManager]   Zone%d: E=%d S=%d P=%d en=%d\n",
                          z, preset.zones[z].effectId, preset.zones[z].speed,
                          preset.zones[z].paletteId, preset.zones[z].enabled);
        }
    }
}

// ============================================================================
// State Application
// ============================================================================

bool PresetManager::applyPresetState(const PresetData& preset) {
    if (!_wsClient) {
        Serial.println("[PresetManager] Cannot apply: no WebSocket client");
        return false;
    }

    if (!_wsClient->isConnected()) {
        Serial.println("[PresetManager] Cannot apply: WebSocket not connected");
        return false;
    }

    // Apply global parameters
    _wsClient->sendEffectChange(preset.effectId);
    _wsClient->sendBrightnessChange(preset.brightness);
    _wsClient->sendPaletteChange(preset.paletteId);
    _wsClient->sendSpeedChange(preset.speed);
    _wsClient->sendMoodChange(preset.mood);
    _wsClient->sendFadeAmountChange(preset.fade);
    _wsClient->sendComplexityChange(preset.complexity);
    _wsClient->sendVariationChange(preset.variation);

    // Apply zone state if zone mode enabled
    if (preset.zoneModeEnabled) {
        _wsClient->sendZoneEnable(true);

        for (uint8_t z = 0; z < preset.zoneCount && z < 4; z++) {
            if (preset.zones[z].enabled) {
                _wsClient->sendZoneEffect(z, preset.zones[z].effectId);
                _wsClient->sendZoneSpeed(z, preset.zones[z].speed);
                _wsClient->sendZoneBrightness(z, preset.zones[z].brightness);
                _wsClient->sendZonePalette(z, preset.zones[z].paletteId);
            }
        }
    } else {
        _wsClient->sendZoneEnable(false);
    }

    // Apply color correction settings
    bool gammaEnabled = (preset.gamma > 0);
    float gammaValue = gammaEnabled ? (preset.gamma / 10.0f) : 2.2f;
    
    // Get current mode from WebSocket client state (presets don't store mode)
    uint8_t currentMode = _wsClient->getColorCorrectionState().mode;

    _wsClient->sendColorCorrectionConfig(
        gammaEnabled,
        gammaValue,
        preset.autoExposure,
        110,  // Default auto-exposure target
        preset.brownGuardrail,
        currentMode  // Include current mode - server requires all fields in setConfig
    );

    Serial.printf("[PresetManager] Applied color correction: gamma=%s (%.1f), ae=%s, brown=%s\n",
                  gammaEnabled ? "ON" : "OFF", gammaValue,
                  preset.autoExposure ? "ON" : "OFF",
                  preset.brownGuardrail ? "ON" : "OFF");

    // Update local ParameterHandler state to match
    if (_paramHandler) {
        _paramHandler->setValue(ParameterId::EffectId, preset.effectId);
        _paramHandler->setValue(ParameterId::Brightness, preset.brightness);
        _paramHandler->setValue(ParameterId::PaletteId, preset.paletteId);
        _paramHandler->setValue(ParameterId::Speed, preset.speed);
        _paramHandler->setValue(ParameterId::Mood, preset.mood);
        _paramHandler->setValue(ParameterId::FadeAmount, preset.fade);
        _paramHandler->setValue(ParameterId::Complexity, preset.complexity);
        _paramHandler->setValue(ParameterId::Variation, preset.variation);
    }

    // Log complete applied state
    Serial.printf("[PresetManager] Applied: E=%d B=%d P=%d S=%d M=%d F=%d C=%d V=%d\n",
                  preset.effectId, preset.brightness, preset.paletteId, preset.speed,
                  preset.mood, preset.fade, preset.complexity, preset.variation);
    Serial.printf("[PresetManager]   Zones: enabled=%d count=%d\n",
                  preset.zoneModeEnabled, preset.zoneCount);

    return true;
}

// ============================================================================
// Internal Helpers
// ============================================================================

void PresetManager::updateOccupancyCache() const {
    _cachedOccupancy = PresetStorage::getOccupancyMask();
    _occupancyCacheValid = true;
}

void PresetManager::sendFeedback(uint8_t slot, PresetAction action, bool success) {
    if (_feedbackCallback) {
        _feedbackCallback(slot, action, success);
    }
}
