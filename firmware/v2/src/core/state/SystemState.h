#pragma once

#include <cstdint>
#include <array>

namespace lightwaveos {
namespace state {

// Maximum configuration constants
constexpr uint8_t MAX_ZONES = 4;
constexpr uint8_t MAX_PALETTE_COUNT = 64;
// Maximum effect ID allowed by CQRS/state commands.
// Keep in sync with RendererActor::MAX_EFFECTS (upper bound for effect IDs).
constexpr uint8_t MAX_EFFECT_COUNT = 104;

/**
 * Zone configuration state
 * Represents the state of a single zone in multi-zone mode
 */
struct ZoneState {
    uint8_t effectId;       // Current effect ID for this zone
    uint8_t paletteId;      // Current palette ID for this zone
    uint8_t brightness;     // Zone-specific brightness (0-255)
    uint8_t speed;          // Zone-specific animation speed (1-100)
    bool enabled;           // Whether this zone is active

    // Default constructor with safe initial values
    ZoneState()
        : effectId(0)
        , paletteId(0)
        , brightness(255)
        , speed(15)
        , enabled(false)
    {}

    // Copy constructor
    ZoneState(const ZoneState& other) = default;

    // Assignment operator
    ZoneState& operator=(const ZoneState& other) = default;
};

/**
 * Complete system state (immutable snapshot)
 *
 * This is the single source of truth for all LightwaveOS state.
 * State is immutable - modifications create new copies via with*() methods.
 *
 * Size: ~100 bytes (cache-friendly)
 * Thread-safety: Immutable, safe for concurrent reads
 */
struct SystemState {
    // Version for optimistic concurrency control
    // Incremented on every state change
    uint32_t version;

    // ==================== Global Settings ====================

    uint8_t currentEffectId;     // Active effect (0-(MAX_EFFECT_COUNT-1))
    uint8_t currentPaletteId;    // Active palette (0-63)
    uint8_t brightness;          // Global brightness (0-255)
    uint8_t speed;               // Global animation speed (1-100)
    uint8_t gHue;                // Auto-incrementing hue (0-255)

    // ==================== Visual Parameters ====================

    uint8_t intensity;           // Effect intensity (0-255)
    uint8_t saturation;          // Color saturation (0-255)
    uint8_t complexity;          // Pattern complexity (0-255)
    uint8_t variation;           // Pattern variation (0-255)

    // ==================== Zone Mode ====================

    bool zoneModeEnabled;        // Whether zone mode is active
    uint8_t activeZoneCount;     // Number of active zones (1-4)
    std::array<ZoneState, MAX_ZONES> zones;  // Zone configurations

    // ==================== Transition State ====================

    bool transitionActive;       // Whether a transition is in progress
    uint8_t transitionType;      // Type of transition (0-11)
    uint8_t transitionProgress;  // Transition progress (0-255)

    // ==================== Constructors ====================

    /**
     * Default constructor with safe initial values
     * Matches v1 defaults for backward compatibility
     */
    SystemState();

    /**
     * Copy constructor
     */
    SystemState(const SystemState& other) = default;

    /**
     * Assignment operator
     */
    SystemState& operator=(const SystemState& other) = default;

    // ==================== Functional Update Methods ====================
    // All methods return new state with incremented version

    /**
     * Create modified copy with new effect ID
     * @param effectId New effect ID (0-(MAX_EFFECT_COUNT-1))
     * @return New state with updated effect
     */
    SystemState withEffect(uint8_t effectId) const;

    /**
     * Create modified copy with new brightness
     * @param value New brightness (0-255)
     * @return New state with updated brightness
     */
    SystemState withBrightness(uint8_t value) const;

    /**
     * Create modified copy with new palette
     * @param paletteId New palette ID (0-63)
     * @return New state with updated palette
     */
    SystemState withPalette(uint8_t paletteId) const;

    /**
     * Create modified copy with new speed
     * @param value New speed (1-100)
     * @return New state with updated speed
     */
    SystemState withSpeed(uint8_t value) const;

    /**
     * Create modified copy with zone enabled/disabled
     * @param zoneId Zone index (0-3)
     * @param enabled Whether zone is enabled
     * @return New state with updated zone
     */
    SystemState withZoneEnabled(uint8_t zoneId, bool enabled) const;

    /**
     * Create modified copy with zone effect changed
     * @param zoneId Zone index (0-3)
     * @param effectId New effect ID for zone
     * @return New state with updated zone effect
     */
    SystemState withZoneEffect(uint8_t zoneId, uint8_t effectId) const;

    /**
     * Create modified copy with zone palette changed
     * @param zoneId Zone index (0-3)
     * @param paletteId New palette ID for zone
     * @return New state with updated zone palette
     */
    SystemState withZonePalette(uint8_t zoneId, uint8_t paletteId) const;

    /**
     * Create modified copy with zone brightness changed
     * @param zoneId Zone index (0-3)
     * @param brightness New brightness for zone (0-255)
     * @return New state with updated zone brightness
     */
    SystemState withZoneBrightness(uint8_t zoneId, uint8_t brightness) const;

    /**
     * Create modified copy with zone speed changed
     * @param zoneId Zone index (0-3)
     * @param speed New speed for zone (1-100)
     * @return New state with updated zone speed
     */
    SystemState withZoneSpeed(uint8_t zoneId, uint8_t speed) const;

    /**
     * Create modified copy with zone mode toggled
     * @param enabled Whether zone mode is enabled
     * @param zoneCount Number of active zones (1-4)
     * @return New state with updated zone mode
     */
    SystemState withZoneMode(bool enabled, uint8_t zoneCount) const;

    /**
     * Create modified copy with transition state updated
     * @param type Transition type (0-11)
     * @param progress Transition progress (0-255)
     * @return New state with updated transition
     */
    SystemState withTransition(uint8_t type, uint8_t progress) const;

    /**
     * Create modified copy with transition started
     * @param type Transition type (0-11)
     * @return New state with transition active
     */
    SystemState withTransitionStarted(uint8_t type) const;

    /**
     * Create modified copy with transition completed
     * @return New state with transition inactive
     */
    SystemState withTransitionCompleted() const;

    /**
     * Create modified copy with incremented hue
     * Used for auto-cycling effects
     * @return New state with hue incremented (wraps at 255)
     */
    SystemState withIncrementedHue() const;

    /**
     * Create modified copy with visual parameters updated
     * @param intensity New intensity (0-255)
     * @param saturation New saturation (0-255)
     * @param complexity New complexity (0-255)
     * @param variation New variation (0-255)
     * @return New state with updated parameters
     */
    SystemState withVisualParams(uint8_t intensity, uint8_t saturation,
                                  uint8_t complexity, uint8_t variation) const;

    /**
     * Create modified copy with intensity changed
     * @param value New intensity (0-255)
     * @return New state with updated intensity
     */
    SystemState withIntensity(uint8_t value) const;

    /**
     * Create modified copy with saturation changed
     * @param value New saturation (0-255)
     * @return New state with updated saturation
     */
    SystemState withSaturation(uint8_t value) const;

    /**
     * Create modified copy with complexity changed
     * @param value New complexity (0-255)
     * @return New state with updated complexity
     */
    SystemState withComplexity(uint8_t value) const;

    /**
     * Create modified copy with variation changed
     * @param value New variation (0-255)
     * @return New state with updated variation
     */
    SystemState withVariation(uint8_t value) const;
};

} // namespace state
} // namespace lightwaveos
