/**
 * Palette Lookup node — maps index + brightness to CRGB via a colour palette.
 *
 * Firmware equivalent: `ctx.palette.getColor(index, brightness)`
 *
 * Uses a built-in rainbow palette for the POC. The palette maps
 * index (0-255) to hue, then applies brightness as value.
 * This matches how most firmware effects use palette-driven colour.
 */

import { PortType, type NodeDefinition } from '../../engine/types';

const HALF_STRIP = 160;

/** Simple rainbow palette: index 0-255 → hue 0-255 */
function paletteGetColor(index: number, brightness: number): [number, number, number] {
  // Convert palette index to HSV, then to RGB
  const h = index & 0xFF;
  const s = 255;
  const v = Math.max(0, Math.min(255, Math.round(brightness)));

  if (v === 0) return [0, 0, 0];

  const region = (h / 43) | 0;
  const remainder = (h - region * 43) * 6;
  const p = (v * (255 - s)) >> 8;
  const q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  const t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

  switch (region) {
    case 0:  return [v, t, p];
    case 1:  return [q, v, p];
    case 2:  return [p, v, t];
    case 3:  return [p, q, v];
    case 4:  return [t, p, v];
    default: return [v, p, q];
  }
}

export const paletteLookupNode: NodeDefinition = {
  type: 'palette-lookup',
  label: 'Palette Lookup',
  layer: 'composition',
  inputs: [
    { name: 'brightness', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
    { name: 'index', type: PortType.SCALAR, defaultValue: 0 },
  ],
  outputs: [
    { name: 'out', type: PortType.CRGB_160, defaultValue: new Uint8Array(HALF_STRIP * 3) },
  ],
  parameters: [
    { name: 'indexOffset', min: 0, max: 255, step: 1, defaultValue: 0, label: 'Palette Index Offset' },
    { name: 'indexSpread', min: 0, max: 128, step: 1, defaultValue: 48, label: 'Spatial Index Spread' },
  ],
  hasState: false,
  cppTemplate: 'node-palette-lookup',

  evaluate(inputs, params, _state, _dt, _controlBus) {
    const brightnessArr = inputs.get('brightness');
    const baseIndex = (inputs.get('index') as number) ?? 0;
    const indexOffset = params.get('indexOffset') ?? 0;
    const indexSpread = params.get('indexSpread') ?? 48;

    const out = new Uint8Array(HALF_STRIP * 3);

    for (let i = 0; i < HALF_STRIP; i++) {
      const progress = i / (HALF_STRIP - 1);

      let brightness: number;
      if (brightnessArr instanceof Float32Array) {
        brightness = (brightnessArr[i] ?? 0) * 255;
      } else {
        brightness = ((brightnessArr as number) ?? 0) * 255;
      }

      const palIndex = (Math.round(baseIndex + indexOffset + progress * indexSpread)) & 0xFF;
      const [r, g, b] = paletteGetColor(palIndex, brightness);
      out[i * 3] = r!;
      out[i * 3 + 1] = g!;
      out[i * 3 + 2] = b!;
    }

    return {
      outputs: new Map([['out', out]]),
      nextState: null,
    };
  },
};
