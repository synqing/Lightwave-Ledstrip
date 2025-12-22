/**
 * @file CoreEffects.h
 * @brief Core v2 effects with CENTER PAIR compliance
 *
 * All effects use the canonical CENTER ORIGIN pattern:
 * - TRUE center is BETWEEN LEDs 79 and 80
 * - Effects treat 79/80 as the center PAIR
 * - Symmetric expansion: leftLed = 79 - dist, rightLed = 80 + dist
 *
 * Effects use RenderContext for Actor-based rendering.
 */

#pragma once

#include "../core/actors/RendererActor.h"

namespace lightwaveos {
namespace effects {

// ==================== CENTER PAIR Constants ====================

constexpr uint16_t CENTER_LEFT = 79;   // Last LED of left half
constexpr uint16_t CENTER_RIGHT = 80;  // First LED of right half
constexpr uint16_t HALF_LENGTH = 80;   // LEDs per half
constexpr uint16_t STRIP_LENGTH = 160; // LEDs per strip

constexpr uint16_t centerPairDistance(uint16_t index) {
    return (index <= CENTER_LEFT) ? (CENTER_LEFT - index) : (index - CENTER_RIGHT);
}

constexpr float centerPairSignedPosition(uint16_t index) {
    return (index <= CENTER_LEFT) ? -((float)(CENTER_LEFT - index) + 0.5f)
                                  : ((float)(index - CENTER_RIGHT) + 0.5f);
}

// ==================== Helper Macros ====================

// Set LED with bounds checking for strip 1 (0-159)
#define SET_STRIP1(ctx, idx, color) \
    if ((idx) < STRIP_LENGTH) { (ctx).leds[(idx)] = (color); }

// Set LED with bounds checking for strip 2 (160-319)
#define SET_STRIP2(ctx, idx, color) \
    if ((idx) >= STRIP_LENGTH && (idx) < 320) { (ctx).leds[(idx)] = (color); }

// Set symmetric LEDs from center (both strips)
#define SET_CENTER_PAIR(ctx, dist, color) do { \
    uint16_t left1 = CENTER_LEFT - (dist); \
    uint16_t right1 = CENTER_RIGHT + (dist); \
    uint16_t left2 = 160 + CENTER_LEFT - (dist); \
    uint16_t right2 = 160 + CENTER_RIGHT + (dist); \
    SET_STRIP1(ctx, left1, color); \
    SET_STRIP1(ctx, right1, color); \
    SET_STRIP2(ctx, left2, color); \
    SET_STRIP2(ctx, right2, color); \
} while(0)

// ==================== Effect Function Declarations ====================

using namespace lightwaveos::actors;

// Basic Effects
void effectFire(RenderContext& ctx);
void effectOcean(RenderContext& ctx);
void effectPlasma(RenderContext& ctx);
void effectConfetti(RenderContext& ctx);
void effectSinelon(RenderContext& ctx);
void effectJuggle(RenderContext& ctx);
void effectBPM(RenderContext& ctx);
void effectWave(RenderContext& ctx);
void effectRipple(RenderContext& ctx);
void effectHeartbeat(RenderContext& ctx);

// Advanced Effects
void effectInterference(RenderContext& ctx);
void effectBreathing(RenderContext& ctx);
void effectPulse(RenderContext& ctx);

// ==================== Effect Registration Helper ====================

/**
 * @brief Register all core effects with RendererActor
 * @param renderer Pointer to RendererActor
 * @return Number of effects registered
 */
uint8_t registerCoreEffects(RendererActor* renderer);

/**
 * @brief Register ALL effects (core + LGP) with RendererActor
 * @param renderer Pointer to RendererActor
 * @return Total number of effects registered
 */
uint8_t registerAllEffects(RendererActor* renderer);

} // namespace effects
} // namespace lightwaveos
