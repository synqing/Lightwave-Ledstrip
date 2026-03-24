import { PortType, type NodeDefinition } from '../../engine/types';

export const beatStrengthNode: NodeDefinition = {
  type: 'beat-strength-source',
  label: 'Beat Strength',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-beat-strength',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const strength = controlBus?.beatStrength ?? 0;
    return {
      outputs: new Map([['out', strength]]),
      nextState: null,
    };
  },
};
