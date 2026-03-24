import { PortType, type NodeDefinition } from '../../engine/types';

const HALF_STRIP = 160;
const FULL_STRIP = 320;

/**
 * LED Output node — takes CRGB_160 (half-strip) and produces CRGB_320
 * via centre-origin mirroring matching SET_CENTER_PAIR from CoreEffects.h.
 *
 * Centre pair: LEDs 79/80 (index 0 in half-strip)
 * Mirror: LED 79-dist = half[dist], LED 80+dist = half[dist]
 * Strip 2 (160-319): identical to strip 1.
 */
export const ledOutputNode: NodeDefinition = {
  type: 'led-output',
  label: 'LED Output',
  layer: 'output',
  inputs: [
    { name: 'colour', type: PortType.CRGB_160, defaultValue: new Uint8Array(HALF_STRIP * 3) },
  ],
  outputs: [
    { name: 'leds', type: PortType.CRGB_160, defaultValue: new Uint8Array(FULL_STRIP * 3) },
  ],
  parameters: [
    { name: 'fadeAmount', min: 0, max: 60, step: 1, defaultValue: 20, label: 'Trail Fade Amount' },
  ],
  hasState: true,
  cppTemplate: 'node-led-output',

  getInitialState() {
    return { prevLeds: new Uint8Array(FULL_STRIP * 3) };
  },

  evaluate(inputs, params, state, _dt, _controlBus) {
    const halfStrip = inputs.get('colour');
    const fadeAmount = params.get('fadeAmount') ?? 20;
    const prev = (state?.prevLeds as Uint8Array) ?? new Uint8Array(FULL_STRIP * 3);

    const leds = new Uint8Array(FULL_STRIP * 3);

    // Apply trail persistence: fade previous frame
    const fadeScale = Math.max(0, 255 - fadeAmount) / 255;
    for (let i = 0; i < FULL_STRIP * 3; i++) {
      leds[i] = Math.round((prev[i] ?? 0) * fadeScale);
    }

    if (halfStrip instanceof Uint8Array) {
      // Centre-origin mirror: SET_CENTER_PAIR pattern
      for (let dist = 0; dist < HALF_STRIP; dist++) {
        const r = halfStrip[dist * 3] ?? 0;
        const g = halfStrip[dist * 3 + 1] ?? 0;
        const b = halfStrip[dist * 3 + 2] ?? 0;

        // Strip 1: LED 79-dist (left), LED 80+dist (right)
        const left1 = 79 - dist;
        const right1 = 80 + dist;

        // Strip 2: LED 239-dist (left), LED 240+dist (right)
        const left2 = 239 - dist;
        const right2 = 240 + dist;

        // Max blend (new frame over faded previous)
        const indices = [left1, right1, left2, right2];
        for (const idx of indices) {
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
