/**
 * @file AudioActor.h
 * @brief Actor for audio capture and processing pipeline
 *
 * The AudioActor runs on Core 0 and handles:
 * - I2S audio capture from SPH0645 microphone
 * - 256-sample hop capture at 62.5 Hz (Tab5 parity)
 * - Future: Goertzel frequency analysis, beat detection
 *
 * Architecture:
 *   AudioActor (Core 0, Priority 4)
 *     |
 *     +-> AudioCapture (I2S DMA)
 *     |
 *     +-> [Phase 2: AudioProcessor]
 *     |
 *     +-> [Phase 2: ControlBus output]
 *
 * The actor ticks every 16ms (matching hop duration) to capture
 * audio samples. Processing is deferred to Phase 2 implementation.
 *
 * Thread Safety:
 * - All capture/processing runs in the actor's task (Core 0)
 * - Results are published via MessageBus (lock-free)
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../config/features.h"

#if FEATURE_AUDIO_SYNC

#include "../core/actors/Actor.h"
#include "../config/audio_config.h"
#include "AudioCapture.h"

namespace lightwaveos {
namespace audio {

/**
 * @brief AudioActor state
 */
enum class AudioActorState : uint8_t {
    UNINITIALIZED = 0,  // Not started
    INITIALIZING,       // Starting up
    RUNNING,            // Normal operation
    PAUSED,             // Temporarily paused (muted)
    ERROR               // Initialization failed
};

/**
 * @brief Audio actor statistics
 */
struct AudioActorStats {
    uint32_t tickCount;          // Total ticks processed
    uint32_t captureSuccessCount; // Successful captures
    uint32_t captureFailCount;   // Failed captures
    uint32_t lastTickTimeUs;     // Time of last tick
    AudioActorState state;       // Current state

    void reset() {
        tickCount = 0;
        captureSuccessCount = 0;
        captureFailCount = 0;
        lastTickTimeUs = 0;
        state = AudioActorState::UNINITIALIZED;
    }
};

/**
 * @brief Actor responsible for audio capture and processing
 *
 * Runs on Core 0 at priority 4 (below Renderer at 5).
 * Tick interval is 16ms to match the 256-sample hop size at 16kHz.
 *
 * Phase 1 (Current):
 * - Initialize AudioCapture on start
 * - Capture hops in onTick()
 * - Track statistics
 *
 * Phase 2 (Future):
 * - Add AudioProcessor for Goertzel analysis
 * - Publish band energies to ControlBus
 * - Beat detection and onset events
 */
class AudioActor : public actors::Actor {
public:
    /**
     * @brief Construct the AudioActor
     *
     * Uses configuration from audio_config.h for timing and core affinity.
     */
    AudioActor();

    /**
     * @brief Destructor
     */
    ~AudioActor() override;

    // ========================================================================
    // State Accessors
    // ========================================================================

    /**
     * @brief Get current state
     */
    AudioActorState getState() const { return m_state; }

    /**
     * @brief Get audio actor statistics
     */
    const AudioActorStats& getStats() const { return m_stats; }

    /**
     * @brief Get audio capture statistics
     */
    const CaptureStats& getCaptureStats() const { return m_capture.getStats(); }

    /**
     * @brief Check if audio capture is working
     */
    bool isCapturing() const { return m_state == AudioActorState::RUNNING; }

    // ========================================================================
    // Control
    // ========================================================================

    /**
     * @brief Pause audio capture
     *
     * Stops capturing but keeps I2S initialized.
     */
    void pause();

    /**
     * @brief Resume audio capture
     *
     * Resumes capturing after pause.
     */
    void resume();

    /**
     * @brief Reset statistics
     */
    void resetStats();

    // ========================================================================
    // Buffer Access (for Phase 2 processing)
    // ========================================================================

    /**
     * @brief Get the most recent hop buffer
     *
     * Returns pointer to internal buffer containing last captured hop.
     * Valid only after successful capture. May be overwritten on next tick.
     *
     * @return Pointer to HOP_SIZE int16_t samples, or nullptr if no capture
     */
    const int16_t* getLastHop() const;

    /**
     * @brief Check if a new hop is available since last check
     *
     * Clears the flag when called.
     *
     * @return true if new hop available
     */
    bool hasNewHop();

protected:
    // ========================================================================
    // Actor Overrides
    // ========================================================================

    /**
     * @brief Initialize audio capture hardware
     *
     * Called when actor task starts on Core 0.
     */
    void onStart() override;

    /**
     * @brief Handle incoming messages
     *
     * Supported messages:
     * - SHUTDOWN: Stop actor
     * - HEALTH_CHECK: Respond with status
     */
    void onMessage(const actors::Message& msg) override;

    /**
     * @brief Capture one hop of audio samples
     *
     * Called every 16ms (matching hop duration).
     */
    void onTick() override;

    /**
     * @brief Cleanup audio capture hardware
     *
     * Called when actor is stopping.
     */
    void onStop() override;

private:
    // ========================================================================
    // Internal State
    // ========================================================================

    // Audio capture driver
    AudioCapture m_capture;

    // Current state
    AudioActorState m_state;

    // Statistics
    AudioActorStats m_stats;

    // Sample buffer for last captured hop
    int16_t m_hopBuffer[HOP_SIZE];

    // Flag for new hop availability
    volatile bool m_newHopAvailable;

    // ========================================================================
    // Internal Methods
    // ========================================================================

    /**
     * @brief Perform one capture cycle
     */
    void captureHop();

    /**
     * @brief Handle capture error
     */
    void handleCaptureError(CaptureResult result);
};

// ============================================================================
// Actor Configuration
// ============================================================================

namespace ActorConfigs {

/**
 * @brief Configuration for AudioActor
 *
 * Runs on Core 0 at priority 4 (below Renderer at 5).
 * 16ms tick interval matches hop size for Tab5 parity.
 */
inline actors::ActorConfig Audio() {
    return actors::ActorConfig(
        "Audio",                                    // name
        AUDIO_ACTOR_STACK_WORDS,                    // stackSize (12KB)
        AUDIO_ACTOR_PRIORITY,                       // priority (4)
        AUDIO_ACTOR_CORE,                           // coreId (0)
        16,                                         // queueSize
        pdMS_TO_TICKS(AUDIO_ACTOR_TICK_MS)          // tickInterval (16ms)
    );
}

} // namespace ActorConfigs

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
