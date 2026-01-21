// LGPStarBurstNarrativeEffect.cpp
/**
 * @file LGPStarBurstNarrativeEffect.cpp
 * @brief LGP Star Burst effect implementation (narrative conductor + coherent color/motion)
 *
 * MIS Phase 2 Integration:
 * - Uses BehaviorSelection to adapt rendering based on music style
 * - StyleTiming adjusts state machine for different music types
 * - PaletteStrategy controls when/how palette changes occur
 * - SaliencyEmphasis weights visual dimensions based on what's salient
 */

#include "LGPStarBurstNarrativeEffect.h"
#include "../CoreEffects.h"      // STRIP_LENGTH, HALF_LENGTH, centerPairDistance(...)
#include "../../config/features.h"
#include <FastLED.h>             // fadeToBlackBy
#include <cmath>                 // sinf, expf, fabsf, fmodf, roundf

namespace lightwaveos {
namespace effects {
namespace ieffect {

// ------------------------------
// Small math helpers (no malloc)
// ------------------------------

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static inline uint8_t clampU8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

// Asymmetric one-pole smoother:
// - fast rise, slower fall (human-perceptual friendly)
static float smoothValue(float current, float target, float rise, float fall) {
    const float alpha = (target > current) ? rise : fall;
    return current + (target - current) * alpha;
}

// -----------------------------------------
// Enhancement 2: Behavior blend helpers
// -----------------------------------------

// Get behavior-specific burst multiplier for blending
static float getBehaviorBurstMultiplier(plugins::VisualBehavior behavior) {
    switch (behavior) {
        case plugins::VisualBehavior::PULSE_ON_BEAT: return 1.5f;      // Strong bursts
        case plugins::VisualBehavior::BREATHE_WITH_DYNAMICS: return 0.8f;  // Moderate
        case plugins::VisualBehavior::TEXTURE_FLOW: return 0.4f;       // Subtle
        case plugins::VisualBehavior::DRIFT_WITH_HARMONY: return 0.6f; // Medium
        case plugins::VisualBehavior::SHIMMER_WITH_MELODY: return 0.7f; // Medium-light
        default: return 1.0f;
    }
}

// Get behavior-specific shimmer intensity for blending
static float getBehaviorShimmerIntensity(plugins::VisualBehavior behavior) {
    switch (behavior) {
        case plugins::VisualBehavior::SHIMMER_WITH_MELODY: return 1.0f; // Full shimmer
        case plugins::VisualBehavior::TEXTURE_FLOW: return 0.6f;        // Some shimmer
        case plugins::VisualBehavior::DRIFT_WITH_HARMONY: return 0.3f;  // Subtle
        default: return 0.0f;  // No shimmer
    }
}

// 0..1 smoothstep with explicit duration
static inline float smoothstepDur(float t, float duration) {
    if (duration <= 0.0f) return 1.0f;
    float x = clamp01(t / duration);
    return x * x * (3.0f - 2.0f * x);
}

// Exponential decay toward 0 with time constant tau (seconds)
static inline float expDecay(float value, float dt, float tauSeconds) {
    if (tauSeconds <= 0.0f) return 0.0f;
    return value * expf(-dt / tauSeconds);
}

// Linear interpolation
static inline float lerp(float a, float b, float t) {
    return a + (b - a) * clamp01(t);
}

// Priority 3: Helper to detect and handle chord changes
static void handleChordChange(uint8_t& prevRootBin, bool& prevMinor, 
                              uint8_t newRootBin, bool newMinor,
                              float& transitionProgress) {
    if (prevRootBin != newRootBin || prevMinor != newMinor) {
        prevRootBin = newRootBin;
        prevMinor = newMinor;
        transitionProgress = 0.0f;  // Start transition
    }
}

LGPStarBurstNarrativeEffect::LGPStarBurstNarrativeEffect()
    : m_phase(0.0f)
{
}

bool LGPStarBurstNarrativeEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Reset story conductor
    m_storyPhase   = StoryPhase::REST;
    m_storyTimeS   = 0.0f;
    m_quietTimeS   = 0.0f;
    m_phraseHoldS  = 10.0f; // allow immediate first commit
    m_chordChangePulse = 0.0f;

    // Reset key/palette gating
    m_candidateRootBin = 0;
    m_candidateMinor   = false;
    m_keyRootBin       = 0;
    m_keyMinor         = false;
    m_keyRootBinSmooth = 0.0f;

    // Reset audio features
    m_phase = 0.0f;
    m_burst = 0.0f;
    m_lastHopSeq = 0;

    m_chromaEnergySum = 0.0f;
    m_chromaHistIdx = 0;
    for (uint8_t i = 0; i < CHROMA_HISTORY; ++i) {
        m_chromaEnergyHist[i] = 0.0f;
    }

    m_energyAvg = 0.0f;
    m_energyDelta = 0.0f;
    m_dominantBin = 0;

    m_energyAvgSmooth = 0.0f;
    m_energyDeltaSmooth = 0.0f;
    m_dominantBinSmooth = 0.0f;

    // -----------------------------------------
    // MIS Phase 2: Initialize behavior selection
    // -----------------------------------------
    m_currentBehavior = plugins::VisualBehavior::DRIFT_WITH_HARMONY;
    m_paletteStrategy = plugins::PaletteStrategy::HARMONIC_COMMIT;
    m_styleTiming = plugins::StyleTiming::forStyle(audio::MusicStyle::UNKNOWN);
    m_saliencyEmphasis = plugins::SaliencyEmphasis::neutral();
    m_shimmerPhase = 0.0f;
    m_styleBlend = 0.0f;
    m_prevStyle = audio::MusicStyle::UNKNOWN;

    // -----------------------------------------
    // Enhancement 1: Dynamic Color Warmth
    // -----------------------------------------
    m_warmthOffset = 0.0f;

    // -----------------------------------------
    // Enhancement 2: Behavior Transition Blending
    // -----------------------------------------
    m_prevBehavior = plugins::VisualBehavior::DRIFT_WITH_HARMONY;
    m_behaviorBlend = 1.0f;  // Start fully blended to current

    // -----------------------------------------
    // Enhancement 3: Texture Layer
    // -----------------------------------------
    m_texturePhase = 0.0f;
    m_textureIntensity = 0.0f;
    m_fluxSmooth = 0.0f;

