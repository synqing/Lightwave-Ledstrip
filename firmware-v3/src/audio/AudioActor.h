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
 * @brief Phase 1 Diagnostic: Audio pipeline health metrics
 * 
 * Tracks capture rate, publish rate, and frame freshness for
 * systematic debugging of audio availability issues.
 * Modeled after Emotiscope's proven diagnostic approach.
 */
struct AudioPipelineDiagnostics {
    // === Phase 1.1: Capture Rate Diagnostics ===
    uint64_t diagStartTimeUs = 0;      ///< When diagnostics started
    uint32_t captureAttempts = 0;      ///< Total capture attempts
    uint32_t captureSuccesses = 0;     ///< Successful captures
    uint32_t captureDmaTimeouts = 0;   ///< DMA timeout count
    uint32_t captureReadErrors = 0;    ///< Read error count
    
    // === Phase 1.2: Publish Diagnostics ===
    uint32_t publishCount = 0;         ///< Frames published to SnapshotBuffer
    uint32_t publishSeqGaps = 0;       ///< Sequence number gaps detected
    uint32_t lastPublishSeq = 0;       ///< Last published sequence number
    
    // === Phase 2.1: I2S/ES8311 Hardware Validation ===
    int16_t lastRawMin = 0;            ///< Min raw sample in last hop (pre-DC block)
    int16_t lastRawMax = 0;            ///< Max raw sample in last hop (pre-DC block)
    float lastRawRms = 0.0f;           ///< RMS of raw samples (pre-DC block)
    bool samplesNonZero = false;       ///< True if samples vary (not all zeros/constant)
    uint32_t zeroHopCount = 0;         ///< Count of hops with all-zero samples
    
    // === Phase 2.3: Timing/Latency Diagnostics ===
    uint64_t lastCaptureStartUs = 0;   ///< Start of last capture
    uint64_t lastCaptureEndUs = 0;     ///< End of last capture
    uint64_t lastProcessEndUs = 0;     ///< End of last processHop()
    uint64_t lastPublishTimeUs = 0;    ///< Timestamp of last publish
    uint32_t maxCaptureLatencyUs = 0;  ///< Max capture duration (I2S read)
    uint32_t maxProcessLatencyUs = 0;  ///< Max processHop() duration
    uint32_t avgCaptureLatencyUs = 0;  ///< Exponential moving average of capture latency
    uint32_t avgProcessLatencyUs = 0;  ///< Exponential moving average of process latency
    
