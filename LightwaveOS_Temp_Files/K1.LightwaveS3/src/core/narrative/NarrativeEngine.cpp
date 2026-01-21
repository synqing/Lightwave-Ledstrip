/**
 * @file NarrativeEngine.cpp
 * @brief Implementation of the narrative timing engine
 *
 * LightwaveOS v2 - Narrative Engine
 */

#include "NarrativeEngine.h"
#include "../persistence/NVSManager.h"
#include <cstddef>

namespace lightwaveos {
namespace narrative {

// ==================== NarrativeConfigData Implementation ====================

void NarrativeConfigData::calculateChecksum() {
    // Calculate CRC32 over all fields except checksum
    const size_t dataSize = offsetof(NarrativeConfigData, checksum);
    checksum = persistence::NVSManager::calculateCRC32(this, dataSize);
}

bool NarrativeConfigData::isValid() const {
    // Recalculate checksum and compare
    const size_t dataSize = offsetof(NarrativeConfigData, checksum);
    uint32_t calculated = persistence::NVSManager::calculateCRC32(this, dataSize);
    return (checksum == calculated);
}

// ============================================================================
// Constructor
// ============================================================================

NarrativeEngine::NarrativeEngine()
    : m_lastPhase(PHASE_REST)
    , m_justEnteredPhase(PHASE_REST)
    , m_phaseJustChanged(false)
    , m_enabled(false)
    , m_paused(false)
    , m_pauseStartMs(0)
    , m_totalPausedMs(0)
    , m_tensionOverride(-1.0f)
    , m_manualPhaseControl(false)
{
    // Configure default 4-second cycle
    m_cycle.buildDuration = 1.5f;
    m_cycle.holdDuration = 0.5f;
    m_cycle.releaseDuration = 1.5f;
    m_cycle.restDuration = 0.5f;
    m_cycle.buildCurve = EASE_IN_QUAD;
    m_cycle.releaseCurve = EASE_OUT_QUAD;
    m_cycle.holdBreathe = 0.1f;
    m_cycle.snapAmount = 0.0f;
    m_cycle.durationVariance = 0.0f;

    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        m_zoneOffsets[i] = 0.0f;
    }
}

// ============================================================================
// Enable/Disable
// ============================================================================

void NarrativeEngine::enable() {
    if (!m_enabled) {
        m_enabled = true;
        m_cycle.reset();
        m_lastPhase = m_cycle.getPhase();
        m_phaseJustChanged = false;
        m_totalPausedMs = 0;
    }
}

void NarrativeEngine::disable() {
    m_enabled = false;
}

// ============================================================================
// Core Update
// ============================================================================

void NarrativeEngine::update() {
    if (!m_enabled || m_paused) return;

    NarrativePhase previousPhase = m_cycle.getPhase();
    m_cycle.update();

    NarrativePhase currentPhase = m_cycle.getPhase();
    if (currentPhase != previousPhase) {
        m_phaseJustChanged = true;
        m_justEnteredPhase = currentPhase;
        m_lastPhase = previousPhase;
    } else {
        m_phaseJustChanged = false;
    }
}

// ============================================================================
// Configuration - Durations
// ============================================================================

void NarrativeEngine::setBuildDuration(float seconds) {
    m_cycle.buildDuration = max(0.01f, seconds);
}

void NarrativeEngine::setHoldDuration(float seconds) {
    m_cycle.holdDuration = max(0.0f, seconds);
}

void NarrativeEngine::setReleaseDuration(float seconds) {
    m_cycle.releaseDuration = max(0.01f, seconds);
}

void NarrativeEngine::setRestDuration(float seconds) {
    m_cycle.restDuration = max(0.0f, seconds);
}

void NarrativeEngine::setTempo(float totalCycleDuration) {
    float currentTotal = m_cycle.getTotalDuration();
    if (currentTotal <= 0.0f || totalCycleDuration <= 0.0f) return;

    float scale = totalCycleDuration / currentTotal;
    m_cycle.buildDuration *= scale;
    m_cycle.holdDuration *= scale;
    m_cycle.releaseDuration *= scale;
    m_cycle.restDuration *= scale;
}

// ============================================================================
// Configuration - Curves
// ============================================================================

void NarrativeEngine::setBuildCurve(EasingCurve curve) {
    m_cycle.buildCurve = curve;
}

void NarrativeEngine::setReleaseCurve(EasingCurve curve) {
    m_cycle.releaseCurve = curve;
}

void NarrativeEngine::setHoldBreathe(float amount) {
    m_cycle.holdBreathe = constrain(amount, 0.0f, 1.0f);
}

void NarrativeEngine::setSnapAmount(float amount) {
    m_cycle.snapAmount = constrain(amount, 0.0f, 1.0f);
}

void NarrativeEngine::setDurationVariance(float amount) {
    m_cycle.durationVariance = constrain(amount, 0.0f, 1.0f);
}

// ============================================================================
// Zone Phase Offsets
// ============================================================================

void NarrativeEngine::setZonePhaseOffset(uint8_t zoneId, float offsetRatio) {
    if (zoneId >= MAX_ZONES) return;
    offsetRatio = fmod(offsetRatio, 1.0f);
    if (offsetRatio < 0.0f) offsetRatio += 1.0f;
    m_zoneOffsets[zoneId] = offsetRatio;
}

float NarrativeEngine::getZonePhaseOffset(uint8_t zoneId) const {
    if (zoneId >= MAX_ZONES) return 0.0f;
    return m_zoneOffsets[zoneId];
}

// ============================================================================
// Query Methods - Global
// ============================================================================

float NarrativeEngine::getIntensity() const {
    // Manual override takes precedence (v1 compatibility)
    if (m_tensionOverride >= 0.0f) {
        return constrain(m_tensionOverride, 0.0f, 1.0f);
    }
    
    if (!m_enabled) return 1.0f;
    return m_cycle.getIntensity();
}

NarrativePhase NarrativeEngine::getPhase() const {
    if (!m_enabled) return PHASE_HOLD;
    return m_cycle.getPhase();
}

float NarrativeEngine::getPhaseT() const {
    if (!m_enabled) return 1.0f;
    return m_cycle.getPhaseT();
}

float NarrativeEngine::getCycleT() const {
    if (!m_enabled) return 0.0f;

    uint32_t now = millis();
    float elapsed = (now - m_cycle.cycleStartMs) / 1000.0f;
    float total = m_cycle.currentCycleDuration;
    if (total <= 0.0f) return 0.0f;

    return Easing::clamp01(elapsed / total);
}

// ============================================================================
// v1 NarrativeTension Compatibility Methods
// ============================================================================

float NarrativeEngine::getTempoMultiplier() const {
    float tension = getTension();
    // Formula: multiplier = 1.0 + (tension * 0.5)
    return 1.0f + (tension * 0.5f);
}

float NarrativeEngine::getComplexityScaling() const {
    float tension = getTension();
    // Formula: complexity = baseComplexity * (0.5 + tension * 0.5)
    return 0.5f + (tension * 0.5f);
}

void NarrativeEngine::setTensionOverride(float tension) {
    if (tension < 0.0f) {
        m_tensionOverride = -1.0f;  // Disable override
    } else {
        m_tensionOverride = constrain(tension, 0.0f, 1.0f);
    }
}

void NarrativeEngine::setPhase(NarrativePhase phase, uint32_t durationMs) {
    // Validate phase
    if (phase > PHASE_REST) {
        phase = PHASE_BUILD;  // Clamp to valid range
    }
    
    // Clamp duration to valid range (100ms - 60000ms)
    if (durationMs < 100) {
        durationMs = 100;
    } else if (durationMs > 60000) {
        durationMs = 60000;
    }
    
    // Convert milliseconds to seconds
    float durationSeconds = durationMs / 1000.0f;
    
    // Set phase duration based on which phase
    switch (phase) {
        case PHASE_BUILD:
            setBuildDuration(durationSeconds);
            break;
        case PHASE_HOLD:
            setHoldDuration(durationSeconds);
            break;
        case PHASE_RELEASE:
            setReleaseDuration(durationSeconds);
            break;
        case PHASE_REST:
            setRestDuration(durationSeconds);
            break;
    }
    
    // Manually set the phase in the cycle (v1 compatibility)
    // This allows manual phase control while still using the cycle's auto-advance logic
    m_cycle.phase = phase;
    m_cycle.phaseStartMs = millis();
    m_cycle.initialized = true;
    m_manualPhaseControl = true;
    
    // Update edge detection
    NarrativePhase previousPhase = m_lastPhase;
    m_phaseJustChanged = (phase != previousPhase);
    m_justEnteredPhase = phase;
    m_lastPhase = previousPhase;
}

// ============================================================================
// Query Methods - Zone-specific
// ============================================================================

float NarrativeEngine::getIntensity(uint8_t zoneId) const {
    if (!m_enabled) return 1.0f;
    if (zoneId >= MAX_ZONES) return getIntensity();

    float baseCycleT = getCycleT();
    float offset = m_zoneOffsets[zoneId];
    float offsetCycleT = fmod(baseCycleT + offset, 1.0f);

    return getIntensityAtCycleT(offsetCycleT);
}

NarrativePhase NarrativeEngine::getPhase(uint8_t zoneId) const {
    if (!m_enabled) return PHASE_HOLD;
    if (zoneId >= MAX_ZONES) return getPhase();

    float baseCycleT = getCycleT();
    float offset = m_zoneOffsets[zoneId];
    float offsetCycleT = fmod(baseCycleT + offset, 1.0f);

    return getPhaseAtCycleT(offsetCycleT);
}

float NarrativeEngine::getPhaseT(uint8_t zoneId) const {
    if (!m_enabled) return 1.0f;
    if (zoneId >= MAX_ZONES) return getPhaseT();

    float baseCycleT = getCycleT();
    float offset = m_zoneOffsets[zoneId];
    float offsetCycleT = fmod(baseCycleT + offset, 1.0f);

    return getPhaseTAtCycleT(offsetCycleT);
}

float NarrativeEngine::getCycleT(uint8_t zoneId) const {
    if (!m_enabled) return 0.0f;
    if (zoneId >= MAX_ZONES) return getCycleT();

    float baseCycleT = getCycleT();
    float offset = m_zoneOffsets[zoneId];
    return fmod(baseCycleT + offset, 1.0f);
}

// ============================================================================
// Internal - Calculate at arbitrary cycle position
// ============================================================================

float NarrativeEngine::getIntensityAtCycleT(float cycleT) const {
    NarrativePhase phase = getPhaseAtCycleT(cycleT);
    float phaseT = getPhaseTAtCycleT(cycleT);

    float intensity = 0.0f;

    switch (phase) {
        case PHASE_BUILD:
            intensity = Easing::ease(phaseT, m_cycle.buildCurve);
            break;
        case PHASE_HOLD:
            intensity = m_cycle.applyBreathe(phaseT);
            break;
        case PHASE_RELEASE:
            intensity = 1.0f - Easing::ease(phaseT, m_cycle.releaseCurve);
            break;
        case PHASE_REST:
            intensity = 0.0f;
            break;
    }

    if (m_cycle.snapAmount > 0.0f && (phase == PHASE_BUILD || phase == PHASE_RELEASE)) {
        intensity = m_cycle.applySnap(intensity);
    }

    return Easing::clamp01(intensity);
}

NarrativePhase NarrativeEngine::getPhaseAtCycleT(float cycleT) const {
    float total = m_cycle.getTotalDuration();
    if (total <= 0.0f) return PHASE_BUILD;

    float buildEnd = m_cycle.buildDuration / total;
    float holdEnd = (m_cycle.buildDuration + m_cycle.holdDuration) / total;
    float releaseEnd = (m_cycle.buildDuration + m_cycle.holdDuration + m_cycle.releaseDuration) / total;

    if (cycleT < buildEnd) return PHASE_BUILD;
    if (cycleT < holdEnd) return PHASE_HOLD;
    if (cycleT < releaseEnd) return PHASE_RELEASE;
    return PHASE_REST;
}

float NarrativeEngine::getPhaseTAtCycleT(float cycleT) const {
    float total = m_cycle.getTotalDuration();
    if (total <= 0.0f) return 0.0f;

    float buildEnd = m_cycle.buildDuration / total;
    float holdEnd = (m_cycle.buildDuration + m_cycle.holdDuration) / total;
    float releaseEnd = (m_cycle.buildDuration + m_cycle.holdDuration + m_cycle.releaseDuration) / total;

    if (cycleT < buildEnd) {
        if (buildEnd <= 0.0f) return 0.0f;
        return cycleT / buildEnd;
    }
    if (cycleT < holdEnd) {
        float holdStart = buildEnd;
        float holdDur = holdEnd - holdStart;
        if (holdDur <= 0.0f) return 0.0f;
        return (cycleT - holdStart) / holdDur;
    }
    if (cycleT < releaseEnd) {
        float releaseStart = holdEnd;
        float releaseDur = releaseEnd - releaseStart;
        if (releaseDur <= 0.0f) return 0.0f;
        return (cycleT - releaseStart) / releaseDur;
    }

    float restStart = releaseEnd;
    float restDur = 1.0f - restStart;
    if (restDur <= 0.0f) return 0.0f;
    return (cycleT - restStart) / restDur;
}

// ============================================================================
// Edge Detection
// ============================================================================

bool NarrativeEngine::justEntered(NarrativePhase phase) const {
    return m_phaseJustChanged && m_justEnteredPhase == phase;
}

// ============================================================================
// Manual Control
// ============================================================================

void NarrativeEngine::trigger() {
    m_cycle.trigger();
    m_phaseJustChanged = true;
    m_justEnteredPhase = PHASE_BUILD;
}

void NarrativeEngine::pause() {
    if (!m_paused && m_enabled) {
        m_paused = true;
        m_pauseStartMs = millis();
    }
}

void NarrativeEngine::resume() {
    if (m_paused) {
        m_paused = false;
        uint32_t pauseDuration = millis() - m_pauseStartMs;
        m_cycle.phaseStartMs += pauseDuration;
        m_cycle.cycleStartMs += pauseDuration;
        m_totalPausedMs += pauseDuration;
    }
}

void NarrativeEngine::reset() {
    m_cycle.reset();
    m_lastPhase = PHASE_BUILD;
    m_phaseJustChanged = true;
    m_justEnteredPhase = PHASE_BUILD;
    m_totalPausedMs = 0;
}

// ============================================================================
// Debug
// ============================================================================

void NarrativeEngine::printStatus() const {
    static const char* phaseNames[] = {"BUILD", "HOLD", "RELEASE", "REST"};

    Serial.println(F("\n=== NarrativeEngine Status ==="));
    Serial.print(F("Enabled: ")); Serial.println(m_enabled ? "YES" : "NO");
    Serial.print(F("Paused: ")); Serial.println(m_paused ? "YES" : "NO");

    if (m_enabled) {
        Serial.print(F("Phase: ")); Serial.println(phaseNames[m_cycle.getPhase()]);
        Serial.print(F("PhaseT: ")); Serial.println(getPhaseT(), 3);
        Serial.print(F("CycleT: ")); Serial.println(getCycleT(), 3);
        Serial.print(F("Intensity: ")); Serial.println(getIntensity(), 3);

        Serial.println(F("\nTimings:"));
        Serial.print(F("  Build: ")); Serial.print(m_cycle.buildDuration, 2); Serial.println("s");
        Serial.print(F("  Hold: ")); Serial.print(m_cycle.holdDuration, 2); Serial.println("s");
        Serial.print(F("  Release: ")); Serial.print(m_cycle.releaseDuration, 2); Serial.println("s");
        Serial.print(F("  Rest: ")); Serial.print(m_cycle.restDuration, 2); Serial.println("s");
        Serial.print(F("  Total: ")); Serial.print(m_cycle.getTotalDuration(), 2); Serial.println("s");

        Serial.println(F("\nZone Offsets:"));
        for (uint8_t i = 0; i < MAX_ZONES; i++) {
            Serial.print(F("  Zone ")); Serial.print(i);
            Serial.print(F(": ")); Serial.println(m_zoneOffsets[i], 3);
        }
    }
    Serial.println(F("==============================\n"));
}

// ============================================================================
// NVS Persistence
// ============================================================================

bool NarrativeEngine::saveToNVS() {
    // Ensure NVS is initialized
    if (!NVS_MANAGER.isInitialized()) {
        if (!NVS_MANAGER.init()) {
            Serial.println("[Narrative] ERROR: Failed to initialize NVS");
            return false;
        }
    }

    // Export current configuration
    NarrativeConfigData config;
    config.version = 1;
    config.buildDuration = m_cycle.buildDuration;
    config.holdDuration = m_cycle.holdDuration;
    config.releaseDuration = m_cycle.releaseDuration;
    config.restDuration = m_cycle.restDuration;
    config.buildCurve = static_cast<uint8_t>(m_cycle.buildCurve);
    config.releaseCurve = static_cast<uint8_t>(m_cycle.releaseCurve);
    config.holdBreathe = m_cycle.holdBreathe;
    config.snapAmount = m_cycle.snapAmount;
    config.durationVariance = m_cycle.durationVariance;
    config.enabled = m_enabled;
    
    // Calculate checksum
    config.calculateChecksum();

    // Save to NVS
    static constexpr const char* NVS_NAMESPACE = "narrative";
    static constexpr const char* NVS_KEY_CONFIG = "config";
    
    persistence::NVSResult result = NVS_MANAGER.saveBlob(NVS_NAMESPACE, NVS_KEY_CONFIG, &config, sizeof(config));

    if (result == persistence::NVSResult::OK) {
        Serial.println("[Narrative] Configuration saved to NVS");
        return true;
    } else {
        Serial.printf("[Narrative] ERROR: Save failed: %s\n",
                      persistence::NVSManager::resultToString(result));
        return false;
    }
}

bool NarrativeEngine::loadFromNVS() {
    // Ensure NVS is initialized
    if (!NVS_MANAGER.isInitialized()) {
        if (!NVS_MANAGER.init()) {
            Serial.println("[Narrative] ERROR: Failed to initialize NVS");
            return false;
        }
    }

    // Load from NVS
    NarrativeConfigData config;
    static constexpr const char* NVS_NAMESPACE = "narrative";
    static constexpr const char* NVS_KEY_CONFIG = "config";
    
    persistence::NVSResult result = NVS_MANAGER.loadBlob(NVS_NAMESPACE, NVS_KEY_CONFIG, &config, sizeof(config));

    if (result == persistence::NVSResult::NOT_FOUND) {
        Serial.println("[Narrative] No saved configuration found (first boot)");
        return false;
    }

    if (result != persistence::NVSResult::OK) {
        Serial.printf("[Narrative] ERROR: Load failed: %s\n",
                      persistence::NVSManager::resultToString(result));
        return false;
    }

    // Validate checksum
    if (!config.isValid()) {
        Serial.println("[Narrative] WARNING: Saved config checksum invalid");
        return false;
    }

    // Validate data ranges
    if (config.buildDuration <= 0.0f || config.releaseDuration <= 0.0f ||
        config.holdBreathe < 0.0f || config.holdBreathe > 1.0f ||
        config.snapAmount < 0.0f || config.snapAmount > 1.0f ||
        config.durationVariance < 0.0f || config.durationVariance > 1.0f) {
        Serial.println("[Narrative] WARNING: Saved config contains invalid values");
        return false;
    }

    // Validate curve values (EasingCurve enum should be 0-15 typically)
    if (config.buildCurve > 15 || config.releaseCurve > 15) {
        Serial.println("[Narrative] WARNING: Invalid curve values in saved config");
        return false;
    }

    // Apply loaded values
    m_cycle.buildDuration = config.buildDuration;
    m_cycle.holdDuration = max(0.0f, config.holdDuration);
    m_cycle.releaseDuration = config.releaseDuration;
    m_cycle.restDuration = max(0.0f, config.restDuration);
    m_cycle.buildCurve = static_cast<EasingCurve>(config.buildCurve);
    m_cycle.releaseCurve = static_cast<EasingCurve>(config.releaseCurve);
    m_cycle.holdBreathe = constrain(config.holdBreathe, 0.0f, 1.0f);
    m_cycle.snapAmount = constrain(config.snapAmount, 0.0f, 1.0f);
    m_cycle.durationVariance = constrain(config.durationVariance, 0.0f, 1.0f);
    
    // Apply enabled state (but don't auto-enable, let user control that)
    // The enabled state is loaded but we don't auto-enable on boot
    // User can enable via API if they want
    
    Serial.println("[Narrative] Configuration loaded from NVS");
    return true;
}

} // namespace narrative
} // namespace lightwaveos
