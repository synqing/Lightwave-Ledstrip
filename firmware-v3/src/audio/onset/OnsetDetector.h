/**
 * @file OnsetDetector.h
 * @brief FFT-based onset detection running parallel to the Goertzel pipeline.
 *
 * Computes 1024-point real FFT via esp-dsp (half-length complex trick, ~300us on ESP32-S3),
 * extracts 512 magnitude bins, computes log-spectral flux (Bello 2005 / Dixon 2006)
 * with CPJKU adaptive threshold (Boeck & Widmer 2013) and peak picking.
 *
 * Architecture: the activity gate is OUTPUT PERMISSION ONLY.
 * It must NOT rewrite threshold history, per-band statistics, or scale flux amplitude.
 * The detector's internal statistics always track real spectral flux values.
 * The gate only suppresses emitted events (onset_event, band triggers).
 *
 * Algorithm references:
 *   - Bello et al. (2005) "A Tutorial on Onset Detection in Music Signals"
 *   - Dixon (2006) "Onset Detection Revisited"
 *   - Boeck, Krebs, Widmer (2012) "Evaluating the Online Capabilities of Onset Detection Methods"
 *   - Boeck & Widmer (2013) "Maximum Filter Vibrato Suppression for Onset Detection" (SuperFlux)
 *
 * Integration: Called every hop (125 Hz @ 32kHz/256-hop) from AudioActor.
 * Input: Last 1024 float samples from sample_history[] (DC-blocked, -1..1).
 * Output: OnsetResult merged into ControlBusFrame before publish.
 *
 * Memory: ~14 KB SRAM (all static, no heap allocation in process path).
 * CPU: ~400 us/hop (~5% of 8ms budget) — measured via self-timing.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>

namespace lightwaveos::audio {

enum OnsetGateFlags : uint8_t {
    ONSET_GATE_NONE      = 0,
    ONSET_GATE_ABS_RMS   = 1u << 0,  ///< Raw hop RMS below absolute detector floor
    ONSET_GATE_ACTIVITY  = 1u << 1,  ///< Raw hop RMS remains inside local ambient floor
    ONSET_GATE_NO_PREV   = 1u << 2,  ///< Detector is still priming its first spectrum
    ONSET_GATE_WARMUP    = 1u << 3,  ///< Detector statistics are still in warmup
};

// ============================================================================
// OnsetResult — per-hop output published to ControlBus
// ============================================================================

struct OnsetResult {
    float    flux;           ///< Raw log-spectral flux [0, inf) — sum, not mean
    float    onset_env;      ///< Thresholded onset envelope [0, inf)
    float    onset_event;    ///< 0 = no event, >0 = event strength
    float    bass_flux;      ///< Low-freq flux for kick detection [0, inf)
    float    mid_flux;       ///< Mid-freq flux for snare detection [0, inf)
    float    high_flux;      ///< High-freq flux for hihat detection [0, inf)
    bool     kick_trigger;   ///< Kick drum onset detected
    bool     snare_trigger;  ///< Snare onset detected
    bool     hihat_trigger;  ///< Hi-hat onset detected
    float    input_rms;      ///< Raw hop RMS fed into the onset detector
    float    noise_floor;    ///< Detector-local ambient RMS follower
    float    activity;       ///< 0..1 activity gate opening derived from raw RMS
    uint8_t  gate_flags;     ///< Bitmask of OnsetGateFlags for trace/debug
    uint16_t process_us;     ///< Self-timed processing duration (us)
};

// ============================================================================
// OnsetDetector — 1024-point FFT log-spectral flux onset detector
// ============================================================================

class OnsetDetector {
public:
    // ------------------------------------------------------------------------
    // Tuning parameters — research-backed (CPJKU / Bello / Boeck)
    // ------------------------------------------------------------------------

    struct Config {
        // Warmup: suppress detection output while VU AGC settles.
        // The ES VU takes ~7s to converge (vu_floor 5s ring + 2s pre-fill).
        // Ref: SensoryBridge uses 256 frames (~5s) calibration.
        // Ref: OBTAIN beat tracker requires ~5s settling.
        uint32_t warmupFrames     = 1000;   ///< ~8s @ 125 Hz — VU AGC settling (vu_floor 7s + margin)

        // FFT
        uint16_t fftSize          = 1024;   ///< FFT window size (must be power of 2, max 1024)

        // Musical bin range for full-band flux (31.25 Hz/bin @ 32kHz/1024)
        uint16_t musicalBinLo     = 2;      ///<  ~62 Hz (skip DC and sub-bass rumble)
        uint16_t musicalBinHi     = 128;    ///< ~4000 Hz — musical ceiling

        // Full-band onset threshold: median-based adaptive (Dixon 2006).
        uint8_t  thresholdFrames     = 13;    ///< Median window for threshold (100ms @ 125 Hz)
        float    thresholdMultiplier = 1.5f;  ///< Threshold = max(median * mul, floor)
        float    thresholdFloor      = 0.01f; ///< Absolute floor after RMS gate opens

        // Detector-local activity gate — OUTPUT PERMISSION ONLY.
        // This gate must NOT:
        //   - inject zeros into threshold history
        //   - inject zeros into per-band EMA paths
        //   - scale flux amplitude (no activity^2 weighting)
        //   - rewrite detector readiness state except for absolute near-zero reset
        // It only decides whether onset_event and band triggers are EMITTED.
        float    rmsGate            = 0.002f; ///< Absolute minimum raw PCM RMS for detector
        float    activityFloor      = 0.002f; ///< Minimum ambient floor (raw PCM scale)

        // Ambient follower: hold + slow release
        // Rise tracks changing room/self-noise.
        // Release is intentionally slower than rise (prevents threshold collapse after programme stops).
        // Hold prevents immediate collapse.
        float    noiseFloorRise     = 0.020f; ///< Ambient floor attack (~0.4 s @ 125 Hz)
        float    noiseFloorFall     = 0.005f; ///< Ambient floor release (~1.6 s @ 125 Hz)
        uint16_t activityHoldFrames = 32;     ///< Hold floor for ~256 ms after activity

        float    noiseAdaptK        = 3.0f;   ///< Ignore large transients above ambient floor * K
        float    activityGateK      = 1.5f;   ///< Activity begins above noiseFloor * K
        float    activityRangeK     = 1.5f;   ///< Additional multiple needed to reach full activity
        float    activityRangeMin   = 0.002f; ///< Minimum RMS range for activity ramp
        float    activityCutoff     = 0.05f;  ///< Suppress event emission below this activity

        // Peak picker (CPJKU causal, real-time — no post lookahead)
        uint8_t  preMaxFrames     = 4;      ///< Past window for local max test (~32ms @ 125 Hz)
        uint8_t  peakWait         = 4;      ///< Refractory period in frames (~32ms @ 125 Hz)

        // Band-split bin ranges — non-overlapping, musically relevant only
        uint16_t bassBinLo        = 2;      ///<  ~62 Hz
        uint16_t bassBinHi        = 8;      ///< ~250 Hz — kick drum fundamental
        uint16_t snareBinLo       = 8;      ///< ~250 Hz
        uint16_t snareBinHi       = 32;     ///< ~1000 Hz — snare fundamental + body
        uint16_t hihatBinLo       = 192;    ///< ~6000 Hz
        uint16_t hihatBinHi       = 384;    ///< ~12000 Hz — hi-hat + cymbal range

        // Per-band threshold multipliers
        float    kickThresholdK   = 0.8f;
        float    snareThresholdK  = 1.0f;
        float    hihatThresholdK  = 1.2f;

        // Per-band EMA alpha
        float    kickEmaAlpha     = 0.015f;
        float    snareEmaAlpha    = 0.010f;
        float    hihatEmaAlpha    = 0.005f;

        // Per-band refractory periods (frames @ 125 Hz)
        uint8_t  kickRefractory   = 6;      ///< 48ms
        uint8_t  snareRefractory  = 5;      ///< 40ms
        uint8_t  hihatRefractory  = 3;      ///< 24ms
    };

    void init();
    void init(const Config& cfg);
    void reset();
    OnsetResult process(const float* samples, float currentRms);

private:
    Config m_cfg;

    // FFT buffers (all in SRAM, no heap allocation)
    static constexpr uint16_t MAX_FFT_SIZE = 1024;
    static constexpr uint16_t MAX_BINS     = MAX_FFT_SIZE / 2;

    float m_hannLut[MAX_FFT_SIZE];          ///< Pre-computed Hann window (4 KB)
    float m_fftBuf[MAX_FFT_SIZE];           ///< In-place real FFT buffer (4 KB)
    float m_magnitude[MAX_BINS];            ///< Current magnitude spectrum (2 KB)
    float m_prevMagnitude[MAX_BINS];        ///< Previous magnitude spectrum (2 KB)
    bool  m_hasPrevMag = false;
    bool  m_initialised = false;
    float m_rmsNoiseFloor = 0.0f;           ///< Detector-local ambient RMS follower
    uint16_t m_activityHoldCounter = 0;     ///< Hold ambient floor after activity before release

    // Adaptive threshold state (Dixon 2006 median-based)
    static constexpr uint8_t FLUX_RING_SIZE = 16;
    float    m_fluxRing[FLUX_RING_SIZE] = {};
    uint8_t  m_fluxWriteIdx = 0;
    uint8_t  m_fluxCount = 0;

    // Peak picker state
    static constexpr uint8_t ENV_RING_SIZE = 32;
    float    m_envRing[ENV_RING_SIZE] = {};
    uint8_t  m_envWriteIdx = 0;
    uint32_t m_lastEventFrame = 0;
    uint32_t m_frameCount = 0;

    // Per-band trigger state
    struct BandState {
        float fluxMean  = 0.0f;
        float prevFlux  = 0.0f;
        float prev2Flux = 0.0f;
        uint32_t lastTriggerFrame = 0;
    };
    BandState m_bassState;
    BandState m_midState;
    BandState m_highState;

    // Warmup duration from config
    uint32_t m_warmupFrames = 1000;

    // Internal helpers
    void computeHannLut();
    void computeFFT();
    void computeMagnitudes();
    float computeBandFlux(uint16_t binLo, uint16_t binHi) const;
    float computeActivity(float currentRms);
    float applyAdaptiveThreshold(float flux);
    float peakPick(float env, bool emitEnabled);
    bool bandTrigger(BandState& state, float flux, float thresholdK,
                     float emaAlpha, uint8_t refractoryFrames, bool emitEnabled);
    static float computeMedian(const float* ring, uint8_t ringSize, uint8_t count);

    static uint32_t getTimeUs();
};

} // namespace lightwaveos::audio
