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

#if FEATURE_AUDIO_SYNC

#include <cstring>
#include <cmath>
#include <algorithm>

#ifndef NATIVE_BUILD
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
#include "tempo/TempoTracker.h"

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

AudioPipelineTuning AudioActor::getPipelineTuning() const
{
    AudioPipelineTuning out;
    uint32_t v0;
    uint32_t v1;
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
    uint32_t v1;
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
            // TODO: Send HEALTH_STATUS response when MessageBus is integrated
            break;

        case actors::MessageType::PING:
            // Respond with PONG for latency testing
            // TODO: Send PONG via MessageBus
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

    // Capture one hop of audio
    captureHop();

    // Record tick time
    m_stats.lastTickTimeUs = esp_timer_get_time() - tickStart;

    // Log periodically (every 620 ticks = ~10 seconds) - gated by verbosity >= 2
    auto& dbgCfg = getAudioDebugConfig();
    if (dbgCfg.verbosity >= 2 && (m_stats.tickCount % 620) == 0) {
        const CaptureStats& cstats = m_capture.getStats();
        const ControlBusFrame& frame = m_controlBus.GetFrame();
        // Calculate mic level in dB from pre-gain RMS
        float micLevelDb = (m_lastRmsPreGain > 0.0001f) ? (20.0f * log10f(m_lastRmsPreGain)) : -80.0f;
        LW_LOGI("Audio alive: " LW_CLR_YELLOW "mic=%.1fdB" LW_ANSI_RESET " cap=%lu pk=%d pkC=%d rms=%.4f->%.3f pre=%.4f g=%.2f dc=%.1f clip=%u flux=%.3f min=%d max=%d mean=%.1f",
                 micLevelDb, cstats.hopsCapured, cstats.peakSample, m_lastPeakCentered, m_lastRmsRaw, frame.rms,
                 m_lastRmsPreGain, m_lastAgcGain, m_lastDcEstimate, (unsigned)m_lastClipCount, m_lastFluxMapped,
                 m_lastMinSample, m_lastMaxSample, m_lastMeanSample);

        // Log spike detection stats (get from ControlBus)
        auto spikeStats = m_controlBus.getSpikeStats();
        LW_LOGI("Spike stats: frames=%lu detected=%lu corrected=%lu avg/frame=%.3f removed=%.2f",
                 spikeStats.totalFrames,
                 spikeStats.spikesDetectedBands + spikeStats.spikesDetectedChroma,
                 spikeStats.spikesCorrected,
                 spikeStats.avgSpikesPerFrame,
                 spikeStats.totalEnergyRemoved);

        // Log saliency detection metrics
#if FEATURE_MUSICAL_SALIENCY
        LW_LOGI("Saliency: overall=%.3f dom=%u H=%.3f R=%.3f T=%.3f D=%.3f",
                 frame.saliency.overallSaliency,
                 frame.saliency.dominantType,
                 frame.saliency.harmonicNoveltySmooth,
                 frame.saliency.rhythmicNoveltySmooth,
                 frame.saliency.timbralNoveltySmooth,
                 frame.saliency.dynamicNoveltySmooth);
#endif

        // Log style detection metrics (MIS Phase 2)
#if FEATURE_STYLE_DETECTION
        const StyleClassification& styleClass = m_styleDetector.getClassification();
        LW_LOGI("Style: %u conf=%.2f [R=%.2f H=%.2f M=%.2f T=%.2f D=%.2f]",
                 static_cast<uint8_t>(m_styleDetector.getStyle()),
                 m_styleDetector.getConfidence(),
                 styleClass.styleWeights[0],
                 styleClass.styleWeights[1],
                 styleClass.styleWeights[2],
                 styleClass.styleWeights[3],
                 styleClass.styleWeights[4]);
#endif

        // Log TempoTracker beat tracking metrics
        LW_LOGI(LW_CLR_MAGENTA "Beat:" LW_ANSI_RESET " BPM=%.1f conf=%.2f phase=%.2f lock=%s",
                 m_lastTempoOutput.bpm, m_lastTempoOutput.confidence,
                 m_lastTempoOutput.phase01, m_lastTempoOutput.locked ? "YES" : "no");
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
    CaptureResult result = m_capture.captureHop(m_hopBuffer);

    if (result == CaptureResult::SUCCESS) {
        m_stats.captureSuccessCount++;
        m_newHopAvailable = true;

        // Phase 2: Process the hop through DSP pipeline
        processHop();

    } else {
        m_stats.captureFailCount++;
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
        m_controlBus.Reset();
        // TempoTracker reset
        m_tempo.init();
        m_lastTempoOutput = m_tempo.getOutput();
    }

    // 1. Build AudioTime for this hop
    uint64_t now_us = esp_timer_get_time();
    AudioTime now(m_sampleIndex, SAMPLE_RATE, now_us);

    // Update monotonic counters
    m_sampleIndex += HOP_SIZE;
    m_hopCount++;

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
        m_noiseFloor += noiseFloorRise * (rmsPre - m_noiseFloor);
    }
    if (m_noiseFloor < noiseFloorMin) {
        m_noiseFloor = noiseFloorMin;
    }

    float gateStart = m_noiseFloor * gateStartFactor;
    float gateRange = std::max(gateRangeMin, m_noiseFloor * gateRangeFactor);
    float activity = clamp01((rmsPre - gateStart) / gateRange);

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

    BENCH_END_PHASE(dcAgcLoopUs);

    // === Phase: RMS Compute ===
    BENCH_START_PHASE();

    float rmsRaw = computeRMS(m_hopBufferCentered, HOP_SIZE);
    float rmsMapped = mapLevelDb(rmsRaw, tuning.rmsDbFloor, tuning.rmsDbCeil);
    rmsMapped *= activity;
    m_lastRmsRaw = rmsRaw;
    m_lastRmsMapped = rmsMapped;

    // Flux placeholder - will be computed after Goertzel if useSpectralFlux is enabled
    float fluxMapped = 0.0f;
    if (!m_noveltyTuning.useSpectralFlux) {
        // Legacy RMS-based flux (computed here since it only needs RMS)
        float spectralFlux = std::max(0.0f, rmsMapped - m_prevRMS);
        m_prevRMS = rmsMapped;
        fluxMapped = std::min(1.0f, spectralFlux * tuning.fluxScale);
        m_lastFluxMapped = fluxMapped;
    }

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

        // Throttle 8-band Goertzel debug logging - gated by verbosity >= 5
        auto& dbgCfg8 = getAudioDebugConfig();
        if (dbgCfg8.verbosity >= 5 && ++m_goertzelLogCounter >= dbgCfg8.interval8Band()) {
            m_goertzelLogCounter = 0;
            // Calculate TRUE mic level in dB from pre-gain RMS (0dB = full scale, silence floor at -60dB)
            float micLevelDb = (m_lastRmsPreGain > 0.0001f) ? (20.0f * log10f(m_lastRmsPreGain)) : -80.0f;
            // Bold cyan title, bold yellow for mic dB level, rest uncolored
            LW_LOGD(LW_CLR_CYAN "Goertzel:" LW_ANSI_RESET " " LW_CLR_YELLOW "%.1fdB" LW_ANSI_RESET " raw=[%.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f] "
                     "map=[%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f] "
                     "rms=%.4f->%.3f pre=%.4f g=%.2f dc=%.1f clip=%u pk=%d pkC=%d min=%d max=%d mean=%.1f",
                     micLevelDb,
                     bandsRaw[0], bandsRaw[1], bandsRaw[2], bandsRaw[3], bandsRaw[4], bandsRaw[5], bandsRaw[6], bandsRaw[7],
                     raw.bands[0], raw.bands[1], raw.bands[2], raw.bands[3], raw.bands[4], raw.bands[5], raw.bands[6], raw.bands[7],
                     rmsRaw, rmsMapped, m_lastRmsPreGain, m_lastAgcGain, m_lastDcEstimate, (unsigned)m_lastClipCount,
                     m_capture.getStats().peakSample, m_lastPeakCentered, m_lastMinSample, m_lastMaxSample, m_lastMeanSample);
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
    if (m_noveltyTuning.useSpectralFlux) {
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
        spectralFlux *= m_noveltyTuning.spectralFluxScale;
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
    
    if (m_analyzer.analyze64(m_bins64Raw)) {
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

        // Throttled 64-bin logging - gated by verbosity >= 4
        auto& dbgCfg64 = getAudioDebugConfig();
        // DEFENSIVE: Validate interval to prevent division by zero or invalid access
        uint16_t interval = dbgCfg64.interval64Bin();
        if (interval == 0) {
            interval = 1;  // Safety fallback: prevent division by zero
        }
        
        if (dbgCfg64.verbosity >= 4 && ++m_goertzel64LogCounter >= interval) {
            m_goertzel64LogCounter = 0;
            // DEFENSIVE: Validate array bounds before logging
            // Cyan for 64-bin spectral analysis (title-only coloring)
            LW_LOGD(LW_CLR_CYAN_DIM "64-bin Goertzel:" LW_ANSI_RESET " [%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f]",
                     m_bands64Folded[0], m_bands64Folded[1], m_bands64Folded[2], m_bands64Folded[3],
                     m_bands64Folded[4], m_bands64Folded[5], m_bands64Folded[6], m_bands64Folded[7]);
        }

        TRACE_END();
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
        processNoiseCalibration(rmsMapped, raw.bands, raw.chroma, nowMs);
    }


    // === Phase: ControlBus Update ===
    BENCH_START_PHASE();

        // 7a. Populate tempo tracker state for rhythmic saliency
    // Effects use MusicalGrid via ctx.audio.*, not these fields directly
    raw.tempoLocked = m_lastTempoOutput.locked;
    raw.tempoConfidence = m_lastTempoOutput.confidence;
    raw.tempoBeatTick = m_lastTempoOutput.beat_tick && m_lastTempoOutput.locked;

    // 7. Update ControlBus with attack/release smoothing
    m_controlBus.setSmoothing(tuning.controlBusAlphaFast, tuning.controlBusAlphaSlow);
    m_controlBus.setSilenceParameters(tuning.silenceThreshold, tuning.silenceHysteresisMs);
    m_controlBus.UpdateFromHop(now, raw);

    BENCH_END_PHASE(controlBusUs);

    // === Phase: Style Detection ===
    // Update style detector with current hop features (after ControlBus has chord state)
#if FEATURE_STYLE_DETECTION
    {
        bool chordChanged = (m_controlBus.GetFrame().chordState.rootNote != m_prevChordRoot);
        m_prevChordRoot = m_controlBus.GetFrame().chordState.rootNote;
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

    // 8. Publish frame to renderer via lock-free SnapshotBuffer
    // Copy style detection results to frame before publishing
    {
        ControlBusFrame frameToPublish = m_controlBus.GetFrame();
#if FEATURE_STYLE_DETECTION
        frameToPublish.currentStyle = m_styleDetector.getStyle();
        frameToPublish.styleConfidence = m_styleDetector.getConfidence();
#else
        frameToPublish.currentStyle = MusicStyle::UNKNOWN;
        frameToPublish.styleConfidence = 0.0f;
#endif
        m_controlBusBuffer.Publish(frameToPublish);
    }

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

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
