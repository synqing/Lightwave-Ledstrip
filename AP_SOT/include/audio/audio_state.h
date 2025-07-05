/*
 * Audio State Interface for LGP Visualizer
 * =========================================
 * 
 * This header defines the complete audio pipeline output interface.
 * Agent B (Visual Pipeline) should include this file to access audio data.
 * 
 * DESIGN PHILOSOPHY:
 * - Core data is always available and updated at 100Hz
 * - Extended features can be enabled/disabled without breaking compatibility
 * - All values are normalized to 0.0-1.0 where possible for consistency
 * - No locking required - audio updates are atomic enough at our timescales
 * 
 * LGP CONTEXT:
 * - Zones 0-3: Edge 1 (Bass to Mid frequencies)
 * - Zones 4-7: Edge 2 (Mid to High frequencies)
 * - Light diffuses through the plate, creating natural blending
 */

#ifndef AUDIO_STATE_H
#define AUDIO_STATE_H

#include <stdint.h>

// Feature flags - which extended features are currently active
#define AUDIO_FEATURE_BEAT      (1 << 0)
#define AUDIO_FEATURE_SPECTRAL  (1 << 1)
#define AUDIO_FEATURE_DYNAMICS  (1 << 2)
#define AUDIO_FEATURE_ONSET     (1 << 3)
#define AUDIO_FEATURE_BALANCE   (1 << 4)
#define AUDIO_FEATURE_PITCH     (1 << 5)  // Future
#define AUDIO_FEATURE_STEREO    (1 << 6)  // Future

// Core audio data - ALWAYS AVAILABLE
typedef struct {
    // Raw frequency bin data - ALL 96 BINS
    // Updated every 8ms (125Hz) - 16kHz ÷ 128 samples
    // Range: 0.0 (silence) to 1.0 (maximum energy)
    // This is what Agent B is expecting!
    float audio_bins[96];
    
    // Pre-calculated zone energies for edge-lit LGP
    // Zones 0-3: Edge 1 (bass to mid)
    // Zones 4-7: Edge 2 (mid to treble)
    // Pre-scaled for perceptual loudness
    float zone_energies[8];
    
    // Global audio energy (RMS)
    // Useful for overall brightness effects, AGC, etc.
    // Range: 0.0 to 1.0
    float global_energy;
    
    // System health - true when audio is working
    // Check this before using audio data
    bool initialized;
    
    // Padding for alignment
    uint8_t _padding[3];
} AudioCore;

// Beat detection data
typedef struct {
    // Confidence that we're currently on a beat
    // 0.0 = no beat, 1.0 = definite beat
    float beat_confidence;
    
    // Current tempo in BPM
    // Typically 60-180, but can go outside this range
    float tempo_bpm;
    
    // Position within current beat cycle
    // 0.0 = beat just hit, 0.5 = halfway to next beat, 1.0 = next beat
    float beat_phase;
    
    // Milliseconds since last detected beat
    // Useful for creating decay effects
    uint32_t last_beat_ms;
    
    // Which frequency range triggered the beat
    // 0 = bass, 1 = low-mid, 2 = high-mid, 3 = treble
    uint8_t beat_band;
    
    // Padding for alignment
    uint8_t _padding[3];
    
    // Voice of God (VoG) Confidence Engine outputs
    float vog_confidence;     // Raw confidence score (0.0-1.0) from VoG algorithm
    float beat_hardness;      // Normalized beat intensity for visual modulation
} BeatData;

// Spectral characteristics
typedef struct {
    // Spectral centroid - where the "center of mass" of spectrum is
    // 0.0 = very bass heavy, 1.0 = very treble heavy
    // Indicates overall "brightness" of sound
    float spectral_centroid;
    
    // How spread out the frequency content is
    // 0.0 = narrow (like a pure tone), 1.0 = wide (like white noise)
    float spectral_spread;
    
    // How quickly the spectrum is changing
    // 0.0 = static, 1.0 = rapidly changing
    // Good for reactive effects
    float spectral_flux;
    
    // Zero crossing rate - indicates "noisiness"
    // 0.0 = smooth/tonal, 1.0 = noisy/harsh
    float zero_crossing_rate;
} SpectralFeatures;

