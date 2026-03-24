import { PortType, type NodeDefinition } from '../../engine/types';

const HALF_STRIP = 160;
const RGB_SIZE = HALF_STRIP * 3;

export const blendNode: NodeDefinition = {
  type: 'blend',
  label: 'Blend',
  layer: 'composition',
  inputs: [
    { name: 'a', type: PortType.CRGB_160, defaultValue: new Uint8Array(RGB_SIZE) },
    { name: 'b', type: PortType.CRGB_160, defaultValue: new Uint8Array(RGB_SIZE) },
    { name: 'factor', type: PortType.SCALAR, defaultValue: 0.5 },
  ],
  outputs: [
    { name: 'out', type: PortType.CRGB_160, defaultValue: new Uint8Array(RGB_SIZE) },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-blend',

  evaluate(inputs, _params, _state, _dt, _controlBus) {
    const a = inputs.get('a');
    const b = inputs.get('b');
    const factor = Math.max(0, Math.min(1, (inputs.get('factor') as number) ?? 0.5));
    const invFactor = 1.0 - factor;

    const out = new Uint8Array(RGB_SIZE);

    const arrA = a instanceof Uint8Array ? a : new Uint8Array(RGB_SIZE);
    const arrB = b instanceof Uint8Array ? b : new Uint8Array(RGB_SIZE);

    for (let i = 0; i < RGB_SIZE; i++) {
      out[i] = Math.round((arrA[i] ?? 0) * invFactor + (arrB[i] ?? 0) * factor);
    }

    return {
      outputs: new Map([['out', out]]),
      nextState: null,
    };
  },
};
