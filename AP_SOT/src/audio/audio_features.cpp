/*
 * Audio Feature Extraction Engine
 * ================================
 * Professional-grade audio analysis for the LGP visualizer
 * Extracts all useful features from audio signal at 125Hz
 * 
 * DEPRECATION NOTICE
 * ==================
 * This file is part of the legacy monolithic audio pipeline.
 * It will be replaced by the pluggable node architecture.
 * 
 * Replacement: zone_mapper_node.h + audio node metadata
 * Migration: See DEPRECATION_TRACKER.md
 * Target removal: After Phase 3 completion
 * 
 * DO NOT ADD NEW FEATURES TO THIS FILE
 */

#include "audio/audio_features.h"
#include "audio/multiband_agc_system.h"
#include <Arduino.h>
#include <math.h>

// Static variables for feature extraction
static float prev_spectrum[96] = {0};
static float spectral_flux_history[16] = {0};
static uint8_t flux_history_index = 0;

// Beat detection state
static float beat_energy_history[43] = {0};  // ~344ms at 125Hz
static uint8_t beat_history_index = 0;
static float beat_variance = 0.0f;
static float beat_threshold = 1.5f;
static uint32_t last_beat_time = 0;
static float tempo_candidates[4] = {0};
static uint32_t tempo_last_beats[4] = {0};

// Multiband AGC system - the heart of cochlear audio processing
static MultibandAGCSystem multiband_agc;
static float agc_processed_bins[96] = {0};  // AGC-processed frequency bins
static float raw_frequency_bins[96] = {0};  // RAW bins for beat detection

// Onset detection state  
static float onset_threshold = 0.3f;
static float prev_total_energy = 0.0f;

void AudioFeatures::init() {
    // Reset all history buffers
    memset(prev_spectrum, 0, sizeof(prev_spectrum));
    memset(spectral_flux_history, 0, sizeof(spectral_flux_history));
    memset(beat_energy_history, 0, sizeof(beat_energy_history));
    
    // Initialize the multiband AGC system - COCHLEAR PROCESSING!
    multiband_agc.init(16000.0f);  // 16kHz sample rate
    Serial.println("AudioFeatures: Multiband AGC initialized");
    
    // Debug AGC band information
    Serial.println("AGC Band Mapping:");
    for (int band = 0; band < 4; band++) {
        float gain, energy, ceiling;
        multiband_agc.getBandInfo(band, gain, energy, ceiling);
        Serial.printf("  Band %d: gain=%.2f, energy=%.2f, ceiling=%.2f\n", 
                     band, gain, energy, ceiling);
    }
    
    // Initialize feature flags - ENABLE BEAT DETECTION!
    audio_state.feature_flags = AUDIO_FEATURE_SPECTRAL | 
                               AUDIO_FEATURE_DYNAMICS |
                               AUDIO_FEATURE_BALANCE |
                               AUDIO_FEATURE_BEAT;  // CRITICAL: Enable beat tracking!
}

