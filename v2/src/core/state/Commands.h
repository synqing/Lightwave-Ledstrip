#pragma once

#include "ICommand.h"

namespace lightwaveos {
namespace state {

// ==================== Effect Commands ====================

/**
 * Set current effect
 */
class SetEffectCommand : public ICommand {
public:
    explicit SetEffectCommand(uint8_t effectId)
        : m_effectId(effectId) {}

    SystemState apply(const SystemState& current) const override {
        return current.withEffect(m_effectId);
    }

    const char* getName() const override {
        return "SetEffect";
    }

    bool validate(const SystemState& current) const override {
        (void)current;
        return m_effectId < MAX_EFFECT_COUNT;
    }

private:
    uint8_t m_effectId;
};

// ==================== Brightness Commands ====================

/**
 * Set global brightness
 */
class SetBrightnessCommand : public ICommand {
public:
    explicit SetBrightnessCommand(uint8_t brightness)
        : m_brightness(brightness) {}

    SystemState apply(const SystemState& current) const override {
        return current.withBrightness(m_brightness);
    }

    const char* getName() const override {
        return "SetBrightness";
    }

private:
    uint8_t m_brightness;
};

// ==================== Palette Commands ====================

/**
 * Set current palette
 */
class SetPaletteCommand : public ICommand {
public:
    explicit SetPaletteCommand(uint8_t paletteId)
        : m_paletteId(paletteId) {}

    SystemState apply(const SystemState& current) const override {
        return current.withPalette(m_paletteId);
    }

    const char* getName() const override {
        return "SetPalette";
    }

    bool validate(const SystemState& current) const override {
        (void)current;
        return m_paletteId < MAX_PALETTE_COUNT;
    }

private:
    uint8_t m_paletteId;
};

// ==================== Speed Commands ====================

/**
 * Set animation speed
 */
class SetSpeedCommand : public ICommand {
public:
    explicit SetSpeedCommand(uint8_t speed)
        : m_speed(speed) {}

    SystemState apply(const SystemState& current) const override {
        return current.withSpeed(m_speed);
    }

    const char* getName() const override {
        return "SetSpeed";
    }

    bool validate(const SystemState& current) const override {
        (void)current;
        return m_speed >= 1 && m_speed <= 50;
    }

private:
    uint8_t m_speed;
};

// ==================== Zone Commands ====================

/**
 * Enable or disable a specific zone
 */
class ZoneEnableCommand : public ICommand {
public:
    ZoneEnableCommand(uint8_t zoneId, bool enabled)
        : m_zoneId(zoneId), m_enabled(enabled) {}

    SystemState apply(const SystemState& current) const override {
        return current.withZoneEnabled(m_zoneId, m_enabled);
    }

    const char* getName() const override {
        return "ZoneEnable";
    }

    bool validate(const SystemState& current) const override {
        (void)current;
        return m_zoneId < MAX_ZONES;
    }

private:
    uint8_t m_zoneId;
    bool m_enabled;
};

/**
 * Set effect for a specific zone
 */
class ZoneSetEffectCommand : public ICommand {
public:
    ZoneSetEffectCommand(uint8_t zoneId, uint8_t effectId)
        : m_zoneId(zoneId), m_effectId(effectId) {}

    SystemState apply(const SystemState& current) const override {
        return current.withZoneEffect(m_zoneId, m_effectId);
    }

    const char* getName() const override {
        return "ZoneSetEffect";
    }

    bool validate(const SystemState& current) const override {
        (void)current;
        return m_zoneId < MAX_ZONES && m_effectId < MAX_EFFECT_COUNT;
    }

private:
    uint8_t m_zoneId;
    uint8_t m_effectId;
};

/**
 * Set palette for a specific zone
 */
class ZoneSetPaletteCommand : public ICommand {
public:
    ZoneSetPaletteCommand(uint8_t zoneId, uint8_t paletteId)
        : m_zoneId(zoneId), m_paletteId(paletteId) {}

    SystemState apply(const SystemState& current) const override {
        return current.withZonePalette(m_zoneId, m_paletteId);
    }

    const char* getName() const override {
        return "ZoneSetPalette";
    }

    bool validate(const SystemState& current) const override {
        (void)current;
        return m_zoneId < MAX_ZONES && m_paletteId < MAX_PALETTE_COUNT;
    }

private:
    uint8_t m_zoneId;
    uint8_t m_paletteId;
};

/**
 * Set brightness for a specific zone
 */
class ZoneSetBrightnessCommand : public ICommand {
public:
    ZoneSetBrightnessCommand(uint8_t zoneId, uint8_t brightness)
        : m_zoneId(zoneId), m_brightness(brightness) {}

    SystemState apply(const SystemState& current) const override {
        return current.withZoneBrightness(m_zoneId, m_brightness);
    }

    const char* getName() const override {
        return "ZoneSetBrightness";
    }

    bool validate(const SystemState& current) const override {
        (void)current;
        return m_zoneId < MAX_ZONES;
    }

private:
    uint8_t m_zoneId;
    uint8_t m_brightness;
};

/**
 * Set speed for a specific zone
 */
class ZoneSetSpeedCommand : public ICommand {
public:
    ZoneSetSpeedCommand(uint8_t zoneId, uint8_t speed)
        : m_zoneId(zoneId), m_speed(speed) {}

