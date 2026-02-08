/**
 * @file limits.h
 * @brief Single source of truth for system-wide array limits
 *
 * This file defines all array size limits that are used across multiple
 * components. Using a centralised definition prevents drift between
 * components (e.g., effect count in WebServer vs RendererActor).
 *
 * When adding new effects, palettes, or zones - update the constants HERE
 * and all components will automatically use the new values.
 *
 * CRITICAL: This is the ONLY place where MAX_EFFECTS should be defined.
 * All other files MUST #include this header and reference limits::MAX_EFFECTS.
 * Static assertions throughout the codebase enforce compile-time parity.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <stdint.h>

namespace lightwaveos {
namespace limits {

// ============================================================================
// Effect System Limits
// ============================================================================

/**
 * @brief Maximum number of registered effects - SINGLE SOURCE OF TRUTH
 *
 * This value must be >= the number of effects registered in registerAllEffects().
 * Current count: 122 (as of Feb 2026)
 *
 * Referenced by:
 * - RendererActor.h: m_effects[MAX_EFFECTS] array
 * - WebServer.h: CachedRendererState::effectNames[MAX_EFFECTS]
 * - AudioEffectMapping.h: AudioMappingRegistry::MAX_EFFECTS
 * - PatternRegistry.cpp: EXPECTED_EFFECT_COUNT validation
 * - BuiltinEffectRegistry.h: Static assertion for headroom
 *
 * When adding new effects:
 * 1. Update this constant
 * 2. Add effect to registerAllEffects() in CoreEffects.cpp
 * 3. Add metadata entry to PATTERN_METADATA[] in PatternRegistry.cpp
 * 4. Build will fail with static_assert if counts don't match
 *
 * DO NOT duplicate this constant elsewhere. Use limits::MAX_EFFECTS.
 */
constexpr uint8_t MAX_EFFECTS = 122;

// ============================================================================
// Palette System Limits
// ============================================================================

/**
 * @brief Maximum number of palettes
 *
 * Current count: 75 (as of Feb 2026)
 */
constexpr uint8_t MAX_PALETTES = 75;

// ============================================================================
// Zone System Limits
// ============================================================================

/**
 * @brief Maximum number of LED zones
 */
constexpr uint8_t MAX_ZONES = 8;

} // namespace limits
} // namespace lightwaveos