void AudioFeatures::updateCore(float* frequency_bins, int num_bins) {
    // ========================================================================
    // CRITICAL ARCHITECTURE - DO NOT MODIFY WITHOUT UNDERSTANDING!
    // ========================================================================
    // We maintain DUAL data paths to solve the AGC/Beat Detection conflict:
    // 
    // 1. RAW PATH: Goertzel → Beat Detection 
    //    - Preserves dynamic range information
    //    - Beat detection REQUIRES raw dynamics to detect transients
    //    - AGC destroys this information by normalizing everything
    //
    // 2. AGC PATH: Goertzel → Multiband AGC → Visual Effects
    //    - Normalizes frequency content for consistent visuals
    //    - 4-band cochlear processing for perceptual balance
    //    - Prevents quiet frequencies from disappearing
    //
    // HISTORY: Previous implementations put AGC before beat detection,
    // which completely broke beat tracking. DO NOT MAKE THIS MISTAKE AGAIN!
    // ========================================================================
    
    // Store RAW frequency bins for beat detection (NEVER NORMALIZE THESE!)
    memcpy(raw_frequency_bins, frequency_bins, sizeof(float) * min(num_bins, 96));
    
    // Calculate global energy from RAW Goertzel output (for beat detection)
    float total_energy = 0.0f;
    for (int i = 0; i < num_bins; i++) {
        total_energy += frequency_bins[i] * frequency_bins[i];
    }
    float raw_rms = sqrtf(total_energy / num_bins);
    
    // Store RAW energy for beat detection (NO NORMALIZATION!)
    // Beat detector expects values in Goertzel magnitude range (0-10000)
    audio_state.core.global_energy = raw_rms;  // Keep raw for beat detector
    
    // Process through Multiband AGC for visualization ONLY
    // This sophisticated cochlear-inspired AGC system:
    // - Divides spectrum into 4 perceptual bands (bass/low-mid/high-mid/treble)
    // - Each band has independent gain control with musical time constants
    // - Cross-band coupling prevents "swimming" artifacts
    // - Dynamic range tracking adapts to content
    bool is_silence = (raw_rms < 50.0f);  // Silence threshold
    multiband_agc.process(frequency_bins, agc_processed_bins, num_bins, is_silence);
    
    // Copy AGC-processed bins to audio_state for VISUAL EFFECTS ONLY!
    // Zone calculations and visual effects use these normalized values
    memcpy(audio_state.core.audio_bins, agc_processed_bins, 
           sizeof(float) * min(num_bins, 96));
    
    // Debug the dual data paths and AGC output
    static int debug_count = 0;
    if (++debug_count % 100 == 0) {
        // Calculate AGC output statistics
        float agc_min = 999999.0f, agc_max = 0.0f, agc_avg = 0.0f;
        for (int i = 0; i < num_bins; i++) {
            if (agc_processed_bins[i] < agc_min) agc_min = agc_processed_bins[i];
            if (agc_processed_bins[i] > agc_max) agc_max = agc_processed_bins[i];
            agc_avg += agc_processed_bins[i];
        }
        agc_avg /= num_bins;
        
        Serial.printf("AUDIO PATH: raw_rms=%.1f | AGC: min=%.2f, max=%.2f, avg=%.2f\n",
                     raw_rms, agc_min, agc_max, agc_avg);
        
        // Show first few bins for both paths
        Serial.printf("  RAW[0-3]: %.1f, %.1f, %.1f, %.1f\n",
                     frequency_bins[0], frequency_bins[1], frequency_bins[2], frequency_bins[3]);
        Serial.printf("  AGC[0-3]: %.1f, %.1f, %.1f, %.1f\n",
                     agc_processed_bins[0], agc_processed_bins[1], agc_processed_bins[2], agc_processed_bins[3]);
    }
    
    // Update zone energies using AGC-processed data for visualization
    updateZoneEnergies();
    
    // Update timestamp and counter
    audio_state.last_update_ms = millis();
    audio_state.update_counter++;
}

