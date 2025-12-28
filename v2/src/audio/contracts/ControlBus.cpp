#include "ControlBus.h"
#include <cstring>  // for memcpy

namespace lightwaveos::audio {

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

ControlBus::ControlBus() { Reset(); }

void ControlBus::Reset() {
    m_frame = ControlBusFrame{};
    m_rms_s = 0.0f;
    m_flux_s = 0.0f;
    for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        m_bands_s[i] = 0.0f;
        m_heavy_bands_s[i] = 0.0f;
        m_bands_despiked[i] = 0.0f;
    }
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        m_chroma_s[i] = 0.0f;
        m_heavy_chroma_s[i] = 0.0f;
        m_chroma_despiked[i] = 0.0f;
    }
    // Reset lookahead buffers
    m_lookahead_bands.init(CONTROLBUS_NUM_BANDS);
    m_lookahead_chroma.init(CONTROLBUS_NUM_CHROMA);
    // Reset Zone AGC state (both bands and chroma)
    for (uint8_t z = 0; z < CONTROLBUS_NUM_ZONES; ++z) {
        m_zones[z].reset();
        m_chroma_zones[z].reset();
    }
    // Reset spike detection telemetry
    m_spikeStats.reset();
    // Waveform array is zero-initialized by ControlBusFrame{} constructor
}

void ControlBus::setSmoothing(float alphaFast, float alphaSlow) {
    if (alphaFast < 0.0f) alphaFast = 0.0f;
    if (alphaFast > 1.0f) alphaFast = 1.0f;
    if (alphaSlow < 0.0f) alphaSlow = 0.0f;
    if (alphaSlow > 1.0f) alphaSlow = 1.0f;
    m_alpha_fast = alphaFast;
    m_alpha_slow = alphaSlow;
}

void ControlBus::setAttackRelease(float bandAttack, float bandRelease,
                                   float heavyBandAttack, float heavyBandRelease) {
    m_band_attack = clamp01(bandAttack);
    m_band_release = clamp01(bandRelease);
    m_heavy_band_attack = clamp01(heavyBandAttack);
    m_heavy_band_release = clamp01(heavyBandRelease);
}

void ControlBus::setZoneAGCRates(float attack, float release) {
    for (uint8_t z = 0; z < CONTROLBUS_NUM_ZONES; ++z) {
        m_zones[z].attack_rate = clamp01(attack);
        m_zones[z].release_rate = clamp01(release);
    }
}

void ControlBus::setZoneMinFloor(float floor) {
    float clamped = (floor < 0.0001f) ? 0.0001f : floor;  // Prevent division by zero
    for (uint8_t z = 0; z < CONTROLBUS_NUM_ZONES; ++z) {
        m_zones[z].min_floor = clamped;
        m_chroma_zones[z].min_floor = clamped;  // Apply to chroma zones too
    }
}

void ControlBus::setChromaZoneAGCRates(float attack, float release) {
    for (uint8_t z = 0; z < CONTROLBUS_NUM_ZONES; ++z) {
        m_chroma_zones[z].attack_rate = clamp01(attack);
        m_chroma_zones[z].release_rate = clamp01(release);
    }
}

/**
 * @brief Detect and remove single-frame spikes using 2-frame lookahead.
 *
 * Implements Sensory Bridge's spike detection algorithm:
 * - Maintains a 3-frame ring buffer (oldest, middle, newest)
 * - When middle frame changes direction from both neighbors (spike), replace with average
 * - Output is delayed by 2 frames to allow lookahead
 *
 * @param buffer   Ring buffer for history tracking
 * @param input    Current frame's raw values (clamped 0..1)
 * @param output   Despiked output values (2 frames delayed)
 * @param num_bands Number of bands to process
 * @param isBands  True if processing bands, false if processing chroma (for telemetry)
 */
