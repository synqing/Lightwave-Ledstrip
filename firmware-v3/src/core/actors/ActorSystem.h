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
#include "../../config/effect_ids.h"
#include <memory>

// Audio integration (Phase 2)
#if FEATURE_AUDIO_SYNC
#include "../../audio/AudioActor.h"
#include "../../audio/contracts/ControlBus.h"
#include "../../audio/contracts/SnapshotBuffer.h"
#endif

// AMOLED test rig display
#if FEATURE_AMOLED_DISPLAY
#include "../../hal/display/DisplayActor.h"
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

enum class TransitionDispatchResult : uint8_t {
    None = 0,
    Sent,
    RendererUnavailable,
    TransitionsDisabled,
    QueueSaturated,
    SendFailed
};

// ============================================================================
// Manual Arm/Fire Staging (Phase 2.3)
// ============================================================================

/**
 * @brief Two independent staging slots for the manual arm/fire workflow.
 *
 * Slot 1 (queued transition): type + duration + easing. Sentinel
 * `kNoTransition` = 0xFF means no transition queued.
 * Slot 2 (staged effect): the effect that will fire on Enter. Sentinel
 * `INVALID_EFFECT_ID` means no effect staged.
 *
 * `isArmedPair()` is true when BOTH slots are populated — the canonical
 * "ready to fire a transition" condition. `hasStagedEffect()` alone is
 * a hard cut on fire. `hasQueuedTransition()` alone preserves the legacy
 * t/T → next-effect-change auto-fire path.
 *
 * State is derived from sentinels — there is no separate `isArmed` bool.
 *
 * Thread safety: mutations are Core-0 only (SerialCLI tick today; future
 * REST/WS surfaces will need a mutex). The renderer (Core 1) never reads
 * this struct directly — fire-time values are packed into the
 * START_TRANSITION message via the existing queue.
 */
struct ManualStaging {
    static constexpr uint8_t kNoTransition = 0xFF;

    uint8_t  queuedTransitionType   = kNoTransition;
    uint16_t queuedDurationMs       = 0;      // 0 = tier default
    uint8_t  queuedEasing           = 0xFF;   // 0xFF = engine default
    EffectId stagedEffect           = INVALID_EFFECT_ID;

    bool hasQueuedTransition() const { return queuedTransitionType != kNoTransition; }
    bool hasStagedEffect()    const { return stagedEffect != INVALID_EFFECT_ID; }
    bool isArmedPair()        const { return hasStagedEffect() && hasQueuedTransition(); }
    bool isAnyArmed()         const { return hasStagedEffect() || hasQueuedTransition(); }
};

/**
 * @brief Result of fireArmedPair() — distinguishes the four outcomes.
 */
enum class FireResult : uint8_t {
    Fired = 0,            // Paired transition dispatched
    HardCut,              // Staged-effect-only — setEffect dispatched
    NotArmed,             // Both slots empty; silent per Captain spec
    Busy,                 // A transition is already running
    TransitionsDisabled,  // Kill switch off; staged effect set as hard cut
};

/**
 * @brief System-wide statistics
 */
struct SystemStats {
    uint32_t uptimeMs;              // Time since start
    uint32_t totalMessages;         // Total messages processed
    uint32_t heapFreeBytes;         // Current free heap
    uint32_t heapMinFreeBytes;      // Minimum free heap ever
    uint32_t spiramFreeBytes;       // Current free SPIRAM
    uint8_t activeActors;           // Number of running actors

    SystemStats()
        : uptimeMs(0), totalMessages(0)
        , heapFreeBytes(0), heapMinFreeBytes(0)
        , spiramFreeBytes(0)
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

#if FEATURE_AMOLED_DISPLAY
    /**
     * @brief Get the DisplayActor (AMOLED test rig)
     */
    lightwaveos::display::DisplayActor* getDisplay() { return m_display.get(); }
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
     * @param effectId Effect ID to set (stable namespaced EffectId)
     * @return true if message was sent
     */
    bool setEffect(EffectId effectId);

    /**
     * @brief Start a transition to a new effect (thread-safe)
     *
     * Sends a START_TRANSITION message to the RendererActor.
     *
     * @param effectId Target effect ID (stable namespaced EffectId)
     * @param transitionType Transition type (0-11)
     * @return true if message was sent
     */
    bool startTransition(EffectId effectId,
                         uint8_t transitionType,
                         uint16_t durationMs = 0,
                         uint8_t easing = 0xFF);

    TransitionDispatchResult getLastTransitionDispatchResult() const {
        return m_lastTransitionDispatchResult;
    }

    // ========================================================================
    // Manual Arm/Fire Staging (Phase 2.3)
    // ========================================================================
    //
    // Mutators below are Core-0 only — SerialCLI tick today. Future
    // REST/WS arm/fire surfaces will need a mutex around the staging
    // member. The renderer never reads m_staging directly; fireArmedPair
    // packs the staged values into a START_TRANSITION message which the
    // renderer processes through the existing queue.

    /**
     * @brief Queue a transition type to fire on the next deliberate trigger.
     *
     * Replaces any previously queued transition. The legacy t/T fast path
     * still consumes this on the next effect change (Space/n/N) when no
     * effect is staged. Explicit Enter consumes it paired with the staged
     * effect (Phase 2.3 deliberate-fire path).
     */
    void queueTransition(uint8_t transitionType,
                         uint16_t durationMs = 0,
                         uint8_t easing = 0xFF);

