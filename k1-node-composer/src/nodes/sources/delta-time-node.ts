import { PortType, type NodeDefinition } from '../../engine/types';

/** Outputs the frame delta time (seconds since last evaluate). */
export const deltaTimeNode: NodeDefinition = {
  type: 'delta-time-source',
  label: 'Delta Time',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-delta-time',

  evaluate(_inputs, _params, _state, dt, _controlBus) {
    return {
      outputs: new Map([['out', dt]]),
      nextState: null,
    };
  },
};
