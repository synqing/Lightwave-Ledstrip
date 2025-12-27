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

// Always include macros - they define no-ops when FEATURE_AUDIO_BENCHMARK is disabled
#include "AudioBenchmarkMacros.h"

// MabuTrace integration for Perfetto timeline visualization
// No-ops when FEATURE_MABUTRACE is disabled
#include "AudioBenchmarkTrace.h"

#ifndef NATIVE_BUILD
#include <esp_log.h>
#include <esp_timer.h>
#else
// Native build stubs for ESP-IDF APIs
#define ESP_LOGI(tag, fmt, ...)
#define ESP_LOGD(tag, fmt, ...)
#define ESP_LOGE(tag, fmt, ...)
#define ESP_LOGW(tag, fmt, ...)
inline uint64_t esp_timer_get_time() { return 0; }
inline uint32_t esp_log_timestamp() { return 0; }
#endif

static const char* TAG = "AudioActor";

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
        ESP_LOGI(TAG, "Pausing audio capture");
        m_state = AudioActorState::PAUSED;
    }
}

void AudioActor::resume()
{
    if (m_state == AudioActorState::PAUSED) {
        ESP_LOGI(TAG, "Resuming audio capture");
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
    ESP_LOGI(TAG, "AudioActor starting on Core %d", xPortGetCoreID());

    m_state = AudioActorState::INITIALIZING;
    m_stats.state = m_state;

    // Initialize I2S audio capture
    if (!m_capture.init()) {
        ESP_LOGE(TAG, "Failed to initialize audio capture");
        m_state = AudioActorState::ERROR;
        m_stats.state = m_state;
        return;
    }

    m_state = AudioActorState::RUNNING;
    m_stats.state = m_state;

    ESP_LOGI(TAG, "AudioActor started (tick=%dms, hop=%d, rate=%.1fHz)",
             AUDIO_ACTOR_TICK_MS, HOP_SIZE, HOP_RATE_HZ);
}

void AudioActor::onMessage(const actors::Message& msg)
{
    switch (msg.type) {
        case actors::MessageType::SHUTDOWN:
            ESP_LOGI(TAG, "Received SHUTDOWN message");
            // Will be handled by base class
            break;

        case actors::MessageType::HEALTH_CHECK:
            ESP_LOGD(TAG, "Health check: state=%d, captures=%lu",
                     static_cast<int>(m_state), m_stats.captureSuccessCount);
            // TODO: Send HEALTH_STATUS response when MessageBus is integrated
            break;

        case actors::MessageType::PING:
            // Respond with PONG for latency testing
            // TODO: Send PONG via MessageBus
            ESP_LOGD(TAG, "PING received");
            break;

        default:
            // Ignore unknown messages
            ESP_LOGD(TAG, "Ignoring message type 0x%02X",
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

    // Log periodically (every 620 ticks = ~10 seconds) - just enough to confirm alive
    if ((m_stats.tickCount % 620) == 0) {
        const CaptureStats& cstats = m_capture.getStats();
        const ControlBusFrame& frame = m_controlBus.GetFrame();
        ESP_LOGI(TAG, "Audio alive: cap=%lu pk=%d pkC=%d rms=%.4f->%.3f pre=%.4f g=%.2f dc=%.1f clip=%u flux=%.3f min=%d max=%d mean=%.1f",
                 cstats.hopsCapured, cstats.peakSample, m_lastPeakCentered, m_lastRmsRaw, frame.rms,
                 m_lastRmsPreGain, m_lastAgcGain, m_lastDcEstimate, (unsigned)m_lastClipCount, m_lastFluxMapped,
                 m_lastMinSample, m_lastMaxSample, m_lastMeanSample);
    }
}

void AudioActor::onStop()
{
    ESP_LOGI(TAG, "AudioActor stopping");

    // Deinitialize audio capture
    m_capture.deinit();

    m_state = AudioActorState::UNINITIALIZED;
    m_stats.state = m_state;

    // Log final statistics
    ESP_LOGI(TAG, "Final stats:");
    ESP_LOGI(TAG, "  Total ticks: %lu", m_stats.tickCount);
    ESP_LOGI(TAG, "  Successful captures: %lu", m_stats.captureSuccessCount);
    ESP_LOGI(TAG, "  Failed captures: %lu", m_stats.captureFailCount);

    const CaptureStats& cstats = m_capture.getStats();
    ESP_LOGI(TAG, "  DMA timeouts: %lu", cstats.dmaTimeouts);
    ESP_LOGI(TAG, "  Read errors: %lu", cstats.readErrors);
    ESP_LOGI(TAG, "  Max read time: %lu us", cstats.maxReadTimeUs);
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
        m_analyzer.reset();
        m_chromaAnalyzer.reset();
        m_controlBus.Reset();
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

    // 3. Compute spectral flux (half-wave rectified RMS derivative)
    float fluxMapped = std::max(0.0f, rmsMapped - m_prevRMS) * tuning.fluxScale;
    if (fluxMapped > 1.0f) fluxMapped = 1.0f;
    m_prevRMS = rmsMapped;
    m_lastFluxMapped = fluxMapped;

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
#if FEATURE_AUDIO_OA
    if (m_prevHopValid) {
        std::memcpy(window512, m_prevHopCentered, HOP_SIZE * sizeof(int16_t));
        std::memcpy(window512 + HOP_SIZE, m_hopBufferCentered, HOP_SIZE * sizeof(int16_t));
        oaReady = true;
    }
#else
    // Fallback to original accumulation for 32ms cadence
    m_analyzer.accumulate(m_hopBufferCentered, HOP_SIZE);
    m_chromaAnalyzer.accumulate(m_hopBufferCentered, HOP_SIZE);
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
            m_lastBands[i] = band;
            raw.bands[i] = band * activity;
        }

        // Throttle Goertzel debug logging to once per ~2 seconds (prevents serial spam)
        m_goertzelLogCounter++;
        if (m_goertzelLogCounter >= GOERTZEL_LOG_INTERVAL) {
            m_goertzelLogCounter = 0;
            // Use ANSI color codes (bright green) for visual distinction from DMA dbg
            ESP_LOGD(TAG,
                     "\033[1;32mGoertzel: raw=[%.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f] "
                     "map=[%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f] "
                     "rms=%.4f->%.3f pre=%.4f g=%.2f dc=%.1f clip=%u pk=%d pkC=%d min=%d max=%d mean=%.1f\033[0m",
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

    // === Phase: ControlBus Update ===
    BENCH_START_PHASE();

    // 7. Update ControlBus with attack/release smoothing
    m_controlBus.setSmoothing(tuning.controlBusAlphaFast, tuning.controlBusAlphaSlow);
    m_controlBus.UpdateFromHop(now, raw);

    BENCH_END_PHASE(controlBusUs);

    // === Phase: Publish ===
    BENCH_START_PHASE();

    // 8. Publish frame to renderer via lock-free SnapshotBuffer
    m_controlBusBuffer.Publish(m_controlBus.GetFrame());

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
            ESP_LOGE(TAG, "Capture error: not initialized");
            m_state = AudioActorState::ERROR;
            m_stats.state = m_state;
            break;

        case CaptureResult::DMA_TIMEOUT:
            // DMA timeouts can be transient - don't change state
            ESP_LOGW(TAG, "Capture: DMA timeout");
            break;

        case CaptureResult::READ_ERROR:
            ESP_LOGW(TAG, "Capture: read error");
            break;

        case CaptureResult::BUFFER_OVERFLOW:
            ESP_LOGW(TAG, "Capture: buffer overflow");
            break;

        default:
            ESP_LOGW(TAG, "Capture: unknown error %d", static_cast<int>(result));
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

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
