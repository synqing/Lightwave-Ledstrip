#pragma once
#include <stdint.h>
#include <math.h>
#include "AudioTime.h"
#include "MusicalSaliency.h"
#include "StyleDetector.h"

namespace lightwaveos::audio {

static constexpr uint8_t CONTROLBUS_NUM_BANDS  = 8;
static constexpr uint8_t CONTROLBUS_NUM_CHROMA = 12;
static constexpr uint8_t CONTROLBUS_WAVEFORM_N = 128;  // Sensory Bridge NATIVE_RESOLUTION waveform points

// Lookahead spike smoothing constants (Sensory Bridge pattern)
static constexpr size_t LOOKAHEAD_FRAMES = 3;      // 3-frame ring buffer for spike detection
static constexpr size_t LOOKAHEAD_MAX_BANDS = 64;  // Support future 64-bin expansion

// Zone AGC configuration (Sensory Bridge insight: 4 zones across frequency spectrum)
// Prevents bass frequencies from dominating the visualization
static constexpr uint8_t CONTROLBUS_NUM_ZONES = 4;

/**
 * @brief Chord type enumeration for triad classification.
 */
enum class ChordType : uint8_t {
    NONE = 0,       ///< No chord detected / insufficient confidence
    MAJOR = 1,      ///< Major triad (root + major 3rd + perfect 5th)
    MINOR = 2,      ///< Minor triad (root + minor 3rd + perfect 5th)
    DIMINISHED = 3, ///< Diminished triad (root + minor 3rd + diminished 5th)
    AUGMENTED = 4   ///< Augmented triad (root + major 3rd + augmented 5th)
};

/**
 * @brief Chord detection state published per frame.
 *
 * Detects triads from chromagram by finding dominant pitch class (root)
 * and checking intervals for third (+3 or +4 semitones) and fifth (+6, +7, or +8).
 */
struct ChordState {
    uint8_t rootNote = 0;           ///< 0-11 (C=0, C#=1, D=2, ..., B=11)
    ChordType type = ChordType::NONE;
    float confidence = 0.0f;        ///< 0.0-1.0 triad energy ratio
    float rootStrength = 0.0f;      ///< Energy at root pitch class
    float thirdStrength = 0.0f;     ///< Energy at third interval
    float fifthStrength = 0.0f;     ///< Energy at fifth interval
};

/**
 * @brief Raw (unsmoothed) per-hop measurements produced by the audio thread.
 */
struct ControlBusRawInput {
    float rms = 0.0f;   // 0..1
    float flux = 0.0f;  // 0..1 (novelty proxy)

    float bands[CONTROLBUS_NUM_BANDS] = {0};     // 0..1
    float chroma[CONTROLBUS_NUM_CHROMA] = {0};   // 0..1 (optional in Phase 2)
    int16_t waveform[CONTROLBUS_WAVEFORM_N] = {0};  // Time-domain samples (int16_t range: -32768 to 32767)

    // Phase 1.2: Onset detection for percussive elements
    float snareEnergy = 0.0f;       // 0..1 snare band energy (150-300 Hz)
    float hihatEnergy = 0.0f;       // 0..1 hi-hat band energy (6-12 kHz)
    bool snareTrigger = false;      // True on snare onset frame
    bool hihatTrigger = false;      // True on hi-hat onset frame

    // Phase 2: Full 64-bin Goertzel spectrum (110 Hz - 4186 Hz)
    static constexpr uint8_t BINS_64_COUNT = 64;
    float bins64[BINS_64_COUNT] = {0};  // 0..1 normalized magnitudes

    // Tempo tracker output (saliency computation ONLY - effects read MusicalGrid)
    bool tempoLocked = false;       ///< TempoTracker lock state (saliency; effects read MusicalGrid)
    float tempoConfidence = 0.0f;   ///< TempoTracker confidence (saliency; effects read MusicalGrid.confidence)
    bool tempoBeatTick = false;     ///< TempoTracker beat tick gated by lock (saliency support)
};

/**
 * @brief Published control frame consumed by renderer (BY VALUE).
 */
struct ControlBusFrame {
    AudioTime t;
    uint32_t hop_seq = 0;

    float rms = 0.0f;
    float flux = 0.0f;
    float fast_rms = 0.0f;
    float fast_flux = 0.0f;

    float bands[CONTROLBUS_NUM_BANDS] = {0};
    float chroma[CONTROLBUS_NUM_CHROMA] = {0};
    float heavy_bands[CONTROLBUS_NUM_BANDS] = {0};
    float heavy_chroma[CONTROLBUS_NUM_CHROMA] = {0};
    int16_t waveform[CONTROLBUS_WAVEFORM_N] = {0};  // Time-domain samples (int16_t range: -32768 to 32767)

    ChordState chordState;  // Chord detection results (root, type, confidence)

