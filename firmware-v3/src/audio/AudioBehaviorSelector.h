/**
 * @file AudioBehaviorSelector.h
 * @brief Narrative-driven behavior selection mixin for effects
 *
 * Part of the Musical Intelligence System (MIS) - Phase 3
 * Builds on BehaviorSelection.h to add:
 * - NarrativePhase detection (BUILD/HOLD/RELEASE/REST)
 * - Smooth transitions between behaviors with progress tracking
 * - Per-effect registration of supported behaviors
 * - Fallback handling when preferred behavior unavailable
 *
 * Usage in effects:
 * @code
 * class MyEffect : public IEffect {
 *     AudioBehaviorSelector m_selector;
 *
 *     void init() override {
 *         m_selector.registerBehavior(VisualBehavior::BREATHING);
 *         m_selector.registerBehavior(VisualBehavior::PULSING);
 *         m_selector.setFallbackBehavior(VisualBehavior::BREATHING);
 *         m_selector.setTransitionTime(500);  // 500ms blend
 *     }
 *
 *     void render(EffectContext& ctx) override {
 *         m_selector.update(ctx);
 *
 *         switch (m_selector.currentBehavior()) {
 *             case VisualBehavior::BREATHING: renderBreathing(ctx); break;
 *             case VisualBehavior::PULSING:   renderPulsing(ctx);   break;
 *             // ...
 *         }
 *
 *         // Optional: blend during transitions
 *         if (m_selector.isTransitioning()) {
 *             float t = m_selector.transitionProgress();
 *             // Blend between previous and current render...
 *         }
 *     }
 * };
 * @endcode
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include "../plugins/api/BehaviorSelection.h"
#include "../config/features.h"

namespace lightwaveos {

// Forward declaration
namespace plugins { struct EffectContext; }

namespace audio {

/**
 * @brief Narrative phases for storytelling-driven visual behavior
 *
 * Based on the LGP Storytelling Framework's BUILD/HOLD/RELEASE/REST model.
 * Audio analysis drives transitions between phases based on energy, flux,
 * and beat proximity.
 */
enum class NarrativePhase : uint8_t {
    /**
     * @brief Low energy, between phrases - contemplative, minimal
     * Entry: Low RMS (<0.15) and low flux (<0.1) sustained
     * Visual: Sparse, breathing, slow movement
     */
    REST = 0,

    /**
     * @brief Rising energy/flux, approaching downbeat - tension building
     * Entry: Rising flux (>0.3) near downbeat (phase >0.75 or <0.1)
     * Visual: Edge approach, increasing complexity, anticipation
     */
    BUILD = 1,

    /**
     * @brief Peak energy, strong beats - maximum presence
     * Entry: High RMS (>0.65) with strong beats (strength >0.5)
     * Visual: Dense patterns, pulsing, maximum intensity
     */
    HOLD = 2,

    /**
     * @brief Falling energy, post-peak - resolving, returning to rest
     * Entry: Energy dropping from HOLD (current < 0.8 * peak)
     * Visual: Centre pulse, dissolution, fading complexity
     */
    RELEASE = 3
};

/**
 * @brief Get human-readable name for narrative phase
 */
inline const char* getNarrativePhaseName(NarrativePhase phase) {
    switch (phase) {
        case NarrativePhase::REST:    return "REST";
        case NarrativePhase::BUILD:   return "BUILD";
        case NarrativePhase::HOLD:    return "HOLD";
        case NarrativePhase::RELEASE: return "RELEASE";
        default:                      return "UNKNOWN";
    }
}

/**
 * @brief Behavior registration entry for per-effect behavior support
 */
struct BehaviorEntry {
    plugins::VisualBehavior behavior;
    float priority;     ///< Selection priority (higher = preferred)
    bool enabled;       ///< Whether this behavior is available

    BehaviorEntry()
        : behavior(plugins::VisualBehavior::BREATHE_WITH_DYNAMICS)
        , priority(1.0f)
        , enabled(false)
    {}
};

