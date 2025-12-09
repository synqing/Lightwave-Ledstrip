#include "ZoneComposer.h"
#include "../../config/hardware_config.h"
#include <Arduino.h>

// External references to global LED buffers and effects
extern CRGB strip1[];
extern CRGB strip2[];
extern CRGB leds[];
extern Effect effects[];
extern const uint8_t NUM_EFFECTS;

ZoneComposer::ZoneComposer()
    : m_zoneCount(3)  // Default to 3-zone mode
    , m_enabled(false)
    , m_configManager(nullptr)
    , m_activeConfig(ZONE_CONFIGS_3ZONE)  // Start with 3-zone config
    , m_configSize(3)                      // Start with 3 zones
{
    // Initialize zone configuration
    for (uint8_t i = 0; i < HardwareConfig::MAX_ZONES; i++) {
        m_zoneEffects[i] = 0;  // Default to first effect
        m_zoneEnabled[i] = false;  // Disabled by default
    }

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

    // Step 2: Execute effect - it renders to full strip1/strip2
    effects[effectId].function();

    // Step 3: Extract zone segment and map to output
    mapZoneToOutput(zoneId);

    // Step 4: Clear strips for next zone
    fill_solid(strip1, 160, CRGB::Black);
    fill_solid(strip2, 160, CRGB::Black);
}

void ZoneComposer::mapZoneToOutput(uint8_t zoneId) {
    if (zoneId >= m_configSize) {  // Use dynamic size check
        return;
    }

    const ZoneDefinition& zone = m_activeConfig[zoneId];  // Use active config

    // Copy zone segments from strip1/strip2 to output buffers
    // Zone layout: left segment (20 LEDs) + right segment (20 LEDs)

    // Copy left segment for strip1
    for (uint8_t i = zone.strip1StartLeft; i <= zone.strip1EndLeft; i++) {
        if (i < 160) {
            m_outputStrip1[i] = strip1[i];
        }
    }

    // Copy right segment for strip1
    for (uint8_t i = zone.strip1StartRight; i <= zone.strip1EndRight; i++) {
        if (i < 160) {
            m_outputStrip1[i] = strip1[i];
        }
    }

    // Copy left segment for strip2
    for (uint8_t i = zone.strip2StartLeft; i <= zone.strip2EndLeft; i++) {
        if (i < 160) {
            m_outputStrip2[i] = strip2[i];
        }
    }

    // Copy right segment for strip2
    for (uint8_t i = zone.strip2StartRight; i <= zone.strip2EndRight; i++) {
        if (i < 160) {
            m_outputStrip2[i] = strip2[i];
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
