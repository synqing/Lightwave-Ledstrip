import { PortType, type NodeDefinition } from '../../engine/types';

export const livelinessNode: NodeDefinition = {
  type: 'liveliness-source',
  label: 'Liveliness',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-liveliness',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const value = controlBus?.liveliness ?? 0;
    return {
      outputs: new Map([['out', value]]),
      nextState: null,
    };
  },
};
