#include "lgp_synesthetic_orchestra_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"
#include <math.h>

#define MAX_INSTRUMENTS 8
#define HARMONIC_SERIES 12
#define COLOR_MEMORY_SIZE 32
#define TIMBRE_BANDS 6

// Musical note to color mapping (based on synesthesia research)
static const uint8_t NOTE_TO_HUE[12] = {
    0,    // C  - Red
    15,   // C# - Red-Orange  
    30,   // D  - Orange
    45,   // D# - Yellow-Orange
    60,   // E  - Yellow
    75,   // F  - Yellow-Green
    96,   // F# - Green
    120,  // G  - Blue-Green
    140,  // G# - Cyan
    160,  // A  - Blue
    200,  // A# - Purple
    240   // B  - Magenta
};

// Instrument timbre to color properties mapping
struct InstrumentProfile {
    uint8_t base_saturation;
    uint8_t brightness_range;
    float spatial_width;
    float attack_speed;
    float decay_rate;
    uint8_t harmonic_complexity;
    bool has_vibrato;
    float vibrato_rate;
};

// Recognized instrument profiles (via spectral analysis)
static const InstrumentProfile INSTRUMENT_PROFILES[MAX_INSTRUMENTS] = {
    {255, 200, 10.0f, 0.95f, 0.98f, 3, false, 0},     // Piano/Keyboard
    {220, 180, 15.0f, 0.80f, 0.95f, 5, true, 0.1f},   // Strings
    {200, 220, 20.0f, 0.99f, 0.90f, 8, false, 0},     // Brass
    {180, 200, 25.0f, 0.85f, 0.92f, 6, true, 0.15f},  // Woodwinds
    {240, 255, 5.0f,  0.99f, 0.85f, 2, false, 0},     // Percussion
    {230, 210, 18.0f, 0.70f, 0.96f, 4, true, 0.08f},  // Voice
    {255, 240, 30.0f, 0.60f, 0.99f, 10, true, 0.2f},  // Synthesizer
    {210, 190, 12.0f, 0.90f, 0.94f, 7, false, 0}      // Guitar
};

struct MusicalVoice {
    float fundamental_freq;
    float amplitude;
    float harmonic_content[HARMONIC_SERIES];
    uint8_t detected_note;
    uint8_t detected_octave;
    uint8_t instrument_type;
    float confidence;
    SQ15x16 position;
    SQ15x16 velocity;
    SQ15x16 envelope_phase;
    uint32_t onset_time;
    bool active;
    float pitch_bend;
    float vibrato_phase;
};

struct ColorMemory {
    uint8_t hue;
    uint8_t saturation;
    uint8_t brightness;
    float age;
    float importance;
};

static MusicalVoice voices[MAX_INSTRUMENTS];
static ColorMemory color_memory[COLOR_MEMORY_SIZE];
static float timbre_analysis[TIMBRE_BANDS];
static float spectral_centroid = 0;
static float spectral_flux = 0;
static float zero_crossing_rate = 0;
static float harmonic_to_noise_ratio = 0;

// Psychoacoustic analysis
static float consonance_level = 0;
static float dissonance_level = 0;
static float tonal_gravity_center = 0;

// Synesthetic visualization state
static SQ15x16 synesthetic_flow_phase = 0;
static float color_bleeding_amount = 0;
static float temporal_smearing = 0;
static bool grapheme_mode = false;  // Show note names as colors

// Helper function to detect musical notes from frequency
uint8_t frequency_to_note(float freq, uint8_t* octave) {
    if (freq < 20) return 0;
    
    float A4 = 440.0f;
    float C0 = A4 * pow(2, -4.75f);
    
    float half_steps = 12.0f * log2(freq / C0);
    *octave = (uint8_t)(half_steps / 12.0f);
    return (uint8_t)((int)round(half_steps) % 12);
}

// Analyze timbre characteristics
uint8_t detect_instrument_type(float* harmonic_content, float spectral_centroid, float attack_time) {
    uint8_t best_match = 0;
    float best_score = 0;
    
    // Simple heuristic-based instrument detection
    float odd_even_ratio = 0;
    for (int h = 1; h < HARMONIC_SERIES; h += 2) {
        odd_even_ratio += harmonic_content[h];
    }
    
    if (attack_time < 0.01f && spectral_centroid > 2000) {
        return 4; // Percussion
    } else if (odd_even_ratio > 0.7f && spectral_centroid < 1500) {
        return 2; // Brass (strong odd harmonics)
    } else if (harmonic_content[0] > 0.8f && harmonic_content[1] < 0.2f) {
        return 0; // Piano (strong fundamental)
    } else if (spectral_centroid > 1000 && spectral_centroid < 2000) {
        return 1; // Strings
    }
    
    return 6; // Default to synthesizer
}

