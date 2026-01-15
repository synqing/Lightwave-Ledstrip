/**
 * @file AudioNode.cpp
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

#include "AudioNode.h"

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

// K1 front-end
#include "k1/K1AudioFrontEnd.h"
#include "k1/FeatureBus.h"

// Runtime-configurable audio debug verbosity
#include "AudioDebugConfig.h"

// TempoTracker integration
// #include "tempo/TempoTracker.h" (Removed - Phase 2 cleanup)

// Perceptual band weights for spectral flux calculation (derived from K1 research)
// Reduced disparity to detect weak beats (hi-hats, snares) - matches TempoTracker weights
// Reduced from 4.67x to 2.4x disparity per research document recommendation
namespace {
    constexpr float PERCEPTUAL_BAND_WEIGHTS[8] = {
        1.2f,   // Band 0: Sub-bass (20-40Hz) - critical for kick drums
        1.1f,   // Band 1: Bass (40-80Hz) - fundamental bass notes
        1.0f,   // Band 2: Low-mid (80-160Hz) - bass harmonics
        0.8f,   // Band 3: Mid (160-320Hz) - lower vocals, snare body
        0.7f,   // Band 4: Upper-mid (320-640Hz) - vocals, instruments
        0.5f,   // Band 5: Presence (640-1280Hz) - clarity frequencies
        0.5f,   // Band 6: Brilliance (1280-2560Hz) - sibilance, hi-hats
        0.5f    // Band 7: Air (2560-5120Hz) - sparkle, treble transients
    };
    constexpr float PERCEPTUAL_BAND_WEIGHT_SUM =
        1.2f + 1.1f + 1.0f + 0.8f + 0.7f + 0.5f + 0.5f + 0.5f;  // 6.3f
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

AudioNode::AudioNode()
    : Node(NodeConfigs::Audio())
    , m_state(AudioNodeState::UNINITIALIZED)
    , m_stackMinFreeWords(AUDIO_ACTOR_STACK_WORDS)
{
    m_stats.reset();
    memset(m_hopBuffer, 0, sizeof(m_hopBuffer));
    m_pipelineTuning = clampAudioPipelineTuning(AudioPipelineTuning{});
    m_noiseFloor = m_pipelineTuning.noiseFloorMin;
}

AudioNode::~AudioNode()
{
    // Base class handles task cleanup
    // AudioCapture destructor handles I2S cleanup
}

// ============================================================================
// Control Methods
// ============================================================================

void AudioNode::pause()
{
    if (m_state == AudioNodeState::RUNNING) {
        LW_LOGI("Pausing audio capture");
        m_state = AudioNodeState::PAUSED;
    }
}

void AudioNode::resume()
{
    if (m_state == AudioNodeState::PAUSED) {
        LW_LOGI("Resuming audio capture");
        m_state = AudioNodeState::RUNNING;
    }
}

void AudioNode::resetStats()
{
    m_stats.reset();
    m_capture.resetStats();
}

AudioPipelineTuning AudioNode::getPipelineTuning() const
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

void AudioNode::setPipelineTuning(const AudioPipelineTuning& tuning)
{
    AudioPipelineTuning clamped = clampAudioPipelineTuning(tuning);
    uint32_t v = m_pipelineTuningSeq.load(std::memory_order_relaxed);
    m_pipelineTuningSeq.store(v + 1U, std::memory_order_release);
    m_pipelineTuning = clamped;
    m_pipelineTuningSeq.store(v + 2U, std::memory_order_release);
}

AudioContractTuning AudioNode::getContractTuning() const
{
    AudioContractTuning out;
    uint32_t v0;
    uint32_t v1;
    do {
        v0 = m_contractTuningSeq.load(std::memory_order_acquire);
        if (v0 & 1U) continue;
        out = m_contractTuning;
        v1 = m_contractTuningSeq.load(std::memory_order_acquire);
    } while (v0 != v1 || (v1 & 1U));
    return out;
}

void AudioNode::setContractTuning(const AudioContractTuning& tuning)
{
    AudioContractTuning clamped = clampAudioContractTuning(tuning);
    uint32_t v = m_contractTuningSeq.load(std::memory_order_relaxed);
    m_contractTuningSeq.store(v + 1U, std::memory_order_release);
    m_contractTuning = clamped;
    m_contractTuningSeq.store(v + 2U, std::memory_order_release);

    // TempoTracker: DISABLED - replaced by Emotiscope tempo detection
    // TempoTrackerTuning tt;
    // tt.minBpm = clamped.bpmMin;
    // tt.maxBpm = clamped.bpmMax;
    // tt.lockStrength = clamped.phaseCorrectionGain;
    // tt.confRise = 0.4f;
    // tt.confFall = 0.2f;
    // tt.onsetThreshK = 1.8f;
    // tt.refractoryMs = 100;
    // tt.baselineAlpha = 0.22f;
    // m_tempo.setTuning(tt);
}

void AudioNode::resetDspState()
{
    m_dspResetPending.store(true, std::memory_order_release);
}

AudioDspState AudioNode::getDspState() const
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

const int16_t* AudioNode::getLastHop() const
{
    if (m_state == AudioNodeState::RUNNING || m_state == AudioNodeState::PAUSED) {
        return m_hopBuffer;
    }
    return nullptr;
}

bool AudioNode::hasNewHop()
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

void AudioNode::onStart()
{
    LW_LOGI("AudioNode starting on Core %d", xPortGetCoreID());

    m_state = AudioNodeState::INITIALIZING;
    m_stats.state = m_state;

    // Initialize I2S audio capture
    if (!m_capture.init()) {
        LW_LOGE("Failed to initialize audio capture");
        m_state = AudioNodeState::ERROR;
        m_stats.state = m_state;
        return;
    }

    // K1 Dual-Bank Goertzel Front-End: DISABLED - replaced by Emotiscope pipeline
    // if (!m_k1FrontEnd.init()) {
    //     LW_LOGE("Failed to initialize K1 front-end");
    //     m_state = AudioNodeState::ERROR;
    //     m_stats.state = m_state;
    //     return;
    // }

    // Initialize Emotiscope Audio Pipeline
    m_emotiscope.init();
    LW_LOGI("Emotiscope audio pipeline initialized (64-bin Goertzel, 96-bin tempo)");

    m_state = AudioNodeState::RUNNING;
    m_stats.state = m_state;

    // TempoTracker: DISABLED - replaced by Emotiscope tempo detection
    // m_tempo.init();
    // m_lastTempoOutput = m_tempo.getOutput();

    LW_LOGI("AudioNode started (tick=%dms, hop=%d, rate=%.1fHz)",
             AUDIO_ACTOR_TICK_MS, HOP_SIZE, HOP_RATE_HZ);
}

void AudioNode::onMessage(const nodes::Message& msg)
{
    switch (msg.type) {
        case nodes::MessageType::SHUTDOWN:
            LW_LOGI("Received SHUTDOWN message");
            // Will be handled by base class
            break;

        case nodes::MessageType::HEALTH_CHECK:
            LW_LOGD("Health check: state=%d, captures=%lu",
                     static_cast<int>(m_state), m_stats.captureSuccessCount);
            // TODO: Send HEALTH_STATUS response when MessageBus is integrated
            break;

        case nodes::MessageType::PING:
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

void AudioNode::onTick()
{
    // Skip if not in running state
    if (m_state != AudioNodeState::RUNNING) {
        return;
    }

#ifndef NATIVE_BUILD
    UBaseType_t highWater = uxTaskGetStackHighWaterMark(nullptr);
    if (highWater < m_stackMinFreeWords) {
        m_stackMinFreeWords = highWater;
    }
#endif

    m_stats.tickCount++;

    // Capture one hop of audio
    captureHop();

    // Advance tempo phase every hop (audio-thread-owned) to avoid cross-core races with the renderer
    // TempoTracker advancePhase: DISABLED - replaced by Emotiscope tempo detection
    // float delta_sec = static_cast<float>(HOP_SIZE) / static_cast<float>(SAMPLE_RATE);
    // uint64_t t_samples_phase = m_sampleIndex;
    // m_tempo.advancePhase(delta_sec, t_samples_phase);
    // m_lastTempoOutput = m_tempo.getOutput();

    // Log periodically - gated by verbosity >= 2, time-based rate limiting (2 seconds minimum)
    auto& dbgCfg = getAudioDebugConfig();
    static uint32_t s_lastStatusLogMs = 0;
    uint32_t nowMs = millis();
    if (dbgCfg.verbosity >= 2 && (nowMs - s_lastStatusLogMs >= 2000)) {
        s_lastStatusLogMs = nowMs;
        const CaptureStats& cstats = m_capture.getStats();
        const ControlBusFrame& frame = m_controlBus.GetFrame();
        // Calculate mic level in dB from pre-gain RMS
        float micLevelDb = (m_lastRmsPreGain > 0.0001f) ? (20.0f * log10f(m_lastRmsPreGain)) : -80.0f;
        LW_LOGI("Audio alive: " LW_CLR_YELLOW "mic=%.1fdB" LW_ANSI_RESET " stk=%u cap=%lu pk=%d pkC=%d rms=%.4f->%.3f pre=%.4f g=%.2f dc=%.1f clip=%u flux=%.3f min=%d max=%d mean=%.1f",
                 micLevelDb, (unsigned)m_stackMinFreeWords, cstats.hopsCapured, cstats.peakSample, m_lastPeakCentered, m_lastRmsRaw, frame.rms,
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

        // Log Emotiscope beat tracking metrics
        {
            const auto& emoOut = m_emotiscope.getOutput();
            uint8_t topIdx = emoOut.top_bpm_index;
            float bpm = emotiscope::TEMPO_LOW + topIdx;
            float phase01 = (emoOut.tempi_phase[topIdx] + M_PI) / (2.0f * M_PI);
            float beatStrength = (emoOut.tempi_beat[topIdx] + 1.0f) / 2.0f;
            bool locked = (emoOut.tempo_confidence > 0.25f);
            LW_LOGI(LW_CLR_MAGENTA "Beat:" LW_ANSI_RESET " BPM=%.1f conf=%.2f phase=%.2f beat=%.2f lock=%s",
                     bpm, emoOut.tempo_confidence, phase01, beatStrength, locked ? "YES" : "no");
        }
    }
}

void AudioNode::onStop()
{
    LW_LOGI("AudioNode stopping");

    // Deinitialize audio capture
    m_capture.deinit();

    m_state = AudioNodeState::UNINITIALIZED;
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

void AudioNode::captureHop()
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

void AudioNode::processHop()
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
        // TempoTracker reset: DISABLED - replaced by Emotiscope
        // m_tempo.init();
        // m_lastTempoOutput = m_tempo.getOutput();
    }

    // 1. Build AudioTime for this hop
    // Use sample counter as single timebase (deterministic, native-safe)
    uint64_t now_us = (m_sampleIndex * 1000000ULL) / SAMPLE_RATE;
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

    const bool agcEnabled = tuning.agcEnabled;
    const float fixedGain = 4.0f;  // Fixed amplification (already applied in AudioCapture)

    // === Phase: DC/AGC Loop ===
    BENCH_START_PHASE();

    // ========================================================================
    // Phase 2 Integration: Populate Ring Buffer for Dual-Bank Processing
    // ========================================================================
    // Convert int16_t samples to float and push into ring buffer
    // This feeds both RhythmBank (24 bins) and HarmonyBank (64 bins)
    for (size_t i = 0; i < HOP_SIZE; ++i) {
        // Normalize int16_t → float [-1.0, 1.0]
        float sample = static_cast<float>(m_hopBuffer[i]) / 32768.0f;
        m_ringBuffer.push(sample);
        m_hopBufferFloat[i] = sample;  // Also store for Emotiscope
    }

    // ========================================================================
    // Emotiscope Audio Processing (64-bin Goertzel, VU, Chroma, Tempo)
    // ========================================================================
    m_emotiscope.process(m_hopBufferFloat, HOP_SIZE);

    // Emotiscope GPU-side updates
    // updateNovelty() must run at NOVELTY_LOG_HZ (50Hz), not audio rate (200Hz)
    // At 200Hz, spectral flux values would be ~4x too small
    // HOP_RATE = SAMPLE_RATE/HOP_SIZE = 12800/64 = 200Hz
    // Decimation = 200Hz / 50Hz = 4
    constexpr uint8_t NOVELTY_DECIMATION = 4;
    if ((m_hopCount % NOVELTY_DECIMATION) == 0) {
        m_emotiscope.updateNovelty();  // Populates novelty_curve for tempo detection
    }

    // Tempo phase advance - runs every hop for smooth phase tracking
    // delta = hop duration in reference frames
    // HOP_SIZE=64, SAMPLE_RATE=12800 -> 5ms per hop
    // REFERENCE_FPS=120 -> 1 frame = 8.33ms
    // delta = 5ms / 8.33ms = 0.6 reference frames per hop
    constexpr float HOP_DURATION_SEC = static_cast<float>(HOP_SIZE) / static_cast<float>(SAMPLE_RATE);
    constexpr float DELTA_FRAMES = HOP_DURATION_SEC * emotiscope::REFERENCE_FPS;
    m_emotiscope.updateTempiPhase(DELTA_FRAMES);

    // K1 Front-End: DISABLED - replaced by Emotiscope pipeline
    // k1::AudioFeatureFrame k1Frame;
    // if (m_k1FrontEnd.isInitialized()) {
    //     k1::AudioChunk chunk;
    //     memcpy(chunk.samples, m_hopBuffer, HOP_SIZE * sizeof(int16_t));
    //     chunk.n = HOP_SIZE;
    //     chunk.sample_counter_end = m_sampleIndex;
    //
    //     bool is_clipping = (maxRaw > 30000 || minRaw < -30000);
    //     k1Frame = m_k1FrontEnd.processHop(chunk, is_clipping);
    //     m_featureBus.publish(k1Frame);
    // }
    k1::AudioFeatureFrame k1Frame;  // Keep empty frame for compatibility

    int32_t minC = 32767;
    int32_t maxC = -32768;
    int32_t peakC = 0;
    uint16_t clipCount = 0;

    int64_t sumSqPre = 0;
    
    if (agcEnabled) {
        // Current AGC path (backward compatible)
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
    } else {
        // Fixed gain path: no DC removal, no AGC (4x gain already applied in AudioCapture)
        // Use same audio for both visual effects and beat tracking
        for (size_t i = 0; i < HOP_SIZE; ++i) {
            // AudioCapture already applied 4x gain, so use samples directly
            int32_t c = (int32_t)m_hopBuffer[i];
            if (c < -32768) c = -32768;
            if (c > 32767) c = 32767;

            m_hopBufferCentered[i] = (int16_t)c;
            
            sumSqPre += (int64_t)c * (int64_t)c;
            if (c < minC) minC = c;
            if (c > maxC) maxC = c;
            int32_t a = (c < 0) ? -c : c;
            if (a > peakC) peakC = a;
        }
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

    float activity = 1.0f;  // Default activity when AGC disabled

    if (agcEnabled) {
        // Noise floor gating and AGC gain calculation (only when AGC enabled)
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
        activity = clamp01((rmsPre - gateStart) / gateRange);

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
    } else {
        // AGC disabled: use fixed gain (already applied in AudioCapture)
        // No noise floor tracking or gain adjustment needed
        // Note: 4x gain is already applied in AudioCapture, so AGC gain is 1.0 (unity)
        m_lastAgcGain = 1.0f;
    }

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
    int16_t* window512 = m_window512;
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
        LW_LOGW("AudioNode stack low! High water mark: %u words (%.1f KB remaining)",
                stackHighWater, stackHighWater * 4.0f / 1024.0f);
    }
#endif

    // 5. Build ControlBusRawInput
    // Reuse member to save stack (cleared to avoid stale triggers)
    std::memset(&m_rawInput, 0, sizeof(ControlBusRawInput));
    ControlBusRawInput& raw = m_rawInput;
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
    bool goertzelTriggered = false;
    float bandsPre[NUM_BANDS] = {0}; // Pre-AGC bands for beat tracking.

    {
        TRACE_SCOPE("goertzel_analyze");

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

            // Populate bandsPre for beat tracking (Inverse AGC)
            // Note: m_agcGain is current, window512 is mostly current.
            float invGain = 1.0f / (m_agcGain + 1e-6f);
            for (int i = 0; i < NUM_BANDS; ++i) {
                // Reconstruct pre-AGC magnitude: post / gain
                float bandLinearPre = bandsRaw[i] * invGain;
                // Map to dB/0..1 using same tuning
                float bandMappedPre = mapLevelDb(bandLinearPre, tuning.bandDbFloor, tuning.bandDbCeil);
                // Apply per-band gains for consistency
                bandMappedPre *= tuning.perBandGains[i];
                if (bandMappedPre > 1.0f) bandMappedPre = 1.0f;
                // Apply noise floor gating if enabled
                if (tuning.usePerBandNoiseFloor && bandMappedPre < tuning.perBandNoiseFloors[i]) {
                    bandMappedPre = 0.0f;
                }
                // Apply activity gate (matches raw.bands behavior but uses pre-AGC values)
                bandsPre[i] = bandMappedPre * activity;
            }

            // === Phase 2: 64-bin FFT Analysis ===
            // Call analyze64() to populate raw.bins64[] with semitone-spaced frequency data
            // (55 Hz - 2093 Hz, A1 - C7, 5.25 octaves)
            if (m_analyzer.analyze64(m_bins64Raw)) {
                // Fresh 64-bin data available - apply same processing as 8-band
                for (int i = 0; i < 64; ++i) {
                    float binVal = mapLevelDb(m_bins64Raw[i], tuning.bandDbFloor, tuning.bandDbCeil);

                    // Apply activity gate (consistency with 8-band processing)
                    binVal *= activity;

                    // Clamp to [0, 1]
                    if (binVal > 1.0f) binVal = 1.0f;

                    // Cache for next hop when analysis isn't ready
                    m_bins64Cached[i] = binVal;
                    raw.bins64[i] = binVal;
                }
            } else {
                // No new 64-bin analysis this hop - reuse cached values
                // (prevents "picket fence" dropouts in FFT data)
                for (int i = 0; i < 64; ++i) {
                    raw.bins64[i] = m_bins64Cached[i] * activity;
                }
            }

            // Throttle 8-band Goertzel debug logging - gated by verbosity >= 5
            // Use time-based rate limiting (1 second minimum) to prevent serial spam
            auto& dbgCfg8 = getAudioDebugConfig();
            static uint32_t s_last8BandLogMs = 0;
            uint32_t nowMs = millis();
            if (dbgCfg8.verbosity >= 5 && (nowMs - s_last8BandLogMs >= 1000)) {
                s_last8BandLogMs = nowMs;
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
    }

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
    // Phase 2 Integration: Use K1 Front-End Output
    // ========================================================================
    // Convert K1's AudioFeatureFrame to AudioNode's AudioFeatureFrame
    // K1 produces rhythm_novelty (~2.5) which is ~32x stronger than old flux (~0.08)
    m_latestFrame.rhythmFlux = k1Frame.rhythm_novelty;
    m_latestFrame.harmonyFlux = k1Frame.harmony_valid ? k1Frame.chroma_stability : 0.0f;
    memcpy(m_latestFrame.chroma, k1Frame.chroma12, 12 * sizeof(float));
    m_latestFrame.chromaStability = k1Frame.chroma_stability;
    m_latestFrame.timestamp = m_sampleIndex;

    // ========================================================================
    // TempoTracker Beat Tracker Processing (Phase 2 Integration)
    // ========================================================================
    // Pass unified onset strength to tempo tracker (70% rhythm + 30% harmony)
    float onsetStrength = m_latestFrame.getOnsetStrength();

    // TempoTracker: DISABLED - replaced by Emotiscope tempo detection
    // m_tempo.updateNovelty(onsetStrength, m_sampleIndex);
    // float delta_sec = static_cast<float>(HOP_SIZE) / static_cast<float>(SAMPLE_RATE);
    // m_tempo.updateTempo(m_latestFrame, m_sampleIndex);
    // m_lastTempoOutput = m_tempo.getOutput();



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
    bool chromaTriggered = false;
    {
        TRACE_SCOPE("chroma_analyze");

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
    }

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

    // ========================================================================
    // Emotiscope Integration: Override raw inputs with Emotiscope outputs
    // ========================================================================
    {
        const auto& emoOut = m_emotiscope.getOutput();

        // Copy 64-bin spectrogram directly (Emotiscope's native output)
        for (int i = 0; i < 64; ++i) {
            raw.bins64[i] = emoOut.spectrogram[i];
        }

        // Aggregate 64 bins into 8 bands (matching plan's band mapping)
        // Band 0: bins 0-3   (sub-bass, A1-C2)
        // Band 1: bins 4-11  (bass, C#2-G2)
        // Band 2: bins 12-19 (low-mid, G#2-D#3)
        // Band 3: bins 20-31 (mid, E3-B3)
        // Band 4: bins 32-43 (high-mid, C4-G4)
        // Band 5: bins 44-51 (presence, G#4-D#5)
        // Band 6: bins 52-59 (brilliance, E5-B5)
        // Band 7: bins 60-63 (air, C6-C7)
        raw.bands[0] = (emoOut.spectrogram[0] + emoOut.spectrogram[1] +
                        emoOut.spectrogram[2] + emoOut.spectrogram[3]) / 4.0f;
        float band1 = 0.0f;
        for (int i = 4; i <= 11; ++i) band1 += emoOut.spectrogram[i];
        raw.bands[1] = band1 / 8.0f;
        float band2 = 0.0f;
        for (int i = 12; i <= 19; ++i) band2 += emoOut.spectrogram[i];
        raw.bands[2] = band2 / 8.0f;
        float band3 = 0.0f;
        for (int i = 20; i <= 31; ++i) band3 += emoOut.spectrogram[i];
        raw.bands[3] = band3 / 12.0f;
        float band4 = 0.0f;
        for (int i = 32; i <= 43; ++i) band4 += emoOut.spectrogram[i];
        raw.bands[4] = band4 / 12.0f;
        float band5 = 0.0f;
        for (int i = 44; i <= 51; ++i) band5 += emoOut.spectrogram[i];
        raw.bands[5] = band5 / 8.0f;
        float band6 = 0.0f;
        for (int i = 52; i <= 59; ++i) band6 += emoOut.spectrogram[i];
        raw.bands[6] = band6 / 8.0f;
        raw.bands[7] = (emoOut.spectrogram[60] + emoOut.spectrogram[61] +
                        emoOut.spectrogram[62] + emoOut.spectrogram[63]) / 4.0f;

        // Use Emotiscope's VU level for RMS
        raw.rms = emoOut.vu_level;

        // Use Emotiscope's novelty for flux
        raw.flux = std::min(1.0f, emoOut.current_novelty * 10.0f);  // Scale to 0-1 range

        // Copy chromagram
        for (int i = 0; i < 12; ++i) {
            raw.chroma[i] = emoOut.chromagram[i];
        }

        // Log Emotiscope outputs periodically (verbosity >= 3)
        auto& dbgCfgEmo = getAudioDebugConfig();
        static uint32_t s_lastEmoLogMs = 0;
        uint32_t nowMs = millis();
        if (dbgCfgEmo.verbosity >= 3 && (nowMs - s_lastEmoLogMs >= 2000)) {
            s_lastEmoLogMs = nowMs;
            float topBpm = emotiscope::TEMPO_LOW + emoOut.top_bpm_index;
            LW_LOGI(LW_CLR_GREEN "Emotiscope:" LW_ANSI_RESET " vu=%.3f novelty=%.4f silence=%s "
                     "BPM=%.1f(idx=%u) conf=%.2f bands=[%.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f]",
                     emoOut.vu_level, emoOut.current_novelty,
                     emoOut.silence_detected ? "YES" : "no",
                     topBpm, emoOut.top_bpm_index, emoOut.tempo_confidence,
                     raw.bands[0], raw.bands[1], raw.bands[2], raw.bands[3],
                     raw.bands[4], raw.bands[5], raw.bands[6], raw.bands[7]);
        }
    }

    // === Phase: ControlBus Update ===
    BENCH_START_PHASE();

    // 7a. Populate beat tracker state using EMOTISCOPE tempo (replaces TempoTracker)
    // Emotiscope uses Goertzel on the novelty curve for robust tempo detection
    {
        const auto& emoOut = m_emotiscope.getOutput();
        uint8_t topIdx = emoOut.top_bpm_index;

        // BPM: 48 + index (range 48-143 BPM)
        raw.tempo.bpm = emotiscope::TEMPO_LOW + topIdx;

        // Phase: convert from radians [-PI, PI] to [0, 1)
        float phaseRad = emoOut.tempi_phase[topIdx];
        raw.tempo.phase01 = (phaseRad + M_PI) / (2.0f * M_PI);

        // Confidence from Emotiscope's tempo analysis
        raw.tempo.confidence = emoOut.tempo_confidence;

        // Locked when confidence exceeds threshold
        raw.tempo.locked = (emoOut.tempo_confidence > 0.25f);

        // Beat strength: convert from [-1, 1] to [0, 1]
        float beatSignal = emoOut.tempi_beat[topIdx];
        raw.tempo.beat_strength = (beatSignal + 1.0f) / 2.0f;

        // Beat tick: rising edge detection (beat crosses 0.8 threshold from below)
        const float BEAT_THRESHOLD = 0.8f;
        bool beatHigh = (raw.tempo.beat_strength > BEAT_THRESHOLD);
        bool prevBeatHigh = (m_prevEmoBeat > BEAT_THRESHOLD);
        raw.tempo.beat_tick = beatHigh && !prevBeatHigh && raw.tempo.locked;
        m_prevEmoBeat = raw.tempo.beat_strength;
    }

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
        // Use Emotiscope beat tracker confidence for style detection
        float beatConfidence = raw.tempo.locked ? raw.tempo.confidence : 0.0f;
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
        // Reuse member to save stack
        m_frameToPublish = m_controlBus.GetFrame();
#if FEATURE_STYLE_DETECTION
        m_frameToPublish.currentStyle = m_styleDetector.getStyle();
        m_frameToPublish.styleConfidence = m_styleDetector.getConfidence();
#else
        m_frameToPublish.currentStyle = MusicStyle::UNKNOWN;
        m_frameToPublish.styleConfidence = 0.0f;
#endif
        m_controlBusBuffer.Publish(m_frameToPublish);
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

float AudioNode::computeRMS(const int16_t* samples, size_t count)
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

void AudioNode::handleCaptureError(CaptureResult result)
{
    // Log error based on type
    switch (result) {
        case CaptureResult::NOT_INITIALIZED:
            LW_LOGE("Capture error: not initialized");
            m_state = AudioNodeState::ERROR;
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
void AudioNode::aggregateBenchmarkStats()
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

bool AudioNode::startNoiseCalibration(uint32_t durationMs, float safetyMultiplier)
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

void AudioNode::cancelNoiseCalibration()
{
    if (m_noiseCalibration.state != CalibrationState::IDLE) {
        LW_LOGI("Calibration cancelled");
        m_noiseCalibration.reset();
    }
}

bool AudioNode::applyCalibrationResults()
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

void AudioNode::processNoiseCalibration(float rms, const float* bands, const float* chroma, uint32_t nowMs)
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