    // -----------------------------------------
    // 64-bin Spectrum Enhancement
    // -----------------------------------------
    m_kickBurst = 0.0f;
    m_trebleShimmerIntensity = 0.0f;
    
    // Initialize audio smoothing
    m_targetRms = 0.0f;
    m_rmsFollower.reset(0.0f);
    
    // Priority 1: Initialize mood-adjusted followers
    m_targetEnergyDelta = 0.0f;
    m_targetFlux = 0.0f;
    m_targetKickBurst = 0.0f;
    m_energyDeltaFollower.reset(0.0f);
    m_fluxFollower.reset(0.0f);
    m_kickBurstFollower.reset(0.0f);
    
    // Priority 3: Initialize chord progression tracking
    m_prevKeyRootBin = 0;
    m_prevKeyMinor = false;
    m_chordTransitionProgress = 1.0f;
    
    // Priority 2: Initialize ray count
    m_rayCount = 2.0f;
    
    // Priority 4: Initialize beat alignment
    m_beatAlignedPhaseOffset = 0.0f;
    m_beatAlignmentActive = false;
    
    // Priority 6: Initialize burst shape
    m_burstShape = BurstShape::EXPONENTIAL;

    return true;
}

void LGPStarBurstNarrativeEffect::render(plugins::EffectContext& ctx) {
    // ------------------------------
    // Normalize controls + time step
    // ------------------------------
    const float speedNorm = ctx.speed / 50.0f;         // typical 0..~2
    const float intensityNorm = ctx.brightness / 255.0f;
    const float dt = ctx.deltaTimeMs * 0.001f;         // seconds

    const bool hasAudio = ctx.audio.available;

#if FEATURE_AUDIO_SYNC
    const bool newHop = hasAudio && (ctx.audio.controlBus.hop_seq != m_lastHopSeq);

    // -----------------------------------------
    // MIS Phase 2: BEHAVIOR SELECTION UPDATE
    // -----------------------------------------
    if (hasAudio) {
        // Get behavior recommendation from style + saliency
        plugins::BehaviorContext behaviorCtx = plugins::selectBehavior(
            ctx.audio.musicStyle(),
            ctx.audio.saliencyFrame(),
            ctx.audio.styleConfidence()
        );

        // -----------------------------------------
        // Enhancement 2: BEHAVIOR TRANSITION BLENDING
        // -----------------------------------------
        plugins::VisualBehavior newBehavior = behaviorCtx.recommendedPrimary;
        if (newBehavior != m_currentBehavior) {
            // Behavior changed: start blending from old to new
            m_prevBehavior = m_currentBehavior;
            m_currentBehavior = newBehavior;
            m_behaviorBlend = 0.0f;  // Start at old behavior
        }

        // Blend toward new behavior - rate varies by target
        float blendRate = (m_currentBehavior == plugins::VisualBehavior::PULSE_ON_BEAT) ? 2.0f :
                          (m_currentBehavior == plugins::VisualBehavior::TEXTURE_FLOW) ? 0.8f : 1.3f;
        m_behaviorBlend = clamp01(m_behaviorBlend + dt * blendRate);

        m_paletteStrategy = plugins::selectPaletteStrategy(ctx.audio.musicStyle());

        // Update saliency emphasis
        m_saliencyEmphasis = plugins::SaliencyEmphasis::fromSaliency(ctx.audio.saliencyFrame());

        // -----------------------------------------
        // Enhancement 1: DYNAMIC COLOR WARMTH
        // -----------------------------------------
        // Smooth RMS with mood-adjusted smoothing
        float moodNorm = ctx.getMoodNormalized();
        float rmsNorm = m_rmsFollower.updateWithMood(m_targetRms, dt, moodNorm);
        float targetWarmth = (rmsNorm - 0.5f) * 60.0f;  // Map 0-1 to -30..+30

        // Style-aware scaling: strongest for DYNAMIC, subtle for RHYTHMIC
        float warmthScale = (ctx.audio.musicStyle() == audio::MusicStyle::DYNAMIC_DRIVEN) ? 1.5f :
                            (ctx.audio.musicStyle() == audio::MusicStyle::RHYTHMIC_DRIVEN) ? 0.4f :
                            (ctx.audio.musicStyle() == audio::MusicStyle::TEXTURE_DRIVEN) ? 0.8f : 0.6f;
        targetWarmth *= warmthScale;

        // Asymmetric smoothing: fast rise on crescendos, slow fall on decrescendos
        const float warmthRise = dt / (0.15f + dt);  // ~150ms rise
        const float warmthFall = dt / (0.60f + dt);  // ~600ms fall
        m_warmthOffset = smoothValue(m_warmthOffset, targetWarmth, warmthRise, warmthFall);

        // Smooth style transitions to prevent jarring switches
        audio::MusicStyle currentStyle = ctx.audio.musicStyle();
        if (currentStyle != m_prevStyle) {
            // Style changed: start blending to new timing
            m_styleBlend = 0.0f;
            m_prevStyle = currentStyle;
        }

        // Blend toward current style's timing over ~2 seconds
        m_styleBlend = clamp01(m_styleBlend + dt * 0.5f);

        // Get target timing for current style
        plugins::StyleTiming targetTiming = plugins::StyleTiming::forStyle(currentStyle);

        // Interpolate timing parameters based on blend
        m_styleTiming.phraseGateDuration = lerp(m_styleTiming.phraseGateDuration, targetTiming.phraseGateDuration, m_styleBlend * dt * 2.0f);
        m_styleTiming.buildThreshold = lerp(m_styleTiming.buildThreshold, targetTiming.buildThreshold, m_styleBlend * dt * 2.0f);
        m_styleTiming.holdDuration = lerp(m_styleTiming.holdDuration, targetTiming.holdDuration, m_styleBlend * dt * 2.0f);
        m_styleTiming.releaseSpeed = lerp(m_styleTiming.releaseSpeed, targetTiming.releaseSpeed, m_styleBlend * dt * 2.0f);
        m_styleTiming.quietThreshold = lerp(m_styleTiming.quietThreshold, targetTiming.quietThreshold, m_styleBlend * dt * 2.0f);
        m_styleTiming.colorTransitionSpeed = lerp(m_styleTiming.colorTransitionSpeed, targetTiming.colorTransitionSpeed, m_styleBlend * dt * 2.0f);
        m_styleTiming.motionTransitionSpeed = lerp(m_styleTiming.motionTransitionSpeed, targetTiming.motionTransitionSpeed, m_styleBlend * dt * 2.0f);
        m_styleTiming.attackMultiplier = lerp(m_styleTiming.attackMultiplier, targetTiming.attackMultiplier, m_styleBlend * dt * 2.0f);
        m_styleTiming.decayMultiplier = lerp(m_styleTiming.decayMultiplier, targetTiming.decayMultiplier, m_styleBlend * dt * 2.0f);
    }

    // -----------------------------------------
    // AUDIO FEATURE UPDATE (hop-gated)
    // -----------------------------------------
    if (hasAudio && newHop) {
        m_lastHopSeq = ctx.audio.controlBus.hop_seq;
        m_targetRms = ctx.audio.rms();

        // Transform chroma into a stable "brightness proxy"
        float maxBinVal = 0.0f;
        uint8_t dominantBin = 0;

        float chromaEnergyMean = 0.0f; // mean of 12 bins after transform (0..1)
        for (uint8_t i = 0; i < 12; ++i) {
            const float bin = ctx.audio.controlBus.chroma[i];
            float bright = bin * bin;     // emphasize strong notes
            bright *= 1.5f;
            bright = clamp01(bright);

            if (bright > maxBinVal) {
                maxBinVal = bright;
                dominantBin = i;
            }
            chromaEnergyMean += bright;
        }
        chromaEnergyMean *= (1.0f / 12.0f); // mean

        // Candidate tonal center from ChordState API (uses proper triad detection)
        // Falls back to dominant bin when chord confidence is low
        if (ctx.audio.chordConfidence() > 0.4f) {
            m_candidateRootBin = ctx.audio.rootNote();
            m_candidateMinor = ctx.audio.isMinor();
        } else {
            // Low confidence: use dominant chroma bin as fallback
            m_candidateRootBin = dominantBin;
            // No reliable third detection at low confidence
        }

        // 4-hop moving baseline to compute novelty (energyDelta)
        m_chromaEnergySum -= m_chromaEnergyHist[m_chromaHistIdx];
        m_chromaEnergyHist[m_chromaHistIdx] = chromaEnergyMean;
        m_chromaEnergySum += chromaEnergyMean;
        m_chromaHistIdx = (uint8_t)((m_chromaHistIdx + 1) % CHROMA_HISTORY);

        m_energyAvg = m_chromaEnergySum / (float)CHROMA_HISTORY;

        m_energyDelta = chromaEnergyMean - m_energyAvg;
        if (m_energyDelta < 0.0f) m_energyDelta = 0.0f;

        m_dominantBin = dominantBin;

        // =====================================================================
        // 64-bin KICK BURST (bins 0-5 = 110-155 Hz)
        // BYPASSES story conductor for immediate sub-bass response.
        // Deep kick drums trigger instant starburst expansion regardless of
        // what phase the story conductor is in. This gives punchy response.
        // Priority 1: Now uses mood-adjusted smoothing
        // =====================================================================
        float kickSum = 0.0f;
        for (uint8_t i = 0; i < 6; ++i) {
            kickSum += ctx.audio.bin(i);
        }
        float kickAvg = kickSum / 6.0f;
        m_targetKickBurst = kickAvg;  // Update target every hop
        
        // Priority 1: Smooth with mood-adjusted follower (fast attack, fast decay)
        float moodNorm = ctx.getMoodNormalized();
        float dt = ctx.deltaTimeMs * 0.001f;
        m_kickBurst = m_kickBurstFollower.updateWithMood(m_targetKickBurst, dt, moodNorm);

        // Direct burst injection when kick is strong (bypasses story conductor)
        if (m_kickBurst > 0.4f) {
            m_burst = fmaxf(m_burst, m_kickBurst * 0.8f);
        }

        // =====================================================================
        // 64-bin TREBLE SHIMMER (bins 48-63 = 1.3-4.2 kHz)
        // Hi-hat and cymbal energy for enhanced shimmer layer intensity.
        // =====================================================================
        float trebleSum = 0.0f;
        for (uint8_t i = 48; i < 64; ++i) {
            trebleSum += ctx.audio.binAdaptive(i);
        }
        m_trebleShimmerIntensity = trebleSum / 16.0f;

        // -----------------------------------------
        // BEHAVIOR-ADAPTIVE IMPACT TRIGGERS
        // -----------------------------------------
        // Different behaviors have different trigger thresholds and responses
        const float impactThreshold =
            (m_currentBehavior == plugins::VisualBehavior::PULSE_ON_BEAT) ? 0.03f :  // Lower threshold for rhythmic
            (m_currentBehavior == plugins::VisualBehavior::TEXTURE_FLOW) ? 0.08f :   // Higher threshold for texture
            0.05f;  // Default

        if (m_energyDelta > impactThreshold) {
            m_burst = 1.0f;
        }

        // -----------------------------------------
        // PALETTE STRATEGY: When to commit palette changes
        // -----------------------------------------
        switch (m_paletteStrategy) {
            case plugins::PaletteStrategy::RHYTHMIC_SNAP:
                // Commit on beat during HOLD phase
                if (ctx.audio.isOnBeat() && m_storyPhase == StoryPhase::HOLD) {
                    // Priority 3: Detect chord change
                    handleChordChange(m_prevKeyRootBin, m_prevKeyMinor, 
                                     m_candidateRootBin, m_candidateMinor,
                                     m_chordTransitionProgress);
                    m_keyRootBin = m_candidateRootBin;
                    m_keyMinor = m_candidateMinor;
                    m_phraseHoldS = 0.0f;
                }
                break;

            case plugins::PaletteStrategy::HARMONIC_COMMIT:
                // Original behavior: commit when phrase gate allows
                // (handled in state machine below)
                break;

            case plugins::PaletteStrategy::MELODIC_DRIFT:
                // Continuous slow drift toward candidate
                if (m_storyPhase != StoryPhase::REST) {
                    // Drift root toward candidate slowly
                    float driftRate = dt * 0.3f;
                    float targetRoot = (float)m_candidateRootBin;
                    m_keyRootBinSmooth += (targetRoot - m_keyRootBinSmooth) * driftRate;
                    m_keyMinor = m_candidateMinor;  // Follow chord quality
                }
                break;

            case plugins::PaletteStrategy::TEXTURE_EVOLVE:
                // Very slow evolution, based on flux
                if (ctx.audio.flux() > 0.3f && m_phraseHoldS > m_styleTiming.phraseGateDuration) {
                    // Priority 3: Detect chord change
                    handleChordChange(m_prevKeyRootBin, m_prevKeyMinor, 
                                     m_candidateRootBin, m_candidateMinor,
                                     m_chordTransitionProgress);
                    m_keyRootBin = m_candidateRootBin;
                    m_keyMinor = m_candidateMinor;
                    m_phraseHoldS = 0.0f;
                }
                break;

            case plugins::PaletteStrategy::DYNAMIC_WARMTH:
                // Commit on dynamic peaks
                if (ctx.audio.rms() > 0.6f && m_phraseHoldS > m_styleTiming.phraseGateDuration * 0.5f) {
                    // Priority 3: Detect chord change
                    handleChordChange(m_prevKeyRootBin, m_prevKeyMinor, 
                                     m_candidateRootBin, m_candidateMinor,
                                     m_chordTransitionProgress);
                    m_keyRootBin = m_candidateRootBin;
                    m_keyMinor = m_candidateMinor;
                    m_phraseHoldS = 0.0f;
                }
                break;
        }
    } else
#endif
    if (!hasAudio) {
        // No audio: decay toward calm
        m_energyAvg *= 0.98f;
        m_energyDelta = 0.0f;
    }

    // -----------------------------------------
    // SMOOTHING (dt-aware, asymmetric)
    // Priority 1: Use mood-adjusted smoothing for energyDelta
    // -----------------------------------------
    const float riseAvg  = dt / (0.20f + dt);
    const float fallAvg  = dt / (0.50f + dt);
    const float alphaBin = dt / (0.25f + dt);

    m_energyAvgSmooth   = smoothValue(m_energyAvgSmooth,   m_energyAvg,   riseAvg, fallAvg);
    
    // Priority 1: Replace manual smoothing with mood-adjusted follower
    float moodNorm = ctx.getMoodNormalized();
    m_energyDeltaSmooth = m_energyDeltaFollower.updateWithMood(m_targetEnergyDelta, dt, moodNorm);
    if (m_energyDeltaSmooth < 0.0f) m_energyDeltaSmooth = 0.0f;  // Clamp negative values

    m_dominantBinSmooth += ((float)m_dominantBin - m_dominantBinSmooth) * alphaBin;
    if (m_dominantBinSmooth < 0.0f) m_dominantBinSmooth = 0.0f;
    if (m_dominantBinSmooth > 11.0f) m_dominantBinSmooth = 11.0f;

    // -----------------------------------------
    // STORY CONDUCTOR UPDATE
    // -----------------------------------------
    m_storyTimeS  += dt;
    m_phraseHoldS += dt;

    const bool quietNow = (m_energyAvgSmooth < 0.08f) && (m_energyDeltaSmooth < 0.015f);
    if (quietNow) m_quietTimeS += dt;
    else          m_quietTimeS = 0.0f;

    // Decay chord change pulse (snare-driven visual accent)
    m_chordChangePulse = expDecay(m_chordChangePulse, dt, 0.15f);

#if FEATURE_AUDIO_SYNC
    // Snare-driven story phase transitions
    if (hasAudio && ctx.audio.isSnareHit()) {
        if (m_storyPhase == StoryPhase::REST) {
            // Wake up on snare hit
            m_storyPhase = StoryPhase::BUILD;
            m_storyTimeS = 0.0f;
        } else if (m_storyPhase == StoryPhase::HOLD) {
            // Trigger chord change pulse on snare during HOLD
            m_chordChangePulse = 1.0f;
        }
    }
#endif

    // -----------------------------------------
    // STYLE-ADAPTIVE STATE MACHINE
    // -----------------------------------------
    // Timing constants now come from m_styleTiming which adapts to music style
    // - RHYTHMIC: shorter states, snappier transitions
    // - HARMONIC: longer build phases, smoother palette transitions
    // - TEXTURE: very long phrase gates, organic motion
    const float phraseGate = m_styleTiming.phraseGateDuration;
    const float buildThreshold = m_styleTiming.buildThreshold;
    const float holdDur = m_styleTiming.holdDuration;
    const float releaseMultiplier = m_styleTiming.releaseSpeed;
    const float quietThresh = m_styleTiming.quietThreshold;

    // REST -> BUILD: wake up (quiet->active) => commit palette/key (phrase gate)
    // BUILD -> HOLD: sustained energy
    // HOLD -> RELEASE: energy drops or quiet persists
    // RELEASE -> REST: quiet persists
    switch (m_storyPhase) {
        case StoryPhase::REST:
            // Style-adaptive entry threshold
            if (!quietNow && m_phraseHoldS > 0.6f) {
                // Commit palette on phrase gate (HARMONIC_COMMIT strategy default)
                if (m_paletteStrategy == plugins::PaletteStrategy::HARMONIC_COMMIT &&
                    m_phraseHoldS > phraseGate) {
                    // Priority 3: Detect chord change
                    handleChordChange(m_prevKeyRootBin, m_prevKeyMinor, 
                                     m_candidateRootBin, m_candidateMinor,
                                     m_chordTransitionProgress);
                    m_keyRootBin = m_candidateRootBin;
                    m_keyMinor   = m_candidateMinor;
                    m_phraseHoldS = 0.0f;
                }
                m_storyPhase = StoryPhase::BUILD;
                m_storyTimeS = 0.0f;
            }
            break;

        case StoryPhase::BUILD:
            // Style-adaptive quiet detection
            if (m_quietTimeS > quietThresh * 0.8f) {
                m_storyPhase = StoryPhase::REST;
                m_storyTimeS = 0.0f;
                break;
            }
            // Style-adaptive energy threshold for HOLD entry
            if (m_storyTimeS > (1.2f / releaseMultiplier) && m_energyAvgSmooth > buildThreshold) {
                m_storyPhase = StoryPhase::HOLD;
                m_storyTimeS = 0.0f;
            }
            break;

        case StoryPhase::HOLD:
            // Style-adaptive quiet threshold
            if (m_quietTimeS > quietThresh) {
                m_storyPhase = StoryPhase::RELEASE;
                m_storyTimeS = 0.0f;
                break;
            }
            // Style-adaptive hold duration and exit threshold
            if (m_storyTimeS > holdDur && m_energyAvgSmooth < (buildThreshold * 0.9f)) {
                m_storyPhase = StoryPhase::RELEASE;
                m_storyTimeS = 0.0f;
            }
            break;

        case StoryPhase::RELEASE:
            if (m_quietTimeS > quietThresh * 1.3f) {
                m_storyPhase = StoryPhase::REST;
                m_storyTimeS = 0.0f;
                break;
            }
            // Style-adaptive release speed
            if (m_storyTimeS > (1.0f / releaseMultiplier) && !quietNow) {
                m_storyPhase = StoryPhase::BUILD;
                m_storyTimeS = 0.0f;
            }
            break;
    }

    // Priority 3: Update chord transition progress
    if (m_chordTransitionProgress < 1.0f) {
        // Transition duration: 0.5-1.0 seconds based on style
        float transitionDuration = 0.5f + 0.5f * (1.0f - m_styleTiming.colorTransitionSpeed);
        m_chordTransitionProgress = clamp01(m_chordTransitionProgress + dt / transitionDuration);
    }
    
    // Smooth committed root bin - rate adapts to style
    // RHYTHMIC: faster color changes (colorTransitionSpeed = 0.8)
    // HARMONIC: slower, more intentional (colorTransitionSpeed = 0.3)
    const float colorSmoothTau = 0.35f / m_styleTiming.colorTransitionSpeed;
    m_keyRootBinSmooth += ((float)m_keyRootBin - m_keyRootBinSmooth) * (dt / (colorSmoothTau + dt));
    if (m_keyRootBinSmooth < 0.0f) m_keyRootBinSmooth = 0.0f;
    if (m_keyRootBinSmooth > 11.0f) m_keyRootBinSmooth = 11.0f;

    // Story envelope 0..1 - durations adapt to style
    const float buildDur = 1.2f / m_styleTiming.releaseSpeed;
    const float releaseDur = 1.0f * m_styleTiming.decayMultiplier;

    float env = 0.0f;
    if (m_storyPhase == StoryPhase::REST) {
        env = 0.0f;
    } else if (m_storyPhase == StoryPhase::BUILD) {
        env = smoothstepDur(m_storyTimeS, buildDur);
    } else if (m_storyPhase == StoryPhase::HOLD) {
        env = 1.0f;
    } else { // RELEASE
        env = 1.0f - smoothstepDur(m_storyTimeS, releaseDur);
    }
    env = clamp01(env);

    // -----------------------------------------
    // PHASE + IMPACT DYNAMICS (ChevronWaves golden pattern)
    // -----------------------------------------
    // FIX: Use heavy_bands (pre-smoothed) and slew limiting to prevent jog-dial jitter
    // This replaces the over-complex multi-factor speed modulation that caused motion chaos

    // Use heavy_bands instead of raw energy - already smoothed by audio pipeline
    float heavyEnergy = 0.0f;
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        heavyEnergy = (ctx.audio.controlBus.heavy_bands[1] +
                       ctx.audio.controlBus.heavy_bands[2]) / 2.0f;
    }
#endif
    // =========================================================================
    // SPEED CONTROL FIX: Match LGPInterferenceScannerEffect golden pattern
    // Audio modulates speed in [0.6, 1.4] range, user slider (speedNorm) is
    // multiplicative on top. Slew limiting prevents jog-dial jitter.
    // =========================================================================
    // Target range: [0.6, 1.4] based on audio energy (narrower than before)
    float targetSpeed = 0.6f + 0.8f * heavyEnergy;
    if (targetSpeed > 1.4f) targetSpeed = 1.4f;  // Upper clamp only