void ControlBus::detectAndRemoveSpikes(LookaheadBuffer& buffer,
                                        const float* input,
                                        float* output,
                                        size_t num_bands,
                                        bool isBands) {
    // Handle disabled state - passthrough with no delay
    if (!buffer.enabled) {
        memcpy(output, input, num_bands * sizeof(float));
        return;
    }

    // Ring buffer indices (Sensory Bridge naming: past_index, look_ahead_1, look_ahead_2)
    // We write to 'newest' and output from 'oldest' (2-frame delay)
    const size_t newest = buffer.current_frame;
    const size_t middle = (buffer.current_frame + LOOKAHEAD_FRAMES - 1) % LOOKAHEAD_FRAMES;
    const size_t oldest = (buffer.current_frame + LOOKAHEAD_FRAMES - 2) % LOOKAHEAD_FRAMES;

    // Store current frame into newest slot
    for (size_t i = 0; i < num_bands; ++i) {
        buffer.history[newest][i] = input[i];
    }

    // Track warmup period - need 3 frames before spike detection is valid
    if (buffer.frames_filled < LOOKAHEAD_FRAMES) {
        buffer.frames_filled++;
        // During warmup, output zeros to avoid transient artifacts
        for (size_t i = 0; i < num_bands; ++i) {
            output[i] = 0.0f;
        }
        // Advance ring buffer
        buffer.current_frame = (buffer.current_frame + 1) % LOOKAHEAD_FRAMES;
        return;
    }

    // Telemetry counters for this frame
    uint32_t frameSpikesDetected = 0;
    uint32_t frameSpikesCorrected = 0;
    float frameEnergyRemoved = 0.0f;

    // Spike detection: check if middle frame is a spike relative to neighbors
    // A spike occurs when middle has opposite direction from both transitions
    for (size_t i = 0; i < num_bands; ++i) {
        const float oldest_val = buffer.history[oldest][i];
        const float middle_val = buffer.history[middle][i];
        const float newest_val = buffer.history[newest][i];

        // Direction detection (Sensory Bridge pattern)
        // look_ahead_1_rising: middle > oldest (rising into middle)
        // look_ahead_2_rising: newest > middle (rising out of middle)
        const bool rising_into_middle = middle_val > oldest_val;
        const bool rising_out_of_middle = newest_val > middle_val;

        // Spike detection: direction changes at middle frame
        // Peak:   rising_into AND falling_out  (middle is local maximum)
        // Trough: falling_into AND rising_out  (middle is local minimum)
        const bool is_direction_change = (rising_into_middle != rising_out_of_middle);

        if (is_direction_change) {
            // Track spike detection
            frameSpikesDetected++;

            // Calculate expected value (average of neighbors)
            const float expected = (oldest_val + newest_val) * 0.5f;

            // Only correct if deviation is significant
            // This prevents over-smoothing gradual changes
            const float deviation = (middle_val > expected) ?
                                    (middle_val - expected) : (expected - middle_val);

            // Threshold: 15% of expected value, with floor to handle near-zero values
            const float threshold_floor = 0.02f;  // Minimum threshold
            const float threshold = (expected * 0.15f > threshold_floor) ?
                                    expected * 0.15f : threshold_floor;

            if (deviation > threshold) {
                // Replace spike with average of neighbors
                buffer.history[middle][i] = expected;

                // Track correction telemetry
                frameSpikesCorrected++;
                frameEnergyRemoved += deviation;
            }
        }
    }

    // Update telemetry stats
    if (isBands) {
        m_spikeStats.spikesDetectedBands += frameSpikesDetected;
    } else {
        m_spikeStats.spikesDetectedChroma += frameSpikesDetected;
    }
    m_spikeStats.spikesCorrected += frameSpikesCorrected;
    m_spikeStats.totalEnergyRemoved += frameEnergyRemoved;

    // Rolling averages (EMA alpha=0.02)
    const float alpha = 0.02f;
    m_spikeStats.avgSpikesPerFrame = lerp(m_spikeStats.avgSpikesPerFrame,
                                           (float)frameSpikesDetected, alpha);
    if (frameSpikesCorrected > 0) {
        float avgMag = frameEnergyRemoved / (float)frameSpikesCorrected;
        m_spikeStats.avgCorrectionMagnitude = lerp(m_spikeStats.avgCorrectionMagnitude,
                                                    avgMag, alpha);
    }

    // Output the oldest frame (2-frame delay) after any spike corrections propagate
    // Note: corrections are applied to middle, which becomes oldest in 1 more frame
    // So we output oldest which has already been through spike detection
    for (size_t i = 0; i < num_bands; ++i) {
        output[i] = buffer.history[oldest][i];
    }

    // Advance ring buffer index
    buffer.current_frame = (buffer.current_frame + 1) % LOOKAHEAD_FRAMES;
}

