/**
 * C++ code generation templates for all 10 node types.
 *
 * Each template function returns one or more lines of C++ code
 * to be inserted into the render() method body, or member variable
 * declarations for the class body.
 *
 * Generated code targets the LightwaveOS IEffect interface:
 *   - Centre-origin rendering via SET_CENTER_PAIR
 *   - No heap allocation in render()
 *   - British English in comments
 *   - bands[] for audio, not bin()
 */

// ---------------------------------------------------------------------------
// Source nodes
// ---------------------------------------------------------------------------

export function templateRms(varName: string): string {
  return `    const float ${varName} = ctx.controlBus.rms;`;
}

export function templateBand(varName: string, index: number): string {
  return `    const float ${varName} = ctx.controlBus.bands[${index}];`;
}

export function templateBeatStrength(varName: string): string {
  return `    const float ${varName} = ctx.controlBus.beatStrength;`;
}

// ---------------------------------------------------------------------------
// Processing nodes
// ---------------------------------------------------------------------------

export function templateEmaSmooth(
  varName: string,
  inputVar: string,
  memberVar: string,
  tau: number,
): string {
  return [
    `    const float ${varName}_alpha = 1.0f - expf(-rawDt / ${Math.max(tau, 0.001).toFixed(4)}f);`,
    `    ${memberVar} += (${inputVar} - ${memberVar}) * ${varName}_alpha;`,
    `    const float ${varName} = ${memberVar};`,
  ].join('\n');
}

export function templateMaxFollower(
  normalisedVar: string,
  followerVar: string,
  inputVar: string,
  memberFollower: string,
  attackTau: number,
  decayTau: number,
  floor: number,
): string {
  return [
    `    const float ${normalisedVar}_atkAlpha = 1.0f - expf(-rawDt / ${Math.max(attackTau, 0.001).toFixed(4)}f);`,
    `    const float ${normalisedVar}_decAlpha = 1.0f - expf(-rawDt / ${Math.max(decayTau, 0.001).toFixed(4)}f);`,
    `    if (${inputVar} > ${memberFollower}) {`,
    `        ${memberFollower} += (${inputVar} - ${memberFollower}) * ${normalisedVar}_atkAlpha;`,
    `    } else {`,
    `        ${memberFollower} += (${inputVar} - ${memberFollower}) * ${normalisedVar}_decAlpha;`,
    `    }`,
    `    if (${memberFollower} < ${floor.toFixed(4)}f) ${memberFollower} = ${floor.toFixed(4)}f;`,
    `    const float ${normalisedVar} = fminf(1.0f, fmaxf(0.0f, ${inputVar} / ${memberFollower}));`,
    `    const float ${followerVar} = ${memberFollower};`,
  ].join('\n');
}

export function templateScale(
  varName: string,
  inputVar: string,
  factor: number,
): string {
  return `    const float ${varName} = ${inputVar} * ${factor.toFixed(4)}f;`;
}

// ---------------------------------------------------------------------------
// Geometry nodes
// ---------------------------------------------------------------------------

export function templateGaussian(
  varName: string,
  amplitudeVar: string,
  sigma: number,
  centre: number,
): string {
  const sigma2 = (2.0 * sigma * sigma).toFixed(6);
  return [
    `    // Gaussian distribution (centre-origin, half-strip)`,
    `    float ${varName}[kHalfStrip];`,
    `    {`,
    `        const float sigma2 = ${sigma2}f;`,
    `        for (int i = 0; i < kHalfStrip; i++) {`,
    `            const float dist = static_cast<float>(i) / (kHalfStrip - 1) - ${centre.toFixed(4)}f;`,
    `            ${varName}[i] = ${amplitudeVar} * expf(-(dist * dist) / fmaxf(sigma2, 0.0001f));`,
    `        }`,
    `    }`,
  ].join('\n');
}

