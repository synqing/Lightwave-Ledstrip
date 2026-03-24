import { PortType, type NodeDefinition } from '../../engine/types';

const HALF_STRIP = 160;
const RGB_SIZE = HALF_STRIP * 3;

export const scaleBrightnessNode: NodeDefinition = {
  type: 'scale-brightness',
  label: 'Scale Brightness',
  layer: 'composition',
  inputs: [
    { name: 'colour', type: PortType.CRGB_160, defaultValue: new Uint8Array(RGB_SIZE) },
    { name: 'scale', type: PortType.SCALAR, defaultValue: 1.0 },
  ],
  outputs: [
    { name: 'out', type: PortType.CRGB_160, defaultValue: new Uint8Array(RGB_SIZE) },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-scale-brightness',

  evaluate(inputs, _params, _state, _dt, _controlBus) {
    const colour = inputs.get('colour');
    const scale = Math.max(0, Math.min(1, (inputs.get('scale') as number) ?? 1.0));

    const out = new Uint8Array(RGB_SIZE);
    const arr = colour instanceof Uint8Array ? colour : new Uint8Array(RGB_SIZE);

    for (let i = 0; i < RGB_SIZE; i++) {
      out[i] = Math.round((arr[i] ?? 0) * scale);
    }

    return {
      outputs: new Map([['out', out]]),
      nextState: null,
    };
  },
};
