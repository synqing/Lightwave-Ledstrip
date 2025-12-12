/*
 * Audio Feature Extraction Engine Header
 * ======================================
 * Provides comprehensive audio analysis at 125Hz
 * 
 * CRITICAL ARCHITECTURE NOTES:
 * ---------------------------
 * This module implements DUAL DATA PATHS to solve the AGC/Beat conflict:
 * 
 * 1. RAW PATH: Used for beat detection (preserves dynamics)
 * 2. AGC PATH: Used for visualization (normalized output)
 * 
 * The Multiband AGC system provides cochlear-inspired processing with:
 * - 4 perceptual bands (bass/low-mid/high-mid/treble)
 * - Independent gain control per band
 * - Cross-band coupling to prevent artifacts
 * - Dynamic time constants based on signal variance
 * 
 * WARNING: DO NOT process beat detection on AGC output!
 * Beat detection REQUIRES raw dynamic range to function.
 */

#ifndef AUDIO_FEATURES_H
#define AUDIO_FEATURES_H

#include "audio_state.h"

class AudioFeatures {
public:
    // Initialize the feature extraction engine
    static void init();
    
    // Main processing function - call this with RAW Goertzel output
    // IMPORTANT: Pass RAW frequency bins, NOT normalized values!
    // This function handles the dual data path architecture internally
    static void process(float* frequency_bins, int num_bins);
    
    // Enable/disable specific features to save CPU
    static void enableBeatDetection(bool enable);
    static void enableOnsetDetection(bool enable);
    
private:
    // Core features (always calculated)
    static void updateCore(float* frequency_bins, int num_bins);
    static void updateZoneEnergies();
    
    // Extended features (optional)
    static void updateSpectral();
    static void updateDynamics();
    static void updateBeat();
    static void updateOnset();
    static void updateBalance();
    
    // Helper functions
    static void updateTempo(float instant_bpm);
};

#endif // AUDIO_FEATURES_H