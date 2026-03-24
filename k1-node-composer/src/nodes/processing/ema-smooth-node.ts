import { PortType, type NodeDefinition, type NodeState } from '../../engine/types';

export const emaSmoothNode: NodeDefinition = {
  type: 'ema-smooth',
  label: 'EMA Smooth',
  layer: 'processing',
  inputs: [
    { name: 'value', type: PortType.SCALAR, defaultValue: 0 },
  ],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [
    { name: 'tau', min: 0.01, max: 2.0, step: 0.01, defaultValue: 0.05, label: 'Time Constant (s)' },
  ],
  hasState: true,
  cppTemplate: 'node-ema-smooth',

  getInitialState(): NodeState {
    return { value: 0 };
  },

  evaluate(inputs, params, state, dt, _controlBus) {
    const target = inputs.get('value') as number ?? 0;
    const tau = params.get('tau') ?? 0.05;
    const prev = (state?.value as number) ?? 0;

    const alpha = 1.0 - Math.exp(-dt / Math.max(tau, 0.001));
    const next = prev + (target - prev) * alpha;

    return {
      outputs: new Map([['out', next]]),
      nextState: { value: next },
    };
  },
};
