import { PortType, type NodeDefinition } from '../../engine/types';

export const hihatEnergyNode: NodeDefinition = {
  type: 'hihat-energy-source',
  label: 'Hi-Hat Energy',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-hihat-energy',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const value = controlBus?.hihatEnergy ?? 0;
    return {
      outputs: new Map([['out', value]]),
      nextState: null,
    };
  },
};
