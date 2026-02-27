/**
 * @file AudioEffectMapping.cpp
 * @brief Audio-to-Visual Parameter Mapping System Implementation
 *
 * LightwaveOS v2 - Phase 4 Audio API Enhancement
 */

#include "AudioEffectMapping.h"

#if FEATURE_AUDIO_SYNC

#include <cstring>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <esp_heap_caps.h>
#define LW_LOG_TAG "AudioMapping"
#include "../../utils/Log.h"
#else
#include <chrono>
#include <cstdlib>
#endif

namespace lightwaveos {
namespace audio {

namespace {
#if defined(NATIVE_BUILD)
using AllocFn = void* (*)(size_t, size_t);
static AllocFn s_testAllocFn = nullptr;

static void* allocZeroed(size_t count, size_t size) {
    if (s_testAllocFn) {
        return s_testAllocFn(count, size);
    }
    return std::calloc(count, size);
}

static uint32_t lw_micros() {
    using namespace std::chrono;
    static const steady_clock::time_point start = steady_clock::now();
    return static_cast<uint32_t>(duration_cast<microseconds>(steady_clock::now() - start).count());
}
#else
static void* allocZeroed(size_t count, size_t size) {
    return heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static uint32_t lw_micros() { return micros(); }
#endif
} // namespace

// =============================================================================
// CURVE APPLICATION FUNCTIONS
// =============================================================================

float AudioParameterMapping::applyCurve(float normalizedInput) const {
    // Clamp input to [0,1]
    float x = normalizedInput;
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;

    switch (curve) {
        case MappingCurve::LINEAR:
            return x;

        case MappingCurve::SQUARED:
            // Gentle start, aggressive end
            return x * x;

        case MappingCurve::SQRT:
            // Aggressive start, gentle end
            return sqrtf(x);

        case MappingCurve::LOG:
            // Logarithmic: y = log(x+1)/log(2)
            // Maps [0,1] -> [0,1] with compression at high values
            return log1pf(x) / 0.693147f;  // log(2) ≈ 0.693147

        case MappingCurve::EXP:
            // Exponential: y = (e^x - 1)/(e - 1)
            // Maps [0,1] -> [0,1] with expansion at high values
            return (expf(x) - 1.0f) / 1.718282f;  // e - 1 ≈ 1.718282

        case MappingCurve::INVERTED:
            return 1.0f - x;

        default:
            return x;
    }
}

float AudioParameterMapping::apply(float rawInput) const {
    if (!enabled || source == AudioSource::NONE || target == VisualTarget::NONE) {
        return 0.0f;
    }

    // Apply gain
    float scaled = rawInput * gain;

    // Normalize to [0,1] based on input range
    float range = inputMax - inputMin;
    if (range < 0.0001f) range = 1.0f;  // Prevent division by zero

    float normalized = (scaled - inputMin) / range;

    // Clamp to [0,1]
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    // Apply curve
    float curved = applyCurve(normalized);

    // Map to output range
    return outputMin + curved * (outputMax - outputMin);
}

void AudioParameterMapping::updateSmoothed(float rawInput, float dtSeconds) {
    if (!enabled) return;

    float targetValue = apply(rawInput);

    // dt-corrected IIR: alpha = 1 - exp(-dt/tau) for frame-rate-independent smoothing
    if (dtSeconds <= 0.0f) dtSeconds = 1.0f / 120.0f;
    float tau = (tauSeconds > 0.0001f) ? tauSeconds : 0.15f;
    float alpha = 1.0f - expf(-dtSeconds / tau);
    if (alpha < 0.05f) alpha = 0.05f;
    if (alpha > 0.95f) alpha = 0.95f;

    smoothedValue = alpha * targetValue + (1.0f - alpha) * smoothedValue;
}

float AudioParameterMapping::getSmoothedOutput() const {
    return smoothedValue;
}

// =============================================================================
// EFFECT AUDIO MAPPING IMPLEMENTATION
// =============================================================================

void EffectAudioMapping::calculateChecksum() {
    // CRC32 of all data except checksum field itself
    const size_t dataSize = offsetof(EffectAudioMapping, checksum);
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(this);

    for (size_t i = 0; i < dataSize; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }

    checksum = crc ^ 0xFFFFFFFF;
}

bool EffectAudioMapping::isValid() const {
    if (version != VERSION) return false;

    // Recalculate and compare checksum
    EffectAudioMapping temp = *this;
    temp.calculateChecksum();
    return (temp.checksum == checksum);
}

const AudioParameterMapping* EffectAudioMapping::findMapping(VisualTarget target) const {
    for (uint8_t i = 0; i < mappingCount && i < MAX_MAPPINGS_PER_EFFECT; i++) {
        if (mappings[i].target == target && mappings[i].enabled) {
            return &mappings[i];
        }
    }
    return nullptr;
}

AudioParameterMapping* EffectAudioMapping::findMapping(VisualTarget target) {
    for (uint8_t i = 0; i < mappingCount && i < MAX_MAPPINGS_PER_EFFECT; i++) {
        if (mappings[i].target == target && mappings[i].enabled) {
            return &mappings[i];
        }
    }
    return nullptr;
}

const AudioParameterMapping* EffectAudioMapping::findMappingBySourceTarget(AudioSource source, VisualTarget target) const {
    for (uint8_t i = 0; i < mappingCount && i < MAX_MAPPINGS_PER_EFFECT; i++) {
        if (mappings[i].source == source && mappings[i].target == target) {
            return &mappings[i];
        }
    }
    return nullptr;
}

AudioParameterMapping* EffectAudioMapping::findMappingBySourceTarget(AudioSource source, VisualTarget target) {
    for (uint8_t i = 0; i < mappingCount && i < MAX_MAPPINGS_PER_EFFECT; i++) {
        if (mappings[i].source == source && mappings[i].target == target) {
            return &mappings[i];
        }
    }
    return nullptr;
}

bool EffectAudioMapping::addMapping(const AudioParameterMapping& mapping) {
    AudioParameterMapping* existing = findMappingBySourceTarget(mapping.source, mapping.target);
    if (existing) {
        *existing = mapping;
        calculateChecksum();
        return true;
    }

    if (mappingCount >= MAX_MAPPINGS_PER_EFFECT) {
        return false;
    }

    mappings[mappingCount++] = mapping;
    calculateChecksum();
    return true;
}

bool EffectAudioMapping::removeMapping(VisualTarget target) {
    for (uint8_t i = 0; i < mappingCount; i++) {
        if (mappings[i].target == target) {
            // Shift remaining mappings down
            for (uint8_t j = i; j < mappingCount - 1; j++) {
                mappings[j] = mappings[j + 1];
            }
            mappingCount--;

            // Clear the last slot
            mappings[mappingCount] = AudioParameterMapping();

            calculateChecksum();
            return true;
        }
    }
    return false;
}

void EffectAudioMapping::clearMappings() {
    mappingCount = 0;
    for (uint8_t i = 0; i < MAX_MAPPINGS_PER_EFFECT; i++) {
        mappings[i] = AudioParameterMapping();
    }
    calculateChecksum();
}

// =============================================================================
// AUDIO MAPPING REGISTRY IMPLEMENTATION
// =============================================================================

AudioMappingRegistry& AudioMappingRegistry::instance() {
    static AudioMappingRegistry inst;
    return inst;
}

bool AudioMappingRegistry::begin() {
    if (m_ready && m_mappings != nullptr) {
        return true;
    }

    const size_t bytes = static_cast<size_t>(MAX_EFFECTS) * sizeof(EffectAudioMapping);
    auto* table = static_cast<EffectAudioMapping*>(allocZeroed(1, bytes));
    if (!table) {
#ifndef NATIVE_BUILD
        if (!m_allocFailureLogged) {
            LW_LOGW("PSRAM allocation failed for mappings table (%u bytes) - disabling audio mappings",
                    static_cast<unsigned>(bytes));
            m_allocFailureLogged = true;
        }
#endif
        m_mappings = nullptr;
        m_ready = false;
        return false;
    }

    m_mappings = table;

    // Initialise all slots as empty (INVALID_EFFECT_ID).
    // Slots are claimed on-demand by findOrClaimSlot().
    for (uint16_t i = 0; i < MAX_EFFECTS; i++) {
        m_mappings[i] = EffectAudioMapping();
        // effectId defaults to INVALID_EFFECT_ID via struct initialiser
        m_mappings[i].calculateChecksum();
    }

    m_ready = true;
    return true;
}

#if defined(NATIVE_BUILD)
void AudioMappingRegistry::setTestAllocator(void* (*allocFn)(size_t count, size_t size)) {
    s_testAllocFn = allocFn;
}
#endif

// ---------------------------------------------------------------------------
// Slot lookup (linear scan by effectId field)
// ---------------------------------------------------------------------------

int16_t AudioMappingRegistry::findSlot(EffectId effectId) const {
    if (!m_mappings || effectId == INVALID_EFFECT_ID) return -1;
    for (uint16_t i = 0; i < MAX_EFFECTS; i++) {
        if (m_mappings[i].effectId == effectId) return static_cast<int16_t>(i);
    }
    return -1;
}

int16_t AudioMappingRegistry::findOrClaimSlot(EffectId effectId) {
    if (!m_mappings || effectId == INVALID_EFFECT_ID) return -1;
    int16_t firstFree = -1;
    for (uint16_t i = 0; i < MAX_EFFECTS; i++) {
        if (m_mappings[i].effectId == effectId) return static_cast<int16_t>(i);
        if (firstFree < 0 && m_mappings[i].effectId == INVALID_EFFECT_ID) {
            firstFree = static_cast<int16_t>(i);
        }
    }
    // Claim the first free slot
    if (firstFree >= 0) {
        m_mappings[firstFree].effectId = effectId;
        m_mappings[firstFree].calculateChecksum();
    }
    return firstFree;
}

// ---------------------------------------------------------------------------
// Public API — all use linear-scan findSlot / findOrClaimSlot
// ---------------------------------------------------------------------------

const EffectAudioMapping* AudioMappingRegistry::getMapping(EffectId effectId) const {
    if (!m_ready || m_mappings == nullptr) return nullptr;
    int16_t slot = findSlot(effectId);
    if (slot < 0) return nullptr;
    return &m_mappings[slot];
}

EffectAudioMapping* AudioMappingRegistry::getMapping(EffectId effectId) {
    if (!m_ready || m_mappings == nullptr) return nullptr;
    int16_t slot = findSlot(effectId);
    if (slot < 0) return nullptr;
    return &m_mappings[slot];
}

bool AudioMappingRegistry::setMapping(EffectId effectId, const EffectAudioMapping& config) {
    if (!m_ready || m_mappings == nullptr) return false;
    int16_t slot = findOrClaimSlot(effectId);
    if (slot < 0) return false;

    float savedSmoothed[EffectAudioMapping::MAX_MAPPINGS_PER_EFFECT];
    for (uint8_t i = 0; i < EffectAudioMapping::MAX_MAPPINGS_PER_EFFECT; i++) {
        savedSmoothed[i] = m_mappings[slot].mappings[i].smoothedValue;
    }

    m_mappings[slot] = config;
    m_mappings[slot].effectId = effectId;

    for (uint8_t i = 0; i < EffectAudioMapping::MAX_MAPPINGS_PER_EFFECT; i++) {
        m_mappings[slot].mappings[i].smoothedValue = savedSmoothed[i];
    }
    m_mappings[slot].calculateChecksum();
    return true;
}

void AudioMappingRegistry::setEffectMappingEnabled(EffectId effectId, bool enabled) {
    if (!m_ready || m_mappings == nullptr) return;
    int16_t slot = findOrClaimSlot(effectId);
    if (slot < 0) return;
    m_mappings[slot].globalEnabled = enabled;
    m_mappings[slot].calculateChecksum();
}

bool AudioMappingRegistry::hasActiveMappings(EffectId effectId) const {
    if (!m_ready || m_mappings == nullptr) return false;
    int16_t slot = findSlot(effectId);
    if (slot < 0) return false;
    const EffectAudioMapping& mapping = m_mappings[slot];
    return mapping.globalEnabled && mapping.mappingCount > 0;
}

uint16_t AudioMappingRegistry::getActiveEffectCount() const {
    if (!m_ready || m_mappings == nullptr) return 0;
    uint16_t count = 0;
    for (uint16_t i = 0; i < MAX_EFFECTS; i++) {
        if (m_mappings[i].effectId != INVALID_EFFECT_ID &&
            m_mappings[i].globalEnabled && m_mappings[i].mappingCount > 0) {
            count++;
        }
    }
    return count;
}

uint16_t AudioMappingRegistry::getTotalMappingCount() const {
    if (!m_ready || m_mappings == nullptr) return 0;
    uint16_t count = 0;
    for (uint16_t i = 0; i < MAX_EFFECTS; i++) {
        if (m_mappings[i].effectId != INVALID_EFFECT_ID && m_mappings[i].globalEnabled) {
            count += m_mappings[i].mappingCount;
        }
    }
    return count;
}

// =============================================================================
// AUDIO VALUE EXTRACTION
// =============================================================================

float AudioMappingRegistry::getAudioValue(
    AudioSource source,
    const ControlBusFrame& bus,
    const MusicalGridSnapshot& grid
) {
    switch (source) {
        // Energy metrics
        case AudioSource::RMS:
            return bus.rms;
        case AudioSource::FAST_RMS:
            return bus.fast_rms;
        case AudioSource::FLUX:
            return bus.flux;
        case AudioSource::FAST_FLUX:
            return bus.fast_flux;

        // Individual frequency bands
        case AudioSource::BAND_0:
            return bus.bands[0];
        case AudioSource::BAND_1:
            return bus.bands[1];
        case AudioSource::BAND_2:
            return bus.bands[2];
        case AudioSource::BAND_3:
            return bus.bands[3];
        case AudioSource::BAND_4:
            return bus.bands[4];
        case AudioSource::BAND_5:
            return bus.bands[5];
        case AudioSource::BAND_6:
            return bus.bands[6];
        case AudioSource::BAND_7:
            return bus.bands[7];

        // Aggregated bands
        case AudioSource::BASS:
            return (bus.bands[0] + bus.bands[1]) * 0.5f;
        case AudioSource::MID:
            return (bus.bands[2] + bus.bands[3] + bus.bands[4]) / 3.0f;
        case AudioSource::TREBLE:
            return (bus.bands[5] + bus.bands[6] + bus.bands[7]) / 3.0f;
        case AudioSource::HEAVY_BASS: {
            float bass = (bus.bands[0] + bus.bands[1]) * 0.5f;
            return bass * bass;  // Squared response
        }

        // Musical timing
        case AudioSource::BEAT_PHASE:
            return grid.beat_phase01;
        case AudioSource::BPM:
            return grid.bpm_smoothed;  // Note: range is 30-300, not 0-1
        case AudioSource::TEMPO_CONFIDENCE:
            return grid.tempo_confidence;

        case AudioSource::NONE:
        default:
            return 0.0f;
    }
}

// =============================================================================
// RUNTIME APPLICATION
// =============================================================================

void AudioMappingRegistry::applySingleMapping(
    AudioParameterMapping& mapping,
    float audioValue,
    float dtSeconds,
    uint8_t& targetValue,
    uint8_t minVal,
    uint8_t maxVal
) {
    // Update smoothed value (dt-corrected)
    mapping.updateSmoothed(audioValue, dtSeconds);

    // Get output value
    float output = mapping.getSmoothedOutput();

    // Clamp to valid range
    if (output < minVal) output = minVal;
    if (output > maxVal) output = maxVal;

    if (mapping.additive) {
        // Additive mode: add to existing value
        int newVal = (int)targetValue + (int)output;
        if (newVal > maxVal) newVal = maxVal;
        if (newVal < minVal) newVal = minVal;
        targetValue = (uint8_t)newVal;
    } else {
        // Replace mode
        targetValue = (uint8_t)output;
    }
}

void AudioMappingRegistry::applyMappings(
    EffectId effectId,
    const ControlBusFrame& bus,
    const MusicalGridSnapshot& grid,
    bool audioAvailable,
    float dtSeconds,
    uint8_t& brightness,
    uint8_t& speed,
    uint8_t& intensity,
    uint8_t& saturation,
    uint8_t& complexity,
    uint8_t& variation,
    uint8_t& hue
) {
    if (!m_ready || m_mappings == nullptr) return;

    int16_t slot = findSlot(effectId);
    if (slot < 0) return;

    EffectAudioMapping& config = m_mappings[slot];
    if (!config.globalEnabled || config.mappingCount == 0) return;

    // Performance instrumentation
    uint32_t startMicros = lw_micros();

    if (dtSeconds <= 0.0f) dtSeconds = 1.0f / 120.0f;

    if (audioAvailable) {
        // Process each mapping with live audio
        for (uint8_t i = 0; i < config.mappingCount && i < EffectAudioMapping::MAX_MAPPINGS_PER_EFFECT; i++) {
            AudioParameterMapping& mapping = config.mappings[i];

            if (!mapping.enabled || mapping.source == AudioSource::NONE) continue;

            float audioValue = getAudioValue(mapping.source, bus, grid);

            switch (mapping.target) {
                case VisualTarget::BRIGHTNESS:
                    applySingleMapping(mapping, audioValue, dtSeconds, brightness, 0, 160);
                    break;
                case VisualTarget::SPEED:
                    applySingleMapping(mapping, audioValue, dtSeconds, speed, 1, 50);
                    break;
                case VisualTarget::INTENSITY:
                    applySingleMapping(mapping, audioValue, dtSeconds, intensity, 0, 255);
                    break;
                case VisualTarget::SATURATION:
                    applySingleMapping(mapping, audioValue, dtSeconds, saturation, 0, 255);
                    break;
                case VisualTarget::COMPLEXITY:
                    applySingleMapping(mapping, audioValue, dtSeconds, complexity, 0, 255);
                    break;
                case VisualTarget::VARIATION:
                    applySingleMapping(mapping, audioValue, dtSeconds, variation, 0, 255);
                    break;
                case VisualTarget::HUE:
                    applySingleMapping(mapping, audioValue, dtSeconds, hue, 0, 255);
                    break;
                case VisualTarget::NONE:
                default:
                    break;
            }
        }
    } else {
        // Audio absent: decay smoothed values toward outputMin then apply to targets
        constexpr float kDecayTauSeconds = 0.3f;
        float decayAlpha = 1.0f - expf(-dtSeconds / kDecayTauSeconds);
        if (decayAlpha < 0.01f) decayAlpha = 0.01f;
        if (decayAlpha > 0.5f) decayAlpha = 0.5f;

        for (uint8_t i = 0; i < config.mappingCount && i < EffectAudioMapping::MAX_MAPPINGS_PER_EFFECT; i++) {
            AudioParameterMapping& mapping = config.mappings[i];
            if (!mapping.enabled || mapping.target == VisualTarget::NONE) continue;

            float rest = mapping.outputMin;
            mapping.smoothedValue += (rest - mapping.smoothedValue) * decayAlpha;
            if (mapping.smoothedValue < mapping.outputMin) mapping.smoothedValue = mapping.outputMin;
            if (mapping.smoothedValue > mapping.outputMax) mapping.smoothedValue = mapping.outputMax;

            float output = mapping.getSmoothedOutput();
            uint8_t minVal = 0, maxVal = 255;
            uint8_t* pTarget = nullptr;
            switch (mapping.target) {
                case VisualTarget::BRIGHTNESS: minVal = 0; maxVal = 160; pTarget = &brightness; break;
                case VisualTarget::SPEED:      minVal = 1; maxVal = 50;  pTarget = &speed; break;
                case VisualTarget::INTENSITY:  minVal = 0; maxVal = 255; pTarget = &intensity; break;
                case VisualTarget::SATURATION: minVal = 0; maxVal = 255; pTarget = &saturation; break;
                case VisualTarget::COMPLEXITY: minVal = 0; maxVal = 255; pTarget = &complexity; break;
                case VisualTarget::VARIATION:  minVal = 0; maxVal = 255; pTarget = &variation; break;
                case VisualTarget::HUE:       minVal = 0; maxVal = 255; pTarget = &hue; break;
                default: break;
            }
            if (pTarget) {
                if (output < minVal) output = static_cast<float>(minVal);
                if (output > maxVal) output = static_cast<float>(maxVal);
                if (mapping.additive) {
                    int newVal = (int)*pTarget + (int)(output + 0.5f);
                    if (newVal > maxVal) newVal = maxVal;
                    if (newVal < minVal) newVal = minVal;
                    *pTarget = (uint8_t)newVal;
                } else {
                    *pTarget = (uint8_t)(output + 0.5f);
                }
            }
        }
    }

    // Record performance
    m_lastApplyMicros = lw_micros() - startMicros;
    m_applyCount++;

    if (m_lastApplyMicros > m_maxApplyMicros) {
        m_maxApplyMicros = m_lastApplyMicros;
    }
    m_totalApplyMicros += m_lastApplyMicros;
}

void AudioMappingRegistry::resetStats() {
    m_applyCount = 0;
    m_lastApplyMicros = 0;
    m_maxApplyMicros = 0;
    m_totalApplyMicros = 0;
}

// =============================================================================
// STRING UTILITIES
// =============================================================================

const char* AudioMappingRegistry::getSourceName(AudioSource source) {
    switch (source) {
        case AudioSource::RMS: return "RMS";
        case AudioSource::FAST_RMS: return "FAST_RMS";
        case AudioSource::FLUX: return "FLUX";
        case AudioSource::FAST_FLUX: return "FAST_FLUX";
        case AudioSource::BAND_0: return "BAND_0";
        case AudioSource::BAND_1: return "BAND_1";
        case AudioSource::BAND_2: return "BAND_2";
        case AudioSource::BAND_3: return "BAND_3";
        case AudioSource::BAND_4: return "BAND_4";
        case AudioSource::BAND_5: return "BAND_5";
        case AudioSource::BAND_6: return "BAND_6";
        case AudioSource::BAND_7: return "BAND_7";
        case AudioSource::BASS: return "BASS";
        case AudioSource::MID: return "MID";
        case AudioSource::TREBLE: return "TREBLE";
        case AudioSource::HEAVY_BASS: return "HEAVY_BASS";
        case AudioSource::BEAT_PHASE: return "BEAT_PHASE";
        case AudioSource::BPM: return "BPM";
        case AudioSource::TEMPO_CONFIDENCE: return "TEMPO_CONFIDENCE";
        case AudioSource::NONE:
        default: return "NONE";
    }
}

const char* AudioMappingRegistry::getTargetName(VisualTarget target) {
    switch (target) {
        case VisualTarget::BRIGHTNESS: return "BRIGHTNESS";
        case VisualTarget::SPEED: return "SPEED";
        case VisualTarget::INTENSITY: return "INTENSITY";
        case VisualTarget::SATURATION: return "SATURATION";
        case VisualTarget::COMPLEXITY: return "COMPLEXITY";
        case VisualTarget::VARIATION: return "VARIATION";
        case VisualTarget::HUE: return "HUE";
        case VisualTarget::NONE:
        default: return "NONE";
    }
}

const char* AudioMappingRegistry::getCurveName(MappingCurve curve) {
    switch (curve) {
        case MappingCurve::LINEAR: return "LINEAR";
        case MappingCurve::SQUARED: return "SQUARED";
        case MappingCurve::SQRT: return "SQRT";
        case MappingCurve::LOG: return "LOG";
        case MappingCurve::EXP: return "EXP";
        case MappingCurve::INVERTED: return "INVERTED";
        default: return "LINEAR";
    }
}

AudioSource AudioMappingRegistry::parseSource(const char* name) {
    if (!name) return AudioSource::NONE;

    if (strcmp(name, "RMS") == 0) return AudioSource::RMS;
    if (strcmp(name, "FAST_RMS") == 0) return AudioSource::FAST_RMS;
    if (strcmp(name, "FLUX") == 0) return AudioSource::FLUX;
    if (strcmp(name, "FAST_FLUX") == 0) return AudioSource::FAST_FLUX;
    if (strcmp(name, "BAND_0") == 0) return AudioSource::BAND_0;
    if (strcmp(name, "BAND_1") == 0) return AudioSource::BAND_1;
    if (strcmp(name, "BAND_2") == 0) return AudioSource::BAND_2;
    if (strcmp(name, "BAND_3") == 0) return AudioSource::BAND_3;
    if (strcmp(name, "BAND_4") == 0) return AudioSource::BAND_4;
    if (strcmp(name, "BAND_5") == 0) return AudioSource::BAND_5;
    if (strcmp(name, "BAND_6") == 0) return AudioSource::BAND_6;
    if (strcmp(name, "BAND_7") == 0) return AudioSource::BAND_7;
    if (strcmp(name, "BASS") == 0) return AudioSource::BASS;
    if (strcmp(name, "MID") == 0) return AudioSource::MID;
    if (strcmp(name, "TREBLE") == 0) return AudioSource::TREBLE;
    if (strcmp(name, "HEAVY_BASS") == 0) return AudioSource::HEAVY_BASS;
    if (strcmp(name, "BEAT_PHASE") == 0) return AudioSource::BEAT_PHASE;
    if (strcmp(name, "BPM") == 0) return AudioSource::BPM;
    if (strcmp(name, "TEMPO_CONFIDENCE") == 0) return AudioSource::TEMPO_CONFIDENCE;

    return AudioSource::NONE;
}

VisualTarget AudioMappingRegistry::parseTarget(const char* name) {
    if (!name) return VisualTarget::NONE;

    if (strcmp(name, "BRIGHTNESS") == 0) return VisualTarget::BRIGHTNESS;
    if (strcmp(name, "SPEED") == 0) return VisualTarget::SPEED;
    if (strcmp(name, "INTENSITY") == 0) return VisualTarget::INTENSITY;
    if (strcmp(name, "SATURATION") == 0) return VisualTarget::SATURATION;
    if (strcmp(name, "COMPLEXITY") == 0) return VisualTarget::COMPLEXITY;
    if (strcmp(name, "VARIATION") == 0) return VisualTarget::VARIATION;
    if (strcmp(name, "HUE") == 0) return VisualTarget::HUE;

    return VisualTarget::NONE;
}

MappingCurve AudioMappingRegistry::parseCurve(const char* name) {
    if (!name) return MappingCurve::LINEAR;

    if (strcmp(name, "LINEAR") == 0) return MappingCurve::LINEAR;
    if (strcmp(name, "SQUARED") == 0) return MappingCurve::SQUARED;
    if (strcmp(name, "SQRT") == 0) return MappingCurve::SQRT;
    if (strcmp(name, "LOG") == 0) return MappingCurve::LOG;
    if (strcmp(name, "EXP") == 0) return MappingCurve::EXP;
    if (strcmp(name, "INVERTED") == 0) return MappingCurve::INVERTED;

    return MappingCurve::LINEAR;
}

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