    // SLEW LIMITING (0.3/sec max change rate) - matches LGPInterferenceScannerEffect
    const float maxSlewRate = 0.3f;
    float slewLimit = maxSlewRate * dt;
    float speedDelta = targetSpeed - m_speedSmooth;
    if (speedDelta > slewLimit) speedDelta = slewLimit;
    if (speedDelta < -slewLimit) speedDelta = -slewLimit;
    m_speedSmooth += speedDelta;
    // No lower clamp - allow speed to go low when audio is quiet
    // User slider (speedNorm) provides full control range

    // Phase accumulation: monotonic, dt-corrected
    // FIX: Wrap EVERY frame to prevent discontinuity at wrap point
    // Priority 4: Beat alignment for rhythmic styles
    float phaseIncrement = speedNorm * 240.0f * m_speedSmooth * dt;
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        audio::MusicStyle style = ctx.audio.musicStyle();
        float styleConf = ctx.audio.styleConfidence();
        m_beatAlignmentActive = (style == audio::MusicStyle::RHYTHMIC_DRIVEN && styleConf > 0.6f);
        
        if (m_beatAlignmentActive && ctx.audio.isOnBeat()) {
            // Snap phase offset to beat-synchronized value
            // Align to 0, π/2, π, or 3π/2 based on beat position
            float beatPhase = fmodf(m_phase, 1.5708f);  // π/2
            m_beatAlignedPhaseOffset = -beatPhase;  // Snap to beat boundary
        }
        