void light_mode_lgp_synesthetic_orchestra() {
    cache_frame_config();
    
    // Advanced audio analysis
    get_smooth_spectrogram();
    get_smooth_chromagram();
    calculate_vu();
    
    // Calculate spectral features
    spectral_centroid = 0;
    float total_magnitude = 0;
    for (int i = 0; i < 96; i++) {
        float freq = i * 20.0f; // Approximate frequency mapping
        spectral_centroid += freq * spectrogram_smooth[i];
        total_magnitude += spectrogram_smooth[i];
    }
    if (total_magnitude > 0) {
        spectral_centroid /= total_magnitude;
    }
    
    // Calculate spectral flux (change over time)
    static float prev_spectrum[96] = {0};
    spectral_flux = 0;
    for (int i = 0; i < 96; i++) {
        float diff = spectrogram_smooth[i] - prev_spectrum[i];
        if (diff > 0) spectral_flux += diff;
        prev_spectrum[i] = spectrogram_smooth[i];
    }
    
    // Timbre analysis across frequency bands
    for (int band = 0; band < TIMBRE_BANDS; band++) {
        timbre_analysis[band] = 0;
        int start = band * 16;
        int end = start + 16;
        for (int i = start; i < end && i < 96; i++) {
            timbre_analysis[band] += spectrogram_smooth[i];
        }
        timbre_analysis[band] /= 16.0f;
    }
    
    // Calculate consonance/dissonance from chromagram
    consonance_level = 0;
    dissonance_level = 0;
    
    // Check for consonant intervals (octave, fifth, fourth, major third)
    consonance_level += chromagram_smooth[0] * chromagram_smooth[0];  // Octave
    consonance_level += chromagram_smooth[0] * chromagram_smooth[7];  // Fifth
    consonance_level += chromagram_smooth[0] * chromagram_smooth[5];  // Fourth
    consonance_level += chromagram_smooth[0] * chromagram_smooth[4];  // Major third
    
    // Check for dissonant intervals (minor second, tritone)
    dissonance_level += chromagram_smooth[0] * chromagram_smooth[1];  // Minor second
    dissonance_level += chromagram_smooth[0] * chromagram_smooth[6];  // Tritone
    
    // Find tonal center (strongest note in chromagram)
    float max_chroma = 0;
    int tonal_center = 0;
    for (int i = 0; i < 12; i++) {
        if (chromagram_smooth[i] > max_chroma) {
            max_chroma = chromagram_smooth[i];
            tonal_center = i;
        }
    }
    tonal_gravity_center = tonal_center;
    
    // Update synesthetic flow
    synesthetic_flow_phase += SQ15x16(0.01f + spectral_flux * 0.05f);
    color_bleeding_amount = consonance_level * 0.5f + 0.2f;
    temporal_smearing = (1.0f - dissonance_level) * 0.3f + 0.1f;
    
    // Voice detection and tracking
    static uint32_t voice_id_counter = 0;
    
    // Find dominant frequencies and create voices
    for (int v = 0; v < MAX_INSTRUMENTS; v++) {
        if (!voices[v].active) {
            // Look for new onset
            float peak_magnitude = 0;
            int peak_bin = 0;
            
            for (int i = 5; i < 80; i++) {  // Skip very low frequencies
                if (spectrogram_smooth[i] > peak_magnitude && spectrogram_smooth[i] > 0.1f) {
                    // Check if it's a local maximum
                    if (i > 0 && i < 95 &&
                        spectrogram_smooth[i] > spectrogram_smooth[i-1] &&
                        spectrogram_smooth[i] > spectrogram_smooth[i+1]) {
                        peak_magnitude = spectrogram_smooth[i];
                        peak_bin = i;
                    }
                }
            }
            
            if (peak_magnitude > 0.2f) {
                // New voice detected
                voices[v].active = true;
                voices[v].fundamental_freq = peak_bin * 20.0f;
                voices[v].amplitude = peak_magnitude;
                voices[v].onset_time = millis();
                
                // Detect note and octave
                uint8_t octave;
                voices[v].detected_note = frequency_to_note(voices[v].fundamental_freq, &octave);
                voices[v].detected_octave = octave;
                
                // Extract harmonic content
                for (int h = 0; h < HARMONIC_SERIES; h++) {
                    int harmonic_bin = peak_bin * (h + 1);
                    if (harmonic_bin < 96) {
                        voices[v].harmonic_content[h] = spectrogram_smooth[harmonic_bin];
                    } else {
                        voices[v].harmonic_content[h] = 0;
                    }
                }
                
                // Detect instrument type
                float attack_time = 0.05f; // Simplified for now
                voices[v].instrument_type = detect_instrument_type(
                    voices[v].harmonic_content, spectral_centroid, attack_time);
                
                // Set initial position based on frequency
                voices[v].position = SQ15x16(NATIVE_RESOLUTION) * 
                                   SQ15x16(voices[v].fundamental_freq / 2000.0f);
                voices[v].velocity = SQ15x16(0.5f - random8() / 512.0f);
                voices[v].envelope_phase = 0;
                voices[v].confidence = peak_magnitude;
                voices[v].pitch_bend = 0;
                voices[v].vibrato_phase = 0;
                
                break;
            }
        } else {
            // Update existing voice
            voices[v].envelope_phase += SQ15x16(0.02f);
            
            // Apply instrument envelope
            InstrumentProfile& profile = INSTRUMENT_PROFILES[voices[v].instrument_type];
            voices[v].amplitude *= profile.decay_rate;
            
            // Update position with instrument-specific movement
            voices[v].position += voices[v].velocity * SQ15x16(profile.spatial_width * 0.1f);
            
            // Add vibrato if applicable
            if (profile.has_vibrato) {
                voices[v].vibrato_phase += profile.vibrato_rate;
                voices[v].pitch_bend = sin(voices[v].vibrato_phase) * 0.1f;
            }
            
            // Wrap position
            if (voices[v].position < 0) voices[v].position += NATIVE_RESOLUTION;
            if (voices[v].position >= NATIVE_RESOLUTION) voices[v].position -= NATIVE_RESOLUTION;
            
            // Deactivate quiet voices
            if (voices[v].amplitude < 0.05f) {
                voices[v].active = false;
            }
        }
    }
    
    // Update color memory
    for (int m = 0; m < COLOR_MEMORY_SIZE; m++) {
        color_memory[m].age += 0.02f;
        color_memory[m].importance *= 0.98f;
    }
    
    // Render synesthetic visualization
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        CRGB16 pixel_color = CRGB16(0, 0, 0);
        float total_influence = 0;
        
        // Accumulate influence from all active voices
        for (int v = 0; v < MAX_INSTRUMENTS; v++) {
            if (!voices[v].active) continue;
            
            float dist = abs(i - voices[v].position.getInteger());
            InstrumentProfile& profile = INSTRUMENT_PROFILES[voices[v].instrument_type];
            
            // Instrument-specific spatial spread
            float spread = profile.spatial_width;
            if (dist < spread) {
                float influence = (1.0f - dist / spread) * voices[v].amplitude;
                
                // Base color from note
                uint8_t note_hue = NOTE_TO_HUE[voices[v].detected_note];
                
                // Modify hue based on octave (higher octaves = lighter/shifted hue)
                note_hue += voices[v].detected_octave * 5;
                
                // Add pitch bend effect
                note_hue += (int)(voices[v].pitch_bend * 30);
                
                // Saturation from instrument timbre
                uint8_t saturation = profile.base_saturation;
                
                // Brightness from amplitude and envelope
                uint8_t brightness = voices[v].amplitude * profile.brightness_range;
                
                // Apply synesthetic color bleeding
                if (color_bleeding_amount > 0) {
                    // Blend with neighboring notes
                    uint8_t prev_note = (voices[v].detected_note + 11) % 12;
                    uint8_t next_note = (voices[v].detected_note + 1) % 12;
                    
                    note_hue = note_hue * (1.0f - color_bleeding_amount) +
                              NOTE_TO_HUE[prev_note] * (color_bleeding_amount * 0.5f) +
                              NOTE_TO_HUE[next_note] * (color_bleeding_amount * 0.5f);
                }
                
                // Add temporal smearing (motion blur effect)
                for (int t = 1; t <= 3; t++) {
                    int smear_pos = i - (voices[v].velocity * t).getInteger();
                    if (smear_pos >= 0 && smear_pos < NATIVE_RESOLUTION) {
                        float smear_influence = influence * temporal_smearing / t;
                        total_influence += smear_influence;
                    }
                }
                
                // Create color
                CRGB16 voice_color = hsv_to_rgb_fast(note_hue, saturation, brightness);
                
                // Add instrument-specific effects
                if (voices[v].instrument_type == 4) { // Percussion
                    // Sharp attack flash
                    if (millis() - voices[v].onset_time < 50) {
                        voice_color = CRGB16(65535, 65535, 65535); // White flash
                    }
                } else if (voices[v].instrument_type == 1) { // Strings
                    // Shimmering effect
                    uint8_t shimmer = sin8(i * 10 + synesthetic_flow_phase.getInteger() * 5);
                    voice_color = scale_color(voice_color, SQ15x16(0.8f + shimmer / 1275.0f));
                }
                
                // Accumulate color
                pixel_color = add_clipped(pixel_color, scale_color(voice_color, SQ15x16(influence)));
                total_influence += influence;
            }
        }
        
        // Background ambience based on tonal center
        uint8_t ambient_hue = NOTE_TO_HUE[(int)tonal_gravity_center];
        uint8_t ambient_brightness = 20 + consonance_level * 30;
        CRGB16 ambient_color = hsv_to_rgb_fast(ambient_hue, 100, ambient_brightness);
        pixel_color = add_clipped(pixel_color, ambient_color);
        
        // Add harmonic resonance patterns
        float harmonic_wave = 0;
        for (int h = 0; h < 4; h++) {
            float freq = (h + 1) * 0.1f;
            harmonic_wave += sin(i * freq + synesthetic_flow_phase.getInteger() * 0.01f) * 
                            chromagram_smooth[h * 3 % 12];
        }
        
        if (harmonic_wave > 0.5f) {
            uint8_t resonance_hue = ambient_hue + 180; // Complementary color
            CRGB16 resonance_color = hsv_to_rgb_fast(resonance_hue, 150, harmonic_wave * 100);
            pixel_color = add_clipped(pixel_color, resonance_color);
        }
        
        // Dissonance creates visual distortion
        if (dissonance_level > 0.3f) {
            // Chromatic aberration effect
            int offset = (int)(dissonance_level * 5 * sin(i * 0.2f));
            if (i + offset >= 0 && i + offset < NATIVE_RESOLUTION) {
                pixel_color.r = (pixel_color.r + leds_16[i + offset].r) / 2;
            }
            if (i - offset >= 0 && i - offset < NATIVE_RESOLUTION) {
                pixel_color.b = (pixel_color.b + leds_16[i - offset].b) / 2;
            }
        }
        
        // Grapheme mode - show note names as distinct color blocks
        if (grapheme_mode && total_influence > 0.5f) {
            // Find strongest voice at this position
            int strongest_voice = -1;
            float max_influence = 0;
            for (int v = 0; v < MAX_INSTRUMENTS; v++) {
                if (!voices[v].active) continue;
                float dist = abs(i - voices[v].position.getInteger());
                if (dist < 5) {
                    float influence = voices[v].amplitude;
                    if (influence > max_influence) {
                        max_influence = influence;
                        strongest_voice = v;
                    }
                }
            }
            
            if (strongest_voice >= 0) {
                // Show pure note color
                uint8_t pure_hue = NOTE_TO_HUE[voices[strongest_voice].detected_note];
                pixel_color = hsv_to_rgb_fast(pure_hue, 255, 255);
            }
        }
        
        leds_16[i] = pixel_color;
    }
    
    // Add final effects
    
    // Spectral sparkles at high frequencies
    if (spectral_centroid > 1500) {
        for (int i = 0; i < NATIVE_RESOLUTION; i++) {
            if (random8() < spectral_flux * 50) {
                CRGB16 sparkle = hsv_to_rgb_fast(random8(), 100, 255);
                leds_16[i] = add_clipped(leds_16[i], sparkle);
            }
        }
    }
    
    // Bass pulse glow
    float bass_energy = 0;
    for (int i = 0; i < 10; i++) {
        bass_energy += spectrogram_smooth[i];
    }
    if (bass_energy > 0.5f) {
        for (int i = 0; i < NATIVE_RESOLUTION; i++) {
            CRGB16 bass_glow = hsv_to_rgb_fast(0, 255, bass_energy * 50);
            leds_16[i] = add_clipped(leds_16[i], bass_glow);
        }
    }
    
    apply_global_brightness();
}