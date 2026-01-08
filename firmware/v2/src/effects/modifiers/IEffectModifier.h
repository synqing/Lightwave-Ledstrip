/**
 * @file IEffectModifier.h
 * @brief Core interface for effect modifiers (Phase A Quick Win - A2)
 *
 * Effect modifiers are post-processing layers that transform LED output
 * AFTER the base effect renders. They stack in order: each modifier
 * receives the output of the previous modifier.
 *
 * Architecture:
 * - Modifiers DO NOT modify EffectContext parameters
 * - Modifiers transform the LED buffer AFTER effect render
 * - Modifiers can maintain internal state between frames
 * - Modifiers must be lightweight (<2KB memory per instance)
 *
 * Example:
 *   1. FireEffect renders to leds[]
 *   2. SpeedModifier (2.0x) time-warps animation
 *   3. IntensityModifier scales brightness by beat
 *   4. MirrorModifier creates symmetry
 *   5. Final output sent to FastLED
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <cstdint>
#include "../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#else
#include "../../../test/unit/mocks/fastled_mock.h"
#endif

namespace lightwaveos {
namespace effects {
namespace modifiers {

/**
 * @brief Modifier type enumeration for identification
 */
enum class ModifierType : uint8_t {
    SPEED = 0,          // Temporal scaling (0.1x - 3.0x)
    INTENSITY,          // Brightness envelope (audio-reactive)
    COLOR_SHIFT,        // Palette rotation (hue offset)
    MIRROR,             // Symmetry break/restore
    GLITCH,             // Controlled chaos (beat-synced)
    CUSTOM              // User-defined modifiers
};

/**
 * @brief Modifier metadata for UI display and API
 */
struct ModifierMetadata {
    const char* name;           // Display name (max 32 chars)
    const char* description;    // Brief description (max 128 chars)
    ModifierType type;          // Modifier type
    uint8_t version;            // Modifier version

    ModifierMetadata(const char* n = "Unnamed",
                     const char* d = "",
                     ModifierType t = ModifierType::CUSTOM,
                     uint8_t v = 1)
        : name(n), description(d), type(t), version(v) {}
};

/**
 * @brief Core effect modifier interface
 *
 * Modifiers transform LED buffers AFTER effects render. They receive
 * the full EffectContext (read-only for parameters) and can modify
 * the LED buffer in place.
 *
 * Thread safety: apply() is always called from Core 1's render task,
 * same as IEffect::render(). No additional synchronization needed.
 */
class IEffectModifier {
public:
    virtual ~IEffectModifier() = default;

    //--------------------------------------------------------------------------
    // Lifecycle Methods
    //--------------------------------------------------------------------------

    /**
     * @brief Initialize the modifier
     * @param ctx Effect context (for parameter access)
     * @return true if initialization succeeded
     *
     * Called once when the modifier is added to the stack.
     * Allocate any internal state here.
     */
    virtual bool init(const plugins::EffectContext& ctx) = 0;

    /**
     * @brief Apply the modifier transformation
     * @param ctx Effect context (LED buffer writable, parameters read-only)
     *
     * Called at 120 FPS after effect render. Transform ctx.leds[] in place.
     * This is the hot path - optimize for speed.
     *
     * CRITICAL: Modifiers MUST preserve CENTER ORIGIN aesthetic where applicable.
     * Use ctx.getDistanceFromCenter(i) for position-based transformations.
     */
    virtual void apply(plugins::EffectContext& ctx) = 0;

    /**
     * @brief Unapply/cleanup the modifier
     *
     * Called when the modifier is removed from the stack.
     * Free any allocated resources.
     */
    virtual void unapply() = 0;

    //--------------------------------------------------------------------------
    // Metadata Methods
    //--------------------------------------------------------------------------

    /**
     * @brief Get modifier metadata
     * @return Reference to static metadata struct
     */
    virtual const ModifierMetadata& getMetadata() const = 0;

    /**
     * @brief Get modifier name (convenience accessor)
     */
    const char* getName() const { return getMetadata().name; }

    /**
     * @brief Get modifier type
     */
    ModifierType getType() const { return getMetadata().type; }

    //--------------------------------------------------------------------------
    // Parameter Methods (optional - for configurable modifiers)
    //--------------------------------------------------------------------------

    /**
     * @brief Set a parameter value
     * @param name Parameter name
     * @param value New value
     * @return true if parameter was found and set
     */
    virtual bool setParameter(const char* name, float value) {
        (void)name;
        (void)value;
        return false;
    }

    /**
     * @brief Get a parameter value
     * @param name Parameter name
     * @return Current value or 0.0f if not found
     */
    virtual float getParameter(const char* name) const {
        (void)name;
        return 0.0f;
    }

    //--------------------------------------------------------------------------
    // State Query
    //--------------------------------------------------------------------------

    /**
     * @brief Check if modifier is enabled
     * @return true if modifier is active
     */
    virtual bool isEnabled() const { return true; }

    /**
     * @brief Enable/disable modifier
     * @param enabled true to enable, false to disable
     */
    virtual void setEnabled(bool enabled) { (void)enabled; }

    /**
     * @brief Check if this is a pre-render modifier
     * @return true if modifier applies BEFORE effect render, false if AFTER
     *
     * Pre-render modifiers (e.g., SpeedModifier) modify context parameters
     * before the effect renders. Post-render modifiers (e.g., IntensityModifier,
     * ColorShiftModifier) transform the LED buffer after the effect renders.
     */
    virtual bool isPreRender() const { return false; }
};

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