    void reset() {
        diagStartTimeUs = 0;
        captureAttempts = 0;
        captureSuccesses = 0;
        captureDmaTimeouts = 0;
        captureReadErrors = 0;
        publishCount = 0;
        publishSeqGaps = 0;
        lastPublishSeq = 0;
        lastRawMin = 0;
        lastRawMax = 0;
        lastRawRms = 0.0f;
        samplesNonZero = false;
        zeroHopCount = 0;
        lastCaptureStartUs = 0;
        lastCaptureEndUs = 0;
        lastProcessEndUs = 0;
        lastPublishTimeUs = 0;
        maxCaptureLatencyUs = 0;
        maxProcessLatencyUs = 0;
        avgCaptureLatencyUs = 0;
        avgProcessLatencyUs = 0;
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

#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    /**
     * @brief Get current microphone gain in dB
     * @return Gain in dB (0, 6, 12, 18, 24, 30, 36, 42), or -1 if not supported
     */
    int8_t getMicGainDb() const { return m_capture.getMicGainDb(); }

    /**
     * @brief Set microphone input gain
     * @param gainDb Gain in dB (must be 0, 6, 12, 18, 24, 30, 36, or 42)
     * @return true if gain was set successfully
     */
    bool setMicGainDb(int8_t gainDb) { return m_capture.setMicGainDb(gainDb); }
#endif

    /**
     * @brief Get pipeline diagnostics (Phase 1 debugging)
     */
    const AudioPipelineDiagnostics& getDiagnostics() const { return m_diag; }

    /**
     * @brief Print comprehensive pipeline diagnostics to serial
     * 
     * Called by 'adbg diag' serial command or automatically every 10 seconds.
     * Includes capture rate, publish rate, sample health, and timing.
     */
    void printDiagnostics();

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
    // One-Shot Debug Output Methods (called by serial commands)
    // ========================================================================

    /**
     * @brief Print health summary (mic level, RMS, AGC state)
     *
     * Called by 'dbg status' or 'adbg status' serial command.
     * Prints regardless of verbosity level (user-triggered).
     */
    void printStatus();

    /**
     * @brief Print current 8-band and 64-bin spectrum
     *
     * Called by 'dbg spectrum' or 'adbg spectrum' serial command.
     * Prints regardless of verbosity level (user-triggered).
     */
    void printSpectrum();

    /**
     * @brief Print beat tracking state (BPM, phase, confidence)
     *
     * Called by 'dbg beat' or 'adbg beat' serial command.
     * Prints regardless of verbosity level (user-triggered).
     */
    void printBeat();


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
    // TempoTracker Integration (replaces K1 Pipeline)
    // ========================================================================

    /**
     * @brief Get const reference to TempoTracker for diagnostics
     */
    const TempoTracker& getTempo() const { return m_tempo; }

    /**
     * @brief Get mutable reference to TempoTracker for phase advancement
     *
     * Called by ActorSystem to give RendererActor access to advancePhase().
     * RendererActor calls advancePhase() at 120 FPS in the render loop.
     */
    TempoTracker& getTempoMut() { return m_tempo; }

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

    // Pipeline diagnostics (Phase 1: systematic audio debugging)
    AudioPipelineDiagnostics m_diag;

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
    // TempoTracker Beat Tracker
    // ========================================================================

    /// Tempo tracker
    TempoTracker m_tempo;

    /// Last tempo output for diagnostics
    TempoTrackerOutput m_lastTempoOutput;

    /// Cached 64-bin spectrum for tempo tracker novelty input
    /// Updated every ~94ms when 64-bin analysis completes, used every hop
    float m_bins64Cached[64] = {0};
    float m_bins64AdaptiveMax = 0.0001f;  // Sensory Bridge adaptive normalisation follower

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

    // ========================================================================
    // Sensory Bridge parity side-car pipeline (3.1.0 + 4.1.1)
    // ========================================================================
    void processSbWaveformSidecar(const ControlBusRawInput& raw);
    void processSbBloomSidecar(const ControlBusRawInput& raw);
    void updateSbNoveltyAndHueShift();

    // Parity buffers/state (3.1.0 waveform)
    static constexpr uint8_t SB_WAVEFORM_POINTS = CONTROLBUS_WAVEFORM_N;
    static constexpr uint8_t SB_WAVEFORM_HISTORY = 4;
    int16_t m_sbWaveformHistory[SB_WAVEFORM_HISTORY][SB_WAVEFORM_POINTS] = {{0}};
    uint8_t m_sbWaveformHistoryIndex = 0;
    float m_sbMaxWaveformValFollower = 750.0f;  // Sensory Bridge sweet spot min level
    float m_sbWaveformPeakScaled = 0.0f;
    float m_sbWaveformPeakScaledLast = 0.0f;
    float m_sbNoteChroma[CONTROLBUS_NUM_CHROMA] = {0};
    float m_sbChromaMaxVal = 0.0f;

    // Parity buffers/state (4.1.1 bloom)
    static constexpr uint8_t SB_NUM_FREQS = 64;
    static constexpr uint8_t SB_SPECTRAL_HISTORY = 5;
    float m_sbSpectrogram[SB_NUM_FREQS] = {0};
    float m_sbSpectrogramSmooth[SB_NUM_FREQS] = {0};
    float m_sbChromagramSmooth[CONTROLBUS_NUM_CHROMA] = {0};
    float m_sbChromagramMaxPeak = 0.001f;
    int16_t m_sbWaveform[SB_WAVEFORM_POINTS] = {0};
    float m_sbSpectralHistory[SB_SPECTRAL_HISTORY][SB_NUM_FREQS] = {{0}};
    float m_sbNoveltyCurve[SB_SPECTRAL_HISTORY] = {0};
    uint8_t m_sbSpectralHistoryIndex = 0;

    // Auto colour shift (4.1.1)
    float m_sbHuePosition = 0.0f;
    float m_sbHueShiftSpeed = 0.0f;
    float m_sbHuePushDirection = -1.0f;
    float m_sbHueDestination = 0.0f;
    float m_sbHueShiftingMix = -0.35f;
    float m_sbHueShiftingMixTarget = 1.0f;
    uint32_t m_sbRand = 0x12345678u;
};

// ============================================================================
// Actor Configuration
// ============================================================================

namespace ActorConfigs {

/**
 * @brief Ceiling conversion from milliseconds to ticks.
 * 
 * Unlike pdMS_TO_TICKS() which floors (8ms -> 0 ticks at 100Hz),
 * this macro rounds UP so 8ms -> 1 tick. Uses integer math with
 * +999 to implement ceiling divide by 1000.
 */
#define LW_MS_TO_TICKS_CEIL(ms) \
    ((TickType_t)(((ms) <= 0) ? 0 : \
      (((uint64_t)(ms) * (uint64_t)configTICK_RATE_HZ + 999ULL) / 1000ULL)))

/**
 * @brief Ceiling conversion with minimum of 1 tick for non-zero ms.
 * 
 * This is the correct macro for actor tick intervals where ms > 0
 * must result in at least 1 tick to prevent hot loops.
 */
#define LW_MS_TO_TICKS_CEIL_MIN1(ms) \
    ((TickType_t)(((ms) <= 0) ? 0 : \
      (LW_MS_TO_TICKS_CEIL(ms) == 0 ? 1 : LW_MS_TO_TICKS_CEIL(ms))))

/**
 * @brief Configuration for AudioActor
 *
 * Runs on Core 0 at priority 4 (below Renderer at 5).
 * 
 * SCHEDULER-ALIGNED MODE:
 * With HOP_SIZE=160 @ 16kHz = 10ms = 100 Hz, the hop rate now matches
 * the FreeRTOS tick rate (CONFIG_FREERTOS_HZ=100). This eliminates
 * timing drift and multi-hop compensation hacks.
 */
inline actors::ActorConfig Audio() {
    return actors::ActorConfig(
        "Audio",                                    // name
        AUDIO_ACTOR_STACK_WORDS,                    // stackSize (16KB)
        AUDIO_ACTOR_PRIORITY,                       // priority (4)
        AUDIO_ACTOR_CORE,                           // coreId (0)
        16,                                         // queueSize
        LW_MS_TO_TICKS_CEIL_MIN1(AUDIO_ACTOR_TICK_MS) // tickInterval: 10ms = 1 tick @ 100Hz RTOS
    );
}

} // namespace ActorConfigs

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
