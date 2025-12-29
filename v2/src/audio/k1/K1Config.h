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
static constexpr float ST2_MAG_SMOOTH       = 0.92f;  // ~0.5s half-life at 10 Hz update

// Update cadence
static constexpr int   ST2_UPDATE_HZ        = 10;     // Run resonators 10x/sec

// Top-K candidates to pass to Stage 3
static constexpr int   ST2_TOPK             = 12;

// ================================
// Stage 3: Tactus Resolver
// ================================

// Tactus prior: Gaussian centered at TACTUS_CENTER with TACTUS_SIGMA
static constexpr float ST3_TACTUS_CENTER    = 120.0f; // Most music perceived here
static constexpr float ST3_TACTUS_SIGMA     = 30.0f;  // Width of preference

// Family contribution weights
static constexpr float ST3_HALF_CONTRIB     = 0.4f;   // Half-tempo contributes 40% to family
static constexpr float ST3_DOUBLE_CONTRIB   = 0.4f;   // Double-tempo contributes 40%

// Stability bonus for staying near current lock
static constexpr float ST3_STABILITY_BONUS  = 0.25f;
static constexpr float ST3_STABILITY_WINDOW = 4.0f;   // BPM tolerance ±4 for "same tempo"

// Hysteresis: challenger must win for N consecutive updates
static constexpr int   ST3_SWITCH_FRAMES    = 8;      // ~0.8s at 10 Hz
static constexpr float ST3_SWITCH_RATIO     = 1.15f;  // Challenger must beat incumbent by 15%

// Minimum confidence to report a lock
static constexpr float ST3_MIN_CONFIDENCE   = 0.15f;

// Tempo Density Memory (KDE-style)
static constexpr int   ST3D_ENABLE              = 1;
static constexpr float ST3D_DECAY               = 0.965f;
static constexpr int   ST3D_KERNEL_RADIUS_BPM   = 2;
static constexpr int   ST3D_KERNEL_SHAPE_TRI    = 1;
static constexpr float ST3D_MIN_ADD_MAG         = 0.08f;
static constexpr int   ST3D_TOPK_USE            = 6;
static constexpr float ST3D_DENSITY_FLOOR       = 1e-6f;

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
