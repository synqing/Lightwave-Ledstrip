#include "ParameterSweeper.h"

// External global parameters (defined in main.cpp)
extern uint8_t brightnessVal;
extern uint8_t effectSpeed;
extern uint8_t effectIntensity;
extern uint8_t effectSaturation;
extern uint8_t effectComplexity;
extern uint8_t effectVariation;

// ============================================================================
// PARAMETER SWEEPER IMPLEMENTATION
// ============================================================================

ParameterSweeper::ParameterSweeper() {
    cancelAll();
}

bool ParameterSweeper::startSweep(ParamId param, uint8_t zone, uint8_t startVal,
                                   uint8_t targetVal, uint16_t durationMs) {
    if (durationMs == 0) {
        // Instant change
        applyValue(param, zone, targetVal);
        return true;
    }

    // Check if there's already a sweep for this param/zone and replace it
    int8_t slot = findSweep(param, zone);
    if (slot < 0) {
        slot = findFreeSlot();
    }

    if (slot < 0) {
        // No available slots - apply target immediately as fallback
        applyValue(param, zone, targetVal);
        return false;
    }

    // Set up the sweep
    m_sweeps[slot].paramId = static_cast<uint8_t>(param);
    m_sweeps[slot].targetZone = zone;
    m_sweeps[slot].startValue = startVal;
    m_sweeps[slot].targetValue = targetVal;
    m_sweeps[slot].startTimeMs = millis();
    m_sweeps[slot].durationMs = durationMs;

    return true;
}

bool ParameterSweeper::startSweepFromCurrent(ParamId param, uint8_t zone,
                                              uint8_t targetVal, uint16_t durationMs) {
    uint8_t currentVal = getCurrentParamValue(param, zone);
    return startSweep(param, zone, currentVal, targetVal, durationMs);
}

void ParameterSweeper::update(uint32_t currentTimeMs) {
    for (uint8_t i = 0; i < MAX_SWEEPS; i++) {
        if (!m_sweeps[i].isActive()) continue;

        // Get current interpolated value
        uint8_t value = m_sweeps[i].getCurrentValue(currentTimeMs);

        // Apply the value
        applyValue(static_cast<ParamId>(m_sweeps[i].paramId),
                   m_sweeps[i].targetZone, value);

        // Check if sweep is complete
        if (m_sweeps[i].isComplete(currentTimeMs)) {
            m_sweeps[i].clear();
        }
    }
}

void ParameterSweeper::cancelAll() {
    for (uint8_t i = 0; i < MAX_SWEEPS; i++) {
        m_sweeps[i].clear();
    }
}

void ParameterSweeper::cancelParam(ParamId param) {
    for (uint8_t i = 0; i < MAX_SWEEPS; i++) {
        if (m_sweeps[i].isActive() && m_sweeps[i].paramId == static_cast<uint8_t>(param)) {
            m_sweeps[i].clear();
        }
    }
}

void ParameterSweeper::cancelZone(uint8_t zone) {
    for (uint8_t i = 0; i < MAX_SWEEPS; i++) {
        if (m_sweeps[i].isActive() && m_sweeps[i].targetZone == zone) {
            m_sweeps[i].clear();
        }
    }
}

uint8_t ParameterSweeper::activeSweepCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_SWEEPS; i++) {
        if (m_sweeps[i].isActive()) count++;
    }
    return count;
}

void ParameterSweeper::applyValue(ParamId param, uint8_t zone, uint8_t value) {
    // For now, we only support global parameters
    // Zone-specific parameters would require ZoneComposer integration
    (void)zone;  // Unused for now

    switch (param) {
        case PARAM_BRIGHTNESS:
            brightnessVal = value;
            break;
        case PARAM_SPEED:
            effectSpeed = value;
            break;
        case PARAM_INTENSITY:
            effectIntensity = value;
            break;
        case PARAM_SATURATION:
            effectSaturation = value;
            break;
        case PARAM_COMPLEXITY:
            effectComplexity = value;
            break;
        case PARAM_VARIATION:
            effectVariation = value;
            break;
        default:
            break;
    }
}

uint8_t ParameterSweeper::getCurrentParamValue(ParamId param, uint8_t zone) {
    (void)zone;  // Global parameters for now

    switch (param) {
        case PARAM_BRIGHTNESS:
            return brightnessVal;
        case PARAM_SPEED:
            return effectSpeed;
        case PARAM_INTENSITY:
            return effectIntensity;
        case PARAM_SATURATION:
            return effectSaturation;
        case PARAM_COMPLEXITY:
            return effectComplexity;
        case PARAM_VARIATION:
            return effectVariation;
        default:
            return 128;  // Default mid-value
    }
}

int8_t ParameterSweeper::findFreeSlot() const {
    for (uint8_t i = 0; i < MAX_SWEEPS; i++) {
        if (!m_sweeps[i].isActive()) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;
}

int8_t ParameterSweeper::findSweep(ParamId param, uint8_t zone) const {
    for (uint8_t i = 0; i < MAX_SWEEPS; i++) {
        if (m_sweeps[i].isActive() &&
            m_sweeps[i].paramId == static_cast<uint8_t>(param) &&
            m_sweeps[i].targetZone == zone) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;
}