void ControlBus::UpdateFromHop(const AudioTime& now, const ControlBusRawInput& raw) {
    m_frame.t = now;
    m_frame.hop_seq++;

    // ========================================================================
    // Stage 1: Clamp raw inputs to 0..1
    // ========================================================================
    m_frame.fast_rms = clamp01(raw.rms);
    m_rms_s = lerp(m_rms_s, m_frame.fast_rms, m_alpha_fast);
    m_frame.rms = m_rms_s;

    m_frame.fast_flux = clamp01(raw.flux);
    m_flux_s = lerp(m_flux_s, m_frame.fast_flux, m_alpha_slow);
    m_frame.flux = m_flux_s;

    // Temporary buffer for clamped band values
    float clamped_bands[CONTROLBUS_NUM_BANDS];
    for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        clamped_bands[i] = clamp01(raw.bands[i]);
    }

    // Temporary buffer for clamped chroma values
    float clamped_chroma[CONTROLBUS_NUM_CHROMA];
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        clamped_chroma[i] = clamp01(raw.chroma[i]);
    }

    // ========================================================================
    // Stage 2: Spike detection (lookahead smoothing)
    // Removes single-frame spikes that cause visual flicker
    // Output delayed by 2 frames (~32ms at 60fps)
    // ========================================================================
    detectAndRemoveSpikes(m_lookahead_bands, clamped_bands, m_bands_despiked, CONTROLBUS_NUM_BANDS, true);
    detectAndRemoveSpikes(m_lookahead_chroma, clamped_chroma, m_chroma_despiked, CONTROLBUS_NUM_CHROMA, false);

    // ========================================================================
    // Stage 3: Zone AGC (optional)
    // Normalizes each frequency zone independently to prevent bass dominance
    // Zone boundaries: 0-1 (sub-bass), 2-3 (low-mid), 4-5 (mid), 6-7 (high)
    // ========================================================================
    float normalized_bands[CONTROLBUS_NUM_BANDS];
    if (m_zone_agc_enabled) {
        // Update zone max magnitudes
        for (uint8_t z = 0; z < CONTROLBUS_NUM_ZONES; ++z) {
            // Find max in this zone (2 bands per zone for 8-band system)
            uint8_t start_band = z * 2;
            uint8_t end_band = start_band + 2;
            float zone_max = 0.0f;
            for (uint8_t i = start_band; i < end_band && i < CONTROLBUS_NUM_BANDS; ++i) {
                if (m_bands_despiked[i] > zone_max) {
                    zone_max = m_bands_despiked[i];
                }
            }
            m_zones[z].max_mag = zone_max;

            // Smoothed follower (Sensory Bridge pattern)
            if (zone_max > m_zones[z].max_mag_follower) {
                // Attack: rise toward new maximum
                float delta = zone_max - m_zones[z].max_mag_follower;
                m_zones[z].max_mag_follower += delta * m_zones[z].attack_rate;
            } else {
                // Release: slowly decay toward new (lower) maximum
                float delta = m_zones[z].max_mag_follower - zone_max;
                m_zones[z].max_mag_follower -= delta * m_zones[z].release_rate;
            }

            // Clamp follower to minimum floor
            if (m_zones[z].max_mag_follower < m_zones[z].min_floor) {
                m_zones[z].max_mag_follower = m_zones[z].min_floor;
            }

            // Normalize bands in this zone
            float norm_factor = 1.0f / m_zones[z].max_mag_follower;
            for (uint8_t i = start_band; i < end_band && i < CONTROLBUS_NUM_BANDS; ++i) {
                float normalized = m_bands_despiked[i] * norm_factor;
                normalized_bands[i] = clamp01(normalized);  // Clamp to prevent overshoot
            }
        }
    } else {
        // Zone AGC disabled - use despiked values directly
        for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
            normalized_bands[i] = m_bands_despiked[i];
        }
    }

    // ========================================================================
    // Stage 4: Asymmetric attack/release smoothing
    // Fast attack for transients, slow release for LGP viewing
    // ========================================================================
    for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        float target = normalized_bands[i];
        // Fast attack, slow release for normal bands
        float alpha = (target > m_bands_s[i]) ? m_band_attack : m_band_release;
        m_bands_s[i] = lerp(m_bands_s[i], target, alpha);
        m_frame.bands[i] = m_bands_s[i];

        // Extra-smoothed heavy bands for ambient effects
        float heavyAlpha = (target > m_heavy_bands_s[i]) ? m_heavy_band_attack : m_heavy_band_release;
        m_heavy_bands_s[i] = lerp(m_heavy_bands_s[i], target, heavyAlpha);
        m_frame.heavy_bands[i] = m_heavy_bands_s[i];
    }

    // ========================================================================
    // Stage 3b: Chroma Zone AGC (optional)
    // Normalizes each chroma zone independently (3 chroma bins per zone)
    // Zone 0: C,C#,D (0-2) | Zone 1: D#,E,F (3-5) | Zone 2: F#,G,G# (6-8) | Zone 3: A,A#,B (9-11)
    // ========================================================================
    float normalized_chroma[CONTROLBUS_NUM_CHROMA];
    if (m_chroma_zone_agc_enabled) {
        // Update chroma zone max magnitudes
        for (uint8_t z = 0; z < CONTROLBUS_NUM_ZONES; ++z) {
            // Find max in this zone (3 chroma bins per zone for 12-bin system)
            uint8_t start_bin = z * 3;
            uint8_t end_bin = start_bin + 3;
            float zone_max = 0.0f;
            for (uint8_t i = start_bin; i < end_bin && i < CONTROLBUS_NUM_CHROMA; ++i) {
                if (m_chroma_despiked[i] > zone_max) {
                    zone_max = m_chroma_despiked[i];
                }
            }
            m_chroma_zones[z].max_mag = zone_max;

            // Smoothed follower (same attack/release pattern as band Zone AGC)
            if (zone_max > m_chroma_zones[z].max_mag_follower) {
                // Attack: rise toward new maximum
                float delta = zone_max - m_chroma_zones[z].max_mag_follower;
                m_chroma_zones[z].max_mag_follower += delta * m_chroma_zones[z].attack_rate;
            } else {
                // Release: slowly decay toward new (lower) maximum
                float delta = m_chroma_zones[z].max_mag_follower - zone_max;
                m_chroma_zones[z].max_mag_follower -= delta * m_chroma_zones[z].release_rate;
            }

            // Clamp follower to minimum floor
            if (m_chroma_zones[z].max_mag_follower < m_chroma_zones[z].min_floor) {
                m_chroma_zones[z].max_mag_follower = m_chroma_zones[z].min_floor;
            }

            // Normalize chroma bins in this zone
            float norm_factor = 1.0f / m_chroma_zones[z].max_mag_follower;
            for (uint8_t i = start_bin; i < end_bin && i < CONTROLBUS_NUM_CHROMA; ++i) {
                float normalized = m_chroma_despiked[i] * norm_factor;
                normalized_chroma[i] = clamp01(normalized);  // Clamp to prevent overshoot
            }
        }
    } else {
        // Chroma Zone AGC disabled - use despiked values directly
        for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
            normalized_chroma[i] = m_chroma_despiked[i];
        }
    }

    // Chroma: Zone AGC normalized values with asymmetric smoothing
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        float target = normalized_chroma[i];
        float alpha = (target > m_chroma_s[i]) ? m_band_attack : m_band_release;
        m_chroma_s[i] = lerp(m_chroma_s[i], target, alpha);
        m_frame.chroma[i] = m_chroma_s[i];

        float heavyAlpha = (target > m_heavy_chroma_s[i]) ? m_heavy_band_attack : m_heavy_band_release;
        m_heavy_chroma_s[i] = lerp(m_heavy_chroma_s[i], target, heavyAlpha);
        m_frame.heavy_chroma[i] = m_heavy_chroma_s[i];
    }

    // ========================================================================
    // Stage 4b: Chord detection from chromagram (Priority 6)
    // Detects Major/Minor/Diminished/Augmented triads from pitch-class energy
    // ========================================================================
    if (m_chord_detection_enabled) {
        detectChord(m_frame.chroma);
    }

    // ========================================================================
    // Stage 5: Copy waveform data (no processing)
    // ========================================================================
    for (uint8_t i = 0; i < CONTROLBUS_WAVEFORM_N; ++i) {
        m_frame.waveform[i] = raw.waveform[i];
    }

    // ========================================================================
    // Stage 5b: Copy onset detection fields (Phase 1.2 - passthrough)
    // Snare/hi-hat detection performed upstream in GoertzelAnalyzer
    // ========================================================================
    m_frame.snareEnergy = clamp01(raw.snareEnergy);
    m_frame.hihatEnergy = clamp01(raw.hihatEnergy);
    m_frame.snareTrigger = raw.snareTrigger;
    m_frame.hihatTrigger = raw.hihatTrigger;

    // ========================================================================
    // Stage 5c: Copy 64-bin Goertzel spectrum (Phase 2 - passthrough)
    // Full spectrum available for visualizer effects
    // ========================================================================
    for (uint8_t i = 0; i < ControlBusRawInput::BINS_64_COUNT; ++i) {
        m_frame.bins64[i] = clamp01(raw.bins64[i]);
    }

    // ========================================================================
    // Stage 6: Update spike detection telemetry frame counter
    // ========================================================================
    m_spikeStats.totalFrames++;
}

