/**
 * C++ code generation templates for node types.
 *
 * Each template function returns one or more lines of C++ code
 * to be inserted into the render() method body, or member variable
 * declarations for the class body.
 *
 * Generated code targets the LightwaveOS plugins::IEffect interface:
 *   - Centre-origin rendering via SET_CENTER_PAIR
 *   - No heap allocation in render()
 *   - British English in comments
 *   - ctx.audio.* for audio access (backend-agnostic)
 */

// ---------------------------------------------------------------------------
// Source nodes
// ---------------------------------------------------------------------------

export function templateRms(varName: string): string {
  return `    const float ${varName} = ctx.audio.available ? ctx.audio.rms() : 0.0f;`;
}

export function templateBand(varName: string, index: number): string {
  return `    const float ${varName} = ctx.audio.available ? ctx.audio.getBand(${index}) : 0.0f;`;
}

export function templateBeatStrength(varName: string): string {
  return `    const float ${varName} = ctx.audio.available ? ctx.audio.beatStrength() : 0.0f;`;
}

export function templateSilentScale(varName: string): string {
  return `    const float ${varName} = ctx.audio.available ? ctx.audio.silentScale() : 1.0f;`;
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

export function templateMultiply(
  varName: string,
  inputA: string,
  inputB: string,
): string {
  return `    const float ${varName} = ${inputA} * ${inputB};`;
}

export function templateMultiplyArray(
  varName: string,
  inputA: string,
  inputB: string,
): string {
  return [
    `    float ${varName}[kHalfStrip];`,
    `    for (int i = 0; i < kHalfStrip; i++) {`,
    `        ${varName}[i] = ${inputA}[i] * ${inputB};`,
    `    }`,
  ].join('\n');
}

export function templateAdd(
  varName: string,
  inputA: string,
  inputB: string,
): string {
  return `    const float ${varName} = ${inputA} + ${inputB};`;
}

export function templatePower(
  varName: string,
  inputVar: string,
  exponent: number,
): string {
  if (exponent === 2.0) {
    return `    const float ${varName} = ${inputVar} * ${inputVar};`;
  }
  return `    const float ${varName} = powf(${inputVar}, ${exponent.toFixed(1)}f);`;
}

export function templatePowerArray(
  varName: string,
  inputVar: string,
  exponent: number,
): string {
  if (exponent === 2.0) {
    return [
      `    float ${varName}[kHalfStrip];`,
      `    for (int i = 0; i < kHalfStrip; i++) {`,
      `        ${varName}[i] = ${inputVar}[i] * ${inputVar}[i];`,
      `    }`,
    ].join('\n');
  }
  return [
    `    float ${varName}[kHalfStrip];`,
    `    for (int i = 0; i < kHalfStrip; i++) {`,
    `        ${varName}[i] = powf(${inputVar}[i], ${exponent.toFixed(1)}f);`,
    `    }`,
  ].join('\n');
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
  dualStripMode: number = 0,
  colour2Var: string = '',
): string {
  const lines = [
    `    // Centre-origin output with trail persistence`,
    `    const uint8_t fadeScale = ${Math.max(0, 255 - fadeAmount)};`,
    `    for (uint16_t i = 0; i < ctx.ledCount; i++) {`,
    `        ctx.leds[i].nscale8(fadeScale);`,
    `    }`,
  ];

  if (dualStripMode === 1 && colour2Var) {
    // Independent dual-strip mode
    lines.push(
      `    // Strip 1: centre-origin from colour input`,
      `    for (int dist = 0; dist < kHalfStrip; dist++) {`,
      `        const uint16_t left = 79 - dist;`,
      `        const uint16_t right = 80 + dist;`,
      `        if (left < kHalfStrip) ctx.leds[left] = ${colourArrayVar}[dist];`,
      `        if (right < kHalfStrip) ctx.leds[right] = ${colourArrayVar}[dist];`,
      `    }`,
      `    // Strip 2: independent centre-origin from colour2 input`,
      `    for (int dist = 0; dist < kHalfStrip; dist++) {`,
      `        const uint16_t left2 = 239 - dist;`,
      `        const uint16_t right2 = 240 + dist;`,
      `        if (left2 >= kHalfStrip && left2 < ctx.ledCount) ctx.leds[left2] = ${colour2Var}[dist];`,
      `        if (right2 >= kHalfStrip && right2 < ctx.ledCount) ctx.leds[right2] = ${colour2Var}[dist];`,
      `    }`,
    );
  } else {
    // Locked mode: both strips identical via SET_CENTER_PAIR
    lines.push(
      `    for (int dist = 0; dist < kHalfStrip; dist++) {`,
      `        SET_CENTER_PAIR(ctx, dist, ${colourArrayVar}[dist]);`,
      `    }`,
    );
  }

  return lines.join('\n');
}

// ---------------------------------------------------------------------------
// Subpixel renderer output node
// ---------------------------------------------------------------------------

export function templateSubpixelRenderer(
  colourArrayVar: string,
  fadeAmount: number,
  resolution: number,
  dualStripMode: number = 0,
  colour2Var: string = '',
): string {
  const interpBlock = (srcVar: string, pixelVar: string): string[] => [
    `        if (spResolution <= 1.0f) {`,
    `            ${pixelVar} = ${srcVar}[dist];`,
    `        } else {`,
    `            const float subpixelShift = (spResolution - 1.0f) / spResolution;`,
    `            const float srcPos = static_cast<float>(dist);`,
    `            const float fractionalPos = srcPos + sinf(srcPos * M_PI / kHalfStrip * spResolution) * subpixelShift;`,
    `            const float clampedPos = fminf(static_cast<float>(kHalfStrip - 1), fmaxf(0.0f, fractionalPos));`,
    `            const int idx0 = static_cast<int>(clampedPos);`,
    `            const int idx1 = min(idx0 + 1, kHalfStrip - 1);`,
    `            const float frac = clampedPos - static_cast<float>(idx0);`,
    `            ${pixelVar}.r = static_cast<uint8_t>(${srcVar}[idx0].r + (${srcVar}[idx1].r - ${srcVar}[idx0].r) * frac);`,
    `            ${pixelVar}.g = static_cast<uint8_t>(${srcVar}[idx0].g + (${srcVar}[idx1].g - ${srcVar}[idx0].g) * frac);`,
    `            ${pixelVar}.b = static_cast<uint8_t>(${srcVar}[idx0].b + (${srcVar}[idx1].b - ${srcVar}[idx0].b) * frac);`,
    `        }`,
  ];

  const lines = [
    `    // Subpixel renderer — anti-aliased centre-origin output`,
    `    const uint8_t spFadeScale = ${Math.max(0, 255 - fadeAmount)};`,
    `    for (uint16_t i = 0; i < ctx.ledCount; i++) {`,
    `        ctx.leds[i].nscale8(spFadeScale);`,
    `    }`,
    `    const float spResolution = ${resolution.toFixed(1)}f;`,
  ];

  if (dualStripMode === 1 && colour2Var) {
    // Independent mode: strip 1 and strip 2 rendered separately
    lines.push(
      `    // Strip 1: subpixel-interpolated centre-origin`,
      `    for (int dist = 0; dist < kHalfStrip; dist++) {`,
      `        CRGB pixel;`,
      ...interpBlock(colourArrayVar, 'pixel'),
      `        const uint16_t left = 79 - dist;`,
      `        const uint16_t right = 80 + dist;`,
      `        if (left < kHalfStrip) ctx.leds[left] = pixel;`,
      `        if (right < kHalfStrip) ctx.leds[right] = pixel;`,
      `    }`,
      `    // Strip 2: independent subpixel-interpolated centre-origin`,
      `    for (int dist = 0; dist < kHalfStrip; dist++) {`,
      `        CRGB pixel2;`,
      ...interpBlock(colour2Var, 'pixel2'),
      `        const uint16_t left2 = 239 - dist;`,
      `        const uint16_t right2 = 240 + dist;`,
      `        if (left2 >= kHalfStrip && left2 < ctx.ledCount) ctx.leds[left2] = pixel2;`,
      `        if (right2 >= kHalfStrip && right2 < ctx.ledCount) ctx.leds[right2] = pixel2;`,
      `    }`,
    );
  } else {
    // Locked mode: both strips via SET_CENTER_PAIR
    lines.push(
      `    for (int dist = 0; dist < kHalfStrip; dist++) {`,
      `        CRGB pixel;`,
      ...interpBlock(colourArrayVar, 'pixel'),
      `        SET_CENTER_PAIR(ctx, dist, pixel);`,
      `    }`,
    );
  }

  return lines.join('\n');
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

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class ${className} final : public plugins::IEffect {
public:
${memberDeclarations}

    ${className}() = default;
    ~${className}() override = default;

    bool init(plugins::EffectContext& ctx) override {
        (void)ctx;
${initBody}
        return true;
    }

    void render(plugins::EffectContext& ctx) override {
        const float rawDt = ctx.getSafeRawDeltaSeconds();
${renderBody}
    }

    void cleanup() override {}

    const plugins::EffectMetadata& getMetadata() const override {
        static plugins::EffectMetadata meta{
            "${effectName}",
            "Generated by K1 Node Composer",
            plugins::EffectCategory::CUSTOM, 1
        };
        return meta;
    }

    uint8_t getParameterCount() const override { return 0; }
    const plugins::EffectParameter* getParameter(uint8_t) const override { return nullptr; }
    bool setParameter(const char*, float) override { return false; }
    float getParameter(const char*) const override { return 0.0f; }
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
`;
}
