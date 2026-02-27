/**
 * @file BehaviorSelection.h
 * @brief Visual behavior selection based on music style and saliency
 *
 * Part of the Musical Intelligence System (MIS) - Phase 2
 * Maps detected music styles to appropriate visual response behaviors,
 * enabling effects to adapt their rendering strategy based on what's
 * musically important in the current audio.
 *
 * Usage:
 * @code
 * #include "BehaviorSelection.h"
 * using namespace lightwaveos::plugins;
 *
 * void MyEffect::render(EffectContext& ctx) {
 *     BehaviorContext behavior = selectBehavior(
 *         ctx.audio.musicStyle(),
 *         ctx.audio.saliencyFrame(),
 *         ctx.audio.styleConfidence()
 *     );
 *
 *     switch (behavior.recommendedPrimary) {
 *         case VisualBehavior::PULSE_ON_BEAT:
 *             // Sharp expansion on beat...
 *             break;
 *         case VisualBehavior::DRIFT_WITH_HARMONY:
 *             // Slow color evolution...
 *             break;
 *         // ...
 *     }
 * }
 * @endcode
 */

#pragma once

#include <cstdint>
#include "../../config/features.h"

#if FEATURE_AUDIO_SYNC
#include "../../audio/contracts/StyleDetector.h"
#include "../../audio/contracts/MusicalSaliency.h"
#else
// Provide stub MusicStyle enum when audio is disabled
namespace audio {
enum class MusicStyle : uint8_t {
    UNKNOWN = 0,
    RHYTHMIC_DRIVEN = 1,
    HARMONIC_DRIVEN = 2,
    MELODIC_DRIVEN = 3,
    TEXTURE_DRIVEN = 4,
    DYNAMIC_DRIVEN = 5
};
} // namespace audio
#endif

namespace lightwaveos {
namespace plugins {

/**
 * @brief Visual behavior strategies for audio-reactive effects
 *
 * Each behavior represents a distinct visual response strategy optimized
 * for different types of musical content. Effects should select the
 * appropriate behavior based on the current music style and saliency.
 */
enum class VisualBehavior : uint8_t {
    /**
     * @brief Sharp expansion synced to beat (for rhythmic music)
     *
     * Characteristics:
     * - Quick, punchy response on beat detection
     * - High contrast between beat and off-beat states
     * - Expansion from center outward
     *
     * Best for: EDM, hip-hop, dance, any music with strong beats
     * Temporal class: REACTIVE (100-300ms)
     */
    PULSE_ON_BEAT = 0,

    /**
     * @brief Slow color evolution with chords (for harmonic music)
     *
     * Characteristics:
     * - Gradual color palette shifts following chord changes
     * - Smooth, organic transitions between states
     * - Emphasis on mood and emotional color
     *
     * Best for: Jazz, classical, chord-heavy progressions
     * Temporal class: SLOW (500ms-5s)
     */
    DRIFT_WITH_HARMONY = 1,

    /**
     * @brief Sparkle following treble (for melodic music)
     *
     * Characteristics:
     * - Fine-grained shimmer responding to high frequencies
     * - Spatial movement tracking melodic contour
     * - Light, airy feel with quick sparkle decay
     *
     * Best for: Vocal pop, lead instrument focus, melodic content
     * Temporal class: REACTIVE (100-300ms)
     */
    SHIMMER_WITH_MELODY = 2,

    /**
     * @brief Organic swell with RMS (for dynamic music)
     *
     * Characteristics:
     * - Smooth breathing motion following overall energy
     * - Intensity scaling with dynamics
     * - Natural, organic rhythm independent of beat
     *
     * Best for: Orchestral, cinematic, music with wide dynamic range
     * Temporal class: SUSTAINED (300ms-2s)
     */
    BREATHE_WITH_DYNAMICS = 3,

