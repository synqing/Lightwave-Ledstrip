// ============================================================================
// PipelineAdapter.cpp — Bridge: PipelineCore -> ControlBusRawInput
// ============================================================================
//
// See PipelineAdapter.h for design rationale and usage.
//
// IMPLEMENTATION NOTES:
//
//   1. The bins64 shim uses simple 4:1 averaging. This is intentionally crude.
//      It exists for backward compatibility, not accuracy. Effects that need
//      accurate frequency queries should use the semantic API.
//
//   2. Percussion derivation uses FrequencyMap named bands, not hardcoded
//      bin indices. When the snare/hi-hat frequency ranges are tuned,
//      the changes propagate automatically.
//
//   3. Flux normalisation is a single scale factor. PipelineCore's log-flux
//      output has different dynamics than the ES backend. The scale factor
//      MUST be tuned during integration listening sessions. Start at 1.0.
//
//   4. Waveform handling: PipelineCore hop buffer is 256 samples.
//      ControlBusRawInput.waveform is 128 samples. We subsample 2:1.
//      This is safe -- waveform is used for visual rendering, not analysis.
//
// AUTHOR:  Integration artifact -- CTO advisory
// DATE:    2026-02-19
// VERSION: 2.0.0
// ============================================================================

#include "PipelineAdapter.h"

// FrequencyMap types are in ::audio namespace; bring NamedBand into scope
// to keep the derivePercussion() call site readable.
using ::audio::NamedBand;

// ============================================================================
// adapt() -- Main conversion: FeatureFrame -> ControlBusRawInput
// ============================================================================

