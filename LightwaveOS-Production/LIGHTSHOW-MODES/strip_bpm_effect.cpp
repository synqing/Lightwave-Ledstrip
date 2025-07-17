#include "strip_bpm_effect.h"
#include "../src/globals.h"
#include "../src/GDFT.h"

// Advanced BPM detection state
static uint32_t last_beat_time = 0;
static uint32_t beat_interval = 500;  // Default 120 BPM
static float beat_confidence = 0;
static uint8_t beat_phase = 0;

// Advanced audio analysis state
static float tempo_prediction = 120.0f;  // Predicted tempo in BPM
static float harmonic_content[12] = {0};  // Harmonic analysis
static float rhythm_stability = 0.0f;     // How stable the rhythm is
static float beat_strength_history[8] = {0};  // Recent beat strengths
static uint8_t beat_history_index = 0;
static float spectral_centroid = 0.0f;    // Brightness measure
static float spectral_rolloff = 0.0f;     // High frequency content
static float zero_crossing_rate = 0.0f;   // Measure of noisiness
static uint32_t beat_intervals[16] = {0}; // Beat interval history
static uint8_t interval_index = 0;
static float onset_detection_threshold = 1.5f;

// Advanced spectral analysis
static void analyze_spectrum() {
    // Calculate spectral centroid (brightness)
    float weighted_sum = 0;
    float magnitude_sum = 0;
    
    for (int i = 0; i < NUM_FREQS; i++) {
        float magnitude = spectrogram_smooth[i];
        float frequency = (i * 22050.0f) / NUM_FREQS;  // Assuming 44.1kHz sample rate
        
        weighted_sum += magnitude * frequency;
        magnitude_sum += magnitude;
    }
    
    spectral_centroid = (magnitude_sum > 0.001f) ? (weighted_sum / magnitude_sum) : 0;
    
    // Calculate spectral rolloff (85% of energy)
    float cumulative_energy = 0;
    float total_energy = magnitude_sum;
    
    for (int i = 0; i < NUM_FREQS; i++) {
        cumulative_energy += spectrogram_smooth[i];
        if (cumulative_energy >= 0.85f * total_energy) {
            spectral_rolloff = (i * 22050.0f) / NUM_FREQS;
            break;
        }
    }
    
    // Estimate zero crossing rate from high frequency content
    float high_freq_energy = 0;
    for (int i = NUM_FREQS/2; i < NUM_FREQS; i++) {
        high_freq_energy += spectrogram_smooth[i];
    }
    zero_crossing_rate = high_freq_energy / (NUM_FREQS/2);
}

// Harmonic analysis
static void analyze_harmonics() {
    // Use chromagram data for harmonic analysis
    get_smooth_chromagram();
    
    for (int i = 0; i < 12; i++) {
        harmonic_content[i] = chromagram_smooth[i];
    }
    
    // Calculate harmonic richness
    float fundamental_strength = 0;
    float harmonic_strength = 0;
    
    for (int i = 0; i < 12; i++) {
        if (i == 0 || i == 7) {  // Root and fifth
            fundamental_strength += harmonic_content[i];
        } else {
            harmonic_strength += harmonic_content[i];
        }
    }
    
    // Update rhythm stability based on harmonic consistency
    float harmonic_stability = fundamental_strength / (fundamental_strength + harmonic_strength + 0.001f);
    rhythm_stability = rhythm_stability * 0.9f + harmonic_stability * 0.1f;
}

// Advanced tempo prediction
static void predict_tempo() {
    // Analyze beat interval history
    float interval_sum = 0;
    int valid_intervals = 0;
    
    for (int i = 0; i < 16; i++) {
        if (beat_intervals[i] > 0) {
            interval_sum += beat_intervals[i];
            valid_intervals++;
        }
    }
    
    if (valid_intervals > 4) {
        float avg_interval = interval_sum / valid_intervals;
        tempo_prediction = 60000.0f / avg_interval;  // Convert ms to BPM
        
        // Constrain to reasonable BPM range
        tempo_prediction = constrain(tempo_prediction, 60.0f, 200.0f);
        
        // Calculate rhythm stability based on interval variance
        float variance = 0;
        for (int i = 0; i < 16; i++) {
            if (beat_intervals[i] > 0) {
                float diff = beat_intervals[i] - avg_interval;
                variance += diff * diff;
            }
        }
        variance /= valid_intervals;
        
        // Lower variance = higher stability
        rhythm_stability = 1.0f / (1.0f + variance / 10000.0f);
    }
}

