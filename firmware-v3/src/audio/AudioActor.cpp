/**
 * @file AudioActor.cpp
 * @brief Actor implementation for audio capture and processing
 *
 * Phase 1 Implementation:
 * - Initializes AudioCapture on Core 0
 * - Captures 256-sample hops every 16ms tick
 * - Tracks capture statistics
 * - Does NOT process audio (deferred to Phase 2)
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "AudioActor.h"
#include "../core/bus/MessageBus.h"

#if FEATURE_AUDIO_SYNC

#include <cstring>
#include <cmath>
#include <algorithm>

#ifndef NATIVE_BUILD
#include <Arduino.h>  // For Serial in one-shot debug methods
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

// Always include macros - they define no-ops when FEATURE_AUDIO_BENCHMARK is disabled
#include "AudioBenchmarkMacros.h"

// MabuTrace integration for Perfetto timeline visualization
// No-ops when FEATURE_MABUTRACE is disabled
#include "AudioBenchmarkTrace.h"

// Unified logging system (preserves colored output conventions)
#define LW_LOG_TAG "Audio"
#include "utils/Log.h"

// Runtime-configurable audio debug verbosity
#include "AudioDebugConfig.h"

// TempoTracker integration
#if !FEATURE_AUDIO_BACKEND_ESV11 && !FEATURE_AUDIO_BACKEND_PIPELINECORE
#include "tempo/TempoTracker.h"
#endif

// Perceptual band weights for spectral flux calculation (derived from K1 research)
// Bass bands weighted higher for better kick detection
namespace {
    constexpr float PERCEPTUAL_BAND_WEIGHTS[8] = {
        1.4f,   // Band 0: Sub-bass (20-40Hz) - critical for kick drums
        1.3f,   // Band 1: Bass (40-80Hz) - fundamental bass notes
        1.0f,   // Band 2: Low-mid (80-160Hz) - bass harmonics
        0.9f,   // Band 3: Mid (160-320Hz) - lower vocals, snare body
        0.8f,   // Band 4: Upper-mid (320-640Hz) - vocals, instruments
        0.6f,   // Band 5: Presence (640-1280Hz) - clarity frequencies
        0.4f,   // Band 6: Brilliance (1280-2560Hz) - sibilance, hi-hats
        0.3f    // Band 7: Air (2560-5120Hz) - sparkle, treble transients
    };
    constexpr float PERCEPTUAL_BAND_WEIGHT_SUM =
        1.4f + 1.3f + 1.0f + 0.9f + 0.8f + 0.6f + 0.4f + 0.3f;  // 6.7f

    inline bool shouldRunSbSidecar(uint32_t hopCount) {
#if FEATURE_SB_PARITY_SIDECAR
#if AUDIO_SB_SIDECAR_DECIMATION > 1
        return (hopCount % static_cast<uint32_t>(AUDIO_SB_SIDECAR_DECIMATION)) == 0U;
#else
        (void)hopCount;
        return true;
#endif
#else
        (void)hopCount;
        return false;
#endif
    }

#if FEATURE_TRANSLATION_ENGINE
    inline float clamp01(float x) {
        if (x < 0.0f) return 0.0f;
        if (x > 1.0f) return 1.0f;
        return x;
    }

    inline float clampf(float x, float lo, float hi) {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    inline float computeTranslationDeltaSeconds(
        const lightwaveos::audio::ControlBusFrame& frame,
        lightwaveos::audio::AudioTime& previous,
        bool& hasPrevious
    ) {
        const uint32_t sampleRate = (frame.t.sample_rate_hz > 0U)
            ? frame.t.sample_rate_hz
            : lightwaveos::audio::SAMPLE_RATE;
        float dt = static_cast<float>(lightwaveos::audio::HOP_SIZE) / static_cast<float>(sampleRate);
        if (hasPrevious) {
            const float measured = lightwaveos::audio::AudioTime_SecondsBetween(previous, frame.t);
            if (measured > 0.0f) {
                dt = measured;
            }
        }
        previous = frame.t;
        hasPrevious = true;
        return clampf(dt, 0.0005f, 0.05f);
    }

    inline lightwaveos::audio::AudioFeatures buildTranslationFeatures(
        const lightwaveos::audio::ControlBusFrame& frame
    ) {
        lightwaveos::audio::AudioFeatures features{};
        features.hop_sequence = frame.hop_seq;
        features.rms = clamp01(frame.rms);
        features.fast_rms = clamp01(frame.fast_rms);
        features.flux = clamp01(frame.flux);
        features.fast_flux = clamp01(frame.fast_flux);
        features.bass = clamp01((frame.bands[0] + frame.bands[1]) * 0.5f);
        features.mid = clamp01((frame.bands[2] + frame.bands[3] + frame.bands[4]) / 3.0f);
        features.treble = clamp01((frame.bands[5] + frame.bands[6] + frame.bands[7]) / 3.0f);
        features.harmonic_saliency = clamp01(frame.saliency.harmonicNoveltySmooth);
        features.rhythmic_saliency = clamp01(frame.saliency.rhythmicNoveltySmooth);
        features.timbral_saliency = clamp01(frame.saliency.timbralNoveltySmooth);
        features.dynamic_saliency = clamp01(frame.saliency.dynamicNoveltySmooth);
        features.liveliness = clamp01(frame.liveliness);
        features.tempo_bpm = frame.tempoBpm;
        features.tempo_confidence = clamp01(frame.tempoConfidence);
        features.beat_strength = clamp01(frame.tempoBeatStrength);
        features.beat_tick = frame.tempoBeatTick;
        features.downbeat_tick = frame.tempoDownbeatTick;
        features.snare_trigger = frame.snareTrigger;
        features.hihat_trigger = frame.hihatTrigger;
        features.is_silent = frame.isSilent;
        features.silent_scale = clamp01(frame.silentScale);
        features.style = frame.currentStyle;
        features.style_confidence = clamp01(frame.styleConfidence);
        features.chord_root = static_cast<uint8_t>(frame.chordState.rootNote % 12u);
        features.chord_type = frame.chordState.type;
        features.chord_confidence = clamp01(frame.chordState.confidence);
        return features;
    }

#if FEATURE_TRANSLATION_DEBUG
    inline float computeChangeScoreEstimate(const lightwaveos::audio::TranslationState& state) {
        const float beatCountEstimate =
            static_cast<float>(state.bar_in_phrase * 4u + state.beat_in_bar + 1u);
        const float inv = 1.0f / std::max(1.0f, beatCountEstimate);
        const float meanEnergy = state.phrase_energy_accum * inv;
        const float meanFlux = state.phrase_flux_accum * inv;
        const float meanHarmonic = state.phrase_harmonic_accum * inv;
        return clamp01(
            0.45f * fabsf(meanEnergy - state.previous_phrase_energy) +
            0.30f * fabsf(meanFlux - state.previous_phrase_flux) +
            0.25f * fabsf(meanHarmonic - state.previous_phrase_harmonic)
        );
    }

    inline float computeRefractoryRemaining(const lightwaveos::audio::TranslationState& state) {
        const float bpm = clampf(state.held_bpm, 60.0f, 180.0f);
        const float beatPeriod = 60.0f / bpm;
        const float refractory = std::max(0.06f, 0.25f * beatPeriod);
        return std::max(0.0f, refractory - state.time_since_last_onset_s);
    }

    inline void maybeLogTranslationSnapshot(
        const lightwaveos::audio::AudioFeatures& features,
        const lightwaveos::audio::TranslationState& state,
        float silenceSourceRms,
        bool lastLatchValid,
        float lastLatchBpm,
        float lastLatchConfidence,
        uint64_t lastLatchUs,
        bool lastLatchTempoLocked,
        bool lastLatchBeatTick,
        bool lastLatchDownbeatTick,
        uint64_t nowUs,
        uint64_t& lastLogUs
    ) {
        if (nowUs == 0ULL) {
            return;
        }
        if (lastLogUs != 0ULL && (nowUs - lastLogUs) < 2000000ULL) {
            return;
        }
        lastLogUs = nowUs;
        LW_LOGD(
            "xlat in rms=%.3f srcsil=%.3f frms=%.3f flux=%.3f fflux=%.3f bpm=%.1f conf=%.3f beat=%.3f tick=%u down=%u silent=%u sscale=%.3f",
            features.rms,
            silenceSourceRms,
            features.fast_rms,
            features.flux,
            features.fast_flux,
            features.tempo_bpm,
            features.tempo_confidence,
            features.beat_strength,
            features.beat_tick ? 1u : 0u,
            features.downbeat_tick ? 1u : 0u,
            features.is_silent ? 1u : 0u,
            features.silent_scale
        );
        LW_LOGD("xlat st motion=%u timing=%u held_bpm=%.1f hold_age=%.2f phrase=%.3f bpulse=%.3f bright=%.3f change=%.3f refr=%.3f",
                static_cast<unsigned>(state.current_motion),
                state.timing_reliable ? 1u : 0u,
                state.held_bpm,
                state.held_bpm_age_s,
                state.phrase_progress,
                state.beat_pulse,
                state.brightness_scale,
                computeChangeScoreEstimate(state),
                computeRefractoryRemaining(state));
        const float lastLatchAgeSeconds =
            (lastLatchValid && nowUs >= lastLatchUs)
                ? static_cast<float>(nowUs - lastLatchUs) / 1000000.0f
                : 0.0f;
        LW_LOGD(
            "xlat lat valid=%u bpm=%.1f conf=%.3f age=%.2f lock=%u tick=%u down=%u",
            lastLatchValid ? 1u : 0u,
            lastLatchBpm,
            lastLatchConfidence,
            lastLatchAgeSeconds,
            lastLatchTempoLocked ? 1u : 0u,
            lastLatchBeatTick ? 1u : 0u,
            lastLatchDownbeatTick ? 1u : 0u
        );
    }

    inline void recordTranslationLatch(
        const lightwaveos::audio::AudioFeatures& features,
        bool tempoLocked,
        bool latchDetected,
        uint64_t nowUs,
        bool& lastLatchValid,
        float& lastLatchBpm,
        float& lastLatchConfidence,
        uint64_t& lastLatchUs,
        bool& lastLatchTempoLocked,
        bool& lastLatchBeatTick,
        bool& lastLatchDownbeatTick
    ) {
        if (!latchDetected || nowUs == 0ULL) {
            return;
        }
        lastLatchValid = true;
        lastLatchBpm = features.tempo_bpm;
        lastLatchConfidence = features.tempo_confidence;
        lastLatchUs = nowUs;
        lastLatchTempoLocked = tempoLocked;
        lastLatchBeatTick = features.beat_tick;
        lastLatchDownbeatTick = features.downbeat_tick;
    }

    inline void maybeLogTranslationEvent(
        const lightwaveos::audio::AudioFeatures& features,
        const lightwaveos::audio::TranslationState& state,
        float silenceSourceRms,
        bool tempoLocked,
        float previousHeldBpm,
        bool previousTimingReliable,
        uint64_t nowUs
    ) {
        if (nowUs == 0ULL) {
            return;
        }
        const bool heldChanged = fabsf(state.held_bpm - previousHeldBpm) > 0.05f;
        const bool timingChanged = state.timing_reliable != previousTimingReliable;
        if (!heldChanged && !timingChanged) {
            return;
        }
        LW_LOGD(
            "xlat evt heldchg=%u timechg=%u motion=%u timing=%u bpm=%.1f conf=%.3f lock=%u tick=%u down=%u srcsil=%.3f held=%.1f hold_age=%.2f",
            heldChanged ? 1u : 0u,
            timingChanged ? 1u : 0u,
            static_cast<unsigned>(state.current_motion),
            state.timing_reliable ? 1u : 0u,
            features.tempo_bpm,
            features.tempo_confidence,
            tempoLocked ? 1u : 0u,
            features.beat_tick ? 1u : 0u,
            features.downbeat_tick ? 1u : 0u,
            silenceSourceRms,
            state.held_bpm,
            state.held_bpm_age_s
        );
    }
#endif
#endif
}

#ifndef NATIVE_BUILD
#include <esp_timer.h>
#else
// Native build stubs for ESP-IDF APIs
inline uint64_t esp_timer_get_time() { return 0; }
inline uint32_t esp_log_timestamp() { return 0; }
#endif

namespace lightwaveos {
namespace audio {

#if !FEATURE_AUDIO_BACKEND_ESV11
AudioActor::ZoneAgcSnapshot AudioActor::getZoneAgcSnapshot() const {
    ZoneAgcSnapshot snapshot;
#ifndef NATIVE_BUILD
    portENTER_CRITICAL(&m_controlBusApiMux);
#endif
    snapshot.enabled = m_controlBus.getZoneAGCEnabled();
    snapshot.lookaheadEnabled = m_controlBus.getLookaheadEnabled();
    for (uint8_t z = 0; z < CONTROLBUS_NUM_ZONES; ++z) {
        snapshot.followers[z] = m_controlBus.getZoneFollower(z);
        snapshot.maxMags[z] = m_controlBus.getZoneMaxMag(z);
    }
#ifndef NATIVE_BUILD
    portEXIT_CRITICAL(&m_controlBusApiMux);
#endif
    return snapshot;
}

void AudioActor::setZoneAgcEnabled(bool enabled) {
#ifndef NATIVE_BUILD
    portENTER_CRITICAL(&m_controlBusApiMux);
#endif
    m_controlBus.setZoneAGCEnabled(enabled);
#ifndef NATIVE_BUILD
    portEXIT_CRITICAL(&m_controlBusApiMux);
#endif
}

void AudioActor::setLookaheadEnabled(bool enabled) {
#ifndef NATIVE_BUILD
    portENTER_CRITICAL(&m_controlBusApiMux);
#endif
    m_controlBus.setLookaheadEnabled(enabled);
#ifndef NATIVE_BUILD
    portEXIT_CRITICAL(&m_controlBusApiMux);
#endif
}

void AudioActor::setZoneAgcRates(float attackRate, float releaseRate) {
#ifndef NATIVE_BUILD
    portENTER_CRITICAL(&m_controlBusApiMux);
#endif
    m_controlBus.setZoneAGCRates(attackRate, releaseRate);
#ifndef NATIVE_BUILD
    portEXIT_CRITICAL(&m_controlBusApiMux);
#endif
}

void AudioActor::setZoneMinFloor(float minFloor) {
#ifndef NATIVE_BUILD
    portENTER_CRITICAL(&m_controlBusApiMux);
#endif
    m_controlBus.setZoneMinFloor(minFloor);
#ifndef NATIVE_BUILD
    portEXIT_CRITICAL(&m_controlBusApiMux);
#endif
}

SpikeDetectionStats AudioActor::getSpikeDetectionStats() const {
    SpikeDetectionStats stats;
#ifndef NATIVE_BUILD
    portENTER_CRITICAL(&m_controlBusApiMux);
#endif
    stats = m_controlBus.getSpikeStats();
#ifndef NATIVE_BUILD
    portEXIT_CRITICAL(&m_controlBusApiMux);
#endif
    return stats;
}

void AudioActor::resetSpikeDetectionStats() {
#ifndef NATIVE_BUILD
    portENTER_CRITICAL(&m_controlBusApiMux);
#endif
    m_controlBus.resetSpikeStats();
#ifndef NATIVE_BUILD
    portEXIT_CRITICAL(&m_controlBusApiMux);
#endif
}

ControlBusFrame AudioActor::getControlBusFrameSnapshot() const {
    ControlBusFrame frame;
#ifndef NATIVE_BUILD
    portENTER_CRITICAL(&m_controlBusApiMux);
#endif
    frame = m_controlBus.GetFrameRef();
#ifndef NATIVE_BUILD
    portEXIT_CRITICAL(&m_controlBusApiMux);
#endif
    return frame;
}
#endif  // !FEATURE_AUDIO_BACKEND_ESV11

#if FEATURE_AUDIO_BACKEND_ESV11

// ============================================================================
// ES v1.1_320 Backend Implementation
// ============================================================================

AudioActor::AudioActor()
    : Actor(ActorConfigs::Audio())
    , m_state(AudioActorState::UNINITIALIZED)
{
    m_stats.reset();
    m_diag.reset();
    m_esAdapter.reset();
}

AudioActor::~AudioActor() = default;

void AudioActor::pause()
{
    if (m_state == AudioActorState::RUNNING) {
        LW_LOGI("Pausing ES v1.1 audio backend");
        m_state = AudioActorState::PAUSED;
    }
}

void AudioActor::resume()
{
    if (m_state == AudioActorState::PAUSED) {
        LW_LOGI("Resuming ES v1.1 audio backend");
        m_state = AudioActorState::RUNNING;
    }
}

void AudioActor::resetStats()
{
    m_stats.reset();
    m_diag.reset();
    m_sampleIndex = 0;
    m_hopCount = 0;
    m_esHopSeq = 0;
    m_esChunkCounter = 0;
    m_esAdapter.reset();
}

void AudioActor::printDiagnostics()
{
    LW_LOGI("ES v1.1 audio backend: chunks=%lu publishes=%lu",
            (unsigned long)m_stats.tickCount, (unsigned long)m_diag.publishCount);
}

void AudioActor::printStatus()
{
#ifndef NATIVE_BUILD
    ControlBusFrame latest{};
    m_controlBusBuffer.ReadLatest(latest);
    Serial.println("=== Audio Status (ES v1.1 backend) ===");
    Serial.printf("  RMS: %.3f  Flux: %.3f\n", latest.rms, latest.flux);
    Serial.printf("  BPM: %.1f  Conf: %.3f  BeatTick: %d\n",
                  latest.es_bpm, latest.es_tempo_confidence, latest.es_beat_tick ? 1 : 0);
    Serial.printf("  Onset: in=%.5f floor=%.5f act=%.3f gate[abs=%u act=%u prev=%u warm=%u] flux=%.3f env=%.3f evt=%.3f k/s/h=%u/%u/%u us=%u\n",
                  m_lastOnsetInputRms,
                  m_lastOnsetNoiseFloor,
                  m_lastOnsetActivity,
                  (m_lastOnsetGateFlags & ONSET_GATE_ABS_RMS) ? 1u : 0u,
                  (m_lastOnsetGateFlags & ONSET_GATE_ACTIVITY) ? 1u : 0u,
                  (m_lastOnsetGateFlags & ONSET_GATE_NO_PREV) ? 1u : 0u,
                  (m_lastOnsetGateFlags & ONSET_GATE_WARMUP) ? 1u : 0u,
                  latest.onsetFlux,
                  latest.onsetEnv,
                  latest.onsetEvent,
                  latest.kickTrigger ? 1u : 0u,
                  latest.snareTrigger ? 1u : 0u,
                  latest.hihatTrigger ? 1u : 0u,
                  latest.onsetProcessUs);
#endif
}

void AudioActor::printSpectrum()
{
#ifndef NATIVE_BUILD
    ControlBusFrame latest{};
    m_controlBusBuffer.ReadLatest(latest);
    Serial.println("=== Spectrum (ES v1.1 backend) ===");
    Serial.print("  Bands:");
    for (int i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        Serial.printf(" %.3f", latest.bands[i]);
    }
    Serial.println();
#endif
}

void AudioActor::printBeat()
{
#ifndef NATIVE_BUILD
    ControlBusFrame latest{};
    m_controlBusBuffer.ReadLatest(latest);
    Serial.println("=== Beat (ES v1.1 backend) ===");
    Serial.printf("  BPM: %.1f  Conf: %.3f  Phase01@t: %.3f  BeatInBar: %u\n",
                  latest.es_bpm, latest.es_tempo_confidence, latest.es_phase01_at_audio_t,
                  (unsigned)latest.es_beat_in_bar);
#endif
}

const int16_t* AudioActor::getLastHop() const
{
    return nullptr;
}

bool AudioActor::hasNewHop()
{
    return false;
}

void AudioActor::onStart()
{
    m_state = AudioActorState::INITIALIZING;
    m_diag.reset();
    m_diag.diagStartTimeUs = esp_timer_get_time();
#if FEATURE_TRANSLATION_ENGINE
    translation_init(&m_translationState);
    m_translationLastAudioTime = AudioTime{};
    m_translationHaveLastAudioTime = false;
    m_smoothedTempoConfidence = 0.0f;
    m_translationLastDebugLogUs = 0ULL;
#if FEATURE_TRANSLATION_DEBUG
    m_translationLastLatchValid = false;
    m_translationLastLatchBpm = 0.0f;
    m_translationLastLatchConfidence = 0.0f;
    m_translationLastLatchUs = 0ULL;
    m_translationLastLatchTempoLocked = false;
    m_translationLastLatchBeatTick = false;
    m_translationLastLatchDownbeatTick = false;
#endif
#endif
    m_standardBeatInBar = 0u;

    if (!m_esBackend.init()) {
        m_state = AudioActorState::ERROR;
        LW_LOGE("ES v1.1 backend init failed");
        return;
    }

    // Configure Stage B derived features (silence detection).
    // Schmitt trigger: rmsUngated < threshold starts a hold timer.
    // After hysteresis expires, silentScale fades to 0 (tau=0.19s, ~400ms).
    // Total silence-to-black: ~150ms hold + ~400ms fade = ~550ms.
    // Recovery is instant (silentScale snaps to 1.0 on first non-silent frame).
    constexpr float kEsSilenceThreshold = 0.005f;
    constexpr float kEsSilenceHysteresisMs = 150.0f;
#ifdef AUDIO_SILENCE_GATE_DISABLED
    m_controlBus.setSilenceParameters(kEsSilenceThreshold, 0.0f);  // Disabled
#else
    m_controlBus.setSilenceParameters(kEsSilenceThreshold, kEsSilenceHysteresisMs);
#endif

    // Retune ControlBus smoothing for current frame rate (50 Hz or 125 Hz)
    m_controlBus.setMoodSmoothing(128);  // Balanced default

    // Initialise FFT onset detector (1024-point, runs every hop)
    m_onsetDetector.init();
    LW_LOGI("OnsetDetector: 1024-point FFT (esp-dsp), 31.25 Hz/bin, 512 bins");

    m_state = AudioActorState::RUNNING;
    LW_LOGI("ES v1.1 audio backend: INITIALISED (%.0f Hz hop rate)", audio::HOP_RATE_HZ);
}

void AudioActor::onMessage(const actors::Message& msg)
{
    // Keep minimal: shutdown is handled by Actor base; ignore other messages.
    (void)msg;
}

void AudioActor::onTick()
{
    if (m_state != AudioActorState::RUNNING) {
        // In self-clocked mode, avoid hot looping when paused/error.
        sleep(5);
        return;
    }

    const uint64_t now_us = esp_timer_get_time();
    m_stats.tickCount++;

    // Chunk processing blocks on I2S read (~5ms at 12.8kHz, 64 samples)
    m_diag.captureAttempts++;
    TRACE_BEGIN("i2s_dma_read");
    if (!m_esBackend.readAndProcessChunk(now_us)) {
        TRACE_END();
        m_stats.captureFailCount++;
        m_diag.captureReadErrors++;
        vTaskDelay(1);  // Block to let IDLE0 feed watchdog (taskYIELD insufficient - only yields to equal/higher priority)
        return;
    }
    TRACE_END();  // i2s_dma_read
    m_stats.captureSuccessCount++;

    // CRITICAL: vTaskDelay(1) blocks for one tick, letting IDLE0 feed the watchdog.
    // taskYIELD() is WRONG here - it only yields to equal/higher priority tasks,
    // but IDLE runs at priority 0 while Audio runs at priority 4.
    vTaskDelay(1);

    // Publish at hop rate: every N chunks where N = ceil(HOP_SIZE / CHUNK_SIZE)
    // 12.8kHz: 256/64 = 4 chunks → 50 Hz publish
    // 32kHz:   256/128 = 2 chunks → 125 Hz publish
    constexpr uint8_t CHUNKS_PER_HOP = (audio::HOP_SIZE + audio::ESV11_CHUNK_SIZE - 1) / audio::ESV11_CHUNK_SIZE;
    m_esChunkCounter++;
    if (m_esChunkCounter < CHUNKS_PER_HOP) {
        return;
    }
    m_esChunkCounter = 0;

    TRACE_BEGIN("controlbus_build");
    esv11::EsV11Outputs es{};
    m_esBackend.getLatestOutputs(es);
    m_sampleIndex = es.sample_index;

    ControlBusFrame frame{};
    m_esHopSeq++;
    m_esAdapter.buildFrame(frame, es, m_esHopSeq);

    // CLOCK SPINE FIX (ES backend): Ensure frame.t uses END-OF-HOP semantics
    // es.sample_index from EsV11Backend represents the post-hop sample index
    frame.t = AudioTime(es.sample_index, audio::SAMPLE_RATE, now_us);

    // ========================================================================
    // Raw PCM hop RMS (pre-AGC) — used by onset detector AND BR eligibility gate.
    // Computed once, consumed by both subsystems below.
    // ========================================================================
    float rawHopRms = 0.0f;
    {
        const float* history = m_esBackend.getSampleHistory();
        const size_t histLen = m_esBackend.getSampleHistoryLength();
        if (history != nullptr && histLen >= HOP_SIZE) {
            const float* hopTail = history + histLen - HOP_SIZE;
            double sumSq = 0.0;
            for (uint16_t i = 0; i < HOP_SIZE; ++i) {
                const double s = static_cast<double>(hopTail[i]);
                sumSq += s * s;
            }
            rawHopRms = static_cast<float>(std::sqrt(sumSq / static_cast<double>(HOP_SIZE)));
        }
    }

    // ========================================================================
    // FFT Onset Detection (1024-point spectral flux, every hop)
    // ========================================================================
    {
        const float* history = m_esBackend.getSampleHistory();
        const size_t histLen = m_esBackend.getSampleHistoryLength();
        constexpr uint16_t ONSET_FFT_SIZE = 1024;

        if (history != nullptr && histLen >= ONSET_FFT_SIZE) {
            // Point to last 1024 contiguous samples in the history buffer
            const float* tail = history + histLen - ONSET_FFT_SIZE;

            TRACE_BEGIN("onset_detect");
            OnsetResult onset = m_onsetDetector.process(tail, rawHopRms);
            TRACE_END();
            TRACE_COUNTER("onset_input_rms", static_cast<int32_t>(onset.input_rms * 1000000.0f));
            TRACE_COUNTER("onset_noise_floor", static_cast<int32_t>(onset.noise_floor * 1000000.0f));
            TRACE_COUNTER("onset_activity", static_cast<int32_t>(onset.activity * 1000.0f));
            TRACE_COUNTER("onset_gate_flags", static_cast<int32_t>(onset.gate_flags));
            TRACE_COUNTER("onset_flux", static_cast<int32_t>(onset.flux * 1000.0f));
            TRACE_COUNTER("onset_env", static_cast<int32_t>(onset.onset_env * 1000.0f));
            TRACE_COUNTER("onset_event_strength", static_cast<int32_t>(onset.onset_event * 1000.0f));
            TRACE_COUNTER("onset_bass_flux", static_cast<int32_t>(onset.bass_flux * 1000.0f));
            TRACE_COUNTER("onset_mid_flux", static_cast<int32_t>(onset.mid_flux * 1000.0f));
            TRACE_COUNTER("onset_high_flux", static_cast<int32_t>(onset.high_flux * 1000.0f));
            TRACE_COUNTER("onset_process_us", static_cast<int32_t>(onset.process_us));
            if (onset.onset_event > 0.0f) TRACE_INSTANT("ONSET_EVENT");
            if (onset.kick_trigger) TRACE_INSTANT("ONSET_KICK");
            if (onset.snare_trigger) TRACE_INSTANT("ONSET_SNARE");
            if (onset.hihat_trigger) TRACE_INSTANT("ONSET_HIHAT");

            m_lastOnsetInputRms = onset.input_rms;
            m_lastOnsetNoiseFloor = onset.noise_floor;
            m_lastOnsetActivity = onset.activity;
            m_lastOnsetGateFlags = onset.gate_flags;

            // Merge onset results into ControlBusFrame
            frame.onsetFlux      = onset.flux;
            frame.onsetEnv       = onset.onset_env;
            frame.onsetEvent     = onset.onset_event;
            frame.onsetBassFlux  = onset.bass_flux;
            frame.onsetMidFlux   = onset.mid_flux;
            frame.onsetHighFlux  = onset.high_flux;
            frame.onsetProcessUs = onset.process_us;

            // FFT onset triggers demoted to telemetry — NOT published to ControlBus.
            // Band-energy ratio detector below is the live trigger source.
        }
    }

    // ========================================================================
    // Band-energy ratio detector (Path B — live trigger source)
    //
    // Variance-adaptive threshold on grouped band energies from EsV11Adapter.
    // Runs AFTER the adapter has populated frame.bands[0..7].
    //
    // Front-door eligibility: raw PCM RMS must exceed brRmsGate.
    // This separates "any acoustic energy?" (raw RMS) from "which percussion
    // event?" (band ratio).  AGC-normalised bands cannot answer the first
    // question — silence energy overlaps music energy in that domain.
    //
    // Ref: Patin, Parallelcube, WLED Sound Reactive.
    // ========================================================================
    {
        const float kickEnergy  = frame.bands[0] + frame.bands[1];
        const float snareEnergy = frame.bands[2] + frame.bands[3];
        const float hihatEnergy = frame.bands[5] + frame.bands[6] + frame.bands[7];

        // RMS gate with hold: once RMS exceeds threshold, gate stays open
        // for brGateHold frames after RMS drops.  Bridges inter-beat gaps
        // (e.g. 435ms at 138 BPM) without flickering.
        if (rawHopRms > m_bandRatioCfg.brRmsGate) {
            m_brGateHoldCounter = m_bandRatioCfg.brGateHold;
        } else if (m_brGateHoldCounter > 0) {
            --m_brGateHoldCounter;
        }
        const bool rmsEligible = (m_brGateHoldCounter > 0);

        bool kickFired  = false;
        bool snareFired = false;
        bool hihatFired = false;

        if (rmsEligible) {
            kickFired  = bandRatioDetect(m_kickChannel,  kickEnergy,  m_bandRatioCfg.refractory, m_bandRatioCfg.kickAbsFloor);
            snareFired = bandRatioDetect(m_snareChannel, snareEnergy, m_bandRatioCfg.refractory, m_bandRatioCfg.snareAbsFloor);
            hihatFired = bandRatioDetect(m_hihatChannel, hihatEnergy, m_bandRatioCfg.refractory, m_bandRatioCfg.hihatAbsFloor);
        }
        m_bandRatioFrame++;

        if (kickFired)  frame.kickTrigger  = true;
        if (snareFired) frame.snareTrigger = true;
        if (hihatFired) frame.hihatTrigger = true;

        // Derived global onset = union of any band trigger
        if (kickFired || snareFired || hihatFired) {
            frame.onsetEvent = 1.0f;
        }

        TRACE_COUNTER("br_raw_rms", static_cast<int32_t>(rawHopRms * 1000000.0f));
        TRACE_COUNTER("br_rms_eligible", rmsEligible ? 1 : 0);
        TRACE_COUNTER("br_kick_energy",  static_cast<int32_t>(kickEnergy * 1000.0f));
        TRACE_COUNTER("br_snare_energy", static_cast<int32_t>(snareEnergy * 1000.0f));
        TRACE_COUNTER("br_hihat_energy", static_cast<int32_t>(hihatEnergy * 1000.0f));
        if (kickFired)  TRACE_INSTANT("BR_KICK");
        if (snareFired) TRACE_INSTANT("BR_SNARE");
        if (hihatFired) TRACE_INSTANT("BR_HIHAT");
    }

    // ========================================================================
    // Audio confidence envelope (novelty-assisted)
    //
    // Separate from BR triggers — answers "is music actively present?"
    // Uses raw RMS (physical presence) + spectral novelty (spectrum changing).
    // Quiet changing music → high novelty → confidence stays high.
    // Static fan noise at same RMS → zero novelty → confidence decays.
    //
    // Envelope: instant attack, configurable hold, exponential release.
    // ========================================================================
    {
        // Compute spectral novelty: sum of absolute band deltas frame-to-frame.
        // Uses post-AGC bands (fine here — we only care about change, not level).
        float novelty = 0.0f;
        for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
            const float delta = frame.bands[i] - m_prevBands[i];
            novelty += std::fabs(delta);
            m_prevBands[i] = frame.bands[i];
        }
        frame.spectralNovelty = novelty;

        // Determine if music is present: raw RMS above floor AND spectrum is changing.
        const bool rmsPresent = (rawHopRms > m_confidenceCfg.rmsFloor);
        const bool noveltyPresent = (novelty > m_confidenceCfg.noveltyFloor);
        const bool musicPresent = rmsPresent && noveltyPresent;

        if (musicPresent) {
            // Instant attack — confidence jumps to 1.0 immediately.
            m_audioConfidence = 1.0f;
            m_confidenceHoldCounter = m_confidenceCfg.holdFrames;
        } else if (m_confidenceHoldCounter > 0) {
            // Hold phase — confidence stays at current level.
            --m_confidenceHoldCounter;
        } else {
            // Release phase — exponential decay toward 0.
            m_audioConfidence *= (1.0f - m_confidenceCfg.releaseAlpha);
            if (m_audioConfidence < 0.001f) {
                m_audioConfidence = 0.0f;
            }
        }

        frame.audioConfidence = m_audioConfidence;

        TRACE_COUNTER("audio_confidence", static_cast<int32_t>(m_audioConfidence * 1000.0f));
        TRACE_COUNTER("spectral_novelty", static_cast<int32_t>(novelty * 1000.0f));
    }

    // ========================================================================
    // Stage B: Backend-agnostic derived features (chord, saliency, silence, liveliness)
    //
    // ES adapter produces Stage A output (normalised rms/flux/bands/chroma).
    // Bridge tempo fields from ES-specific to standard, then run Stage B.
    // ========================================================================

    // Bridge ES tempo fields → standard fields consumed by Stage B
    frame.tempoLocked = frame.es_tempo_confidence > 0.5f;
    frame.tempoConfidence = frame.es_tempo_confidence;
    // P1 seam fix: pass beat/downbeat through unconditionally.
    // The translator has its own confidence check (kTempoConfidenceReliable = 0.35)
    // and timing_reliable state machine. The old gate (&&tempoLocked, conf>0.5)
    // suppressed ALL beat events in the 0.35-0.5 confidence zone, preventing
    // BLOOM and RECOIL motion states from ever triggering.
    frame.tempoBeatTick = frame.es_beat_tick;
    frame.tempoDownbeatTick = frame.es_downbeat_tick;
    frame.tempoBpm = frame.es_bpm;
    frame.tempoBeatStrength = frame.es_beat_strength;

    // Use raw PCM RMS (pre-AGC) for the silence detector.
    // The previous approach (mean of post-AGC bands) was wrong: the bins64 AGC
    // follower (floor 0.05, max gain 20x) amplifies microphone self-noise above
    // the 0.005 silence threshold, causing the gate to reopen during true silence.
    // Raw PCM RMS from sample_history is ~0.001 during silence — well below 0.005.
    float rmsUngated = 0.0f;
    {
        const float* history = m_esBackend.getSampleHistory();
        const size_t histLen = m_esBackend.getSampleHistoryLength();
        if (history != nullptr && histLen >= audio::HOP_SIZE) {
            const float* hopTail = history + histLen - audio::HOP_SIZE;
            double sumSq = 0.0;
            for (uint16_t i = 0; i < audio::HOP_SIZE; ++i) {
                const double s = static_cast<double>(hopTail[i]);
                sumSq += s * s;
            }
            rmsUngated = static_cast<float>(std::sqrt(sumSq / static_cast<double>(audio::HOP_SIZE)));
        }
    }

    // Estimate hop dt from configured frame rate
    constexpr float ES_HOP_DT = audio::HOP_DURATION_MS / 1000.0f;

    m_controlBus.applyDerivedFeatures(frame, ES_HOP_DT, rmsUngated);
#if FEATURE_TRANSLATION_ENGINE
    {
        AudioFeatures translated = buildTranslationFeatures(frame);
        const float previousHeldBpm = m_translationState.held_bpm;
        const bool previousTimingReliable = m_translationState.timing_reliable;
        const float dtSeconds = computeTranslationDeltaSeconds(
            frame,
            m_translationLastAudioTime,
            m_translationHaveLastAudioTime
        );

        // Asymmetric confidence follower: instant attack, ~2s half-life decay.
        // Bridges confidence dips that kill timing_reliable.
        // 0.994^(dt*60): at 62Hz each hop decays by 0.994^0.97≈0.994,
        // after 2s (124 hops) retains ~48% of peak (half-life ≈ 2s).
        const float rawConf = translated.tempo_confidence;
        if (rawConf > m_smoothedTempoConfidence) {
            m_smoothedTempoConfidence = rawConf;  // instant attack
        } else {
            m_smoothedTempoConfidence *= powf(m_esBackend.tempoParams().confDecay, dtSeconds * 60.0f);
        }
        translated.tempo_confidence = m_smoothedTempoConfidence;

        translation_update(&translated, dtSeconds, &m_translationState);
        translation_get_parameters(&m_translationState, &frame.scene);
#if FEATURE_TRANSLATION_DEBUG
        const uint64_t logNowUs = (frame.t.monotonic_us != 0ULL) ? frame.t.monotonic_us : esp_timer_get_time();
        const bool latchDetected =
            (fabsf(m_translationState.held_bpm - previousHeldBpm) > 0.05f) ||
            (!previousTimingReliable && m_translationState.timing_reliable);
        recordTranslationLatch(
            translated,
            frame.tempoLocked,
            latchDetected,
            logNowUs,
            m_translationLastLatchValid,
            m_translationLastLatchBpm,
            m_translationLastLatchConfidence,
            m_translationLastLatchUs,
            m_translationLastLatchTempoLocked,
            m_translationLastLatchBeatTick,
            m_translationLastLatchDownbeatTick
        );
        maybeLogTranslationEvent(
            translated,
            m_translationState,
            clamp01(rmsUngated),
            frame.tempoLocked,
            previousHeldBpm,
            previousTimingReliable,
            logNowUs
        );
        maybeLogTranslationSnapshot(
            translated,
            m_translationState,
            clamp01(rmsUngated),
            m_translationLastLatchValid,
            m_translationLastLatchBpm,
            m_translationLastLatchConfidence,
            m_translationLastLatchUs,
            m_translationLastLatchTempoLocked,
            m_translationLastLatchBeatTick,
            m_translationLastLatchDownbeatTick,
            logNowUs,
            m_translationLastDebugLogUs
        );
#endif
    }
#else
    frame.scene = kDefaultSceneParameters;
#endif
    TRACE_END();  // controlbus_build
    TRACE_COUNTER("audio_rms", static_cast<int32_t>(frame.rms * 10000));

    TRACE_BEGIN("snapshot_publish");
    m_controlBusBuffer.Publish(frame);

    m_hopCount++;
    m_diag.publishCount++;
    m_diag.lastPublishTimeUs = now_us;

    // Track sequence gaps
    uint32_t expectedSeq = m_diag.lastPublishSeq + 1;
    if (m_diag.lastPublishSeq > 0 && frame.hop_seq != expectedSeq) {
        m_diag.publishSeqGaps++;
    }
    m_diag.lastPublishSeq = frame.hop_seq;
    TRACE_END();  // snapshot_publish

    // GROUND-TRUTH DIAGNOSTIC: 2-second periodic translator field dump.
    // Shows raw vs smoothed confidence + translator state.
    // Remove after validation. ~62 Hz hop rate → 124 hops ≈ 2s.
#if !defined(NATIVE_BUILD) && defined(LW_TEMPO_GT_LOG) && LW_TEMPO_GT_LOG
    if ((m_hopCount % 124) == 0) {
        const float rawConf = frame.es_tempo_confidence;
#if FEATURE_TRANSLATION_ENGINE
        Serial.printf("[XLAT_GT] bpm=%.1f rawC=%.3f smC=%.3f held=%.1f timing=%d tick=%d motion=%u rms=%.3f\n",
                      frame.es_bpm, rawConf, m_smoothedTempoConfidence,
                      m_translationState.held_bpm,
                      m_translationState.timing_reliable ? 1 : 0,
                      frame.es_beat_tick ? 1 : 0,
                      static_cast<unsigned>(m_translationState.current_motion),
                      frame.rms);
#else
        Serial.printf("[TEMPO_GT] bpm=%.1f conf=%.3f str=%.3f tick=%d down=%d rms=%.3f\n",
                      frame.es_bpm, rawConf,
                      frame.es_beat_strength,
                      frame.es_beat_tick ? 1 : 0,
                      frame.es_downbeat_tick ? 1 : 0,
                      frame.rms);
#endif
    }
#endif
}

void AudioActor::onStop()
{
    m_state = AudioActorState::PAUSED;
}

#elif FEATURE_AUDIO_BACKEND_PIPELINECORE

// ============================================================================
// PipelineCore Backend Implementation
// ============================================================================

// ============================================================================
// Constructor / Destructor
// ============================================================================

AudioActor::AudioActor()
    : Actor(ActorConfigs::Audio())
    , m_state(AudioActorState::UNINITIALIZED)
{
    m_stats.reset();
    memset(m_hopBuffer, 0, sizeof(m_hopBuffer));
    m_pipelineTuning = clampAudioPipelineTuning(AudioPipelineTuning{});
}

AudioActor::~AudioActor()
{
    // Base class handles task cleanup
    // AudioCapture destructor handles I2S cleanup
}

// ============================================================================
// Control Methods
// ============================================================================

void AudioActor::pause()
{
    if (m_state == AudioActorState::RUNNING) {
        LW_LOGI("Pausing audio capture (PipelineCore)");
        m_state = AudioActorState::PAUSED;
    }
}

void AudioActor::resume()
{
    if (m_state == AudioActorState::PAUSED) {
        LW_LOGI("Resuming audio capture (PipelineCore)");
        m_state = AudioActorState::RUNNING;
    }
}

void AudioActor::resetStats()
{
    m_stats.reset();
    m_capture.resetStats();
}

// ============================================================================
// One-Shot Debug Output Methods
// ============================================================================

void AudioActor::printStatus()
{
#ifndef NATIVE_BUILD
    const ControlBusFrame& frame = m_controlBus.GetFrameRef();

    Serial.println("=== Audio Status (PipelineCore) ===");
    Serial.printf("  RMS: %.4f (frame)\n", frame.rms);
    Serial.printf("  Flux: %.4f (mapped), raw=%.4f\n", frame.flux, m_lastFrame.flux);
    Serial.printf("  Tempo: %.1f BPM (locked: %s)\n",
                  m_lastFrame.tempo_bpm,
                  frame.tempoLocked ? "YES" : "no");

    const CaptureStats& cstats = m_capture.getStats();
    Serial.printf("  Captures: %lu (failed: %lu)\n", cstats.hopsCapured, m_stats.captureFailCount);
    Serial.printf("  Hops: %lu\n", (unsigned long)m_hopCount);

    // Spike stats
    auto spikeStats = m_controlBus.getSpikeStats();
    Serial.printf("  Spikes: detected=%lu corrected=%lu avg/frame=%.3f\n",
                  spikeStats.spikesDetectedBands + spikeStats.spikesDetectedChroma,
                  spikeStats.spikesCorrected,
                  spikeStats.avgSpikesPerFrame);

#if FEATURE_MUSICAL_SALIENCY
    Serial.printf("  Saliency: overall=%.3f dom=%u H=%.3f R=%.3f T=%.3f D=%.3f\n",
                  frame.saliency.overallSaliency,
                  frame.saliency.dominantType,
                  frame.saliency.harmonicNoveltySmooth,
                  frame.saliency.rhythmicNoveltySmooth,
                  frame.saliency.timbralNoveltySmooth,
                  frame.saliency.dynamicNoveltySmooth);
#endif

#if FEATURE_STYLE_DETECTION
    const StyleClassification& styleClass = m_styleDetector.getClassification();
    Serial.printf("  Style: %u conf=%.2f [R=%.2f H=%.2f M=%.2f T=%.2f D=%.2f]\n",
                  static_cast<uint8_t>(m_styleDetector.getStyle()),
                  m_styleDetector.getConfidence(),
                  styleClass.styleWeights[0],
                  styleClass.styleWeights[1],
                  styleClass.styleWeights[2],
                  styleClass.styleWeights[3],
                  styleClass.styleWeights[4]);
#endif
#endif // NATIVE_BUILD
}

void AudioActor::printSpectrum()
{
#ifndef NATIVE_BUILD
    Serial.println("=== Audio Spectrum (PipelineCore) ===");

    // 8-band from FeatureFrame
    Serial.print("  8-band: [");
    for (int i = 0; i < 8; i++) {
        Serial.printf("%.3f%s", m_lastFrame.bands[i], (i < 7) ? " " : "");
    }
    Serial.println("]");

    // Flux
    Serial.printf("  Spectral Flux: %.3f\n", m_lastFrame.flux);

    // Chroma (12 pitch classes)
    const ControlBusFrame& frame = m_controlBus.GetFrameRef();
    Serial.print("  Chroma: [");
    for (int i = 0; i < 12; i++) {
        Serial.printf("%.2f%s", frame.chroma[i], (i < 11) ? " " : "");
    }
    Serial.println("]");

    // bins256 peak
    float maxBin = 0.0f;
    uint16_t maxIdx = 0;
    for (uint16_t i = 0; i < 256; i++) {
        if (frame.bins256[i] > maxBin) { maxBin = frame.bins256[i]; maxIdx = i; }
    }
    Serial.printf("  bins256 peak: [%u]=%.4f (%.1f Hz)\n",
                  maxIdx, maxBin, maxIdx * frame.binHz);
#endif // NATIVE_BUILD
}

void AudioActor::printBeat()
{
#ifndef NATIVE_BUILD
    Serial.println("=== Beat Tracking (PipelineCore) ===");
    Serial.printf("  BPM: %.1f\n", m_lastFrame.tempo_bpm);
    Serial.printf("  Phase: %.2f\n", m_lastFrame.beat_phase);
    Serial.printf("  Beat Event: %.2f\n", m_lastFrame.beat_event);

    const ControlBusFrame& frame = m_controlBus.GetFrameRef();
    Serial.printf("  Locked: %s\n", frame.tempoLocked ? "YES" : "no");
    Serial.printf("  Confidence: %.2f\n", frame.tempoConfidence);
#endif // NATIVE_BUILD
}

void AudioActor::printDiagnostics()
{
    // Calculate rates
    uint64_t now_us = esp_timer_get_time();
    uint64_t elapsed_us = now_us - m_diag.diagStartTimeUs;
    float elapsed_s = elapsed_us / 1000000.0f;

    float captureRate = (elapsed_s > 0.1f) ? (m_diag.captureSuccesses / elapsed_s) : 0.0f;
    float publishRate = (elapsed_s > 0.1f) ? (m_diag.publishCount / elapsed_s) : 0.0f;
    float successPct = (m_diag.captureAttempts > 0)
        ? (100.0f * m_diag.captureSuccesses / m_diag.captureAttempts) : 0.0f;

    const float expectedRate = HOP_RATE_HZ;
    bool rateOk = (captureRate >= expectedRate * 0.9f && captureRate <= expectedRate * 1.1f);

    LW_LOGI("========== AUDIO PIPELINE DIAGNOSTICS (PipelineCore) ==========");

    LW_LOGI("CAPTURE: rate=%.1f Hz (expect %.1f) %s | success=%.1f%% | attempts=%lu ok=%lu",
            captureRate, expectedRate, rateOk ? "OK" : "PROBLEM",
            successPct, (unsigned long)m_diag.captureAttempts, (unsigned long)m_diag.captureSuccesses);

    if (m_diag.captureDmaTimeouts > 0 || m_diag.captureReadErrors > 0) {
        LW_LOGW("  ERRORS: DMA_timeouts=%lu read_errors=%lu",
                (unsigned long)m_diag.captureDmaTimeouts, (unsigned long)m_diag.captureReadErrors);
    }

    LW_LOGI("PUBLISH: rate=%.1f Hz | count=%lu | seq_gaps=%lu",
            publishRate, (unsigned long)m_diag.publishCount, (unsigned long)m_diag.publishSeqGaps);

    LW_LOGI("SAMPLES: raw=[%d..%d] rms=%.4f nonzero=%s zero_hops=%lu",
            m_diag.lastRawMin, m_diag.lastRawMax, m_diag.lastRawRms,
            m_diag.samplesNonZero ? "YES" : "NO",
            (unsigned long)m_diag.zeroHopCount);

    if (!m_diag.samplesNonZero || m_diag.zeroHopCount > 10) {
        LW_LOGW("  WARNING: I2S may not be receiving audio data!");
    }

    LW_LOGI("TIMING: capture avg=%lu max=%lu us | process avg=%lu max=%lu us",
            (unsigned long)m_diag.avgCaptureLatencyUs, (unsigned long)m_diag.maxCaptureLatencyUs,
            (unsigned long)m_diag.avgProcessLatencyUs, (unsigned long)m_diag.maxProcessLatencyUs);

    if (m_diag.lastPublishTimeUs > 0) {
        uint32_t frameAge_ms = (now_us - m_diag.lastPublishTimeUs) / 1000;
        LW_LOGI("FRESHNESS: last_publish=%lu ms ago | hop_seq=%lu",
                (unsigned long)frameAge_ms, (unsigned long)m_diag.lastPublishSeq);
    }

    LW_LOGI("PIPELINE: tempo=%.1f BPM phase=%.2f onset_env=%.3f",
            m_lastFrame.tempo_bpm, m_lastFrame.beat_phase, m_lastFrame.onset_env);

    bool healthy = rateOk && m_diag.samplesNonZero &&
                   m_diag.captureDmaTimeouts == 0 && m_diag.publishSeqGaps == 0;
    LW_LOGI("HEALTH: %s", healthy ? "OK - Pipeline functioning normally" : "ISSUES DETECTED - See warnings above");
    LW_LOGI("=================================================");
}

// ============================================================================
// Pipeline Tuning
// ============================================================================

AudioPipelineTuning AudioActor::getPipelineTuning() const
{
    AudioPipelineTuning out;
    uint32_t v0;
    uint32_t v1 = 0;
    do {
        v0 = m_pipelineTuningSeq.load(std::memory_order_acquire);
        if (v0 & 1U) continue;
        out = m_pipelineTuning;
        v1 = m_pipelineTuningSeq.load(std::memory_order_acquire);
    } while (v0 != v1 || (v1 & 1U));
    return out;
}

void AudioActor::setPipelineTuning(const AudioPipelineTuning& tuning)
{
    AudioPipelineTuning clamped = clampAudioPipelineTuning(tuning);
    uint32_t v = m_pipelineTuningSeq.load(std::memory_order_relaxed);
    m_pipelineTuningSeq.store(v + 1U, std::memory_order_release);
    m_pipelineTuning = clamped;
    m_pipelineTuningSeq.store(v + 2U, std::memory_order_release);
}

void AudioActor::resetDspState()
{
    m_dspResetPending.store(true, std::memory_order_release);
}

AudioDspState AudioActor::getDspState() const
{
    AudioDspState out;
    uint32_t v0;
    uint32_t v1 = 0;
    do {
        v0 = m_dspStateSeq.load(std::memory_order_acquire);
        if (v0 & 1U) continue;
        out = m_dspState;
        v1 = m_dspStateSeq.load(std::memory_order_acquire);
    } while (v0 != v1 || (v1 & 1U));
    return out;
}

// ============================================================================
// Buffer Access
// ============================================================================

const int16_t* AudioActor::getLastHop() const
{
    if (m_state == AudioActorState::RUNNING || m_state == AudioActorState::PAUSED) {
        return m_hopBuffer;
    }
    return nullptr;
}

bool AudioActor::hasNewHop()
{
    if (m_newHopAvailable) {
        m_newHopAvailable = false;
        return true;
    }
    return false;
}

// ============================================================================
// Actor Lifecycle
// ============================================================================

void AudioActor::onStart()
{
    LW_LOGI("AudioActor starting on Core %d (PipelineCore backend)", xPortGetCoreID());

    m_state = AudioActorState::INITIALIZING;
    m_stats.state = m_state;

    // Initialize diagnostics
    m_diag.reset();
    m_diag.diagStartTimeUs = esp_timer_get_time();
    m_consecutiveZeroHops = 0;
    m_lastRecoveryAttemptHop = 0;
#if FEATURE_TRANSLATION_ENGINE
    translation_init(&m_translationState);
    m_translationLastAudioTime = AudioTime{};
    m_translationHaveLastAudioTime = false;
    m_translationLastDebugLogUs = 0ULL;
#if FEATURE_TRANSLATION_DEBUG
    m_translationLastLatchValid = false;
    m_translationLastLatchBpm = 0.0f;
    m_translationLastLatchConfidence = 0.0f;
    m_translationLastLatchUs = 0ULL;
    m_translationLastLatchTempoLocked = false;
    m_translationLastLatchBeatTick = false;
    m_translationLastLatchDownbeatTick = false;
#endif
#endif
    m_standardBeatInBar = 0u;

    // Initialize I2S audio capture
    if (!m_capture.init()) {
        LW_LOGE("Failed to initialize audio capture");
        m_state = AudioActorState::ERROR;
        m_stats.state = m_state;
        return;
    }

    // Initialize PipelineCore DSP engine
    PipelineConfig cfg;
    cfg.sampleRate = SAMPLE_RATE;
    cfg.hopSize = HOP_SIZE;
    cfg.windowSize = FFT_SIZE;      // N=512 regardless of hop size
    m_pipeline.setConfig(cfg);
    LW_LOGI("PipelineCore initialized (sr=%lu, hop=%u, win=%u)",
             (unsigned long)cfg.sampleRate, cfg.hopSize, cfg.windowSize);

    // Initialize PipelineAdapter bridge
    PipelineAdapter::Config adapterCfg;
    adapterCfg.sampleRate = static_cast<float>(SAMPLE_RATE);
    adapterCfg.fftSize = FFT_SIZE;
    m_adapter.init(adapterCfg);
    LW_LOGI("PipelineAdapter initialized (binHz=%.1f)",
             adapterCfg.sampleRate / static_cast<float>(adapterCfg.fftSize));

    m_state = AudioActorState::RUNNING;
    m_stats.state = m_state;

    LW_LOGI("AudioActor started (tick=%dms, hop=%d, rate=%.1fHz)",
             AUDIO_ACTOR_TICK_MS, HOP_SIZE, HOP_RATE_HZ);
    LW_LOGI("Pipeline diagnostics enabled - will log every 10 seconds");
}

void AudioActor::onMessage(const actors::Message& msg)
{
    switch (msg.type) {
        case actors::MessageType::SHUTDOWN:
            LW_LOGI("Received SHUTDOWN message");
            break;

        case actors::MessageType::HEALTH_CHECK:
            LW_LOGD("Health check: state=%d, captures=%lu",
                     static_cast<int>(m_state), m_stats.captureSuccessCount);
            break;

        case actors::MessageType::PING:
            LW_LOGD("PING received");
            break;

        default:
            LW_LOGD("Ignoring message type 0x%02X",
                     static_cast<uint8_t>(msg.type));
            break;
    }
}

void AudioActor::onTick()
{
    // Skip if not in running state
    if (m_state != AudioActorState::RUNNING) {
        return;
    }

    m_stats.tickCount++;

    // Record tick start time
    uint64_t tickStart = esp_timer_get_time();

    // Capture one hop
    captureHop();

    // Feed the watchdog. vTaskDelay(1) blocks for one FreeRTOS tick (10ms at
    // CONFIG_FREERTOS_HZ=100), which lets IDLE0 run. taskYIELD() was tried but
    // caused all-zero DMA — likely because IDLE0 runs at priority 0 and
    // taskYIELD only yields to equal-or-higher priority tasks, starving
    // the I2S DMA housekeeping. This caps PipelineCore at ~96 Hz frame rate
    // vs 125 Hz target. Event-driven I2S (DMA interrupt → queue) is the
    // proper fix — Sprint 2.
    vTaskDelay(1);

    // Update tick timing
    m_stats.lastTickTimeUs = (uint32_t)(esp_timer_get_time() - tickStart);
    m_stats.state = m_state;
}

void AudioActor::onStop()
{
    LW_LOGI("AudioActor stopping (PipelineCore)");
    m_capture.deinit();
    m_state = AudioActorState::PAUSED;
    m_stats.state = m_state;
}

// ============================================================================
// Audio Capture
// ============================================================================

void AudioActor::captureHop()
{
    static constexpr uint32_t ZERO_HOPS_RECOVERY_THRESHOLD = 250;  // ~2s at 125 Hz
    static constexpr uint32_t RECOVERY_RETRY_GAP_HOPS = 500;       // ~4s cooldown

    m_diag.captureAttempts++;
    m_diag.lastCaptureStartUs = esp_timer_get_time();

    TRACE_BEGIN("i2s_dma_read");
    CaptureResult result = m_capture.captureHop(m_hopBuffer);
    TRACE_END();

    m_diag.lastCaptureEndUs = esp_timer_get_time();
    uint32_t captureLatency = static_cast<uint32_t>(m_diag.lastCaptureEndUs - m_diag.lastCaptureStartUs);
    if (captureLatency > m_diag.maxCaptureLatencyUs) {
        m_diag.maxCaptureLatencyUs = captureLatency;
    }
    m_diag.avgCaptureLatencyUs = (m_diag.avgCaptureLatencyUs * 7 + captureLatency) / 8;

    if (result == CaptureResult::SUCCESS) {
        m_stats.captureSuccessCount++;
        m_diag.captureSuccesses++;
        m_newHopAvailable = true;

        // DMA recovered — clear consecutive timeout counter, post recovery event
        if (m_consecutiveDmaTimeouts > 0) {
            if (m_dmaFailureSignalled) {
                m_dmaFailureSignalled = false;
                MSG_BUS.publish(actors::Message(actors::MessageType::AUDIO_FAILURE_RECOVERED));
                LW_LOGI("Audio DMA recovered after %lu timeouts", (unsigned long)m_consecutiveDmaTimeouts);
            }
            m_consecutiveDmaTimeouts = 0;
        }

        // Analyze raw samples before processing
        int16_t rawMin = 32767, rawMax = -32768;
        int64_t rawSumSq = 0;
        bool allSame = true;
        int16_t firstSample = m_hopBuffer[0];
        for (size_t i = 0; i < HOP_SIZE; ++i) {
            int16_t s = m_hopBuffer[i];
            if (s < rawMin) rawMin = s;
            if (s > rawMax) rawMax = s;
            rawSumSq += static_cast<int64_t>(s) * s;
            if (s != firstSample) allSame = false;
        }
        m_diag.lastRawMin = rawMin;
        m_diag.lastRawMax = rawMax;
        m_diag.lastRawRms = std::sqrt(static_cast<float>(rawSumSq) / HOP_SIZE) / 32768.0f;
        m_diag.samplesNonZero = !allSame && (rawMax != rawMin);
        const bool flatline = allSame || (rawMin == rawMax);
        if (flatline) {
            m_diag.zeroHopCount++;
            m_consecutiveZeroHops++;
        } else {
            m_consecutiveZeroHops = 0;
        }

        if (m_consecutiveZeroHops >= ZERO_HOPS_RECOVERY_THRESHOLD &&
            (m_hopCount - m_lastRecoveryAttemptHop) >= RECOVERY_RETRY_GAP_HOPS) {
            m_lastRecoveryAttemptHop = m_hopCount;
            if (!recoverCapturePath()) {
                m_stats.captureFailCount++;
                return;
            }
            return;
        }

        // DC diagnostic: trace-only logging (opt-in at verbosity 5).
        static uint32_t s_dcDiagCounter = 0;
        auto& dbgCfg = getAudioDebugConfig();
        if (dbgCfg.verbosity >= 5 && ++s_dcDiagCounter >= 1250) {
            s_dcDiagCounter = 0;
            int64_t dcSum = 0;
            for (size_t i = 0; i < HOP_SIZE; ++i) {
                dcSum += m_hopBuffer[i];
            }
            int32_t dcMean = static_cast<int32_t>(dcSum / HOP_SIZE);
            LW_LOGD("DC_DIAG: mean=%ld min=%d max=%d rms=%.4f zeros=%lu",
                    (long)dcMean, rawMin, rawMax, m_diag.lastRawRms,
                    (unsigned long)m_diag.zeroHopCount);
        }

        // Process the hop through PipelineCore DSP
        uint64_t processStart = esp_timer_get_time();
        processHop();
        uint64_t processEnd = esp_timer_get_time();
        m_diag.lastProcessEndUs = processEnd;

        uint32_t processLatency = static_cast<uint32_t>(processEnd - processStart);
        if (processLatency > m_diag.maxProcessLatencyUs) {
            m_diag.maxProcessLatencyUs = processLatency;
        }
        m_diag.avgProcessLatencyUs = (m_diag.avgProcessLatencyUs * 7 + processLatency) / 8;

    } else {
        m_stats.captureFailCount++;
        if (result == CaptureResult::DMA_TIMEOUT) {
            m_diag.captureDmaTimeouts++;
            m_consecutiveDmaTimeouts++;
            // DEC-011: signal sustained DMA failure after ~2s (250 hops at 125 Hz)
            if (m_consecutiveDmaTimeouts >= ZERO_HOPS_RECOVERY_THRESHOLD && !m_dmaFailureSignalled) {
                m_dmaFailureSignalled = true;
                MSG_BUS.publish(actors::Message(actors::MessageType::AUDIO_FAILURE_DETECTED));
                LW_LOGW("Audio DMA failure sustained (%lu timeouts)", (unsigned long)m_consecutiveDmaTimeouts);
            }
        } else {
            m_diag.captureReadErrors++;
        }
        handleCaptureError(result);
    }
}

bool AudioActor::recoverCapturePath()
{
    LW_LOGW("Capture flatline detected (%lu consecutive hops). Reinitialising I2S.",
            (unsigned long)m_consecutiveZeroHops);

    m_capture.deinit();
    vTaskDelay(1);

    if (!m_capture.init()) {
        LW_LOGE("I2S recovery failed");
        m_state = AudioActorState::ERROR;
        m_stats.state = m_state;
        return false;
    }

    m_consecutiveZeroHops = 0;
    LW_LOGI("I2S recovery succeeded");
    return true;
}

// ============================================================================
// PipelineCore DSP Processing
// ============================================================================

void AudioActor::processHop()
{
    TRACE_SCOPE("audio_pipeline");
    BENCH_DECL_TIMING();
    BENCH_START_FRAME();

    // Handle DSP reset requests
    if (m_dspResetPending.exchange(false, std::memory_order_acq_rel)) {
        m_pipeline.reset();
        PipelineAdapter::Config adapterCfg;
        adapterCfg.sampleRate = static_cast<float>(SAMPLE_RATE);
        adapterCfg.fftSize = FFT_SIZE;
        m_adapter.init(adapterCfg);
#if FEATURE_STYLE_DETECTION
        m_styleDetector.reset();
#endif
        m_prevChordRoot = 0;
#ifndef NATIVE_BUILD
        portENTER_CRITICAL(&m_controlBusApiMux);
#endif
        m_controlBus.Reset();
#ifndef NATIVE_BUILD
        portEXIT_CRITICAL(&m_controlBusApiMux);
#endif
#if FEATURE_TRANSLATION_ENGINE
        translation_init(&m_translationState);
        m_translationLastAudioTime = AudioTime{};
        m_translationHaveLastAudioTime = false;
        m_translationLastDebugLogUs = 0ULL;
#if FEATURE_TRANSLATION_DEBUG
        m_translationLastLatchValid = false;
        m_translationLastLatchBpm = 0.0f;
        m_translationLastLatchConfidence = 0.0f;
        m_translationLastLatchUs = 0ULL;
        m_translationLastLatchTempoLocked = false;
        m_translationLastLatchBeatTick = false;
        m_translationLastLatchDownbeatTick = false;
#endif
#endif
        m_standardBeatInBar = 0u;
    }

    // Build AudioTime with END-OF-HOP semantics
    uint64_t now_us = (m_diag.lastCaptureEndUs != 0)
        ? m_diag.lastCaptureEndUs
        : esp_timer_get_time();
    uint64_t hop_end_sample_index = m_sampleIndex + HOP_SIZE;
    AudioTime now(hop_end_sample_index, SAMPLE_RATE, now_us);
    m_sampleIndex = hop_end_sample_index;
    m_hopCount++;

    // === Phase: PipelineCore Feed ===
    BENCH_START_PHASE();
    TRACE_BEGIN("dc_agc_loop");

    // 1. Feed raw samples to PipelineCore (DC removal, FFT, features computed internally)
    uint32_t ts = static_cast<uint32_t>(m_sampleIndex * 1000000ULL / SAMPLE_RATE);
    m_pipeline.pushSamples(m_hopBuffer, HOP_SIZE, ts);

    // 2. Pull feature frame (returns true when hop has been fully processed)
    if (!m_pipeline.pullFrame(m_lastFrame)) {
        // PipelineCore hasn't accumulated a full window yet -- nothing to publish
        TRACE_END();  // dc_agc_loop
        BENCH_END_PHASE(dcAgcLoopUs);
        BENCH_END_FRAME(&m_benchmarkRing);
        return;
    }

    TRACE_END();  // dc_agc_loop
    BENCH_END_PHASE(dcAgcLoopUs);

    // === Phase: Adapter Bridge ===
    BENCH_START_PHASE();
    TRACE_BEGIN("rms_flux");

    // 3. Bridge: FeatureFrame → ControlBusRawInput
    ControlBusRawInput raw{};
    m_adapter.adapt(
        m_lastFrame,
        m_pipeline.getMagnitudeSpectrum(),
        m_pipeline.getHopBuffer(),
        raw
    );
    raw.tempoDownbeatTick = false;
    if (raw.tempoBeatTick) {
        raw.tempoDownbeatTick = (m_standardBeatInBar == 0u);
        m_standardBeatInBar = static_cast<uint8_t>((m_standardBeatInBar + 1u) % 4u);
    }

    TRACE_END();  // rms_flux
    TRACE_COUNTER("audio_rms", static_cast<int32_t>(raw.rms * 10000));
    TRACE_COUNTER("audio_pre_gain_rms", static_cast<int32_t>(m_lastFrame.rms * 10000));
    BENCH_END_PHASE(rmsComputeUs);

    // Update DSP state snapshot for diagnostics
    {
        AudioDspState state;
        state.rmsRaw = m_lastFrame.rms;
        state.rmsMapped = raw.rms;
        state.rmsPreGain = m_lastFrame.rms;
        state.fluxMapped = raw.flux;
        state.agcGain = 1.0f;   // PipelineCore doesn't expose AGC
        state.dcEstimate = 0.0f;
        state.noiseFloor = 0.0f;

        uint32_t v = m_dspStateSeq.load(std::memory_order_relaxed);
        m_dspStateSeq.store(v + 1U, std::memory_order_release);
        m_dspState = state;
        m_dspStateSeq.store(v + 2U, std::memory_order_release);
    }

    // === Phase: Sensory Bridge parity side-car ===
    BENCH_START_PHASE();

    // 4. SB parity sidecar (uses bins64 shim from adapter)
    if (shouldRunSbSidecar(m_hopCount)) {
        processSbWaveformSidecar(raw);
        processSbBloomSidecar(raw);
    }

    BENCH_END_PHASE(goertzelUs);

    // === Phase: Noise Calibration ===
    {
        uint32_t nowMs = static_cast<uint32_t>(now_us / 1000);
        processNoiseCalibration(m_lastFrame.rms, raw.bands, raw.chroma, nowMs);
    }

    // === Phase: ControlBus Update ===
    BENCH_START_PHASE();
    TRACE_BEGIN("controlbus_build");

    const AudioPipelineTuning tuning = getPipelineTuning();
#ifndef NATIVE_BUILD
    portENTER_CRITICAL(&m_controlBusApiMux);
#endif
    m_controlBus.setSmoothing(tuning.controlBusAlphaFast, tuning.controlBusAlphaSlow);
#ifdef AUDIO_SILENCE_GATE_DISABLED
    m_controlBus.setSilenceParameters(tuning.silenceThreshold, 0.0f);
#else
    m_controlBus.setSilenceParameters(tuning.silenceThreshold, tuning.silenceHysteresisMs);
#endif
    m_controlBus.UpdateFromHop(now, raw);
#ifndef NATIVE_BUILD
    portEXIT_CRITICAL(&m_controlBusApiMux);
#endif

    TRACE_END();  // controlbus_build
    BENCH_END_PHASE(controlBusUs);

    // === Phase: Style Detection ===
#if FEATURE_STYLE_DETECTION
    {
        const ControlBusFrame frameRef = getControlBusFrameSnapshot();
        bool chordChanged = (frameRef.chordState.rootNote != m_prevChordRoot);
        m_prevChordRoot = frameRef.chordState.rootNote;
        float beatConfidence = raw.tempoLocked ? raw.tempoConfidence : 0.0f;
        m_styleDetector.update(
            raw.rms,
            raw.flux,
            raw.bands,
            beatConfidence,
            chordChanged
        );
    }
#endif

    // === Phase: Publish ===
    BENCH_START_PHASE();
    TRACE_BEGIN("snapshot_publish");

    // 5. Publish frame to renderer via lock-free SnapshotBuffer
    {
        ControlBusFrame frameToPublish = getControlBusFrameSnapshot();
#if FEATURE_STYLE_DETECTION
        frameToPublish.currentStyle = m_styleDetector.getStyle();
        frameToPublish.styleConfidence = m_styleDetector.getConfidence();
#else
        frameToPublish.currentStyle = MusicStyle::UNKNOWN;
        frameToPublish.styleConfidence = 0.0f;
#endif
        // Sensory Bridge parity fields (side-car pipeline)
#if FEATURE_SB_PARITY_SIDECAR
        std::memcpy(frameToPublish.sb_waveform, m_sbWaveform, sizeof(m_sbWaveform));
        frameToPublish.sb_waveform_peak_scaled = m_sbWaveformPeakScaled;
        frameToPublish.sb_waveform_peak_scaled_last = m_sbWaveformPeakScaledLast;
        std::memcpy(frameToPublish.sb_note_chromagram, m_sbNoteChroma, sizeof(m_sbNoteChroma));
        frameToPublish.sb_chromagram_max_val = m_sbChromaMaxVal;
        std::memcpy(frameToPublish.sb_spectrogram, m_sbSpectrogram, sizeof(m_sbSpectrogram));
        std::memcpy(frameToPublish.sb_spectrogram_smooth, m_sbSpectrogramSmooth, sizeof(m_sbSpectrogramSmooth));
        std::memcpy(frameToPublish.sb_chromagram_smooth, m_sbChromagramSmooth, sizeof(m_sbChromagramSmooth));
        frameToPublish.sb_hue_position = m_sbHuePosition;
        frameToPublish.sb_hue_shifting_mix = m_sbHueShiftingMix;
#endif
#if FEATURE_TRANSLATION_ENGINE
        {
            const AudioFeatures translated = buildTranslationFeatures(frameToPublish);
            const float previousHeldBpm = m_translationState.held_bpm;
            const bool previousTimingReliable = m_translationState.timing_reliable;
            const float dtSeconds = computeTranslationDeltaSeconds(
                frameToPublish,
                m_translationLastAudioTime,
                m_translationHaveLastAudioTime
            );
            translation_update(&translated, dtSeconds, &m_translationState);
            translation_get_parameters(&m_translationState, &frameToPublish.scene);
#if FEATURE_TRANSLATION_DEBUG
            const uint64_t logNowUs = (frameToPublish.t.monotonic_us != 0ULL)
                ? frameToPublish.t.monotonic_us
                : esp_timer_get_time();
            const bool latchDetected =
                (fabsf(m_translationState.held_bpm - previousHeldBpm) > 0.05f) ||
                (!previousTimingReliable && m_translationState.timing_reliable);
            recordTranslationLatch(
                translated,
                frameToPublish.tempoLocked,
                latchDetected,
                logNowUs,
                m_translationLastLatchValid,
                m_translationLastLatchBpm,
                m_translationLastLatchConfidence,
                m_translationLastLatchUs,
                m_translationLastLatchTempoLocked,
                m_translationLastLatchBeatTick,
                m_translationLastLatchDownbeatTick
            );
            maybeLogTranslationEvent(
                translated,
                m_translationState,
                clamp01(raw.rmsUngated),
                frameToPublish.tempoLocked,
                previousHeldBpm,
                previousTimingReliable,
                logNowUs
            );
            maybeLogTranslationSnapshot(
                translated,
                m_translationState,
                clamp01(raw.rmsUngated),
                m_translationLastLatchValid,
                m_translationLastLatchBpm,
                m_translationLastLatchConfidence,
                m_translationLastLatchUs,
                m_translationLastLatchTempoLocked,
                m_translationLastLatchBeatTick,
                m_translationLastLatchDownbeatTick,
                logNowUs,
                m_translationLastDebugLogUs
            );
#endif
        }
#else
        frameToPublish.scene = kDefaultSceneParameters;
#endif
        m_controlBusBuffer.Publish(frameToPublish);

        // Track publish statistics
        m_diag.publishCount++;
        m_diag.lastPublishTimeUs = esp_timer_get_time();

        uint32_t expectedSeq = m_diag.lastPublishSeq + 1;
        if (m_diag.lastPublishSeq > 0 && frameToPublish.hop_seq != expectedSeq) {
            m_diag.publishSeqGaps++;
        }
        m_diag.lastPublishSeq = frameToPublish.hop_seq;
    }

    TRACE_END();  // snapshot_publish
    BENCH_END_PHASE(publishUs);

    // Stack high-water mark (trace-only, every ~10 seconds at 125 Hz).
    static uint32_t s_stackLogCounter = 0;
    auto& dbgCfg = getAudioDebugConfig();
    if (dbgCfg.verbosity >= 5 && ++s_stackLogCounter >= 1250) {
        s_stackLogCounter = 0;
        UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
        LW_LOGD("STACK: AudioActor high-water mark = %u words (%u bytes free)",
                (unsigned)hwm, (unsigned)(hwm * sizeof(StackType_t)));
    }

    BENCH_END_FRAME(&m_benchmarkRing);

#if FEATURE_AUDIO_BENCHMARK
    if (++m_benchmarkAggregateCounter >= BENCHMARK_AGGREGATE_INTERVAL) {
        aggregateBenchmarkStats();
        m_benchmarkAggregateCounter = 0;
    }
#endif
}

// ============================================================================
// Sensory Bridge Parity Side-Car Pipeline
// ============================================================================

void AudioActor::processSbWaveformSidecar(const ControlBusRawInput& raw)
{
    for (uint8_t i = 0; i < SB_WAVEFORM_POINTS; ++i) {
        int16_t sample = raw.waveform[i];
        m_sbWaveform[i] = sample;
        m_sbWaveformHistory[m_sbWaveformHistoryIndex][i] = sample;
    }
    m_sbWaveformHistoryIndex++;
    if (m_sbWaveformHistoryIndex >= SB_WAVEFORM_HISTORY) {
        m_sbWaveformHistoryIndex = 0;
    }

    float maxWaveformValRaw = 0.0f;
    for (uint8_t i = 0; i < SB_WAVEFORM_POINTS; ++i) {
        int16_t sample = m_sbWaveform[i];
        int16_t absSample = (sample < 0) ? -sample : sample;
        if (absSample > maxWaveformValRaw) {
            maxWaveformValRaw = static_cast<float>(absSample);
        }
    }

    float maxWaveformVal = maxWaveformValRaw - 750.0f;
    if (maxWaveformVal < 0.0f) maxWaveformVal = 0.0f;

    if (maxWaveformVal > m_sbMaxWaveformValFollower) {
        float delta = maxWaveformVal - m_sbMaxWaveformValFollower;
        m_sbMaxWaveformValFollower += delta * 0.6f;
    } else if (maxWaveformVal < m_sbMaxWaveformValFollower) {
        float delta = m_sbMaxWaveformValFollower - maxWaveformVal;
        m_sbMaxWaveformValFollower -= delta * 0.03f;
        if (m_sbMaxWaveformValFollower < 750.0f) m_sbMaxWaveformValFollower = 750.0f;
    }

    float waveformPeakScaledRaw = 0.0f;
    if (m_sbMaxWaveformValFollower > 0.0f) {
        waveformPeakScaledRaw = maxWaveformVal / m_sbMaxWaveformValFollower;
    }

    if (waveformPeakScaledRaw > m_sbWaveformPeakScaled) {
        float delta = waveformPeakScaledRaw - m_sbWaveformPeakScaled;
        m_sbWaveformPeakScaled += delta * 0.7f;
    } else if (waveformPeakScaledRaw < m_sbWaveformPeakScaled) {
        float delta = m_sbWaveformPeakScaled - waveformPeakScaledRaw;
        m_sbWaveformPeakScaled -= delta * 0.7f;
    }

    m_sbWaveformPeakScaledLast =
        (m_sbWaveformPeakScaled * 0.3f) + (m_sbWaveformPeakScaledLast * 0.7f);

    m_sbChromaMaxVal = 0.0f;
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        m_sbNoteChroma[i] = 0.0f;
    }
    for (uint8_t octave = 0; octave < 6; ++octave) {
        for (uint8_t note = 0; note < CONTROLBUS_NUM_CHROMA; ++note) {
            uint16_t noteIndex = static_cast<uint16_t>(12 * octave + note);
            if (noteIndex < SB_NUM_FREQS) {
                float val = raw.bins64Adaptive[noteIndex];
                m_sbNoteChroma[note] += val;
                if (m_sbNoteChroma[note] > 1.0f) m_sbNoteChroma[note] = 1.0f;
                if (m_sbNoteChroma[note] > m_sbChromaMaxVal) m_sbChromaMaxVal = m_sbNoteChroma[note];
            }
        }
    }
    if (m_sbChromaMaxVal < 0.0001f) m_sbChromaMaxVal = 0.0001f;
}

void AudioActor::processSbBloomSidecar(const ControlBusRawInput& raw)
{
    float moodNorm = static_cast<float>(m_controlBus.getMood()) / 255.0f;
    float smoothingRate = 25.0f + (5.0f * moodNorm);
    float alpha = 1.0f - std::exp(-smoothingRate * (HOP_DURATION_MS * 0.001f));

    for (uint8_t i = 0; i < SB_NUM_FREQS; ++i) {
        float target = raw.bins64Adaptive[i];
        m_sbSpectrogram[i] = m_sbSpectrogram[i] + (target - m_sbSpectrogram[i]) * alpha;
    }

    for (uint8_t i = 0; i < SB_NUM_FREQS; ++i) {
        float noteBrightness = m_sbSpectrogram[i];
        if (m_sbSpectrogramSmooth[i] < noteBrightness) {
            float distance = noteBrightness - m_sbSpectrogramSmooth[i];
            m_sbSpectrogramSmooth[i] += distance * 0.95f;
        } else if (m_sbSpectrogramSmooth[i] > noteBrightness) {
            float distance = m_sbSpectrogramSmooth[i] - noteBrightness;
            m_sbSpectrogramSmooth[i] -= distance * 0.95f;
        }
        if (m_sbSpectrogramSmooth[i] < 0.0f) m_sbSpectrogramSmooth[i] = 0.0f;
        if (m_sbSpectrogramSmooth[i] > 1.0f) m_sbSpectrogramSmooth[i] = 1.0f;
    }

    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        m_sbChromagramSmooth[i] = 0.0f;
    }
    const float chromaDiv = 64.0f / 12.0f;
    for (uint8_t i = 0; i < SB_NUM_FREQS; ++i) {
        float noteMagnitude = m_sbSpectrogramSmooth[i];
        if (noteMagnitude < 0.0f) noteMagnitude = 0.0f;
        if (noteMagnitude > 1.0f) noteMagnitude = 1.0f;
        uint8_t chromaBin = static_cast<uint8_t>(i % 12);
        m_sbChromagramSmooth[chromaBin] += noteMagnitude / chromaDiv;
    }

    m_sbChromagramMaxPeak *= 0.98f;
    if (m_sbChromagramMaxPeak < 0.01f) m_sbChromagramMaxPeak = 0.01f;
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        if (m_sbChromagramSmooth[i] > m_sbChromagramMaxPeak) {
            float distance = m_sbChromagramSmooth[i] - m_sbChromagramMaxPeak;
            m_sbChromagramMaxPeak += distance * 0.5f;
        }
    }
    float multiplier = 1.0f / m_sbChromagramMaxPeak;
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        m_sbChromagramSmooth[i] *= multiplier;
        if (m_sbChromagramSmooth[i] > 1.0f) m_sbChromagramSmooth[i] = 1.0f;
    }

    updateSbNoveltyAndHueShift();
}

void AudioActor::updateSbNoveltyAndHueShift()
{
    int16_t roundedIndex = static_cast<int16_t>(m_sbSpectralHistoryIndex) - 1;
    while (roundedIndex < 0) roundedIndex += SB_SPECTRAL_HISTORY;

    float noveltyNow = 0.0f;
    for (uint8_t i = 0; i < SB_NUM_FREQS; ++i) {
        float noveltyBin = m_sbSpectrogram[i] - m_sbSpectralHistory[roundedIndex][i];
        if (noveltyBin < 0.0f) noveltyBin = 0.0f;
        noveltyNow += noveltyBin;
    }
    noveltyNow /= SB_NUM_FREQS;

    for (uint8_t i = 0; i < SB_NUM_FREQS; ++i) {
        m_sbSpectralHistory[m_sbSpectralHistoryIndex][i] = m_sbSpectrogram[i];
    }
    m_sbNoveltyCurve[m_sbSpectralHistoryIndex] = std::sqrt(noveltyNow);

    m_sbSpectralHistoryIndex++;
    if (m_sbSpectralHistoryIndex >= SB_SPECTRAL_HISTORY) m_sbSpectralHistoryIndex = 0;

    int16_t noveltyIndex = static_cast<int16_t>(m_sbSpectralHistoryIndex) - 1;
    while (noveltyIndex < 0) noveltyIndex += SB_SPECTRAL_HISTORY;
    float noveltyVal = m_sbNoveltyCurve[noveltyIndex];

    noveltyVal -= 0.10f;
    if (noveltyVal < 0.0f) noveltyVal = 0.0f;
    noveltyVal *= 1.111111f;
    noveltyVal = noveltyVal * noveltyVal * noveltyVal;
    if (noveltyVal > 0.05f) noveltyVal = 0.05f;

    if (noveltyVal > m_sbHueShiftSpeed) {
        m_sbHueShiftSpeed = noveltyVal * 0.75f;
    } else {
        m_sbHueShiftSpeed *= 0.99f;
    }

    m_sbHuePosition += (m_sbHueShiftSpeed * m_sbHuePushDirection);
    while (m_sbHuePosition < 0.0f) m_sbHuePosition += 1.0f;
    while (m_sbHuePosition >= 1.0f) m_sbHuePosition -= 1.0f;

    if (std::fabs(m_sbHuePosition - m_sbHueDestination) <= 0.01f) {
        m_sbHuePushDirection *= -1.0f;
        m_sbHueShiftingMixTarget *= -1.0f;
        m_sbRand = m_sbRand * 1664525u + 1013904223u;
        m_sbHueDestination = static_cast<float>(m_sbRand) / 4294967295.0f;
    }

    float hueMixDistance = std::fabs(m_sbHueShiftingMix - m_sbHueShiftingMixTarget);
    if (m_sbHueShiftingMix < m_sbHueShiftingMixTarget) {
        m_sbHueShiftingMix += hueMixDistance * 0.01f;
    } else if (m_sbHueShiftingMix > m_sbHueShiftingMixTarget) {
        m_sbHueShiftingMix -= hueMixDistance * 0.01f;
    }
}

// ============================================================================
// Utility Methods
// ============================================================================

float AudioActor::computeRMS(const int16_t* samples, size_t count)
{
    if (count == 0) return 0.0f;
    int64_t sumSq = 0;
    for (size_t i = 0; i < count; ++i) {
        int32_t s = samples[i];
        sumSq += s * s;
    }
    float rms = std::sqrt(static_cast<float>(sumSq) / count);
    return std::min(1.0f, rms / 32768.0f);
}

void AudioActor::handleCaptureError(CaptureResult result)
{
    switch (result) {
        case CaptureResult::NOT_INITIALIZED:
            LW_LOGE("Capture error: not initialized");
            m_state = AudioActorState::ERROR;
            m_stats.state = m_state;
            break;
        case CaptureResult::DMA_TIMEOUT:
            LW_LOGW("Capture: DMA timeout");
            break;
        case CaptureResult::READ_ERROR:
            LW_LOGW("Capture: read error");
            break;
        case CaptureResult::BUFFER_OVERFLOW:
            LW_LOGW("Capture: buffer overflow");
            break;
        default:
            LW_LOGW("Capture: unknown error %d", static_cast<int>(result));
            break;
    }
}

// ============================================================================
// Benchmark Aggregation
// ============================================================================

#if FEATURE_AUDIO_BENCHMARK
void AudioActor::aggregateBenchmarkStats()
{
    AudioBenchmarkSample sample;
    while (m_benchmarkRing.pop(sample)) {
        m_benchmarkStats.updateFromSample(sample);
    }
    TRACE_COUNTER("cpu_load", static_cast<int32_t>(m_benchmarkStats.cpuLoadPercent * 100));
}
#endif

// ============================================================================
// Noise Calibration (SensoryBridge pattern)
// ============================================================================

bool AudioActor::startNoiseCalibration(uint32_t durationMs, float safetyMultiplier)
{
    if (m_noiseCalibration.state == CalibrationState::MEASURING ||
        m_noiseCalibration.state == CalibrationState::REQUESTED) {
        LW_LOGW("Calibration already in progress");
        return false;
    }

    m_noiseCalibration.reset();
    m_noiseCalibration.durationMs = durationMs;
    m_noiseCalibration.safetyMultiplier = safetyMultiplier;
    m_noiseCalibration.state = CalibrationState::REQUESTED;

    LW_LOGI("Noise calibration requested: %ums, multiplier=%.2f",
             durationMs, safetyMultiplier);
    return true;
}

void AudioActor::cancelNoiseCalibration()
{
    if (m_noiseCalibration.state != CalibrationState::IDLE) {
        LW_LOGI("Calibration cancelled");
        m_noiseCalibration.reset();
    }
}

bool AudioActor::applyCalibrationResults()
{
    if (!m_noiseCalibration.result.valid) {
        LW_LOGW("Cannot apply: no valid calibration results");
        return false;
    }

    AudioPipelineTuning tuning = getPipelineTuning();
    for (int i = 0; i < 8; ++i) {
        tuning.perBandNoiseFloors[i] = m_noiseCalibration.result.bandFloors[i];
    }
    tuning.usePerBandNoiseFloor = true;
    tuning.noiseFloorMin = m_noiseCalibration.result.overallRms * m_noiseCalibration.safetyMultiplier;
    setPipelineTuning(tuning);

    LW_LOGI("Applied calibration: noiseFloorMin=%.6f, perBand enabled",
             tuning.noiseFloorMin);
    return true;
}

void AudioActor::processNoiseCalibration(float rms, const float* bands, const float* chroma, uint32_t nowMs)
{
    switch (m_noiseCalibration.state) {
        case CalibrationState::IDLE:
        case CalibrationState::COMPLETE:
        case CalibrationState::FAILED:
            return;

        case CalibrationState::REQUESTED:
            m_noiseCalibration.startTimeMs = nowMs;
            m_noiseCalibration.state = CalibrationState::MEASURING;
            LW_LOGI("Calibration started: measuring for %ums", m_noiseCalibration.durationMs);
            [[fallthrough]];

        case CalibrationState::MEASURING: {
            uint32_t elapsed = nowMs - m_noiseCalibration.startTimeMs;
            if (elapsed >= m_noiseCalibration.durationMs) {
                if (m_noiseCalibration.sampleCount > 0) {
                    float invCount = 1.0f / static_cast<float>(m_noiseCalibration.sampleCount);
                    m_noiseCalibration.result.overallRms = m_noiseCalibration.rmsSum * invCount;
                    m_noiseCalibration.result.peakRms = m_noiseCalibration.peakRms;
                    m_noiseCalibration.result.sampleCount = m_noiseCalibration.sampleCount;
                    for (int i = 0; i < 8; ++i) {
                        float avg = m_noiseCalibration.bandSum[i] * invCount;
                        m_noiseCalibration.result.bandFloors[i] = avg * m_noiseCalibration.safetyMultiplier;
                    }
                    for (int i = 0; i < 12; ++i) {
                        float avg = m_noiseCalibration.chromaSum[i] * invCount;
                        m_noiseCalibration.result.chromaFloors[i] = avg * m_noiseCalibration.safetyMultiplier;
                    }
                    m_noiseCalibration.result.valid = true;
                    m_noiseCalibration.state = CalibrationState::COMPLETE;
                    LW_LOGI("Calibration complete: avgRMS=%.6f, peak=%.6f, samples=%u",
                             m_noiseCalibration.result.overallRms,
                             m_noiseCalibration.result.peakRms,
                             m_noiseCalibration.result.sampleCount);
                } else {
                    LW_LOGE("Calibration failed: no samples collected");
                    m_noiseCalibration.state = CalibrationState::FAILED;
                }
                return;
            }

            if (rms > m_noiseCalibration.maxAllowedRms) {
                LW_LOGW("Calibration aborted: RMS %.4f exceeds max %.4f (not silent)",
                         rms, m_noiseCalibration.maxAllowedRms);
                m_noiseCalibration.state = CalibrationState::FAILED;
                return;
            }

            m_noiseCalibration.rmsSum += rms;
            if (rms > m_noiseCalibration.peakRms) m_noiseCalibration.peakRms = rms;
            for (int i = 0; i < 8; ++i) m_noiseCalibration.bandSum[i] += bands[i];
            for (int i = 0; i < 12; ++i) m_noiseCalibration.chromaSum[i] += chroma[i];
            m_noiseCalibration.sampleCount++;

            if ((m_noiseCalibration.sampleCount % 62) == 0) {
                float progress = (float)elapsed / (float)m_noiseCalibration.durationMs * 100.0f;
                LW_LOGD("Calibrating: %.0f%% (%u samples, avgRMS=%.5f)",
                         progress, m_noiseCalibration.sampleCount,
                         m_noiseCalibration.rmsSum / m_noiseCalibration.sampleCount);
            }
            break;
        }
    }
}

#else  // Goertzel backend (default)

// ============================================================================
// Constructor / Destructor
// ============================================================================

AudioActor::AudioActor()
    : Actor(ActorConfigs::Audio())
    , m_state(AudioActorState::UNINITIALIZED)
{
    m_stats.reset();
    memset(m_hopBuffer, 0, sizeof(m_hopBuffer));
    m_pipelineTuning = clampAudioPipelineTuning(AudioPipelineTuning{});
    m_noiseFloor = m_pipelineTuning.noiseFloorMin;
}

AudioActor::~AudioActor()
{
    // Base class handles task cleanup
    // AudioCapture destructor handles I2S cleanup
}

// ============================================================================
// Control Methods
// ============================================================================

void AudioActor::pause()
{
    if (m_state == AudioActorState::RUNNING) {
        LW_LOGI("Pausing audio capture");
        m_state = AudioActorState::PAUSED;
    }
}

void AudioActor::resume()
{
    if (m_state == AudioActorState::PAUSED) {
        LW_LOGI("Resuming audio capture");
        m_state = AudioActorState::RUNNING;
    }
}

void AudioActor::resetStats()
{
    m_stats.reset();
    m_capture.resetStats();
}

// ============================================================================
// One-Shot Debug Output Methods
// ============================================================================

void AudioActor::printStatus()
{
#ifndef NATIVE_BUILD
    const CaptureStats& cstats = m_capture.getStats();
    const ControlBusFrame& frame = m_controlBus.GetFrameRef();
    float micLevelDb = (m_lastRmsPreGain > 0.0001f) ? (20.0f * log10f(m_lastRmsPreGain)) : -80.0f;

    Serial.println("=== Audio Status ===");
    Serial.printf("  Mic Level: %.1f dB\n", micLevelDb);
    Serial.printf("  RMS: %.4f -> %.3f (pre-gain: %.4f)\n", m_lastRmsRaw, frame.rms, m_lastRmsPreGain);
    Serial.printf("  AGC Gain: %.2f\n", m_lastAgcGain);
    Serial.printf("  DC Estimate: %.1f\n", m_lastDcEstimate);
    Serial.printf("  Noise Floor: %.5f\n", m_noiseFloor);
    Serial.printf("  Clips: %u\n", (unsigned)m_lastClipCount);
    Serial.printf("  Captures: %lu (failed: %lu)\n", cstats.hopsCapured, m_stats.captureFailCount);
    Serial.printf("  Peak: %d (centered: %d)\n", cstats.peakSample, m_lastPeakCentered);
    Serial.printf("  Onset: in=%.5f floor=%.5f act=%.3f gate[abs=%u act=%u prev=%u warm=%u] flux=%.3f env=%.3f evt=%.3f k/s/h=%u/%u/%u us=%u\n",
                  m_lastOnsetInputRms,
                  m_lastOnsetNoiseFloor,
                  m_lastOnsetActivity,
                  (m_lastOnsetGateFlags & ONSET_GATE_ABS_RMS) ? 1u : 0u,
                  (m_lastOnsetGateFlags & ONSET_GATE_ACTIVITY) ? 1u : 0u,
                  (m_lastOnsetGateFlags & ONSET_GATE_NO_PREV) ? 1u : 0u,
                  (m_lastOnsetGateFlags & ONSET_GATE_WARMUP) ? 1u : 0u,
                  frame.onsetFlux,
                  frame.onsetEnv,
                  frame.onsetEvent,
                  frame.kickTrigger ? 1u : 0u,
                  frame.snareTrigger ? 1u : 0u,
                  frame.hihatTrigger ? 1u : 0u,
                  frame.onsetProcessUs);

    // Spike stats
    auto spikeStats = m_controlBus.getSpikeStats();
    Serial.printf("  Spikes: detected=%lu corrected=%lu avg/frame=%.3f\n",
                  spikeStats.spikesDetectedBands + spikeStats.spikesDetectedChroma,
                  spikeStats.spikesCorrected,
                  spikeStats.avgSpikesPerFrame);

#if FEATURE_MUSICAL_SALIENCY
    // Saliency metrics
    Serial.printf("  Saliency: overall=%.3f dom=%u H=%.3f R=%.3f T=%.3f D=%.3f\n",
                  frame.saliency.overallSaliency,
                  frame.saliency.dominantType,
                  frame.saliency.harmonicNoveltySmooth,
                  frame.saliency.rhythmicNoveltySmooth,
                  frame.saliency.timbralNoveltySmooth,
                  frame.saliency.dynamicNoveltySmooth);
#endif

#if FEATURE_STYLE_DETECTION
    // Style detection
    const StyleClassification& styleClass = m_styleDetector.getClassification();
    Serial.printf("  Style: %u conf=%.2f [R=%.2f H=%.2f M=%.2f T=%.2f D=%.2f]\n",
                  static_cast<uint8_t>(m_styleDetector.getStyle()),
                  m_styleDetector.getConfidence(),
                  styleClass.styleWeights[0],
                  styleClass.styleWeights[1],
                  styleClass.styleWeights[2],
                  styleClass.styleWeights[3],
                  styleClass.styleWeights[4]);
#endif
#endif // NATIVE_BUILD
}

void AudioActor::printSpectrum()
{
#ifndef NATIVE_BUILD
    Serial.println("=== Audio Spectrum ===");

    // 8-band spectrum
    Serial.print("  8-band: [");
    for (int i = 0; i < 8; i++) {
        Serial.printf("%.3f%s", m_lastBands[i], (i < 7) ? " " : "");
    }
    Serial.println("]");

    // 64-bin folded to 8 bands
    Serial.print("  64-bin (folded): [");
    for (int i = 0; i < 8; i++) {
        Serial.printf("%.3f%s", m_bands64Folded[i], (i < 7) ? " " : "");
    }
    Serial.println("]");

    Serial.printf("  Spectral Flux: %.3f\n", m_lastFluxMapped);

    // Chroma (12 pitch classes)
    const ControlBusFrame& frame = m_controlBus.GetFrameRef();
    Serial.print("  Chroma: [");
    for (int i = 0; i < 12; i++) {
        Serial.printf("%.2f%s", frame.chroma[i], (i < 11) ? " " : "");
    }
    Serial.println("]");
#endif // NATIVE_BUILD
}

void AudioActor::printBeat()
{
#ifndef NATIVE_BUILD
    Serial.println("=== Beat Tracking ===");
    Serial.printf("  BPM: %.1f\n", m_lastTempoOutput.bpm);
    Serial.printf("  Confidence: %.2f\n", m_lastTempoOutput.confidence);
    Serial.printf("  Phase: %.2f\n", m_lastTempoOutput.phase01);
    Serial.printf("  Locked: %s\n", m_lastTempoOutput.locked ? "YES" : "no");
    Serial.printf("  Beat Tick: %s\n", m_lastTempoOutput.beat_tick ? "YES" : "no");
#endif // NATIVE_BUILD
}

void AudioActor::printDiagnostics()
{
    // Calculate rates
    uint64_t now_us = esp_timer_get_time();
    uint64_t elapsed_us = now_us - m_diag.diagStartTimeUs;
    float elapsed_s = elapsed_us / 1000000.0f;
    
    float captureRate = (elapsed_s > 0.1f) ? (m_diag.captureSuccesses / elapsed_s) : 0.0f;
    float publishRate = (elapsed_s > 0.1f) ? (m_diag.publishCount / elapsed_s) : 0.0f;
    float successPct = (m_diag.captureAttempts > 0) 
        ? (100.0f * m_diag.captureSuccesses / m_diag.captureAttempts) : 0.0f;
    
    // Expected rate for P4: 125 Hz (16kHz / 128 samples)
    const float expectedRate = HOP_RATE_HZ;
    bool rateOk = (captureRate >= expectedRate * 0.9f && captureRate <= expectedRate * 1.1f);
    
    LW_LOGI("========== AUDIO PIPELINE DIAGNOSTICS ==========");
    
    // Phase 1.1: Capture Rate
    LW_LOGI("CAPTURE: rate=%.1f Hz (expect %.1f) %s | success=%.1f%% | attempts=%lu ok=%lu",
            captureRate, expectedRate, rateOk ? "OK" : "PROBLEM",
            successPct, (unsigned long)m_diag.captureAttempts, (unsigned long)m_diag.captureSuccesses);
    
    if (m_diag.captureDmaTimeouts > 0 || m_diag.captureReadErrors > 0) {
        LW_LOGW("  ERRORS: DMA_timeouts=%lu read_errors=%lu",
                (unsigned long)m_diag.captureDmaTimeouts, (unsigned long)m_diag.captureReadErrors);
    }
    
    // Phase 1.2: Publish Rate
    LW_LOGI("PUBLISH: rate=%.1f Hz | count=%lu | seq_gaps=%lu",
            publishRate, (unsigned long)m_diag.publishCount, (unsigned long)m_diag.publishSeqGaps);
    
    // Phase 2.1: I2S/ES8311 Hardware Validation
    LW_LOGI("SAMPLES: raw=[%d..%d] rms=%.4f nonzero=%s zero_hops=%lu",
            m_diag.lastRawMin, m_diag.lastRawMax, m_diag.lastRawRms,
            m_diag.samplesNonZero ? "YES" : "NO",
            (unsigned long)m_diag.zeroHopCount);
    
    if (!m_diag.samplesNonZero || m_diag.zeroHopCount > 10) {
        LW_LOGW("  WARNING: I2S may not be receiving audio data!");
    }
    
    // Phase 2.3: Timing
    LW_LOGI("TIMING: capture avg=%lu max=%lu us | process avg=%lu max=%lu us",
            (unsigned long)m_diag.avgCaptureLatencyUs, (unsigned long)m_diag.maxCaptureLatencyUs,
            (unsigned long)m_diag.avgProcessLatencyUs, (unsigned long)m_diag.maxProcessLatencyUs);
    
    // Frame freshness check
    if (m_diag.lastPublishTimeUs > 0) {
        uint32_t frameAge_ms = (now_us - m_diag.lastPublishTimeUs) / 1000;
        LW_LOGI("FRESHNESS: last_publish=%lu ms ago | hop_seq=%lu",
                (unsigned long)frameAge_ms, (unsigned long)m_diag.lastPublishSeq);
    }
    
    // Quick health summary
    bool healthy = rateOk && m_diag.samplesNonZero && 
                   m_diag.captureDmaTimeouts == 0 && m_diag.publishSeqGaps == 0;
    LW_LOGI("HEALTH: %s", healthy ? "OK - Pipeline functioning normally" : "ISSUES DETECTED - See warnings above");
    LW_LOGI("=================================================");
}

AudioPipelineTuning AudioActor::getPipelineTuning() const
{
    AudioPipelineTuning out;
    uint32_t v0;
    uint32_t v1 = 0;
    do {
        v0 = m_pipelineTuningSeq.load(std::memory_order_acquire);
        if (v0 & 1U) continue;
        out = m_pipelineTuning;
        v1 = m_pipelineTuningSeq.load(std::memory_order_acquire);
    } while (v0 != v1 || (v1 & 1U));
    return out;
}

void AudioActor::setPipelineTuning(const AudioPipelineTuning& tuning)
{
    AudioPipelineTuning clamped = clampAudioPipelineTuning(tuning);
    uint32_t v = m_pipelineTuningSeq.load(std::memory_order_relaxed);
    m_pipelineTuningSeq.store(v + 1U, std::memory_order_release);
    m_pipelineTuning = clamped;
    m_pipelineTuningSeq.store(v + 2U, std::memory_order_release);
}

void AudioActor::resetDspState()
{
    m_dspResetPending.store(true, std::memory_order_release);
}

AudioDspState AudioActor::getDspState() const
{
    AudioDspState out;
    uint32_t v0;
    uint32_t v1 = 0;
    do {
        v0 = m_dspStateSeq.load(std::memory_order_acquire);
        if (v0 & 1U) continue;
        out = m_dspState;
        v1 = m_dspStateSeq.load(std::memory_order_acquire);
    } while (v0 != v1 || (v1 & 1U));
    return out;
}

// ============================================================================
// Buffer Access
// ============================================================================

const int16_t* AudioActor::getLastHop() const
{
    if (m_state == AudioActorState::RUNNING || m_state == AudioActorState::PAUSED) {
        return m_hopBuffer;
    }
    return nullptr;
}

bool AudioActor::hasNewHop()
{
    if (m_newHopAvailable) {
        m_newHopAvailable = false;
        return true;
    }
    return false;
}

// ============================================================================
// Actor Lifecycle
// ============================================================================

void AudioActor::onStart()
{
    LW_LOGI("AudioActor starting on Core %d", xPortGetCoreID());

    m_state = AudioActorState::INITIALIZING;
    m_stats.state = m_state;

    // Initialize diagnostics
    m_diag.reset();
    m_diag.diagStartTimeUs = esp_timer_get_time();
#if FEATURE_TRANSLATION_ENGINE
    translation_init(&m_translationState);
    m_translationLastAudioTime = AudioTime{};
    m_translationHaveLastAudioTime = false;
    m_translationLastDebugLogUs = 0ULL;
#if FEATURE_TRANSLATION_DEBUG
    m_translationLastLatchValid = false;
    m_translationLastLatchBpm = 0.0f;
    m_translationLastLatchConfidence = 0.0f;
    m_translationLastLatchUs = 0ULL;
    m_translationLastLatchTempoLocked = false;
    m_translationLastLatchBeatTick = false;
    m_translationLastLatchDownbeatTick = false;
#endif
#endif
    m_standardBeatInBar = 0u;

    // Initialize I2S audio capture
    if (!m_capture.init()) {
        LW_LOGE("Failed to initialize audio capture");
        m_state = AudioActorState::ERROR;
        m_stats.state = m_state;
        return;
    }

    m_state = AudioActorState::RUNNING;
    m_stats.state = m_state;

    // Initialize TempoTracker beat tracker
    m_tempo.init();
    // Initialize last output state
    m_lastTempoOutput = m_tempo.getOutput();
    LW_LOGI("TempoTracker initialized");

    LW_LOGI("AudioActor started (tick=%dms, hop=%d, rate=%.1fHz)",
             AUDIO_ACTOR_TICK_MS, HOP_SIZE, HOP_RATE_HZ);
    LW_LOGI("Pipeline diagnostics enabled - will log every 10 seconds");
}

void AudioActor::onMessage(const actors::Message& msg)
{
    switch (msg.type) {
        case actors::MessageType::SHUTDOWN:
            LW_LOGI("Received SHUTDOWN message");
            // Will be handled by base class
            break;

        case actors::MessageType::HEALTH_CHECK:
            LW_LOGD("Health check: state=%d, captures=%lu",
                     static_cast<int>(m_state), m_stats.captureSuccessCount);
            // TODO: Send HEALTH_STATUS response once MessageBus integration lands
            break;

        case actors::MessageType::PING:
            // Respond with PONG for latency testing
            // TODO: Send PONG once MessageBus integration lands
            LW_LOGD("PING received");
            break;

        default:
            // Ignore unknown messages
            LW_LOGD("Ignoring message type 0x%02X",
                     static_cast<uint8_t>(msg.type));
            break;
    }
}

void AudioActor::onTick()
{
    // Skip if not in running state
    if (m_state != AudioActorState::RUNNING) {
        return;
    }

    m_stats.tickCount++;

    // Record tick start time
    uint64_t tickStart = esp_timer_get_time();

    // =========================================================================
    // SELF-CLOCKED AUDIO CAPTURE (tickInterval=0 mode)
    // =========================================================================
    // In self-clocked mode, the I2S DMA is the timing source. Each onTick():
    //   1. Blocks on i2s_channel_read() until a hop is ready (~8ms between hops)
    //   2. Processes exactly ONE hop
    //   3. Returns immediately, allowing Actor::run() to call onTick() again
    //
    // This achieves 125 Hz (= 16000 Hz / 128 samples) naturally, bypassing
    // the 100 Hz FreeRTOS tick limitation entirely.
    // =========================================================================
    
    // Single blocking capture - the I2S read IS the clock
    captureHop();

    // Record tick time
    m_stats.lastTickTimeUs = esp_timer_get_time() - tickStart;

    // ========================================================================
    // Periodic Debug Logging (Refactored: opt-in via verbosity level 5)
    // ========================================================================
    // REMOVED: The old verbose periodic logging (5 lines every 10 seconds).
    // Status/spectrum/beat info is now available via one-shot commands:
    //   - adbg status   -> printStatus()
    //   - adbg spectrum -> printSpectrum()
    //   - adbg beat     -> printBeat()
    //
    // Periodic logging is now condensed to a single line at level 5 (TRACE).
    // Use 'adbg 5' to enable, or keep 'adbg 0-4' for silence.
    // ========================================================================
    auto& dbgCfg = getAudioDebugConfig();

    // Level 5 (TRACE): Single-line condensed status every ~10 seconds
    // This replaces the old 5-line verbose output at level 2
    if (dbgCfg.verbosity >= 5 && (m_stats.tickCount % 620) == 0) {
        float micLevelDb = (m_lastRmsPreGain > 0.0001f) ? (20.0f * log10f(m_lastRmsPreGain)) : -80.0f;
        LW_LOGD("Audio: mic=%.1fdB rms=%.3f agc=%.2f bpm=%.1f lock=%s",
                micLevelDb, m_controlBus.GetFrameRef().rms, m_lastAgcGain,
                m_lastTempoOutput.bpm, m_lastTempoOutput.locked ? "Y" : "n");
    }

    // Level 2+ (WARNING): Log warnings when they actually happen (not periodic)
    // High spike rate warning - threshold raised from 5.0 to 10.0 because:
    // - 20 bins checked per frame (8 bands + 12 chroma)
    // - At low signal levels, noise fluctuations trigger direction-change detection
    // - 10 spikes/frame is more realistic threshold for actual problems
    auto spikeStats = m_controlBus.getSpikeStats();
    static uint32_t lastSpikeWarningTick = 0;
    if (dbgCfg.verbosity >= 2 && spikeStats.avgSpikesPerFrame > 10.0f &&
        (m_stats.tickCount - lastSpikeWarningTick) > 620) {
        LW_LOGW("High spike rate: avg=%.1f/frame", spikeStats.avgSpikesPerFrame);
        lastSpikeWarningTick = m_stats.tickCount;
    }

    // ========================================================================
    // Phase 1 Pipeline Diagnostics (every 10 seconds, ~1250 ticks @ 125 Hz)
    // Always enabled - critical for debugging audio availability issues
    // ========================================================================
    if ((m_stats.tickCount % 1250) == 0 && m_stats.tickCount > 0) {
        printDiagnostics();
    }
}

void AudioActor::onStop()
{
    LW_LOGI("AudioActor stopping");

    // Deinitialize audio capture
    m_capture.deinit();

    m_state = AudioActorState::UNINITIALIZED;
    m_stats.state = m_state;

    // Log final statistics
    LW_LOGI("Final stats:");
    LW_LOGI("  Total ticks: %lu", m_stats.tickCount);
    LW_LOGI("  Successful captures: %lu", m_stats.captureSuccessCount);
    LW_LOGI("  Failed captures: %lu", m_stats.captureFailCount);

    const CaptureStats& cstats = m_capture.getStats();
    LW_LOGI("  DMA timeouts: %lu", cstats.dmaTimeouts);
    LW_LOGI("  Read errors: %lu", cstats.readErrors);
    LW_LOGI("  Max read time: %lu us", cstats.maxReadTimeUs);
}

// ============================================================================
// Internal Methods
// ============================================================================

void AudioActor::captureHop()
{
    // Phase 1.1: Track capture timing
    uint64_t captureStart = esp_timer_get_time();
    m_diag.captureAttempts++;
    m_diag.lastCaptureStartUs = captureStart;

    TRACE_BEGIN("i2s_dma_read");
    CaptureResult result = m_capture.captureHop(m_hopBuffer);
    TRACE_END();

    uint64_t captureEnd = esp_timer_get_time();
    m_diag.lastCaptureEndUs = captureEnd;
    uint32_t captureLatency = static_cast<uint32_t>(captureEnd - captureStart);

    // Update capture latency stats
    if (captureLatency > m_diag.maxCaptureLatencyUs) {
        m_diag.maxCaptureLatencyUs = captureLatency;
    }
    // Exponential moving average (alpha = 0.125)
    m_diag.avgCaptureLatencyUs = (m_diag.avgCaptureLatencyUs * 7 + captureLatency) / 8;

    if (result == CaptureResult::SUCCESS) {
        m_stats.captureSuccessCount++;
        m_diag.captureSuccesses++;
        m_newHopAvailable = true;

        // Phase 2.1: Analyze raw samples before DC blocking
        int16_t rawMin = 32767, rawMax = -32768;
        int64_t rawSumSq = 0;
        bool allSame = true;
        int16_t firstSample = m_hopBuffer[0];
        for (size_t i = 0; i < HOP_SIZE; ++i) {
            int16_t s = m_hopBuffer[i];
            if (s < rawMin) rawMin = s;
            if (s > rawMax) rawMax = s;
            rawSumSq += static_cast<int64_t>(s) * s;
            if (s != firstSample) allSame = false;
        }
        m_diag.lastRawMin = rawMin;
        m_diag.lastRawMax = rawMax;
        m_diag.lastRawRms = std::sqrt(static_cast<float>(rawSumSq) / HOP_SIZE) / 32768.0f;
        m_diag.samplesNonZero = !allSame && (rawMax != rawMin);
        if (allSame || (rawMin == 0 && rawMax == 0)) {
            m_diag.zeroHopCount++;
        }

        // Phase 2: Process the hop through DSP pipeline
        uint64_t processStart = esp_timer_get_time();
        processHop();
        uint64_t processEnd = esp_timer_get_time();
        m_diag.lastProcessEndUs = processEnd;
        
        uint32_t processLatency = static_cast<uint32_t>(processEnd - processStart);
        if (processLatency > m_diag.maxProcessLatencyUs) {
            m_diag.maxProcessLatencyUs = processLatency;
        }
        m_diag.avgProcessLatencyUs = (m_diag.avgProcessLatencyUs * 7 + processLatency) / 8;

    } else {
        m_stats.captureFailCount++;
        // Phase 1.1: Track failure types
        if (result == CaptureResult::DMA_TIMEOUT) {
            m_diag.captureDmaTimeouts++;
        } else {
            m_diag.captureReadErrors++;
        }
        handleCaptureError(result);
    }
}

// ============================================================================
// Phase 2: DSP Processing
// ============================================================================

void AudioActor::processHop()
{
    // MabuTrace: Wrap entire pipeline for Perfetto timeline visualization
    TRACE_SCOPE("audio_pipeline");

    // Phase 2B: Benchmark instrumentation - zero overhead when disabled
    BENCH_DECL_TIMING();
    BENCH_START_FRAME();

    auto clamp01 = [](float x) -> float {
        if (x < 0.0f) return 0.0f;
        if (x > 1.0f) return 1.0f;
        return x;
    };

    auto mapLevelDb = [&](float x, float dbFloor, float dbCeil) -> float {
        const float eps = 1e-6f;
        if (dbCeil <= dbFloor + 1e-3f) {
            return 0.0f;
        }
        float db = 20.0f * log10f(x + eps);
        float t = (db - dbFloor) / (dbCeil - dbFloor);
        return clamp01(t);
    };

    const AudioPipelineTuning tuning = getPipelineTuning();

    if (m_dspResetPending.exchange(false, std::memory_order_acq_rel)) {
        m_dcEstimate = 0.0f;
        m_agcGain = 1.0f;
        m_noiseFloor = tuning.noiseFloorMin;
        m_prevRMS = 0.0f;
        // Priority 5: Reset per-band history for spectral flux
        for (int i = 0; i < 8; ++i) {
            m_prevBands[i] = 0.0f;
        }
        m_analyzer.reset();
        m_chromaAnalyzer.reset();
#if FEATURE_STYLE_DETECTION
        m_styleDetector.reset();
#endif
        m_prevChordRoot = 0;
#ifndef NATIVE_BUILD
        portENTER_CRITICAL(&m_controlBusApiMux);
#endif
        m_controlBus.Reset();
#ifndef NATIVE_BUILD
        portEXIT_CRITICAL(&m_controlBusApiMux);
#endif
        // TempoTracker reset
        m_tempo.init();
        m_lastTempoOutput = m_tempo.getOutput();
        m_bins64AdaptiveMax = 0.0001f;
#if FEATURE_TRANSLATION_ENGINE
        translation_init(&m_translationState);
        m_translationLastAudioTime = AudioTime{};
        m_translationHaveLastAudioTime = false;
        m_translationLastDebugLogUs = 0ULL;
#if FEATURE_TRANSLATION_DEBUG
        m_translationLastLatchValid = false;
        m_translationLastLatchBpm = 0.0f;
        m_translationLastLatchConfidence = 0.0f;
        m_translationLastLatchUs = 0ULL;
        m_translationLastLatchTempoLocked = false;
        m_translationLastLatchBeatTick = false;
        m_translationLastLatchDownbeatTick = false;
#endif
#endif
        m_standardBeatInBar = 0u;
    }

    // 1. Build AudioTime for this hop
    // CLOCK SPINE FIX: Use END-OF-HOP semantics
    // - t.sample_index = sample immediately AFTER this hop (monotonic)
    // - t.timestamp_us = capture end time for this hop (not processing start)
    // This ensures renderer extrapolation uses the correct audio timeline.
    uint64_t now_us = (m_diag.lastCaptureEndUs != 0)
        ? m_diag.lastCaptureEndUs
        : esp_timer_get_time();
    uint64_t hop_end_sample_index = m_sampleIndex + HOP_SIZE;
    AudioTime now(hop_end_sample_index, SAMPLE_RATE, now_us);

    // Update monotonic counters (sample index now represents END of processed audio)
    m_sampleIndex = hop_end_sample_index;
    m_hopCount++;

    // DEBUG: Log clock spine values every 2 seconds (LWLS backend) - DISABLED
    // static uint64_t lastClockSpineLogLwls = 0;
    // uint64_t logNow = esp_timer_get_time();
    // if (logNow - lastClockSpineLogLwls >= 2000000) {  // 2 seconds
    //     lastClockSpineLogLwls = logNow;
    //     LW_LOGI("[CLOCK_SPINE:LWLS] now.t: sample_idx=%llu mono_us=%llu captureEnd=%llu hop=%lu",
    //             (unsigned long long)now.sample_index,
    //             (unsigned long long)now.monotonic_us,
    //             (unsigned long long)m_diag.lastCaptureEndUs,
    //             (unsigned long)m_hopCount);
    // }

    int32_t minRaw = 32767;
    int32_t maxRaw = -32768;
    int64_t sumRaw = 0;
    for (size_t i = 0; i < HOP_SIZE; ++i) {
        int32_t s = m_hopBuffer[i];
        if (s < minRaw) minRaw = s;
        if (s > maxRaw) maxRaw = s;
        sumRaw += s;
    }
    float meanRaw = (float)sumRaw / (float)HOP_SIZE;
    m_lastMeanSample = meanRaw;

    const float dcAlpha = tuning.dcAlpha;
    const float agcTargetRms = tuning.agcTargetRms;
    const float agcMinGain = tuning.agcMinGain;    // Don't attenuate below min
    const float agcMaxGain = tuning.agcMaxGain;
    const float agcAttack = tuning.agcAttack;
    const float agcRelease = tuning.agcRelease;

    const float noiseFloorMin = tuning.noiseFloorMin;
    const float noiseFloorRise = tuning.noiseFloorRise;
    const float noiseFloorFall = tuning.noiseFloorFall;
    const float gateStartFactor = tuning.gateStartFactor;
    const float gateRangeFactor = tuning.gateRangeFactor;
    const float gateRangeMin = tuning.gateRangeMin;

    // === Phase: DC/AGC Loop ===
    BENCH_START_PHASE();
    TRACE_BEGIN("dc_agc_loop");

    int32_t minC = 32767;
    int32_t maxC = -32768;
    int32_t peakC = 0;
    uint16_t clipCount = 0;

    int64_t sumSqPre = 0;
    for (size_t i = 0; i < HOP_SIZE; ++i) {
        float x = (float)m_hopBuffer[i];
        m_dcEstimate += dcAlpha * (x - m_dcEstimate);
        float dcRemoved = x - m_dcEstimate;

        int32_t preI = (int32_t)lroundf(dcRemoved);
        if (preI < -32768) preI = -32768;
        if (preI > 32767) preI = 32767;
        sumSqPre += (int64_t)preI * (int64_t)preI;

        float g = m_agcGain;
        int32_t gI = (int32_t)lroundf(dcRemoved * g);
        int32_t c = gI;
        if (c < -32768) c = -32768;
        if (c > 32767) c = 32767;
        if (c != gI) clipCount++;

        m_hopBufferCentered[i] = (int16_t)c;
        if (c < minC) minC = c;
        if (c > maxC) maxC = c;
        int32_t a = (c < 0) ? -c : c;
        if (a > peakC) peakC = a;
    }
    m_lastMinSample = (int16_t)minC;
    m_lastMaxSample = (int16_t)maxC;
    m_lastPeakCentered = (int16_t)peakC;
    m_lastDcEstimate = m_dcEstimate;
    m_lastClipCount = clipCount;

    float rmsPre = 0.0f;
    if (HOP_SIZE > 0) {
        float rmsPreAbs = std::sqrt(static_cast<float>(sumSqPre) / (float)HOP_SIZE);
        rmsPre = std::min(1.0f, rmsPreAbs / 32768.0f);
    }
    m_lastRmsPreGain = rmsPre;

    if (m_noiseFloor < noiseFloorMin) {
        m_noiseFloor = noiseFloorMin;
    }
    if (rmsPre < m_noiseFloor) {
        m_noiseFloor += noiseFloorFall * (rmsPre - m_noiseFloor);
    } else {
        // SNR-based rise guard: freeze floor when signal clearly present
        const float snr = rmsPre / std::max(m_noiseFloor, 0.0001f);
        const bool measuringAmbient =
            (rmsPre <= tuning.silenceThreshold) ||
            (snr < 3.0f);  // Increased from 2.0: more aggressive freeze to prevent drift

        if (measuringAmbient) {
            // Use configured noiseFloorRise instead of hardcoded value
            m_noiseFloor += noiseFloorRise * (rmsPre - m_noiseFloor);
        }
        // SNR >= 3.0: signal clearly present, freeze floor (no rise)
    }
    if (m_noiseFloor < noiseFloorMin) {
        m_noiseFloor = noiseFloorMin;
    }

    float gateStart = m_noiseFloor * gateStartFactor;
    float gateRange = std::max(gateRangeMin, m_noiseFloor * gateRangeFactor);
    float activity = clamp01((rmsPre - gateStart) / gateRange);

    // Recovery: If gate is closed but signal is clearly present, force noise floor down
    // This prevents the floor from getting stuck above valid audio signals
    const float MIN_SIGNAL_THRESHOLD = noiseFloorMin * 3.0f;  // Signal must be at least 3x minimum floor
    if (activity < 0.01f && rmsPre > MIN_SIGNAL_THRESHOLD && rmsPre > m_noiseFloor) {
        // Signal is present but gate is closed - noise floor is too high
        // Force it down aggressively (10x faster than normal fall)
        m_noiseFloor += (noiseFloorFall * 10.0f) * (rmsPre * 0.8f - m_noiseFloor);
        // Recalculate gate with corrected floor
        gateStart = m_noiseFloor * gateStartFactor;
        gateRange = std::max(gateRangeMin, m_noiseFloor * gateRangeFactor);
        activity = clamp01((rmsPre - gateStart) / gateRange);
    }

    if (clipCount > 0) {
        m_agcGain *= tuning.agcClipReduce;
    } else if (rmsPre <= gateStart) {
        m_agcGain += tuning.agcIdleReturnRate * (1.0f - m_agcGain);
    } else {
        float desired = agcTargetRms / (rmsPre + 1e-6f);
        if (desired < agcMinGain) desired = agcMinGain;
        if (desired > agcMaxGain) desired = agcMaxGain;
        float rate = (desired > m_agcGain) ? agcAttack : agcRelease;
        m_agcGain += rate * (desired - m_agcGain);
    }
    if (m_agcGain < agcMinGain) m_agcGain = agcMinGain;
    if (m_agcGain > agcMaxGain) m_agcGain = agcMaxGain;
    m_lastAgcGain = m_agcGain;

    TRACE_END();  // dc_agc_loop
    BENCH_END_PHASE(dcAgcLoopUs);

    // === Phase: RMS Compute ===
    BENCH_START_PHASE();
    TRACE_BEGIN("rms_flux");

    float rmsRaw = computeRMS(m_hopBufferCentered, HOP_SIZE);
    const float rmsMappedUngated = mapLevelDb(rmsRaw, tuning.rmsDbFloor, tuning.rmsDbCeil);
    float rmsMapped = rmsMappedUngated * activity;
    m_lastRmsRaw = rmsRaw;
    m_lastRmsMapped = rmsMapped;

    // Flux placeholder - will be computed after Goertzel if useSpectralFlux is enabled
    float fluxMapped = 0.0f;
    if (!tuning.noveltyUseSpectralFlux) {
        // Legacy RMS-based flux (computed here since it only needs RMS)
        float spectralFlux = std::max(0.0f, rmsMapped - m_prevRMS);
        m_prevRMS = rmsMapped;
        fluxMapped = std::min(1.0f, spectralFlux * tuning.fluxScale);
        m_lastFluxMapped = fluxMapped;
    }

    TRACE_END();  // rms_flux
    TRACE_COUNTER("audio_rms", static_cast<int32_t>(rmsRaw * 10000));
    TRACE_COUNTER("audio_pre_gain_rms", static_cast<int32_t>(m_lastRmsPreGain * 10000));
    BENCH_END_PHASE(rmsComputeUs);

    {
        AudioDspState state;
        state.rmsRaw = m_lastRmsRaw;
        state.rmsMapped = m_lastRmsMapped;
        state.rmsPreGain = m_lastRmsPreGain;
        state.fluxMapped = m_lastFluxMapped;
        state.agcGain = m_lastAgcGain;
        state.dcEstimate = m_lastDcEstimate;
        state.noiseFloor = m_noiseFloor;
        state.minSample = m_lastMinSample;
        state.maxSample = m_lastMaxSample;
        state.peakCentered = m_lastPeakCentered;
        state.meanSample = m_lastMeanSample;
        state.clipCount = m_lastClipCount;

        uint32_t v = m_dspStateSeq.load(std::memory_order_relaxed);
        m_dspStateSeq.store(v + 1U, std::memory_order_release);
        m_dspState = state;
        m_dspStateSeq.store(v + 2U, std::memory_order_release);
    }

    // [DIAG A2] DSP pipeline health — rmsPreGain, agcGain, activity, noiseFloor
    {
        auto& dbgCfg = audio::getAudioDebugConfig();
        static uint32_t s_a2Hop = 0;
        s_a2Hop++;
        if (dbgCfg.verbosity >= 5 && (s_a2Hop % dbgCfg.intervalDMA()) == 0) {
            LW_LOGD(LW_CLR_CYAN "[DIAG-A2]" LW_ANSI_RESET
                    " rmsPreGain=%.4f agcGain=%.2f activity=%.3f noiseFloor=%.5f rmsRaw=%.4f peak=%d",
                    m_lastRmsPreGain, m_lastAgcGain, activity, m_noiseFloor, rmsRaw, (int)m_lastPeakCentered);
        }
    }

    // 4. Analysis window preparation (Overlap-Add)
    // Build 512-sample window from previous + current hop for per-hop analysis
    int16_t window512[GoertzelAnalyzer::WINDOW_SIZE];
    bool oaReady = false;
    // Always accumulate samples for 64-bin Goertzel (needs 1500 samples)
    m_analyzer.accumulate(m_hopBufferCentered, HOP_SIZE);
    m_chromaAnalyzer.accumulate(m_hopBufferCentered, HOP_SIZE);

#if FEATURE_AUDIO_OA
    if (m_prevHopValid) {
        std::memcpy(window512, m_prevHopCentered, HOP_SIZE * sizeof(int16_t));
        std::memcpy(window512 + HOP_SIZE, m_hopBufferCentered, HOP_SIZE * sizeof(int16_t));
        oaReady = true;
    }
#endif

    // STACK MONITORING: Check stack high water mark before large allocations
#ifndef NATIVE_BUILD
    UBaseType_t stackHighWater = uxTaskGetStackHighWaterMark(nullptr);
    if (stackHighWater < 512) {  // Less than 2KB remaining (512 words * 4 bytes)
        LW_LOGW("AudioActor stack low! High water mark: %u words (%.1f KB remaining)",
                stackHighWater, stackHighWater * 4.0f / 1024.0f);
    }
#endif

    // 5. Build ControlBusRawInput
    ControlBusRawInput raw;
    raw.rmsUngated = rmsMappedUngated;
    raw.rms = rmsMapped;
    raw.flux = fluxMapped;

    // 5.5. Downsample waveform: 256 samples -> 128 points (2 samples per point)
    // Use peak (abs max) of each pair to preserve transients (matches Sensory Bridge style)
    constexpr uint8_t WAVEFORM_POINTS = audio::CONTROLBUS_WAVEFORM_N;
    constexpr uint8_t SAMPLES_PER_POINT = HOP_SIZE / WAVEFORM_POINTS;  // 256 / 128 = 2
    for (uint8_t i = 0; i < WAVEFORM_POINTS; ++i) {
        int16_t peak = 0;
        int16_t peakSample = 0;
        uint16_t startIdx = i * SAMPLES_PER_POINT;
        for (uint8_t j = 0; j < SAMPLES_PER_POINT && (startIdx + j) < HOP_SIZE; ++j) {
            int16_t sample = m_hopBufferCentered[startIdx + j];
            int16_t absSample = (sample < 0) ? -sample : sample;
            if (absSample > peak) {
                peak = absSample;
                peakSample = sample;  // Preserve sign
            }
        }
        if (activity < 1.0f) {
            raw.waveform[i] = static_cast<int16_t>(lroundf(peakSample * activity));
        } else {
            raw.waveform[i] = peakSample;
        }
    }

    // === Phase: Goertzel Analysis ===
    BENCH_START_PHASE();
    TRACE_BEGIN("goertzel_analyze");
    bool goertzelTriggered = false;

    // 6. Get band energies
    float bandsRaw[NUM_BANDS] = {0};
#if FEATURE_AUDIO_OA
    if (oaReady ? m_analyzer.analyzeWindow(window512, GoertzelAnalyzer::WINDOW_SIZE, bandsRaw)
                : m_analyzer.analyze(bandsRaw)) {
        goertzelTriggered = true;
#else
    if (m_analyzer.analyze(bandsRaw)) {
        goertzelTriggered = true;
#endif
        // Fresh band data available - Goertzel completed a 512-sample window
        for (int i = 0; i < NUM_BANDS; ++i) {
            float band = mapLevelDb(bandsRaw[i], tuning.bandDbFloor, tuning.bandDbCeil);

            // Phase 2: Per-band gain normalization (boost highs, attenuate bass for LGP balance)
            band *= tuning.perBandGains[i];
            if (band > 1.0f) band = 1.0f;

            // Phase 2: Per-band noise floor gate (calibrated for ambient noise sources)
            if (tuning.usePerBandNoiseFloor && band < tuning.perBandNoiseFloors[i]) {
                band = 0.0f;
            }

            m_lastBands[i] = band;
            raw.bands[i] = band * activity;
        }

        // Throttle 8-band Goertzel debug logging - gated by verbosity >= 5 (TRACE)
        // NOTE: This is now TRACE level - only shows with 'adbg 5'
        // For one-shot spectrum info, use 'adbg spectrum' command
        auto& dbgCfg8 = getAudioDebugConfig();
        if (dbgCfg8.verbosity >= 5 && ++m_goertzelLogCounter >= dbgCfg8.interval8Band()) {
            m_goertzelLogCounter = 0;
            // Condensed format: just the mapped band values (most useful for tuning)
            LW_LOGD(LW_CLR_CYAN "8-band:" LW_ANSI_RESET " [%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f]",
                     raw.bands[0], raw.bands[1], raw.bands[2], raw.bands[3],
                     raw.bands[4], raw.bands[5], raw.bands[6], raw.bands[7]);
        }

        // Persisted bands updated above (unscaled)
    } else {
        // No new analysis this hop - reuse last known bands
        // This prevents "picket fence" dropouts where bands would be 0 every other hop
        for (int i = 0; i < NUM_BANDS; ++i) {
            raw.bands[i] = m_lastBands[i] * activity;
        }
    }

    TRACE_END();
    BENCH_END_PHASE(goertzelUs);
    BENCH_SET_FLAG(goertzelTriggered, goertzelTriggered ? 1 : 0);

    // Perceptually-Weighted Spectral Flux
    // Bass bands weighted higher to improve kick detection and reduce
    // false triggers from hi-hats and treble transients.
    float unclippedFlux = 0.0f;
    if (tuning.noveltyUseSpectralFlux) {
        float spectralFlux = 0.0f;
        for (int i = 0; i < NUM_BANDS; ++i) {
            float delta = raw.bands[i] - m_prevBands[i];
            // Perceptual weighting: bass (bands 0-1) weighted highest,
            // treble (bands 6-7) weighted lowest
            float weight = PERCEPTUAL_BAND_WEIGHTS[i];
            // Half-wave rectification: only positive changes (onsets) contribute
            // Negative deltas (decay) suppressed at 0.6x to handle AGC oscillation
            float weightedDelta = (delta > 0.0f) ? (delta * weight) : (-delta * 0.6f * weight);
            spectralFlux += weightedDelta;
            m_prevBands[i] = raw.bands[i];
        }
        // Normalize by weight sum for consistent scaling across all band configurations
        spectralFlux /= PERCEPTUAL_BAND_WEIGHT_SUM;
        spectralFlux *= tuning.noveltySpectralFluxScale;
        unclippedFlux = spectralFlux * tuning.fluxScale;
        fluxMapped = std::min(1.0f, unclippedFlux);  // Hard clamp for UI/effects
        m_lastFluxMapped = fluxMapped;
        raw.flux = fluxMapped;  // Update raw.flux with clamped value
    } else {
        unclippedFlux = fluxMapped;
    }

    // ========================================================================
    // TempoTracker Beat Tracker Processing
    // ========================================================================
    TRACE_BEGIN("tempo_update");
    // EMOTISCOPE PARITY: Use full 64-bin spectrum for novelty detection
    // - Spectral flux from 64-bin Goertzel when ready (~10 Hz @ 12.8kHz)
    // - VU derivative from RMS every hop (50 Hz @ 12.8kHz)
    // 64-bin analysis fires when analyze64() completes (every 1500 samples)
    m_tempo.updateNovelty(
        m_analyze64Ready ? m_bins64Cached : nullptr,  // 64-bin magnitudes or nullptr
        audio::NUM_FREQS,                              // num_bins = 64 (Emotiscope parity)
        rmsRaw,                                        // RMS for VU calculation
        m_analyze64Ready                               // bins_ready flag
    );
    m_analyze64Ready = false;  // Reset flag after use

    // Update tempo detection (interleaved Goertzel computation)
    // CRITICAL: delta_sec must match actual hop duration (HOP_SIZE / SAMPLE_RATE)
    // At 12800 Hz with HOP_SIZE=256: 256/12800 = 0.020 sec = 20ms
    float delta_sec = HOP_DURATION_MS / 1000.0f;  // 20ms per hop
    m_tempo.updateTempo(delta_sec);

    // Store for change detection (used by getTempo() diagnostics)
    m_lastTempoOutput = m_tempo.getOutput();
    TRACE_END();  // tempo_update

    // Note: advancePhase() is called by RendererActor at 120 FPS
    // This separation allows smooth beat tracking at render rate
    // while novelty and tempo updates happen at audio rate (~50 Hz @ 12.8kHz)

    // ========================================================================
    // 64-bin Goertzel Analysis (Sensory Bridge parity)
    // Runs less frequently - needs 1500 samples (~94ms to accumulate)
    // ========================================================================
    // DEFENSIVE: Clear buffers before use (moved from stack to class members to reduce stack usage)
    memset(m_bins64Raw, 0, sizeof(m_bins64Raw));
    memset(m_bands64Folded, 0, sizeof(m_bands64Folded));
    
    const bool bins64Ready = m_analyzer.analyze64(m_bins64Raw);
    if (bins64Ready) {
        TRACE_BEGIN("goertzel64_fold");

        // Fold 64 bins -> 8 bands (8 bins per band, take max)
        for (size_t bin = 0; bin < GoertzelAnalyzer::NUM_BINS; ++bin) {
            size_t bandIdx = bin >> 3;  // bin / 8
            // DEFENSIVE: Bounds check to prevent out-of-bounds access
            if (bandIdx < 8) {
                m_bands64Folded[bandIdx] = std::max(m_bands64Folded[bandIdx], m_bins64Raw[bin]);
            }
        }

        // Store for logging comparison
        for (int i = 0; i < 8; ++i) {
            m_lastBands64[i] = m_bands64Folded[i];
        }
        m_analyze64Ready = true;

        // Cache 64-bin spectrum for TempoTracker novelty input
        // This is used every hop for tempo detection (stale data better than coarse 8-band)
        memcpy(m_bins64Cached, m_bins64Raw, sizeof(m_bins64Cached));

        // Phase 1.3: Publish full 64-bin spectrum to ControlBusRawInput
        // Apply activity gating and store in raw.bins64 for ControlBus passthrough
        for (size_t i = 0; i < GoertzelAnalyzer::NUM_BINS; ++i) {
            raw.bins64[i] = m_bins64Raw[i] * activity;
        }

        // [DIAG A4] 64-bin Goertzel trigger and magnitude
        {
            auto& dbgCfg = audio::getAudioDebugConfig();
            static uint32_t s_a4Count = 0;
            s_a4Count++;
            if (dbgCfg.verbosity >= 5 && (s_a4Count % 10) == 0) {
                float maxBin = 0.0f;
                size_t maxIdx = 0;
                for (size_t i = 0; i < GoertzelAnalyzer::NUM_BINS; ++i) {
                    if (m_bins64Raw[i] > maxBin) { maxBin = m_bins64Raw[i]; maxIdx = i; }
                }
                LW_LOGD(LW_CLR_MAGENTA "[DIAG-A4]" LW_ANSI_RESET
                        " 64bin trigger#%lu maxBin[%u]=%.4f adaptiveMax=%.4f",
                        (unsigned long)s_a4Count, (unsigned)maxIdx, maxBin, m_bins64AdaptiveMax);
            }
        }

        // Throttled 64-bin logging - gated by verbosity >= 5 (TRACE)
        // NOTE: Elevated from level 4 to level 5 to reduce default noise
        // For one-shot spectrum info, use 'adbg spectrum' command
        auto& dbgCfg64 = getAudioDebugConfig();
        // DEFENSIVE: Validate interval to prevent division by zero or invalid access
        uint16_t interval = dbgCfg64.interval64Bin();
        if (interval == 0) {
            interval = 1;  // Safety fallback: prevent division by zero
        }

        if (dbgCfg64.verbosity >= 5 && ++m_goertzel64LogCounter >= interval) {
            m_goertzel64LogCounter = 0;
            // Condensed format: 64-bin folded to 8 bands
            LW_LOGD(LW_CLR_CYAN_DIM "64-bin:" LW_ANSI_RESET " [%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f]",
                     m_bands64Folded[0], m_bands64Folded[1], m_bands64Folded[2], m_bands64Folded[3],
                     m_bands64Folded[4], m_bands64Folded[5], m_bands64Folded[6], m_bands64Folded[7]);
        }

        TRACE_END();
    }

    // Persist 64-bin spectrum between analysis triggers (avoid "picket fence" zeros).
    // analyze64() completes every ~94ms; effects render every frame and expect stable bins.
    if (!bins64Ready) {
        for (size_t i = 0; i < GoertzelAnalyzer::NUM_BINS; ++i) {
            raw.bins64[i] = m_bins64Cached[i] * activity;
        }
    }

    // Sensory Bridge adaptive normalisation (max follower)
    // Always compute per-hop (even when bins are stale) to keep decay behaviour consistent.
    const float sbScale = tuning.bins64AdaptiveScale;
    const float sbFloor = tuning.bins64AdaptiveFloor;
    const float sbDecay = tuning.bins64AdaptiveDecay;
    const float sbRise = tuning.bins64AdaptiveRise;
    const float sbFall = tuning.bins64AdaptiveFall;

    float max_value = 0.00001f;
    for (size_t i = 0; i < GoertzelAnalyzer::NUM_BINS; ++i) {
        float scaled = raw.bins64[i] * sbScale;
        if (scaled > max_value) {
            max_value = scaled;
        }
    }
    max_value *= sbDecay;

    if (max_value > m_bins64AdaptiveMax) {
        float delta = max_value - m_bins64AdaptiveMax;
        m_bins64AdaptiveMax += delta * sbRise;
    } else if (m_bins64AdaptiveMax > max_value) {
        float delta = m_bins64AdaptiveMax - max_value;
        m_bins64AdaptiveMax -= delta * sbFall;
    }

    if (m_bins64AdaptiveMax < sbFloor) {
        m_bins64AdaptiveMax = sbFloor;
    }

    float multiplier = 1.0f / m_bins64AdaptiveMax;
    for (size_t i = 0; i < GoertzelAnalyzer::NUM_BINS; ++i) {
        raw.bins64Adaptive[i] = (raw.bins64[i] * sbScale) * multiplier;
    }

    // === Phase: Sensory Bridge parity side-car ===
    if (shouldRunSbSidecar(m_hopCount)) {
        processSbWaveformSidecar(raw);
        processSbBloomSidecar(raw);
    }

    // MabuTrace: Detect false trigger - activity gated but no significant band energy
    // This helps identify noise floor calibration issues
    if (goertzelTriggered && activity > 0.1f) {
        float totalBandEnergy = 0.0f;
        for (int i = 0; i < NUM_BANDS; ++i) {
            totalBandEnergy += raw.bands[i];
        }
        // If activity says "signal present" but bands show nothing, it's a false trigger
        if (totalBandEnergy < 0.05f) {
            TRACE_INSTANT("FALSE_TRIGGER");
        }
    }

    // === Phase: Chroma Analysis ===
    BENCH_START_PHASE();
    TRACE_BEGIN("chroma_analyze");
    bool chromaTriggered = false;

    // 6.5. Get chromagram
    float chromaRaw[12] = {0};
    static float lastChroma[12] = {0};
#if FEATURE_AUDIO_OA
    if (oaReady ? m_chromaAnalyzer.analyzeWindow(window512, ChromaAnalyzer::WINDOW_SIZE, chromaRaw)
                : m_chromaAnalyzer.analyze(chromaRaw)) {
        chromaTriggered = true;
#else
    if (m_chromaAnalyzer.analyze(chromaRaw)) {
        chromaTriggered = true;
#endif
        // Fresh chroma data available
        for (int i = 0; i < 12; ++i) {
            float chroma = mapLevelDb(chromaRaw[i], tuning.chromaDbFloor, tuning.chromaDbCeil);
            lastChroma[i] = chroma;
            raw.chroma[i] = chroma * activity;
        }
    } else {
        // No new chroma this hop - reuse last known chroma
        for (int i = 0; i < 12; ++i) {
            raw.chroma[i] = lastChroma[i] * activity;
        }
    }

    TRACE_END();
    BENCH_END_PHASE(chromaUs);
    BENCH_SET_FLAG(chromaTriggered, chromaTriggered ? 1 : 0);

#if FEATURE_AUDIO_OA
    // Update previous hop buffer for next window
    std::memcpy(m_prevHopCentered, m_hopBufferCentered, HOP_SIZE * sizeof(int16_t));
    m_prevHopValid = true;
#endif

    // === Phase: Noise Calibration ===
    // Process noise calibration state machine if active
    {
        uint32_t nowMs = (uint32_t)(now_us / 1000);
        // Use pre-gate RMS to avoid calibrating on gated (zeroed) signal
        processNoiseCalibration(m_lastRmsPreGain, raw.bands, raw.chroma, nowMs);
    }


    // === Phase: ControlBus Update ===
    BENCH_START_PHASE();
    TRACE_BEGIN("controlbus_build");

        // 7a. Populate tempo tracker state for rhythmic saliency
    // Effects use MusicalGrid via ctx.audio.*, not these fields directly
    raw.tempoLocked = m_lastTempoOutput.locked;
    raw.tempoConfidence = m_lastTempoOutput.confidence;
    raw.tempoBeatTick = m_lastTempoOutput.beat_tick && m_lastTempoOutput.locked;
    raw.tempoDownbeatTick = false;
    if (raw.tempoBeatTick) {
        raw.tempoDownbeatTick = (m_standardBeatInBar == 0u);
        m_standardBeatInBar = static_cast<uint8_t>((m_standardBeatInBar + 1u) % 4u);
    }

    // 7. Update ControlBus with attack/release smoothing
#ifndef NATIVE_BUILD
    portENTER_CRITICAL(&m_controlBusApiMux);
#endif
    m_controlBus.setSmoothing(tuning.controlBusAlphaFast, tuning.controlBusAlphaSlow);
#ifdef AUDIO_SILENCE_GATE_DISABLED
    m_controlBus.setSilenceParameters(tuning.silenceThreshold, 0.0f);
#else
    m_controlBus.setSilenceParameters(tuning.silenceThreshold, tuning.silenceHysteresisMs);
#endif
    m_controlBus.UpdateFromHop(now, raw);
#ifndef NATIVE_BUILD
    portEXIT_CRITICAL(&m_controlBusApiMux);
#endif

    TRACE_END();  // controlbus_build
    BENCH_END_PHASE(controlBusUs);

    // === Phase: Style Detection ===
    // Update style detector with current hop features (after ControlBus has chord state)
#if FEATURE_STYLE_DETECTION
    {
        const ControlBusFrame frameRef = getControlBusFrameSnapshot();
        bool chordChanged = (frameRef.chordState.rootNote != m_prevChordRoot);
        m_prevChordRoot = frameRef.chordState.rootNote;
        // Use TempoTracker beat tracker confidence for style detection
        float beatConfidence = m_lastTempoOutput.locked ? m_lastTempoOutput.confidence : 0.0f;
        m_styleDetector.update(
            rmsMapped,
            fluxMapped,
            raw.bands,
            beatConfidence,
            chordChanged
        );
    }
#endif

    // === Phase: Publish ===
    BENCH_START_PHASE();
    TRACE_BEGIN("snapshot_publish");

    // 8. Publish frame to renderer via lock-free SnapshotBuffer
    // Copy style detection results to frame before publishing
    {
        ControlBusFrame frameToPublish = getControlBusFrameSnapshot();
#if FEATURE_STYLE_DETECTION
        frameToPublish.currentStyle = m_styleDetector.getStyle();
        frameToPublish.styleConfidence = m_styleDetector.getConfidence();
#else
        frameToPublish.currentStyle = MusicStyle::UNKNOWN;
        frameToPublish.styleConfidence = 0.0f;
#endif
        // Sensory Bridge parity fields (side-car pipeline)
#if FEATURE_SB_PARITY_SIDECAR
        std::memcpy(frameToPublish.sb_waveform, m_sbWaveform, sizeof(m_sbWaveform));
        frameToPublish.sb_waveform_peak_scaled = m_sbWaveformPeakScaled;
        frameToPublish.sb_waveform_peak_scaled_last = m_sbWaveformPeakScaledLast;
        std::memcpy(frameToPublish.sb_note_chromagram, m_sbNoteChroma, sizeof(m_sbNoteChroma));
        frameToPublish.sb_chromagram_max_val = m_sbChromaMaxVal;
        std::memcpy(frameToPublish.sb_spectrogram, m_sbSpectrogram, sizeof(m_sbSpectrogram));
        std::memcpy(frameToPublish.sb_spectrogram_smooth, m_sbSpectrogramSmooth, sizeof(m_sbSpectrogramSmooth));
        std::memcpy(frameToPublish.sb_chromagram_smooth, m_sbChromagramSmooth, sizeof(m_sbChromagramSmooth));
        frameToPublish.sb_hue_position = m_sbHuePosition;
        frameToPublish.sb_hue_shifting_mix = m_sbHueShiftingMix;
#endif
#if FEATURE_TRANSLATION_ENGINE
        {
            const AudioFeatures translated = buildTranslationFeatures(frameToPublish);
            const float previousHeldBpm = m_translationState.held_bpm;
            const bool previousTimingReliable = m_translationState.timing_reliable;
            const float dtSeconds = computeTranslationDeltaSeconds(
                frameToPublish,
                m_translationLastAudioTime,
                m_translationHaveLastAudioTime
            );
            translation_update(&translated, dtSeconds, &m_translationState);
            translation_get_parameters(&m_translationState, &frameToPublish.scene);
#if FEATURE_TRANSLATION_DEBUG
            const uint64_t logNowUs = (frameToPublish.t.monotonic_us != 0ULL)
                ? frameToPublish.t.monotonic_us
                : esp_timer_get_time();
            const bool latchDetected =
                (fabsf(m_translationState.held_bpm - previousHeldBpm) > 0.05f) ||
                (!previousTimingReliable && m_translationState.timing_reliable);
            recordTranslationLatch(
                translated,
                frameToPublish.tempoLocked,
                latchDetected,
                logNowUs,
                m_translationLastLatchValid,
                m_translationLastLatchBpm,
                m_translationLastLatchConfidence,
                m_translationLastLatchUs,
                m_translationLastLatchTempoLocked,
                m_translationLastLatchBeatTick,
                m_translationLastLatchDownbeatTick
            );
            maybeLogTranslationEvent(
                translated,
                m_translationState,
                clamp01(raw.rmsUngated),
                frameToPublish.tempoLocked,
                previousHeldBpm,
                previousTimingReliable,
                logNowUs
            );
            maybeLogTranslationSnapshot(
                translated,
                m_translationState,
                clamp01(raw.rmsUngated),
                m_translationLastLatchValid,
                m_translationLastLatchBpm,
                m_translationLastLatchConfidence,
                m_translationLastLatchUs,
                m_translationLastLatchTempoLocked,
                m_translationLastLatchBeatTick,
                m_translationLastLatchDownbeatTick,
                logNowUs,
                m_translationLastDebugLogUs
            );
#endif
        }
#else
        frameToPublish.scene = kDefaultSceneParameters;
#endif
        m_controlBusBuffer.Publish(frameToPublish);
        
        // Phase 1.2: Track publish statistics
        m_diag.publishCount++;
        m_diag.lastPublishTimeUs = esp_timer_get_time();
        
        // Detect sequence gaps (indicates missed frames)
        uint32_t expectedSeq = m_diag.lastPublishSeq + 1;
        if (m_diag.lastPublishSeq > 0 && frameToPublish.hop_seq != expectedSeq) {
            m_diag.publishSeqGaps++;
        }
        m_diag.lastPublishSeq = frameToPublish.hop_seq;
    }

    TRACE_END();  // snapshot_publish
    BENCH_END_PHASE(publishUs);

    // === End Frame: Push sample to ring buffer ===
    BENCH_END_FRAME(&m_benchmarkRing);

#if FEATURE_AUDIO_BENCHMARK
    // Aggregate stats periodically (~1 second)
    if (++m_benchmarkAggregateCounter >= BENCHMARK_AGGREGATE_INTERVAL) {
        aggregateBenchmarkStats();
        m_benchmarkAggregateCounter = 0;
    }
#endif
}

void AudioActor::processSbWaveformSidecar(const ControlBusRawInput& raw)
{
    // Store latest waveform and push into 4-frame history (3.1.0 parity)
    for (uint8_t i = 0; i < SB_WAVEFORM_POINTS; ++i) {
        int16_t sample = raw.waveform[i];
        m_sbWaveform[i] = sample;
        m_sbWaveformHistory[m_sbWaveformHistoryIndex][i] = sample;
    }
    m_sbWaveformHistoryIndex++;
    if (m_sbWaveformHistoryIndex >= SB_WAVEFORM_HISTORY) {
        m_sbWaveformHistoryIndex = 0;
    }

    // Peak follower (3.1.0 parity)
    float maxWaveformValRaw = 0.0f;
    for (uint8_t i = 0; i < SB_WAVEFORM_POINTS; ++i) {
        int16_t sample = m_sbWaveform[i];
        int16_t absSample = (sample < 0) ? -sample : sample;
        if (absSample > maxWaveformValRaw) {
            maxWaveformValRaw = static_cast<float>(absSample);
        }
    }

    float maxWaveformVal = maxWaveformValRaw - 750.0f;  // Sweet spot min level
    if (maxWaveformVal < 0.0f) {
        maxWaveformVal = 0.0f;
    }

    if (maxWaveformVal > m_sbMaxWaveformValFollower) {
        float delta = maxWaveformVal - m_sbMaxWaveformValFollower;
        m_sbMaxWaveformValFollower += delta * 0.6f;
    } else if (maxWaveformVal < m_sbMaxWaveformValFollower) {
        float delta = m_sbMaxWaveformValFollower - maxWaveformVal;
        m_sbMaxWaveformValFollower -= delta * 0.03f;
        if (m_sbMaxWaveformValFollower < 750.0f) {
            m_sbMaxWaveformValFollower = 750.0f;
        }
    }

    float waveformPeakScaledRaw = 0.0f;
    if (m_sbMaxWaveformValFollower > 0.0f) {
        waveformPeakScaledRaw = maxWaveformVal / m_sbMaxWaveformValFollower;
    }

    if (waveformPeakScaledRaw > m_sbWaveformPeakScaled) {
        float delta = waveformPeakScaledRaw - m_sbWaveformPeakScaled;
        m_sbWaveformPeakScaled += delta * 0.7f;
    } else if (waveformPeakScaledRaw < m_sbWaveformPeakScaled) {
        float delta = m_sbWaveformPeakScaled - waveformPeakScaledRaw;
        m_sbWaveformPeakScaled -= delta * 0.7f;
    }

    m_sbWaveformPeakScaledLast =
        (m_sbWaveformPeakScaled * 0.3f) + (m_sbWaveformPeakScaledLast * 0.7f);

    // 3.1.0 chromagram (note_spectrogram -> note_chromagram)
    m_sbChromaMaxVal = 0.0f;
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        m_sbNoteChroma[i] = 0.0f;
    }
    for (uint8_t octave = 0; octave < 6; ++octave) {
        for (uint8_t note = 0; note < CONTROLBUS_NUM_CHROMA; ++note) {
            uint16_t noteIndex = static_cast<uint16_t>(12 * octave + note);
            if (noteIndex < SB_NUM_FREQS) {
                float val = raw.bins64Adaptive[noteIndex];
                m_sbNoteChroma[note] += val;
                if (m_sbNoteChroma[note] > 1.0f) {
                    m_sbNoteChroma[note] = 1.0f;
                }
                if (m_sbNoteChroma[note] > m_sbChromaMaxVal) {
                    m_sbChromaMaxVal = m_sbNoteChroma[note];
                }
            }
        }
    }
    if (m_sbChromaMaxVal < 0.0001f) {
        m_sbChromaMaxVal = 0.0001f;
    }
}

void AudioActor::processSbBloomSidecar(const ControlBusRawInput& raw)
{
    // 4.1.1 spectrogram smoothing (low-pass with mood influence)
    float moodNorm = static_cast<float>(m_controlBus.getMood()) / 255.0f;
    float smoothingRate = 25.0f + (5.0f * moodNorm);
    float alpha = 1.0f - std::exp(-smoothingRate * (HOP_DURATION_MS * 0.001f));

    for (uint8_t i = 0; i < SB_NUM_FREQS; ++i) {
        float target = raw.bins64Adaptive[i];
        m_sbSpectrogram[i] = m_sbSpectrogram[i] + (target - m_sbSpectrogram[i]) * alpha;
    }

    // 4.1.1 get_smooth_spectrogram follower
    for (uint8_t i = 0; i < SB_NUM_FREQS; ++i) {
        float noteBrightness = m_sbSpectrogram[i];
        if (m_sbSpectrogramSmooth[i] < noteBrightness) {
            float distance = noteBrightness - m_sbSpectrogramSmooth[i];
            m_sbSpectrogramSmooth[i] += distance * 0.95f;
        } else if (m_sbSpectrogramSmooth[i] > noteBrightness) {
            float distance = m_sbSpectrogramSmooth[i] - noteBrightness;
            m_sbSpectrogramSmooth[i] -= distance * 0.95f;
        }
        if (m_sbSpectrogramSmooth[i] < 0.0f) m_sbSpectrogramSmooth[i] = 0.0f;
        if (m_sbSpectrogramSmooth[i] > 1.0f) m_sbSpectrogramSmooth[i] = 1.0f;
    }

    // 4.1.1 make_smooth_chromagram
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        m_sbChromagramSmooth[i] = 0.0f;
    }
    const float chromaDiv = 64.0f / 12.0f;
    for (uint8_t i = 0; i < SB_NUM_FREQS; ++i) {
        float noteMagnitude = m_sbSpectrogramSmooth[i];
        if (noteMagnitude < 0.0f) noteMagnitude = 0.0f;
        if (noteMagnitude > 1.0f) noteMagnitude = 1.0f;
        uint8_t chromaBin = static_cast<uint8_t>(i % 12);
        m_sbChromagramSmooth[chromaBin] += noteMagnitude / chromaDiv;
    }

    m_sbChromagramMaxPeak *= 0.98f;
    if (m_sbChromagramMaxPeak < 0.01f) {
        m_sbChromagramMaxPeak = 0.01f;
    }
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        if (m_sbChromagramSmooth[i] > m_sbChromagramMaxPeak) {
            float distance = m_sbChromagramSmooth[i] - m_sbChromagramMaxPeak;
            m_sbChromagramMaxPeak += distance * 0.5f;
        }
    }
    float multiplier = 1.0f / m_sbChromagramMaxPeak;
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        m_sbChromagramSmooth[i] *= multiplier;
        if (m_sbChromagramSmooth[i] > 1.0f) m_sbChromagramSmooth[i] = 1.0f;
    }

    updateSbNoveltyAndHueShift();
}

void AudioActor::updateSbNoveltyAndHueShift()
{
    // Calculate novelty (4.1.1 calculate_novelty)
    int16_t roundedIndex = static_cast<int16_t>(m_sbSpectralHistoryIndex) - 1;
    while (roundedIndex < 0) {
        roundedIndex += SB_SPECTRAL_HISTORY;
    }

    float noveltyNow = 0.0f;
    for (uint8_t i = 0; i < SB_NUM_FREQS; ++i) {
        float noveltyBin = m_sbSpectrogram[i] - m_sbSpectralHistory[roundedIndex][i];
        if (noveltyBin < 0.0f) noveltyBin = 0.0f;
        noveltyNow += noveltyBin;
    }
    noveltyNow /= SB_NUM_FREQS;

    for (uint8_t i = 0; i < SB_NUM_FREQS; ++i) {
        m_sbSpectralHistory[m_sbSpectralHistoryIndex][i] = m_sbSpectrogram[i];
    }
    m_sbNoveltyCurve[m_sbSpectralHistoryIndex] = std::sqrt(noveltyNow);

    m_sbSpectralHistoryIndex++;
    if (m_sbSpectralHistoryIndex >= SB_SPECTRAL_HISTORY) {
        m_sbSpectralHistoryIndex = 0;
    }

    // Process colour shift (4.1.1 process_color_shift)
    int16_t noveltyIndex = static_cast<int16_t>(m_sbSpectralHistoryIndex) - 1;
    while (noveltyIndex < 0) {
        noveltyIndex += SB_SPECTRAL_HISTORY;
    }
    float noveltyVal = m_sbNoveltyCurve[noveltyIndex];

    noveltyVal -= 0.10f;
    if (noveltyVal < 0.0f) {
        noveltyVal = 0.0f;
    }
    noveltyVal *= 1.111111f;
    noveltyVal = noveltyVal * noveltyVal * noveltyVal;
    if (noveltyVal > 0.05f) {
        noveltyVal = 0.05f;
    }

    if (noveltyVal > m_sbHueShiftSpeed) {
        m_sbHueShiftSpeed = noveltyVal * 0.75f;
    } else {
        m_sbHueShiftSpeed *= 0.99f;
    }

    m_sbHuePosition += (m_sbHueShiftSpeed * m_sbHuePushDirection);
    while (m_sbHuePosition < 0.0f) {
        m_sbHuePosition += 1.0f;
    }
    while (m_sbHuePosition >= 1.0f) {
        m_sbHuePosition -= 1.0f;
    }

    if (std::fabs(m_sbHuePosition - m_sbHueDestination) <= 0.01f) {
        m_sbHuePushDirection *= -1.0f;
        m_sbHueShiftingMixTarget *= -1.0f;
        m_sbRand = m_sbRand * 1664525u + 1013904223u;
        m_sbHueDestination = static_cast<float>(m_sbRand) / 4294967295.0f;
    }

    float hueMixDistance = std::fabs(m_sbHueShiftingMix - m_sbHueShiftingMixTarget);
    if (m_sbHueShiftingMix < m_sbHueShiftingMixTarget) {
        m_sbHueShiftingMix += hueMixDistance * 0.01f;
    } else if (m_sbHueShiftingMix > m_sbHueShiftingMixTarget) {
        m_sbHueShiftingMix -= hueMixDistance * 0.01f;
    }
}

float AudioActor::computeRMS(const int16_t* samples, size_t count)
{
    if (count == 0) return 0.0f;

    // Accumulate sum of squares
    int64_t sumSq = 0;
    for (size_t i = 0; i < count; ++i) {
        int32_t s = samples[i];
        sumSq += s * s;
    }

    // Compute RMS and normalize to [0.0, 1.0]
    // Max int16_t is 32767, so max RMS is 32767 (for DC signal)
    float rms = std::sqrt(static_cast<float>(sumSq) / count);
    return std::min(1.0f, rms / 32768.0f);
}

void AudioActor::handleCaptureError(CaptureResult result)
{
    // Log error based on type
    switch (result) {
        case CaptureResult::NOT_INITIALIZED:
            LW_LOGE("Capture error: not initialized");
            m_state = AudioActorState::ERROR;
            m_stats.state = m_state;
            break;

        case CaptureResult::DMA_TIMEOUT:
            // DMA timeouts can be transient - don't change state
            LW_LOGW("Capture: DMA timeout");
            break;

        case CaptureResult::READ_ERROR:
            LW_LOGW("Capture: read error");
            break;

        case CaptureResult::BUFFER_OVERFLOW:
            LW_LOGW("Capture: buffer overflow");
            break;

        default:
            LW_LOGW("Capture: unknown error %d", static_cast<int>(result));
            break;
    }

    // If too many consecutive failures, consider recovery
    // For now, just log - Phase 2 may add auto-recovery logic
}

// ============================================================================
// Phase 2B: Benchmark Aggregation
// ============================================================================

#if FEATURE_AUDIO_BENCHMARK
void AudioActor::aggregateBenchmarkStats()
{
    // Pop all available samples and update stats
    AudioBenchmarkSample sample;
    while (m_benchmarkRing.pop(sample)) {
        m_benchmarkStats.updateFromSample(sample);
    }

    // MabuTrace: Record CPU load as a counter for Perfetto visualization
    TRACE_COUNTER("cpu_load", static_cast<int32_t>(m_benchmarkStats.cpuLoadPercent * 100));
}
#endif


// ============================================================================
// Noise Calibration (SensoryBridge pattern)
// ============================================================================

bool AudioActor::startNoiseCalibration(uint32_t durationMs, float safetyMultiplier)
{
    // Only start if not already running
    if (m_noiseCalibration.state == CalibrationState::MEASURING ||
        m_noiseCalibration.state == CalibrationState::REQUESTED) {
        LW_LOGW("Calibration already in progress");
        return false;
    }

    // Reset and configure
    m_noiseCalibration.reset();
    m_noiseCalibration.durationMs = durationMs;
    m_noiseCalibration.safetyMultiplier = safetyMultiplier;
    m_noiseCalibration.state = CalibrationState::REQUESTED;

    LW_LOGI("Noise calibration requested: %ums, multiplier=%.2f",
             durationMs, safetyMultiplier);
    return true;
}

void AudioActor::cancelNoiseCalibration()
{
    if (m_noiseCalibration.state != CalibrationState::IDLE) {
        LW_LOGI("Calibration cancelled");
        m_noiseCalibration.reset();
    }
}

bool AudioActor::applyCalibrationResults()
{
    if (!m_noiseCalibration.result.valid) {
        LW_LOGW("Cannot apply: no valid calibration results");
        return false;
    }

    // Copy results to pipeline tuning
    AudioPipelineTuning tuning = getPipelineTuning();
    for (int i = 0; i < 8; ++i) {
        tuning.perBandNoiseFloors[i] = m_noiseCalibration.result.bandFloors[i];
    }
    tuning.usePerBandNoiseFloor = true;

    // Also update the global noise floor minimum based on measured RMS
    tuning.noiseFloorMin = m_noiseCalibration.result.overallRms * m_noiseCalibration.safetyMultiplier;

    setPipelineTuning(tuning);

    LW_LOGI("Applied calibration: noiseFloorMin=%.6f, perBand enabled",
             tuning.noiseFloorMin);
    return true;
}

void AudioActor::processNoiseCalibration(float rms, const float* bands, const float* chroma, uint32_t nowMs)
{
    switch (m_noiseCalibration.state) {
        case CalibrationState::IDLE:
        case CalibrationState::COMPLETE:
        case CalibrationState::FAILED:
            // Nothing to do
            return;

        case CalibrationState::REQUESTED:
            // Transition to measuring - start the timer
            m_noiseCalibration.startTimeMs = nowMs;
            m_noiseCalibration.state = CalibrationState::MEASURING;
            LW_LOGI("Calibration started: measuring for %ums", m_noiseCalibration.durationMs);
            // Fall through to MEASURING
            [[fallthrough]];

        case CalibrationState::MEASURING: {
            // Check for timeout
            uint32_t elapsed = nowMs - m_noiseCalibration.startTimeMs;
            if (elapsed >= m_noiseCalibration.durationMs) {
                // Calibration complete - compute results
                if (m_noiseCalibration.sampleCount > 0) {
                    float invCount = 1.0f / static_cast<float>(m_noiseCalibration.sampleCount);

                    m_noiseCalibration.result.overallRms = m_noiseCalibration.rmsSum * invCount;
                    m_noiseCalibration.result.peakRms = m_noiseCalibration.peakRms;
                    m_noiseCalibration.result.sampleCount = m_noiseCalibration.sampleCount;

                    // Compute per-band floors with safety multiplier
                    for (int i = 0; i < 8; ++i) {
                        float avg = m_noiseCalibration.bandSum[i] * invCount;
                        m_noiseCalibration.result.bandFloors[i] = avg * m_noiseCalibration.safetyMultiplier;
                    }
                    for (int i = 0; i < 12; ++i) {
                        float avg = m_noiseCalibration.chromaSum[i] * invCount;
                        m_noiseCalibration.result.chromaFloors[i] = avg * m_noiseCalibration.safetyMultiplier;
                    }

                    m_noiseCalibration.result.valid = true;
                    m_noiseCalibration.state = CalibrationState::COMPLETE;

                    LW_LOGI("Calibration complete: avgRMS=%.6f, peak=%.6f, samples=%u",
                             m_noiseCalibration.result.overallRms,
                             m_noiseCalibration.result.peakRms,
                             m_noiseCalibration.result.sampleCount);
                    LW_LOGI("  Bands: [%.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f]",
                             m_noiseCalibration.result.bandFloors[0],
                             m_noiseCalibration.result.bandFloors[1],
                             m_noiseCalibration.result.bandFloors[2],
                             m_noiseCalibration.result.bandFloors[3],
                             m_noiseCalibration.result.bandFloors[4],
                             m_noiseCalibration.result.bandFloors[5],
                             m_noiseCalibration.result.bandFloors[6],
                             m_noiseCalibration.result.bandFloors[7]);
                } else {
                    LW_LOGE("Calibration failed: no samples collected");
                    m_noiseCalibration.state = CalibrationState::FAILED;
                }
                return;
            }

            // Check for too much noise (abort if not silence)
            if (rms > m_noiseCalibration.maxAllowedRms) {
                LW_LOGW("Calibration aborted: RMS %.4f exceeds max %.4f (not silent)",
                         rms, m_noiseCalibration.maxAllowedRms);
                m_noiseCalibration.state = CalibrationState::FAILED;
                return;
            }

            // Accumulate samples
            m_noiseCalibration.rmsSum += rms;
            if (rms > m_noiseCalibration.peakRms) {
                m_noiseCalibration.peakRms = rms;
            }

            for (int i = 0; i < 8; ++i) {
                m_noiseCalibration.bandSum[i] += bands[i];
            }
            for (int i = 0; i < 12; ++i) {
                m_noiseCalibration.chromaSum[i] += chroma[i];
            }
            m_noiseCalibration.sampleCount++;

            // Progress logging (~once per second)
            if ((m_noiseCalibration.sampleCount % 62) == 0) {
                float progress = (float)elapsed / (float)m_noiseCalibration.durationMs * 100.0f;
                LW_LOGD("Calibrating: %.0f%% (%u samples, avgRMS=%.5f)",
                         progress, m_noiseCalibration.sampleCount,
                         m_noiseCalibration.rmsSum / m_noiseCalibration.sampleCount);
            }
            break;
        }
    }
}

#endif // FEATURE_AUDIO_BACKEND_ESV11 / PIPELINECORE / Goertzel

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
