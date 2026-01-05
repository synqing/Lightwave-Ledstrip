/**
 * @file AudioNode.h
 * @brief Actor for audio capture and processing pipeline
 *
 * The AudioNode runs on Core 0 and handles:
 * - I2S audio capture from SPH0645 microphone
 * - 256-sample hop capture at 62.5 Hz (Tab5 parity)
 * - Future: Goertzel frequency analysis, beat detection
 *
 * Architecture:
 *   AudioNode (Core 0, Priority 4)
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

#include "../core/actors/Node.h"
#include "../config/audio_config.h"
#include "AudioCapture.h"
#include "AudioTuning.h"
#include "GoertzelAnalyzer.h"
#include "ChromaAnalyzer.h"
#if FEATURE_STYLE_DETECTION
#include "StyleDetector.h"
#endif
#include "contracts/AudioTime.h"
#include "contracts/ControlBus.h"
#include "contracts/SnapshotBuffer.h"

// Tempo tracker
#include "tempo/TempoTracker.h"

#if FEATURE_AUDIO_BENCHMARK
#include "AudioBenchmarkMetrics.h"
#include "AudioBenchmarkRing.h"
#endif

namespace lightwaveos {
namespace audio {

/**
 * @brief AudioNode state
 */
enum class AudioNodeState : uint8_t {
    UNINITIALIZED = 0,  // Not started
    INITIALIZING,       // Starting up
    RUNNING,            // Normal operation
    PAUSED,             // Temporarily paused (muted)
    ERROR               // Initialization failed
};

/**
 * @brief Audio actor statistics
 */
struct AudioNodeStats {
    uint32_t tickCount;          // Total ticks processed
    uint32_t captureSuccessCount; // Successful captures
    uint32_t captureFailCount;   // Failed captures
    uint32_t lastTickTimeUs;     // Time of last tick
    AudioNodeState state;       // Current state

