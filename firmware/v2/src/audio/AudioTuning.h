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
    LGP_SMOOTH = 4,         ///< Optimized for LGP viewing (slow release, per-band gains)
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

/**
 * @brief Noise calibration state machine (SensoryBridge pattern)
 *
 * Implements automatic noise floor calibration via a silent measurement period.
 * The calibration procedure:
 * 1. User triggers calibration via API (state -> REQUESTED)
 * 2. System waits for audio to stabilize (state -> MEASURING)
 * 3. During MEASURING, accumulates RMS energy per band
 * 4. After durationMs, computes average and applies multiplier (state -> COMPLETE)
 * 5. Results can be applied to AudioPipelineTuning or saved to NVS
 */
enum class CalibrationState : uint8_t {
    IDLE = 0,       ///< No calibration in progress
    REQUESTED,      ///< User requested, waiting to start
    MEASURING,      ///< Actively measuring silence (accumulating samples)
    COMPLETE,       ///< Measurement complete, results ready
    FAILED          ///< Calibration failed (e.g., too much noise detected)
};

struct NoiseCalibrationResult {
    float bandFloors[8] = {0};      ///< Measured noise floor per band
    float chromaFloors[12] = {0};   ///< Measured noise floor per chroma bin
    float overallRms = 0.0f;        ///< Overall RMS during calibration
    float peakRms = 0.0f;           ///< Peak RMS seen (to detect non-silence)
    uint32_t sampleCount = 0;       ///< Number of hop samples accumulated
    bool valid = false;             ///< True if calibration succeeded
};

struct NoiseCalibrationState {
    CalibrationState state = CalibrationState::IDLE;
    uint32_t startTimeMs = 0;           ///< When calibration started (millis())
    uint32_t durationMs = 3000;         ///< How long to measure (default 3 seconds)
    float safetyMultiplier = 1.2f;      ///< Multiply measured floor by this (SensoryBridge uses 1.2-1.5x)
    float maxAllowedRms = 0.15f;        ///< Abort if RMS exceeds this (not silence)

    // Accumulation buffers (running sums)
    float bandSum[8] = {0};
    float chromaSum[12] = {0};
    float rmsSum = 0.0f;
    float peakRms = 0.0f;
    uint32_t sampleCount = 0;

    // Result (populated when state == COMPLETE)
    NoiseCalibrationResult result;

    void reset() {
        state = CalibrationState::IDLE;
        startTimeMs = 0;
        for (int i = 0; i < 8; ++i) bandSum[i] = 0;
        for (int i = 0; i < 12; ++i) chromaSum[i] = 0;
        rmsSum = 0.0f;
        peakRms = 0.0f;
        sampleCount = 0;
        result = NoiseCalibrationResult{};
    }
};

struct AudioPipelineTuning {
    float dcAlpha = 0.001f;

    float agcTargetRms = 0.25f;
    float agcMinGain = 1.0f;
    float agcMaxGain = 40.0f;      // Was 100.0 - cap to prevent runaway amplification
    float agcAttack = 0.03f;       // Was 0.08 - gentler approach (3% per hop)
    float agcRelease = 0.015f;     // Was 0.02 - slightly faster decay (2:1 ratio)
    float agcClipReduce = 0.90f;
    float agcIdleReturnRate = 0.01f;

    float noiseFloorMin = 0.0004f;
    float noiseFloorRise = 0.0005f;
    float noiseFloorFall = 0.01f;

    float gateStartFactor = 1.0f;  // Reduced from 1.5: more permissive gate to prevent false closures
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

    // Attack/release asymmetry for bands (LGP_SMOOTH: fast attack, slow release)
    float bandAttack = 0.15f;           ///< Band rise rate (fast transient response)
    float bandRelease = 0.03f;          ///< Band fall rate (slow decay for LGP viewing)
    float heavyBandAttack = 0.08f;      ///< Heavy band rise (extra smooth for ambient)
    float heavyBandRelease = 0.015f;    ///< Heavy band fall (ultra smooth)

    // Per-band normalization gains (boost highs, attenuate bass for visual balance)
    float perBandGains[8] = {0.8f, 0.85f, 1.0f, 1.2f, 1.5f, 1.8f, 2.0f, 2.2f};

    // Per-band noise floors (calibrated for typical ambient noise sources)
    float perBandNoiseFloors[8] = {0.0008f, 0.0012f, 0.0006f, 0.0005f,
                                    0.0008f, 0.0010f, 0.0012f, 0.0006f};
    bool usePerBandNoiseFloor = false;  ///< Enable per-band noise floor gating

    // Silence detection (fade to black after sustained silence)
    float silenceHysteresisMs = 5000.0f;  ///< 5s default (user-approved), 0 = disabled
    float silenceThreshold = 0.01f;       ///< RMS below this is considered silence

    // Goertzel novelty tuning (runtime adjustable)
    bool noveltyUseSpectralFlux = true;   ///< Use per-band flux instead of RMS-based
    float noveltySpectralFluxScale = 1.0f; ///< Additional scaling before fluxScale