namespace lightwaveos { namespace audio {

void PipelineAdapter::adapt(
    const FeatureFrame& frame,
    const float*        magSpectrum,
    const int16_t*      hopBuffer,
    ControlBusRawInput& out
) {
    // ------------------------------------------------------------------
    // 1. Direct scalar mappings
    // ------------------------------------------------------------------
    out.rms         = frame.rms;
    out.rmsUngated  = frame.rms;   // PipelineCore doesn't have ungated RMS.
                                    // Use gated RMS for now. If silence
                                    // detection misbehaves, add ungated
                                    // output to PipelineCore.

    // Flux mapping: PipelineCore log-flux -> bounded [0,1] novelty.
    // Use soft-knee compression to preserve dynamics and avoid hard ceiling lock.
    float scaledFlux = frame.flux * m_config.fluxScale;
    if (scaledFlux < 0.0f) scaledFlux = 0.0f;
    out.flux = scaledFlux / (1.0f + scaledFlux);

    // ------------------------------------------------------------------
    // 2. Band energies -- direct mapping (both are 8-band, [0,1])
    // ------------------------------------------------------------------
    if (frame.rms >= m_config.silenceRmsGate) {
        for (uint8_t i = 0; i < 8; ++i) {
            out.bands[i] = frame.bands[i];
        }
    } else {
        for (uint8_t i = 0; i < 8; ++i) {
            out.bands[i] = 0.0f;
        }
    }

    // ------------------------------------------------------------------
    // 3. Chroma -- direct mapping (both are 12-bin, [0,1])
    // ------------------------------------------------------------------
    if (frame.rms >= m_config.silenceRmsGate) {
        for (uint8_t i = 0; i < 12; ++i) {
            out.chroma[i] = frame.chroma[i];
        }
    } else {
        for (uint8_t i = 0; i < 12; ++i) {
            out.chroma[i] = 0.0f;
        }
    }

    // ------------------------------------------------------------------
    // 4. Waveform -- subsample 256 -> 128 (every other sample)
    // ------------------------------------------------------------------
    if (hopBuffer) {
        for (uint8_t i = 0; i < 128; ++i) {
            out.waveform[i] = hopBuffer[i * 2];
        }
    }

    // ------------------------------------------------------------------
    // 5. Full-resolution spectrum (RMS-gated)
    // ------------------------------------------------------------------
    // Gate: If RMS is below silence threshold, zero the spectrum.
    // Without this gate, peak-normalization amplifies mic noise to 1.0,
    // saturating bins64/spectrogram and driving LEDs to max brightness.
    const bool spectrumGateOpen = (frame.rms >= m_config.silenceRmsGate);

    if (magSpectrum && spectrumGateOpen) {
        normaliseMagnitudes(magSpectrum, m_bins256, BINS256_COUNT);
        memcpy(out.bins256, m_bins256, sizeof(float) * BINS256_COUNT);
    } else {
        memset(m_bins256, 0, sizeof(float) * BINS256_COUNT);
        memset(out.bins256, 0, sizeof(float) * BINS256_COUNT);
    }

    // ------------------------------------------------------------------
    // 6. Bins64 backward-compat shim (DEPRECATED)
    // ------------------------------------------------------------------
    if (magSpectrum && spectrumGateOpen) {
        buildBins64Shim(m_bins256, out.bins64);
        // bins64Adaptive: for now, copy bins64.
        // The existing ControlBus adaptive normalisation will run on top.
        // When bins64 is removed, this goes with it.
        memcpy(out.bins64Adaptive, out.bins64, sizeof(float) * BINS64_LEGACY_COUNT);
    } else {
        memset(out.bins64, 0, sizeof(float) * BINS64_LEGACY_COUNT);
        memset(out.bins64Adaptive, 0, sizeof(float) * BINS64_LEGACY_COUNT);
    }

    // ------------------------------------------------------------------
    // 7. Percussion detection (derived from full spectrum)
    // ------------------------------------------------------------------
    if (magSpectrum && spectrumGateOpen) {
        derivePercussion(
            m_bins256,
            frame.onset_env,
            out.snareEnergy,
            out.hihatEnergy,
            out.snareTrigger,
            out.hihatTrigger
        );
    } else {
        out.snareEnergy = 0.0f;
        out.hihatEnergy = 0.0f;
        out.snareTrigger = false;
        out.hihatTrigger = false;
    }

    // ------------------------------------------------------------------
    // 8. Tempo/beat -- tracker-native lock and confidence passthrough
    // ------------------------------------------------------------------
    out.tempoLocked = (frame.tempo_locked > 0.5f);
    float conf = frame.tempo_confidence;
    if (conf < 0.0f) conf = 0.0f;
    if (conf > 1.0f) conf = 1.0f;
    out.tempoConfidence = conf;
    out.tempoBeatTick  = (frame.beat_event > 0.0f);

    // ------------------------------------------------------------------
    // 9. Full-resolution tempo fields for ControlBus passthrough
    // ------------------------------------------------------------------
    out.tempoBpm           = frame.tempo_bpm;
    out.tempoBeatStrength  = (frame.beat_event > 0.0f) ? frame.beat_event : 0.0f;
    out.binHz              = m_config.sampleRate / static_cast<float>(m_config.fftSize);
}

// ============================================================================
// buildBins64Shim() -- 4:1 averaging for backward compatibility
// ============================================================================
//
// DEPRECATED: This shim exists only for effects that haven't been migrated
// to the frequency-semantic API. Do not optimise it.
//
// Input:  normalised magnitude spectrum, 256 bins
// Output: 64-bin summary, each bin = mean of 4 consecutive FFT bins
//
// At 32kHz/512-pt FFT, each shim bin covers 250 Hz (4 x 62.5 Hz).
// shim[0] = mean(FFT bins 0-3)   ->   0 - 250 Hz
// shim[1] = mean(FFT bins 4-7)   -> 250 - 500 Hz
// ...
// shim[63] = mean(FFT bins 252-255) -> 15750 - 16000 Hz
//
// This is NOT frequency-equivalent to the Goertzel distribution.
// That's by design -- hardcoded-index effects are being migrated.
//

void PipelineAdapter::buildBins64Shim(const float* normSpectrum, float* bins64Out) {
    for (uint16_t i = 0; i < BINS64_LEGACY_COUNT; ++i) {
        uint16_t base = i * 4;
        bins64Out[i] = (normSpectrum[base]     +
                        normSpectrum[base + 1] +
                        normSpectrum[base + 2] +
                        normSpectrum[base + 3]) * 0.25f;
    }
}

// ============================================================================
// derivePercussion() -- Snare and hi-hat from frequency-semantic queries
// ============================================================================
//
// Uses FrequencyMap named bands (NamedBand::SNARE, NamedBand::HIHAT).
// Triggers fire when:
//   1. Band energy exceeds previous frame by threshold (onset delta)
//   2. Global onset envelope is above gate (prevents false triggers in silence)
//
// TUNING: snareOnsetThreshold, hihatOnsetThreshold, onsetEnvGate in Config.
//

void PipelineAdapter::derivePercussion(
    const float* normSpectrum,
    float  onsetEnv,
    float& snareEnergyOut,
    float& hihatEnergyOut,
    bool&  snareTriggerOut,
    bool&  hihatTriggerOut
) {
    // Compute current band energies
    float snareEnergy = m_freqMap.bandMeanEnergy(normSpectrum, NamedBand::SNARE);
    float hihatEnergy = m_freqMap.bandMeanEnergy(normSpectrum, NamedBand::HIHAT);

    // Output continuous energy levels
    snareEnergyOut = snareEnergy;
    hihatEnergyOut = hihatEnergy;

    // Onset triggers: energy delta + global gate
    bool gateOpen = (onsetEnv > m_config.onsetEnvGate);

    float snareDelta = snareEnergy - m_prevSnareEnergy;
    float hihatDelta = hihatEnergy - m_prevHihatEnergy;

    snareTriggerOut = gateOpen && (snareDelta > m_config.snareOnsetThreshold);
    hihatTriggerOut = gateOpen && (hihatDelta > m_config.hihatOnsetThreshold);

    // Update previous state
    m_prevSnareEnergy = snareEnergy;
    m_prevHihatEnergy = hihatEnergy;
}

// ============================================================================
// normaliseMagnitudes() -- Scale raw FFT magnitudes to [0, 1]
// ============================================================================
//
// Strategy: Find peak magnitude, divide all bins by peak.
// If peak is zero (silence), output all zeros.
//
// NOTE: This is a simple peak normalisation. The ControlBus smoothing
// pipeline (Zone AGC, attack/release) runs on top and handles the
// dynamic range compression that effects expect.
//

void PipelineAdapter::normaliseMagnitudes(
    const float* magIn,
    float*       normOut,
    uint16_t     count
) {
    // Zero bin 0 (DC) — never musically relevant, and a residual DC offset
    // from I2S bit-shifting can dominate the peak, crushing all real
    // frequency content to near-zero after normalisation.
    normOut[0] = 0.0f;

    // Find peak (skip bin 0)
    float peak = 0.0f;
    for (uint16_t i = 1; i < count; ++i) {
        if (magIn[i] > peak) peak = magIn[i];
    }

    // Normalise (skip bin 0, already zeroed)
    if (peak > 1e-10f) {
        float invPeak = 1.0f / peak;
        for (uint16_t i = 1; i < count; ++i) {
            normOut[i] = magIn[i] * invPeak;
        }
    } else {
        for (uint16_t i = 1; i < count; ++i) {
            normOut[i] = 0.0f;
        }
    }
}

}} // namespace lightwaveos::audio
