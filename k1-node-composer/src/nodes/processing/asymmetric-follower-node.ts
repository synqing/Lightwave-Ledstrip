import { PortType, type NodeDefinition, type NodeState } from '../../engine/types';

export const asymmetricFollowerNode: NodeDefinition = {
  type: 'asymmetric-follower',
  label: 'Asymmetric Follower',
  layer: 'processing',
  inputs: [
    { name: 'value', type: PortType.SCALAR, defaultValue: 0 },
  ],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [
    { name: 'riseTau', min: 0.01, max: 2.0, step: 0.01, defaultValue: 0.03, label: 'Rise Tau (s)' },
    { name: 'fallTau', min: 0.01, max: 5.0, step: 0.01, defaultValue: 0.20, label: 'Fall Tau (s)' },
  ],
  hasState: true,
  cppTemplate: 'node-asymmetric-follower',

  getInitialState(): NodeState {
    return { value: 0 };
  },

  evaluate(inputs, params, state, dt, _controlBus) {
    const input = inputs.get('value') as number ?? 0;
    const riseTau = params.get('riseTau') ?? 0.03;
    const fallTau = params.get('fallTau') ?? 0.20;
    const prev = (state?.value as number) ?? 0;

    // Select attack or decay rate based on signal direction
    const tau = input > prev ? riseTau : fallTau;
    const alpha = 1.0 - Math.exp(-dt / Math.max(tau, 0.001));
    const next = prev + (input - prev) * alpha;

    return {
      outputs: new Map([['out', next]]),
      nextState: { value: next },
    };
  },
};