void AudioFeatures::updateZoneEnergies() {
    // RE-ENABLED with proper normalization for SpectraSynq platform
    float* bins = audio_state.core.audio_bins;
    float* zones = audio_state.core.zone_energies;
    
    // Zero out zones
    for (int z = 0; z < 8; z++) {
        zones[z] = 0.0f;
    }
    
    // Calculate zone energies WITHOUT normalization first
    // Musical scale aware zone mapping - each octave has 12 bins (one per semitone)
    float raw_zones[8] = {0};
    
    // Bass zones (0-1): 27.5-110Hz
    for (int i = 0; i < 12; i++) raw_zones[0] += bins[i];
    raw_zones[0] = raw_zones[0] / 12.0f;
    
    for (int i = 12; i < 24; i++) raw_zones[1] += bins[i];
    raw_zones[1] = raw_zones[1] / 12.0f;
    
    // Mid zones (2-5): 110-880Hz
    for (int i = 24; i < 36; i++) raw_zones[2] += bins[i];
    raw_zones[2] = raw_zones[2] / 12.0f;
    
    for (int i = 36; i < 48; i++) raw_zones[3] += bins[i];
    raw_zones[3] = raw_zones[3] / 12.0f;
    
    for (int i = 48; i < 60; i++) raw_zones[4] += bins[i];
    raw_zones[4] = raw_zones[4] / 12.0f;
    
    for (int i = 60; i < 72; i++) raw_zones[5] += bins[i];
    raw_zones[5] = raw_zones[5] / 12.0f;
    
    // High zones (6-7): 1760-7040Hz
    for (int i = 72; i < 84; i++) raw_zones[6] += bins[i];
    raw_zones[6] = raw_zones[6] / 12.0f;
    
    for (int i = 84; i < 96; i++) raw_zones[7] += bins[i];
    raw_zones[7] = raw_zones[7] / 12.0f;
    
    // Apply perceptual boost factors
    const float boost_factors[8] = {
        2.0f,   // Zone 0: Bass boost
        1.5f,   // Zone 1: Low-mid boost
        1.0f,   // Zone 2: No boost
        1.0f,   // Zone 3: No boost
        1.0f,   // Zone 4: No boost
        1.0f,   // Zone 5: No boost
        1.2f,   // Zone 6: High boost
        1.5f    // Zone 7: Treble boost
    };
    
    // Find max zone value AFTER boost for proper normalization
    float max_zone = 0.0f;
    for (int z = 0; z < 8; z++) {
        float boosted = raw_zones[z] * boost_factors[z];
        if (boosted > max_zone) {
            max_zone = boosted;
        }
    }
    
    // Normalize all zones together to maintain relative levels
    float zone_norm = (max_zone > 0.01f) ? (0.95f / max_zone) : 1.0f;
    
    // Apply normalization and boost together
    for (int z = 0; z < 8; z++) {
        zones[z] = raw_zones[z] * boost_factors[z] * zone_norm;
        zones[z] = constrain(zones[z], 0.0f, 1.0f);
    }
    
    // Debug zone calculation every 2 seconds
    static int zone_debug = 0;
    if (++zone_debug % 250 == 0) {
        Serial.printf("ZONES: max_zone=%.2f, norm=%.4f | ", max_zone, zone_norm);
        Serial.printf("Z[0-3]=%.2f,%.2f,%.2f,%.2f | Z[4-7]=%.2f,%.2f,%.2f,%.2f\n",
                     zones[0], zones[1], zones[2], zones[3],
                     zones[4], zones[5], zones[6], zones[7]);
    }
}

void AudioFeatures::updateSpectral() {
    float* bins = audio_state.core.audio_bins;
    SpectralFeatures* spec = &audio_state.ext.spectral;
    
    // Calculate spectral centroid (center of mass)
    float weighted_sum = 0.0f;
    float magnitude_sum = 0.0f;
    
    for (int i = 0; i < 96; i++) {
        weighted_sum += bins[i] * i;
        magnitude_sum += bins[i];
    }
    
    if (magnitude_sum > 0.001f) {
        spec->spectral_centroid = weighted_sum / (magnitude_sum * 96.0f);
    } else {
        spec->spectral_centroid = 0.0f;
    }
    
    // Calculate spectral spread (variance around centroid)
    if (magnitude_sum > 0.001f) {
        float variance = 0.0f;
        float cent_bin = spec->spectral_centroid * 96.0f;
        
        for (int i = 0; i < 96; i++) {
            float diff = i - cent_bin;
            variance += bins[i] * diff * diff;
        }
        
        spec->spectral_spread = sqrtf(variance / magnitude_sum) / 48.0f;  // Normalize
        spec->spectral_spread = constrain(spec->spectral_spread, 0.0f, 1.0f);
    } else {
        spec->spectral_spread = 0.0f;
    }
    
    // Calculate spectral flux (change rate)
    float flux = 0.0f;
    for (int i = 0; i < 96; i++) {
        float diff = bins[i] - prev_spectrum[i];
        if (diff > 0) {  // Only positive changes (onsets)
            flux += diff;
        }
        prev_spectrum[i] = bins[i];
    }
    
    // Add to flux history for smoothing
    spectral_flux_history[flux_history_index] = flux;
    flux_history_index = (flux_history_index + 1) % 16;
    
    // Average flux over history
    float avg_flux = 0.0f;
    for (int i = 0; i < 16; i++) {
        avg_flux += spectral_flux_history[i];
    }
    spec->spectral_flux = constrain(avg_flux / 16.0f, 0.0f, 1.0f);
    
    // Zero crossing rate (simplified - using spectral roughness instead)
    float roughness = 0.0f;
    for (int i = 1; i < 95; i++) {
        float local_var = fabsf(bins[i] - (bins[i-1] + bins[i+1]) * 0.5f);
        roughness += local_var;
    }
    spec->zero_crossing_rate = constrain(roughness / 48.0f, 0.0f, 1.0f);
}

