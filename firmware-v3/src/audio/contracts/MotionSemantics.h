/**
 * @file MotionSemantics.h
 * @brief Layer 2 motion-semantic inference chain
 *
 * Transforms ControlBusFrame DSP signals into a 6-axis motion-semantic frame
 * based on the Canonical Motion-Semantic Ontology (Laban Effort 4D + 2 modifiers).
 *
 * Pipeline: ControlBusFrame -> inference -> author overrides -> smoothing -> MotionSemanticFrame
 *
 * Total cost: ~228 bytes RAM, ~0.21ms per frame. No dynamic allocation.
 *
 * @see ControlBus.h — source audio data contract
 * @see SmoothingEngine.h — AsymmetricFollower used for per-dimension smoothing
 */

#pragma once

#include <stdint.h>
#include <math.h>
#include "ControlBus.h"
#include "../../effects/enhancement/SmoothingEngine.h"

namespace lightwaveos::audio {

// ============================================================================
// Feature detection: timing_jitter in ControlBusFrame
// ============================================================================
// When timing_jitter is added to ControlBusFrame, define this macro in the
// build or in ControlBus.h to enable direct wiring.
#ifndef CONTROLBUS_HAS_TIMING_JITTER
#define MOTIONSEMANTICS_FLUIDITY_DEFAULT 0.5f  // TODO: wire timing_jitter from ControlBusFrame
#endif

// ============================================================================
// Confidence constants (0-255 scale)
// ============================================================================

static constexpr uint8_t MSEM_CONF_HIGH   = 255;  ///< Weight, Time, Impulse
static constexpr uint8_t MSEM_CONF_MEDIUM = 128;  ///< Fluidity, Space
static constexpr uint8_t MSEM_CONF_LOW    =  64;  ///< Flow (no validated audio correlate)

// Inferred mask bit positions
static constexpr uint8_t MSEM_BIT_WEIGHT   = 0;
static constexpr uint8_t MSEM_BIT_TIME     = 1;
static constexpr uint8_t MSEM_BIT_SPACE    = 2;
static constexpr uint8_t MSEM_BIT_FLOW     = 3;
static constexpr uint8_t MSEM_BIT_FLUIDITY = 4;
static constexpr uint8_t MSEM_BIT_IMPULSE  = 5;

// ============================================================================
// MotionSemanticFrame
// ============================================================================

/**
 * @brief 6-axis motion-semantic frame produced every render cycle.
 *
 * Laban Effort 4D (weight, time_quality, space, flow) plus two execution
 * modifiers (fluidity, impulse_strength). All axes are continuous [0.0, 1.0].
 */
struct MotionSemanticFrame {
    // Laban Effort 4D — primary dimensions
    float weight         = 0.5f;  ///< [0.0 = Light, 1.0 = Strong]
    float time_quality   = 0.5f;  ///< [0.0 = Sustained, 1.0 = Sudden]
    float space          = 0.5f;  ///< [0.0 = Direct, 1.0 = Indirect]
    float flow           = 0.5f;  ///< [0.0 = Bound, 1.0 = Free]

    // Execution modifiers
    float fluidity       = 0.5f;  ///< [0.0 = Jerky, 1.0 = Fluid]
    float impulse_strength = 0.0f; ///< [0.0 = Weak, 1.0 = Explosive]

    // Metadata
    uint8_t inferred_mask  = 0x3F; ///< Bitmask: bit set = dimension was inferred (not overridden)
                                   ///< bit0=weight, bit1=time, bit2=space, bit3=flow, bit4=fluidity, bit5=impulse
    uint8_t confidence_min = 0;    ///< Lowest confidence among inferred dimensions (0-255)
};

// ============================================================================
// MotionAuthorOverrides
// ============================================================================

/**
 * @brief Per-dimension author overrides. NAN = use inferred value.
 *
 * Creators can lock any dimension to a fixed value, bypassing inference.
 * When a dimension is overridden, its inferred_mask bit is cleared and
 * it does not contribute to confidence_min.
 */
struct MotionAuthorOverrides {
    float weight           = NAN;  ///< NAN or [0.0, 1.0]
    float time_quality     = NAN;  ///< NAN or [0.0, 1.0]
    float space            = NAN;  ///< NAN or [0.0, 1.0]
    float flow             = NAN;  ///< NAN or [0.0, 1.0]
    float fluidity         = NAN;  ///< NAN or [0.0, 1.0]
    float impulse_strength = NAN;  ///< NAN or [0.0, 1.0]

