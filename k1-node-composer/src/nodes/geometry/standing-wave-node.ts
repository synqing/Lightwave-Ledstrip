import { PortType, type NodeDefinition, type NodeState } from '../../engine/types';

const HALF_STRIP = 160;

export const standingWaveNode: NodeDefinition = {
  type: 'standing-wave',
  label: 'Standing Wave',
  layer: 'geometry',
  inputs: [
    { name: 'amplitude', type: PortType.SCALAR, defaultValue: 1.0 },
  ],
  outputs: [
    { name: 'out', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
  ],
  parameters: [
    { name: 'mode_n', min: 2, max: 8, step: 1, defaultValue: 3, label: 'Mode N (harmonics)' },
    { name: 'contrast', min: 1, max: 5, step: 0.1, defaultValue: 2, label: 'Contrast (exponent)' },
    { name: 'driftSpeed', min: 0.0, max: 5.0, step: 0.1, defaultValue: 1.0, label: 'Drift Speed' },
  ],
  hasState: true,
  cppTemplate: 'node-standing-wave',

  getInitialState(): NodeState {
    return { phase: 0 };
  },

  evaluate(inputs, params, state, dt, _controlBus) {
    const amplitude = (inputs.get('amplitude') as number) ?? 1.0;
    const mode_n = params.get('mode_n') ?? 3;
    const contrast = params.get('contrast') ?? 2;
    const driftSpeed = params.get('driftSpeed') ?? 1.0;

    let phase = (state?.phase as number) ?? 0;
    phase += driftSpeed * dt;

    const out = new Float32Array(HALF_STRIP);
    for (let i = 0; i < HALF_STRIP; i++) {
      const progress = i / (HALF_STRIP - 1);
      const raw = Math.abs(Math.sin(mode_n * Math.PI * progress + phase));
      out[i] = amplitude * Math.pow(raw, contrast);
    }

    return {
      outputs: new Map([['out', out]]),
      nextState: { phase },
    };
  },
};