void AudioFeatures::updateDynamics() {
    DynamicsData* dyn = &audio_state.ext.dynamics;
    float* bins = audio_state.core.audio_bins;
    
    // Find peak level
    float peak = 0.0f;
    for (int i = 0; i < 96; i++) {
        if (bins[i] > peak) {
            peak = bins[i];
        }
    }
    dyn->peak_level = peak;
    
    // RMS is already calculated
    dyn->rms_level = audio_state.core.global_energy;
    
    // Crest factor (peak to RMS ratio)
    if (dyn->rms_level > 0.001f) {
        dyn->crest_factor = dyn->peak_level / dyn->rms_level;
        dyn->crest_factor = constrain(dyn->crest_factor, 1.0f, 10.0f) / 10.0f;
    } else {
        dyn->crest_factor = 1.0f;
    }
    
    // Silence probability
    float silence_thresh = 0.01f;
    if (dyn->rms_level < silence_thresh) {
        dyn->silence_probability = 1.0f - (dyn->rms_level / silence_thresh);
    } else {
        dyn->silence_probability = 0.0f;
    }
}

void AudioFeatures::updateBeat() {
    if (!(audio_state.feature_flags & AUDIO_FEATURE_BEAT)) return;
    
    BeatData* beat = &audio_state.ext.beat;
    float current_energy = audio_state.core.global_energy;
    
    // Add current energy to history
    beat_energy_history[beat_history_index] = current_energy;
    beat_history_index = (beat_history_index + 1) % 43;
    
    // Calculate average energy
    float avg_energy = 0.0f;
    for (int i = 0; i < 43; i++) {
        avg_energy += beat_energy_history[i];
    }
    avg_energy /= 43.0f;
    
    // Calculate variance
    float variance = 0.0f;
    for (int i = 0; i < 43; i++) {
        float diff = beat_energy_history[i] - avg_energy;
        variance += diff * diff;
    }
    variance /= 43.0f;
    beat_variance = sqrtf(variance);
    
    // Dynamic threshold
    float threshold = avg_energy + (beat_variance * beat_threshold);
    
    // Beat detection
    if (current_energy > threshold && current_energy > avg_energy * 1.3f) {
        uint32_t now = millis();
        uint32_t time_since_last = now - last_beat_time;
        
        // Minimum 200ms between beats (300 BPM max)
        if (time_since_last > 200) {
            beat->beat_confidence = (current_energy - threshold) / beat_variance;
            beat->beat_confidence = constrain(beat->beat_confidence, 0.0f, 1.0f);
            
            // Update tempo estimation
            if (time_since_last < 2000) {  // Reasonable tempo range
                float instant_bpm = 60000.0f / time_since_last;
                updateTempo(instant_bpm);
            }
            
            last_beat_time = now;
            beat->last_beat_ms = now;
            
            // Determine which band triggered the beat
            float max_zone = 0.0f;
            beat->beat_band = 0;
            for (int i = 0; i < 8; i++) {
                if (audio_state.core.zone_energies[i] > max_zone) {
                    max_zone = audio_state.core.zone_energies[i];
                    beat->beat_band = i / 2;  // Map 8 zones to 4 bands
                }
            }
        }
    } else {
        // Decay confidence
        beat->beat_confidence *= 0.95f;
    }
    
    // Update beat phase
    if (beat->tempo_bpm > 0) {
        float ms_per_beat = 60000.0f / beat->tempo_bpm;
        float ms_since_beat = millis() - beat->last_beat_ms;
        beat->beat_phase = fmodf(ms_since_beat / ms_per_beat, 1.0f);
    }
}

void AudioFeatures::updateTempo(float instant_bpm) {
    // Simple tempo tracking with 4 candidates
    BeatData* beat = &audio_state.ext.beat;
    
    // Find closest candidate
    int closest = -1;
    float min_diff = 20.0f;  // Max 20 BPM difference to match
    
    for (int i = 0; i < 4; i++) {
        if (tempo_candidates[i] > 0) {
            float diff = fabsf(tempo_candidates[i] - instant_bpm);
            if (diff < min_diff) {
                min_diff = diff;
                closest = i;
            }
        }
    }
    
    if (closest >= 0) {
        // Update existing candidate (moving average)
        tempo_candidates[closest] = tempo_candidates[closest] * 0.7f + instant_bpm * 0.3f;
        tempo_last_beats[closest] = millis();
    } else {
        // Add new candidate
        for (int i = 0; i < 4; i++) {
            if (millis() - tempo_last_beats[i] > 5000) {  // Expired
                tempo_candidates[i] = instant_bpm;
                tempo_last_beats[i] = millis();
                break;
            }
        }
    }
    
    // Choose most recent tempo
    uint32_t most_recent = 0;
    beat->tempo_bpm = 0;
    for (int i = 0; i < 4; i++) {
        if (tempo_last_beats[i] > most_recent) {
            most_recent = tempo_last_beats[i];
            beat->tempo_bpm = tempo_candidates[i];
        }
    }
}