        // Blend between free-running and beat-aligned
        float beatBlend = m_beatAlignmentActive ? (0.3f + 0.4f * styleConf) : 0.0f;
        phaseIncrement += m_beatAlignedPhaseOffset * beatBlend * dt * 2.0f;
        m_beatAlignedPhaseOffset *= (1.0f - dt * 5.0f);  // Decay offset
    }
#endif
    m_phase += phaseIncrement;
    m_phase = fmodf(m_phase, 6.2831853f);

    // Shimmer phase for SHIMMER_WITH_MELODY behavior
    // FIX: Wrap every frame
    m_shimmerPhase += dt * 15.0f;  // Fast shimmer
    m_shimmerPhase = fmodf(m_shimmerPhase, 6.2831853f);

    // -----------------------------------------
    // Enhancement 3: TEXTURE FLOW LAYER
    // Priority 1: Use mood-adjusted smoothing for flux
    // -----------------------------------------
#if FEATURE_AUDIO_SYNC
    const float rawFlux = hasAudio ? ctx.audio.flux() : 0.0f;
    // Priority 1: Update target and smooth with mood-adjusted follower
    m_targetFlux = rawFlux;
    // Reuse moodNorm from above (line 472)
    m_fluxSmooth = m_fluxFollower.updateWithMood(m_targetFlux, dt, moodNorm);
#else
    const float rawFlux = 0.0f;
    m_targetFlux = 0.0f;
    m_fluxSmooth = 0.0f;
#endif

