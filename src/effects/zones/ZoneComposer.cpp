#include "ZoneComposer.h"
#include "../../config/hardware_config.h"
#include <Arduino.h>

// External references to global LED buffers and effects
extern CRGB strip1[];
extern CRGB strip2[];
extern CRGB leds[];
extern Effect effects[];
extern const uint8_t NUM_EFFECTS;
extern uint8_t effectSpeed;  // Global speed variable for effects

// Palette access for per-zone palette support (using master palette system)
extern CRGBPalette16 currentPalette;
extern const TProgmemRGBGradientPaletteRef gMasterPalettes[];
extern const uint8_t gMasterPaletteCount;

// Visual parameters for per-zone parameter support (Phase C.4)
extern VisualParams visualParams;

ZoneComposer::ZoneComposer()
    : m_zoneCount(3)  // Default to 3-zone mode
    , m_enabled(true)   // Zone mode ON by default at boot
    , m_configManager(nullptr)
    , m_activeConfig(ZONE_CONFIGS_3ZONE)  // Start with 3-zone config
    , m_configSize(3)                      // Start with 3 zones
{
    // Initialize zone configuration with custom defaults
    // Zone 0 (center): Wave (effect ID 2)
    m_zoneEffects[0] = 2;
    m_zoneEnabled[0] = true;
    m_zoneBrightness[0] = 255;
    m_zoneSpeed[0] = 25;
    m_zonePalette[0] = 0;  // Use global palette
    m_zoneBlendMode[0] = BLEND_OVERWRITE;

    // Zone 1 (middle): LGP Wave Collision (effect ID 11)
    m_zoneEffects[1] = 11;
    m_zoneEnabled[1] = true;
    m_zoneBrightness[1] = 255;
    m_zoneSpeed[1] = 25;
    m_zonePalette[1] = 0;
    m_zoneBlendMode[1] = BLEND_OVERWRITE;

    // Zone 2 (outer): LGP Diamond Lattice (effect ID 12)
    m_zoneEffects[2] = 12;
    m_zoneEnabled[2] = true;
    m_zoneBrightness[2] = 255;
    m_zoneSpeed[2] = 25;
    m_zonePalette[2] = 0;
    m_zoneBlendMode[2] = BLEND_OVERWRITE;

    // Zone 3 (4-zone mode only): Default to first effect, disabled
    m_zoneEffects[3] = 0;
    m_zoneEnabled[3] = false;
    m_zoneBrightness[3] = 255;
    m_zoneSpeed[3] = 25;
    m_zonePalette[3] = 0;
    m_zoneBlendMode[3] = BLEND_OVERWRITE;

    // Clear buffers
    fill_solid(m_tempStrip1, 40, CRGB::Black);
    fill_solid(m_tempStrip2, 40, CRGB::Black);
    fill_solid(m_outputStrip1, 160, CRGB::Black);
    fill_solid(m_outputStrip2, 160, CRGB::Black);

    // Create config manager
    m_configManager = new ZoneConfigManager(this);
}

void ZoneComposer::setZoneEffect(uint8_t zoneId, uint8_t effectId) {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        Serial.printf("ERROR: Invalid zone ID %d (max %d)\n", zoneId, HardwareConfig::MAX_ZONES - 1);
        return;
    }
    if (effectId >= NUM_EFFECTS) {
        Serial.printf("ERROR: Invalid effect ID %d (max %d)\n", effectId, NUM_EFFECTS - 1);
        return;
    }

    m_zoneEffects[zoneId] = effectId;
    m_zoneEnabled[zoneId] = true;  // Auto-enable zone when effect is set
    Serial.printf("Zone %d effect set to %d (enabled: %s)\n", zoneId, effectId, m_zoneEnabled[zoneId] ? "true" : "false");
}

void ZoneComposer::enableZone(uint8_t zoneId, bool enabled) {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        Serial.printf("ERROR: Invalid zone ID %d\n", zoneId);
        return;
    }

    m_zoneEnabled[zoneId] = enabled;
    Serial.printf("Zone %d %s\n", zoneId, enabled ? "enabled" : "disabled");
}

