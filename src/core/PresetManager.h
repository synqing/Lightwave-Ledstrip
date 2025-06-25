#ifndef PRESET_MANAGER_H
#define PRESET_MANAGER_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "../config/hardware_config.h"
#include "../core/EffectTypes.h"

#define MAX_PRESETS 16
#define PRESET_NAME_LENGTH 16
#define PRESET_FILE_PATH "/presets/"

struct Preset {
    // Metadata
    char name[PRESET_NAME_LENGTH];
    uint32_t timestamp;
    uint8_t tags;
    
    // Effect settings
    uint8_t effectId;
    uint8_t paletteIndex;
    uint8_t brightness;
    uint8_t fadeAmount;
    uint8_t speed;
    
    // Visual parameters
    uint8_t intensity;
    uint8_t saturation;
    uint8_t complexity;
    uint8_t variation;
    
    // Sync settings
    HardwareConfig::SyncMode syncMode;
    HardwareConfig::PropagationMode propagationMode;
    
    // Transition preferences
    bool useRandomTransitions;
    
    // Checksum for validation
    uint16_t checksum;
    
    void calculateChecksum() {
        checksum = 0;
        uint8_t* data = (uint8_t*)this;
        for (size_t i = 0; i < sizeof(Preset) - sizeof(checksum); i++) {
            checksum += data[i];
        }
    }
    
    bool isValid() const {
        uint16_t calc = 0;
        uint8_t* data = (uint8_t*)this;
        for (size_t i = 0; i < sizeof(Preset) - sizeof(checksum); i++) {
            calc += data[i];
        }
        return calc == checksum;
    }
};

class PresetManager {
private:
    Preset m_presets[MAX_PRESETS];
    bool m_presetValid[MAX_PRESETS];
    uint8_t m_currentPreset = 0;
    
    // Morphing state
    bool m_morphing = false;
    Preset m_morphSource;
    Preset m_morphTarget;
    float m_morphProgress = 0.0f;
    uint32_t m_morphStartTime = 0;
    uint32_t m_morphDuration = 2000; // 2 seconds default
    
    // Sequence playback
    uint8_t m_sequence[MAX_PRESETS];
    uint8_t m_sequenceLength = 0;
    uint8_t m_sequenceIndex = 0;
    bool m_sequencePlaying = false;
    uint32_t m_sequenceTimer = 0;
    uint32_t m_sequenceDuration = 10000; // 10 seconds per preset
    
public:
    PresetManager() {
        memset(m_presetValid, 0, sizeof(m_presetValid));
    }
    
    bool begin() {
        if (!SPIFFS.begin(true)) {
            Serial.println("❌ Failed to mount SPIFFS");
            return false;
        }
        
        // Create presets directory if it doesn't exist
        if (!SPIFFS.exists(PRESET_FILE_PATH)) {
            SPIFFS.mkdir(PRESET_FILE_PATH);
        }
        
        // Load all presets from SPIFFS
        loadAllPresets();
        
        Serial.println("✅ Preset Manager initialized");
        return true;
    }
    
    bool saveCurrentState(uint8_t slot, const char* name = nullptr);
    bool loadPreset(uint8_t slot, bool immediate = true);
    void startMorph(uint8_t targetSlot, uint32_t duration = 2000);
    void updateMorph();
    
    void startSequence(uint8_t* presets, uint8_t length, uint32_t duration);
    void updateSequence();
    void stopSequence();
    
    // Quick access methods
    void quickSave(uint8_t slot);
    void quickLoad(uint8_t slot);
    
    // Encoder integration
    void handleEncoderInput(uint8_t encoderId, int32_t delta);
    void handleButtonPress(uint8_t encoderId);
    
    // Getters
    bool isMorphing() const { return m_morphing; }
    float getMorphProgress() const { return m_morphProgress; }
    uint8_t getCurrentPreset() const { return m_currentPreset; }
    bool isPresetValid(uint8_t slot) const { 
        return slot < MAX_PRESETS && m_presetValid[slot]; 
    }
    const char* getPresetName(uint8_t slot) const {
        if (isPresetValid(slot)) {
            return m_presets[slot].name;
        }
        return nullptr;
    }
    
private:
    void applyPreset(const Preset& preset);
    void interpolatePresets(const Preset& from, const Preset& to, Preset& result, float t);
    bool savePresetToFile(uint8_t slot);
    bool loadPresetFromFile(uint8_t slot);
    void loadAllPresets();
    uint8_t findPresetIndex(const Preset& preset);
    
    // Smooth interpolation helper
    float smoothStep(float t) {
        return t * t * (3.0f - 2.0f * t);
    }
};