    // Texture phase accumulator - rate modulated by flux
    const float textureRate = 3.0f * (0.5f + m_fluxSmooth);
    m_texturePhase += dt * textureRate * 6.2831853f;
    m_texturePhase = fmodf(m_texturePhase, 6.2831853f);

    // Texture intensity - active only for TEXTURE_FLOW behavior
    const float targetTextureIntensity = (m_currentBehavior == plugins::VisualBehavior::TEXTURE_FLOW)
        ? (0.8f + 0.2f * m_fluxSmooth) : 0.0f;
    m_textureIntensity = lerp(m_textureIntensity, targetTextureIntensity, dt * 2.0f);

    // -----------------------------------------
    // BEHAVIOR-ADAPTIVE BURST RESPONSE
    // -----------------------------------------
    // Attack/decay multipliers adapt burst to music style
    const float attackMult = m_styleTiming.attackMultiplier;
    const float decayMult = m_styleTiming.decayMultiplier;
    
    // Priority 6: Select burst shape based on behavior
    switch (m_currentBehavior) {
        case plugins::VisualBehavior::PULSE_ON_BEAT:
            m_burstShape = BurstShape::GAUSSIAN;  // Tight, focused
            break;
        case plugins::VisualBehavior::TEXTURE_FLOW:
            m_burstShape = BurstShape::LINEAR;  // Soft, diffuse
            break;
        case plugins::VisualBehavior::DRIFT_WITH_HARMONY:
            m_burstShape = BurstShape::EXPONENTIAL;  // Natural, current
            break;
        default:
            m_burstShape = BurstShape::EXPONENTIAL;
            break;
    }
    