    /**
     * @brief Gradual morphing with flux (for textural music)
     *
     * Characteristics:
     * - Slow, continuous evolution following spectral changes
     * - Subtle texture variations
     * - Non-rhythmic, flowing motion
     *
     * Best for: Ambient, drone, atmospheric, textural content
     * Temporal class: SLOW (500ms-5s)
     */
    TEXTURE_FLOW = 4
};

/**
 * @brief Get human-readable name for visual behavior
 * @param behavior The visual behavior enum value
 * @return C-string name of the behavior
 */
inline const char* getVisualBehaviorName(VisualBehavior behavior) {
    switch (behavior) {
        case VisualBehavior::PULSE_ON_BEAT:       return "Pulse On Beat";
        case VisualBehavior::DRIFT_WITH_HARMONY:  return "Drift With Harmony";
        case VisualBehavior::SHIMMER_WITH_MELODY: return "Shimmer With Melody";
        case VisualBehavior::BREATHE_WITH_DYNAMICS: return "Breathe With Dynamics";
        case VisualBehavior::TEXTURE_FLOW:        return "Texture Flow";
        default:                                   return "Unknown";
    }
}

#if FEATURE_AUDIO_SYNC

/**
 * @brief Behavior selection context with recommendations
 *
 * Contains the current music style, confidence level, and recommended
 * visual behaviors. Effects can use the primary behavior for main response
 * and secondary behavior for subtle layering.
 */
struct BehaviorContext {
    /**
     * @brief Current detected music style
     */
    audio::MusicStyle currentStyle = audio::MusicStyle::UNKNOWN;

    /**
     * @brief Confidence in the detected style (0.0-1.0)
     *
     * Higher values indicate more certainty in style classification.
     * Effects may want to blend behaviors when confidence is low.
     */
    float styleConfidence = 0.0f;

    /**
     * @brief Primary recommended visual behavior
     *
     * This is the main behavior the effect should adopt based on
     * the current music style and saliency analysis.
     */
    VisualBehavior recommendedPrimary = VisualBehavior::BREATHE_WITH_DYNAMICS;

    /**
     * @brief Secondary recommended visual behavior
     *
     * Optional secondary behavior for layering or fallback.
     * Useful when the primary behavior doesn't fully capture
     * the musical content.
     */
    VisualBehavior recommendedSecondary = VisualBehavior::PULSE_ON_BEAT;

    /**
     * @brief Reference to the current saliency frame
     *
     * Provides access to detailed saliency metrics for fine-grained
     * behavior adjustments beyond the primary/secondary recommendations.
     */
    const audio::MusicalSaliencyFrame* saliencyFrame = nullptr;

    /**
     * @brief Check if style detection is confident enough to use
     * @param threshold Minimum confidence threshold (default 0.3)
     * @return true if styleConfidence >= threshold
     */
    bool isConfident(float threshold = 0.3f) const {
        return styleConfidence >= threshold;
    }

    /**
     * @brief Get blend factor for mixing primary and secondary behaviors
     *
     * Returns a value indicating how much to favor the primary behavior.
     * Low confidence = more blending with secondary.
     *
     * @return Blend factor (0.0-1.0), where 1.0 = 100% primary
     */
    float getPrimaryBlend() const {
        // Map confidence to blend: 0.0-0.3 conf -> 0.5-0.7 blend, 0.3-1.0 -> 0.7-1.0
        if (styleConfidence < 0.3f) {
            return 0.5f + (styleConfidence / 0.3f) * 0.2f;
        }
        return 0.7f + ((styleConfidence - 0.3f) / 0.7f) * 0.3f;
    }
};

#else

/**
 * @brief Stub BehaviorContext when FEATURE_AUDIO_SYNC is disabled
 *
 * Provides the same API with default behaviors so effects compile
 * without #if guards everywhere.
 */
struct BehaviorContext {
    uint8_t currentStyle = 0;  // UNKNOWN
    float styleConfidence = 0.0f;
    VisualBehavior recommendedPrimary = VisualBehavior::BREATHE_WITH_DYNAMICS;
    VisualBehavior recommendedSecondary = VisualBehavior::PULSE_ON_BEAT;

    bool isConfident(float threshold = 0.3f) const {
        (void)threshold;
        return false;
    }