void ZoneComposer::setZoneBrightness(uint8_t zoneId, uint8_t brightness) {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        Serial.printf("ERROR: Invalid zone ID %d\n", zoneId);
        return;
    }

    m_zoneBrightness[zoneId] = brightness;
    Serial.printf("Zone %d brightness set to %d\n", zoneId, brightness);
}

uint8_t ZoneComposer::getZoneBrightness(uint8_t zoneId) const {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        return 255;
    }
    return m_zoneBrightness[zoneId];
}

void ZoneComposer::setZoneSpeed(uint8_t zoneId, uint8_t speed) {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        Serial.printf("ERROR: Invalid zone ID %d\n", zoneId);
        return;
    }

    // Clamp speed to valid range (1-50)
    if (speed < 1) speed = 1;
    if (speed > 50) speed = 50;

    m_zoneSpeed[zoneId] = speed;
    Serial.printf("Zone %d speed set to %d\n", zoneId, speed);
}

uint8_t ZoneComposer::getZoneSpeed(uint8_t zoneId) const {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        return 25;
    }
    return m_zoneSpeed[zoneId];
}

void ZoneComposer::setZonePalette(uint8_t zoneId, uint8_t paletteId) {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        Serial.printf("ERROR: Invalid zone ID %d\n", zoneId);
        return;
    }

    m_zonePalette[zoneId] = paletteId;
    Serial.printf("Zone %d palette set to %d%s\n", zoneId, paletteId,
                 paletteId == 0 ? " (global)" : "");
}

uint8_t ZoneComposer::getZonePalette(uint8_t zoneId) const {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        return 0;
    }
    return m_zonePalette[zoneId];
}

void ZoneComposer::setZoneBlendMode(uint8_t zoneId, BlendMode mode) {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        Serial.printf("ERROR: Invalid zone ID %d\n", zoneId);
        return;
    }
    
    if (mode >= BLEND_MODE_COUNT) {
        mode = BLEND_OVERWRITE;
    }

    m_zoneBlendMode[zoneId] = mode;
    Serial.printf("Zone %d blend mode set to %d\n", zoneId, (int)mode);
}

BlendMode ZoneComposer::getZoneBlendMode(uint8_t zoneId) const {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        return BLEND_OVERWRITE;
    }
    return m_zoneBlendMode[zoneId];
}

void ZoneComposer::setZoneCount(uint8_t count) {
    if (count > HardwareConfig::MAX_ZONES || count < 1) {
        Serial.printf("ERROR: Zone count %d out of range (1-%d)\n", count, HardwareConfig::MAX_ZONES);
        return;
    }

    m_zoneCount = count;

    // Select configuration based on zone count
    if (count == 3) {
        m_activeConfig = ZONE_CONFIGS_3ZONE;
        m_configSize = 3;
        Serial.println("Switched to 3-zone configuration (30+90+40 LEDs)");
    } else {
        m_activeConfig = ZONE_CONFIGS_4ZONE;
        m_configSize = 4;
        Serial.println("Switched to 4-zone configuration (40+40+40+40 LEDs)");
    }

    Serial.printf("Zone count set to %d\n", count);
}

void ZoneComposer::enable() {
    m_enabled = true;
    Serial.println("Zone Composer enabled");
}

void ZoneComposer::disable() {
    m_enabled = false;
    Serial.println("Zone Composer disabled");
}

void ZoneComposer::clearOutputBuffers() {
    fill_solid(m_outputStrip1, 160, CRGB::Black);
    fill_solid(m_outputStrip2, 160, CRGB::Black);
}

