/*
-----------------------------------------------------------------------------------------
    Enhanced Beat Detection Implementation
    Advanced audio analysis for precise tempo tracking and beat prediction
-----------------------------------------------------------------------------------------

    DEPRECATION NOTICE
    ==================
    This file is part of the legacy monolithic audio pipeline.
    It will be replaced by the pluggable node architecture.
    
    Replacement: beat_detector_node.h
    Migration: See DEPRECATION_TRACKER.md
    Target removal: After Phase 3 completion
    
    DO NOT ADD NEW FEATURES TO THIS FILE
*/

#include "../include/audio/enhanced_beat_detector.h"

// Phase-Locked Loop Implementation
void PhaseLockLoop::update(float input_phase) {
    // Calculate phase error
    float phase_error = input_phase - phase;
    
    // Wrap phase error to [-π, π]
    while (phase_error > M_PI) phase_error -= 2 * M_PI;
    while (phase_error < -M_PI) phase_error += 2 * M_PI;
    
    // Update frequency based on phase error
    frequency += frequency_error_gain * phase_error;
    
    // Clamp frequency to valid range
    if (frequency < frequency_min) frequency = frequency_min;
    if (frequency > frequency_max) frequency = frequency_max;
    
    // Update phase
    phase += frequency * (1.0f / 60.0f);  // Assuming 60 FPS update rate
    phase += phase_error_gain * phase_error;
    
    // Wrap phase to [0, 2π]
    while (phase >= 2 * M_PI) phase -= 2 * M_PI;
    while (phase < 0) phase += 2 * M_PI;
}

void PhaseLockLoop::reset() {
    phase = 0.0f;
    frequency = 2.0f;  // 120 BPM default
}

void PhaseLockLoop::setFrequencyLimits(float min_hz, float max_hz) {
    frequency_min = min_hz;
    frequency_max = max_hz;
}

// Onset Detector Implementation
OnsetDetector::OnsetDetector() {
    for (int i = 0; i < NUM_BANDS; i++) {
        band_energy[i] = 0.0f;
        adaptive_threshold[i] = 0.1f;
        for (int j = 0; j < HISTORY_SIZE; j++) {
            band_history[i][j] = 0.0f;
        }
    }
}

void OnsetDetector::processBands(const float* frequency_bins, int num_bins) {
    int bins_per_band = num_bins / NUM_BANDS;
    
    for (int band = 0; band < NUM_BANDS; band++) {
        float energy = 0.0f;
        int start_bin = band * bins_per_band;
        int end_bin = (band + 1) * bins_per_band;
        
        // Calculate energy in this frequency band
        for (int bin = start_bin; bin < end_bin && bin < num_bins; bin++) {
            energy += frequency_bins[bin] * frequency_bins[bin];
        }
        
        band_energy[band] = sqrt(energy / bins_per_band);
        
        // Update history
        band_history[band][history_index] = band_energy[band];
    }
    
    history_index = (history_index + 1) % HISTORY_SIZE;
}

float OnsetDetector::calculateSpectralFlux(int band, float current_energy) {
    float prev_energy = band_history[band][(history_index - 1 + HISTORY_SIZE) % HISTORY_SIZE];
    float flux = fmaxf(0.0f, current_energy - prev_energy);
    return flux;
}

void OnsetDetector::updateAdaptiveThreshold(int band, float flux) {
    // Calculate mean of recent flux values
    float mean_flux = 0.0f;
    for (int i = 0; i < HISTORY_SIZE; i++) {
        int idx = (history_index - i + HISTORY_SIZE) % HISTORY_SIZE;
        mean_flux += calculateSpectralFlux(band, band_history[band][idx]);
    }
    mean_flux /= HISTORY_SIZE;
    
    // Adaptive threshold with median-based approach
    // SCALED: Original was designed for unscaled values, now we have /8 scaling
    // So we need to scale the threshold calculation by 8x
    adaptive_threshold[band] = mean_flux * 1.5f + 0.8f;  // Was 0.1f, now 0.1f * 8 = 0.8f
}

float OnsetDetector::getOnsetStrength() {
    float total_onset = 0.0f;
    
    // DEBUG: Check what we're getting
    static int onset_debug_counter = 0;
    bool should_debug = (++onset_debug_counter > 250); // Every 2 seconds
    if (should_debug) onset_debug_counter = 0;
    
    for (int band = 0; band < NUM_BANDS; band++) {
        float flux = calculateSpectralFlux(band, band_energy[band]);
        updateAdaptiveThreshold(band, flux);
        
        // HACK: For now, use raw energy for onset detection since flux is broken with /8 scaling
        // The proper fix would be to redesign the onset detector for scaled values
        if (band_energy[band] > adaptive_threshold[band]) {
            // Weight by frequency band (higher frequencies contribute less)
            float weight = 1.0f / (1.0f + band * 0.3f);
            total_onset += band_energy[band] * weight;  // Use energy instead of flux
        }
        
        if (should_debug && band == 0) {  // Debug first band
            Serial.printf("ONSET DEBUG Band %d: energy=%.1f, flux=%.4f, threshold=%.4f\n",
                         band, band_energy[band], flux, adaptive_threshold[band]);
        }
    }
    
    if (should_debug) {
        Serial.printf("ONSET DEBUG Total: %.4f\n", total_onset);
    }
    
    return total_onset;
}

