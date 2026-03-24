import { PortType, type NodeDefinition, type NodeState } from '../../engine/types';

const HALF_STRIP = 160;

export const sinusoidalSumNode: NodeDefinition = {
  type: 'sinusoidal-sum',
  label: 'Sinusoidal Sum',
  layer: 'geometry',
  inputs: [
    { name: 'amplitude', type: PortType.SCALAR, defaultValue: 1.0 },
  ],
  outputs: [
    { name: 'out', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
  ],
  parameters: [
    { name: 'f1', min: 0.02, max: 0.2, step: 0.005, defaultValue: 0.06, label: 'Frequency 1' },
    { name: 'f2', min: 0.05, max: 0.3, step: 0.005, defaultValue: 0.145, label: 'Frequency 2' },
    { name: 'f3', min: 0.1, max: 0.5, step: 0.005, defaultValue: 0.31, label: 'Frequency 3' },
    { name: 'driftSpeed', min: 0.0, max: 5.0, step: 0.1, defaultValue: 1.0, label: 'Drift Speed' },
  ],
  hasState: true,
  cppTemplate: 'node-sinusoidal-sum',

  getInitialState(): NodeState {
    return { phase: 0 };
  },

  evaluate(inputs, params, state, dt, _controlBus) {
    const amplitude = (inputs.get('amplitude') as number) ?? 1.0;
    const f1 = params.get('f1') ?? 0.06;
    const f2 = params.get('f2') ?? 0.145;
    const f3 = params.get('f3') ?? 0.31;
    const driftSpeed = params.get('driftSpeed') ?? 1.0;

    let phase = (state?.phase as number) ?? 0;
    phase += driftSpeed * dt;

    const out = new Float32Array(HALF_STRIP);
    for (let i = 0; i < HALF_STRIP; i++) {
      const x = i;
      // Three sine waves with different frequency ratios and phase offsets
      const sum =
        Math.sin(f1 * x + phase) +
        0.7 * Math.sin(f2 * x - phase * 1.2) +
        0.3 * Math.sin(f3 * x + phase * 2.1);
      out[i] = amplitude * (sum / 2.0);
    }

    return {
      outputs: new Map([['out', out]]),
      nextState: { phase },
    };
  },
};
