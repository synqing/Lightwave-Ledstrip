/**
 * @file K1Config.h
 * @brief K1-Lightwave Beat Tracker v2 Configuration
 *
 * Ported from Tab5.DSP for ESP32-S3 Lightwave v2 integration.
 *
 * Architecture: Continuous Novelty → Resonator Bank → Tactus Resolver → PLL
 */

#pragma once

namespace lightwaveos {
namespace audio {
namespace k1 {

// ================================
// Audio Configuration
// ================================
// These MUST match Lightwave's AudioCapture settings
static constexpr int   K1_SAMPLE_RATE_HZ = 16000;
static constexpr int   K1_HOP_N          = 256;    // 50% overlap → ~62.5 Hz frame rate

// Derived: novelty frame rate
static constexpr float K1_NOVELTY_FS     = (float)K1_SAMPLE_RATE_HZ / (float)K1_HOP_N;  // ~62.5 Hz

// ================================
// Stage 1: Novelty Extraction (Perceptual Weighting)
// ================================
// Perceptual frequency weights for onset detection
// Higher weight = more contribution to onset signal
// Bass-heavy weighting improves kick drum detection
static constexpr float K1_BAND_WEIGHTS[8] = {
    1.00f,  // Band 0: 60 Hz (sub-bass) - kick drums, max weight
    0.85f,  // Band 1: 120 Hz (bass) - bass guitar, low synth
    0.60f,  // Band 2: 250 Hz (low-mid) - body of instruments
    0.40f,  // Band 3: 500 Hz (mid) - vocal fundamentals
    0.25f,  // Band 4: 1000 Hz (high-mid) - vocal presence
    0.15f,  // Band 5: 2000 Hz (high) - hi-hats begin
    0.08f,  // Band 6: 4000 Hz (brilliance) - hi-hats, cymbals
    0.04f   // Band 7: 7800 Hz (air) - near-noise, minimal weight
};

// Sum of weights for normalization (precomputed: 3.37f)
static constexpr float K1_BAND_WEIGHT_SUM = 1.00f + 0.85f + 0.60f + 0.40f + 0.25f + 0.15f + 0.08f + 0.04f;

// ================================
// Stage 2: Resonator Bank
// ================================
static constexpr int   ST2_BPM_MIN          = 60;
static constexpr int   ST2_BPM_MAX          = 180;
static constexpr int   ST2_BPM_STEP         = 1;
static constexpr int   ST2_BPM_BINS         = ((ST2_BPM_MAX - ST2_BPM_MIN) / ST2_BPM_STEP) + 1;  // 121 bins

// Novelty history for Goertzel (seconds)
static constexpr float ST2_HISTORY_SEC      = 8.0f;
static constexpr int   ST2_HISTORY_FRAMES   = (int)(ST2_HISTORY_SEC * K1_NOVELTY_FS);  // ~500 frames

// Magnitude smoothing (EMA alpha)
// Changed from 0.92 to 0.85: faster response (~0.4s half-life) for quicker 138 BPM detection
static constexpr float ST2_MAG_SMOOTH       = 0.85f;

// Update cadence
static constexpr int   ST2_UPDATE_HZ        = 10;     // Run resonators 10x/sec

// Top-K candidates to pass to Stage 3
static constexpr int   ST2_TOPK             = 12;

// ================================
// Stage 3: Tactus Resolver
// ================================

// Tactus prior: Gaussian centered at TACTUS_CENTER with TACTUS_SIGMA
static constexpr float ST3_TACTUS_CENTER    = 120.0f; // Most music perceived here
static constexpr float ST3_TACTUS_SIGMA     = 40.0f;  // Width of preference (was 30, widened to reduce bias)

// Family contribution weights
static constexpr float ST3_HALF_CONTRIB     = 0.4f;   // Half-tempo contributes 40% to family
static constexpr float ST3_DOUBLE_CONTRIB   = 0.4f;   // Double-tempo contributes 40%

// Stability bonus for staying near current lock
static constexpr float ST3_STABILITY_BONUS  = 0.12f;  // Reduced from 0.25 to reduce lock-in strength
static constexpr float ST3_STABILITY_WINDOW = 2.0f;   // BPM tolerance ±2 for "same tempo" (Tab5.DSP alignment)

// Hysteresis: challenger must win for N consecutive updates
static constexpr int   ST3_SWITCH_FRAMES    = 8;      // ~0.8s at 10 Hz
static constexpr float ST3_SWITCH_RATIO     = 1.15f;  // Challenger must beat incumbent by 15%

// Minimum confidence to report a lock
static constexpr float ST3_MIN_CONFIDENCE   = 0.15f;

// Tempo Density Memory (KDE-style)
static constexpr int   ST3D_ENABLE              = 1;
static constexpr float ST3D_DECAY               = 0.97f;  // Tab5.DSP alignment - slower decay for longer memory
static constexpr int   ST3D_KERNEL_RADIUS_BPM   = 2;
static constexpr int   ST3D_KERNEL_SHAPE_TRI    = 1;
static constexpr float ST3D_MIN_ADD_MAG         = 0.08f;
static constexpr int   ST3D_TOPK_USE            = 6;
static constexpr float ST3D_DENSITY_FLOOR       = 1e-6f;

// ================================
// Debug Flags
// ================================
// Set to 1 to enable verbose tactus scoring diagnostics
#define K1_DEBUG_TACTUS_SCORES 0

// ================================
// Stage 4: Beat Clock (PLL)
// ================================

// PLL gains (lower = slower tracking, more stable)
static constexpr float ST4_PHASE_GAIN       = 0.08f;  // How fast to correct phase error
static constexpr float ST4_FREQ_GAIN        = 0.002f; // How fast to correct frequency (BPM)

// Maximum correction per update
static constexpr float ST4_MAX_PHASE_CORR   = 0.15f;  // Max phase correction (fraction of beat)
static constexpr float ST4_MAX_FREQ_CORR    = 2.0f;   // Max BPM correction per update

// Beat tick debounce (percentage of beat period)
static constexpr float ST4_BEAT_DEBOUNCE_RATIO = 0.6f;

// Lock verification period (Phase 5)
static constexpr uint32_t LOCK_VERIFY_MS = 2500;      // 2.5 second verification period
static constexpr float COMPETITOR_THRESHOLD = 1.10f;  // 10% advantage to reconsider during verification

// ================================
// Legacy Aliases (K1TactusResolver.cpp compatibility)
// ================================
static constexpr int   K1_BPM_BINS         = ST2_BPM_BINS;
static constexpr int   K1_TOPK             = ST2_TOPK;
static constexpr int   K1_TOPK_USE         = ST3D_TOPK_USE;
static constexpr float K1_DENSITY_FLOOR    = ST3D_DENSITY_FLOOR;
static constexpr float K1_STABILITY_WINDOW = ST3_STABILITY_WINDOW;
static constexpr float K1_SWITCH_RATIO     = ST3_SWITCH_RATIO;
static constexpr int   K1_SWITCH_FRAMES    = ST3_SWITCH_FRAMES;

} // namespace k1
} // namespace audio
} // namespace lightwaveos