    SystemState apply(const SystemState& current) const override {
        return current.withZoneSpeed(m_zoneId, m_speed);
    }

    const char* getName() const override {
        return "ZoneSetSpeed";
    }

    bool validate(const SystemState& current) const override {
        (void)current;
        return m_zoneId < MAX_ZONES && m_speed >= 1 && m_speed <= 50;
    }

private:
    uint8_t m_zoneId;
    uint8_t m_speed;
};

/**
 * Enable/disable zone mode and set zone count
 */
class SetZoneModeCommand : public ICommand {
public:
    SetZoneModeCommand(bool enabled, uint8_t zoneCount)
        : m_enabled(enabled), m_zoneCount(zoneCount) {}

    SystemState apply(const SystemState& current) const override {
        return current.withZoneMode(m_enabled, m_zoneCount);
    }

    const char* getName() const override {
        return "SetZoneMode";
    }

    bool validate(const SystemState& current) const override {
        (void)current;
        return m_zoneCount >= 1 && m_zoneCount <= MAX_ZONES;
    }

private:
    bool m_enabled;
    uint8_t m_zoneCount;
};

// ==================== Transition Commands ====================

/**
 * Trigger a transition
 */
class TriggerTransitionCommand : public ICommand {
public:
    explicit TriggerTransitionCommand(uint8_t transitionType)
        : m_transitionType(transitionType) {}

    SystemState apply(const SystemState& current) const override {
        return current.withTransitionStarted(m_transitionType);
    }

    const char* getName() const override {
        return "TriggerTransition";
    }

    bool validate(const SystemState& current) const override {
        (void)current;
        return m_transitionType < 12;  // 12 transition types in v1
    }

private:
    uint8_t m_transitionType;
};

/**
 * Update transition progress
 */
class UpdateTransitionCommand : public ICommand {
public:
    UpdateTransitionCommand(uint8_t transitionType, uint8_t progress)
        : m_transitionType(transitionType), m_progress(progress) {}

    SystemState apply(const SystemState& current) const override {
        return current.withTransition(m_transitionType, m_progress);
    }

    const char* getName() const override {
        return "UpdateTransition";
    }

    bool validate(const SystemState& current) const override {
        (void)current;
        return m_transitionType < 12;
    }

private:
    uint8_t m_transitionType;
    uint8_t m_progress;
};

/**
 * Complete transition
 */
class CompleteTransitionCommand : public ICommand {
public:
    CompleteTransitionCommand() {}

    SystemState apply(const SystemState& current) const override {
        return current.withTransitionCompleted();
    }

    const char* getName() const override {
        return "CompleteTransition";
    }

private:
};

// ==================== Hue Commands ====================

/**
 * Increment global hue (for auto-cycling effects)
 */
class IncrementHueCommand : public ICommand {
public:
    IncrementHueCommand() {}

    SystemState apply(const SystemState& current) const override {
        return current.withIncrementedHue();
    }

    const char* getName() const override {
        return "IncrementHue";
    }

private:
};

// ==================== Visual Parameter Commands ====================

/**
 * Set all visual parameters at once
 */
class SetVisualParamsCommand : public ICommand {
public:
    SetVisualParamsCommand(uint8_t intensity, uint8_t saturation,
                           uint8_t complexity, uint8_t variation)
        : m_intensity(intensity)
        , m_saturation(saturation)
        , m_complexity(complexity)
        , m_variation(variation) {}

    SystemState apply(const SystemState& current) const override {
        return current.withVisualParams(m_intensity, m_saturation,
                                        m_complexity, m_variation);
    }

    const char* getName() const override {
        return "SetVisualParams";
    }

private:
    uint8_t m_intensity;
    uint8_t m_saturation;
    uint8_t m_complexity;
    uint8_t m_variation;
};

/**
 * Set intensity parameter
 */
class SetIntensityCommand : public ICommand {
public:
    explicit SetIntensityCommand(uint8_t intensity)
        : m_intensity(intensity) {}

    SystemState apply(const SystemState& current) const override {
        return current.withIntensity(m_intensity);
    }

    const char* getName() const override {
        return "SetIntensity";
    }

private:
    uint8_t m_intensity;
};

/**
 * Set saturation parameter
 */
class SetSaturationCommand : public ICommand {
public:
    explicit SetSaturationCommand(uint8_t saturation)
        : m_saturation(saturation) {}

    SystemState apply(const SystemState& current) const override {
        return current.withSaturation(m_saturation);
    }

    const char* getName() const override {
        return "SetSaturation";
    }

private:
    uint8_t m_saturation;
};

/**
 * Set complexity parameter
 */
class SetComplexityCommand : public ICommand {
public:
    explicit SetComplexityCommand(uint8_t complexity)
        : m_complexity(complexity) {}

    SystemState apply(const SystemState& current) const override {
        return current.withComplexity(m_complexity);
    }

    const char* getName() const override {
        return "SetComplexity";
    }

private:
    uint8_t m_complexity;
};

/**
 * Set variation parameter
 */
class SetVariationCommand : public ICommand {
public:
    explicit SetVariationCommand(uint8_t variation)
        : m_variation(variation) {}

    SystemState apply(const SystemState& current) const override {
        return current.withVariation(m_variation);
    }

    const char* getName() const override {
        return "SetVariation";
    }

private:
    uint8_t m_variation;
};

} // namespace state
} // namespace lightwaveos