void ZoneComposer::render() {
    if (!m_enabled) {
        return;
    }

    // Clear output buffers
    clearOutputBuffers();

    // DEBUG: Log which zones are being rendered
    static uint32_t lastLog = 0;
    if (millis() - lastLog > 2000) {  // Log every 2 seconds
        Serial.printf("DEBUG ZoneComposer: Rendering %d zones. Enabled: ", m_zoneCount);
        for (uint8_t i = 0; i < m_zoneCount; i++) {
            Serial.printf("[Z%d:%s,E%d] ", i, m_zoneEnabled[i] ? "ON" : "OFF", m_zoneEffects[i]);
        }
        Serial.println();
        lastLog = millis();
    }

    // Render each enabled zone
    for (uint8_t i = 0; i < m_zoneCount; i++) {
        if (m_zoneEnabled[i]) {
            renderZone(i);
        }
    }

    // Copy composite output to main buffers
    copyOutputToMain();
}

void ZoneComposer::renderZone(uint8_t zoneId) {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        return;
    }

    uint8_t effectId = m_zoneEffects[zoneId];
    if (effectId >= NUM_EFFECTS || effects[effectId].function == nullptr) {
        return;
    }

    // SIMPLIFIED PHASE 1 APPROACH:
    // Clear main strips, render effect, extract zone segment, clear again

    // Step 1: Clear main strips
    fill_solid(strip1, 160, CRGB::Black);
    fill_solid(strip2, 160, CRGB::Black);

    // Step 2: Set per-zone speed before effect execution
    // Effects that use effectSpeed will now use the zone-specific value
    effectSpeed = m_zoneSpeed[zoneId];

    // Step 3: Apply per-zone visual parameters (Phase C.4 - swap/restore pattern)
    VisualParams savedParams = visualParams;
    visualParams = m_zoneVisualParams[zoneId];

    // Step 4: Apply per-zone palette (save original first)
    CRGBPalette16 savedPalette = currentPalette;
    uint8_t zonePaletteId = m_zonePalette[zoneId];
    if (zonePaletteId > 0 && zonePaletteId <= gMasterPaletteCount) {
        // Use zone-specific palette (1-indexed: paletteId 1 = gMasterPalettes[0])
        currentPalette = gMasterPalettes[zonePaletteId - 1];
    }

    // Step 5: Execute effect - it renders to full strip1/strip2
    // Effect now uses zone-specific speed, palette, and visual params
    effects[effectId].function();

    // Step 6: Restore original palette and visual params
    currentPalette = savedPalette;
    visualParams = savedParams;

    // Step 7: Extract zone segment and map to output (applies per-zone brightness)
    mapZoneToOutput(zoneId);

    // Step 8: Clear strips for next zone
    fill_solid(strip1, 160, CRGB::Black);
    fill_solid(strip2, 160, CRGB::Black);
}

void ZoneComposer::mapZoneToOutput(uint8_t zoneId) {
    if (zoneId >= m_configSize) {  // Use dynamic size check
        return;
    }

    const ZoneDefinition& zone = m_activeConfig[zoneId];  // Use active config
    uint8_t brightness = m_zoneBrightness[zoneId];
    BlendMode blendMode = m_zoneBlendMode[zoneId];

    // Helper lambda to apply brightness scaling to a pixel
    auto applyBrightness = [brightness](CRGB& pixel) {
        if (brightness < 255) {
            pixel.nscale8(brightness);
        }
    };

    // Copy zone segments from strip1/strip2 to output buffers
    // Zone layout: left segment (20 LEDs) + right segment (20 LEDs)
    // Apply per-zone brightness during copy

    // Copy left segment for strip1
    for (uint8_t i = zone.strip1StartLeft; i <= zone.strip1EndLeft; i++) {
        if (i < 160) {
            CRGB source = strip1[i];
            applyBrightness(source);
            m_outputStrip1[i] = BlendingEngine::blendPixels(m_outputStrip1[i], source, blendMode);
        }
    }

    // Copy right segment for strip1
    for (uint8_t i = zone.strip1StartRight; i <= zone.strip1EndRight; i++) {
        if (i < 160) {
            CRGB source = strip1[i];
            applyBrightness(source);
            m_outputStrip1[i] = BlendingEngine::blendPixels(m_outputStrip1[i], source, blendMode);
        }
    }

    // Copy left segment for strip2
    for (uint8_t i = zone.strip2StartLeft; i <= zone.strip2EndLeft; i++) {
        if (i < 160) {
            CRGB source = strip2[i];
            applyBrightness(source);
            m_outputStrip2[i] = BlendingEngine::blendPixels(m_outputStrip2[i], source, blendMode);
        }
    }

    // Copy right segment for strip2
    for (uint8_t i = zone.strip2StartRight; i <= zone.strip2EndRight; i++) {
        if (i < 160) {
            CRGB source = strip2[i];
            applyBrightness(source);
            m_outputStrip2[i] = BlendingEngine::blendPixels(m_outputStrip2[i], source, blendMode);
        }
    }
}