    MusicalSaliencyFrame saliency;  // Musical saliency metrics (harmonic, rhythmic, timbral, dynamic novelty)

    // MIS Phase 2: Adaptive style detection
    MusicStyle currentStyle = MusicStyle::UNKNOWN;  ///< Detected music style classification
    float styleConfidence = 0.0f;                   ///< Style detection confidence (0.0-1.0)

    // Phase 1.2: Onset detection for percussive elements (passthrough from raw)
    float snareEnergy = 0.0f;       // 0..1 snare band energy (150-300 Hz)
    float hihatEnergy = 0.0f;       // 0..1 hi-hat band energy (6-12 kHz)
    bool snareTrigger = false;      // True on snare onset frame
    bool hihatTrigger = false;      // True on hi-hat onset frame

    // Phase 2: Full 64-bin Goertzel spectrum (110 Hz - 4186 Hz)
    static constexpr uint8_t BINS_64_COUNT = 64;
    float bins64[BINS_64_COUNT] = {0};  // 0..1 normalized magnitudes

    // Tempo tracker output (saliency computation ONLY - effects read MusicalGrid)
    bool tempoLocked = false;       ///< TempoTracker lock state (saliency; effects read MusicalGrid)
    float tempoConfidence = 0.0f;   ///< TempoTracker confidence (saliency; effects read MusicalGrid.confidence)
    bool tempoBeatTick = false;     ///< TempoTracker beat tick gated by lock (saliency support)

    // Silence detection (Sensory Bridge pattern)
    // silentScale fades from 1.0 to 0.0 after silenceHysteresisMs of silence
    float silentScale = 1.0f;       ///< 0.0=silent, 1.0=active (multiply with brightness)
    bool isSilent = false;          ///< True when silence threshold exceeded duration
};

/**
 * @brief Lookahead buffer for spike detection (Sensory Bridge pattern).
 *
 * Uses a 3-frame ring buffer to detect single-frame spikes that cause
 * visual flicker. When the middle frame is a spike (direction change),
 * it's replaced with the average of its neighbors.
 *
 * Output is delayed by 2 frames (~32ms at 60fps) which is acceptable
 * for visual effects but maintains fast transient response.
 */
struct LookaheadBuffer {
    float history[LOOKAHEAD_FRAMES][LOOKAHEAD_MAX_BANDS] = {{0}};
    size_t current_frame = 0;   // Ring buffer write index
    size_t num_bands = CONTROLBUS_NUM_BANDS;  // Actual bands in use
    size_t frames_filled = 0;   // Tracks warmup period (0-3)
    bool enabled = true;        // Allow runtime disable for beat detection

    void init(size_t bands) {
        num_bands = (bands <= LOOKAHEAD_MAX_BANDS) ? bands : LOOKAHEAD_MAX_BANDS;
        for (size_t f = 0; f < LOOKAHEAD_FRAMES; ++f) {
            for (size_t b = 0; b < LOOKAHEAD_MAX_BANDS; ++b) {
                history[f][b] = 0.0f;
            }
        }
        current_frame = 0;
        frames_filled = 0;
    }

    void reset() {
        init(num_bands);
    }
};

/**
 * @brief Per-zone automatic gain control state
 *
 * Each zone tracks its own maximum magnitude and uses a smoothed follower
 * for normalization. This prevents loud bass from washing out mid/high content.
 *
 * Zone boundaries for 8-band system (2 bands per zone):
 *   Zone 0: bands 0-1  (60-120 Hz)   - Sub-bass/Bass
 *   Zone 1: bands 2-3  (250-500 Hz)  - Low-mid
 *   Zone 2: bands 4-5  (1-2 kHz)     - Mid/High-mid
 *   Zone 3: bands 6-7  (4-7.8 kHz)   - Presence/Brilliance
 *
 * For future 64-band system (16 bins per zone):
 *   Zone 0: bins 0-15  (110-207 Hz)
 *   Zone 1: bins 16-31 (220-415 Hz)
 *   Zone 2: bins 32-47 (440-830 Hz)
 *   Zone 3: bins 48-63 (880-4186 Hz)
 */
struct ZoneAGC {
    float max_mag = 0.0f;           ///< Current zone maximum magnitude
    float max_mag_follower = 1.0f;  ///< Smoothed maximum for normalization (starts at 1.0 to avoid initial spike)
    float attack_rate = 0.05f;      ///< How fast to increase follower (Sensory Bridge default)
    float release_rate = 0.05f;     ///< How fast to decrease follower (Sensory Bridge default)
    float min_floor = 0.01f;        ///< Minimum follower value (caps max gain at 100x, was 0.001f/1000x)