/**
 * @brief Audio-driven behavior selector with narrative phase tracking
 *
 * This class acts as a mixin that effects can include to get intelligent
 * behavior selection based on musical content. It:
 * 1. Analyzes audio to determine narrative phase (BUILD/HOLD/RELEASE/REST)
 * 2. Selects appropriate behavior from the effect's registered behaviors
 * 3. Manages smooth transitions between behaviors
 * 4. Provides fallback when audio is unavailable
 */
class AudioBehaviorSelector {
public:
    static constexpr size_t MAX_BEHAVIORS = 8;

    AudioBehaviorSelector();

    //--------------------------------------------------------------------------
    // Configuration (call in effect's init())
    //--------------------------------------------------------------------------

    /**
     * @brief Register a behavior this effect supports
     * @param behavior The visual behavior to register
     * @param priority Selection priority (higher = preferred when multiple match)
     */
    void registerBehavior(plugins::VisualBehavior behavior, float priority = 1.0f);

    /**
     * @brief Unregister a behavior
     * @param behavior The visual behavior to remove
     */
    void unregisterBehavior(plugins::VisualBehavior behavior);

    /**
     * @brief Set the fallback behavior when audio unavailable or no match
     * @param behavior Must be a registered behavior
     */
    void setFallbackBehavior(plugins::VisualBehavior behavior);

    /**
     * @brief Set transition duration between behaviors
     * @param ms Transition time in milliseconds (default 500)
     */
    void setTransitionTime(uint16_t ms);

    /**
     * @brief Set energy thresholds for phase detection
     * @param rest Energy below this enters REST (default 0.15)
     * @param build Energy above this with rising flux enters BUILD
     * @param hold Energy above this with strong beats enters HOLD (default 0.65)
     */
    void setEnergyThresholds(float rest, float build, float hold);

    /**
     * @brief Reset all state (call when effect changes or reinitializes)
     */
    void reset();

    //--------------------------------------------------------------------------
    // Per-frame update (call at start of effect's render())
    //--------------------------------------------------------------------------

    /**
     * @brief Update state based on current audio context
     * @param ctx Effect context with audio data
     *
     * This should be called once per frame at the start of render().
     * It updates:
     * - Narrative phase based on energy/flux/beat analysis
     * - Current behavior based on phase and music style
     * - Transition progress
     */
    void update(const plugins::EffectContext& ctx);

    //--------------------------------------------------------------------------
    // Query current state (use in effect's render())
    //--------------------------------------------------------------------------

    /**
     * @brief Get current narrative phase
     */
    NarrativePhase narrativePhase() const { return m_phase; }

    /**
     * @brief Get current active behavior
     * @return The behavior to use for rendering (accounts for transitions)
     */
    plugins::VisualBehavior currentBehavior() const { return m_currentBehavior; }

    /**
     * @brief Get target behavior (what we're transitioning to)
     */
    plugins::VisualBehavior targetBehavior() const { return m_targetBehavior; }

    /**
     * @brief Get previous behavior (what we're transitioning from)
     */
    plugins::VisualBehavior previousBehavior() const { return m_previousBehavior; }

    /**
     * @brief Check if currently transitioning between behaviors
     */
    bool isTransitioning() const { return m_transitionProgress < 1.0f; }

    /**
     * @brief Get transition progress (0.0 to 1.0)
     * @return 0.0 at start of transition, 1.0 when complete
     *
     * Use this for blending between behavior renders:
     * @code
     * CRGB blended = blend(previousRender, currentRender, transitionProgress() * 255);
     * @endcode
     */
    float transitionProgress() const { return m_transitionProgress; }

    /**
     * @brief Get phase intensity (0.0 to 1.0 within current phase)
     *
     * For BUILD: 0→1 as tension increases
     * For HOLD: 0→1 as sustain progresses
     * For RELEASE: 0→1 as resolution progresses
     * For REST: constant low value
     */
    float phaseIntensity() const { return m_phaseIntensity; }

