import { PortType, type NodeDefinition } from '../../engine/types';

export const snareTriggerNode: NodeDefinition = {
  type: 'snare-trigger-source',
  label: 'Snare Trigger',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.BOOL, defaultValue: false },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-snare-trigger',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const value = controlBus?.snareTrigger ?? false;
    return {
      outputs: new Map([['out', value]]),
      nextState: null,
    };
  },
};