    /**
     * @brief Clear only the queued transition slot.
     */
    void clearQueuedTransition();

    /**
     * @brief Stage an effect for deliberate firing.
     *
     * Suppresses Space/n/N auto-cycle (collision rule). Enter fires the
     * staged effect — with the queued transition if armed, otherwise as
     * a hard cut.
     */
    void stageEffect(EffectId eid);

    /**
     * @brief Clear only the staged effect slot.
     */
    void clearStagedEffect();

    /**
     * @brief Clear both staging slots (Esc keystroke).
     */
    void disarmAll();

    /**
     * @brief Fire the armed staging — paired transition, hard cut, or no-op.
     *
     * @return FireResult discriminating Fired / HardCut / NotArmed / Busy /
     *         TransitionsDisabled. SerialCLI uses this to emit the correct
     *         echo line (or stay silent on NotArmed per Captain spec).
     */
    FireResult fireArmedPair();

    /**
     * @brief Read-only view of the current staging slots.
     *
     * Callers MUST stay on Core 0. Reference is stable until the next
     * staging mutation on the same core.
     */
    const ManualStaging& getStaging() const { return m_staging; }

    /**
     * @brief Compute leadTime for a transition + optional duration override.
     *
     * Phase 2.4 will surface this via REST POST /api/v1/transitions/leadtime
     * (declared here so the SerialCLI helper can already delegate). Returns
     * `effectiveDuration + kTransitionSafetyMarginMs`. Returns 0 when
     * `transitionType` is out of range (invalid input → no lead).
     */
    uint16_t getRealLeadTime(uint8_t transitionType,
                             uint16_t requestedDurationMs = 0) const;

    /**
     * @brief Safety margin added on top of effective duration for leadTime.
     *
     * Empirical: covers message-queue traversal, frame quantisation, and
     * target effect init time on typical effects. Refine with profiling
     * data when available.
     */
    static constexpr uint16_t kTransitionSafetyMarginMs = 50;

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

    /**
     * @brief Set edge mixer mode
     * @param mode 0=mirror, 1=analogous, 2=complementary
     */
    bool setEdgeMixerMode(uint8_t mode);

    /**
     * @brief Set edge mixer spread
     * @param spread Hue spread in degrees (0-60)
     */
    bool setEdgeMixerSpread(uint8_t spread);

    /**
     * @brief Set edge mixer strength
     * @param strength Mix strength (0-255)
     */
    bool setEdgeMixerStrength(uint8_t strength);

    /**
     * @brief Set edge mixer spatial mode
     * @param spatial 0=uniform, 1=centre_gradient
     */
    bool setEdgeMixerSpatial(uint8_t spatial);

    /**
     * @brief Set edge mixer temporal mode
     * @param temporal 0=static, 1=rms_gate
     */
    bool setEdgeMixerTemporal(uint8_t temporal);

    /**
     * @brief Persist EdgeMixer state to NVS
     */
    bool saveEdgeMixerToNVS();

    /**
     * @brief Set LED output dithering
     * @param enabled true=enable FastLED temporal dithering
     */
    bool setLedDithering(bool enabled);

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

    /**
     * @brief Inject Trinity structure segment change
     * @param index Segment index (0-255)
     * @param labelHash16 16-bit hash of segment label (stable identifier, avoids string storage)
     * @param startSec Segment start time (seconds)
     * @param endSec Segment end time (seconds)
     * @return true if message was sent
     */
    bool trinitySegment(uint8_t index, uint16_t labelHash16, float startSec, float endSec);
#endif

#if FEATURE_AUDIO_SYNC
    uint8_t getStimulusMode() const;
    bool setStimulusMode(uint8_t mode);
    bool clearStimulus();
    bool copyStimulusFrame(lightwaveos::audio::ControlBusFrame& out) const;
    bool publishStimulusFrame(const lightwaveos::audio::ControlBusFrame& frame);
    const lightwaveos::audio::SnapshotBuffer<lightwaveos::audio::ControlBusFrame>& getStimulusControlBusBuffer() const;
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
#if FEATURE_AMOLED_DISPLAY
    std::unique_ptr<lightwaveos::display::DisplayActor> m_display;  // AMOLED test rig
#endif
    // Future: std::unique_ptr<NetworkActor> m_network;
    // Future: std::unique_ptr<HmiActor> m_hmi;
    // Future: std::unique_ptr<StateStoreActor> m_stateStore;
    // Future: std::unique_ptr<SyncManagerActor> m_syncManager;
    // Future: std::unique_ptr<PluginManagerActor> m_pluginManager;

    // State
    SystemState m_state;
    uint32_t m_startTime;
    TransitionDispatchResult m_lastTransitionDispatchResult;
    ManualStaging m_staging;  // Phase 2.3 — Core-0 only
#if FEATURE_AUDIO_SYNC
    lightwaveos::audio::SnapshotBuffer<lightwaveos::audio::ControlBusFrame> m_stimulusControlBusBuffer;
    lightwaveos::audio::ControlBusFrame m_stimulusLastFrame;
    uint32_t m_stimulusLastPublishMs = 0;
    uint8_t m_stimulusMode = 0;
    SemaphoreHandle_t m_stimulusMutex = nullptr;
#endif
};

} // namespace actors
} // namespace lightwaveos
