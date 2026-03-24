import { PortType, type NodeDefinition } from '../../engine/types';

const HALF_STRIP = 160;
const RGB_SIZE = HALF_STRIP * 3;

/**
 * Saturating colour addition — matches FastLED's qadd8 behaviour.
 * Each channel is clamped to 255 (no wrap-around).
 */
export const addColourNode: NodeDefinition = {
  type: 'add-colour',
  label: 'Add Colour',
  layer: 'composition',
  inputs: [
    { name: 'a', type: PortType.CRGB_160, defaultValue: new Uint8Array(RGB_SIZE) },
    { name: 'b', type: PortType.CRGB_160, defaultValue: new Uint8Array(RGB_SIZE) },
  ],
  outputs: [
    { name: 'out', type: PortType.CRGB_160, defaultValue: new Uint8Array(RGB_SIZE) },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-add-colour',

  evaluate(inputs, _params, _state, _dt, _controlBus) {
    const a = inputs.get('a');
    const b = inputs.get('b');

    const out = new Uint8Array(RGB_SIZE);
    const arrA = a instanceof Uint8Array ? a : new Uint8Array(RGB_SIZE);
    const arrB = b instanceof Uint8Array ? b : new Uint8Array(RGB_SIZE);

    for (let i = 0; i < RGB_SIZE; i++) {
      // Saturating add: clamp to 255, matching qadd8
      out[i] = Math.min(255, (arrA[i] ?? 0) + (arrB[i] ?? 0));
    }

    return {
      outputs: new Map([['out', out]]),
      nextState: null,
    };
  },
};
