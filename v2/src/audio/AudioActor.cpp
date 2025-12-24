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
#include <esp_log.h>

static const char* TAG = "AudioActor";

namespace lightwaveos {
namespace audio {

// ============================================================================
// Constructor / Destructor
// ============================================================================

AudioActor::AudioActor()
    : Actor(ActorConfigs::Audio())
    , m_state(AudioActorState::UNINITIALIZED)
    , m_newHopAvailable(false)
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

    ESP_LOGI(TAG, "AudioActor started successfully");
    ESP_LOGI(TAG, "  Tick interval: %d ms", AUDIO_ACTOR_TICK_MS);
    ESP_LOGI(TAG, "  Hop size: %d samples", HOP_SIZE);
    ESP_LOGI(TAG, "  Frame rate: %.1f Hz", HOP_RATE_HZ);
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
    uint32_t tickStart = esp_log_timestamp();

    // Capture one hop of audio
    captureHop();

    // Record tick time
    m_stats.lastTickTimeUs = (esp_log_timestamp() - tickStart) * 1000;

    // Log periodically (every 62 ticks = ~1 second)
    if ((m_stats.tickCount % 62) == 0) {
        const CaptureStats& cstats = m_capture.getStats();
        ESP_LOGD(TAG, "Audio tick: captures=%lu, fails=%lu, peak=%d",
                 cstats.hopsCapured, cstats.dmaTimeouts + cstats.readErrors,
                 cstats.peakSample);
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

        // Phase 2: Process the hop here
        // - Goertzel frequency analysis
        // - Band energy extraction
        // - Beat detection
        // - Publish to ControlBus

    } else {
        m_stats.captureFailCount++;
        handleCaptureError(result);
    }
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
