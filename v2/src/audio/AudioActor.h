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

#include <atomic>

#include "../core/actors/Actor.h"
#include "../config/audio_config.h"
#include "AudioCapture.h"
#include "AudioTuning.h"
#include "GoertzelAnalyzer.h"
#include "ChromaAnalyzer.h"
#include "contracts/AudioTime.h"
#include "contracts/ControlBus.h"
#include "contracts/SnapshotBuffer.h"

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
 * @brief Snapshot of DSP state for diagnostics
 */
struct AudioDspState {
    float rmsRaw = 0.0f;
    float rmsMapped = 0.0f;
    float rmsPreGain = 0.0f;
    float fluxMapped = 0.0f;

    float agcGain = 1.0f;
    float dcEstimate = 0.0f;
    float noiseFloor = 0.0f;

    int16_t minSample = 0;
    int16_t maxSample = 0;
    int16_t peakCentered = 0;
    float meanSample = 0.0f;
    uint16_t clipCount = 0;
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

    // ========================================================================
    // Phase 2: Cross-Core Access
    // ========================================================================

    /**
     * @brief Get the ControlBus snapshot buffer for cross-core reads
     *
     * RendererActor calls this to get a reference to the SnapshotBuffer,
     * then reads snapshots by value for thread-safe access.
     *
     * @return Reference to the ControlBusFrame SnapshotBuffer
     */
    const SnapshotBuffer<ControlBusFrame>& getControlBusBuffer() const {
        return m_controlBusBuffer;
    }

    /**
     * @brief Get current sample index (monotonic)
     */
    uint64_t getSampleIndex() const { return m_sampleIndex; }

    /**
     * @brief Get hop count since start
     */
    uint32_t getHopCount() const { return m_hopCount; }

    /**
     * @brief Get current audio pipeline tuning (by value)
     */
    AudioPipelineTuning getPipelineTuning() const;

    /**
     * @brief Update audio pipeline tuning
     */
    void setPipelineTuning(const AudioPipelineTuning& tuning);

    /**
     * @brief Reset DSP state (AGC, DC estimate, noise floor)
     */
    void resetDspState();

    /**
     * @brief Get last DSP diagnostics snapshot
     */
    AudioDspState getDspState() const;

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
    int16_t m_hopBufferCentered[HOP_SIZE];

    // Flag for new hop availability (atomic for thread safety on dual-core ESP32)
    std::atomic<bool> m_newHopAvailable{false};

    // ========================================================================
    // Phase 2: DSP Processing State
    // ========================================================================

    // Goertzel frequency analyzer (8 bands, 512-sample window)
    GoertzelAnalyzer m_analyzer;

    // Chromagram analyzer (12 pitch classes, 512-sample window)
    ChromaAnalyzer m_chromaAnalyzer;

    // ControlBus state machine (smoothing, attack/release)
    ControlBus m_controlBus;

    // Lock-free buffer for cross-core sharing with RendererActor
    SnapshotBuffer<ControlBusFrame> m_controlBusBuffer;

    // Monotonic sample counter (64-bit for no overflow)
    uint64_t m_sampleIndex = 0;

    // Hop counter since start
    uint32_t m_hopCount = 0;

    // Previous RMS for flux calculation
    float m_prevRMS = 0.0f;

    // Last valid frequency bands (persisted between Goertzel updates)
    float m_lastBands[8] = {0};

    float m_lastRmsRaw = 0.0f;
    float m_lastRmsMapped = 0.0f;
    float m_lastFluxMapped = 0.0f;
    int16_t m_lastMinSample = 0;
    int16_t m_lastMaxSample = 0;
    int16_t m_lastPeakCentered = 0;
    float m_lastMeanSample = 0.0f;
    float m_lastRmsPreGain = 0.0f;
    float m_lastAgcGain = 1.0f;
    float m_lastDcEstimate = 0.0f;
    uint16_t m_lastClipCount = 0;

    float m_dcEstimate = 0.0f;
    float m_agcGain = 1.0f;
    float m_noiseFloor = 0.001f;

    AudioPipelineTuning m_pipelineTuning;
    std::atomic<uint32_t> m_pipelineTuningSeq{0};

    AudioDspState m_dspState;
    std::atomic<uint32_t> m_dspStateSeq{0};
    std::atomic<bool> m_dspResetPending{false};

    // Throttle for Goertzel debug logging (log once per ~2 seconds)
    uint32_t m_goertzelLogCounter = 0;
    static constexpr uint32_t GOERTZEL_LOG_INTERVAL = 62;  // ~2 seconds @ 31 Hz

    // ========================================================================
    // Internal Methods
    // ========================================================================

    /**
     * @brief Perform one capture cycle
     */
    void captureHop();

    /**
     * @brief Process captured hop through DSP pipeline (Phase 2)
     *
     * Called after successful capture. Performs:
     * 1. RMS calculation
     * 2. Spectral flux calculation
     * 3. Goertzel band analysis (accumulated over 512 samples)
     * 4. ControlBus update with smoothing
     * 5. SnapshotBuffer publish for renderer
     */
    void processHop();

    /**
     * @brief Compute RMS energy of sample buffer
     *
     * @param samples Pointer to int16_t samples
     * @param count Number of samples
     * @return Normalized RMS value [0.0, 1.0]
     */
    float computeRMS(const int16_t* samples, size_t count);

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
        AUDIO_ACTOR_STACK_WORDS,                    // stackSize (16KB)
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
