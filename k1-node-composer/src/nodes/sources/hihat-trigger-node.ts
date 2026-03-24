import { PortType, type NodeDefinition } from '../../engine/types';

export const hihatTriggerNode: NodeDefinition = {
  type: 'hihat-trigger-source',
  label: 'Hi-Hat Trigger',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.BOOL, defaultValue: false },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-hihat-trigger',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const value = controlBus?.hihatTrigger ?? false;
    return {
      outputs: new Map([['out', value]]),
      nextState: null,
    };
  },
};