void ZoneComposer::copyOutputToMain() {
    // Copy composite output to main strip buffers
    memcpy(strip1, m_outputStrip1, 160 * sizeof(CRGB));
    memcpy(strip2, m_outputStrip2, 160 * sizeof(CRGB));

    // Also update unified leds[] buffer
    memcpy(leds, strip1, 160 * sizeof(CRGB));
    memcpy(&leds[160], strip2, 160 * sizeof(CRGB));
}

// Zone query methods
uint8_t ZoneComposer::getZoneEffect(uint8_t zoneId) const {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        return 0;
    }
    return m_zoneEffects[zoneId];
}

bool ZoneComposer::isZoneEnabled(uint8_t zoneId) const {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        return false;
    }
    return m_zoneEnabled[zoneId];
}

// Configuration management methods
bool ZoneComposer::saveConfig() {
    if (!m_configManager) {
        Serial.println("❌ Config manager not initialized");
        return false;
    }
    return m_configManager->saveToNVS();
}

bool ZoneComposer::loadConfig() {
    if (!m_configManager) {
        Serial.println("❌ Config manager not initialized");
        return false;
    }
    return m_configManager->loadFromNVS();
}

bool ZoneComposer::loadPreset(uint8_t presetId) {
    if (!m_configManager) {
        Serial.println("❌ Config manager not initialized");
        return false;
    }
    return m_configManager->loadPreset(presetId);
}

const char* ZoneComposer::getPresetName(uint8_t presetId) const {
    if (!m_configManager) {
        return "Error";
    }
    return m_configManager->getPresetName(presetId);
}

// ============================================================================
// User Preset Management (Phase C.1) - Wrappers for ZoneConfigManager
// ============================================================================

bool ZoneComposer::saveUserPreset(uint8_t slot, const char* name) {
    if (!m_configManager) return false;
    return m_configManager->saveUserPreset(slot, name);
}

bool ZoneComposer::loadUserPreset(uint8_t slot) {
    if (!m_configManager) return false;
    return m_configManager->loadUserPreset(slot);
}

bool ZoneComposer::deleteUserPreset(uint8_t slot) {
    if (!m_configManager) return false;
    return m_configManager->deleteUserPreset(slot);
}

bool ZoneComposer::hasUserPreset(uint8_t slot) const {
    if (!m_configManager) return false;
    return m_configManager->hasUserPreset(slot);
}

bool ZoneComposer::getUserPreset(uint8_t slot, UserPreset& preset) const {
    if (!m_configManager) return false;
    return m_configManager->getUserPreset(slot, preset);
}

uint8_t ZoneComposer::getFilledUserPresetCount() const {
    if (!m_configManager) return 0;
    return m_configManager->getFilledUserPresetCount();
}

