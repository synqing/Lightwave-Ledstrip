import { PortType, type NodeDefinition, type NodeState } from '../../engine/types';

export const maxFollowerNode: NodeDefinition = {
  type: 'max-follower',
  label: 'Max Follower',
  layer: 'processing',
  inputs: [
    { name: 'value', type: PortType.SCALAR, defaultValue: 0 },
  ],
  outputs: [
    { name: 'normalised', type: PortType.SCALAR, defaultValue: 0 },
    { name: 'follower', type: PortType.SCALAR, defaultValue: 0.15 },
  ],
  parameters: [
    { name: 'attackTau', min: 0.01, max: 1.0, step: 0.01, defaultValue: 0.058, label: 'Attack Tau (s)' },
    { name: 'decayTau', min: 0.05, max: 5.0, step: 0.05, defaultValue: 0.50, label: 'Decay Tau (s)' },
    { name: 'floor', min: 0.001, max: 0.5, step: 0.001, defaultValue: 0.04, label: 'Floor' },
  ],
  hasState: true,
  cppTemplate: 'node-max-follower',

  getInitialState(): NodeState {
    return { follower: 0.15 };
  },

  evaluate(inputs, params, state, dt, _controlBus) {
    const value = inputs.get('value') as number ?? 0;
    const attackTau = params.get('attackTau') ?? 0.058;
    const decayTau = params.get('decayTau') ?? 0.50;
    const floor = params.get('floor') ?? 0.04;

    let follower = (state?.follower as number) ?? 0.15;

    const attackAlpha = 1.0 - Math.exp(-dt / Math.max(attackTau, 0.001));
    const decayAlpha = 1.0 - Math.exp(-dt / Math.max(decayTau, 0.001));

    if (value > follower) {
      follower += (value - follower) * attackAlpha;
    } else {
      follower += (value - follower) * decayAlpha;
    }
    if (follower < floor) follower = floor;

    const normalised = Math.min(1.0, Math.max(0.0, value / follower));

    return {
      outputs: new Map([
        ['normalised', normalised],
        ['follower', follower],
      ]),
      nextState: { follower },
    };
  },
};
