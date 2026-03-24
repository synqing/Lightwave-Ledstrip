import { PortType, type NodeDefinition } from '../../engine/types';

export const downbeatTickNode: NodeDefinition = {
  type: 'downbeat-tick-source',
  label: 'Downbeat Tick',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.BOOL, defaultValue: false },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-downbeat-tick',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const value = controlBus?.downbeatTick ?? false;
    return {
      outputs: new Map([['out', value]]),
      nextState: null,
    };
  },
};