void OnsetDetector::reset() {
    for (int i = 0; i < NUM_BANDS; i++) {
        band_energy[i] = 0.0f;
        adaptive_threshold[i] = 0.1f;
        for (int j = 0; j < HISTORY_SIZE; j++) {
            band_history[i][j] = 0.0f;
        }
    }
    history_index = 0;
}

// Genre Classifier Implementation
GenreClassifier::GenreClassifier() {
    // Initialize genre profiles
    profiles[ELECTRONIC] = {128.0f, 0.9f, 2000.0f, 8000.0f};
    profiles[ROCK] = {120.0f, 0.8f, 1500.0f, 6000.0f};
    profiles[JAZZ] = {140.0f, 0.6f, 1200.0f, 5000.0f};
    profiles[CLASSICAL] = {100.0f, 0.7f, 800.0f, 4000.0f};
    profiles[AMBIENT] = {80.0f, 0.5f, 600.0f, 3000.0f};
    
    for (int i = 0; i < NUM_GENRES; i++) {
        genre_scores[i] = 0.0f;
    }
}

void GenreClassifier::calculateSpectralFeatures(const float* frequency_bins, int num_bins,
                                               float& centroid, float& rolloff) {
    float total_energy = 0.0f;
    float weighted_sum = 0.0f;
    
    // Calculate spectral centroid
    for (int i = 0; i < num_bins; i++) {
        float energy = frequency_bins[i] * frequency_bins[i];
        total_energy += energy;
        weighted_sum += energy * (i + 1);  // Frequency index as weight
    }
    
    centroid = (total_energy > 0) ? (weighted_sum / total_energy) * 50.0f : 0.0f;  // Scale to Hz
    
    // Calculate spectral rolloff (85% of energy)
    float cumulative_energy = 0.0f;
    float target_energy = total_energy * 0.85f;
    rolloff = 0.0f;
    
    for (int i = 0; i < num_bins; i++) {
        cumulative_energy += frequency_bins[i] * frequency_bins[i];
        if (cumulative_energy >= target_energy) {
            rolloff = (i + 1) * 50.0f;  // Scale to Hz
            break;
        }
    }
}

void GenreClassifier::updateGenreScores(float tempo, float rhythm_reg, 
                                       float centroid, float rolloff) {
    for (int i = 0; i < NUM_GENRES; i++) {
        float tempo_diff = fabsf(tempo - profiles[i].tempo_preference) / 50.0f;
        float rhythm_diff = fabsf(rhythm_reg - profiles[i].rhythm_regularity);
        float centroid_diff = fabsf(centroid - profiles[i].spectral_centroid) / 1000.0f;
        float rolloff_diff = fabsf(rolloff - profiles[i].spectral_rolloff) / 2000.0f;
        
        // Combined distance metric (lower is better)
        float distance = tempo_diff + rhythm_diff + centroid_diff + rolloff_diff;
        float score = 1.0f / (1.0f + distance);
        
        // Exponential moving average
        genre_scores[i] = 0.9f * genre_scores[i] + 0.1f * score;
    }
    
    // Find best genre
    float best_score = 0.0f;
    for (int i = 0; i < NUM_GENRES; i++) {
        if (genre_scores[i] > best_score) {
            best_score = genre_scores[i];
            current_genre = i;
        }
    }
}

void GenreClassifier::analyzeSpectrum(const float* frequency_bins, int num_bins, float current_bpm) {
    float centroid, rolloff;
    calculateSpectralFeatures(frequency_bins, num_bins, centroid, rolloff);
    
    // Estimate rhythm regularity (simplified)
    float rhythm_regularity = 0.7f;  // Default value, would need beat history for real calculation
    
    updateGenreScores(current_bpm, rhythm_regularity, centroid, rolloff);
}

float GenreClassifier::getGenreConfidence() const {
    return genre_scores[current_genre];
}

const char* GenreClassifier::getGenreName() const {
    const char* names[] = {"Electronic", "Rock", "Jazz", "Classical", "Ambient"};
    return names[current_genre];
}

// Enhanced Beat Detector Implementation
EnhancedBeatDetector::EnhancedBeatDetector() {
    reset();
}