    void reset() {
        max_mag = 0.0f;
        max_mag_follower = 1.0f;
    }
};

/**
 * @brief Statistics for spike detection effectiveness tracking.
 * Enables before/after comparison to verify spike removal is helping.
 */
struct SpikeDetectionStats {
    uint32_t totalFrames = 0;          // Frames processed
    uint32_t spikesDetectedBands = 0;  // Direction changes detected in bands
    uint32_t spikesDetectedChroma = 0; // Direction changes detected in chroma
    uint32_t spikesCorrected = 0;      // Corrections applied (above threshold)
    float totalEnergyRemoved = 0.0f;   // Cumulative correction magnitude
    float avgSpikesPerFrame = 0.0f;    // Rolling average
    float avgCorrectionMagnitude = 0.0f; // Rolling average

    void reset() {
        totalFrames = 0;
        spikesDetectedBands = 0;
        spikesDetectedChroma = 0;
        spikesCorrected = 0;
        totalEnergyRemoved = 0.0f;
        avgSpikesPerFrame = 0.0f;
        avgCorrectionMagnitude = 0.0f;
    }
};

/**
 * @brief Smoothed DSP control signals at hop cadence.
 *
 * Audio thread owns a ControlBus instance; each hop:
 *  - UpdateFromHop(now, raw)
 *  - Publish(GetFrame()) to SnapshotBuffer<ControlBusFrame>
 *
 * Processing pipeline:
 *  1. Raw input clamped to 0..1
 *  2. Lookahead spike removal (2-frame delay, removes flicker)
 *  3. Asymmetric attack/release smoothing
 *  4. Output to frame
 */
class ControlBus {
public:
    ControlBus();

    void Reset();

    void UpdateFromHop(const AudioTime& now, const ControlBusRawInput& raw);

    ControlBusFrame GetFrame() const { return m_frame; }

    void setSmoothing(float alphaFast, float alphaSlow);
    void setAttackRelease(float bandAttack, float bandRelease,
                          float heavyBandAttack, float heavyBandRelease);
    float getAlphaFast() const { return m_alpha_fast; }
    float getAlphaSlow() const { return m_alpha_slow; }

    /**
     * @brief Set all smoothing parameters from mood value (Sensory Bridge pattern)
     * @param mood 0-255: 0=reactive (fast attack/slow decay), 255=smooth (slow attack/fast decay)
     *
     * Maps mood to both follower smoothing (attack/release) and exponential averaging.
     * Implements the Sensory Bridge MOOD knob behavior:
     * - Low mood: More responsive to transients, faster attack, slower decay
     * - High mood: More sustained, dreamier feel, slower attack, faster decay
     *
     * Affects:
     * - m_alpha_fast/slow: RMS/flux smoothing
     * - m_band_attack/release: Band energy smoothing
     * - m_heavy_band_attack/release: Extra-smooth band smoothing
     */
    void setMoodSmoothing(uint8_t mood);
    uint8_t getMood() const { return m_mood; }

    // Lookahead spike smoothing control
    void setLookaheadEnabled(bool enabled) { m_lookahead_bands.enabled = enabled; }
    bool getLookaheadEnabled() const { return m_lookahead_bands.enabled; }

    // Zone AGC control (Sensory Bridge pattern)
    void setZoneAGCEnabled(bool enabled) { m_zone_agc_enabled = enabled; }
    bool getZoneAGCEnabled() const { return m_zone_agc_enabled; }
    void setZoneAGCRates(float attack, float release);
    void setZoneMinFloor(float floor);

    // Chroma Zone AGC control (3 chroma bins per zone: C-D, D#-F, F#-G#, A-B)
    void setChromaZoneAGCEnabled(bool enabled) { m_chroma_zone_agc_enabled = enabled; }
    bool getChromaZoneAGCEnabled() const { return m_chroma_zone_agc_enabled; }
    void setChromaZoneAGCRates(float attack, float release);

    // Access zone state for debugging/visualization
    float getZoneFollower(uint8_t zone) const {
        return (zone < CONTROLBUS_NUM_ZONES) ? m_zones[zone].max_mag_follower : 1.0f;
    }
    float getZoneMaxMag(uint8_t zone) const {
        return (zone < CONTROLBUS_NUM_ZONES) ? m_zones[zone].max_mag : 0.0f;
    }
    float getChromaZoneFollower(uint8_t zone) const {
        return (zone < CONTROLBUS_NUM_ZONES) ? m_chroma_zones[zone].max_mag_follower : 1.0f;
    }
    float getChromaZoneMaxMag(uint8_t zone) const {
        return (zone < CONTROLBUS_NUM_ZONES) ? m_chroma_zones[zone].max_mag : 0.0f;
    }

    // Spike detection telemetry
    const SpikeDetectionStats& getSpikeStats() const { return m_spikeStats; }
    void resetSpikeStats() { m_spikeStats.reset(); }

