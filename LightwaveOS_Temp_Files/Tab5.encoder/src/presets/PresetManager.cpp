// ============================================================================
// PresetManager - Core Preset Logic Implementation
// ============================================================================

#include "PresetManager.h"
#include "../config/AnsiColors.h"
#include "../hub/state/HubState.h"
#include "../parameters/ParameterHandler.h"
#include "../parameters/ParameterMap.h"

// ============================================================================
// Constructor
// ============================================================================

PresetManager::PresetManager(ParameterHandler* paramHandler, HubState* hubState)
    : _paramHandler(paramHandler)
    , _hubState(hubState)
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
        LOG_PRESET("Storage init failed");
        return false;
    }

    // Cache initial occupancy
    updateOccupancyCache();

    LOG_PRESET("Initialised, %d presets stored", getOccupiedCount());
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
        LOG_PRESET("Saved preset to slot %d", slot);
    } else {
        LOG_PRESET("Failed to save preset to slot %d", slot);
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
        LOG_PRESET("Recall failed: slot %d empty or invalid", slot);
        sendFeedback(slot, PresetAction::RECALL, false);
        return false;
    }

    // Apply to device
    bool success = applyPresetState(preset);

    if (success) {
        LOG_PRESET("Recalled preset from slot %d", slot);
    } else {
        LOG_PRESET("Failed to apply preset from slot %d", slot);
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
        LOG_PRESET("Deleted preset from slot %d", slot);
    } else {
        LOG_PRESET("Failed to delete preset from slot %d", slot);
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

    // Capture from HubState when available (authoritative desired state).
    // Fallback to encoder/ParameterHandler cache if HubState is not wired.
    if (_hubState) {
        HubState::FullSnapshot snap = _hubState->createFullSnapshot(0);  // nodeId=0 -> defaults

        preset.effectId = snap.global.effectId;
        preset.brightness = snap.global.brightness;
        preset.paletteId = snap.global.paletteId;
        preset.speed = snap.global.speed;

        // Tab5 legacy labels: Mood/Fade map onto modern intensity/saturation.
        preset.mood = snap.global.intensity;
        preset.fade = snap.global.saturation;
        preset.complexity = snap.global.complexity;
        preset.variation = snap.global.variation;

        // Zones
        preset.zoneModeEnabled = _hubState->areZonesEnabled() ? 1 : 0;
        preset.zoneCount = 4;
        for (uint8_t z = 0; z < 4; ++z) {
            preset.zones[z].effectId = snap.zones[z].effectId;
            preset.zones[z].speed = snap.zones[z].speed;
            preset.zones[z].brightness = snap.zones[z].brightness;
            preset.zones[z].enabled = 1;
            preset.zones[z].paletteId = snap.zones[z].paletteId;
        }
    } else if (_paramHandler) {
        uint8_t values[PARAMETER_COUNT];
        _paramHandler->getAllValues(values);

        preset.effectId = values[static_cast<uint8_t>(ParameterId::EffectId)];
        preset.paletteId = values[static_cast<uint8_t>(ParameterId::PaletteId)];
        preset.speed = values[static_cast<uint8_t>(ParameterId::Speed)];
        preset.mood = values[static_cast<uint8_t>(ParameterId::Mood)];
        preset.fade = values[static_cast<uint8_t>(ParameterId::FadeAmount)];
        preset.complexity = values[static_cast<uint8_t>(ParameterId::Complexity)];
        preset.variation = values[static_cast<uint8_t>(ParameterId::Variation)];
        preset.brightness = values[static_cast<uint8_t>(ParameterId::Brightness)];

        // Without HubState, we cannot know whether zone mode is active. Default to OFF
        // so saving a preset does not implicitly force zones on at recall.
        preset.zoneModeEnabled = 0;
        preset.zoneCount = 4;
        for (uint8_t z = 0; z < 4; ++z) {
            const uint8_t effectIdx = 8 + (z * 2);
            const uint8_t speedIdx = 9 + (z * 2);
            preset.zones[z].effectId = values[effectIdx];
            preset.zones[z].speed = values[speedIdx];
            preset.zones[z].brightness = 255;
            preset.zones[z].enabled = 1;
            preset.zones[z].paletteId = 0;
        }
    }

    // Colour correction is not yet owned by HubState in hub mode; persist defaults.
    preset.gamma = 22;
    preset.brownGuardrail = 0;
    preset.autoExposure = 0;

    // Log complete captured state
    LOG_PRESET("Captured: E=%u B=%u P=%u S=%u M=%u F=%u C=%u V=%u",
               (unsigned)preset.effectId, (unsigned)preset.brightness, (unsigned)preset.paletteId, (unsigned)preset.speed,
               (unsigned)preset.mood, (unsigned)preset.fade, (unsigned)preset.complexity, (unsigned)preset.variation);
}

