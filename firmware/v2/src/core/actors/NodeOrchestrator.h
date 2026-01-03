/**
 * @file NodeOrchestrator.h
 * @brief Orchestrates all Nodes in the LightwaveOS v2 system
 *
 * The NodeOrchestrator is the top-level manager that:
 * - Creates and owns all Node instances
 * - Starts/stops Nodes in the correct order
 * - Provides access to Nodes for external code
 * - Handles system-wide events (shutdown, etc.)
 *
 * Startup order:
 * 1. StateStoreNode - Load saved state
 * 2. RendererNode - Initialize LEDs
 * 3. NetworkNode - Start web server
 * 4. HmiNode - Start encoder polling
 * 5. PluginManagerNode - Load plugins
 * 6. SyncManagerNode - Connect to peers
 *
 * Shutdown order: Reverse of startup
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "Node.h"
#include "RendererNode.h"
#include "ShowNode.h"
#include "../bus/MessageBus.h"
#include "../../config/features.h"
#include <memory>

// Audio integration (Phase 2)
#if FEATURE_AUDIO_SYNC
#include "../../audio/AudioNode.h"
#endif

namespace lightwaveos {
namespace nodes {

// ============================================================================
// System State
// ============================================================================

/**
 * @brief Overall system state
 */
enum class SystemState : uint8_t {
    UNINITIALIZED = 0,  // Not yet started
    STARTING,           // Nodes being created
    RUNNING,            // All nodes running
    STOPPING,           // Shutdown in progress
    STOPPED             // All nodes stopped
};

/**
 * @brief System-wide statistics
 */
struct SystemStats {
    uint32_t uptimeMs;              // Time since start
    uint32_t totalMessages;         // Total messages processed
    uint32_t heapFreeBytes;         // Current free heap
    uint32_t heapMinFreeBytes;      // Minimum free heap ever
    uint8_t activeNodes;           // Number of running nodes

    SystemStats()
        : uptimeMs(0), totalMessages(0)
        , heapFreeBytes(0), heapMinFreeBytes(0)
        , activeNodes(0) {}
};

// ============================================================================
// NodeOrchestrator Class
// ============================================================================

/**
 * @brief Top-level Node orchestration
 *
 * Singleton class that manages the lifecycle of all Nodes.
 *
 * Usage:
 *   NodeOrchestrator::instance().init();
 *   NodeOrchestrator::instance().start();
 *   // ... application running ...
 *   NodeOrchestrator::instance().shutdown();
 */
class NodeOrchestrator {
public:
    /**
     * @brief Get the singleton instance
     */
    static NodeOrchestrator& instance();

    // Prevent copying
    NodeOrchestrator(const NodeOrchestrator&) = delete;
    NodeOrchestrator& operator=(const NodeOrchestrator&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    /**
     * @brief Initialize the system (create nodes)
     *
     * Creates all Node instances but does not start them.
     * Call this once during setup().
     *
     * @return true if all nodes created successfully
     */
    bool init();

    /**
     * @brief Start all nodes
     *
     * Starts nodes in dependency order. Call after init().
     *
     * @return true if all nodes started successfully
     */
    bool start();

    /**
     * @brief Shutdown all nodes gracefully
     *
     * Stops nodes in reverse order. Blocks until complete.
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
    // Node Access
    // ========================================================================

    /**
     * @brief Get the RendererNode
     *
     * Returns nullptr if not initialized.
     */
    RendererNode* getRenderer() { return m_renderer.get(); }
    const RendererNode* getRenderer() const { return m_renderer.get(); }

    /**
     * @brief Get the ShowNode
     *
     * Returns nullptr if not initialized.
     */
    ShowNode* getShowDirector() { return m_showDirector.get(); }
    const ShowNode* getShowDirector() const { return m_showDirector.get(); }

#if FEATURE_AUDIO_SYNC
    /**
     * @brief Get the AudioNode (Phase 2)
     *
     * Returns nullptr if not initialized or FEATURE_AUDIO_SYNC disabled.
     */
    audio::AudioNode* getAudio() { return m_audio.get(); }
    const audio::AudioNode* getAudio() const { return m_audio.get(); }
#endif

    // Future: getNetwork(), getHmi(), getStateStore(), etc.

    // ========================================================================
    // Convenience Commands
    // ========================================================================

    /**
     * @brief Set the current effect
     *
     * Sends a SET_EFFECT message to the RendererNode.
     *
     * @param effectId Effect ID to set
     * @return true if message was sent
     */
    bool setEffect(uint8_t effectId);

    /**
     * @brief Start a transition to a new effect (thread-safe)
     *
     * Sends a START_TRANSITION message to the RendererNode.
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
    NodeOrchestrator();
    ~NodeOrchestrator();

    // Node instances (using unique_ptr for RAII cleanup)
    std::unique_ptr<RendererNode> m_renderer;
    std::unique_ptr<ShowNode> m_showDirector;
#if FEATURE_AUDIO_SYNC
    std::unique_ptr<audio::AudioNode> m_audio;  // Phase 2: Audio capture and DSP
#endif
    // Future: std::unique_ptr<NetworkNode> m_network;
    // Future: std::unique_ptr<HmiNode> m_hmi;
    // Future: std::unique_ptr<StateStoreNode> m_stateStore;
    // Future: std::unique_ptr<SyncManagerNode> m_syncManager;
    // Future: std::unique_ptr<PluginManagerNode> m_pluginManager;

    // State
    SystemState m_state;
    uint32_t m_startTime;
};

} // namespace nodes
} // namespace lightwaveos