    float getPrimaryBlend() const {
        return 0.5f;
    }
};

/**
 * @brief Stub PaletteStrategy when FEATURE_AUDIO_SYNC is disabled
 */
enum class PaletteStrategy : uint8_t {
    RHYTHMIC_SNAP = 0,
    HARMONIC_COMMIT = 1,
    MELODIC_DRIFT = 2,
    TEXTURE_EVOLVE = 3,
    DYNAMIC_WARMTH = 4
};

/**
 * @brief Stub StyleTiming when FEATURE_AUDIO_SYNC is disabled
 */
struct StyleTiming {
    float phraseGateDuration = 2.0f;
    float buildThreshold = 0.20f;
    float holdDuration = 2.5f;
    float releaseSpeed = 0.8f;
    float quietThreshold = 0.6f;
    float colorTransitionSpeed = 0.5f;
    float motionTransitionSpeed = 0.5f;
    float attackMultiplier = 1.0f;
    float decayMultiplier = 1.0f;

    static StyleTiming forStyle(audio::MusicStyle) {
        return StyleTiming{};
    }
};

/**
 * @brief Stub SaliencyEmphasis when FEATURE_AUDIO_SYNC is disabled
 */
struct SaliencyEmphasis {
    float colorEmphasis = 0.25f;
    float motionEmphasis = 0.25f;
    float textureEmphasis = 0.25f;
    float intensityEmphasis = 0.25f;