// ============================================================================
// State Application
// ============================================================================

bool PresetManager::applyPresetState(const PresetData& preset) {
    if (!preset.isValid()) {
        LOG_PRESET("applyPresetState: invalid preset");
        return false;
    }

    if (!_hubState) {
        LOG_PRESET("applyPresetState: HubState not wired");
        return false;
    }

    // Apply global state (HubState is authoritative; HubMain handles batching + applyAt).
    _hubState->setGlobalEffect(preset.effectId);
    _hubState->setGlobalPalette(preset.paletteId);
    _hubState->setGlobalSpeed(preset.speed);
    _hubState->setGlobalBrightness(preset.brightness);
    _hubState->setGlobalIntensity(preset.mood);
    _hubState->setGlobalSaturation(preset.fade);
    _hubState->setGlobalComplexity(preset.complexity);
    _hubState->setGlobalVariation(preset.variation);

    // Apply zones only when zone mode is enabled in the preset.
    _hubState->setZonesEnabled(preset.zoneModeEnabled != 0);
    if (preset.zoneModeEnabled) {
        for (uint8_t z = 0; z < 4; ++z) {
            if (!preset.zones[z].enabled) continue;
            _hubState->setZoneEffectAll(z, preset.zones[z].effectId);
            _hubState->setZoneSpeedAll(z, preset.zones[z].speed);
            _hubState->setZoneBrightnessAll(z, preset.zones[z].brightness);
            _hubState->setZonePaletteAll(z, preset.zones[z].paletteId);
        }
    }

    // Sync local encoder/UI values so the knobs match the recalled preset.
    if (_paramHandler) {
        _paramHandler->applyLocalValue(static_cast<uint8_t>(ParameterId::EffectId), preset.effectId, true);
        _paramHandler->applyLocalValue(static_cast<uint8_t>(ParameterId::PaletteId), preset.paletteId, true);
        _paramHandler->applyLocalValue(static_cast<uint8_t>(ParameterId::Speed), preset.speed, true);
        _paramHandler->applyLocalValue(static_cast<uint8_t>(ParameterId::Mood), preset.mood, true);
        _paramHandler->applyLocalValue(static_cast<uint8_t>(ParameterId::FadeAmount), preset.fade, true);
        _paramHandler->applyLocalValue(static_cast<uint8_t>(ParameterId::Complexity), preset.complexity, true);
        _paramHandler->applyLocalValue(static_cast<uint8_t>(ParameterId::Variation), preset.variation, true);
        _paramHandler->applyLocalValue(static_cast<uint8_t>(ParameterId::Brightness), preset.brightness, true);

        // Zone encoders
        for (uint8_t z = 0; z < 4; ++z) {
            const uint8_t effectIdx = 8 + (z * 2);
            const uint8_t speedIdx = 9 + (z * 2);
            _paramHandler->applyLocalValue(effectIdx, preset.zones[z].effectId, true);
            _paramHandler->applyLocalValue(speedIdx, preset.zones[z].speed, true);
        }
    }

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
