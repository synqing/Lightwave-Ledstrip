import { PortType, type NodeDefinition } from '../../engine/types';

const HALF_STRIP = 160;

export const gaussianNode: NodeDefinition = {
  type: 'gaussian',
  label: 'Gaussian',
  layer: 'geometry',
  inputs: [
    { name: 'amplitude', type: PortType.SCALAR, defaultValue: 1.0 },
  ],
  outputs: [
    { name: 'out', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
  ],
  parameters: [
    { name: 'sigma', min: 0.01, max: 1.0, step: 0.01, defaultValue: 0.25, label: 'Sigma (width)' },
    { name: 'centre', min: 0.0, max: 1.0, step: 0.01, defaultValue: 0.0, label: 'Peak Shift (0=centre, 1=edge)' },
  ],
  hasState: false,
  cppTemplate: 'node-gaussian',

  evaluate(inputs, params, _state, _dt, _controlBus) {
    const amplitude = (inputs.get('amplitude') as number) ?? 1.0;
    const sigma = params.get('sigma') ?? 0.25;
    const centre = params.get('centre') ?? 0.0;
    const sigma2 = 2.0 * sigma * sigma;

    const out = new Float32Array(HALF_STRIP);
    for (let i = 0; i < HALF_STRIP; i++) {
      const dist = i / (HALF_STRIP - 1) - centre;
      out[i] = amplitude * Math.exp(-(dist * dist) / Math.max(sigma2, 0.0001));
    }

    return {
      outputs: new Map([['out', out]]),
      nextState: null,
    };
  },
};
