/**
 * @file AudioTuning.h
 * @brief Runtime-tunable audio pipeline and contract parameters
 *
 * This keeps DSP tuning adjustable via API without recompiling.
 * Values are clamped to safe ranges to avoid unstable behaviour.
 */

#pragma once

#include <algorithm>
#include <cstdint>

namespace lightwaveos::audio {

/**
 * @brief Audio pipeline configuration presets for A/B testing
 *
 * Each preset represents a different tuning philosophy derived from
 * comparative analysis of LightwaveOS and Sensory Bridge audio pipelines.
 */
enum class AudioPreset : uint8_t {
    LIGHTWAVE_V2 = 0,       ///< Current LightwaveOS defaults (4:1 AGC ratio)
    SENSORY_BRIDGE = 1,     ///< Sensory Bridge style (50:1 AGC ratio, faster attack)
    AGGRESSIVE_AGC = 2,     ///< High compression, very fast response
    CONSERVATIVE_AGC = 3,   ///< Low compression, smooth with minimal pumping
    CUSTOM = 255            ///< User-defined parameters
};

/**
 * @brief Per-frequency noise floor for band-specific calibration
 *
 * Sensory Bridge insight: Different frequency bands have different
 * ambient noise characteristics (HVAC at 120Hz, fans at 1-4kHz).
 */
struct PerBandNoiseFloor {
    float bands[8] = {0.0004f, 0.0004f, 0.0004f, 0.0004f,
                      0.0004f, 0.0004f, 0.0004f, 0.0004f};
    float multiplier = 1.0f;  ///< Applied during playback (Sensory Bridge uses 1.5x)
};

struct AudioPipelineTuning {
    float dcAlpha = 0.001f;

    float agcTargetRms = 0.25f;
    float agcMinGain = 1.0f;
    float agcMaxGain = 100.0f;
    float agcAttack = 0.08f;
    float agcRelease = 0.02f;
    float agcClipReduce = 0.90f;
    float agcIdleReturnRate = 0.01f;

    float noiseFloorMin = 0.0004f;
    float noiseFloorRise = 0.0005f;
    float noiseFloorFall = 0.01f;

    float gateStartFactor = 1.5f;
    float gateRangeFactor = 1.5f;
    float gateRangeMin = 0.0005f;

    float rmsDbFloor = -65.0f;
    float rmsDbCeil = -12.0f;
    float bandDbFloor = -65.0f;
    float bandDbCeil = -12.0f;
    float chromaDbFloor = -65.0f;
    float chromaDbCeil = -12.0f;

    float fluxScale = 1.0f;

    float controlBusAlphaFast = 0.35f;
    float controlBusAlphaSlow = 0.12f;

    // Silence detection (Sensory Bridge insight: 10-second hysteresis)
    float silenceHysteresisMs = 0.0f;    ///< 0 = disabled, >0 = wait time before standby
    float silenceThreshold = 0.01f;       ///< RMS below this is considered silence
};

struct AudioContractTuning {
    float audioStalenessMs = 100.0f;

    float bpmMin = 30.0f;
    float bpmMax = 300.0f;
    float bpmTau = 0.50f;
    float confidenceTau = 1.00f;
    float phaseCorrectionGain = 0.35f;
    float barCorrectionGain = 0.20f;

