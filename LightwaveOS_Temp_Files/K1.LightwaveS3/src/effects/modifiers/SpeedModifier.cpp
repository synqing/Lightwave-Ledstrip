/**
 * @file SpeedModifier.cpp
 * @brief Speed modifier implementation
 */

#include "SpeedModifier.h"
#include <Arduino.h>
#include <algorithm>

namespace lightwaveos {
namespace effects {
namespace modifiers {

SpeedModifier::SpeedModifier(float multiplier)
    : m_multiplier(std::clamp(multiplier, MIN_MULTIPLIER, MAX_MULTIPLIER))
    , m_originalSpeed(0)
    , m_enabled(true) {
}

bool SpeedModifier::init(const plugins::EffectContext& ctx) {
    m_originalSpeed = ctx.speed;
    Serial.printf("[SpeedModifier] Initialized (multiplier: %.2f, original speed: %d)\n",
                  m_multiplier, m_originalSpeed);
    return true;
}

void SpeedModifier::apply(plugins::EffectContext& ctx) {
    if (!m_enabled) return;

    // PRE-RENDER: Modify speed parameter before effect renders
    // This is called from RendererNode::renderFrame() BEFORE effect->render()
    uint8_t newSpeed = static_cast<uint8_t>(ctx.speed * m_multiplier);

    // Clamp to valid range [1, 100]
    if (newSpeed == 0) newSpeed = 1;
    if (newSpeed > 100) newSpeed = 100;

    ctx.speed = newSpeed;
}

void SpeedModifier::unapply() {
    Serial.println("[SpeedModifier] Unapplied");
    // No cleanup needed
}

const ModifierMetadata& SpeedModifier::getMetadata() const {
    static ModifierMetadata metadata{
        "Speed",
        "Temporal scaling (0.1x - 3.0x)",
        ModifierType::SPEED,
        1
    };
    return metadata;
}

bool SpeedModifier::setParameter(const char* name, float value) {
    if (strcmp(name, "multiplier") == 0) {
        setMultiplier(value);
        return true;
    } else if (strcmp(name, "enabled") == 0) {
        setEnabled(value > 0.5f);
        return true;
    }
    return false;
}

float SpeedModifier::getParameter(const char* name) const {
    if (strcmp(name, "multiplier") == 0) {
        return m_multiplier;
    } else if (strcmp(name, "enabled") == 0) {
        return m_enabled ? 1.0f : 0.0f;
    }
    return 0.0f;
}

void SpeedModifier::setMultiplier(float multiplier) {
    m_multiplier = std::clamp(multiplier, MIN_MULTIPLIER, MAX_MULTIPLIER);
    Serial.printf("[SpeedModifier] Set multiplier: %.2f\n", m_multiplier);
}

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
