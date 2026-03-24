import { PortType, type NodeDefinition } from '../../engine/types';

const HALF_STRIP = 160;

/**
 * HSV to RGB conversion matching FastLED's hsv2rgb_rainbow.
 * Hue: 0-255 (wrapping), Saturation: 0-255, Value: 0-255.
 */
function hsv2rgb(h: number, s: number, v: number): [number, number, number] {
  h = h & 0xFF;
  s = Math.max(0, Math.min(255, s));
  v = Math.max(0, Math.min(255, v));

  if (s === 0) return [v, v, v];

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

export const hsvToRgbNode: NodeDefinition = {
  type: 'hsv-to-rgb',
  label: 'HSV → RGB',
  layer: 'composition',
  inputs: [
    { name: 'value', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
    { name: 'hue', type: PortType.SCALAR, defaultValue: 0 },
    { name: 'saturation', type: PortType.SCALAR, defaultValue: 255 },
  ],
  outputs: [
    { name: 'out', type: PortType.CRGB_160, defaultValue: new Uint8Array(HALF_STRIP * 3) },
  ],
  parameters: [
    { name: 'hueOffset', min: 0, max: 255, step: 1, defaultValue: 0, label: 'Hue Offset' },
    { name: 'hueSpread', min: 0, max: 128, step: 1, defaultValue: 48, label: 'Spatial Hue Spread' },
  ],
  hasState: false,
  cppTemplate: 'node-hsv-to-rgb',

  evaluate(inputs, params, _state, _dt, _controlBus) {
    const valueArr = inputs.get('value');
    const baseHue = (inputs.get('hue') as number) ?? 0;
    const saturation = (inputs.get('saturation') as number) ?? 255;
    const hueOffset = params.get('hueOffset') ?? 0;
    const hueSpread = params.get('hueSpread') ?? 48;

    const out = new Uint8Array(HALF_STRIP * 3);

    for (let i = 0; i < HALF_STRIP; i++) {
      const progress = i / (HALF_STRIP - 1);

      // Brightness from value array (0-1 float → 0-255 uint8)
      let brightness: number;
      if (valueArr instanceof Float32Array) {
        brightness = Math.max(0, Math.min(255, Math.round((valueArr[i] ?? 0) * 255)));
      } else {
        brightness = Math.max(0, Math.min(255, Math.round(((valueArr as number) ?? 0) * 255)));
      }

      // Hue: base + offset + spatial spread
      const hue = (Math.round(baseHue + hueOffset + progress * hueSpread)) & 0xFF;

      const [r, g, b] = hsv2rgb(hue, Math.round(saturation), brightness);
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