    uint8_t beatsPerBar = 4;
    uint8_t beatUnit = 4;
};

inline float clampf(float v, float lo, float hi) {
    return std::min(std::max(v, lo), hi);
}

inline AudioPipelineTuning clampAudioPipelineTuning(const AudioPipelineTuning& in) {
    AudioPipelineTuning out = in;

    out.dcAlpha = clampf(out.dcAlpha, 0.000001f, 0.1f);

    out.agcTargetRms = clampf(out.agcTargetRms, 0.01f, 1.0f);
    out.agcMinGain = clampf(out.agcMinGain, 0.1f, 50.0f);
    out.agcMaxGain = clampf(out.agcMaxGain, 1.0f, 500.0f);
    if (out.agcMaxGain < out.agcMinGain) {
        out.agcMaxGain = out.agcMinGain;
    }
    out.agcAttack = clampf(out.agcAttack, 0.0f, 1.0f);
    out.agcRelease = clampf(out.agcRelease, 0.0f, 1.0f);
    out.agcClipReduce = clampf(out.agcClipReduce, 0.1f, 1.0f);
    out.agcIdleReturnRate = clampf(out.agcIdleReturnRate, 0.0f, 1.0f);

    out.noiseFloorMin = clampf(out.noiseFloorMin, 0.0f, 0.1f);
    out.noiseFloorRise = clampf(out.noiseFloorRise, 0.0f, 1.0f);
    out.noiseFloorFall = clampf(out.noiseFloorFall, 0.0f, 1.0f);

    out.gateStartFactor = clampf(out.gateStartFactor, 0.0f, 10.0f);
    out.gateRangeFactor = clampf(out.gateRangeFactor, 0.0f, 10.0f);
    out.gateRangeMin = clampf(out.gateRangeMin, 0.0f, 0.1f);

    out.rmsDbFloor = clampf(out.rmsDbFloor, -120.0f, 0.0f);
    out.rmsDbCeil = clampf(out.rmsDbCeil, -120.0f, 0.0f);
    if (out.rmsDbCeil <= out.rmsDbFloor + 1.0f) {
        out.rmsDbCeil = out.rmsDbFloor + 1.0f;
    }

    out.bandDbFloor = clampf(out.bandDbFloor, -120.0f, 0.0f);
    out.bandDbCeil = clampf(out.bandDbCeil, -120.0f, 0.0f);
    if (out.bandDbCeil <= out.bandDbFloor + 1.0f) {
        out.bandDbCeil = out.bandDbFloor + 1.0f;
    }

    out.chromaDbFloor = clampf(out.chromaDbFloor, -120.0f, 0.0f);
    out.chromaDbCeil = clampf(out.chromaDbCeil, -120.0f, 0.0f);
    if (out.chromaDbCeil <= out.chromaDbFloor + 1.0f) {
        out.chromaDbCeil = out.chromaDbFloor + 1.0f;
    }

    out.fluxScale = clampf(out.fluxScale, 0.0f, 10.0f);

    out.controlBusAlphaFast = clampf(out.controlBusAlphaFast, 0.0f, 1.0f);
    out.controlBusAlphaSlow = clampf(out.controlBusAlphaSlow, 0.0f, 1.0f);

    out.silenceHysteresisMs = clampf(out.silenceHysteresisMs, 0.0f, 60000.0f);
    out.silenceThreshold = clampf(out.silenceThreshold, 0.0f, 1.0f);

    return out;
}

inline AudioContractTuning clampAudioContractTuning(const AudioContractTuning& in) {
    AudioContractTuning out = in;

    out.audioStalenessMs = clampf(out.audioStalenessMs, 10.0f, 1000.0f);

    out.bpmMin = clampf(out.bpmMin, 20.0f, 200.0f);
    out.bpmMax = clampf(out.bpmMax, 60.0f, 400.0f);
    if (out.bpmMax < out.bpmMin + 1.0f) {
        out.bpmMax = out.bpmMin + 1.0f;
    }

    out.bpmTau = clampf(out.bpmTau, 0.01f, 10.0f);
    out.confidenceTau = clampf(out.confidenceTau, 0.01f, 10.0f);
    out.phaseCorrectionGain = clampf(out.phaseCorrectionGain, 0.0f, 1.0f);
    out.barCorrectionGain = clampf(out.barCorrectionGain, 0.0f, 1.0f);

    if (out.beatsPerBar == 0) out.beatsPerBar = 4;
    if (out.beatUnit == 0) out.beatUnit = 4;
    if (out.beatsPerBar > 12) out.beatsPerBar = 12;
    if (out.beatUnit > 16) out.beatUnit = 16;

    return out;
}

/**
 * @brief Get a predefined audio pipeline configuration
 *
 * Returns tuning parameters for A/B testing different audio pipeline
 * configurations. Each preset is based on comparative analysis of
 * LightwaveOS and Sensory Bridge audio implementations.
 *
 * @param preset The preset to retrieve
 * @return AudioPipelineTuning configured for the preset
 */
inline AudioPipelineTuning getPreset(AudioPreset preset) {
    AudioPipelineTuning tuning;

    switch (preset) {
        case AudioPreset::LIGHTWAVE_V2:
            // Current LightwaveOS defaults - balanced 4:1 AGC ratio
            // Good all-around performance, may pump slightly during gaps
            tuning.agcAttack = 0.08f;
            tuning.agcRelease = 0.02f;
            tuning.controlBusAlphaFast = 0.35f;
            tuning.controlBusAlphaSlow = 0.12f;
            tuning.silenceHysteresisMs = 0.0f;  // No standby dimming
            break;

        case AudioPreset::SENSORY_BRIDGE:
            // Sensory Bridge v4.1.1 style - 50:1 AGC ratio
            // Fast attack for transients, very slow release prevents pumping
            tuning.agcAttack = 0.25f;           // 25% per frame (fast)
            tuning.agcRelease = 0.005f;         // 0.5% per frame (slow)
            tuning.controlBusAlphaFast = 0.45f; // Slightly faster smoothing
            tuning.controlBusAlphaSlow = 0.225f; // Matched ratio
            tuning.silenceHysteresisMs = 10000.0f;  // 10 second standby
            tuning.silenceThreshold = 0.005f;       // Lower threshold
            tuning.noiseFloorMin = 0.0006f;         // 1.5x multiplier baked in
            break;

        case AudioPreset::AGGRESSIVE_AGC:
            // Maximum compression, fastest response
            // Good for EDM/electronic with consistent levels
            tuning.agcAttack = 0.35f;
            tuning.agcRelease = 0.001f;
            tuning.agcMaxGain = 200.0f;
            tuning.controlBusAlphaFast = 0.5f;
            tuning.controlBusAlphaSlow = 0.3f;
            tuning.silenceHysteresisMs = 5000.0f;
            break;

        case AudioPreset::CONSERVATIVE_AGC:
            // Minimal compression, smooth response
            // Good for classical/acoustic with wide dynamics
            tuning.agcAttack = 0.03f;
            tuning.agcRelease = 0.05f;
            tuning.agcMaxGain = 50.0f;
            tuning.controlBusAlphaFast = 0.25f;
            tuning.controlBusAlphaSlow = 0.08f;
            tuning.silenceHysteresisMs = 15000.0f;
            tuning.silenceThreshold = 0.02f;
            break;

        case AudioPreset::CUSTOM:
        default:
            // Return defaults, caller will customize
            break;
    }

    return clampAudioPipelineTuning(tuning);
}

/**
 * @brief Get the name of an audio preset for display/logging
 */
inline const char* getPresetName(AudioPreset preset) {
    switch (preset) {
        case AudioPreset::LIGHTWAVE_V2:    return "LightwaveOS v2";
        case AudioPreset::SENSORY_BRIDGE:  return "Sensory Bridge";
        case AudioPreset::AGGRESSIVE_AGC:  return "Aggressive AGC";
        case AudioPreset::CONSERVATIVE_AGC: return "Conservative AGC";
        case AudioPreset::CUSTOM:          return "Custom";
        default:                           return "Unknown";
    }
}

} // namespace lightwaveos::audio
