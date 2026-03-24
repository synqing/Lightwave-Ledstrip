import { PortType, type NodeDefinition } from '../../engine/types';

const HALF_STRIP = 160;

export const exponentialDecayNode: NodeDefinition = {
  type: 'exponential-decay',
  label: 'Exponential Decay',
  layer: 'geometry',
  inputs: [
    { name: 'amplitude', type: PortType.SCALAR, defaultValue: 1.0 },
  ],
  outputs: [
    { name: 'out', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
  ],
  parameters: [
    { name: 'rate', min: 0.5, max: 10, step: 0.1, defaultValue: 3, label: 'Decay Rate' },
  ],
  hasState: false,
  cppTemplate: 'node-exponential-decay',

  evaluate(inputs, params, _state, _dt, _controlBus) {
    const amplitude = (inputs.get('amplitude') as number) ?? 1.0;
    const rate = params.get('rate') ?? 3;

    const out = new Float32Array(HALF_STRIP);
    for (let i = 0; i < HALF_STRIP; i++) {
      const progress = i / (HALF_STRIP - 1);
      out[i] = amplitude * Math.exp(-rate * progress);
    }

    return {
      outputs: new Map([['out', out]]),
      nextState: null,
    };
  },
};