// Implementation of key methods
inline bool PresetManager::saveCurrentState(uint8_t slot, const char* name) {
    if (slot >= MAX_PRESETS) return false;
    
    Preset& preset = m_presets[slot];
    
    // Save metadata
    if (name) {
        strncpy(preset.name, name, PRESET_NAME_LENGTH - 1);
        preset.name[PRESET_NAME_LENGTH - 1] = '\0';
    } else {
        snprintf(preset.name, PRESET_NAME_LENGTH, "Preset %02d", slot);
    }
    preset.timestamp = millis();
    preset.tags = 0;
    
    // Save current effect state - need extern references
    extern uint8_t currentEffect;
    extern uint8_t currentPaletteIndex;
    extern uint8_t fadeAmount;
    extern uint8_t paletteSpeed;
    extern VisualParams visualParams;
    extern HardwareConfig::SyncMode currentSyncMode;
    extern HardwareConfig::PropagationMode currentPropagationMode;
    extern bool useRandomTransitions;
    
    preset.effectId = currentEffect;
    preset.paletteIndex = currentPaletteIndex;
    preset.brightness = FastLED.getBrightness();
    preset.fadeAmount = fadeAmount;
    preset.speed = paletteSpeed;
    
    // Save visual parameters
    preset.intensity = visualParams.intensity;
    preset.saturation = visualParams.saturation;
    preset.complexity = visualParams.complexity;
    preset.variation = visualParams.variation;
    
    // Save sync settings
    preset.syncMode = currentSyncMode;
    preset.propagationMode = currentPropagationMode;
    
    // Save transition preferences
    preset.useRandomTransitions = useRandomTransitions;
    
    // Calculate checksum
    preset.calculateChecksum();
    
    // Mark as valid
    m_presetValid[slot] = true;
    
    // Save to SPIFFS
    return savePresetToFile(slot);
}

inline bool PresetManager::loadPreset(uint8_t slot, bool immediate) {
    if (slot >= MAX_PRESETS || !m_presetValid[slot]) return false;
    
    const Preset& preset = m_presets[slot];
    
    if (!preset.isValid()) {
        Serial.printf("Preset %d has invalid checksum\n", slot);
        return false;
    }
    
    if (immediate) {
        applyPreset(preset);
    } else {
        // Start morphing
        startMorph(slot);
    }
    
    m_currentPreset = slot;
    Serial.printf("🎵 Loaded preset %d: %s\n", slot, preset.name);
    return true;
}

inline void PresetManager::startMorph(uint8_t targetSlot, uint32_t duration) {
    if (targetSlot >= MAX_PRESETS || !m_presetValid[targetSlot]) return;
    
    // Save current state as morph source
    saveCurrentState(MAX_PRESETS - 1, "MorphTemp");
    m_morphSource = m_presets[MAX_PRESETS - 1];
    
    // Set target
    m_morphTarget = m_presets[targetSlot];
    
    // Start morphing
    m_morphing = true;
    m_morphProgress = 0.0f;
    m_morphStartTime = millis();
    m_morphDuration = duration;
    
    Serial.printf("🔄 Starting morph to preset %d (%dms)\n", targetSlot, duration);
}

inline void PresetManager::updateMorph() {
    if (!m_morphing) return;
    
    uint32_t elapsed = millis() - m_morphStartTime;
    m_morphProgress = constrain((float)elapsed / m_morphDuration, 0.0f, 1.0f);
    
    // Apply smooth interpolation
    float smoothProgress = smoothStep(m_morphProgress);
    
    // Interpolate all parameters
    Preset current;
    interpolatePresets(m_morphSource, m_morphTarget, current, smoothProgress);
    applyPreset(current);
    
    if (m_morphProgress >= 1.0f) {
        m_morphing = false;
        Serial.println("✅ Morph complete");
    }
}

inline void PresetManager::applyPreset(const Preset& preset) {
    // Apply effect - need extern references
    extern uint8_t currentEffect;
    extern uint8_t currentPaletteIndex;
    extern uint8_t fadeAmount;
    extern uint8_t paletteSpeed;
    extern VisualParams visualParams;
    extern HardwareConfig::SyncMode currentSyncMode;
    extern HardwareConfig::PropagationMode currentPropagationMode;
    extern bool useRandomTransitions;
    extern CRGBPalette16 targetPalette;
    extern CRGBPalette16 gGradientPalettes[];
    extern uint8_t gGradientPaletteCount;
    
    // Apply effect (this will trigger a transition)
    extern void startAdvancedTransition(uint8_t newEffect);
    if (preset.effectId < 22) {  // NUM_EFFECTS from main.cpp
        if (preset.effectId != currentEffect) {
            startAdvancedTransition(preset.effectId);
        }
    }
    
    // Apply palette
    currentPaletteIndex = preset.paletteIndex % gGradientPaletteCount;
    targetPalette = CRGBPalette16(gGradientPalettes[currentPaletteIndex]);
    
    // Apply global parameters
    FastLED.setBrightness(preset.brightness);
    fadeAmount = preset.fadeAmount;
    paletteSpeed = preset.speed;
    
    // Apply visual parameters
    visualParams.intensity = preset.intensity;
    visualParams.saturation = preset.saturation;
    visualParams.complexity = preset.complexity;
    visualParams.variation = preset.variation;
    
    // Apply sync settings
    currentSyncMode = preset.syncMode;
    currentPropagationMode = preset.propagationMode;
    
    // Apply transition preferences
    useRandomTransitions = preset.useRandomTransitions;
}