// Enhanced beat detection with onset detection
static void detect_beat() {
    get_smooth_spectrogram();
    analyze_spectrum();
    analyze_harmonics();
    
    // Multi-band onset detection
    float low_energy = 0;    // 0-200Hz
    float mid_energy = 0;    // 200-2000Hz  
    float high_energy = 0;   // 2000Hz+
    
    int low_bins = (200 * NUM_FREQS) / 22050;
    int mid_bins = (2000 * NUM_FREQS) / 22050;
    
    for (int i = 0; i < min(low_bins, NUM_FREQS); i++) {
        low_energy += spectrogram_smooth[i];
    }
    low_energy /= low_bins;
    
    for (int i = low_bins; i < min(mid_bins, NUM_FREQS); i++) {
        mid_energy += spectrogram_smooth[i];
    }
    mid_energy /= (mid_bins - low_bins);
    
    for (int i = mid_bins; i < NUM_FREQS; i++) {
        high_energy += spectrogram_smooth[i];
    }
    high_energy /= (NUM_FREQS - mid_bins);
    
    // Weighted onset detection
    float onset_strength = (low_energy * 0.6f) + (mid_energy * 0.3f) + (high_energy * 0.1f);
    
    // Adaptive threshold with spectral features
    static float onset_avg = 0;
    static float onset_variance = 0;
    
    onset_avg = onset_avg * 0.95f + onset_strength * 0.05f;
    float onset_diff = onset_strength - onset_avg;
    onset_variance = onset_variance * 0.95f + (onset_diff * onset_diff) * 0.05f;
    
    float adaptive_threshold = onset_avg + (sqrt(onset_variance) * onset_detection_threshold);
    
    // Beat detection with multiple criteria
    uint32_t now = millis();
    bool energy_trigger = onset_strength > adaptive_threshold;
    bool timing_valid = (now - last_beat_time) > 100;  // Min 100ms between beats
    bool spectral_trigger = (spectral_centroid > 1000) && (spectral_rolloff > 2000);
    
    if (energy_trigger && timing_valid) {
        // Store beat strength for analysis
        beat_strength_history[beat_history_index] = onset_strength;
        beat_history_index = (beat_history_index + 1) % 8;
        
        // Update beat interval history
        if (last_beat_time > 0) {
            uint32_t new_interval = now - last_beat_time;
            if (new_interval > 200 && new_interval < 2000) {
                beat_intervals[interval_index] = new_interval;
                interval_index = (interval_index + 1) % 16;
                
                // Update current beat interval with temporal smoothing
                beat_interval = beat_interval * 0.7f + new_interval * 0.3f;
                beat_confidence = min(1.0f, beat_confidence + 0.15f);
            }
        }
        
        last_beat_time = now;
        beat_phase = 0;
        
        // Adaptive threshold adjustment
        onset_detection_threshold = 1.2f + (beat_confidence * 0.5f) + (rhythm_stability * 0.3f);
    } else {
        // Decay confidence
        beat_confidence *= 0.999f;
    }
    
    // Update tempo prediction
    predict_tempo();
}