void AudioFeatures::updateOnset() {
    if (!(audio_state.feature_flags & AUDIO_FEATURE_ONSET)) return;
    
    OnsetData* onset = &audio_state.ext.onset;
    float current_energy = audio_state.core.global_energy;
    
    // Simple onset detection - energy increase
    float energy_delta = current_energy - prev_total_energy;
    
    if (energy_delta > onset_threshold && current_energy > 0.1f) {
        onset->onset_detected = true;
        onset->onset_strength = constrain(energy_delta * 2.0f, 0.0f, 1.0f);
        onset->onset_time_ms = millis();
        
        // Find which zone had biggest increase
        float max_increase = 0.0f;
        onset->onset_zone = 0;
        
        for (int i = 0; i < 8; i++) {
            // Note: This is simplified - should track zone history
            if (audio_state.core.zone_energies[i] > max_increase) {
                max_increase = audio_state.core.zone_energies[i];
                onset->onset_zone = i;
            }
        }
    } else {
        onset->onset_detected = false;
        onset->onset_strength *= 0.9f;  // Decay
    }
    
    prev_total_energy = current_energy;
}

void AudioFeatures::updateBalance() {
    if (!(audio_state.feature_flags & AUDIO_FEATURE_BALANCE)) return;
    
    FrequencyBalance* bal = &audio_state.ext.balance;
    float* zones = audio_state.core.zone_energies;
    
    // Calculate band energies from zones
    float bass = (zones[0] + zones[1]) / 2.0f;
    float mid = (zones[2] + zones[3] + zones[4] + zones[5]) / 4.0f;
    float treble = (zones[6] + zones[7]) / 2.0f;
    
    // Normalize to sum to 1.0
    float total = bass + mid + treble;
    if (total > 0.001f) {
        bal->bass_ratio = bass / total;
        bal->mid_ratio = mid / total;
        bal->treble_ratio = treble / total;
        
        // Bass to treble ratio
        if (treble > 0.001f) {
            bal->bass_to_treble = bass / treble;
            bal->bass_to_treble = constrain(bal->bass_to_treble, 0.1f, 10.0f);
        } else {
            bal->bass_to_treble = 10.0f;  // Max bass-heavy
        }
    } else {
        // Silence - neutral balance
        bal->bass_ratio = 0.333f;
        bal->mid_ratio = 0.333f;
        bal->treble_ratio = 0.333f;
        bal->bass_to_treble = 1.0f;
    }
}

void AudioFeatures::process(float* frequency_bins, int num_bins) {
    // Always update core features
    updateCore(frequency_bins, num_bins);
    
    // Update extended features based on flags
    if (audio_state.feature_flags & AUDIO_FEATURE_SPECTRAL) {
        updateSpectral();
    }
    
    if (audio_state.feature_flags & AUDIO_FEATURE_DYNAMICS) {
        updateDynamics();
    }
    
    if (audio_state.feature_flags & AUDIO_FEATURE_BEAT) {
        updateBeat();
    }
    
    if (audio_state.feature_flags & AUDIO_FEATURE_ONSET) {
        updateOnset();
    }
    
    if (audio_state.feature_flags & AUDIO_FEATURE_BALANCE) {
        updateBalance();
    }
}

void AudioFeatures::enableBeatDetection(bool enable) {
    if (enable) {
        audio_state.feature_flags |= AUDIO_FEATURE_BEAT;
    } else {
        audio_state.feature_flags &= ~AUDIO_FEATURE_BEAT;
    }
}

void AudioFeatures::enableOnsetDetection(bool enable) {
    if (enable) {
        audio_state.feature_flags |= AUDIO_FEATURE_ONSET;
    } else {
        audio_state.feature_flags &= ~AUDIO_FEATURE_ONSET;
    }
}