    // Priority 2: Modulate ray count based on audio complexity
    // Higher harmonic saliency → more rays (harmonic series visualization)
    // Higher energy → more complex ray patterns
    float harmonicWeight = m_saliencyEmphasis.colorEmphasis;  // 0-1
    float energyWeight = clamp01(m_energyAvgSmooth * 2.0f);  // 0-1
    m_rayCount = 1.0f + 3.0f * (harmonicWeight * 0.6f + energyWeight * 0.4f);
    if (m_rayCount > 4.0f) m_rayCount = 4.0f;

    // Burst accumulation - sharper for RHYTHMIC, softer for HARMONIC
    // Enhancement 2: Use blended burst multiplier for smooth behavior transitions
    const float blendedBurstMult = lerp(getBehaviorBurstMultiplier(m_prevBehavior),
                                        getBehaviorBurstMultiplier(m_currentBehavior), m_behaviorBlend);
    m_burst += m_energyDeltaSmooth * 0.85f * attackMult * blendedBurstMult;
    if (m_burst > 1.0f) m_burst = 1.0f;

    // Burst decay - shorter for RHYTHMIC (punchy), longer for HARMONIC (sustained)
    const float burstTau = 0.18f * decayMult;
    m_burst = expDecay(m_burst, dt, burstTau);

    // -----------------------------------------
    // BEHAVIOR-SPECIFIC ADDITIONAL TRIGGERS
    // -----------------------------------------
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        switch (m_currentBehavior) {
            case plugins::VisualBehavior::PULSE_ON_BEAT:
                // Trigger burst on beat tick for snappy rhythmic response
                if (ctx.audio.isOnBeat()) {
                    m_burst = clamp01(m_burst + 0.6f * attackMult);
                }
                break;

            case plugins::VisualBehavior::BREATHE_WITH_DYNAMICS: {
                // Modulate burst with smoothed RMS for organic breathing
                // RMS is already smoothed above (in warmth calculation)
                float smoothedRms = m_rmsFollower.getCurrent();
                m_burst = lerp(m_burst, smoothedRms * 0.7f, dt * 2.0f);
                break;
            }

            case plugins::VisualBehavior::TEXTURE_FLOW:
                // Modulate with flux for organic texture
                m_burst = lerp(m_burst, ctx.audio.flux() * 0.5f, dt * 0.5f);
                break;

            default:
                break;
        }
    }