void light_mode_strip_bpm() {
    cache_frame_config();
    get_smooth_spectrogram();
    
    // Update advanced beat detection and analysis
    detect_beat();
    
    // Calculate current beat phase (0-255)
    uint32_t time_since_beat = millis() - last_beat_time;
    if (time_since_beat < beat_interval) {
        beat_phase = (time_since_beat * 255) / beat_interval;
    } else {
        beat_phase = 255;
    }
    
    // Advanced beat-synchronized wave with harmonic content
    uint8_t beat_wave = sin8(beat_phase);
    uint8_t inverted_beat = 255 - beat_wave;
    
    // Harmonic modulation based on musical content
    float harmonic_modulation = 0;
    for (int i = 0; i < 12; i++) {
        harmonic_modulation += harmonic_content[i] * sin((i * beat_phase * 6.28f) / 255.0f);
    }
    harmonic_modulation /= 12.0f;
    
    // Tempo-synchronized traveling wave
    static uint16_t wave_position = 0;
    float tempo_factor = tempo_prediction / 120.0f;  // Normalize to 120 BPM
    wave_position += (frame_config.SPEED / 4) * tempo_factor;
    
    // Spectral brightness affects wave behavior
    float spectral_brightness = spectral_centroid / 5000.0f;  // Normalize
    spectral_brightness = constrain(spectral_brightness, 0.0f, 1.0f);
    
    // Render the effect
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        // Distance from center
        SQ15x16 dist = abs(SQ15x16(i) - SQ15x16(NATIVE_RESOLUTION/2));
        uint16_t dist_int = dist.getInteger();
        
        // Multiple wave layers with harmonic analysis
        uint8_t wave1 = sin8((dist_int * 4) + wave_position);
        uint8_t wave2 = sin8((dist_int * 2) - wave_position/2);
        
        // Harmonic wave layer based on musical content
        uint8_t harmonic_wave = 0;
        for (int h = 0; h < 6; h++) {
            float harmonic_strength = harmonic_content[h * 2];
            harmonic_wave += sin8((dist_int * (h + 1)) + wave_position * (h + 1)) * harmonic_strength;
        }
        harmonic_wave /= 6;
        
        // Combine waves with beat modulation and harmonic content
        uint8_t combined = (wave1 + wave2 + harmonic_wave) / 3;
        combined = scale8(combined, beat_wave);
        
        // Spectral brightness affects wave intensity
        combined = scale8(combined, 128 + (spectral_brightness * 127));
        
        // Enhanced audio reactivity with spectral analysis
        uint8_t freq_index = (i * NUM_FREQS) / NATIVE_RESOLUTION;
        float freq_energy = spectrogram_smooth[freq_index];
        
        // Weight by spectral characteristics
        float spectral_weight = 1.0f;
        if (freq_index < NUM_FREQS/4) {
            spectral_weight = 1.0f + (rhythm_stability * 0.5f);  // Bass weighted by rhythm stability
        } else if (freq_index > NUM_FREQS/2) {
            spectral_weight = 1.0f + (zero_crossing_rate * 0.3f);  // High freq weighted by noisiness
        }
        
        uint8_t audio_brightness = (freq_energy * spectral_weight) * 127;
        
        // Add onset detection boost
        if ((millis() - last_beat_time) < 100) {
            audio_brightness = qadd8(audio_brightness, 64);
        }
        
        // Mix beat-synced wave with enhanced audio visualization
        uint8_t final_brightness = qadd8(combined, audio_brightness);
        
        // Tempo-reactive density control
        float tempo_density_factor = 1.0f;
        if (tempo_prediction > 140) {
            tempo_density_factor = 1.2f;  // Sharper waves for fast tempo
        } else if (tempo_prediction < 80) {
            tempo_density_factor = 0.8f;  // Softer waves for slow tempo
        }
        
        uint8_t effective_density = frame_config.DENSITY * tempo_density_factor;
        
        // Apply density for wave thickness with harmonic modulation
        if (effective_density > 128) {
            // Thick waves with harmonic texture
            uint8_t thickness_boost = scale8(inverted_beat, effective_density - 128);
            thickness_boost = scale8(thickness_boost, 200 + (harmonic_modulation * 55));
            final_brightness = qadd8(final_brightness, thickness_boost);
        } else {
            // Thin waves - apply cutoff with spectral considerations
            uint8_t cutoff_threshold = 255 - effective_density * 2;
            cutoff_threshold = scale8(cutoff_threshold, 200 + (spectral_brightness * 55));
            if (final_brightness < cutoff_threshold) {
                final_brightness = 0;
            }
        }
        
        // Advanced color generation with harmonic analysis
        CRGB16 color;
        if (frame_config.COLOR_MODE == COLOR_MODE_PALETTE) {
            uint8_t palette_index = beat_phase/2 + (dist_int / 2);
            
            // Harmonic content affects palette progression
            for (int h = 0; h < 6; h++) {
                palette_index += harmonic_content[h] * (h + 1) * 4;
            }
            
            // Spectral brightness affects palette selection
            palette_index += spectral_brightness * 32;
            
            color = palette_to_crgb16(palette_arr[frame_config.PALETTE],
                                    palette_index, final_brightness);
        } else if (frame_config.COLOR_MODE == COLOR_MODE_HYBRID) {
            // Hybrid mode with harmonic hue shifts
            uint8_t hue = frame_config.HUE + (beat_wave / 4) + (dist_int / 4);
            
            // Harmonic content creates musical color progressions
            for (int h = 0; h < 12; h++) {
                hue += harmonic_content[h] * (h * 8);  // Musical intervals
            }
            
            // Spectral centroid affects hue brightness
            hue += spectral_brightness * 24;
            
            // Tempo affects saturation
            uint8_t dynamic_saturation = frame_config.SATURATION;
            dynamic_saturation = scale8(dynamic_saturation, 200 + (tempo_prediction / 4));
            
            color = hsv_to_rgb_fast(hue, dynamic_saturation, final_brightness);
        } else {
            // Pure HSV with advanced audio analysis
            uint8_t hue = frame_config.HUE + (dist_int / 4);
            
            // Harmonic content creates hue variations
            for (int h = 0; h < 6; h++) {
                hue += harmonic_content[h] * (h * 6);
            }
            
            // Spectral features affect color
            hue += spectral_brightness * 16;
            
            // Rhythm stability affects saturation
            uint8_t dynamic_saturation = frame_config.SATURATION;
            dynamic_saturation = scale8(dynamic_saturation, 200 + (rhythm_stability * 55));
            
            color = get_mode_color(hue, dynamic_saturation, final_brightness);
        }
        
        // Advanced beat flash with harmonic enhancement
        if (time_since_beat < 50 && beat_confidence > 0.5f) {
            float flash_intensity = 1.5f + (rhythm_stability * 0.5f);
            
            // Harmonic content affects flash color
            float harmonic_flash_factor = 1.0f;
            for (int h = 0; h < 6; h++) {
                harmonic_flash_factor += harmonic_content[h] * 0.1f;
            }
            
            color = scale_color(color, SQ15x16(flash_intensity * harmonic_flash_factor));
        }
        
        // Spectral brightness creates subtle shimmer
        if (spectral_brightness > 0.7f) {
            float shimmer = 1.0f + (sin8((time_since_beat * 8) + (i * 16)) / 512.0f);
            color = scale_color(color, SQ15x16(shimmer));
        }
        
        leds_16[i] = color;
    }
    
    // Apply global brightness
    apply_global_brightness();
}