    void reset() {
        tickCount = 0;
        captureSuccessCount = 0;
        captureFailCount = 0;
        lastTickTimeUs = 0;
        state = AudioNodeState::UNINITIALIZED;
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
class AudioNode : public nodes::Node {
public:
    /**
     * @brief Construct the AudioNode
     *
     * Uses configuration from audio_config.h for timing and core affinity.
     */
    AudioNode();

    /**
     * @brief Destructor
     */
    ~AudioNode() override;

    // ========================================================================
    // State Accessors
    // ========================================================================

    /**
     * @brief Get current state
     */
    AudioNodeState getState() const { return m_state; }

    /**
     * @brief Get audio actor statistics
     */
    const AudioNodeStats& getStats() const { return m_stats; }

    /**
     * @brief Get audio capture statistics
     */
    const CaptureStats& getCaptureStats() const { return m_capture.getStats(); }

    /**
     * @brief Check if audio capture is working
     */
    bool isCapturing() const { return m_state == AudioNodeState::RUNNING; }

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

    // ========================================================================
    // Phase 2A: ControlBus Access for API
    // ========================================================================

    /**
     * @brief Get const reference to ControlBus for reading state
     *
     * Used by WebServer API handlers to access Zone AGC and spike detection
     * telemetry. Thread-safe for reading from Core 0.
     *
     * @return Const reference to ControlBus
     */
    const ControlBus& getControlBusRef() const { return m_controlBus; }

    /**
     * @brief Get mutable reference to ControlBus for configuration
     *
     * Used by WebServer API handlers to configure Zone AGC parameters
     * and reset spike detection stats. Should only be called from Core 0.
     *
     * @return Mutable reference to ControlBus
     */
    ControlBus& getControlBusMut() { return m_controlBus; }

    // ========================================================================
    // Phase 2C: Noise Calibration (SensoryBridge pattern)
    // ========================================================================

    /**
     * @brief Start noise calibration procedure
     *
     * Begins a 3-second (configurable) silent measurement period.
     * During this time, the system accumulates RMS and band energies
     * to establish a per-band noise floor.
     *
     * @param durationMs How long to measure (default 3000ms)
     * @param safetyMultiplier Multiply measured floor by this (default 1.2x)
     * @return true if calibration started, false if already running
     */
    bool startNoiseCalibration(uint32_t durationMs = 3000, float safetyMultiplier = 1.2f);

    /**
     * @brief Cancel an in-progress calibration
     */
    void cancelNoiseCalibration();

    /**
     * @brief Get current calibration state
     */
    CalibrationState getCalibrationState() const { return m_noiseCalibration.state; }

    /**
     * @brief Get calibration result (valid only when state == COMPLETE)
     */
    const NoiseCalibrationResult& getCalibrationResult() const { return m_noiseCalibration.result; }

    /**
     * @brief Get full calibration state for detailed status
     */
    const NoiseCalibrationState& getNoiseCalibrationState() const { return m_noiseCalibration; }

    /**
     * @brief Apply calibration results to current tuning
     *
     * Copies measured noise floors to perBandNoiseFloors and enables them.
     * Call this after calibration completes successfully.
     *
     * @return true if results were applied, false if no valid calibration
     */
    bool applyCalibrationResults();

    // ========================================================================
    // TempoTracker Integration
    // ========================================================================

    /**
     * @brief Get current tempo state
     */
    TempoOutput getTempo() const { return m_lastTempoOutput; }

    /**
     * @brief Check if tempo tracking is initialized
     */
    bool isTempoEnabled() const { return true; }  // Always enabled when audio is running

    // ========================================================================
    // Phase 2B: Benchmark Access
    // ========================================================================

#if FEATURE_AUDIO_BENCHMARK
    /**
     * @brief Get aggregated benchmark statistics
     *
     * Returns stats computed from the ring buffer samples.
     * Updated every ~1 second (62 hops).
     *
     * @return Reference to the benchmark stats struct
     */
    const AudioBenchmarkStats& getBenchmarkStats() const { return m_benchmarkStats; }

    /**
     * @brief Get the raw timing sample ring buffer
     *
     * For WebSocket streaming or detailed analysis. Contains
     * last ~1 second of per-hop timing data.
     *
     * @return Reference to the ring buffer
     */
    const AudioBenchmarkRing& getBenchmarkRing() const { return m_benchmarkRing; }

    /**
     * @brief Reset benchmark statistics
     *
     * Clears aggregated stats and peaks. Ring buffer continues
     * to receive new samples.
     */
    void resetBenchmarkStats() { m_benchmarkStats.reset(); }
#endif

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
    void onMessage(const nodes::Message& msg) override;

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
    AudioNodeState m_state;

    // Statistics
    AudioNodeStats m_stats;

    // Sample buffer for last captured hop
    int16_t m_hopBuffer[HOP_SIZE];
    int16_t m_hopBufferCentered[HOP_SIZE];
    int16_t m_prevHopCentered[HOP_SIZE];
    bool m_prevHopValid = false;

    // Flag for new hop availability (atomic for thread safety on dual-core ESP32)
    std::atomic<bool> m_newHopAvailable{false};

    // ========================================================================
    // Phase 2: DSP Processing State
    // ========================================================================

    // Goertzel frequency analyzer (8 bands, 512-sample window)
    GoertzelAnalyzer m_analyzer;

    // Chromagram analyzer (12 pitch classes, 512-sample window)
    ChromaAnalyzer m_chromaAnalyzer;

    // Style detector (MIS Phase 2: adaptive visual response)
#if FEATURE_STYLE_DETECTION
    StyleDetector m_styleDetector;
#endif

    // Previous chord root for chord change detection (StyleDetector/saliency input)
    uint8_t m_prevChordRoot = 0;

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

    // Priority 5: Per-band history for spectral flux
    float m_prevBands[8] = {0};

    // Last valid frequency bands (persisted between Goertzel updates)
    float m_lastBands[8] = {0};
    float m_lastBands64[8] = {0};  // 64-bin analysis folded to 8 bands
    bool m_analyze64Ready = false;  // True when 64-bin analysis has triggered at least once

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

    // Throttle for 64-bin Goertzel logging (log once per ~2 seconds)
    uint32_t m_goertzel64LogCounter = 0;
    static constexpr uint32_t GOERTZEL64_LOG_INTERVAL = 62;  // ~2 seconds @ 31 Hz

    // ========================================================================
    // Phase 2C: Goertzel Novelty Tuning
    // ========================================================================

    // Goertzel novelty tuning (runtime adjustable)
    GoertzelNoveltyTuning m_noveltyTuning;

    // ========================================================================
    // TempoTracker Beat Tracker
    // ========================================================================

    /// TempoTracker for beat detection
    TempoTracker m_tempo;

    /// Last tempo output for diagnostics
    TempoOutput m_lastTempoOutput;

    /// Cached 64-bin spectrum for tempo tracker novelty input
    /// Updated every ~94ms when 64-bin analysis completes, used every hop
    float m_bins64Cached[64] = {0};

    // ========================================================================
    // Stack Reduction: Large arrays moved from stack to class members
    // ========================================================================
    
    /// Temporary buffer for 64-bin Goertzel analysis (moved from stack to reduce stack usage)
    float m_bins64Raw[GoertzelAnalyzer::NUM_BINS] = {0};
    
    /// Temporary buffer for folded 64-bin bands (moved from stack to reduce stack usage)
    float m_bands64Folded[8] = {0};

    // ========================================================================
    // Phase 2C: Noise Calibration State
    // ========================================================================

    /// Noise calibration state machine (SensoryBridge pattern)
    NoiseCalibrationState m_noiseCalibration;

    // ========================================================================
    // Phase 2B: Benchmark Instrumentation
    // ========================================================================

#if FEATURE_AUDIO_BENCHMARK
    /// Ring buffer for per-hop timing samples (2KB, ~1 second history)
    AudioBenchmarkRing m_benchmarkRing;

    /// Aggregated statistics (updated every ~1 second)
    AudioBenchmarkStats m_benchmarkStats;

    /// Counter for stats aggregation interval
    uint32_t m_benchmarkAggregateCounter = 0;

    /// Aggregate stats every ~1 second (62 hops at 62.5 Hz)
    static constexpr uint32_t BENCHMARK_AGGREGATE_INTERVAL = 62;

    /// Aggregate samples from ring buffer into stats
    void aggregateBenchmarkStats();
#endif

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

    /**
     * @brief Process noise calibration state machine during hop
     *
     * Called by processHop() to accumulate samples during calibration.
     * Handles state transitions and result computation.
     *
     * @param rms Current hop RMS
     * @param bands Current hop band energies (8 bands)
     * @param chroma Current hop chroma values (12 bins)
     * @param nowMs Current time in milliseconds
     */
    void processNoiseCalibration(float rms, const float* bands, const float* chroma, uint32_t nowMs);
};

// ============================================================================
// Node Configuration
// ============================================================================

namespace NodeConfigs {

/**
 * @brief Configuration for AudioNode
 *
 * Runs on Core 0 at priority 4 (below Renderer at 5).
 * 16ms tick interval matches hop size for Tab5 parity.
 */
inline nodes::NodeConfig Audio() {
    return nodes::NodeConfig(
        "Audio",                                    // name
        AUDIO_ACTOR_STACK_WORDS,                    // stackSize (16KB)
        AUDIO_ACTOR_PRIORITY,                       // priority (4)
        AUDIO_ACTOR_CORE,                           // coreId (0)
        16,                                         // queueSize
        pdMS_TO_TICKS(AUDIO_ACTOR_TICK_MS)          // tickInterval (16ms)
    );
}

} // namespace NodeConfigs

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
