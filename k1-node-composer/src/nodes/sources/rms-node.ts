import { PortType, type NodeDefinition } from '../../engine/types';

export const rmsNode: NodeDefinition = {
  type: 'rms-source',
  label: 'RMS',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-rms',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const rms = controlBus?.rms ?? 0;
    return {
      outputs: new Map([['out', rms]]),
      nextState: null,
    };
  },
};
