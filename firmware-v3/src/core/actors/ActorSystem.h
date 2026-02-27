// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ActorSystem.h
 * @brief Orchestrates all Actors in the LightwaveOS v2 system
 *
 * The ActorSystem is the top-level manager that:
 * - Creates and owns all Actor instances
 * - Starts/stops Actors in the correct order
 * - Provides access to Actors for external code
 * - Handles system-wide events (shutdown, etc.)
 *
 * Startup order:
 * 1. StateStoreActor - Load saved state
 * 2. RendererActor - Initialize LEDs
 * 3. NetworkActor - Start web server
 * 4. HmiActor - Start encoder polling
 * 5. PluginManagerActor - Load plugins
 * 6. SyncManagerActor - Connect to peers
 *
 * Shutdown order: Reverse of startup
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "Actor.h"
#include "RendererActor.h"
#include "ShowDirectorActor.h"
#include "../bus/MessageBus.h"
#include "../../config/features.h"
#include <memory>

// Audio integration (Phase 2)
#if FEATURE_AUDIO_SYNC
#include "../../audio/AudioActor.h"
#endif

namespace lightwaveos {
namespace actors {

// ============================================================================
// System State
// ============================================================================

/**
 * @brief Overall system state
 */
enum class SystemState : uint8_t {
    UNINITIALIZED = 0,  // Not yet started
    STARTING,           // Actors being created
    RUNNING,            // All actors running
    STOPPING,           // Shutdown in progress
    STOPPED             // All actors stopped
};

/**
 * @brief System-wide statistics
 */
struct SystemStats {
    uint32_t uptimeMs;              // Time since start
    uint32_t totalMessages;         // Total messages processed
    uint32_t heapFreeBytes;         // Current free heap
    uint32_t heapMinFreeBytes;      // Minimum free heap ever
    uint8_t activeActors;           // Number of running actors

    SystemStats()
        : uptimeMs(0), totalMessages(0)
        , heapFreeBytes(0), heapMinFreeBytes(0)
        , activeActors(0) {}
};

// ============================================================================
// ActorSystem Class
// ============================================================================

/**
 * @brief Top-level Actor orchestration
 *
 * Singleton class that manages the lifecycle of all Actors.
 *
 * Usage:
 *   ActorSystem::instance().init();
 *   ActorSystem::instance().start();
 *   // ... application running ...
 *   ActorSystem::instance().shutdown();
 */
class ActorSystem {
public:
    /**
     * @brief Get the singleton instance
     */
    static ActorSystem& instance();