    static SaliencyEmphasis neutral() {
        return {0.25f, 0.25f, 0.25f, 0.25f};
    }
};

#endif

#if FEATURE_AUDIO_SYNC

/**
 * @brief Select visual behavior based on music style and saliency
 *
 * Maps the detected music style to an appropriate visual behavior.
 * When style is UNKNOWN or confidence is low, falls back to
 * saliency-based selection using the dominant saliency type.
 *
 * @param style Current detected music style
 * @param saliency Current musical saliency frame
 * @param confidence Style detection confidence (0.0-1.0)
 * @return BehaviorContext with recommendations
 *
 * Style-to-Behavior Mapping:
 * - RHYTHMIC_DRIVEN -> PULSE_ON_BEAT (strong beats drive visuals)
 * - HARMONIC_DRIVEN -> DRIFT_WITH_HARMONY (chord changes drive color)
 * - MELODIC_DRIVEN  -> SHIMMER_WITH_MELODY (treble drives sparkle)
 * - DYNAMIC_DRIVEN  -> BREATHE_WITH_DYNAMICS (RMS drives intensity)
 * - TEXTURE_DRIVEN  -> TEXTURE_FLOW (flux drives morphing)
 * - UNKNOWN         -> Fallback to saliency-based selection
 */
inline BehaviorContext selectBehavior(
    audio::MusicStyle style,
    const audio::MusicalSaliencyFrame& saliency,
    float confidence = 0.5f
) {
    BehaviorContext ctx;
    ctx.currentStyle = style;
    ctx.styleConfidence = confidence;
    ctx.saliencyFrame = &saliency;

    // Threshold for confident style-based selection
    constexpr float CONFIDENCE_THRESHOLD = 0.25f;

    // If style is known and confidence is sufficient, use style-based mapping
    if (style != audio::MusicStyle::UNKNOWN && confidence >= CONFIDENCE_THRESHOLD) {
        switch (style) {
            case audio::MusicStyle::RHYTHMIC_DRIVEN:
                ctx.recommendedPrimary = VisualBehavior::PULSE_ON_BEAT;
                ctx.recommendedSecondary = VisualBehavior::BREATHE_WITH_DYNAMICS;
                break;

            case audio::MusicStyle::HARMONIC_DRIVEN:
                ctx.recommendedPrimary = VisualBehavior::DRIFT_WITH_HARMONY;
                ctx.recommendedSecondary = VisualBehavior::SHIMMER_WITH_MELODY;
                break;

            case audio::MusicStyle::MELODIC_DRIVEN:
                ctx.recommendedPrimary = VisualBehavior::SHIMMER_WITH_MELODY;
                ctx.recommendedSecondary = VisualBehavior::DRIFT_WITH_HARMONY;
                break;

            case audio::MusicStyle::DYNAMIC_DRIVEN:
                ctx.recommendedPrimary = VisualBehavior::BREATHE_WITH_DYNAMICS;
                ctx.recommendedSecondary = VisualBehavior::PULSE_ON_BEAT;
                break;

            case audio::MusicStyle::TEXTURE_DRIVEN:
                ctx.recommendedPrimary = VisualBehavior::TEXTURE_FLOW;
                ctx.recommendedSecondary = VisualBehavior::DRIFT_WITH_HARMONY;
                break;

            default:
                // Fall through to saliency-based selection
                break;
        }

        // If we got a valid style mapping, return
        if (style != audio::MusicStyle::UNKNOWN) {
            return ctx;
        }
    }

    // Fallback: saliency-based selection when style is UNKNOWN or low confidence
    // Use the dominant saliency type to infer appropriate behavior
    audio::SaliencyType dominant = saliency.getDominantType();

    switch (dominant) {
        case audio::SaliencyType::RHYTHMIC:
            ctx.recommendedPrimary = VisualBehavior::PULSE_ON_BEAT;
            ctx.recommendedSecondary = VisualBehavior::BREATHE_WITH_DYNAMICS;
            break;

        case audio::SaliencyType::HARMONIC:
            ctx.recommendedPrimary = VisualBehavior::DRIFT_WITH_HARMONY;
            ctx.recommendedSecondary = VisualBehavior::SHIMMER_WITH_MELODY;
            break;

        case audio::SaliencyType::TIMBRAL:
            ctx.recommendedPrimary = VisualBehavior::TEXTURE_FLOW;
            ctx.recommendedSecondary = VisualBehavior::SHIMMER_WITH_MELODY;
            break;

        case audio::SaliencyType::DYNAMIC:
        default:
            ctx.recommendedPrimary = VisualBehavior::BREATHE_WITH_DYNAMICS;
            ctx.recommendedSecondary = VisualBehavior::PULSE_ON_BEAT;
            break;
    }

    return ctx;
}

// ============================================================================
// Palette Strategies
// ============================================================================

/**
 * @brief Palette change strategy based on music style
 *
 * Different music styles warrant different approaches to color/palette changes.
 * Rhythmic music benefits from snappy palette changes on beats, while harmonic
 * music should have smooth transitions following chord progressions.
 */
enum class PaletteStrategy : uint8_t {
    /**
     * @brief Snap palette changes on strong beats
     * For: RHYTHMIC_DRIVEN music (EDM, hip-hop)
     */
    RHYTHMIC_SNAP = 0,

    /**
     * @brief Commit on chord changes with smooth transitions
     * For: HARMONIC_DRIVEN music (jazz, classical)
     */
    HARMONIC_COMMIT = 1,

    /**
     * @brief Continuous drift following melody contour
     * For: MELODIC_DRIVEN music (vocal pop)
     */
    MELODIC_DRIFT = 2,

    /**
     * @brief Slow organic evolution with spectral flux
     * For: TEXTURE_DRIVEN music (ambient, drone)
     */
    TEXTURE_EVOLVE = 3,