export function templateTriangularWave(
  varName: string,
  amplitudeVar: string,
  memberPhase: string,
  spacing: number,
  driftSpeed: number,
  sharpness: number,
): string {
  return [
    `    // Triangular wave with drift (centre-origin, half-strip)`,
    `    ${memberPhase} += ${driftSpeed.toFixed(4)}f * rawDt;`,
    `    float ${varName}[kHalfStrip];`,
    `    {`,
    `        const float spacing = fmaxf(${spacing.toFixed(4)}f, 0.01f);`,
    `        for (int i = 0; i < kHalfStrip; i++) {`,
    `            const float progress = static_cast<float>(i) / (kHalfStrip - 1);`,
    `            const float cellPhase = progress / spacing - ${memberPhase};`,
    `            const float frac = cellPhase - floorf(cellPhase);`,
    `            const float tri = fmaxf(0.0f, fminf(1.0f, 1.0f - fabsf(2.0f * frac - 1.0f)));`,
    `            ${varName}[i] = ${amplitudeVar} * powf(tri, ${sharpness.toFixed(1)}f);`,
    `        }`,
    `    }`,
  ].join('\n');
}

// ---------------------------------------------------------------------------
// Composition nodes
// ---------------------------------------------------------------------------

export function templateHsvToRgb(
  varName: string,
  valueArrayVar: string,
  hueVar: string,
  saturationVar: string,
  hueOffset: number,
  hueSpread: number,
): string {
  return [
    `    // HSV to RGB conversion (per-pixel)`,
    `    CRGB ${varName}[kHalfStrip];`,
    `    {`,
    `        for (int i = 0; i < kHalfStrip; i++) {`,
    `            const float progress = static_cast<float>(i) / (kHalfStrip - 1);`,
    `            const uint8_t brightness = static_cast<uint8_t>(fminf(255.0f, fmaxf(0.0f, ${valueArrayVar}[i] * 255.0f)));`,
    `            const uint8_t hue = static_cast<uint8_t>(static_cast<int>(${hueVar} + ${hueOffset.toFixed(0)}f + progress * ${hueSpread.toFixed(0)}f) & 0xFF);`,
    `            const uint8_t sat = static_cast<uint8_t>(fminf(255.0f, fmaxf(0.0f, ${saturationVar})));`,
    `            ${varName}[i] = CHSV(hue, sat, brightness);`,
    `        }`,
    `    }`,
  ].join('\n');
}

// ---------------------------------------------------------------------------
// Output node
// ---------------------------------------------------------------------------

export function templateLedOutput(
  colourArrayVar: string,
  fadeAmount: number,
): string {
  return [
    `    // Centre-origin output with trail persistence`,
    `    const uint8_t fadeScale = ${Math.max(0, 255 - fadeAmount)};`,
    `    for (int i = 0; i < kTotalLeds; i++) {`,
    `        ctx.leds[i].nscale8(fadeScale);`,
    `    }`,
    `    for (int dist = 0; dist < kHalfStrip; dist++) {`,
    `        SET_CENTER_PAIR(ctx.leds, dist, ${colourArrayVar}[dist]);`,
    `    }`,
  ].join('\n');
}

// ---------------------------------------------------------------------------
// Effect class shell
// ---------------------------------------------------------------------------

export function templateEffectShell(
  effectName: string,
  className: string,
  memberDeclarations: string,
  initBody: string,
  renderBody: string,
): string {
  return `#pragma once

#include "effects/IEffect.h"
#include "effects/EffectContext.h"
#include "effects/CoreEffects.h"
#include "audio/SmoothingEngine.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos::effects::generated {

static constexpr int kHalfStrip = 160;
static constexpr int kTotalLeds = 320;

class ${className} : public IEffect {
public:
${memberDeclarations}

    void init() override {
${initBody}
    }

    void render(EffectContext& ctx) override {
        const float rawDt = ctx.dt;
${renderBody}
    }

    void cleanup() override {}

    EffectMetadata getMetadata() const override {
        return { "${effectName}", "generated" };
    }
};

} // namespace lightwaveos::effects::generated
`;
}