#endif

    // -----------------------------------------
    // TRAILS
    // -----------------------------------------
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // -----------------------------------------
    // RENDER (center-origin, saliency-weighted emphasis)
    // -----------------------------------------
    // Saliency emphasis weights visual dimensions:
    // - colorEmphasis: boost color changes when harmonic saliency dominates
    // - motionEmphasis: boost pulses when rhythmic saliency dominates
    // - textureEmphasis: boost shimmer when timbral saliency dominates
    // - intensityEmphasis: boost brightness when dynamic saliency dominates
    const float colorWeight = m_saliencyEmphasis.colorEmphasis;
    const float motionWeight = m_saliencyEmphasis.motionEmphasis;
    const float textureWeight = m_saliencyEmphasis.textureEmphasis;
    const float intensityWeight = m_saliencyEmphasis.intensityEmphasis;

    // Priority 3: Blend between old and new chord during transition
    float transitionT = smoothstepDur(m_chordTransitionProgress, 1.0f);
    uint8_t blendRootBin = (uint8_t)lerp((float)m_prevKeyRootBin, (float)m_keyRootBin, transitionT);
    bool blendMinor = (transitionT > 0.5f) ? m_keyMinor : m_prevKeyMinor;
    
    const uint8_t rootBin  = (uint8_t)roundf(m_keyRootBinSmooth);
    const uint8_t thirdBin = (uint8_t)((rootBin + (blendMinor ? 3 : 4)) % 12);
    const uint8_t fifthBin = (uint8_t)((rootBin + 7) % 12);
    
    // Priority 5: Harmonic series visualization (7th and 9th when harmonic saliency is high)
    uint8_t seventhBin = 0;
    uint8_t ninthBin = 0;
    float harmonicSeriesWeight = 0.0f;
    if (m_saliencyEmphasis.colorEmphasis > 0.6f) {
        harmonicSeriesWeight = (m_saliencyEmphasis.colorEmphasis - 0.6f) * 2.5f;  // 0-1 range
        seventhBin = (uint8_t)((rootBin + (blendMinor ? 10 : 11)) % 12);  // Minor 7th or Major 7th
        ninthBin = (uint8_t)((rootBin + 2) % 12);  // Major 9th
    }

    const uint8_t binStep  = (uint8_t)(255.0f / 12.0f);
    // Enhancement 1: Apply dynamic color warmth offset to all hues
    const int8_t warmthInt = (int8_t)roundf(m_warmthOffset);
    const uint8_t hueRoot  = (uint8_t)((int16_t)ctx.gHue + rootBin  * binStep + warmthInt);
    const uint8_t hueThird = (uint8_t)((int16_t)ctx.gHue + thirdBin * binStep + warmthInt);
    const uint8_t hueFifth = (uint8_t)((int16_t)ctx.gHue + fifthBin * binStep + warmthInt);
    const uint8_t hueSeventh = (uint8_t)((int16_t)ctx.gHue + seventhBin * binStep + warmthInt);
    const uint8_t hueNinth = (uint8_t)((int16_t)ctx.gHue + ninthBin * binStep + warmthInt);

    // Motion-weighted frequency and falloff
    const float freqBase   = 0.18f + 0.30f * env * (0.5f + 0.5f * motionWeight);
    const float falloff    = 3.2f  - 1.6f  * env;
    const float pulseRate  = (0.8f  + 2.4f  * env) * (0.7f + 0.6f * motionWeight);

    const uint8_t motionIdx = (uint8_t)(((uint32_t)(m_phase * 40.0f)) & 0xFF);

    const int maxLeds = (ctx.ledCount < STRIP_LENGTH) ? (int)ctx.ledCount : (int)STRIP_LENGTH;

    for (int i = 0; i < maxLeds; i++) {
        const float distFromCenter = (float)centerPairDistance((uint16_t)i);
        const float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Priority 2: Dynamic ray count with harmonic series frequencies
        float raySum = 0.0f;
        int rayCountInt = (int)m_rayCount;
        for (int r = 0; r < rayCountInt; r++) {
            float rayFreq = freqBase * (1.0f + (float)r);  // Harmonic series: 1x, 2x, 3x, 4x
            float rayPhase = m_phase * (1.0f + (float)r * 0.3f);  // Slight phase offset per ray
            float rayAmp = (r == 0) ? 1.0f : (0.35f / (float)(r + 1));  // Decreasing amplitude
            raySum += rayAmp * sinf(distFromCenter * rayFreq - rayPhase);
        }
        // Blend fractional ray count
        if (rayCountInt < 4) {
            float fract = m_rayCount - (float)rayCountInt;
            float rayFreq = freqBase * (1.0f + (float)rayCountInt);
            float rayPhase = m_phase * (1.0f + (float)rayCountInt * 0.3f);
            float rayAmp = 0.35f / (float)(rayCountInt + 2);
            raySum += fract * rayAmp * sinf(distFromCenter * rayFreq - rayPhase);
        }
        
        const float spatial = expf(-normalizedDist * falloff);
        const float pulse   = 0.35f + 0.65f * (0.5f + 0.5f * sinf(m_phase * pulseRate));

        float field = 0.5f + 0.5f * raySum;
        field *= spatial;
        field *= pulse;

        // Motion-weighted burst contribution with Priority 6: Burst shape variation
        if (env > 0.02f) {
            float burstDist = normalizedDist * (falloff + 0.6f);
            float burstFalloff = 0.0f;
            switch (m_burstShape) {
                case BurstShape::EXPONENTIAL:
                    burstFalloff = expf(-burstDist);
                    break;
                case BurstShape::LINEAR:
                    burstFalloff = fmaxf(0.0f, 1.0f - burstDist);
                    break;
                case BurstShape::POWER_LAW:
                    burstFalloff = 1.0f / (1.0f + burstDist * burstDist);
                    break;
                case BurstShape::GAUSSIAN:
                    burstFalloff = expf(-burstDist * burstDist * 2.0f);
                    break;
            }
            field += m_burst * env * (0.5f + 0.5f * motionWeight) * burstFalloff;
        }

        field = clamp01(field);
        field *= field; // contrast

        float brightF = field;
        if (m_storyPhase == StoryPhase::REST) {
            brightF *= 0.20f; // real rest
        } else if (m_storyPhase == StoryPhase::BUILD) {
            brightF *= (0.35f + 0.65f * env);
        }

        // Intensity-weighted brightness boost
        brightF *= (0.8f + 0.4f * intensityWeight);

        const uint8_t brightness = clampU8((int)roundf(brightF * intensityNorm * 255.0f));

        // Color-weighted palette index spread (more variation when harmonic salient)
        const int colorSpread = (int)(distFromCenter * (1.5f + 1.5f * colorWeight));
        const int baseIdx = colorSpread + (int)motionIdx;
        const uint8_t paletteIndex = clampU8(baseIdx);

        const float t = clamp01(normalizedDist);

        // Color-weighted triad balance (more third when harmonic salient)
        float wRoot  = clamp01(1.15f - 1.65f * t);
        float wFifth = clamp01(0.35f + 0.95f * t);
        float wThird = env * clamp01(1.0f - fabsf(t - 0.35f) * 3.0f) * (0.8f + 0.4f * colorWeight);
        
        // Priority 5: Add harmonic series weights (7th and 9th) when harmonic saliency is high
        float wSeventh = 0.0f;
        float wNinth = 0.0f;
        if (harmonicSeriesWeight > 0.0f) {
            // 7th appears at mid-distance, 9th at outer edge
            wSeventh = harmonicSeriesWeight * env * clamp01(1.0f - fabsf(t - 0.5f) * 2.5f) * 0.25f;
            wNinth = harmonicSeriesWeight * env * clamp01(t - 0.6f) * 0.20f;
        }

        const float wSum = (wRoot + wThird + wFifth + wSeventh + wNinth);
        if (wSum > 0.0001f) {
            wRoot  /= wSum;
            wThird /= wSum;
            wFifth /= wSum;
            wSeventh /= wSum;
            wNinth /= wSum;
        }

        const uint8_t bRoot  = clampU8((int)roundf(brightness * wRoot));
        const uint8_t bThird = clampU8((int)roundf(brightness * wThird));
        const uint8_t bFifth = clampU8((int)roundf(brightness * wFifth));
        const uint8_t bSeventh = clampU8((int)roundf(brightness * wSeventh));
        const uint8_t bNinth = clampU8((int)roundf(brightness * wNinth));

        CRGB c = CRGB::Black;
        if (bRoot)  c += ctx.palette.getColor((uint8_t)(hueRoot  + paletteIndex), bRoot);
        if (bThird) c += ctx.palette.getColor((uint8_t)(hueThird + paletteIndex), bThird);
        if (bFifth) c += ctx.palette.getColor((uint8_t)(hueFifth + paletteIndex), bFifth);
        if (bSeventh) c += ctx.palette.getColor((uint8_t)(hueSeventh + paletteIndex), bSeventh);
        if (bNinth) c += ctx.palette.getColor((uint8_t)(hueNinth + paletteIndex), bNinth);

        // Motion-weighted burst accent
        if (m_burst > 0.20f && env > 0.25f) {
            const uint8_t accentB = clampU8((int)roundf(brightness * m_burst * 0.55f * (0.7f + 0.6f * motionWeight)));
            c += ctx.palette.getColor((uint8_t)(hueRoot + 128 + paletteIndex), accentB);
        }

        // -----------------------------------------
        // BEHAVIOR-SPECIFIC: SHIMMER_WITH_MELODY sparkle layer
        // -----------------------------------------
        if (m_currentBehavior == plugins::VisualBehavior::SHIMMER_WITH_MELODY) {
            // Texture-weighted shimmer overlay
            const float shimmer = 0.5f + 0.5f * sinf(m_shimmerPhase + distFromCenter * 0.8f);
            // =====================================================================
            // 64-bin TREBLE BOOST: Hi-hat energy (bins 48-63) enhances shimmer
            // When cymbal/hi-hat is active, the sparkle layer intensifies.
            // =====================================================================
            const float trebleBoost = 1.0f + m_trebleShimmerIntensity * 0.5f;  // 1.0 to 1.5x
            const float shimmerIntensity = shimmer * textureWeight * env * 0.4f * trebleBoost;
            if (shimmerIntensity > 0.1f) {
                const uint8_t shimmerB = clampU8((int)roundf(brightness * shimmerIntensity));
                c += ctx.palette.getColor((uint8_t)(hueFifth + 32 + paletteIndex), shimmerB);
            }
        }

        // -----------------------------------------
        // Enhancement 3: TEXTURE FLOW LAYER (Strip 1)
        // -----------------------------------------
        // Organic wave overlay for TEXTURE_DRIVEN music
        if (m_textureIntensity > 0.05f && env > 0.1f) {
            const float wave1 = sinf(m_texturePhase + normalizedDist * 2.5f);
            const float wave2 = 0.5f * sinf(m_texturePhase * 0.7f - normalizedDist * 1.8f);
            const float textureFalloff = expf(-normalizedDist * 2.0f);
            const float textureField = (0.5f + 0.5f * (wave1 + wave2)) * textureFalloff;
            const float textureAmount = textureField * m_textureIntensity * env * textureWeight;

            if (textureAmount > 0.08f) {
                const uint8_t texB = clampU8((int)roundf(brightness * textureAmount * 0.35f));
                c += ctx.palette.getColor((uint8_t)(hueFifth + 48 + paletteIndex), texB);
            }
        }

        // Snare-driven chord change pulse: bright center flash
        if (m_chordChangePulse > 0.15f) {
            const float pulseFade = expf(-normalizedDist * 4.5f);  // Tight center focus
            const uint8_t pulseB = clampU8((int)roundf(brightness * m_chordChangePulse * pulseFade * 0.7f));
            c += ctx.palette.getColor((uint8_t)(hueFifth + 64 + paletteIndex), pulseB);
        }

        ctx.leds[i] = c;

        if (i + STRIP_LENGTH < ctx.ledCount) {
            // FIX: Changed from 85 to 90 to match ChevronWaves pattern
            const uint8_t harmonyShift = 90;

            CRGB c2 = CRGB::Black;
            if (bRoot)  c2 += ctx.palette.getColor((uint8_t)(hueRoot  + harmonyShift + paletteIndex), bRoot);
            if (bThird) c2 += ctx.palette.getColor((uint8_t)(hueThird + harmonyShift + paletteIndex), bThird);
            if (bFifth) c2 += ctx.palette.getColor((uint8_t)(hueFifth + harmonyShift + paletteIndex), bFifth);
            // Priority 5: Add harmonic series colors to Strip 2
            if (bSeventh) c2 += ctx.palette.getColor((uint8_t)(hueSeventh + harmonyShift + paletteIndex), bSeventh);
            if (bNinth) c2 += ctx.palette.getColor((uint8_t)(hueNinth + harmonyShift + paletteIndex), bNinth);

            if (m_burst > 0.20f && env > 0.25f) {
                const uint8_t accentB = clampU8((int)roundf(brightness * m_burst * 0.55f * (0.7f + 0.6f * motionWeight)));
                c2 += ctx.palette.getColor((uint8_t)(hueRoot + harmonyShift + 128 + paletteIndex), accentB);
            }

            // FIX: Removed shimmer layer that was ONLY on Strip 2 - caused visual incoherence
            // Shimmer created asymmetric motion between strips

            // -----------------------------------------
            // Enhancement 3: TEXTURE FLOW LAYER (Strip 2)
            // -----------------------------------------
            // Symmetric texture layer for visual coherence
            if (m_textureIntensity > 0.05f && env > 0.1f) {
                const float wave1 = sinf(m_texturePhase + normalizedDist * 2.5f);
                const float wave2 = 0.5f * sinf(m_texturePhase * 0.7f - normalizedDist * 1.8f);
                const float textureFalloff = expf(-normalizedDist * 2.0f);
                const float textureField = (0.5f + 0.5f * (wave1 + wave2)) * textureFalloff;
                const float textureAmount = textureField * m_textureIntensity * env * textureWeight;

                if (textureAmount > 0.08f) {
                    const uint8_t texB = clampU8((int)roundf(brightness * textureAmount * 0.35f));
                    c2 += ctx.palette.getColor((uint8_t)(hueFifth + harmonyShift + 48 + paletteIndex), texB);
                }
            }

            // Snare-driven chord change pulse for Strip 2
            if (m_chordChangePulse > 0.15f) {
                const float pulseFade = expf(-normalizedDist * 4.5f);
                const uint8_t pulseB = clampU8((int)roundf(brightness * m_chordChangePulse * pulseFade * 0.7f));
                c2 += ctx.palette.getColor((uint8_t)(hueFifth + harmonyShift + 64 + paletteIndex), pulseB);
            }

            ctx.leds[i + STRIP_LENGTH] = c2;
        }
    }
}

void LGPStarBurstNarrativeEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPStarBurstNarrativeEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Star Burst (Narrative)",
        "Center-origin starburst with adaptive style response (MIS Phase 2)",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
