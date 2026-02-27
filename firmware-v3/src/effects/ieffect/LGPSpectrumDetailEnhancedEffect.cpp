/**
 * @file LGPSpectrumDetailEnhancedEffect.cpp
 * @brief Enhanced 64-bin FFT spectrum visualization matching Sensory Bridge pattern
 * 
 * IMPLEMENTATION MATCHES SENSORY BRIDGE get_smooth_spectrogram():
 * - Update targets ONLY on new hops (prevents per-frame noise)
 * - 4-frame history buffer BEFORE smoothing (filters single-frame spikes)
 * - Symmetric 0.75 linear interpolation (simple, NOT asymmetric)
 * - Center-to-edges motion (reversed mapping)
 * - High amplitude gain for full LED range
 */

#include "LGPSpectrumDetailEnhancedEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"

#ifdef FEATURE_EFFECT_VALIDATION
#include "../../validation/EffectValidationMacros.h"
#endif

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include <Arduino.h>
#endif

#include <cmath>
#include <cstring>
#include <cstdio>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// Enhanced parameters
constexpr float AMPLITUDE_GAIN = 4.0f;  // High gain for full LED range
constexpr float MIN_THRESHOLD = 0.002f;  // Very low threshold for sensitivity
// NOTE: SMOOTHING_COEFF removed - using frame-rate independent tau-based smoothing

// PRE_SCALE: Scale colors BEFORE accumulation to prevent overflow
// CRITICAL: 4 treble bins (60-63) all map to distance 0 (center LED)
// With qadd8() saturation, we can safely increase brightness
constexpr uint8_t SPECTRUM_PRE_SCALE = 90;   // 4 bins × 90 = 360 → qadd8 clamps to 255
constexpr uint8_t TRAIL_PRE_SCALE = 45;      // Conservative for trails (also overlapping)
constexpr uint8_t STRIP2_PRE_SCALE = 60;     // Strip 2 at 66% of Strip 1 (supporting role)

bool LGPSpectrumDetailEnhancedEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // =========================================================================
    // PRE-COMPUTE FRAME-RATE INDEPENDENT ALPHA VALUES
    // Formula: alpha = 1 - exp(-dt/tau)
    // This ensures identical visual behavior at any frame rate
    // =========================================================================
    m_smoothingAlpha = 1.0f - expf(-FRAME_DT / SMOOTHING_TAU);  // ~0.154 @ 120 FPS
    m_attackAlpha = 1.0f - expf(-FRAME_DT / ATTACK_TAU);        // ~0.341 @ 120 FPS
    m_releaseAlpha = 1.0f - expf(-FRAME_DT / RELEASE_TAU);      // ~0.027 @ 120 FPS
    m_decayAlpha = expf(-FRAME_DT / DECAY_TAU);                 // ~0.9958 @ 120 FPS (retention)
    m_shimmerAlpha = 1.0f - expf(-FRAME_DT / SHIMMER_SMOOTH_TAU); // ~0.080 @ 120 FPS

    // Initialize history buffer and smoothing arrays
    for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
        for (uint8_t bin = 0; bin < 64; bin++) {
            m_binHistory[i][bin] = 0.0f;
        }
    }
    for (uint8_t bin = 0; bin < 64; bin++) {
        m_binSmoothing[bin] = 0.0f;
        m_shimmerAmp[bin] = 0.0f;  // Initialize smoothed shimmer amplitudes
    }
    m_historyIdx = 0;
    m_lastHopSeq = 0;

    // Initialize reverse trail radial buffer and beat tracking
    memset(m_radialTrail, 0, sizeof(m_radialTrail));
    m_lastBeatInBar = 255;
    m_lastBarPhase = 0.0f;

    // Initialize motion physics v2 state (Direct Energy Coupling)
    for (uint8_t bin = 0; bin < 64; bin++) {
        m_binDistance[bin] = (float)binToLedDistance(bin);  // Start at static position
        m_binMomentum[bin] = 0.0f;                          // No initial momentum
    }

    // Strip 2 gap detection uses local arrays in render() - no initialization needed

    return true;
}

