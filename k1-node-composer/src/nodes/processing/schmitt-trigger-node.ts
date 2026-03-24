import { PortType, type NodeDefinition, type NodeState } from '../../engine/types';

export const schmittTriggerNode: NodeDefinition = {
  type: 'schmitt-trigger',
  label: 'Schmitt Trigger',
  layer: 'processing',
  inputs: [
    { name: 'value', type: PortType.SCALAR, defaultValue: 0 },
  ],
  outputs: [
    { name: 'out', type: PortType.BOOL, defaultValue: false },
  ],
  parameters: [
    { name: 'openThreshold', min: 0, max: 1, step: 0.01, defaultValue: 0.6, label: 'Open Threshold' },
    { name: 'closeThreshold', min: 0, max: 1, step: 0.01, defaultValue: 0.3, label: 'Close Threshold' },
  ],
  hasState: true,
  cppTemplate: 'node-schmitt-trigger',

  getInitialState(): NodeState {
    return { isOpen: false };
  },

  evaluate(inputs, params, state, _dt, _controlBus) {
    const value = inputs.get('value') as number ?? 0;
    const openThreshold = params.get('openThreshold') ?? 0.6;
    const closeThreshold = params.get('closeThreshold') ?? 0.3;
    let isOpen = (state?.isOpen as boolean) ?? false;

    // Hysteresis: open on rising edge, close on falling edge
    if (!isOpen && value > openThreshold) {
      isOpen = true;
    } else if (isOpen && value < closeThreshold) {
      isOpen = false;
    }

    return {
      outputs: new Map([['out', isOpen]]),
      nextState: { isOpen },
    };
  },
};
