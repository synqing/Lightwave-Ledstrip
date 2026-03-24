import { PortType, type NodeDefinition } from '../../engine/types';

export const beatTickNode: NodeDefinition = {
  type: 'beat-tick-source',
  label: 'Beat Tick',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.BOOL, defaultValue: false },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-beat-tick',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const value = controlBus?.beatTick ?? false;
    return {
      outputs: new Map([['out', value]]),
      nextState: null,
    };
  },
};
