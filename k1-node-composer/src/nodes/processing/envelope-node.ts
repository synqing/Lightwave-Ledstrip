/**
 * Envelope node — the most common audio-reactive pattern.
 *
 * When a trigger fires (input crosses threshold), the envelope jumps to
 * peak, then decays exponentially. Models the impact→peak→decay dynamics
 * that every working audio-reactive effect uses.
 *
 * Firmware equivalent: `if (beatStr > m_impact) m_impact = beatStr;
 *                       m_impact *= exp(-dt / decayTau);`
 */

import { PortType, type NodeDefinition, type NodeState } from '../../engine/types';

export const envelopeNode: NodeDefinition = {
  type: 'envelope',
  label: 'Envelope',
  layer: 'processing',
  inputs: [
    { name: 'trigger', type: PortType.SCALAR, defaultValue: 0 },
  ],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [
    { name: 'peak', min: 0.1, max: 1.0, step: 0.05, defaultValue: 1.0, label: 'Peak Level' },
    { name: 'decayTau', min: 0.02, max: 2.0, step: 0.01, defaultValue: 0.18, label: 'Decay Tau (s)' },
    { name: 'threshold', min: 0.0, max: 1.0, step: 0.01, defaultValue: 0.0, label: 'Trigger Threshold' },
  ],
  hasState: true,
  cppTemplate: 'node-envelope',

  getInitialState(): NodeState {
    return { value: 0 };
  },

  evaluate(inputs, params, state, dt, _controlBus) {
    const trigger = (inputs.get('trigger') as number) ?? 0;
    const peak = params.get('peak') ?? 1.0;
    const decayTau = params.get('decayTau') ?? 0.18;
    const threshold = params.get('threshold') ?? 0.0;

    let value = (state?.value as number) ?? 0;

    // Trigger: if input exceeds current value AND threshold, jump to peak
    if (trigger > value && trigger > threshold) {
      value = Math.min(trigger, peak);
    }

    // Exponential decay
    value *= Math.exp(-dt / Math.max(decayTau, 0.001));

    return {
      outputs: new Map([['out', value]]),
      nextState: { value },
    };
  },
};
