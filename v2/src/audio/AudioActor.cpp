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
        ESP_LOGI(TAG, "Audio alive: cap=%lu pk=%d rms=%.2f",
                 cstats.hopsCapured, cstats.peakSample, frame.rms);
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
    // 1. Build AudioTime for this hop
    uint64_t now_us = esp_timer_get_time();
    AudioTime now(m_sampleIndex, SAMPLE_RATE, now_us);

    // Update monotonic counters
    m_sampleIndex += HOP_SIZE;
    m_hopCount++;

    // 2. Compute RMS energy
    float rms = computeRMS(m_hopBuffer, HOP_SIZE);

    // 3. Compute spectral flux (half-wave rectified RMS derivative)
    float flux = std::max(0.0f, rms - m_prevRMS);
    m_prevRMS = rms;

    // 4. Accumulate samples for Goertzel (512-sample window = 2 hops)
    m_analyzer.accumulate(m_hopBuffer, HOP_SIZE);

    // 5. Build ControlBusRawInput
    ControlBusRawInput raw;
    raw.rms = rms;
    raw.flux = flux;

    // Initialize bands to zero (will be filled if Goertzel window is ready)
    for (int i = 0; i < NUM_BANDS; ++i) {
        raw.bands[i] = 0.0f;
    }

    // 6. Get band energies when Goertzel window is full (every 2 hops)
    if (m_analyzer.analyze(raw.bands)) {
        // Fresh band data available - Goertzel completed a 512-sample window
        ESP_LOGD(TAG, "Goertzel: bands[0]=%.3f bands[1]=%.3f", raw.bands[0], raw.bands[1]);
    }

    // 7. Update ControlBus with attack/release smoothing
    m_controlBus.UpdateFromHop(now, raw);

    // 8. Publish frame to renderer via lock-free SnapshotBuffer
    m_controlBusBuffer.Publish(m_controlBus.GetFrame());
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

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
