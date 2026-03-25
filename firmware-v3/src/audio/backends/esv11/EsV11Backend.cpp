/**
 * @file EsV11Backend.cpp
 */

#include "EsV11Backend.h"

#if FEATURE_AUDIO_SYNC && FEATURE_AUDIO_BACKEND_ESV11

#include <cmath>
#include <cstring>

#define LW_LOG_TAG "AudioESV11"
#include "utils/Log.h"

#include "vendor/EsV11Shim.h"
#include "vendor/EsV11Buffers.h"

// Vendor headers (frozen ES v1.1_320 sources)
#include "vendor/global_defines.h"
#include "vendor/microphone.h"
#include "vendor/goertzel.h"
#include "vendor/vu.h"
#include "vendor/tempo.h"

namespace lightwaveos::audio::esv11 {

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

bool EsV11Backend::init()
{
#if defined(ARDUINO) && !defined(NATIVE_BUILD)
    const size_t heapBefore = ESP.getFreeHeap();
    const size_t psramBefore = ESP.getFreePsram();
    const size_t psramSize = ESP.getPsramSize();
    LW_LOGI("ESV11 init: heap=%lu, PSRAM=%lu/%lu", (unsigned long)heapBefore, (unsigned long)psramBefore,
            (unsigned long)psramSize);
#endif

    if (!::esv11_init_buffers()) {
#if defined(ARDUINO) && !defined(NATIVE_BUILD)
        LW_LOGE("ESV11 init: buffer allocation failed (heap=%lu, PSRAM=%lu/%lu)",
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram(), (unsigned long)ESP.getPsramSize());
#else
        LW_LOGE("ESV11 init: buffer allocation failed");
#endif
        return false;
    }

#if defined(ARDUINO) && !defined(NATIVE_BUILD)
    LW_LOGI("ESV11 init: heap %lu -> %lu, PSRAM %lu -> %lu", (unsigned long)heapBefore,
            (unsigned long)ESP.getFreeHeap(), (unsigned long)psramBefore, (unsigned long)ESP.getFreePsram());
#endif

    // ES vendor initialisation ordering is preserved where practical.
    // NOTE: ES expects various tables to be initialised (window lookup, goertzel constants, tempo constants).
    init_window_lookup();
    init_goertzel_constants();
    init_tempo_goertzel_constants();
    init_vu();
    init_i2s_microphone();

    m_sampleIndex = 0;
    m_lastGpuTickUs = 0;
    m_stableTopBin = 0;
    m_stableBinLockedUs = 0;
    m_octaveCandBin = 0;
    m_octaveCandRuns = 0;
    m_genericCandBin = 0;
    m_genericCandFirstUs = 0;
    m_stableBinValidated = false;
    m_beatPhase = 0.0f;
    m_lastRefreshUs = 0;
    m_beatInBar = 0;
    m_latest = EsV11Outputs{};
    return true;
}

void EsV11Backend::tickEsGpu(float delta)
{
    // ES render-domain cadence: novelty curve update + tempi phase advance.
    update_novelty();
    update_tempi_phase(delta);
}

bool EsV11Backend::readAndProcessChunk(uint64_t now_us)
{
    const uint32_t now_ms = static_cast<uint32_t>(now_us / 1000);
    esv11_set_time(now_us, now_ms);

    acquire_sample_chunk();   // reads CHUNK_SIZE=64 and updates sample_history
    m_sampleIndex += CHUNK_SIZE;

    // ES CPU-side DSP stages (mirrors cpu_core.h ordering)
    calculate_magnitudes();
    get_chromagram();
    run_vu();
    update_tempo();

    // ES GPU-side tick with ES-style delta scaling (REFERENCE_FPS, 100)
    if (m_lastGpuTickUs == 0) {
        m_lastGpuTickUs = now_us;
    }
    uint64_t elapsed_us = now_us - m_lastGpuTickUs;
    const float ideal_us_interval = 1000000.0f / static_cast<float>(REFERENCE_FPS);
    const float delta = static_cast<float>(elapsed_us) / ideal_us_interval;
    m_lastGpuTickUs = now_us;
    tickEsGpu(delta);

    refreshOutputs(now_us);
    return true;
}

void EsV11Backend::refreshOutputs(uint64_t now_us)
{
    EsV11Outputs out{};
    out.now_us = now_us;
    out.now_ms = static_cast<uint32_t>(now_us / 1000);
    out.sample_index = m_sampleIndex;

    // Spectrum (ES uses NUM_FREQS=64)
    for (uint16_t i = 0; i < 64; ++i) {
        out.spectrogram_smooth[i] = clamp01(spectrogram_smooth[i]);
    }

    // Chroma
    for (uint8_t i = 0; i < 12; ++i) {
        out.chromagram[i] = clamp01(chromagram[i]);
    }

    // VU + novelty proxy
    out.vu_level = clamp01(vu_level);
    out.novelty_norm_last = clamp01(novelty_curve_normalized[NOVELTY_HISTORY_LENGTH - 1]);

    // Tempo: use octave-aware post-selection on top of ES smoothed bank,
    // with hysteresis + time-based hold to stabilise beat clock.
    //
    // Pre-v3 EsBeatClock ran at 120 FPS and integrated phase continuously
    // without resetting on bin changes. This v3 code replaces that with
    // direct phase_inverted tracking, so bin stability is critical.
    const uint16_t candidate_i = esv11_pick_top_tempo_bin_octave_aware();
    const uint16_t raw_i = esv11_pick_top_tempo_bin_raw();
    // Octave-partner surrogate for BOTH sides of the magnitude comparison.
    // At 32kHz the Goertzel's dominant bin is typically the half-tempo (e.g. 66
    // for a 132 BPM track). The octave-aware picker doubles it to 132, but
    // tempi_smooth[132] has near-zero energy — only tempi_smooth[66] is hot.
    //
    // Candidate side: if octave picker promoted raw→2x, use raw bin's magnitude
    // so the promoted candidate can actually compete in the gate comparison.
    // Current side: if stable bin was previously octave-promoted, its smooth
    // value decays to zero while the half-tempo partner remains energised.
    // Using max(bin, half-bin) prevents the "current decayed" escape hatch
    // from firing while the underlying harmonic is still active.
    float candidate_mag = tempi_smooth[candidate_i];
    if (candidate_i != raw_i && raw_i < NUM_TEMPI) {
        candidate_mag = fmaxf(candidate_mag, tempi_smooth[raw_i]);
    }
    float current_mag = tempi_smooth[m_stableTopBin];
    {
        const uint16_t stabBpmI = TEMPO_LOW + m_stableTopBin;
        const uint16_t halfBpm = stabBpmI / 2;
        if (halfBpm >= TEMPO_LOW) {
            const uint16_t halfIdx = halfBpm - TEMPO_LOW;
            if (halfIdx < NUM_TEMPI) {
                current_mag = fmaxf(current_mag, tempi_smooth[halfIdx]);
            }
        }
    }
    // Time-based hold: don't allow bin switch within hold period of last switch.
    // Magnitude gate: new candidate must be >50% stronger, OR current decayed.
    // Bin stabilisation with octave-aware promotion.
    // Generic switches require 200ms hold + 1.2x magnitude superiority.
    // Octave-family switches (candidate ≈ 2x stable) get promoted after
    // 4 consecutive appearances with no extra magnitude requirement,
    // because half-tempo lock is the dominant failure mode.
    const uint64_t kBinHoldUs = static_cast<uint64_t>(m_tp.holdUs);
    const uint8_t kOctaveRunsRequired = m_tp.octaveRuns;
    const float kConfidenceFloor = m_tp.confFloor;
    const bool holdExpired = (now_us - m_stableBinLockedUs) >= kBinHoldUs;
    // Confidence-proportional gate: once a bin proves stable, raise the bar
    // for switching. Prevents noise-driven switches during quiet passages
    // while keeping fast initial lock-on (gate starts at gateBase, grows
    // toward gateBase+gateScale as hold duration increases).
    const float holdSec = static_cast<float>(now_us - m_stableBinLockedUs) / 1000000.0f;
    const float gateStability = m_stableBinValidated
        ? (holdSec / (holdSec + m_tp.gateTau)) : 0.0f;
    const float kGenericGate = m_tp.gateBase + gateStability * m_tp.gateScale;

    // Low-confidence freeze: when evidence is weak, hold current bin.
    // The translator consumes audio-domain BPM directly — bin jitter during
    // quiet passages kills timing_reliable. v2's renderer-side EsBeatClock
    // absorbed this naturally at 120 FPS, but the translator has no such buffer.
    // Escape hatch: current_mag < 0.005 still allows switches (silence/song change).
    const bool confidenceFreeze = tempo_confidence < kConfidenceFloor
        && current_mag >= m_tp.decayFloor;

    // Octave-family check: is candidate approximately 2x the stable BPM?
    const float stabBpm = static_cast<float>(TEMPO_LOW + m_stableTopBin);
    const float candBpm = static_cast<float>(TEMPO_LOW + candidate_i);
    const float ratio = (stabBpm > 0.0f) ? (candBpm / stabBpm) : 0.0f;
    const bool isOctaveUp = (ratio > m_tp.octRatioLo && ratio < m_tp.octRatioHi);

    if (candidate_i != m_stableTopBin && !confidenceFreeze) {
        // Track octave candidate persistence
        if (isOctaveUp) {
            if (candidate_i == m_octaveCandBin) {
                if (m_octaveCandRuns < 255) ++m_octaveCandRuns;
            } else {
                m_octaveCandBin = candidate_i;
                m_octaveCandRuns = 1;
            }
        } else {
            m_octaveCandRuns = 0;
        }

        // Track generic candidate persistence (time-based).
        // Candidate must be the consistent octave-aware winner for
        // genericPersistUs before generic promotion is allowed.
        // Prevents transient spectral artefacts from causing switches.
        if (candidate_i == m_genericCandBin) {
            // Same candidate persisting — timer keeps running
        } else {
            m_genericCandBin = candidate_i;
            m_genericCandFirstUs = now_us;
        }
        const bool genericPersisted =
            (now_us - m_genericCandFirstUs) >= m_tp.genericPersistUs;

        if (holdExpired) {
            // Octave-family promotion: persistent double-time candidate
            // gets promoted with no magnitude gate (hold expiry is sufficient).
            const bool octavePromotion = isOctaveUp
                && m_octaveCandRuns >= kOctaveRunsRequired;
            const bool genericPromotion = genericPersisted
                && (candidate_mag > current_mag * kGenericGate
                    || current_mag < m_tp.decayFloor);

            if (octavePromotion || genericPromotion) {
                ESP_LOGD("Tempo", "BIN-SWITCH %s stab=%.0f->%.0f runs=%u cM=%.4f sM=%.4f rawSM=%.4f gate=%.2f persist=%.1fs",
                    octavePromotion ? "OCTAVE" : "GENERIC",
                    stabBpm, candBpm,
                    static_cast<unsigned>(m_octaveCandRuns),
                    candidate_mag, current_mag,
                    tempi_smooth[m_stableTopBin], kGenericGate,
                    static_cast<float>(now_us - m_genericCandFirstUs) / 1000000.0f);
                m_stableTopBin = candidate_i;
                m_stableBinLockedUs = now_us;
                m_octaveCandRuns = 0;
                m_genericCandBin = 0;
                m_genericCandFirstUs = 0;
                m_stableBinValidated = false;
            }
        }
    } else {
        // Stable bin matches candidate — reset all candidate tracking
        m_octaveCandRuns = 0;
        m_genericCandBin = 0;
        m_genericCandFirstUs = 0;
    }
    const uint16_t top_i = m_stableTopBin;
    const float top_mag = tempi_smooth[top_i];

    const float bpm = static_cast<float>(TEMPO_LOW + top_i);
    out.top_bpm = bpm;

    // Composite confidence from three independent evidence sources:
    //  1. Raw vendor confidence (spectral dominance: peak/sum of tempi bank)
    //  2. Validated stability (temporal persistence of the stabilised bin)
    //  3. Winner separation (how clearly the winner beats the runner-up)
    // Each measures a different axis of trust. max() means any single
    // strong signal is sufficient evidence for the translator.
    const float rawConfidence = clamp01(tempo_confidence);

    // Validate: stability only counts once raw conf has shown real support.
    // Prevents startup noise bins from earning false trust.
    if (rawConfidence >= m_tp.validationThr) {
        m_stableBinValidated = true;
    }
    const float holdSeconds = static_cast<float>(now_us - m_stableBinLockedUs) / 1000000.0f;
    const float stabilityConfidence = m_stableBinValidated
        ? (holdSeconds / (holdSeconds + m_tp.stabilityTau))
        : 0.0f;

    // Winner separation: how clearly does the stable bin beat the runner-up?
    // peak/sum (vendor) punishes distributed harmonics; peak/second-peak
    // directly measures disambiguation quality.
    float secondPeak = 0.0f;
    for (uint16_t i = 0; i < NUM_TEMPI; ++i) {
        if (i != top_i && tempi_smooth[i] > secondPeak) {
            secondPeak = tempi_smooth[i];
        }
    }
    const float winnerSeparation = (secondPeak > m_tp.wsSepFloor)
        ? clamp01(1.0f - (secondPeak / top_mag))
        : (top_mag > m_tp.wsSepFloor ? 1.0f : 0.0f);

    out.tempo_confidence = fmaxf(rawConfidence,
                           fmaxf(stabilityConfidence, winnerSeparation));
    out.beat_strength = clamp01(top_mag);

    // Beat tick: free-running phase accumulator (replicates pre-v3 EsBeatClock).
    // The Goertzel's phase_inverted is broken as a tick source because
    // calculate_magnitude_of_tempo() overwrites phase every ~192ms, faster than
    // sync_beat_phase() can complete a beat cycle (~462ms at 130 BPM).
    // Instead, drive a simple 0..1 phase ramp from the stable BPM.
    const float dt = (m_lastRefreshUs == 0) ? 0.0f
        : static_cast<float>(now_us - m_lastRefreshUs) / 1000000.0f;
    m_lastRefreshUs = now_us;
    const float phaseInc = (bpm / 60.0f) * dt;
    m_beatPhase += phaseInc;
    bool tick = false;
    if (m_beatPhase >= 1.0f) {
        m_beatPhase -= floorf(m_beatPhase);
        tick = true;
    }
    out.beat_tick = tick;
    // Convert 0..1 beat phase to [-PI, PI] for downstream consumers
    out.phase_radians = (m_beatPhase * 2.0f - 1.0f) * static_cast<float>(M_PI);

    if (tick) {
        m_beatInBar = static_cast<uint8_t>((m_beatInBar + 1) % 4);
    }

    // Diagnostic: dump selection chain + cumulative tick counter
    {
        static uint64_t lastDiagUs = 0;
        static uint32_t tickCount = 0;
        static uint32_t tickCountAtLastDiag = 0;
        if (tick) ++tickCount;
        if (now_us - lastDiagUs >= 2000000ULL) {
            const uint32_t ticksInWindow = tickCount - tickCountAtLastDiag;
            const float diagDt = (lastDiagUs == 0) ? 2.0f
                : static_cast<float>(now_us - lastDiagUs) / 1000000.0f;
            const float tickRate = static_cast<float>(ticksInWindow) / diagDt;
            tickCountAtLastDiag = tickCount;
            lastDiagUs = now_us;
            const uint16_t raw_i = esv11_pick_top_tempo_bin_raw();
            const uint16_t octave_i = esv11_pick_top_tempo_bin_octave_aware();
            const float holdAge = static_cast<float>(now_us - m_stableBinLockedUs) / 1000000.0f;
            ESP_LOGD("Tempo", "diag raw=%.0f oct=%.0f stab=%.0f hold=%.1fs tks=%lu rC=%.2f stC=%.2f wS=%.2f cC=%.2f v=%d",
                static_cast<float>(TEMPO_LOW + raw_i),
                static_cast<float>(TEMPO_LOW + octave_i),
                bpm, holdAge,
                (unsigned long)ticksInWindow,
                rawConfidence, stabilityConfidence, winnerSeparation,
                out.tempo_confidence, m_stableBinValidated ? 1 : 0);
        }
    }

    // Waveform: downsample latest 128 from ES float sample_history into int16.
    // ES sample_history is float -1..1.
    const uint16_t N = 128;
    const float* tail = &sample_history[SAMPLE_HISTORY_LENGTH - N];
    for (uint16_t i = 0; i < N; ++i) {
        float s = tail[i];
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        out.waveform[i] = static_cast<int16_t>(s * 32767.0f);
    }

    m_latest = out;
}

void EsV11Backend::getLatestOutputs(EsV11Outputs& out) const
{
    out = m_latest;
}

const float* EsV11Backend::getSampleHistory() const
{
    return sample_history;  // Vendor global (EsV11Buffers.h)
}

size_t EsV11Backend::getSampleHistoryLength() const
{
    return SAMPLE_HISTORY_LENGTH;  // Vendor define (global_defines.h / 32kHz shim)
}

} // namespace lightwaveos::audio::esv11

#endif
