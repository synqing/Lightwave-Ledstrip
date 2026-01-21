/**
 * @file MirrorModifier.h
 * @brief Symmetry modifier (CENTER ORIGIN compliance)
 *
 * MirrorModifier creates perfect symmetry around the center point (LED 79/80).
 * It replaces one half of the strip with a mirrored copy of the other half.
 *
 * Modes:
 * - LEFT_TO_RIGHT: Mirror left half to right
 * - RIGHT_TO_LEFT: Mirror right half to left
 * - CENTER_OUT: Mirror from center outward (both sides identical)
 * - KALEIDOSCOPE: Alternating symmetry patterns
 *
 * This modifier is CENTER ORIGIN compliant by design - it uses the center
 * point as the axis of symmetry.
 *
 * Usage:
 *   MirrorModifier mirror(MirrorMode::LEFT_TO_RIGHT);
 *   modifierStack.add(&mirror, ctx);
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "IEffectModifier.h"

namespace lightwaveos {
namespace effects {
namespace modifiers {

/**
 * @brief Mirror mode
 */
enum class MirrorMode : uint8_t {
    LEFT_TO_RIGHT = 0,  // Mirror left half (0-79) to right half (80-159)
    RIGHT_TO_LEFT,      // Mirror right half (80-159) to left half (0-79)
    CENTER_OUT,         // Mirror both halves identically from center
    KALEIDOSCOPE        // Alternating patterns
};

/**
 * @brief Symmetry modifier
 */
class MirrorModifier : public IEffectModifier {
public:
    /**
     * @brief Constructor
     * @param mode Mirror mode
     */
    explicit MirrorModifier(MirrorMode mode = MirrorMode::LEFT_TO_RIGHT);

    //--------------------------------------------------------------------------
    // IEffectModifier Implementation
    //--------------------------------------------------------------------------

    bool init(const plugins::EffectContext& ctx) override;
    void apply(plugins::EffectContext& ctx) override;
    void unapply() override;
    const ModifierMetadata& getMetadata() const override;

    //--------------------------------------------------------------------------
    // Parameters
    //--------------------------------------------------------------------------

    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

    /**
     * @brief Set mirror mode
     */
    void setMode(MirrorMode mode);

    /**
     * @brief Get current mode
     */
    MirrorMode getMode() const { return m_mode; }

    //--------------------------------------------------------------------------
    // Enable/Disable
    //--------------------------------------------------------------------------

    bool isEnabled() const override { return m_enabled; }
    void setEnabled(bool enabled) override { m_enabled = enabled; }

private:
    MirrorMode m_mode;
    bool m_enabled;

    // Apply mirroring to a single strip (160 LEDs)
    void mirrorStrip(CRGB* strip, uint16_t count, uint16_t center);
};

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
