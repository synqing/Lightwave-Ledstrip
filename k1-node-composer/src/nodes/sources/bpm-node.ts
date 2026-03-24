import { PortType, type NodeDefinition } from '../../engine/types';

export const bpmNode: NodeDefinition = {
  type: 'bpm-source',
  label: 'BPM',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-bpm',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const value = controlBus?.bpm ?? 0;
    return {
      outputs: new Map([['out', value]]),
      nextState: null,
    };
  },
};
