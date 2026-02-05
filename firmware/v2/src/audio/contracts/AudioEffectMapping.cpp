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

void AudioParameterMapping::updateSmoothed(float rawInput) {
    if (!enabled) return;

    float targetValue = apply(rawInput);

    // IIR smoothing: smoothed = alpha * new + (1-alpha) * old
    // Higher alpha = faster response, lower alpha = more smoothing
    float alpha = smoothingAlpha;
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

bool EffectAudioMapping::addMapping(const AudioParameterMapping& mapping) {
    // Check if mapping for this target already exists
    AudioParameterMapping* existing = findMapping(mapping.target);
    if (existing) {
        *existing = mapping;
        calculateChecksum();
        return true;
    }

    // Add new mapping if space available
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

    // Initialise defaults using in-struct initialisers (not preserved by calloc()).
    for (uint8_t i = 0; i < MAX_EFFECTS; i++) {
        m_mappings[i] = EffectAudioMapping();
        m_mappings[i].effectId = i;
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

const EffectAudioMapping* AudioMappingRegistry::getMapping(uint8_t effectId) const {
    if (!m_ready || m_mappings == nullptr) return nullptr;
    if (effectId >= MAX_EFFECTS) return nullptr;
    return &m_mappings[effectId];
}

EffectAudioMapping* AudioMappingRegistry::getMapping(uint8_t effectId) {
    if (!m_ready || m_mappings == nullptr) return nullptr;
    if (effectId >= MAX_EFFECTS) return nullptr;
    return &m_mappings[effectId];
}

bool AudioMappingRegistry::setMapping(uint8_t effectId, const EffectAudioMapping& config) {
    if (!m_ready || m_mappings == nullptr) return false;
    if (effectId >= MAX_EFFECTS) return false;

    m_mappings[effectId] = config;
    m_mappings[effectId].effectId = effectId;
    m_mappings[effectId].calculateChecksum();
    return true;
}

void AudioMappingRegistry::setEffectMappingEnabled(uint8_t effectId, bool enabled) {
    if (!m_ready || m_mappings == nullptr) return;
    if (effectId >= MAX_EFFECTS) return;
    m_mappings[effectId].globalEnabled = enabled;
    m_mappings[effectId].calculateChecksum();
}

bool AudioMappingRegistry::hasActiveMappings(uint8_t effectId) const {
    if (!m_ready || m_mappings == nullptr) return false;
    if (effectId >= MAX_EFFECTS) return false;
    const EffectAudioMapping& mapping = m_mappings[effectId];
    return mapping.globalEnabled && mapping.mappingCount > 0;
}

uint8_t AudioMappingRegistry::getActiveEffectCount() const {
    if (!m_ready || m_mappings == nullptr) return 0;
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_EFFECTS; i++) {
        if (hasActiveMappings(i)) count++;
    }
    return count;
}

uint16_t AudioMappingRegistry::getTotalMappingCount() const {
    if (!m_ready || m_mappings == nullptr) return 0;
    uint16_t count = 0;
    for (uint8_t i = 0; i < MAX_EFFECTS; i++) {
        if (m_mappings[i].globalEnabled) {
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
    uint8_t& targetValue,
    uint8_t minVal,
    uint8_t maxVal
) {
    // Update smoothed value
    mapping.updateSmoothed(audioValue);

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
    uint8_t effectId,
    const ControlBusFrame& bus,
    const MusicalGridSnapshot& grid,
    bool audioAvailable,
    uint8_t& brightness,
    uint8_t& speed,
    uint8_t& intensity,
    uint8_t& saturation,
    uint8_t& complexity,
    uint8_t& variation,
    uint8_t& hue
) {
    if (!m_ready || m_mappings == nullptr) return;
    if (effectId >= MAX_EFFECTS) return;

    EffectAudioMapping& config = m_mappings[effectId];
    if (!config.globalEnabled || config.mappingCount == 0) return;

    // Performance instrumentation
    uint32_t startMicros = lw_micros();

    // If no fresh audio, skip (keep previous smoothed values)
    if (!audioAvailable) {
        m_lastApplyMicros = lw_micros() - startMicros;
        m_applyCount++;
        return;
    }

    // Process each mapping
    for (uint8_t i = 0; i < config.mappingCount && i < EffectAudioMapping::MAX_MAPPINGS_PER_EFFECT; i++) {
        AudioParameterMapping& mapping = config.mappings[i];

        if (!mapping.enabled || mapping.source == AudioSource::NONE) continue;

        // Get audio value for this source
        float audioValue = getAudioValue(mapping.source, bus, grid);

        // Apply to target
        switch (mapping.target) {
            case VisualTarget::BRIGHTNESS:
                applySingleMapping(mapping, audioValue, brightness, 0, 160);
                break;
            case VisualTarget::SPEED:
                applySingleMapping(mapping, audioValue, speed, 1, 50);
                break;
            case VisualTarget::INTENSITY:
                applySingleMapping(mapping, audioValue, intensity, 0, 255);
                break;
            case VisualTarget::SATURATION:
                applySingleMapping(mapping, audioValue, saturation, 0, 255);
                break;
            case VisualTarget::COMPLEXITY:
                applySingleMapping(mapping, audioValue, complexity, 0, 255);
                break;
            case VisualTarget::VARIATION:
                applySingleMapping(mapping, audioValue, variation, 0, 255);
                break;
            case VisualTarget::HUE:
                applySingleMapping(mapping, audioValue, hue, 0, 255);
                break;
            case VisualTarget::NONE:
            default:
                break;
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