void ZoneComposer::printStatus() const {
    Serial.println("\n========== ZONE COMPOSER STATUS ==========");
    Serial.printf("System: %s\n", m_enabled ? "ENABLED" : "DISABLED");
    Serial.printf("Active Zones: %d/%d\n", m_zoneCount, HardwareConfig::MAX_ZONES);
    Serial.printf("Config Mode: %s\n", m_zoneCount == 3 ? "3-ZONE (30+90+40)" : "4-ZONE (40x4)");

    for (uint8_t i = 0; i < m_zoneCount; i++) {
        Serial.printf("\nZone %d:\n", i);
        Serial.printf("  Status: %s\n", m_zoneEnabled[i] ? "ENABLED" : "DISABLED");
        Serial.printf("  Effect: %d (%s)\n", m_zoneEffects[i],
                     m_zoneEffects[i] < NUM_EFFECTS ? effects[m_zoneEffects[i]].name : "INVALID");
        Serial.printf("  Brightness: %d\n", m_zoneBrightness[i]);
        Serial.printf("  Speed: %d\n", m_zoneSpeed[i]);
        Serial.printf("  Palette: %d%s\n", m_zonePalette[i],
                     m_zonePalette[i] == 0 ? " (global)" : "");
        Serial.printf("  VisualParams: I=%d S=%d C=%d V=%d\n",
                     m_zoneVisualParams[i].intensity,
                     m_zoneVisualParams[i].saturation,
                     m_zoneVisualParams[i].complexity,
                     m_zoneVisualParams[i].variation);

        const ZoneDefinition& zone = m_activeConfig[i];  // Use active config
        Serial.printf("  Strip1 Range: [%d-%d] + [%d-%d]\n",
                     zone.strip1StartLeft, zone.strip1EndLeft,
                     zone.strip1StartRight, zone.strip1EndRight);
        Serial.printf("  Strip2 Range: [%d-%d] + [%d-%d]\n",
                     zone.strip2StartLeft, zone.strip2EndLeft,
                     zone.strip2StartRight, zone.strip2EndRight);
        Serial.printf("  Total LEDs: %d\n", zone.totalLEDs);
    }

    Serial.println("===========================================\n");
}

// ============================================================================
// Per-Zone Visual Parameters (Phase C.4)
// ============================================================================

void ZoneComposer::setZoneVisualParams(uint8_t zoneId, const VisualParams& params) {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        Serial.printf("ERROR: Invalid zone ID %d\n", zoneId);
        return;
    }
    m_zoneVisualParams[zoneId] = params;
}

VisualParams ZoneComposer::getZoneVisualParams(uint8_t zoneId) const {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        return VisualParams();  // Return default
    }
    return m_zoneVisualParams[zoneId];
}

void ZoneComposer::setZoneIntensity(uint8_t zoneId, uint8_t value) {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        Serial.printf("ERROR: Invalid zone ID %d\n", zoneId);
        return;
    }
    m_zoneVisualParams[zoneId].intensity = value;
}

uint8_t ZoneComposer::getZoneIntensity(uint8_t zoneId) const {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        return 128;  // Default intensity
    }
    return m_zoneVisualParams[zoneId].intensity;
}

void ZoneComposer::setZoneSaturation(uint8_t zoneId, uint8_t value) {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        Serial.printf("ERROR: Invalid zone ID %d\n", zoneId);
        return;
    }
    m_zoneVisualParams[zoneId].saturation = value;
}

uint8_t ZoneComposer::getZoneSaturation(uint8_t zoneId) const {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        return 255;  // Default saturation
    }
    return m_zoneVisualParams[zoneId].saturation;
}

void ZoneComposer::setZoneComplexity(uint8_t zoneId, uint8_t value) {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        Serial.printf("ERROR: Invalid zone ID %d\n", zoneId);
        return;
    }
    m_zoneVisualParams[zoneId].complexity = value;
}

uint8_t ZoneComposer::getZoneComplexity(uint8_t zoneId) const {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        return 128;  // Default complexity
    }
    return m_zoneVisualParams[zoneId].complexity;
}

void ZoneComposer::setZoneVariation(uint8_t zoneId, uint8_t value) {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        Serial.printf("ERROR: Invalid zone ID %d\n", zoneId);
        return;
    }
    m_zoneVisualParams[zoneId].variation = value;
}

uint8_t ZoneComposer::getZoneVariation(uint8_t zoneId) const {
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        return 0;  // Default variation
    }
    return m_zoneVisualParams[zoneId].variation;
}