    /**
     * @brief Check whether a specific dimension has an author override.
     * @param bit Bit position (MSEM_BIT_WEIGHT through MSEM_BIT_IMPULSE)
     * @return true if the corresponding field is not NAN
     */
    bool hasOverride(uint8_t bit) const {
        switch (bit) {
            case MSEM_BIT_WEIGHT:   return !isnan(weight);
            case MSEM_BIT_TIME:     return !isnan(time_quality);
            case MSEM_BIT_SPACE:    return !isnan(space);
            case MSEM_BIT_FLOW:     return !isnan(flow);
            case MSEM_BIT_FLUIDITY: return !isnan(fluidity);
            case MSEM_BIT_IMPULSE:  return !isnan(impulse_strength);
            default:                return false;
        }
    }

    /**
     * @brief Clear all overrides (set all fields to NAN).
     */
    void clear() {
        weight           = NAN;
        time_quality     = NAN;
        space            = NAN;
        flow             = NAN;
        fluidity         = NAN;
        impulse_strength = NAN;
    }
};

// ============================================================================
// MotionSemanticEngine
// ============================================================================

/**
 * @brief Deterministic inference engine: ControlBusFrame -> MotionSemanticFrame.
 *
 * Each call to update() performs:
 *  1. Raw inference from ControlBusFrame DSP signals
 *  2. Author override application (replaces inferred values where specified)
 *  3. Per-dimension asymmetric smoothing (fast attack, slow release)
 *  4. Confidence and inferred_mask metadata
 *
 * Uses 6x AsymmetricFollower from SmoothingEngine.h for frame-rate independent
 * temporal smoothing. Total cost: ~0.21ms per frame, 0 heap allocations.
 */
class MotionSemanticEngine {
public:
    MotionSemanticEngine()
        // Tau values: (riseTau, fallTau) in seconds
        // Impulse: fastest — must track transients
        : m_smooth_weight         (0.0f, 0.03f, 0.20f)
        , m_smooth_time_quality   (0.0f, 0.10f, 0.30f)
        , m_smooth_space          (0.0f, 0.10f, 0.25f)
        , m_smooth_flow           (0.0f, 0.10f, 0.25f)
        , m_smooth_fluidity       (0.0f, 0.10f, 0.25f)
        , m_smooth_impulse        (0.0f, 0.01f, 0.15f)
    {}