inline void PresetManager::interpolatePresets(const Preset& from, const Preset& to, Preset& result, float t) {
    // Copy metadata from target
    strcpy(result.name, to.name);
    result.timestamp = to.timestamp;
    result.tags = to.tags;
    
    // Interpolate effect (switch at 50%)
    result.effectId = (t < 0.5f) ? from.effectId : to.effectId;
    
    // Interpolate parameters using lerp8by8 from FastLED
    result.paletteIndex = (t < 0.5f) ? from.paletteIndex : to.paletteIndex;
    result.brightness = lerp8by8(from.brightness, to.brightness, t * 255);
    result.fadeAmount = lerp8by8(from.fadeAmount, to.fadeAmount, t * 255);
    result.speed = lerp8by8(from.speed, to.speed, t * 255);
    
    result.intensity = lerp8by8(from.intensity, to.intensity, t * 255);
    result.saturation = lerp8by8(from.saturation, to.saturation, t * 255);
    result.complexity = lerp8by8(from.complexity, to.complexity, t * 255);
    result.variation = lerp8by8(from.variation, to.variation, t * 255);
    
    // Switch modes at 50%
    result.syncMode = (t < 0.5f) ? from.syncMode : to.syncMode;
    result.propagationMode = (t < 0.5f) ? from.propagationMode : to.propagationMode;
    result.useRandomTransitions = (t < 0.5f) ? from.useRandomTransitions : to.useRandomTransitions;
}

inline bool PresetManager::savePresetToFile(uint8_t slot) {
    char filename[32];
    snprintf(filename, sizeof(filename), "%spreset_%02d.bin", PRESET_FILE_PATH, slot);
    
    File file = SPIFFS.open(filename, FILE_WRITE);
    if (!file) {
        Serial.printf("Failed to open file for writing: %s\n", filename);
        return false;
    }
    
    size_t written = file.write((uint8_t*)&m_presets[slot], sizeof(Preset));
    file.close();
    
    return written == sizeof(Preset);
}

inline bool PresetManager::loadPresetFromFile(uint8_t slot) {
    char filename[32];
    snprintf(filename, sizeof(filename), "%spreset_%02d.bin", PRESET_FILE_PATH, slot);
    
    if (!SPIFFS.exists(filename)) {
        return false;
    }
    
    File file = SPIFFS.open(filename, FILE_READ);
    if (!file) {
        return false;
    }
    
    size_t read = file.read((uint8_t*)&m_presets[slot], sizeof(Preset));
    file.close();
    
    if (read == sizeof(Preset) && m_presets[slot].isValid()) {
        m_presetValid[slot] = true;
        return true;
    }
    
    return false;
}

inline void PresetManager::loadAllPresets() {
    int loaded = 0;
    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        if (loadPresetFromFile(i)) {
            loaded++;
        }
    }
    Serial.printf("📁 Loaded %d presets from SPIFFS\n", loaded);
}

inline void PresetManager::quickSave(uint8_t slot) {
    if (slot < 10) { // Reserve slots 0-9 for quick access
        char name[PRESET_NAME_LENGTH];
        snprintf(name, PRESET_NAME_LENGTH, "Quick %d", slot);
        if (saveCurrentState(slot, name)) {
            Serial.printf("💾 Quick saved to slot %d\n", slot);
        }
    }
}

inline void PresetManager::quickLoad(uint8_t slot) {
    if (slot < 10 && m_presetValid[slot]) {
        loadPreset(slot, false); // Use morphing
    }
}

inline void PresetManager::handleEncoderInput(uint8_t encoderId, int32_t delta) {
    if (encoderId == 7) { // Use encoder 7 for preset navigation
        int newPreset = m_currentPreset + (delta > 0 ? 1 : -1);
        
        // Find next valid preset
        for (int i = 0; i < MAX_PRESETS; i++) {
            if (newPreset < 0) newPreset = MAX_PRESETS - 1;
            if (newPreset >= MAX_PRESETS) newPreset = 0;
            
            if (m_presetValid[newPreset]) {
                loadPreset(newPreset, false); // Morph to preset
                break;
            }
            
            newPreset += (delta > 0 ? 1 : -1);
        }
    }
}

inline void PresetManager::handleButtonPress(uint8_t encoderId) {
    if (encoderId == 7) {
        // Save to current slot on button press
        saveCurrentState(m_currentPreset);
        Serial.printf("💾 Saved current state to preset %d\n", m_currentPreset);
        
        // Flash encoder LED to confirm (if available)
        extern class EncoderLEDFeedback* encoderFeedback;
        if (encoderFeedback) {
            encoderFeedback->flashEncoder(7, 0, 255, 0, 500); // Green flash
        }
    }
}

#endif // PRESET_MANAGER_H