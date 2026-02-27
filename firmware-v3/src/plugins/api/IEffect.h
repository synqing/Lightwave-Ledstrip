/**
 * @file IEffect.h
 * @brief Core plugin interface for LightwaveOS v2 effects
 *
 * All effects (built-in, legacy wrapped, and third-party plugins) implement
 * this interface. The system calls render() at 120 FPS with an EffectContext
 * containing all dependencies - NO global variables.
 *
 * Example implementation:
 * @code
 * class FireEffect : public IEffect {
 * public:
 *     bool init(EffectContext& ctx) override {
 *         // One-time setup
 *         return true;
 *     }
 *
 *     void render(EffectContext& ctx) override {
 *         // Called 120x/second
 *         for (uint16_t i = 0; i < ctx.ledCount; i++) {
 *             float dist = ctx.getDistanceFromCenter(i);
 *             ctx.leds[i] = ctx.palette->getColor(dist * 255);
 *         }
 *     }
 *
 *     void cleanup() override { }
 *
 *     const EffectMetadata& getMetadata() const override {
 *         static EffectMetadata meta{"Fire", "Flames from center",
 *                                    EffectCategory::FIRE, 1};
 *         return meta;
 *     }
 * };
 * @endcode
 */

#pragma once

// =====================================================================
// PSRAM ALLOCATION POLICY (MANDATORY)
// =====================================================================
// All IEffect implementations with buffers >64 bytes MUST allocate them
// from PSRAM using heap_caps_malloc(size, MALLOC_CAP_SPIRAM).
//
// DO NOT declare large arrays as class members -- they end up in internal
// DRAM (.bss section) and starve WiFi/lwIP/FreeRTOS of heap space.
//
// Pattern: Use a PsramData* pointer, allocate in init(), free in cleanup().
// See: docs/MEMORY_ALLOCATION.md section 3.5 "Effect Buffer PSRAM Policy"
// =====================================================================

#include <cstdint>
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace plugins {

// Forward declarations
class EffectContext;
struct EffectMetadata;

/**
 * @brief Effect category for UI organization and filtering
 */
enum class EffectCategory : uint8_t {
    UNCATEGORIZED = 0,
    FIRE,           // Fire, heat, warmth effects
    WATER,          // Ocean, waves, rain
    NATURE,         // Aurora, forest, organic
    GEOMETRIC,      // Patterns, shapes, mathematical
    QUANTUM,        // LGP interference, wave physics
    SHOCKWAVE,      // Pulse, burst, explosion
    AMBIENT,        // Subtle, background, mood
    PARTY,          // Fast, dynamic, music-reactive
    CUSTOM,         // User-created via designer
    LEGACY_LINEAR   // LINEAR patterns EXEMPT from CENTER_ORIGIN (v1 parity)
};

/**
 * @brief Effect metadata for registration and UI display
 */
struct EffectMetadata {
    const char* name;           // Display name (max 32 chars)
    const char* description;    // Brief description (max 128 chars)
    EffectCategory category;    // Category for filtering
    uint8_t version;            // Effect version (for updates)
    const char* author;         // Creator name (optional)
    EffectId id = INVALID_EFFECT_ID;  // Stable namespaced ID (set during registration)

    // Default constructor
    EffectMetadata(const char* n = "Unnamed",
                   const char* d = "",
                   EffectCategory c = EffectCategory::UNCATEGORIZED,
                   uint8_t v = 1,
                   const char* a = nullptr)
        : name(n), description(d), category(c), version(v), author(a) {}
};

/**
 * @brief Effect parameter descriptor for dynamic UI generation
 */
struct EffectParameter {
    const char* name;           // Parameter name (used as key)
    const char* displayName;    // UI label
    float minValue;             // Minimum allowed value
    float maxValue;             // Maximum allowed value
    float defaultValue;         // Initial value

    EffectParameter(const char* n = "", const char* d = "",
                    float min = 0.0f, float max = 1.0f, float def = 0.5f)
        : name(n), displayName(d), minValue(min), maxValue(max), defaultValue(def) {}
};

/**
 * @brief Core effect interface
 *
 * All effects must implement this interface. The system provides an
 * EffectContext with all dependencies - effects should NOT access
 * global variables or hardware directly.
 *
 * Thread safety: render() is always called from Core 1's render task.
 * init() and cleanup() are called from Core 0 during effect transitions.
 */
class IEffect {
public:
    virtual ~IEffect() = default;

    //--------------------------------------------------------------------------
    // Lifecycle Methods
    //--------------------------------------------------------------------------

    /**
     * @brief Initialize the effect
     * @param ctx Effect context with LED buffer and parameters
     * @return true if initialization succeeded
     *
     * Called once when the effect is selected. Use this for one-time setup,
     * allocating any effect-specific state. Keep allocations minimal.
     */
    virtual bool init(EffectContext& ctx) = 0;

    /**
     * @brief Render one frame of the effect
     * @param ctx Effect context with LED buffer and current parameters
     *
     * Called at 120 FPS (every ~8.3ms). This is the hot path - optimize for
     * speed. Avoid allocations, complex calculations, or I/O.
     *
     * IMPORTANT: All effects MUST use CENTER ORIGIN pattern:
     * - Use ctx.getDistanceFromCenter(i) for position calculations
     * - Effects should radiate from center (LED 79/80) outward
     */
    virtual void render(EffectContext& ctx) = 0;

    /**
     * @brief Clean up effect resources
     *
     * Called when switching away from this effect. Free any allocated
     * resources. The effect may be re-initialized later.
     */
    virtual void cleanup() = 0;

    //--------------------------------------------------------------------------
    // Metadata Methods
    //--------------------------------------------------------------------------

    /**
     * @brief Get effect metadata for registration
     * @return Reference to static metadata struct
     */
    virtual const EffectMetadata& getMetadata() const = 0;

    //--------------------------------------------------------------------------
    // Optional Parameter Methods (override for custom parameters)
    //--------------------------------------------------------------------------

    /**
     * @brief Get number of custom parameters
     * @return Number of parameters (0 for effects using only global params)
     */
    virtual uint8_t getParameterCount() const { return 0; }

    /**
     * @brief Get parameter descriptor by index
     * @param index Parameter index (0 to getParameterCount()-1)
     * @return Parameter descriptor or nullptr if invalid index
     */
    virtual const EffectParameter* getParameter(uint8_t index) const {
        (void)index;
        return nullptr;
    }

    /**
     * @brief Set a parameter value
     * @param name Parameter name
     * @param value New value (will be clamped to min/max)
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
};

} // namespace plugins
} // namespace lightwaveos