    /**
     * @brief Intensity-driven palette warmth/coolness
     * For: DYNAMIC_DRIVEN music (orchestral, cinematic)
     */
    DYNAMIC_WARMTH = 4
};

/**
 * @brief Select palette strategy based on music style
 * @param style Detected music style
 * @return Appropriate palette strategy
 */
inline PaletteStrategy selectPaletteStrategy(audio::MusicStyle style) {
    switch (style) {
        case audio::MusicStyle::RHYTHMIC_DRIVEN:
            return PaletteStrategy::RHYTHMIC_SNAP;
        case audio::MusicStyle::HARMONIC_DRIVEN:
            return PaletteStrategy::HARMONIC_COMMIT;
        case audio::MusicStyle::MELODIC_DRIVEN:
            return PaletteStrategy::MELODIC_DRIFT;
        case audio::MusicStyle::TEXTURE_DRIVEN:
            return PaletteStrategy::TEXTURE_EVOLVE;
        case audio::MusicStyle::DYNAMIC_DRIVEN:
            return PaletteStrategy::DYNAMIC_WARMTH;
        case audio::MusicStyle::UNKNOWN:
        default:
            return PaletteStrategy::HARMONIC_COMMIT;
    }
}

// ============================================================================
// Style-Adaptive Timing
// ============================================================================

/**
 * @brief Timing parameters adapted by music style
 *
 * State machine timing and transition speeds should vary based on the
 * detected music style. Rhythmic music needs snappier response, while
 * harmonic/textural music benefits from slower, more organic transitions.
 */
struct StyleTiming {
    // State machine timing (seconds)
    float phraseGateDuration;     ///< How long before committing palette changes
    float buildThreshold;         ///< Energy threshold to enter BUILD phase
    float holdDuration;           ///< Minimum time in HOLD phase
    float releaseSpeed;           ///< How fast RELEASE phase progresses (multiplier)
    float quietThreshold;         ///< Time of quiet before returning to REST

    // Transition smoothing
    float colorTransitionSpeed;   ///< Palette color blend rate (0-1 per second)
    float motionTransitionSpeed;  ///< Motion parameter blend rate (0-1 per second)
    float attackMultiplier;       ///< Multiplier for attack sharpness
    float decayMultiplier;        ///< Multiplier for decay length

    /**
     * @brief Get timing parameters for a music style
     * @param style Detected music style
     * @return StyleTiming struct with appropriate values
     */
    static StyleTiming forStyle(audio::MusicStyle style) {
        StyleTiming t;

        switch (style) {
            case audio::MusicStyle::RHYTHMIC_DRIVEN:
                // EDM/hip-hop: Snappy, punchy, quick response
                t.phraseGateDuration = 1.5f;
                t.buildThreshold = 0.25f;
                t.holdDuration = 1.0f;
                t.releaseSpeed = 1.5f;
                t.quietThreshold = 0.4f;
                t.colorTransitionSpeed = 0.8f;
                t.motionTransitionSpeed = 0.9f;
                t.attackMultiplier = 1.5f;
                t.decayMultiplier = 0.7f;
                break;

            case audio::MusicStyle::HARMONIC_DRIVEN:
                // Jazz/classical: Slow, smooth, patient
                t.phraseGateDuration = 4.0f;
                t.buildThreshold = 0.18f;
                t.holdDuration = 3.0f;
                t.releaseSpeed = 0.6f;
                t.quietThreshold = 0.8f;
                t.colorTransitionSpeed = 0.3f;
                t.motionTransitionSpeed = 0.4f;
                t.attackMultiplier = 0.7f;
                t.decayMultiplier = 1.5f;
                break;

            case audio::MusicStyle::MELODIC_DRIVEN:
                // Vocal pop: Medium responsiveness
                t.phraseGateDuration = 2.5f;
                t.buildThreshold = 0.20f;
                t.holdDuration = 2.0f;
                t.releaseSpeed = 0.9f;
                t.quietThreshold = 0.6f;
                t.colorTransitionSpeed = 0.5f;
                t.motionTransitionSpeed = 0.6f;
                t.attackMultiplier = 1.0f;
                t.decayMultiplier = 1.0f;
                break;

            case audio::MusicStyle::TEXTURE_DRIVEN:
                // Ambient/drone: Very slow, organic
                t.phraseGateDuration = 6.0f;
                t.buildThreshold = 0.12f;
                t.holdDuration = 5.0f;
                t.releaseSpeed = 0.4f;
                t.quietThreshold = 1.2f;
                t.colorTransitionSpeed = 0.15f;
                t.motionTransitionSpeed = 0.2f;
                t.attackMultiplier = 0.5f;
                t.decayMultiplier = 2.0f;
                break;

            case audio::MusicStyle::DYNAMIC_DRIVEN:
                // Orchestral/cinematic: Follow dynamics
                t.phraseGateDuration = 3.0f;
                t.buildThreshold = 0.15f;
                t.holdDuration = 4.0f;
                t.releaseSpeed = 0.5f;
                t.quietThreshold = 0.7f;
                t.colorTransitionSpeed = 0.4f;
                t.motionTransitionSpeed = 0.5f;
                t.attackMultiplier = 0.8f;
                t.decayMultiplier = 1.3f;
                break;

            case audio::MusicStyle::UNKNOWN:
            default:
                // Balanced defaults for any style
                t.phraseGateDuration = 2.0f;
                t.buildThreshold = 0.20f;
                t.holdDuration = 2.5f;
                t.releaseSpeed = 0.8f;
                t.quietThreshold = 0.6f;
                t.colorTransitionSpeed = 0.5f;
                t.motionTransitionSpeed = 0.5f;
                t.attackMultiplier = 1.0f;
                t.decayMultiplier = 1.0f;
                break;
        }

        return t;
    }
};

// ============================================================================
// Saliency-Based Emphasis
// ============================================================================

/**
 * @brief Visual emphasis weights based on saliency dominance
 *
 * Maps saliency types to visual dimensions:
 * - Harmonic -> Color emphasis
 * - Rhythmic -> Motion emphasis
 * - Timbral -> Texture emphasis
 * - Dynamic -> Intensity emphasis
 */
struct SaliencyEmphasis {
    float colorEmphasis;     ///< Weight for color/palette changes (0-1)
    float motionEmphasis;    ///< Weight for motion/pulse effects (0-1)
    float textureEmphasis;   ///< Weight for texture/shimmer (0-1)
    float intensityEmphasis; ///< Weight for brightness/intensity (0-1)

