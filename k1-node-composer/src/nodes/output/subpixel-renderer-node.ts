import { PortType, type NodeDefinition } from '../../engine/types';

const HALF_STRIP = 160;
const FULL_STRIP = 320;

/**
 * Subpixel Renderer node — anti-aliased LED output.
 *
 * Takes a CRGB_160 (half-strip) input and a resolution multiplier.
 * Treats the input as a higher-resolution virtual strip, then
 * downsamples with linear interpolation to kHalfStrip for smoother
 * visual output. Applies centre-origin SET_CENTER_PAIR mirroring.
 *
 * Supports dual-strip independent mode (dualStripMode=1) where
 * strip 2 uses a separate colour2 input.
 *
 * Resolution multiplier:
 *   1.0 = no interpolation (same as standard LED Output)
 *   2.0 = treat input as 320 virtual pixels, downsample to 160
 *   4.0 = treat input as 640 virtual pixels, downsample to 160
 */

/** Interpolate a CRGB half-strip with subpixel anti-aliasing. */
function interpolateHalfStrip(
  source: Uint8Array,
  resolution: number,
): Uint8Array {
  const interpolated = new Uint8Array(HALF_STRIP * 3);

  if (resolution <= 1.0) {
    for (let i = 0; i < HALF_STRIP * 3; i++) {
      interpolated[i] = source[i] ?? 0;
    }
  } else {
    for (let i = 0; i < HALF_STRIP; i++) {
      const srcPos = (i / (HALF_STRIP - 1)) * (HALF_STRIP - 1);
      const subpixelShift = (resolution - 1.0) / resolution;
      const fractionalPos = srcPos + (Math.sin(srcPos * Math.PI / HALF_STRIP * resolution) * subpixelShift);

      const clampedPos = Math.max(0, Math.min(HALF_STRIP - 1, fractionalPos));
      const idx0 = Math.floor(clampedPos);
      const idx1 = Math.min(idx0 + 1, HALF_STRIP - 1);
      const frac = clampedPos - idx0;

      const r0 = source[idx0 * 3] ?? 0;
      const g0 = source[idx0 * 3 + 1] ?? 0;
      const b0 = source[idx0 * 3 + 2] ?? 0;
      const r1 = source[idx1 * 3] ?? 0;
      const g1 = source[idx1 * 3 + 1] ?? 0;
      const b1 = source[idx1 * 3 + 2] ?? 0;

      interpolated[i * 3] = Math.round(r0 + (r1 - r0) * frac);
      interpolated[i * 3 + 1] = Math.round(g0 + (g1 - g0) * frac);
      interpolated[i * 3 + 2] = Math.round(b0 + (b1 - b0) * frac);
    }
  }

  return interpolated;
}

export const subpixelRendererNode: NodeDefinition = {
  type: 'subpixel-renderer',
  label: 'Subpixel Renderer',
  layer: 'output',
  inputs: [
    { name: 'colour', type: PortType.CRGB_160, defaultValue: new Uint8Array(HALF_STRIP * 3) },
    { name: 'colour2', type: PortType.CRGB_160, defaultValue: new Uint8Array(HALF_STRIP * 3) },
  ],
  outputs: [
    { name: 'leds', type: PortType.CRGB_160, defaultValue: new Uint8Array(FULL_STRIP * 3) },
  ],
  parameters: [
    { name: 'resolution', min: 1.0, max: 8.0, step: 0.5, defaultValue: 2.0, label: 'Resolution Multiplier' },
    { name: 'fadeAmount', min: 0, max: 60, step: 1, defaultValue: 20, label: 'Trail Fade Amount' },
    { name: 'dualStripMode', min: 0, max: 1, step: 1, defaultValue: 0, label: 'Strip Mode (0=Locked, 1=Independent)' },
  ],
  hasState: true,
  cppTemplate: 'node-subpixel-renderer',

  getInitialState() {
    return { prevLeds: new Uint8Array(FULL_STRIP * 3) };
  },

  evaluate(inputs, params, state, _dt, _controlBus) {
    const halfStrip = inputs.get('colour');
    const halfStrip2 = inputs.get('colour2');
    const resolution = params.get('resolution') ?? 2.0;
    const fadeAmount = params.get('fadeAmount') ?? 20;
    const dualStripMode = Math.round(params.get('dualStripMode') ?? 0);
    const prev = (state?.prevLeds as Uint8Array) ?? new Uint8Array(FULL_STRIP * 3);

    const leds = new Uint8Array(FULL_STRIP * 3);

    // Apply trail persistence: fade previous frame
    const fadeScale = Math.max(0, 255 - fadeAmount) / 255;
    for (let i = 0; i < FULL_STRIP * 3; i++) {
      leds[i] = Math.round((prev[i] ?? 0) * fadeScale);
    }

    if (halfStrip instanceof Uint8Array) {
      const interpolated = interpolateHalfStrip(halfStrip, resolution);

      // Centre-origin mirror for strip 1
      for (let dist = 0; dist < HALF_STRIP; dist++) {
        const r = interpolated[dist * 3] ?? 0;
        const g = interpolated[dist * 3 + 1] ?? 0;
        const b = interpolated[dist * 3 + 2] ?? 0;

        const left1 = 79 - dist;
        const right1 = 80 + dist;

        const strip1Indices = [left1, right1];
        for (const idx of strip1Indices) {
          if (idx >= 0 && idx < HALF_STRIP) {
            leds[idx * 3] = Math.max(leds[idx * 3] ?? 0, r);
            leds[idx * 3 + 1] = Math.max(leds[idx * 3 + 1] ?? 0, g);
            leds[idx * 3 + 2] = Math.max(leds[idx * 3 + 2] ?? 0, b);
          }
        }

        if (dualStripMode === 0) {
          // Locked: strip 2 mirrors strip 1
          const left2 = 239 - dist;
          const right2 = 240 + dist;
          const strip2Indices = [left2, right2];
          for (const idx of strip2Indices) {
            if (idx >= 0 && idx < FULL_STRIP) {
              leds[idx * 3] = Math.max(leds[idx * 3] ?? 0, r);
              leds[idx * 3 + 1] = Math.max(leds[idx * 3 + 1] ?? 0, g);
              leds[idx * 3 + 2] = Math.max(leds[idx * 3 + 2] ?? 0, b);
            }
          }
        }
      }
    }

    // Independent mode: strip 2 from colour2 input with its own interpolation
    if (dualStripMode === 1 && halfStrip2 instanceof Uint8Array) {
      const interpolated2 = interpolateHalfStrip(halfStrip2, resolution);

      for (let dist = 0; dist < HALF_STRIP; dist++) {
        const r = interpolated2[dist * 3] ?? 0;
        const g = interpolated2[dist * 3 + 1] ?? 0;
        const b = interpolated2[dist * 3 + 2] ?? 0;

        const left2 = 239 - dist;
        const right2 = 240 + dist;

        const strip2Indices = [left2, right2];
        for (const idx of strip2Indices) {
          if (idx >= 0 && idx < FULL_STRIP) {
            leds[idx * 3] = Math.max(leds[idx * 3] ?? 0, r);
            leds[idx * 3 + 1] = Math.max(leds[idx * 3 + 1] ?? 0, g);
            leds[idx * 3 + 2] = Math.max(leds[idx * 3 + 2] ?? 0, b);
          }
        }
      }
    }

    return {
      outputs: new Map([['leds', leds]]),
      nextState: { prevLeds: leds },
    };
  },
};
