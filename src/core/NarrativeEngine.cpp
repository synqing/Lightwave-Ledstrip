#include "NarrativeEngine.h"

#if FEATURE_NARRATIVE_ENGINE

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
{
    // Configure default 4-second cycle
    m_cycle.buildDuration = 1.5f;
    m_cycle.holdDuration = 0.5f;
    m_cycle.releaseDuration = 1.5f;
    m_cycle.restDuration = 0.5f;
    m_cycle.buildCurve = EASE_IN_QUAD;
    m_cycle.releaseCurve = EASE_OUT_QUAD;
    m_cycle.holdBreathe = 0.1f;    // Subtle pulse at peak
    m_cycle.snapAmount = 0.0f;
    m_cycle.durationVariance = 0.0f;

    // All zone offsets start at 0 (synchronized)
    for (uint8_t i = 0; i < HardwareConfig::MAX_ZONES; i++) {
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

    // Store previous phase for edge detection
    NarrativePhase previousPhase = m_cycle.getPhase();

    // Update the internal cycle
    m_cycle.update();

    // Detect phase transitions
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
// Configuration - Phase Durations
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
    // Scale all phases proportionally to hit target total
    float currentTotal = m_cycle.getTotalDuration();
    if (currentTotal <= 0.0f || totalCycleDuration <= 0.0f) return;

    float scale = totalCycleDuration / currentTotal;
    m_cycle.buildDuration *= scale;
    m_cycle.holdDuration *= scale;
    m_cycle.releaseDuration *= scale;
    m_cycle.restDuration *= scale;
}

// ============================================================================
// Configuration - Curve Behavior
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
    if (zoneId >= HardwareConfig::MAX_ZONES) return;
    // Wrap to 0-1 range
    offsetRatio = fmod(offsetRatio, 1.0f);
    if (offsetRatio < 0.0f) offsetRatio += 1.0f;
    m_zoneOffsets[zoneId] = offsetRatio;
}

float NarrativeEngine::getZonePhaseOffset(uint8_t zoneId) const {
    if (zoneId >= HardwareConfig::MAX_ZONES) return 0.0f;
    return m_zoneOffsets[zoneId];
}

// ============================================================================
// Query Methods - Global (no zone offset)
// ============================================================================

float NarrativeEngine::getIntensity() const {
    if (!m_enabled) return 1.0f;  // Full brightness when disabled
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
// Query Methods - Zone-specific (applies phase offset)
// ============================================================================

float NarrativeEngine::getIntensity(uint8_t zoneId) const {
    if (!m_enabled) return 1.0f;
    if (zoneId >= HardwareConfig::MAX_ZONES) return getIntensity();

    float baseCycleT = getCycleT();
    float offset = m_zoneOffsets[zoneId];
    float offsetCycleT = fmod(baseCycleT + offset, 1.0f);

    return getIntensityAtCycleT(offsetCycleT);
}

NarrativePhase NarrativeEngine::getPhase(uint8_t zoneId) const {
    if (!m_enabled) return PHASE_HOLD;
    if (zoneId >= HardwareConfig::MAX_ZONES) return getPhase();

    float baseCycleT = getCycleT();
    float offset = m_zoneOffsets[zoneId];
    float offsetCycleT = fmod(baseCycleT + offset, 1.0f);

    return getPhaseAtCycleT(offsetCycleT);
}

float NarrativeEngine::getPhaseT(uint8_t zoneId) const {
    if (!m_enabled) return 1.0f;
    if (zoneId >= HardwareConfig::MAX_ZONES) return getPhaseT();

    float baseCycleT = getCycleT();
    float offset = m_zoneOffsets[zoneId];
    float offsetCycleT = fmod(baseCycleT + offset, 1.0f);

    return getPhaseTAtCycleT(offsetCycleT);
}

float NarrativeEngine::getCycleT(uint8_t zoneId) const {
    if (!m_enabled) return 0.0f;
    if (zoneId >= HardwareConfig::MAX_ZONES) return getCycleT();

    float baseCycleT = getCycleT();
    float offset = m_zoneOffsets[zoneId];
    return fmod(baseCycleT + offset, 1.0f);
}

// ============================================================================
// Internal - Calculate values at arbitrary cycle position
// ============================================================================

float NarrativeEngine::getIntensityAtCycleT(float cycleT) const {
    // Get phase and phase progress at this cycle position
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

    // Apply snap compression if enabled
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
    // REST phase
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
        // Adjust cycle timestamps to account for pause
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
    const char* phaseNames[] = {"BUILD", "HOLD", "RELEASE", "REST"};

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
        for (uint8_t i = 0; i < HardwareConfig::MAX_ZONES; i++) {
            Serial.print(F("  Zone ")); Serial.print(i);
            Serial.print(F(": ")); Serial.println(m_zoneOffsets[i], 3);
        }
    }
    Serial.println(F("==============================\n"));
}

#endif // FEATURE_NARRATIVE_ENGINE