    /**
     * @brief Compute emphasis weights from saliency frame
     * @param saliency Current saliency metrics
     * @return SaliencyEmphasis struct with normalized weights
     */
    static SaliencyEmphasis fromSaliency(const audio::MusicalSaliencyFrame& saliency) {
        SaliencyEmphasis e;

        // Get raw saliency values
        float harmonic = saliency.harmonicNoveltySmooth;
        float rhythmic = saliency.rhythmicNoveltySmooth;
        float timbral = saliency.timbralNoveltySmooth;
        float dynamic = saliency.dynamicNoveltySmooth;

        // Normalize to sum to ~1.0
        float total = harmonic + rhythmic + timbral + dynamic + 0.001f;

        // Map saliency types to visual dimensions
        e.colorEmphasis = harmonic / total;
        e.motionEmphasis = rhythmic / total;
        e.textureEmphasis = timbral / total;
        e.intensityEmphasis = dynamic / total;

        // Boost the dominant type for clearer visual response
        audio::SaliencyType dominant = saliency.getDominantType();
        constexpr float BOOST = 1.5f;

        switch (dominant) {
            case audio::SaliencyType::HARMONIC:
                e.colorEmphasis *= BOOST;
                break;
            case audio::SaliencyType::RHYTHMIC:
                e.motionEmphasis *= BOOST;
                break;
            case audio::SaliencyType::TIMBRAL:
                e.textureEmphasis *= BOOST;
                break;
            case audio::SaliencyType::DYNAMIC:
                e.intensityEmphasis *= BOOST;
                break;
        }

        // Clamp all values
        if (e.colorEmphasis > 1.0f) e.colorEmphasis = 1.0f;
        if (e.motionEmphasis > 1.0f) e.motionEmphasis = 1.0f;
        if (e.textureEmphasis > 1.0f) e.textureEmphasis = 1.0f;
        if (e.intensityEmphasis > 1.0f) e.intensityEmphasis = 1.0f;

        return e;
    }

    /**
     * @brief Get default emphasis (neutral weights)
     * @return SaliencyEmphasis with equal weights
     */
    static SaliencyEmphasis neutral() {
        return {0.25f, 0.25f, 0.25f, 0.25f};
    }
};

#endif // FEATURE_AUDIO_SYNC

} // namespace plugins
} // namespace lightwaveos
