import { PortType, type NodeDefinition } from '../../engine/types';

export const snareEnergyNode: NodeDefinition = {
  type: 'snare-energy-source',
  label: 'Snare Energy',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-snare-energy',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const value = controlBus?.snareEnergy ?? 0;
    return {
      outputs: new Map([['out', value]]),
      nextState: null,
    };
  },
};
