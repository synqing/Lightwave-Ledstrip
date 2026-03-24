import { PortType, type NodeDefinition, type NodeState } from '../../engine/types';

const TWO_PI = 2.0 * Math.PI;

export const circularEmaNode: NodeDefinition = {
  type: 'circular-ema',
  label: 'Circular EMA',
  layer: 'processing',
  inputs: [
    { name: 'value', type: PortType.ANGLE, defaultValue: 0 },
  ],
  outputs: [
    { name: 'out', type: PortType.ANGLE, defaultValue: 0 },
  ],
  parameters: [
    { name: 'tau', min: 0.01, max: 5.0, step: 0.01, defaultValue: 0.30, label: 'Time Constant (s)' },
  ],
  hasState: true,
  cppTemplate: 'node-circular-ema',

  getInitialState(): NodeState {
    return { angle: 0 };
  },

  evaluate(inputs, params, state, dt, _controlBus) {
    const target = inputs.get('value') as number ?? 0;
    const tau = params.get('tau') ?? 0.30;
    const prev = (state?.angle as number) ?? 0;

    const alpha = 1.0 - Math.exp(-dt / Math.max(tau, 0.001));

    // Shortest-arc delta for circular interpolation
    let delta = target - prev;
    while (delta > Math.PI) delta -= TWO_PI;
    while (delta < -Math.PI) delta += TWO_PI;

    // Wrap result back into [0, 2pi)
    let next = prev + delta * alpha;
    next = ((next % TWO_PI) + TWO_PI) % TWO_PI;

    return {
      outputs: new Map([['out', next]]),
      nextState: { angle: next },
    };
  },
};