    // Prevent copying
    ActorSystem(const ActorSystem&) = delete;
    ActorSystem& operator=(const ActorSystem&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    /**
     * @brief Initialize the system (create actors)
     *
     * Creates all Actor instances but does not start them.
     * Call this once during setup().
     *
     * @return true if all actors created successfully
     */
    bool init();

    /**
     * @brief Start all actors
     *
     * Starts actors in dependency order. Call after init().
     *
     * @return true if all actors started successfully
     */
    bool start();

    /**
     * @brief Shutdown all actors gracefully
     *
     * Stops actors in reverse order. Blocks until complete.
     */
    void shutdown();

    /**
     * @brief Get current system state
     */
    SystemState getState() const { return m_state; }

    /**
     * @brief Check if system is running
     */
    bool isRunning() const { return m_state == SystemState::RUNNING; }

    // ========================================================================
    // Actor Access
    // ========================================================================

    /**
     * @brief Get the RendererActor
     *
     * Returns nullptr if not initialized.
     */
    RendererActor* getRenderer() { return m_renderer.get(); }
    const RendererActor* getRenderer() const { return m_renderer.get(); }

    /**
     * @brief Get the ShowDirectorActor
     *
     * Returns nullptr if not initialized.
     */
    ShowDirectorActor* getShowDirector() { return m_showDirector.get(); }
    const ShowDirectorActor* getShowDirector() const { return m_showDirector.get(); }

#if FEATURE_AUDIO_SYNC
    /**
     * @brief Get the AudioActor (Phase 2)
     *
     * Returns nullptr if not initialized or FEATURE_AUDIO_SYNC disabled.
     */
    lightwaveos::audio::AudioActor* getAudio() { return m_audio.get(); }
    const lightwaveos::audio::AudioActor* getAudio() const { return m_audio.get(); }
#endif

    // Future: getNetwork(), getHmi(), getStateStore(), etc.

    // ========================================================================
    // Convenience Commands
    // ========================================================================

    /**
     * @brief Set the current effect
     *
     * Sends a SET_EFFECT message to the RendererActor.
     *
     * @param effectId Effect ID to set
     * @return true if message was sent
     */
    bool setEffect(uint8_t effectId);

    /**
     * @brief Start a transition to a new effect (thread-safe)
     *
     * Sends a START_TRANSITION message to the RendererActor.
     *
     * @param effectId Target effect ID
     * @param transitionType Transition type (0-11)
     * @return true if message was sent
     */
    bool startTransition(uint8_t effectId, uint8_t transitionType);

    /**
     * @brief Set brightness
     * @param brightness Brightness level (0-255)
     */
    bool setBrightness(uint8_t brightness);

    /**
     * @brief Set animation speed
     * @param speed Speed level (1-50)
     */
    bool setSpeed(uint8_t speed);

    /**
     * @brief Set palette
     * @param paletteIndex Palette index
     */
    bool setPalette(uint8_t paletteIndex);

    /**
     * @brief Set intensity
     * @param intensity Intensity level (0-255)
     */
    bool setIntensity(uint8_t intensity);

    /**
     * @brief Set saturation
     * @param saturation Saturation level (0-255)
     */
    bool setSaturation(uint8_t saturation);

    /**
     * @brief Set complexity
     * @param complexity Complexity level (0-255)
     */
    bool setComplexity(uint8_t complexity);

    /**
     * @brief Set variation
     * @param variation Variation level (0-255)
     */
    bool setVariation(uint8_t variation);

    /**
     * @brief Set global hue
     * @param hue Hue (0-255)
     */
    bool setHue(uint8_t hue);

    /**
     * @brief Set audio mood (Sensory Bridge pattern)
     * @param mood 0-255: 0=reactive, 255=smooth
     */
    bool setMood(uint8_t mood);

    /**
     * @brief Set fade amount (trail effect)
     * @param fadeAmount 0-255: 0=no fade, higher=faster fade
     */
    bool setFadeAmount(uint8_t fadeAmount);

#if FEATURE_AUDIO_SYNC
    // ========================================================================
    // Trinity Sync Commands (Offline ML Analysis)
    // ========================================================================

    /**
     * @brief Inject Trinity beat event
     * @param bpm BPM from Trinity analysis
     * @param phase01 Beat phase [0,1)
     * @param tick True if this is a beat boundary
     * @param downbeat True if this is a downbeat
     * @param beatInBar Position in bar (0-3 for 4/4)
     * @return true if message was sent
     */
    bool trinityBeat(float bpm, float phase01, bool tick, bool downbeat, int beatInBar);

    /**
     * @brief Update Trinity macro values
     * @param energy Overall energy (0-1)
     * @param vocal Vocal presence (0-1)
     * @param bass Bass weight (0-1)
     * @param perc Percussiveness (0-1)
     * @param bright Brightness (0-1)
     * @return true if message was sent
     */
    bool trinityMacro(float energy, float vocal, float bass, float perc, float bright);

    /**
     * @brief Trinity sync control (start/stop/pause/resume/seek)
     * @param action 0=start, 1=stop, 2=pause, 3=resume, 4=seek
     * @param positionSec Position in seconds
     * @param bpm BPM (optional, used for start/seek)
     * @return true if message was sent
     */
    bool trinitySync(uint8_t action, float positionSec, float bpm = 120.0f);
#endif

    // ========================================================================
    // Diagnostics
    // ========================================================================

    /**
     * @brief Get system statistics
     */
    SystemStats getStats() const;

    /**
     * @brief Print system status to serial
     */
    void printStatus();

    /**
     * @brief Get uptime in milliseconds
     */
    uint32_t getUptimeMs() const;

private:
    // Private constructor for singleton
    ActorSystem();
    ~ActorSystem();

    // Actor instances (using unique_ptr for RAII cleanup)
    std::unique_ptr<RendererActor> m_renderer;
    std::unique_ptr<ShowDirectorActor> m_showDirector;
#if FEATURE_AUDIO_SYNC
    std::unique_ptr<lightwaveos::audio::AudioActor> m_audio;  // Phase 2: Audio capture and DSP
#endif
    // Future: std::unique_ptr<NetworkActor> m_network;
    // Future: std::unique_ptr<HmiActor> m_hmi;
    // Future: std::unique_ptr<StateStoreActor> m_stateStore;
    // Future: std::unique_ptr<SyncManagerActor> m_syncManager;
    // Future: std::unique_ptr<PluginManagerActor> m_pluginManager;

    // State
    SystemState m_state;
    uint32_t m_startTime;
};

} // namespace actors
} // namespace lightwaveos