void LGPSpectrumDetailEnhancedEffect::render(plugins::EffectContext& ctx) {
    // Faster LED-level cleanup to prevent saturation buildup between frames
    // fadeAmount=12 (4.7% per frame) - takes 14 frames to halve instead of 77
    fadeToBlackBy(ctx.leds, ctx.ledCount, 12);

    // Fade reverse trail buffer - fixed faster rate for consistent decay
    // (ctx.fadeAmount varies, use fixed value for predictable trail behavior)
    fadeToBlackBy(m_radialTrail, HALF_LENGTH, 15);

#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    if (!ctx.audio.available) {
        return;
    }

    // Prefer adaptive normalisation (Sensory Bridge max follower); fall back to raw bins if unavailable.
    const float* bins64 = ctx.audio.bins64Adaptive();
    if (!bins64) {
        bins64 = ctx.audio.bins64();
    }
    constexpr uint8_t NUM_BINS = 64;
    
    // =========================================================================
    // Sensory Bridge Pattern: Update targets ONLY on new hops
    // This prevents per-frame noise and matches Sensory Bridge exactly
    // =========================================================================
    bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
    if (newHop) {
        m_lastHopSeq = ctx.audio.controlBus.hop_seq;
        
        // Update history buffer ONLY on new hops (when data actually changes)
        // Sensory Bridge Pattern: History Buffer BEFORE Smoothing (filters spikes)
        for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
            m_binHistory[m_historyIdx][bin] = bins64[bin];
        }
        m_historyIdx = (m_historyIdx + 1) % HISTORY_SIZE;
    }
    
    // =========================================================================
    // Sensory Bridge Pattern: 4-Frame Rolling Average (spike filtering)
    // Average history buffer BEFORE smoothing to filter single-frame spikes
    // =========================================================================
    float avgBins[64] = {0.0f};
    for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
        float sum = 0.0f;
        for (uint8_t i = 0; i < HISTORY_SIZE; ++i) {
            sum += m_binHistory[i][bin];
        }
        avgBins[bin] = sum / (float)HISTORY_SIZE;
    }
    
    // =========================================================================
    // FRAME-RATE INDEPENDENT SMOOTHING (tau=50ms)
    // Uses pre-computed m_smoothingAlpha = 1 - exp(-dt/tau)
    // At 120 FPS: alpha ~0.154 (reaches 63% of target in 50ms = 6 frames)
    // =========================================================================
    for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
        float target = avgBins[bin];
        float current = m_binSmoothing[bin];

        // Frame-rate independent exponential smoothing
        m_binSmoothing[bin] = current + (target - current) * m_smoothingAlpha;
    }

    // =========================================================================
    // FRAME-RATE INDEPENDENT DECAY (tau=2000ms)
    // Uses pre-computed m_decayAlpha = exp(-dt/tau) for retention
    // At 120 FPS: alpha ~0.9958 (half-life = ln(2) * tau = ~1.4s)
    // Creates "fluid in slow mo" trails that decay uniformly
    // =========================================================================
    for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
        // Frame-rate independent uniform decay
        m_binSmoothing[bin] *= m_decayAlpha;
    }

    // =========================================================================
    // MOTION PHYSICS v2: Frame-Rate Independent Energy Coupling
    //
    // Philosophy: Energy controls position DIRECTLY with smooth transitions.
    // Uses pre-computed alpha values for frame-rate independence:
    // - m_attackAlpha: 20ms tau for fast attack (near-instant response)
    // - m_releaseAlpha: 300ms tau for slow release (smooth decay)
    // =========================================================================

    // Motion parameters (frame-rate independent)
    constexpr float EXPANSION_FACTOR = 12.0f;   // Max LED displacement from energy
    constexpr float MOMENTUM_FACTOR = 6.0f;     // Additional displacement from momentum
    constexpr float SILENCE_THRESHOLD = 0.01f;  // Below this = "silent"
    constexpr float MOMENTUM_THRESHOLD = 0.05f; // Below this = allow spring return
    constexpr float SPRING_RETURN_TAU = 0.200f; // 200ms spring return time constant

    // Pre-compute spring return alpha (could also be done in init())
    float springReturnAlpha = 1.0f - expf(-FRAME_DT / SPRING_RETURN_TAU);

    for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
        float currentEnergy = m_binSmoothing[bin];
        float staticBase = (float)binToLedDistance(bin);

        // ASYMMETRIC MOMENTUM: Frame-rate independent attack/release
        float alpha = (currentEnergy > m_binMomentum[bin]) ? m_attackAlpha : m_releaseAlpha;
        m_binMomentum[bin] += (currentEnergy - m_binMomentum[bin]) * alpha;

        // DIRECT COUPLING: Position = Base + Instant + MomentumTail
        float directContribution = currentEnergy * EXPANSION_FACTOR;       // INSTANT
        float momentumContribution = m_binMomentum[bin] * MOMENTUM_FACTOR; // SMOOTH

        float rawPosition = staticBase + directContribution + momentumContribution;

        // SPRING RETURN: Frame-rate independent (only during silence)
        if (currentEnergy < SILENCE_THRESHOLD && m_binMomentum[bin] < MOMENTUM_THRESHOLD) {
            rawPosition = rawPosition + (staticBase - rawPosition) * springReturnAlpha;
        }

        // Clamp to valid LED range
        m_binDistance[bin] = fmaxf(0.0f, fminf((float)(HALF_LENGTH - 1), rawPosition));
    }

    // =========================================================================
    // TREBLE SHIMMER: Frame-rate independent amplitude smoothing
    // Oscillation amplitude is smoothed to prevent abrupt size changes
    // Uses m_shimmerAlpha (100ms tau) for smooth amplitude transitions
    // =========================================================================
    float time = (float)millis() * 0.001f;  // Time in seconds
    for (uint8_t bin = 48; bin < 64; ++bin) {  // Treble bins only (highest 16 bins)
        // Target amplitude based on current energy
        float targetAmp = (m_binSmoothing[bin] > MIN_THRESHOLD) ? 0.3f * m_binSmoothing[bin] : 0.0f;

        // Smooth the amplitude using frame-rate independent decay
        m_shimmerAmp[bin] += (targetAmp - m_shimmerAmp[bin]) * m_shimmerAlpha;

        // Apply oscillation with smoothed amplitude
        if (m_shimmerAmp[bin] > 0.001f) {
            float freqNorm = (float)(bin - 48) / 16.0f;  // 0.0-1.0 for treble range
            float oscillationFreq = 4.0f + freqNorm * 8.0f;  // 4-12 Hz
            m_binDistance[bin] += m_shimmerAmp[bin] * sinf(time * oscillationFreq * 6.28f);

            // Re-clamp after oscillation
            m_binDistance[bin] = fmaxf(0.0f, fminf((float)(HALF_LENGTH - 1), m_binDistance[bin]));
        }
    }

    // =========================================================================
    // Detect backbeat trigger (beats 2 and 4) with confidence check and fallback
    // PRIMARY: Use beat_in_bar when tempo confidence is high (musically aligned)
    // FALLBACK: Use bar_phase01 threshold crossing when confidence is low (consistent rhythm)
    // =========================================================================
    bool triggerReverseTrail = false;
    
    if (ctx.audio.available) {
        float tempoConf = ctx.audio.tempoConfidence();
        
        if (tempoConf > 0.3f) {
        // RELIABLE BEAT TRACKING: Use beat_in_bar for musically-aligned triggers
        uint8_t beatInBar = ctx.audio.musicalGrid.beat_in_bar;
        // Trigger on beats 2 and 4 (beat_in_bar == 1 or 3)
        if ((beatInBar == 1 || beatInBar == 3) && beatInBar != m_lastBeatInBar) {
            triggerReverseTrail = true;
        }
        m_lastBeatInBar = beatInBar;
    } else {
        // LOW CONFIDENCE FALLBACK: Use bar_phase01 threshold crossing
        // Triggers when bar phase crosses 0.25 (beat 2) or 0.75 (beat 4)
        float barPhase = ctx.audio.musicalGrid.bar_phase01;
        float lastBarPhase = m_lastBarPhase;
        
        // Detect threshold crossings (0.0→0.25, 0.24→0.26, 0.49→0.51, 0.74→0.76)
        bool crossBeat2 = (lastBarPhase < 0.25f && barPhase >= 0.25f) || 
                          (lastBarPhase > 0.75f && barPhase < 0.26f);  // Wrap detection
        bool crossBeat4 = (lastBarPhase < 0.75f && barPhase >= 0.75f) ||
                          (lastBarPhase > 0.99f && barPhase < 0.76f);  // Wrap detection
        
        if (crossBeat2 || crossBeat4) {
            triggerReverseTrail = true;
        }
        m_lastBarPhase = barPhase;
        m_lastBeatInBar = 255;  // Reset beat tracking when using fallback
        }
    }
    
    // =========================================================================
    // Render reverse trail on trigger (similar to Ripple Enhanced's wave rendering)
    // ENHANCED: Beat triggers velocity pulse for "breathing" expansion effect
    // =========================================================================
    if (triggerReverseTrail) {
        // MOTION ENHANCEMENT v2: Momentum boost on beats 2 & 4
        // Creates coordinated outward expansion pulse (F1-style throttle burst)
        for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
            if (m_binSmoothing[bin] > MIN_THRESHOLD) {
                // Boost momentum by 20-50% based on energy (scales with activity)
                float boostFactor = 0.2f + m_binSmoothing[bin] * 0.3f;
                m_binMomentum[bin] = fminf(1.0f, m_binMomentum[bin] + boostFactor);
            }
        }

        // Render reverse trail: for each active bin, draw trailing wake
        for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
            float magnitude = m_binSmoothing[bin] * AMPLITUDE_GAIN;
            if (magnitude < MIN_THRESHOLD) continue;
            
            // Map bin to LED distance from center
            uint16_t ledDist = binToLedDistance(bin);
            
            // Draw trailing wake: brighter at bin position, fading behind
            // This creates the "reverse" motion as the trail persists while bin moves forward
            for (int16_t trailOffset = -3; trailOffset <= 3; ++trailOffset) {
                int16_t trailDist = ledDist + trailOffset;
                if (trailDist < 0 || trailDist >= HALF_LENGTH) continue;
                
                float trailPos = fabsf((float)trailOffset);
                if (trailPos < 3.0f) {
                    // Brightness: max at bin position, fades away
                    // Halved intensity to prevent trail overflow (4 bins × 45 PRE_SCALE = 180)
                    float trailBrightnessFactor = 0.5f * (1.0f - (trailPos / 3.0f));  // 0.5 at center, 0.0 at edge
                    float brightnessNorm = (float)ctx.brightness / 255.0f;  // Normalize to 0.0-1.0
                    float trailBrightFloat = magnitude * brightnessNorm * 255.0f * trailBrightnessFactor;
                    uint8_t trailBright = (uint8_t)fminf(255.0f, trailBrightFloat);  // Clamp properly
                    
                    CRGB color = frequencyToColor(bin, ctx);
                    color = color.nscale8(trailBright);

                    // PRE_SCALE before accumulation (correct pattern - prevents overflow at source)
                    color.r = scale8(color.r, TRAIL_PRE_SCALE);
                    color.g = scale8(color.g, TRAIL_PRE_SCALE);
                    color.b = scale8(color.b, TRAIL_PRE_SCALE);

                    // Additive blending into radial buffer with qadd8() overflow protection
                    m_radialTrail[trailDist].r = qadd8(m_radialTrail[trailDist].r, color.r);
                    m_radialTrail[trailDist].g = qadd8(m_radialTrail[trailDist].g, color.g);
                    m_radialTrail[trailDist].b = qadd8(m_radialTrail[trailDist].b, color.b);
                }
            }
        }
    }
    
    // =========================================================================
    // SUBPIXEL RENDERING: Anti-aliased LED positioning
    // Instead of truncating to integer positions, distribute color between
    // adjacent LEDs based on fractional position for smooth motion
    // =========================================================================

    uint16_t centerLED = ctx.centerPoint;

    for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
        // Apply amplitude gain to smoothed magnitude
        float magnitude = m_binSmoothing[bin] * AMPLITUDE_GAIN;

        // VISIBILITY FIX: Apply 2x gain boost to bass bins (0-15) for better visibility
        if (bin < 16) {
            magnitude *= 2.0f;
        }

        // Skip if below threshold
        if (magnitude < MIN_THRESHOLD) {
            continue;
        }

        // SUBPIXEL: Keep fractional position for anti-aliasing
        float ledDistFloat = m_binDistance[bin];
        uint16_t ledDistLow = (uint16_t)ledDistFloat;
        uint16_t ledDistHigh = ledDistLow + 1;
        float frac = ledDistFloat - (float)ledDistLow;  // 0.0-1.0 fractional part

        // Get color for this frequency band (using palette system)
        CRGB color = frequencyToColor(bin, ctx);

        // Brightness from magnitude (already includes AMPLITUDE_GAIN)
        float brightnessNorm = (float)ctx.brightness / 255.0f;
        float brightFloat = magnitude * brightnessNorm * 255.0f;
        uint8_t bright = (uint8_t)fminf(255.0f, brightFloat);
        color = color.nscale8(bright);

        // PRE_SCALE before accumulation
        color.r = scale8(color.r, SPECTRUM_PRE_SCALE);
        color.g = scale8(color.g, SPECTRUM_PRE_SCALE);
        color.b = scale8(color.b, SPECTRUM_PRE_SCALE);

        // SUBPIXEL: Distribute color based on fractional position
        // Low LED gets (1-frac) brightness, High LED gets (frac) brightness
        uint8_t lowScale = (uint8_t)((1.0f - frac) * 255.0f);
        uint8_t highScale = (uint8_t)(frac * 255.0f);

        CRGB colorLow = color;
        colorLow.nscale8(lowScale);

        CRGB colorHigh = color;
        colorHigh.nscale8(highScale);

        // Apply LOW position (primary LED)
        uint16_t leftIdxLow = centerLED - 1 - ledDistLow;
        uint16_t rightIdxLow = centerLED + ledDistLow;

        if (leftIdxLow < ctx.ledCount) {
            ctx.leds[leftIdxLow].r = qadd8(ctx.leds[leftIdxLow].r, colorLow.r);
            ctx.leds[leftIdxLow].g = qadd8(ctx.leds[leftIdxLow].g, colorLow.g);
            ctx.leds[leftIdxLow].b = qadd8(ctx.leds[leftIdxLow].b, colorLow.b);
        }
        if (rightIdxLow < ctx.ledCount) {
            ctx.leds[rightIdxLow].r = qadd8(ctx.leds[rightIdxLow].r, colorLow.r);
            ctx.leds[rightIdxLow].g = qadd8(ctx.leds[rightIdxLow].g, colorLow.g);
            ctx.leds[rightIdxLow].b = qadd8(ctx.leds[rightIdxLow].b, colorLow.b);
        }

        // Apply HIGH position (anti-aliased adjacent LED) if within bounds
        if (ledDistHigh < HALF_LENGTH) {
            uint16_t leftIdxHigh = centerLED - 1 - ledDistHigh;
            uint16_t rightIdxHigh = centerLED + ledDistHigh;

            if (leftIdxHigh < ctx.ledCount) {
                ctx.leds[leftIdxHigh].r = qadd8(ctx.leds[leftIdxHigh].r, colorHigh.r);
                ctx.leds[leftIdxHigh].g = qadd8(ctx.leds[leftIdxHigh].g, colorHigh.g);
                ctx.leds[leftIdxHigh].b = qadd8(ctx.leds[leftIdxHigh].b, colorHigh.b);
            }
            if (rightIdxHigh < ctx.ledCount) {
                ctx.leds[rightIdxHigh].r = qadd8(ctx.leds[rightIdxHigh].r, colorHigh.r);
                ctx.leds[rightIdxHigh].g = qadd8(ctx.leds[rightIdxHigh].g, colorHigh.g);
                ctx.leds[rightIdxHigh].b = qadd8(ctx.leds[rightIdxHigh].b, colorHigh.b);
            }
        }
    }
    
    // =========================================================================
    // Blend reverse trail into main LED buffer (creates trailing wake effect)
    // STRIP 1 ONLY (like original) - qadd8() prevents overflow
    // =========================================================================
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        if (m_radialTrail[dist].r > 0 || m_radialTrail[dist].g > 0 || m_radialTrail[dist].b > 0) {
            uint16_t centerLED = ctx.centerPoint;
            uint16_t leftIdx = centerLED - 1 - dist;
            uint16_t rightIdx = centerLED + dist;
            
            if (leftIdx < ctx.ledCount) {
                ctx.leds[leftIdx].r = qadd8(ctx.leds[leftIdx].r, m_radialTrail[dist].r);
                ctx.leds[leftIdx].g = qadd8(ctx.leds[leftIdx].g, m_radialTrail[dist].g);
                ctx.leds[leftIdx].b = qadd8(ctx.leds[leftIdx].b, m_radialTrail[dist].b);
            }
            if (rightIdx < ctx.ledCount) {
                ctx.leds[rightIdx].r = qadd8(ctx.leds[rightIdx].r, m_radialTrail[dist].r);
                ctx.leds[rightIdx].g = qadd8(ctx.leds[rightIdx].g, m_radialTrail[dist].g);
                ctx.leds[rightIdx].b = qadd8(ctx.leds[rightIdx].b, m_radialTrail[dist].b);
            }
        }
    }

    // =========================================================================
    // STRIP 2: GAP DETECTION VIA BOOLEAN MASK
    // Mark LED positions where Strip 1 rendered bins (with magnitude threshold)
    // Gaps are positions NOT in the mask
    // =========================================================================
    constexpr float SIGNIFICANT_MAGNITUDE = 0.05f;  // Only count bins with real energy
    constexpr float MAX_GAP_BRIGHTNESS = 0.7f;
    constexpr uint8_t GAP_PRE_SCALE = 80;

    // Step 1: Create mask of where bins are rendered
    bool hasBin[HALF_LENGTH] = {false};
    float binMagnitudeAt[HALF_LENGTH] = {0.0f};  // Track magnitude at each position

    for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
        float magnitude = m_binSmoothing[bin];
        if (magnitude < SIGNIFICANT_MAGNITUDE) continue;  // Only significant bins

        uint16_t pos = (uint16_t)m_binDistance[bin];
        if (pos >= HALF_LENGTH) continue;

        // Mark this position and immediate neighbors as "has bin"
        hasBin[pos] = true;
        binMagnitudeAt[pos] = fmaxf(binMagnitudeAt[pos], magnitude);

        if (pos > 0) {
            hasBin[pos - 1] = true;
            binMagnitudeAt[pos - 1] = fmaxf(binMagnitudeAt[pos - 1], magnitude * 0.5f);
        }
        if (pos < HALF_LENGTH - 1) {
            hasBin[pos + 1] = true;
            binMagnitudeAt[pos + 1] = fmaxf(binMagnitudeAt[pos + 1], magnitude * 0.5f);
        }
    }

    // Step 2: Compute total audio energy for gap brightness scaling
    float totalEnergy = 0.0f;
    for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
        totalEnergy += m_binSmoothing[bin];
    }
    totalEnergy = fminf(1.0f, totalEnergy / 8.0f);  // Normalize

    // Step 3: Render gaps (positions where hasBin is false)
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        if (hasBin[dist]) continue;  // Skip - Strip 1 renders here

        // THIS IS A GAP - render fill

        // Find nearest bin for color (search outward)
        uint8_t colorBin = 32;  // Default
        float nearestMag = 0.0f;
        for (int16_t offset = 1; offset < 20; ++offset) {
            if (dist >= (uint16_t)offset && hasBin[dist - offset]) {
                colorBin = (uint8_t)((dist - offset) * 64 / HALF_LENGTH);  // Approximate bin
                nearestMag = binMagnitudeAt[dist - offset];
                break;
            }
            if (dist + offset < HALF_LENGTH && hasBin[dist + offset]) {
                colorBin = (uint8_t)((dist + offset) * 64 / HALF_LENGTH);  // Approximate bin
                nearestMag = binMagnitudeAt[dist + offset];
                break;
            }
        }

        // Gap brightness: based on total energy and nearby magnitude
        float gapBright = totalEnergy * MAX_GAP_BRIGHTNESS;
        if (nearestMag > 0.0f) {
            gapBright = fmaxf(gapBright, nearestMag * MAX_GAP_BRIGHTNESS);
        }

        // COLOR: Use distance-based palette index (varies across strip)
        // This ensures different gaps get different colors
        uint8_t paletteIdx = (uint8_t)(ctx.gHue + (dist * 255 / HALF_LENGTH));
        CRGB color = ctx.palette.getColor(paletteIdx, 255);

        // Apply brightness
        uint8_t bright = (uint8_t)(gapBright * (float)ctx.brightness);
        color = color.nscale8(bright);

        // PRE_SCALE
        color.r = scale8(color.r, GAP_PRE_SCALE);
        color.g = scale8(color.g, GAP_PRE_SCALE);
        color.b = scale8(color.b, GAP_PRE_SCALE);

        // Apply to Strip 2 (CENTER ORIGIN)
        uint16_t leftIdx2 = 239 - dist;
        uint16_t rightIdx2 = 240 + dist;

        if (leftIdx2 >= 160 && leftIdx2 < ctx.ledCount) {
            ctx.leds[leftIdx2].r = qadd8(ctx.leds[leftIdx2].r, color.r);
            ctx.leds[leftIdx2].g = qadd8(ctx.leds[leftIdx2].g, color.g);
            ctx.leds[leftIdx2].b = qadd8(ctx.leds[leftIdx2].b, color.b);
        }
        if (rightIdx2 >= 160 && rightIdx2 < ctx.ledCount) {
            ctx.leds[rightIdx2].r = qadd8(ctx.leds[rightIdx2].r, color.r);
            ctx.leds[rightIdx2].g = qadd8(ctx.leds[rightIdx2].g, color.g);
            ctx.leds[rightIdx2].b = qadd8(ctx.leds[rightIdx2].b, color.b);
        }
    }
