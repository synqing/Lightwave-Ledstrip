#pragma once
// ============================================================================
// PipelineAdapter.h — Bridge: PipelineCore -> ControlBusRawInput
// ============================================================================
//
// PURPOSE:
//   Translates PipelineCore's FeatureFrame output into the ControlBusRawInput
//   struct consumed by the existing ControlBus smoothing pipeline.
//
//   This is the ONLY file that knows about both PipelineCore and ControlBus.
//   Neither side depends on the other. If PipelineCore changes its output
//   format, only this file changes. If ControlBus changes its input format,
//   only this file changes.
//
// RESPONSIBILITIES:
//   1. Map FeatureFrame scalar fields -> ControlBusRawInput scalar fields
//   2. Populate bins64[64] backward-compatibility shim (DEPRECATED -- migrate off)
//   3. Populate bins256[256] full-resolution spectrum
//   4. Derive snare/hihat energy from magnitude spectrum via FrequencyMap
//   5. Derive snare/hihat onset triggers from onset envelope + band energy
//   6. Populate tempo/beat fields on ControlBusRawInput for ControlBus passthrough
//
// THREAD:
//   Runs on Core 0 (Audio Thread) inside AudioActor::processHop()
//
// LIFECYCLE:
//   1. Construct once in AudioActor
//   2. Call init() after PipelineCore is configured (need sample rate + FFT size)
//   3. Call adapt() on every PipelineCore hop to produce ControlBusRawInput
//
// AUTHOR:  Integration artifact -- CTO advisory
// DATE:    2026-02-19
// VERSION: 2.0.0
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstring>    // memcpy
#include "PipelineCore.h"             // FeatureFrame
#include "../contracts/ControlBus.h"  // ControlBusRawInput
#include "FrequencyMap.h"

namespace lightwaveos { namespace audio {

// ============================================================================
// Bins64 Backward-Compatibility Shim Configuration
// ============================================================================
//
// DEPRECATION NOTICE:
//   bins64[64] exists ONLY for effects that haven't been migrated to the
//   frequency-semantic API. Once all 11 hardcoded-index effects are migrated,
//   this shim should be removed.
//
// STRATEGY:
//   Simple 4:1 bin averaging from the 256-bin FFT spectrum.
//   This does NOT replicate the Goertzel frequency distribution.
//   Effects with hardcoded indices WILL see different frequency content.
//   That's acceptable because those effects are being migrated.
//
//   For effects that use bin() / binAdaptive() through the EffectContext API
//   without hardcoded indices (e.g., JuggleEffect null check, spectrum
//   visualisers), 4:1 averaging provides a reasonable spectrum overview.
//
static constexpr uint16_t BINS64_LEGACY_COUNT = 64;
static constexpr uint16_t BINS256_COUNT       = 256;

// ============================================================================
// PipelineAdapter
// ============================================================================
class PipelineAdapter {
public:
    // Adapter configuration -- set once at init
    struct Config {
        float sampleRate  = 32000.0f;
        uint16_t fftSize  = 512;

        // Onset detection thresholds for percussion triggers
        // These are TUNING PARAMETERS -- expect to adjust during listening sessions
        float snareOnsetThreshold = 0.15f;   // Minimum snare band energy delta for trigger
        float hihatOnsetThreshold = 0.10f;   // Minimum hi-hat band energy delta for trigger
        float onsetEnvGate        = 0.05f;   // Minimum onset_env to allow percussion triggers

        // Flux normalisation scale factor
        // PipelineCore log-flux has different dynamic range than ES backend flux.
        // This scales PipelineCore flux to [0,1] range expected by ControlBus.
        // Tuned for PipelineCore log-flux dynamic range.
        // This value is intentionally conservative to avoid saturating ControlBus
        // novelty at 1.0 during normal programme material.
        float fluxScale           = 20.0f;

        // RMS silence gate for spectrum normalization.
        // When RMS is below this threshold, spectrum output is zeroed.
        // Prevents peak-normalization from amplifying mic noise to full scale.
        // The Goertzel backend has its own noise floor subtraction; this is
        // PipelineCore's equivalent.
        float silenceRmsGate      = 0.0005f;  // ~-66 dBFS (lets very quiet-but-real mic signal through)
    };

    PipelineAdapter() = default;

    // ------------------------------------------------------------------------
    // init() -- Call once after PipelineCore is configured
    // ------------------------------------------------------------------------
    void init(const Config& cfg) {
        m_config = cfg;
        m_freqMap.init(cfg.sampleRate, cfg.fftSize);
        m_prevSnareEnergy = 0.0f;
        m_prevHihatEnergy = 0.0f;
    }

    // Accessor for FrequencyMap (used by AudioContext extensions)
    const ::audio::FrequencyMap& frequencyMap() const { return m_freqMap; }

    // ------------------------------------------------------------------------
    // adapt() -- Convert FeatureFrame -> ControlBusRawInput
    // ------------------------------------------------------------------------
    //
    // @param frame       PipelineCore output (const -- adapter does not modify)
    // @param magSpectrum Raw magnitude spectrum from PipelineCore (256 floats)
    // @param hopBuffer   Raw time-domain samples from PipelineCore (256 int16_t)
    // @param out         ControlBusRawInput to populate
    //
    // NOTE: magSpectrum and hopBuffer are separate from FeatureFrame because
    //       FeatureFrame only contains derived features (bands, chroma, etc.),
    //       not the raw spectrum. PipelineCore exposes the spectrum via
    //       getMagnitudeSpectrum() accessor.
    //
    void adapt(
        const FeatureFrame& frame,
        const float*        magSpectrum,    // PipelineCore::getMagnitudeSpectrum()
        const int16_t*      hopBuffer,      // PipelineCore::getHopBuffer()
        ControlBusRawInput& out
    );

    // ------------------------------------------------------------------------
    // Full-resolution spectrum access (for AudioContext extensions)
    // ------------------------------------------------------------------------
    //
    // After adapt(), this contains the full 256-bin normalised magnitude spectrum.
    // This is what the frequency-semantic API queries against.
    //
    const float* bins256() const { return m_bins256; }

private:
    Config              m_config;
    ::audio::FrequencyMap m_freqMap;

    // Full-resolution spectrum (populated in adapt())
    float m_bins256[BINS256_COUNT] = {};

    // Percussion onset state
    float m_prevSnareEnergy = 0.0f;
    float m_prevHihatEnergy = 0.0f;

    // Build bins64 backward-compat shim via 4:1 averaging
    void buildBins64Shim(const float* normSpectrum, float* bins64Out);

    // Derive percussion triggers from spectrum + onset envelope
    void derivePercussion(
        const float* normSpectrum,
        float  onsetEnv,
        float& snareEnergyOut,
        float& hihatEnergyOut,
        bool&  snareTriggerOut,
        bool&  hihatTriggerOut
    );

    // Normalise magnitude spectrum to [0, 1] range
    void normaliseMagnitudes(const float* magIn, float* normOut, uint16_t count);
};

}} // namespace lightwaveos::audio