    /**
     * @brief Run the full inference chain for one frame.
     *
     * @param bus       Current ControlBusFrame from the audio pipeline
     * @param dt        Delta time in seconds (hop cadence)
     * @param overrides Optional author overrides (nullptr = all inferred)
     */
    void update(const ControlBusFrame& bus, float dt,
                const MotionAuthorOverrides* overrides = nullptr)
    {
        // ---- 1. Raw inference (pre-smoothing) ----

        // WEIGHT: 40% RMS + 35% onset proxy (fast_flux) + 25% bass energy
        float raw_weight = clampf(
            0.40f * bus.rms +
            0.35f * bus.fast_flux +
            0.25f * bus.bands[0],
            0.0f, 1.0f
        );

        // TIME: normalized tempo (40-200 BPM -> 0.0-1.0)
        float raw_time = clampf(
            (bus.tempoBpm - 40.0f) / 160.0f,
            0.0f, 1.0f
        );

        // SPACE: inverse tonal confidence (high confidence = direct/focused)
        float raw_space = 1.0f - bus.chordState.confidence;

        // FLOW: no validated audio correlate — default neutral until validated
        float raw_flow = 0.5f;

        // FLUIDITY: inverse timing jitter (regular timing = fluid)
#ifdef CONTROLBUS_HAS_TIMING_JITTER
        float raw_fluidity = 1.0f - bus.timing_jitter;
#else
        float raw_fluidity = MOTIONSEMANTICS_FLUIDITY_DEFAULT;  // TODO: wire timing_jitter
#endif

        // IMPULSE: scaled spectral flux peaks
        float raw_impulse = clampf(bus.fast_flux * 2.5f, 0.0f, 1.0f);

        // ---- 2. Apply author overrides ----

        uint8_t mask = 0x3F;  // all 6 bits set = all inferred
        uint8_t conf_min = 255;

        applyOverride(overrides, raw_weight,   MSEM_BIT_WEIGHT,   MSEM_CONF_HIGH,   mask, conf_min);
        applyOverride(overrides, raw_time,     MSEM_BIT_TIME,     MSEM_CONF_HIGH,   mask, conf_min);
        applyOverride(overrides, raw_space,    MSEM_BIT_SPACE,    MSEM_CONF_MEDIUM, mask, conf_min);
        applyOverride(overrides, raw_flow,     MSEM_BIT_FLOW,     MSEM_CONF_LOW,    mask, conf_min);
        applyOverride(overrides, raw_fluidity, MSEM_BIT_FLUIDITY, MSEM_CONF_MEDIUM, mask, conf_min);
        applyOverride(overrides, raw_impulse,  MSEM_BIT_IMPULSE,  MSEM_CONF_HIGH,   mask, conf_min);

        // ---- 3. Temporal smoothing (AsymmetricFollower per dimension) ----

        m_frame.weight           = m_smooth_weight.update(raw_weight, dt);
        m_frame.time_quality     = m_smooth_time_quality.update(raw_time, dt);
        m_frame.space            = m_smooth_space.update(raw_space, dt);
        m_frame.flow             = m_smooth_flow.update(raw_flow, dt);
        m_frame.fluidity         = m_smooth_fluidity.update(raw_fluidity, dt);
        m_frame.impulse_strength = m_smooth_impulse.update(raw_impulse, dt);

        // ---- 4. Metadata ----

        m_frame.inferred_mask  = mask;
        m_frame.confidence_min = (mask == 0) ? 255 : conf_min;
    }

    /**
     * @brief Access the current motion-semantic frame (read-only).
     * @return Reference to the last computed frame
     */
    const MotionSemanticFrame& frame() const { return m_frame; }

private:
    MotionSemanticFrame m_frame{};

    // Per-dimension asymmetric smoothers (rise/fall tau in seconds)
    lightwaveos::effects::enhancement::AsymmetricFollower m_smooth_weight;
    lightwaveos::effects::enhancement::AsymmetricFollower m_smooth_time_quality;
    lightwaveos::effects::enhancement::AsymmetricFollower m_smooth_space;
    lightwaveos::effects::enhancement::AsymmetricFollower m_smooth_flow;
    lightwaveos::effects::enhancement::AsymmetricFollower m_smooth_fluidity;
    lightwaveos::effects::enhancement::AsymmetricFollower m_smooth_impulse;

    /**
     * @brief Clamp a float to [lo, hi] without std::clamp.
     */
    static float clampf(float v, float lo, float hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    /**
     * @brief Apply a single author override if present, updating mask and confidence.
     *
     * If the override for this dimension is not NAN, the raw value is replaced
     * and the corresponding inferred_mask bit is cleared. Otherwise, the
     * dimension's confidence contributes to conf_min.
     */
    static void applyOverride(const MotionAuthorOverrides* overrides,
                               float& raw, uint8_t bit, uint8_t conf,
                               uint8_t& mask, uint8_t& conf_min)
    {
        if (overrides && !isnan(getOverrideValue(overrides, bit))) {
            raw = clampf(getOverrideValue(overrides, bit), 0.0f, 1.0f);
            mask &= ~(1 << bit);
        } else {
            if (conf < conf_min) conf_min = conf;
        }
    }

    /**
     * @brief Retrieve the override float for a given bit position.
     */
    static float getOverrideValue(const MotionAuthorOverrides* o, uint8_t bit) {
        switch (bit) {
            case MSEM_BIT_WEIGHT:   return o->weight;
            case MSEM_BIT_TIME:     return o->time_quality;
            case MSEM_BIT_SPACE:    return o->space;
            case MSEM_BIT_FLOW:     return o->flow;
            case MSEM_BIT_FLUIDITY: return o->fluidity;
            case MSEM_BIT_IMPULSE:  return o->impulse_strength;
            default:                return NAN;
        }
    }
};

} // namespace lightwaveos::audio