    // Adaptive 64-bin normalisation (Sensory Bridge max follower)
    float bins64AdaptiveScale = 200.0f;   ///< Scale normalised bins into SB magnitude space
    float bins64AdaptiveFloor = 4.0f;     ///< Minimum max follower value (SB parity)
    float bins64AdaptiveRise = 0.0050f;   ///< Max follower rise rate
    float bins64AdaptiveFall = 0.0025f;   ///< Max follower fall rate
    float bins64AdaptiveDecay = 0.995f;   ///< Per-frame decay on max_value
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

/**
 * @brief Goertzel novelty tuning parameters
 *
 * Minimal configuration for spectral flux novelty computation.
 * Beat detection will be replaced by K1-Lightwave integration.
 */
struct GoertzelNoveltyTuning {
    bool useSpectralFlux = true;        ///< Use per-band flux instead of RMS-based
    float spectralFluxScale = 1.0f;     ///< Was 2.0 - reduced to prevent saturation
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

    // Attack/release asymmetry
    out.bandAttack = clampf(out.bandAttack, 0.0f, 1.0f);
    out.bandRelease = clampf(out.bandRelease, 0.0f, 1.0f);
    out.heavyBandAttack = clampf(out.heavyBandAttack, 0.0f, 1.0f);
    out.heavyBandRelease = clampf(out.heavyBandRelease, 0.0f, 1.0f);

    // Per-band gains
    for (int i = 0; i < 8; ++i) {
        out.perBandGains[i] = clampf(out.perBandGains[i], 0.1f, 10.0f);
        out.perBandNoiseFloors[i] = clampf(out.perBandNoiseFloors[i], 0.0f, 0.1f);
    }

    out.silenceHysteresisMs = clampf(out.silenceHysteresisMs, 0.0f, 60000.0f);
    out.silenceThreshold = clampf(out.silenceThreshold, 0.0f, 1.0f);

    out.noveltySpectralFluxScale = clampf(out.noveltySpectralFluxScale, 0.1f, 10.0f);

    out.bins64AdaptiveScale = clampf(out.bins64AdaptiveScale, 0.1f, 1000.0f);
    out.bins64AdaptiveFloor = clampf(out.bins64AdaptiveFloor, 0.01f, 1000.0f);
    out.bins64AdaptiveRise = clampf(out.bins64AdaptiveRise, 0.0f, 1.0f);
    out.bins64AdaptiveFall = clampf(out.bins64AdaptiveFall, 0.0f, 1.0f);
    out.bins64AdaptiveDecay = clampf(out.bins64AdaptiveDecay, 0.0f, 1.0f);

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

inline GoertzelNoveltyTuning clampGoertzelNoveltyTuning(const GoertzelNoveltyTuning& in) {
    GoertzelNoveltyTuning out = in;
    out.spectralFluxScale = clampf(out.spectralFluxScale, 0.1f, 10.0f);
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
            tuning.silenceHysteresisMs = 10000.0f;  // 10s standby (Sensory Bridge pattern)
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

        case AudioPreset::LGP_SMOOTH:
            // Optimized for Light Guide Plate viewing
            // Slow release allows light diffusion to settle, per-band gains balance spectrum
            tuning.agcAttack = 0.06f;
            tuning.agcRelease = 0.015f;
            tuning.controlBusAlphaFast = 0.20f;
            tuning.controlBusAlphaSlow = 0.06f;
            // Asymmetric attack/release for smooth LGP response
            tuning.bandAttack = 0.12f;          // Moderate attack (preserves beats)
            tuning.bandRelease = 0.025f;        // Very slow release (smooth decay)
            tuning.heavyBandAttack = 0.06f;     // Extra slow attack
            tuning.heavyBandRelease = 0.012f;   // Ultra slow release
            // Per-band gains: attenuate bass, boost treble for visual balance
            tuning.perBandGains[0] = 0.8f;      // 60Hz - attenuate sub-bass
            tuning.perBandGains[1] = 0.85f;     // 120Hz - attenuate bass
            tuning.perBandGains[2] = 1.0f;      // 250Hz - neutral
            tuning.perBandGains[3] = 1.2f;      // 500Hz - slight boost
            tuning.perBandGains[4] = 1.5f;      // 1kHz - boost
            tuning.perBandGains[5] = 1.8f;      // 2kHz - presence boost
            tuning.perBandGains[6] = 2.0f;      // 4kHz - brilliance boost
            tuning.perBandGains[7] = 2.2f;      // 7.8kHz - air boost
            // Enable per-band noise floors
            tuning.perBandNoiseFloors[0] = 0.0008f;  // 60Hz - power supply hum
            tuning.perBandNoiseFloors[1] = 0.0012f;  // 120Hz - HVAC fundamental
            tuning.perBandNoiseFloors[2] = 0.0006f;  // 250Hz - quiet
            tuning.perBandNoiseFloors[3] = 0.0005f;  // 500Hz - quiet
            tuning.perBandNoiseFloors[4] = 0.0008f;  // 1kHz - fan harmonics
            tuning.perBandNoiseFloors[5] = 0.0010f;  // 2kHz - fan noise
            tuning.perBandNoiseFloors[6] = 0.0012f;  // 4kHz - fan noise peak
            tuning.perBandNoiseFloors[7] = 0.0006f;  // 7.8kHz - quiet
            tuning.usePerBandNoiseFloor = true;
            tuning.silenceHysteresisMs = 8000.0f;   // 8 second standby
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
        case AudioPreset::LGP_SMOOTH:      return "LGP Smooth";
        case AudioPreset::CUSTOM:          return "Custom";
        default:                           return "Unknown";
    }
}

} // namespace lightwaveos::audio
