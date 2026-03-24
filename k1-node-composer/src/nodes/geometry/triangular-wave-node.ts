import { PortType, type NodeDefinition, type NodeState } from '../../engine/types';

const HALF_STRIP = 160;

function fract(x: number): number { return x - Math.floor(x); }
function tri01(x: number): number {
  const f = fract(x);
  return Math.max(0, Math.min(1, 1.0 - Math.abs(2.0 * f - 1.0)));
}

export const triangularWaveNode: NodeDefinition = {
  type: 'triangular-wave',
  label: 'Triangular Wave',
  layer: 'geometry',
  inputs: [
    { name: 'amplitude', type: PortType.SCALAR, defaultValue: 1.0 },
  ],
  outputs: [
    { name: 'out', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
  ],
  parameters: [
    { name: 'spacing', min: 0.02, max: 0.5, step: 0.01, defaultValue: 0.12, label: 'Cell Spacing' },
    { name: 'driftSpeed', min: 0.0, max: 5.0, step: 0.1, defaultValue: 1.0, label: 'Drift Speed' },
    { name: 'sharpness', min: 1.0, max: 5.0, step: 0.1, defaultValue: 2.0, label: 'Sharpness (exponent)' },
  ],
  hasState: true,
  cppTemplate: 'node-triangular-wave',

  getInitialState(): NodeState {
    return { phase: 0 };
  },

  evaluate(inputs, params, state, dt, _controlBus) {
    const amplitude = (inputs.get('amplitude') as number) ?? 1.0;
    const spacing = params.get('spacing') ?? 0.12;
    const driftSpeed = params.get('driftSpeed') ?? 1.0;
    const sharpness = params.get('sharpness') ?? 2.0;

    let phase = (state?.phase as number) ?? 0;
    phase += driftSpeed * dt;

    const out = new Float32Array(HALF_STRIP);
    for (let i = 0; i < HALF_STRIP; i++) {
      const progress = i / (HALF_STRIP - 1);
      const cellPhase = progress / Math.max(spacing, 0.01) - phase;
      const tri = tri01(cellPhase);
      out[i] = amplitude * Math.pow(tri, sharpness);
    }

    return {
      outputs: new Map([['out', out]]),
      nextState: { phase },
    };
  },
};
