import { PortType, type NodeDefinition } from '../../engine/types';

export const fastRmsNode: NodeDefinition = {
  type: 'fast-rms-source',
  label: 'Fast RMS',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-fast-rms',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const value = controlBus?.fastRms ?? 0;
    return {
      outputs: new Map([['out', value]]),
      nextState: null,
    };
  },
};