    /**
     * @brief Get smoothed energy value (with fast attack, slow decay)
     */
    float energy() const { return m_energySmoothed; }

    /**
     * @brief Get smoothed flux value
     */
    float flux() const { return m_fluxSmoothed; }

    //--------------------------------------------------------------------------
    // Convenience audio accessors (from last update)
    //--------------------------------------------------------------------------

    /**
     * @brief Check if on a beat (from last update)
     */
    bool isOnBeat() const { return m_wasOnBeat; }

    /**
     * @brief Check if on a downbeat
     */
    bool isOnDownbeat() const { return m_wasOnDownbeat; }

    /**
     * @brief Get beat phase (0.0-1.0)
     */
    float beatPhase() const { return m_beatPhase; }

    /**
     * @brief Check if audio was available in last update
     */
    bool audioAvailable() const { return m_audioAvailable; }

private:
    //--------------------------------------------------------------------------
    // Registered behaviors
    //--------------------------------------------------------------------------
    std::array<BehaviorEntry, MAX_BEHAVIORS> m_behaviors;
    plugins::VisualBehavior m_fallbackBehavior;

    //--------------------------------------------------------------------------
    // State machine
    //--------------------------------------------------------------------------
    NarrativePhase m_phase;
    NarrativePhase m_previousPhase;
    plugins::VisualBehavior m_currentBehavior;
    plugins::VisualBehavior m_targetBehavior;
    plugins::VisualBehavior m_previousBehavior;

    //--------------------------------------------------------------------------
    // Transition management
    //--------------------------------------------------------------------------
    float m_transitionProgress;
    uint16_t m_transitionTimeMs;
    uint32_t m_transitionStartMs;

    //--------------------------------------------------------------------------
    // Phase intensity tracking
    //--------------------------------------------------------------------------
    float m_phaseIntensity;
    float m_phaseStartEnergy;
    uint32_t m_phaseStartMs;

    //--------------------------------------------------------------------------
    // Smoothed audio signals
    //--------------------------------------------------------------------------
    float m_energySmoothed;
    float m_fluxSmoothed;
    float m_previousEnergy;
    float m_peakEnergy;

    //--------------------------------------------------------------------------
    // Audio state cache
    //--------------------------------------------------------------------------
    bool m_wasOnBeat;
    bool m_wasOnDownbeat;
    float m_beatPhase;
    bool m_audioAvailable;
    uint32_t m_lastBeatTick;

    //--------------------------------------------------------------------------
    // Thresholds
    //--------------------------------------------------------------------------
    float m_restThreshold;
    float m_buildThreshold;
    float m_holdThreshold;

    //--------------------------------------------------------------------------
    // Timing
    //--------------------------------------------------------------------------
    uint32_t m_lastUpdateMs;

    //--------------------------------------------------------------------------
    // Private methods
    //--------------------------------------------------------------------------

    /**
     * @brief Check if a behavior is registered
     */
    bool hasBehavior(plugins::VisualBehavior behavior) const;

    /**
     * @brief Get registration entry for a behavior
     */
    const BehaviorEntry* getBehaviorEntry(plugins::VisualBehavior behavior) const;

    /**
     * @brief Analyze audio to determine narrative phase
     */
    NarrativePhase analyzeNarrativePhase(const plugins::EffectContext& ctx);

    /**
     * @brief Select best behavior for current phase and music style
     */
    plugins::VisualBehavior selectBehaviorForPhase(
        NarrativePhase phase,
        const plugins::EffectContext& ctx
    );

    /**
     * @brief Find the best registered behavior matching a recommendation
     */
    plugins::VisualBehavior findBestMatch(
        plugins::VisualBehavior recommended,
        plugins::VisualBehavior secondary
    );

    /**
     * @brief Start transition to a new behavior
     */
    void beginTransition(plugins::VisualBehavior newBehavior, uint32_t nowMs);

    /**
     * @brief Update phase intensity based on current phase and timing
     */
    void updatePhaseIntensity(uint32_t nowMs);
};

} // namespace audio
} // namespace lightwaveos
