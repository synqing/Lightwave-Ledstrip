/*
-----------------------------------------------------------------------------------------
    Enhanced Beat Detection with Phase-Locked Loop and Genre Classification
    Advanced audio analysis for precise tempo tracking and beat prediction
-----------------------------------------------------------------------------------------
*/

#ifndef ENHANCED_BEAT_DETECTOR_H
#define ENHANCED_BEAT_DETECTOR_H

#include <Arduino.h>
#include <cstdint>
#include <math.h>

// Phase-Locked Loop for beat tracking
class PhaseLockLoop {
private:
    float phase = 0.0f;
    float frequency = 2.0f;  // 120 BPM default (2 Hz)
    float phase_error_gain = 0.1f;
    float frequency_error_gain = 0.01f;
    float frequency_min = 0.5f;   // 30 BPM
    float frequency_max = 4.0f;   // 240 BPM
    
public:
    void update(float input_phase);
    void reset();
    float getPhase() const { return phase; }
    float getFrequency() const { return frequency; }
    float getBPM() const { return frequency * 60.0f; }
    void setFrequencyLimits(float min_hz, float max_hz);
};

// Multi-band onset detection
class OnsetDetector {
private:
    static const int NUM_BANDS = 4;
    static const int HISTORY_SIZE = 8;
    
    float band_energy[NUM_BANDS];
    float band_history[NUM_BANDS][HISTORY_SIZE];
    float adaptive_threshold[NUM_BANDS];
    int history_index = 0;
    
    float calculateSpectralFlux(int band, float current_energy);
    void updateAdaptiveThreshold(int band, float flux);
    
public:
    OnsetDetector();
    void processBands(const float* frequency_bins, int num_bins);
    float getOnsetStrength();
    void reset();
};

// Genre classification for beat tracking adaptation
class GenreClassifier {
private:
    struct GenreProfile {
        float tempo_preference;
        float rhythm_regularity;
        float spectral_centroid;
        float spectral_rolloff;
    };
    
    static const int NUM_GENRES = 5;
    GenreProfile profiles[NUM_GENRES];
    float genre_scores[NUM_GENRES];
    int current_genre = 0;
    
    void calculateSpectralFeatures(const float* frequency_bins, int num_bins, 
                                 float& centroid, float& rolloff);
    void updateGenreScores(float tempo, float rhythm_reg, float centroid, float rolloff);
    
public:
    enum Genre {
        ELECTRONIC = 0,
        ROCK = 1,
        JAZZ = 2,
        CLASSICAL = 3,
        AMBIENT = 4
    };
    
    GenreClassifier();
    void analyzeSpectrum(const float* frequency_bins, int num_bins, float current_bpm);
    Genre getCurrentGenre() const { return static_cast<Genre>(current_genre); }
    float getGenreConfidence() const;
    const char* getGenreName() const;
};

// Enhanced Beat Detector with machine learning-style classification
class EnhancedBeatDetector {
private:
    PhaseLockLoop pll;
    OnsetDetector onset_detector;
    GenreClassifier genre_classifier;
    
    // Beat tracking state
    float beat_confidence = 0.0f;
    float last_beat_time = 0.0f;
    float predicted_next_beat = 0.0f;
    bool beat_detected = false;
    
    // Adaptive parameters
    // NOTE: onset_threshold scaled up 8x to match Goertzel /8 legacy scaling
    float onset_threshold = 2.4f;  // Was 0.3f, now 0.3f * 8 = 2.4f
    float confidence_decay = 0.95f;
    float confidence_boost = 0.8f;
    
    // Performance metrics
    uint32_t beats_detected = 0;
    uint32_t false_positives = 0;
    float average_tempo = 120.0f;
    
    void updateConfidence(float onset_strength);
    bool validateBeatTiming(float current_time);
    void adaptToGenre();
    
public:
    EnhancedBeatDetector();
    
    // Main processing
    void processSpectrum(const float* frequency_bins, int num_bins, uint32_t timestamp_ms);
    void reset();
    
    // Beat information
    bool isBeatDetected() const { return beat_detected; }
    float getBeatConfidence() const { return beat_confidence; }
    float getCurrentBPM() const { return pll.getBPM(); }
    float getPredictedNextBeat() const { return predicted_next_beat; }
    
    // Genre information
    GenreClassifier::Genre getCurrentGenre() const { return genre_classifier.getCurrentGenre(); }
    float getGenreConfidence() const { return genre_classifier.getGenreConfidence(); }
    const char* getCurrentGenreName() const { return genre_classifier.getGenreName(); }
    
    // Performance metrics
    uint32_t getTotalBeatsDetected() const { return beats_detected; }
    float getAverageTempo() const { return average_tempo; }
    float getAccuracy() const;
    
    // Configuration
    void setOnsetThreshold(float threshold) { onset_threshold = threshold; }
    void setTempoRange(float min_bpm, float max_bpm);
    
    // Debug
    void printStatus() const;
};

#endif