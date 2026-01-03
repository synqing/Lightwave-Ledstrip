/**
 * @file NodeOrchestrator.cpp
 * @brief Implementation of the NodeOrchestrator orchestrator
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "NodeOrchestrator.h"
#include "../../effects/zones/ZoneComposer.h"
#if FEATURE_AUDIO_SYNC
#include "../../audio/tempo/EmotiscopeEngine.h"
#endif

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

static const char* TAG = "NodeOrchestrator";
#endif

namespace lightwaveos {
namespace nodes {

// ============================================================================
// Singleton Instance
// ============================================================================

NodeOrchestrator& NodeOrchestrator::instance()
{
    static NodeOrchestrator instance;
    return instance;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

NodeOrchestrator::NodeOrchestrator()
    : m_state(SystemState::UNINITIALIZED)
    , m_startTime(0)
{
}

NodeOrchestrator::~NodeOrchestrator()
{
    if (m_state == SystemState::RUNNING) {
        shutdown();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

bool NodeOrchestrator::init()
{
    if (m_state != SystemState::UNINITIALIZED) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "Already initialized (state=%d)", static_cast<int>(m_state));
#endif
        return false;
    }

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "Initializing Node Orchestrator...");
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
#endif

    m_state = SystemState::STARTING;

    // Create nodes in dependency order
    // 1. RendererNode (must be first - other nodes may depend on it)
    // 2. ShowNode (depends on RendererNode)
    // Future: StateStoreNode, NetworkNode, HmiNode, etc.

    try {
        // Create RendererNode
        m_renderer = std::make_unique<RendererNode>();

        if (m_renderer == nullptr) {
#ifndef NATIVE_BUILD
            ESP_LOGE(TAG, "Failed to create RendererNode");
#endif
            m_state = SystemState::UNINITIALIZED;
            return false;
        }

        // Create ShowNode
        m_showDirector = std::make_unique<ShowNode>();

        if (m_showDirector == nullptr) {
#ifndef NATIVE_BUILD
            ESP_LOGE(TAG, "Failed to create ShowNode");
#endif
            m_state = SystemState::UNINITIALIZED;
            return false;
        }

#if FEATURE_AUDIO_SYNC
        // Create AudioNode (Phase 2)
        m_audio = std::make_unique<audio::AudioNode>();

        if (m_audio == nullptr) {
#ifndef NATIVE_BUILD
            ESP_LOGE(TAG, "Failed to create AudioNode");
#endif
            m_state = SystemState::UNINITIALIZED;
            return false;
        }
#ifndef NATIVE_BUILD
        ESP_LOGI(TAG, "AudioNode created (Phase 2 audio sync enabled)");
#endif
#endif

#ifndef NATIVE_BUILD
        ESP_LOGI(TAG, "Nodes created successfully");
        ESP_LOGI(TAG, "Free heap after init: %lu bytes", esp_get_free_heap_size());
#endif

    } catch (...) {
#ifndef NATIVE_BUILD
        ESP_LOGE(TAG, "Exception during node creation");
#endif
        m_state = SystemState::UNINITIALIZED;
        return false;
    }

    return true;
}

bool NodeOrchestrator::start()
{
    if (m_state != SystemState::STARTING) {
        if (m_state == SystemState::UNINITIALIZED) {
            // Auto-init if not done
            if (!init()) {
                return false;
            }
        } else {
#ifndef NATIVE_BUILD
            ESP_LOGW(TAG, "Cannot start - wrong state: %d", static_cast<int>(m_state));
#endif
            return false;
        }
    }

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "Starting nodes...");
#endif

    // Start nodes in dependency order
    // 1. StateStoreNode (load config) - future
    // 2. RendererNode (init LEDs)
    // 3. ShowNode (depends on RendererNode)
    // 4. NetworkNode (start server) - future
    // 5. HmiNode (start encoder) - future
    // 6. PluginManagerNode - future
    // 7. SyncManagerNode - future

    // Start RendererNode
    if (m_renderer && !m_renderer->start()) {
#ifndef NATIVE_BUILD
        ESP_LOGE(TAG, "Failed to start RendererNode");
#endif
        m_state = SystemState::STOPPED;
        return false;
    }

    // Start ShowNode
    if (m_showDirector && !m_showDirector->start()) {
#ifndef NATIVE_BUILD
        ESP_LOGE(TAG, "Failed to start ShowNode");
#endif
        m_state = SystemState::STOPPED;
        return false;
    }

#if FEATURE_AUDIO_SYNC
    // Start AudioNode (Phase 2)
    if (m_audio && !m_audio->start()) {
#ifndef NATIVE_BUILD
        ESP_LOGE(TAG, "Failed to start AudioNode");
#endif
        // Audio failure is non-fatal - continue without audio
        ESP_LOGW(TAG, "Continuing without audio sync");
    } else if (m_audio) {
        // Wire up audio buffer to renderer for cross-core access
        if (m_renderer) {
            m_renderer->setAudioBuffer(&m_audio->getControlBusBuffer());
            // Wire up EmotiscopeEngine for phase advancement at 120 FPS
            m_renderer->setTempo(&m_audio->getEmotiscopeMut());

            // Phase 2b.1: Wire EmotiscopeEngine to ZoneComposer for per-zone tempo modulation
            zones::ZoneComposer* zoneComposer = m_renderer->getZoneComposer();
            if (zoneComposer) {
                zoneComposer->setEmotiscope(&m_audio->getEmotiscopeMut());
#ifndef NATIVE_BUILD
                ESP_LOGI(TAG, "Zone tempo modulation enabled (Phase 2b.1)");
#endif
            }

#ifndef NATIVE_BUILD
            ESP_LOGI(TAG, "Audio integration enabled - ControlBus + EmotiscopeEngine");
#endif
        }
    }
#endif

    m_startTime = millis();
    m_state = SystemState::RUNNING;

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "All nodes started successfully");
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
#endif

    return true;
}

void NodeOrchestrator::shutdown()
{
    if (m_state != SystemState::RUNNING) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "Not running - nothing to shutdown");
#endif
        return;
    }

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "Shutting down nodes...");
#endif

    m_state = SystemState::STOPPING;

    // Stop nodes in reverse order
    // Future: SyncManagerNode, PluginManagerNode, HmiNode, NetworkNode, etc.

#if FEATURE_AUDIO_SYNC
    // Stop AudioNode (Phase 2) - must stop before renderer
    if (m_audio) {
        // Disconnect cross-node wiring first
        if (m_renderer) {
            m_renderer->setAudioBuffer(nullptr);
        }
        m_audio->stop();
#ifndef NATIVE_BUILD
        ESP_LOGI(TAG, "AudioNode stopped");
#endif
    }
#endif

    // Stop ShowNode
    if (m_showDirector) {
        m_showDirector->stop();
    }

    // Stop RendererNode
    if (m_renderer) {
        m_renderer->stop();
    }

    m_state = SystemState::STOPPED;

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "All nodes stopped");
    ESP_LOGI(TAG, "Final heap: %lu bytes", esp_get_free_heap_size());
#endif
}

// ============================================================================
// Convenience Commands
// ============================================================================

bool NodeOrchestrator::setEffect(uint8_t effectId)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    // Queue backpressure: reject if queue > 90% full
    uint8_t utilization = m_renderer->getQueueUtilization();
    if (utilization >= 90) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "setEffect(%d) rejected - queue saturated (utilization: %d%%)",
                 effectId, utilization);
#endif
        return false;
    }

    Message msg(MessageType::SET_EFFECT, effectId);
    bool success = m_renderer->send(msg, pdMS_TO_TICKS(10));
    if (!success) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "setEffect(%d) failed - queue may be full (utilization: %d%%)",
                 effectId, m_renderer->getQueueUtilization());
#endif
    }
    return success;
}

bool NodeOrchestrator::setBrightness(uint8_t brightness)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    // Queue backpressure: reject if queue > 90% full
    uint8_t utilization = m_renderer->getQueueUtilization();
    if (utilization >= 90) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "setBrightness(%d) rejected - queue saturated (utilization: %d%%)",
                 brightness, utilization);
#endif
        return false;
    }

    Message msg(MessageType::SET_BRIGHTNESS, brightness);
    bool success = m_renderer->send(msg, pdMS_TO_TICKS(10));
    if (!success) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "setBrightness(%d) failed - queue may be full (utilization: %d%%)",
                 brightness, m_renderer->getQueueUtilization());
#endif
    }
    return success;
}

bool NodeOrchestrator::setSpeed(uint8_t speed)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    // Queue backpressure: reject if queue > 90% full
    uint8_t utilization = m_renderer->getQueueUtilization();
    if (utilization >= 90) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "setSpeed(%d) rejected - queue saturated (utilization: %d%%)",
                 speed, utilization);
#endif
        return false;
    }

    Message msg(MessageType::SET_SPEED, speed);
    bool success = m_renderer->send(msg, pdMS_TO_TICKS(10));
    if (!success) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "setSpeed(%d) failed - queue may be full (utilization: %d%%)",
                 speed, m_renderer->getQueueUtilization());
#endif
    }
    return success;
}

bool NodeOrchestrator::setPalette(uint8_t paletteIndex)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    Message msg(MessageType::SET_PALETTE, paletteIndex);
    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool NodeOrchestrator::setIntensity(uint8_t intensity)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    Message msg(MessageType::SET_INTENSITY, intensity);
    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool NodeOrchestrator::setSaturation(uint8_t saturation)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    Message msg(MessageType::SET_SATURATION, saturation);
    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool NodeOrchestrator::setComplexity(uint8_t complexity)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    Message msg(MessageType::SET_COMPLEXITY, complexity);
    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool NodeOrchestrator::setVariation(uint8_t variation)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    Message msg(MessageType::SET_VARIATION, variation);
    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool NodeOrchestrator::setHue(uint8_t hue)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    Message msg(MessageType::SET_HUE, hue);
    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool NodeOrchestrator::setMood(uint8_t mood)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    Message msg(MessageType::SET_MOOD, mood);
    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool NodeOrchestrator::setFadeAmount(uint8_t fadeAmount)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    Message msg(MessageType::SET_FADE_AMOUNT, fadeAmount);
    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool NodeOrchestrator::startTransition(uint8_t effectId, uint8_t transitionType)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    // Queue backpressure: reject if queue > 90% full
    uint8_t utilization = m_renderer->getQueueUtilization();
    if (utilization >= 90) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "startTransition(%d, %d) rejected - queue saturated (utilization: %d%%)",
                 effectId, transitionType, utilization);
#endif
        return false;
    }

    Message msg(MessageType::START_TRANSITION);
    msg.param1 = effectId;
    msg.param2 = transitionType;
    bool success = m_renderer->send(msg, pdMS_TO_TICKS(10));
    if (!success) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "startTransition(%d, %d) failed - queue may be full (utilization: %d%%)",
                 effectId, transitionType, m_renderer->getQueueUtilization());
#endif
    }
    return success;
}

// ============================================================================
// Diagnostics
// ============================================================================

SystemStats NodeOrchestrator::getStats() const
{
    SystemStats stats;

    stats.uptimeMs = getUptimeMs();

    // Get MessageBus stats
    stats.totalMessages = bus::MessageBus::instance().getTotalPublished();

#ifndef NATIVE_BUILD
    // Get heap stats
    stats.heapFreeBytes = esp_get_free_heap_size();
    stats.heapMinFreeBytes = esp_get_minimum_free_heap_size();
#endif

    // Count active nodes
    stats.activeNodes = 0;
    if (m_renderer && m_renderer->isRunning()) stats.activeNodes++;
    if (m_showDirector && m_showDirector->isRunning()) stats.activeNodes++;
#if FEATURE_AUDIO_SYNC
    if (m_audio && m_audio->isRunning()) stats.activeNodes++;
#endif
    // Future: count other nodes

    return stats;
}

void NodeOrchestrator::printStatus()
{
#ifndef NATIVE_BUILD
    SystemStats stats = getStats();

    Serial.println(F("\n=== LightwaveOS v2 Node System ==="));
    Serial.printf("State: %d\n", static_cast<int>(m_state));
    Serial.printf("Uptime: %lu ms\n", stats.uptimeMs);
    Serial.printf("Active nodes: %d\n", stats.activeNodes);
    Serial.printf("Total messages: %lu\n", stats.totalMessages);
    Serial.printf("Heap: %lu / min %lu bytes\n",
                  stats.heapFreeBytes, stats.heapMinFreeBytes);

    // Renderer stats
    if (m_renderer && m_renderer->isRunning()) {
        const RenderStats& rs = m_renderer->getStats();
        Serial.println(F("\n--- Renderer ---"));
        Serial.printf("Effect: %d (%s)\n",
                      m_renderer->getCurrentEffect(),
                      m_renderer->getEffectName(m_renderer->getCurrentEffect()));
        Serial.printf("Brightness: %d\n", m_renderer->getBrightness());
        Serial.printf("Speed: %d\n", m_renderer->getSpeed());
        Serial.printf("FPS: %d (target: %d)\n", rs.currentFPS, LedConfig::TARGET_FPS);
        Serial.printf("CPU: %d%%\n", rs.cpuPercent);
        Serial.printf("Frames: %lu, Drops: %lu\n", rs.framesRendered, rs.frameDrops);
        Serial.printf("Frame time: avg=%lu, min=%lu, max=%lu us\n",
                      rs.avgFrameTimeUs, rs.minFrameTimeUs, rs.maxFrameTimeUs);
        Serial.printf("Stack watermark: %d words\n",
                      m_renderer->getStackHighWaterMark());
    }

    // ShowDirector stats
    if (m_showDirector && m_showDirector->isRunning()) {
        Serial.println(F("\n--- ShowDirector ---"));
        Serial.printf("Has show: %s\n", m_showDirector->hasShow() ? "YES" : "NO");
        if (m_showDirector->hasShow()) {
            Serial.printf("Show ID: %d\n", m_showDirector->getCurrentShowId());
            Serial.printf("Playing: %s\n", m_showDirector->isPlaying() ? "YES" : "NO");
            Serial.printf("Paused: %s\n", m_showDirector->isPaused() ? "YES" : "NO");
            Serial.printf("Chapter: %d\n", m_showDirector->getCurrentChapter());
            Serial.printf("Progress: %.1f%%\n", m_showDirector->getProgress() * 100.0f);
            Serial.printf("Elapsed: %lu ms\n", m_showDirector->getElapsedMs());
            Serial.printf("Remaining: %lu ms\n", m_showDirector->getRemainingMs());
        }
    }

    // MessageBus stats
    Serial.println(F("\n--- MessageBus ---"));
    bus::MessageBus::instance().dumpSubscriptions();

    Serial.println(F("===================================\n"));
#endif
}

uint32_t NodeOrchestrator::getUptimeMs() const
{
    if (m_state != SystemState::RUNNING) {
        return 0;
    }
    return millis() - m_startTime;
}

} // namespace nodes
} // namespace lightwaveos