/**
 * @brief Detect chord type from 12-bin chromagram.
 *
 * Algorithm (music theory based):
 * 1. Find dominant pitch class (root) as max energy bin
 * 2. Check intervals for third (+3 minor, +4 major) and fifth (+6 dim, +7 perfect, +8 aug)
 * 3. Classify as Major/Minor/Diminished/Augmented based on interval strengths
 * 4. Compute confidence as triad energy ratio of total chromagram energy
 *
 * @param chroma Pointer to 12-element chromagram array (C, C#, D, ..., B)
 */
void ControlBus::detectChord(const float* chroma) {
    ChordState& cs = m_frame.chordState;

    // 1. Find dominant pitch class (root candidate)
    uint8_t rootIdx = 0;
    float rootVal = chroma[0];
    float totalEnergy = chroma[0];
    for (uint8_t i = 1; i < CONTROLBUS_NUM_CHROMA; ++i) {
        totalEnergy += chroma[i];
        if (chroma[i] > rootVal) {
            rootVal = chroma[i];
            rootIdx = i;
        }
    }
    cs.rootNote = rootIdx;
    cs.rootStrength = rootVal;

    // 2. Check intervals (using modulo 12 for circular pitch space)
    uint8_t minorThirdIdx = (rootIdx + 3) % 12;
    uint8_t majorThirdIdx = (rootIdx + 4) % 12;
    uint8_t perfectFifthIdx = (rootIdx + 7) % 12;
    uint8_t dimFifthIdx = (rootIdx + 6) % 12;  // Tritone
    uint8_t augFifthIdx = (rootIdx + 8) % 12;

    float minorThird = chroma[minorThirdIdx];
    float majorThird = chroma[majorThirdIdx];
    float perfectFifth = chroma[perfectFifthIdx];
    float dimFifth = chroma[dimFifthIdx];
    float augFifth = chroma[augFifthIdx];

    // 3. Determine chord type based on strongest intervals
    bool hasMinorThird = minorThird > majorThird;
    cs.thirdStrength = hasMinorThird ? minorThird : majorThird;

    // Check which fifth is strongest
    if (perfectFifth >= dimFifth && perfectFifth >= augFifth) {
        cs.fifthStrength = perfectFifth;
        cs.type = hasMinorThird ? ChordType::MINOR : ChordType::MAJOR;
    } else if (dimFifth > perfectFifth && dimFifth > augFifth) {
        cs.fifthStrength = dimFifth;
        cs.type = ChordType::DIMINISHED;  // Diminished always has minor third
    } else {
        cs.fifthStrength = augFifth;
        cs.type = ChordType::AUGMENTED;   // Augmented always has major third
    }

    // 4. Compute confidence (triad energy / total energy)
    // A clean triad should have ~25% of total energy (3/12 bins)
    // Normalize so that a "perfect" triad ratio of 0.4 maps to 1.0
    float triadEnergy = cs.rootStrength + cs.thirdStrength + cs.fifthStrength;
    if (totalEnergy > 0.01f) {
        cs.confidence = clamp01((triadEnergy / totalEnergy) / 0.4f);
    } else {
        cs.confidence = 0.0f;
        cs.type = ChordType::NONE;
    }

    // If confidence is too low, classify as NONE
    if (cs.confidence < 0.3f) {
        cs.type = ChordType::NONE;
    }
}

} // namespace lightwaveos::audio