    // Chord detection control (Priority 6: Musical intelligence from chromagram)
    void setChordDetectionEnabled(bool enabled) { m_chord_detection_enabled = enabled; }
    bool getChordDetectionEnabled() const { return m_chord_detection_enabled; }
    const ChordState& getChordState() const { return m_frame.chordState; }

    // Silence detection control (Sensory Bridge silent_scale pattern)
    void setSilenceParameters(float threshold, float hysteresisMs) {
        m_silence_threshold = threshold;
        m_silence_hysteresis_ms = hysteresisMs;
    }
    float getSilenceThreshold() const { return m_silence_threshold; }
    float getSilenceHysteresisMs() const { return m_silence_hysteresis_ms; }
    bool isSilenceEnabled() const { return m_silence_hysteresis_ms > 0.0f; }

private:
    ControlBusFrame m_frame{};

    // One-pole smoothing state
    float m_rms_s = 0.0f;
    float m_flux_s = 0.0f;
    float m_bands_s[CONTROLBUS_NUM_BANDS] = {0};
    float m_chroma_s[CONTROLBUS_NUM_CHROMA] = {0};
    // LGP_SMOOTH: Separate state for heavy (extra-smoothed) bands
    float m_heavy_bands_s[CONTROLBUS_NUM_BANDS] = {0};
    float m_heavy_chroma_s[CONTROLBUS_NUM_CHROMA] = {0};

    // Lookahead spike smoothing buffers (Sensory Bridge pattern)
    LookaheadBuffer m_lookahead_bands;
    LookaheadBuffer m_lookahead_chroma;

    // Intermediate buffer for spike-smoothed values (before attack/release)
    float m_bands_despiked[CONTROLBUS_NUM_BANDS] = {0};
    float m_chroma_despiked[CONTROLBUS_NUM_CHROMA] = {0};

    // Tunables (Phase-2 defaults; keep them stable until you port Tab5 constants)
    float m_alpha_fast = 0.35f;  // fast response
    float m_alpha_slow = 0.12f;  // slower response

    // LGP_SMOOTH: Asymmetric attack/release for bands
    float m_band_attack = 0.15f;       // Fast rise for transients
    float m_band_release = 0.03f;      // Slow fall for LGP viewing
    float m_heavy_band_attack = 0.08f; // Extra slow rise
    float m_heavy_band_release = 0.015f; // Ultra slow fall

    // Zone AGC state (Sensory Bridge pattern: 4 zones)
    bool m_zone_agc_enabled = true;  // Enabled by default for balanced frequency response
    ZoneAGC m_zones[CONTROLBUS_NUM_ZONES];

    // Chroma Zone AGC state (4 zones, 3 chroma bins per zone)
    // Zone 0: C, C#, D (0-2)   - low notes
    // Zone 1: D#, E, F (3-5)   - mid-low
    // Zone 2: F#, G, G# (6-8)  - mid-high
    // Zone 3: A, A#, B (9-11)  - high notes
    bool m_chroma_zone_agc_enabled = true;  // Enabled by default
    ZoneAGC m_chroma_zones[CONTROLBUS_NUM_ZONES];

    // Spike detection telemetry
    SpikeDetectionStats m_spikeStats;

    // Chord detection state (Priority 6)
    bool m_chord_detection_enabled = true;  // Enabled by default

    // Musical saliency tuning parameters
    SaliencyTuning m_saliencyTuning{};

    // Mood smoothing control (Sensory Bridge MOOD knob)
    uint8_t m_mood = 128;  // Default: balanced (0.5 normalized)

    // Silence detection state machine (Sensory Bridge silent_scale pattern)
    float m_silent_scale_smoothed = 1.0f;   ///< Smoothed output value
    uint32_t m_silence_start_ms = 0;        ///< When silence began (for hysteresis)
    bool m_silence_triggered = false;       ///< True when hysteresis exceeded
    float m_silence_threshold = 0.01f;      ///< RMS below this = silence
    float m_silence_hysteresis_ms = 0.0f;   ///< Time before fade begins (0 = disabled)

    // ========================================================================
    // Stack Reduction: Temporary buffers moved from stack to class members
    // ========================================================================
    
    /// Temporary buffer for clamped band values (moved from stack to reduce stack usage)
    float m_clamped_bands[CONTROLBUS_NUM_BANDS] = {0};
    
    /// Temporary buffer for clamped chroma values (moved from stack to reduce stack usage)
    float m_clamped_chroma[CONTROLBUS_NUM_CHROMA] = {0};

    // Private methods for spike detection
    void detectAndRemoveSpikes(LookaheadBuffer& buffer,
                               const float* input,
                               float* output,
                               size_t num_bands,
                               bool isBands);

    // Private method for chord detection
    void detectChord(const float* chroma);

    // Private method for saliency computation (Musical Intelligence System Phase 1)
    void computeSaliency();
};

} // namespace lightwaveos::audio