#endif  // FEATURE_AUDIO_SYNC
}

uint16_t LGPSpectrumDetailEnhancedEffect::binToLedDistance(uint8_t bin) const {
    // REVERSED: High frequencies at center, low frequencies at edges
    // bin 63 (treble) -> distance 0 (center)
    // bin 0 (bass) -> distance 80 (edge)
    // This creates center-to-edges visual flow
    
    if (bin == 63) {
        return 0;  // Treble at center
    }
    
    // Calculate normalized position (0.0 = bin 0, 1.0 = bin 63)
    float logPos = log10f((float)(bin + 1) / 64.0f);
    float normalized = (logPos + 1.806f) / 1.806f;
    normalized = fmaxf(0.0f, fminf(1.0f, normalized));
    
    // REVERSE: invert normalized (1.0 - normalized)
    // Now: bin 0 → normalized=0 → reversed=1.0 → distance=80
    //      bin 63 → normalized=1.0 → reversed=0.0 → distance=0
    float reversed = 1.0f - normalized;
    uint16_t distance = (uint16_t)(reversed * (float)HALF_LENGTH);
    
    return distance;
}

CRGB LGPSpectrumDetailEnhancedEffect::frequencyToColor(uint8_t bin, const plugins::EffectContext& ctx) const {
    // Map bin to FULL palette range (0-255) with gHue offset for animation
    // bin 0 → 0, bin 63 → 255 (covers entire palette)
    float prog = (float)bin / 63.0f;  // 0.0 to 1.0
    uint8_t paletteIdx = (uint8_t)(prog * 255.0f) + ctx.gHue;
    return ctx.palette.getColor(paletteIdx, 255);  // Full brightness - scaled later in render
}

void LGPSpectrumDetailEnhancedEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPSpectrumDetailEnhancedEffect::getMetadata() const {
    static plugins::EffectMetadata meta(
        "LGP Spectrum Detail Enhanced",
        "Enhanced: Sensory Bridge pattern - 4-frame history, symmetric 0.75 smoothing",
        plugins::EffectCategory::PARTY,
        1
    );
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
