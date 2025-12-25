/**
 * @file AudioActor.cpp
 * @brief Actor implementation for audio capture and two-rate processing
 *
 * TWO-RATE PIPELINE:
 * - Tick interval: 8ms (125 Hz)
 * - FAST LANE: Every tick captures 128 samples, publishes ControlBusFrame
 * - BEAT LANE: Every 2nd tick runs BeatTracker, publishes BeatObsFrame
 *
 * This provides:
 * - 125 Hz texture updates for responsive visuals
 * - 62.5 Hz beat observations for stable musical time (Tab5 parity)
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "AudioActor.h"

#if FEATURE_AUDIO_SYNC

#include <cstring>
#include <cmath>

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
    memset(m_hopBufferCentered, 0, sizeof(m_hopBufferCentered));
    memset(m_beatRingBuffer, 0, sizeof(m_beatRingBuffer));
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

    ESP_LOGI(TAG, "AudioActor started: tick=%dms, fast_hop=%d@%.1fHz, beat_hop=%d@%.1fHz",
             AUDIO_ACTOR_TICK_MS, HOP_FAST, HOP_FAST_HZ, HOP_BEAT, HOP_BEAT_HZ);
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
    m_tickCounter++;

    // Record tick start time
    uint64_t tickStart = esp_timer_get_time();

    // Capture one hop of audio (128 samples @ 8ms tick)
    captureHop();

    // Record tick time
    m_stats.lastTickTimeUs = esp_timer_get_time() - tickStart;

    // Log periodically (every 1250 ticks = ~10 seconds @ 125 Hz)
    if ((m_stats.tickCount % 1250) == 0) {
        const CaptureStats& cstats = m_capture.getStats();
        const ControlBusFrame& frame = m_controlBus.GetFrame();
        ESP_LOGI(TAG, "Audio alive: cap=%lu pk=%d pkC=%d rms=%.4f->%.3f pre=%.4f g=%.2f dc=%.1f clip=%u flux=%.3f bpm=%.1f beat=%d",
                 cstats.hopsCapured, cstats.peakSample, m_lastPeakCentered, m_lastRmsRaw, frame.rms,
                 m_lastRmsPreGain, m_lastAgcGain, m_lastDcEstimate, (unsigned)m_lastClipCount, m_lastFluxMapped,
                 m_beatTracker.getBPM(), m_beatTracker.isBeat() ? 1 : 0);
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
    auto clamp01 = [](float x) -> float {
        if (x < 0.0f) return 0.0f;
        if (x > 1.0f) return 1.0f;
        return x;
    };

    auto mapLevelDb = [&](float x, float dbFloor, float dbCeil) -> float {
        const float eps = 1e-6f;
        float db = 20.0f * log10f(x + eps);
        float t = (db - dbFloor) / (dbCeil - dbFloor);
        return clamp01(t);
    };

    // 1. Build AudioTime for this hop
    uint64_t now_us = esp_timer_get_time();
    AudioTime now(m_sampleIndex, SAMPLE_RATE, now_us);

    // Update monotonic counters (FAST LANE: 128 samples per tick)
    m_sampleIndex += HOP_FAST;
    m_hopCount++;

    int32_t minRaw = 32767;
    int32_t maxRaw = -32768;
    int64_t sumRaw = 0;
    for (size_t i = 0; i < HOP_FAST; ++i) {
        int32_t s = m_hopBuffer[i];
        if (s < minRaw) minRaw = s;
        if (s > maxRaw) maxRaw = s;
        sumRaw += s;
    }
    float meanRaw = (float)sumRaw / (float)HOP_FAST;
    m_lastMeanSample = meanRaw;

    constexpr float dcAlpha = 0.001f;
    // TARGET RMS: 0.25 (-12dB) is a strong signal for the visualizer.
    // Previous value 0.010 was way too low (-40dB).
    constexpr float agcTargetRms = 0.25f;
    constexpr float agcSilenceFloor = 0.00050f;
    constexpr float agcGateRange = 0.00050f;
    constexpr float agcMinGain = 1.0f;    // Don't attenuate below unity
    constexpr float agcMaxGain = 100.0f;  // Reduced from 300.0f - 300x is dangerously high
    constexpr float agcAttack = 0.08f;
    constexpr float agcRelease = 0.02f;

    int32_t minC = 32767;
    int32_t maxC = -32768;
    int32_t peakC = 0;
    uint16_t clipCount = 0;

    int64_t sumSqPre = 0;
    for (size_t i = 0; i < HOP_FAST; ++i) {
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
    if (HOP_FAST > 0) {
        float rmsPreAbs = std::sqrt(static_cast<float>(sumSqPre) / (float)HOP_FAST);
        rmsPre = std::min(1.0f, rmsPreAbs / 32768.0f);
    }
    m_lastRmsPreGain = rmsPre;

    if (clipCount > 0) {
        m_agcGain *= 0.90f;
    } else if (rmsPre <= agcSilenceFloor) {
        m_agcGain += 0.01f * (1.0f - m_agcGain);
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

    float activity = clamp01((rmsPre - agcSilenceFloor) / agcGateRange);
    float rmsRaw = computeRMS(m_hopBufferCentered, HOP_FAST);
    float rmsMapped = mapLevelDb(rmsRaw, -65.0f, -12.0f); // Ceiling -12dB matches target
    rmsMapped *= activity;
    m_lastRmsRaw = rmsRaw;
    m_lastRmsMapped = rmsMapped;

    // 3. Compute spectral flux (half-wave rectified RMS derivative)
    float fluxMapped = std::max(0.0f, rmsMapped - m_prevRMS);
    m_prevRMS = rmsMapped;
    m_lastFluxMapped = fluxMapped;

    // 4. Accumulate samples for Goertzel (512-sample window, 128-sample hops)
    m_analyzer.accumulate(m_hopBufferCentered, HOP_FAST);

    // 4.5. Accumulate samples for Chromagram (512-sample window, 128-sample hops)
    m_chromaAnalyzer.accumulate(m_hopBufferCentered, HOP_FAST);

    // 4.6. Accumulate samples into beat ring buffer for BEAT LANE
    for (size_t i = 0; i < HOP_FAST; ++i) {
        m_beatRingBuffer[m_beatRingWriteIndex] = m_hopBufferCentered[i];
        m_beatRingWriteIndex = (m_beatRingWriteIndex + 1) % HOP_BEAT;
    }

    // 5. Build ControlBusRawInput
    ControlBusRawInput raw;
    raw.rms = rmsMapped;
    raw.flux = fluxMapped;

    // 5.5. Waveform: copy HOP_FAST samples directly (now 128 samples = WAVEFORM_N)
    // Since HOP_FAST == CONTROLBUS_WAVEFORM_N (128), no downsampling needed
    constexpr uint8_t WAVEFORM_POINTS = audio::CONTROLBUS_WAVEFORM_N;
    static_assert(HOP_FAST == WAVEFORM_POINTS, "HOP_FAST must equal WAVEFORM_N for 1:1 mapping");
    for (uint8_t i = 0; i < WAVEFORM_POINTS; ++i) {
        if (activity < 1.0f) {
            raw.waveform[i] = static_cast<int16_t>(lroundf(m_hopBufferCentered[i] * activity));
        } else {
            raw.waveform[i] = m_hopBufferCentered[i];
        }
    }

    // 6. Get band energies from sliding Goertzel window (updates every tick now)
    float bandsRaw[NUM_BANDS] = {0};
    if (m_analyzer.analyze(bandsRaw)) {
        // Fresh band data available - sliding window always has data after startup
        for (int i = 0; i < NUM_BANDS; ++i) {
            float band = mapLevelDb(bandsRaw[i], -65.0f, -12.0f);
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
        // Window not ready yet (startup) - reuse last known bands
        for (int i = 0; i < NUM_BANDS; ++i) {
            raw.bands[i] = m_lastBands[i] * activity;
        }
    }

    // 6.5. Get chromagram from sliding ChromaAnalyzer window
    float chromaRaw[12] = {0};
    static float lastChroma[12] = {0};
    if (m_chromaAnalyzer.analyze(chromaRaw)) {
        // Fresh chroma data available
        for (int i = 0; i < 12; ++i) {
            float chroma = mapLevelDb(chromaRaw[i], -65.0f, -12.0f);
            lastChroma[i] = chroma;
            raw.chroma[i] = chroma * activity;
        }
    } else {
        // No chroma yet (startup) - reuse last known chroma
        for (int i = 0; i < 12; ++i) {
            raw.chroma[i] = lastChroma[i] * activity;
        }
    }

    // 7. Update ControlBus with attack/release smoothing
    m_controlBus.UpdateFromHop(now, raw);

    // 8. Publish frame to renderer via lock-free SnapshotBuffer (FAST LANE - 125 Hz)
    m_controlBusBuffer.Publish(m_controlBus.GetFrame());

    // 9. BEAT LANE: Process every 2nd tick (62.5 Hz)
    if ((m_tickCounter % 2) == 0) {
        processBeatLane(now);
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
// Beat Lane Processing (62.5 Hz)
// ============================================================================

void AudioActor::processBeatLane(const AudioTime& now)
{
    // Get current ControlBusFrame for band energies and RMS
    const ControlBusFrame& frame = m_controlBus.GetFrame();

    // Run beat tracker with current band energies and RMS
    m_beatTracker.process(now, frame.bands, frame.rms);

    // Build BeatObsFrame for MusicalGrid
    BeatObsFrame beatObs;
    beatObs.t_obs = now;
    beatObs.beat_pulse = m_beatTracker.isBeat();
    beatObs.beat_strength = m_beatTracker.getBeatStrength();
    beatObs.downbeat_pulse = false;  // TODO: downbeat detection in future
    beatObs.tempo_valid = m_beatTracker.hasValidTempo();
    beatObs.bpm_est = m_beatTracker.getBPM();
    beatObs.tempo_conf = m_beatTracker.getConfidence();
    beatObs.weighted_flux = m_beatTracker.getWeightedFlux();

    // Publish to renderer via lock-free SnapshotBuffer (BEAT LANE - 62.5 Hz)
    m_beatObsBuffer.Publish(beatObs);

    // Log beat events (throttled to prevent spam)
    if (beatObs.beat_pulse) {
        ESP_LOGD(TAG, "BEAT! bpm=%.1f conf=%.2f strength=%.2f flux=%.3f",
                 beatObs.bpm_est, beatObs.tempo_conf, beatObs.beat_strength, beatObs.weighted_flux);
    }
}

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
