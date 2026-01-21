/**
 * @file ActorSystem.cpp
 * @brief Implementation of the ActorSystem orchestrator
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "ActorSystem.h"
#include <math.h>
#include <cstdio>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

static const char* TAG = "ActorSystem";
#endif

namespace lightwaveos {
namespace actors {

// ============================================================================
// Singleton Instance
// ============================================================================

ActorSystem& ActorSystem::instance()
{
    static ActorSystem instance;
    return instance;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

ActorSystem::ActorSystem()
    : m_state(SystemState::UNINITIALIZED)
    , m_startTime(0)
{
}

ActorSystem::~ActorSystem()
{
    if (m_state == SystemState::RUNNING) {
        shutdown();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

bool ActorSystem::init()
{
    if (m_state != SystemState::UNINITIALIZED) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "Already initialized (state=%d)", static_cast<int>(m_state));
#endif
        return false;
    }

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "Initializing Actor System...");
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
#endif

    m_state = SystemState::STARTING;

    // Create actors in dependency order
    // 1. RendererActor (must be first - other actors may depend on it)
    // 2. ShowDirectorActor (depends on RendererActor)
    // Future: StateStoreActor, NetworkActor, HmiActor, etc.

    try {
        // Create RendererActor
        m_renderer = std::make_unique<RendererActor>();

        if (m_renderer == nullptr) {
#ifndef NATIVE_BUILD
            ESP_LOGE(TAG, "Failed to create RendererActor");
#endif
            m_state = SystemState::UNINITIALIZED;
            return false;
        }

        // Create ShowDirectorActor
        m_showDirector = std::make_unique<ShowDirectorActor>();

        if (m_showDirector == nullptr) {
#ifndef NATIVE_BUILD
            ESP_LOGE(TAG, "Failed to create ShowDirectorActor");
#endif
            m_state = SystemState::UNINITIALIZED;
            return false;
        }

#if FEATURE_AUDIO_SYNC
        // Create AudioActor (Phase 2)
        m_audio = std::make_unique<lightwaveos::audio::AudioActor>();

        if (m_audio == nullptr) {
#ifndef NATIVE_BUILD
            ESP_LOGE(TAG, "Failed to create AudioActor");
#endif
            m_state = SystemState::UNINITIALIZED;
            return false;
        }
#ifndef NATIVE_BUILD
        ESP_LOGI(TAG, "AudioActor created (Phase 2 audio sync enabled)");
#endif
#endif

#ifndef NATIVE_BUILD
        ESP_LOGI(TAG, "Actors created successfully");
        ESP_LOGI(TAG, "Free heap after init: %lu bytes", esp_get_free_heap_size());
#endif

    } catch (...) {
#ifndef NATIVE_BUILD
        ESP_LOGE(TAG, "Exception during actor creation");
#endif
        m_state = SystemState::UNINITIALIZED;
        return false;
    }

    return true;
}

bool ActorSystem::start()
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
    ESP_LOGI(TAG, "Starting actors...");
#endif

    // Start actors in dependency order
    // 1. StateStoreActor (load config) - future
    // 2. RendererActor (init LEDs)
    // 3. ShowDirectorActor (depends on RendererActor)
    // 4. NetworkActor (start server) - future
    // 5. HmiActor (start encoder) - future
    // 6. PluginManagerActor - future
    // 7. SyncManagerActor - future

    // Start RendererActor
    if (m_renderer && !m_renderer->start()) {
#ifndef NATIVE_BUILD
        ESP_LOGE(TAG, "Failed to start RendererActor");
#endif
        m_state = SystemState::STOPPED;
        return false;
    }

    // Start ShowDirectorActor
    if (m_showDirector && !m_showDirector->start()) {
#ifndef NATIVE_BUILD
        ESP_LOGE(TAG, "Failed to start ShowDirectorActor");
#endif
        m_state = SystemState::STOPPED;
        return false;
    }

#if FEATURE_AUDIO_SYNC
    // Start AudioActor (Phase 2)
    if (m_audio && !m_audio->start()) {
#ifndef NATIVE_BUILD
        ESP_LOGE(TAG, "Failed to start AudioActor");
#endif
        // Audio failure is non-fatal - continue without audio
        ESP_LOGW(TAG, "Continuing without audio sync");
    } else if (m_audio) {
        // Wire up audio buffer to renderer for cross-core access
        if (m_renderer) {
            m_renderer->setAudioBuffer(&m_audio->getControlBusBuffer());
            // Wire up TempoTracker for phase advancement at 120 FPS
            m_renderer->setTempo(&m_audio->getTempoMut());
#ifndef NATIVE_BUILD
            ESP_LOGI(TAG, "Audio integration enabled - ControlBus + TempoTracker");
#endif
        }
    }
#endif

    m_startTime = millis();
    m_state = SystemState::RUNNING;

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "All actors started successfully");
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
#endif

    return true;
}

void ActorSystem::shutdown()
{
    if (m_state != SystemState::RUNNING) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "Not running - nothing to shutdown");
#endif
        return;
    }

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "Shutting down actors...");
#endif

    m_state = SystemState::STOPPING;

    // Stop actors in reverse order
    // Future: SyncManagerActor, PluginManagerActor, HmiActor, NetworkActor, etc.

#if FEATURE_AUDIO_SYNC
    // Stop AudioActor (Phase 2) - must stop before renderer
    if (m_audio) {
        // Disconnect cross-actor wiring first
        if (m_renderer) {
            m_renderer->setAudioBuffer(nullptr);
        }
        m_audio->stop();
#ifndef NATIVE_BUILD
        ESP_LOGI(TAG, "AudioActor stopped");
#endif
    }
#endif

    // Stop ShowDirectorActor
    if (m_showDirector) {
        m_showDirector->stop();
    }

    // Stop RendererActor
    if (m_renderer) {
        m_renderer->stop();
    }

    m_state = SystemState::STOPPED;

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "All actors stopped");
    ESP_LOGI(TAG, "Final heap: %lu bytes", esp_get_free_heap_size());
#endif
}

// ============================================================================
// Convenience Commands
// ============================================================================

bool ActorSystem::setEffect(uint8_t effectId)
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

bool ActorSystem::setBrightness(uint8_t brightness)
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

bool ActorSystem::setSpeed(uint8_t speed)
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

bool ActorSystem::setPalette(uint8_t paletteIndex)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        // #region agent log
        {
            FILE* f = fopen("/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.cursor/debug.log", "a");
            if (f) {
                fprintf(f,
                        "{\"sessionId\":\"debug-session\",\"runId\":\"palette-loop-1\",\"hypothesisId\":\"H2\","
                        "\"location\":\"ActorSystem.cpp:setPalette\",\"message\":\"setPalette rejected (renderer not running)\","
                        "\"data\":{\"paletteIndex\":%u,\"rendererReady\":false},\"timestamp\":%lu}\n",
                        static_cast<unsigned>(paletteIndex),
                        static_cast<unsigned long>(millis()));
                fclose(f);
            }
        }
        // #endregion
        return false;
    }

    Message msg(MessageType::SET_PALETTE, paletteIndex);
    bool success = m_renderer->send(msg, pdMS_TO_TICKS(10));
    // #region agent log
    {
        FILE* f = fopen("/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.cursor/debug.log", "a");
        if (f) {
            fprintf(f,
                    "{\"sessionId\":\"debug-session\",\"runId\":\"palette-loop-1\",\"hypothesisId\":\"H2\","
                    "\"location\":\"ActorSystem.cpp:setPalette\",\"message\":\"setPalette dispatched\","
                    "\"data\":{\"paletteIndex\":%u,\"success\":%s,\"queueUtil\":%u},\"timestamp\":%lu}\n",
                    static_cast<unsigned>(paletteIndex),
                    success ? "true" : "false",
                    static_cast<unsigned>(m_renderer->getQueueUtilization()),
                    static_cast<unsigned long>(millis()));
            fclose(f);
        }
    }
    // #endregion
    return success;
}

bool ActorSystem::setIntensity(uint8_t intensity)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    Message msg(MessageType::SET_INTENSITY, intensity);
    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool ActorSystem::setSaturation(uint8_t saturation)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    Message msg(MessageType::SET_SATURATION, saturation);
    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool ActorSystem::setComplexity(uint8_t complexity)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    Message msg(MessageType::SET_COMPLEXITY, complexity);
    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool ActorSystem::setVariation(uint8_t variation)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    Message msg(MessageType::SET_VARIATION, variation);
    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool ActorSystem::setHue(uint8_t hue)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    Message msg(MessageType::SET_HUE, hue);
    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool ActorSystem::setMood(uint8_t mood)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    Message msg(MessageType::SET_MOOD, mood);
    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool ActorSystem::setFadeAmount(uint8_t fadeAmount)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    Message msg(MessageType::SET_FADE_AMOUNT, fadeAmount);
    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

#if FEATURE_AUDIO_SYNC
// ============================================================================
// Trinity Sync Commands (Offline ML Analysis)
// ============================================================================

bool ActorSystem::trinityBeat(float bpm, float phase01, bool tick, bool downbeat, int beatInBar)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    // Queue backpressure check
    uint8_t utilization = m_renderer->getQueueUtilization();
    if (utilization >= 90) {
        return false;
    }

    // Pack BPM as uint16_t (bpm * 100, clamped to 0-65535)
    uint16_t bpmFixed = (uint16_t)(fmaxf(0.0f, fminf(655.35f, bpm)) * 100.0f);
    
    // Pack phase as uint8_t (phase * 255)
    uint8_t phaseFixed = (uint8_t)(fmaxf(0.0f, fminf(1.0f, phase01)) * 255.0f);
    
    // Pack flags: tick (bit 0), downbeat (bit 1), beatInBar (bits 2-3)
    uint32_t flags = 0;
    if (tick) flags |= 0x01;
    if (downbeat) flags |= 0x02;
    flags |= ((beatInBar & 0x03) << 2);

    Message msg(MessageType::TRINITY_BEAT);
    msg.param1 = (bpmFixed >> 8) & 0xFF;  // BPM high byte
    msg.param2 = bpmFixed & 0xFF;          // BPM low byte
    msg.param3 = phaseFixed;
    msg.param4 = flags;

    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool ActorSystem::trinityMacro(float energy, float vocal, float bass, float perc, float bright)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    // Queue backpressure check
    uint8_t utilization = m_renderer->getQueueUtilization();
    if (utilization >= 90) {
        return false;
    }

    // Pack each macro as uint8_t (value * 255, clamped to [0,1])
    uint8_t e = (uint8_t)(fmaxf(0.0f, fminf(1.0f, energy)) * 255.0f);
    uint8_t v = (uint8_t)(fmaxf(0.0f, fminf(1.0f, vocal)) * 255.0f);
    uint8_t b = (uint8_t)(fmaxf(0.0f, fminf(1.0f, bass)) * 255.0f);
    uint8_t p = (uint8_t)(fmaxf(0.0f, fminf(1.0f, perc)) * 255.0f);
    uint8_t br = (uint8_t)(fmaxf(0.0f, fminf(1.0f, bright)) * 255.0f);

    Message msg(MessageType::TRINITY_MACRO);
    msg.param1 = e;   // energy
    msg.param2 = v;   // vocal
    msg.param3 = b;   // bass
    msg.param4 = (p << 24) | (br << 16);  // perc (high byte), bright (low byte)
    msg._reserved = 0;  // Reserved for future expansion

    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}

bool ActorSystem::trinitySync(uint8_t action, float positionSec, float bpm)
{
    if (!m_renderer || !m_renderer->isRunning()) {
        return false;
    }

    // Queue backpressure check
    uint8_t utilization = m_renderer->getQueueUtilization();
    if (utilization >= 90) {
        return false;
    }

    // Pack BPM as uint16_t
    uint16_t bpmFixed = (uint16_t)(fmaxf(0.0f, fminf(655.35f, bpm)) * 100.0f);
    
    // Pack position as uint32_t milliseconds
    uint32_t positionMs = (uint32_t)(fmaxf(0.0f, positionSec) * 1000.0f);

    Message msg(MessageType::TRINITY_SYNC);
    msg.param1 = action & 0xFF;
    msg.param2 = (bpmFixed >> 8) & 0xFF;  // BPM high byte
    msg.param3 = bpmFixed & 0xFF;         // BPM low byte
    msg.param4 = positionMs;

    return m_renderer->send(msg, pdMS_TO_TICKS(10));
}
#endif

bool ActorSystem::startTransition(uint8_t effectId, uint8_t transitionType)
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

SystemStats ActorSystem::getStats() const
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

    // Count active actors
    stats.activeActors = 0;
    if (m_renderer && m_renderer->isRunning()) stats.activeActors++;
    if (m_showDirector && m_showDirector->isRunning()) stats.activeActors++;
#if FEATURE_AUDIO_SYNC
    if (m_audio && m_audio->isRunning()) stats.activeActors++;
#endif
    // Future: count other actors

    return stats;
}

void ActorSystem::printStatus()
{
#ifndef NATIVE_BUILD
    SystemStats stats = getStats();

    Serial.println(F("\n=== LightwaveOS v2 Actor System ==="));
    Serial.printf("State: %d\n", static_cast<int>(m_state));
    Serial.printf("Uptime: %lu ms\n", stats.uptimeMs);
    Serial.printf("Active actors: %d\n", stats.activeActors);
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

uint32_t ActorSystem::getUptimeMs() const
{
    if (m_state != SystemState::RUNNING) {
        return 0;
    }
    return millis() - m_startTime;
}

} // namespace actors
} // namespace lightwaveos