// Dynamic range information
typedef struct {
    // Instantaneous peak level
    // Updates faster than RMS, good for transient effects
    float peak_level;
    
    // RMS level (same as global_energy, included for completeness)
    float rms_level;
    
    // Peak-to-RMS ratio
    // High values = punchy/dynamic, low values = compressed/steady
    float crest_factor;
    
    // Probability that we're in silence
    // 0.0 = definitely sound, 1.0 = definitely silence
    float silence_probability;
} DynamicsData;

// Onset (new sound event) detection
typedef struct {
    // True for one frame when new sound event detected
    bool onset_detected;
    
    // How strong the onset was
    // 0.0 = weak, 1.0 = very strong attack
    float onset_strength;
    
    // Which zone had the strongest onset
    // Useful for directional effects
    uint8_t onset_zone;
    
    // Padding
    uint8_t _padding;
    
    // Timestamp of onset for timing effects
    uint32_t onset_time_ms;
} OnsetData;

// Frequency band balance
typedef struct {
    // Relative energy in each band
    // Normalized so bass + mid + treble = 1.0
    float bass_ratio;    // Zones 0-1 combined
    float mid_ratio;     // Zones 2-5 combined  
    float treble_ratio;  // Zones 6-7 combined
    
    // Direct ratio for contrast effects
    // > 1.0 = bass heavy, < 1.0 = treble heavy
    float bass_to_treble;
} FrequencyBalance;

// Pitch detection (FUTURE FEATURE)
typedef struct {
    // MIDI note number of dominant pitch
    // 60 = Middle C, 69 = A440
    uint8_t dominant_note;
    
    // How confident we are in the pitch detection
    // 0.0 = no clear pitch, 1.0 = very clear pitch
    float note_confidence;
    
    // Pitch bend from exact note
    // -1.0 = one semitone flat, +1.0 = one semitone sharp
    float pitch_bend;
    
    // Padding
    uint8_t _padding;
} PitchData;

// Stereo field analysis (FUTURE FEATURE - requires 2nd mic)
typedef struct {
    // Stereo width
    // 0.0 = mono, 1.0 = wide stereo
    float stereo_width;
    
    // Left/right balance
    // -1.0 = hard left, 0.0 = center, +1.0 = hard right
    float left_right_balance;
    
    // Phase correlation between channels
    // 1.0 = perfectly in phase, -1.0 = inverted phase
    float correlation;
    
    // Padding for alignment
    uint32_t _padding;
} StereoData;

// Extended audio features - check feature_flags to see what's active
typedef struct {
    BeatData beat;
    SpectralFeatures spectral;
    DynamicsData dynamics;
    OnsetData onset;
    FrequencyBalance balance;
    
    // Future features - currently zeroed
    PitchData pitch;
    StereoData stereo;
} AudioExtended;

// Complete audio state structure
typedef struct {
    // Core data - always valid
    AudioCore core;
    
    // Extended features - check feature_flags
    AudioExtended ext;
    
    // Increments every time audio is updated
    // Can be used to detect new data
    uint32_t update_counter;
    
    // Which extended features are currently active
    // Check with AUDIO_FEATURE_* flags
    uint32_t feature_flags;
    
    // Timestamp of last update (milliseconds)
    uint32_t last_update_ms;
    
    // Reserved for future use
    uint32_t _reserved[5];
} AudioState;

// Global audio state instance
// Agent B should read from this structure
extern AudioState audio_state;

// Helper macros for Agent B
#define AUDIO_HAS_BEAT()     (audio_state.feature_flags & AUDIO_FEATURE_BEAT)
#define AUDIO_HAS_SPECTRAL() (audio_state.feature_flags & AUDIO_FEATURE_SPECTRAL)
#define AUDIO_HAS_DYNAMICS() (audio_state.feature_flags & AUDIO_FEATURE_DYNAMICS)
#define AUDIO_HAS_ONSET()    (audio_state.feature_flags & AUDIO_FEATURE_ONSET)
#define AUDIO_HAS_BALANCE()  (audio_state.feature_flags & AUDIO_FEATURE_BALANCE)

// Edge-specific helpers for LGP system
#define EDGE1_ENERGY() ((audio_state.core.zone_energies[0] + \
                         audio_state.core.zone_energies[1] + \
                         audio_state.core.zone_energies[2] + \
                         audio_state.core.zone_energies[3]) * 0.25f)

#define EDGE2_ENERGY() ((audio_state.core.zone_energies[4] + \
                         audio_state.core.zone_energies[5] + \
                         audio_state.core.zone_energies[6] + \
                         audio_state.core.zone_energies[7]) * 0.25f)

#endif // AUDIO_STATE_H