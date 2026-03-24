import { PortType, type NodeDefinition } from '../../engine/types';

export const mixNode: NodeDefinition = {
  type: 'mix',
  label: 'Mix',
  layer: 'processing',
  inputs: [
    { name: 'a', type: PortType.SCALAR, defaultValue: 0 },
    { name: 'b', type: PortType.SCALAR, defaultValue: 0 },
    { name: 'mix', type: PortType.SCALAR, defaultValue: 0.5 },
  ],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-mix',

  evaluate(inputs, _params, _state, _dt, _controlBus) {
    const a = inputs.get('a') as number ?? 0;
    const b = inputs.get('b') as number ?? 0;
    const t = inputs.get('mix') as number ?? 0.5;

    // Linear interpolation: a * (1 - t) + b * t
    const out = a * (1.0 - t) + b * t;

    return {
      outputs: new Map([['out', out]]),
      nextState: null,
    };
  },
};