void EnhancedBeatDetector::processSpectrum(const float* frequency_bins, int num_bins, 
                                         uint32_t timestamp_ms) {
    beat_detected = false;
    
    // Process onset detection
    onset_detector.processBands(frequency_bins, num_bins);
    float onset_strength = onset_detector.getOnsetStrength();
    
    // Update beat confidence
    updateConfidence(onset_strength);
    
    // DEBUG: Log internal state every 5 seconds
    static int debug_counter = 0;
    if (++debug_counter > 625) {
        debug_counter = 0;
        Serial.printf("BEAT DEBUG: onset=%.4f (thresh=%.4f), confidence=%.4f, timestamp=%u\n",
                      onset_strength, onset_threshold, beat_confidence, timestamp_ms);
    }
    
    // Check for beat detection
    if (onset_strength > onset_threshold && beat_confidence > 0.5f) {
        float current_time = timestamp_ms / 1000.0f;
        
        if (validateBeatTiming(current_time)) {
            beat_detected = true;
            beats_detected++;
            last_beat_time = current_time;
            
            // Update PLL with detected beat
            float beat_phase = fmodf(current_time * pll.getFrequency() * 2 * M_PI, 2 * M_PI);
            pll.update(beat_phase);
            
            // Predict next beat
            predicted_next_beat = current_time + (1.0f / pll.getFrequency());
        }
    }
    
    // Update genre classification
    genre_classifier.analyzeSpectrum(frequency_bins, num_bins, getCurrentBPM());
    
    // Adapt parameters based on genre
    adaptToGenre();
    
    // Update average tempo
    average_tempo = 0.95f * average_tempo + 0.05f * getCurrentBPM();
    
    // Decay confidence
    beat_confidence *= confidence_decay;
}

void EnhancedBeatDetector::updateConfidence(float onset_strength) {
    if (onset_strength > onset_threshold) {
        beat_confidence += confidence_boost * (onset_strength / onset_threshold);
        if (beat_confidence > 1.0f) beat_confidence = 1.0f;
    }
}

bool EnhancedBeatDetector::validateBeatTiming(float current_time) {
    if (last_beat_time == 0.0f) return true;  // First beat
    
    float time_since_last = current_time - last_beat_time;
    float expected_interval = 1.0f / pll.getFrequency();
    float timing_error = fabsf(time_since_last - expected_interval) / expected_interval;
    
    // Allow 20% timing deviation
    return timing_error < 0.2f;
}

void EnhancedBeatDetector::adaptToGenre() {
    GenreClassifier::Genre genre = getCurrentGenre();
    
    // DEBUG: Temporarily disable genre adaptation to test with our low threshold
    // Serial.printf("Genre adaptation DISABLED for testing (would set threshold from %.4f to ", onset_threshold);
    
    switch (genre) {
        case GenreClassifier::ELECTRONIC:
            onset_threshold = 0.25f;  // Original value
            confidence_boost = 0.9f;
            break;
        case GenreClassifier::ROCK:
            onset_threshold = 0.3f;   // Original value
            confidence_boost = 0.8f;
            break;
        case GenreClassifier::JAZZ:
            onset_threshold = 0.4f;   // Original value
            confidence_boost = 0.7f;
            break;
        case GenreClassifier::CLASSICAL:
            onset_threshold = 0.35f;  // Original value
            confidence_boost = 0.6f;
            break;
        case GenreClassifier::AMBIENT:
            onset_threshold = 0.5f;   // Original value
            confidence_boost = 0.5f;
            break;
    }
}

void EnhancedBeatDetector::reset() {
    pll.reset();
    onset_detector.reset();
    beat_confidence = 0.0f;
    last_beat_time = 0.0f;
    predicted_next_beat = 0.0f;
    beat_detected = false;
    beats_detected = 0;
    false_positives = 0;
    average_tempo = 120.0f;
}

float EnhancedBeatDetector::getAccuracy() const {
    if (beats_detected == 0) return 0.0f;
    return 1.0f - (static_cast<float>(false_positives) / beats_detected);
}

void EnhancedBeatDetector::setTempoRange(float min_bpm, float max_bpm) {
    pll.setFrequencyLimits(min_bpm / 60.0f, max_bpm / 60.0f);
}

void EnhancedBeatDetector::printStatus() const {
    Serial.println("=== Enhanced Beat Detector Status ===");
    Serial.printf("Current BPM: %.1f\n", getCurrentBPM());
    Serial.printf("Beat Confidence: %.2f\n", beat_confidence);
    Serial.printf("Beats Detected: %u\n", beats_detected);
    Serial.printf("Accuracy: %.2f%%\n", getAccuracy() * 100);
    Serial.printf("Genre: %s (%.2f confidence)\n", getCurrentGenreName(), getGenreConfidence());
    Serial.printf("Next Beat In: %.2fs\n", predicted_next_beat - (millis() / 1000.0f));
    Serial.println("====================================");
}