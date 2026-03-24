import { PortType, type NodeDefinition } from '../../engine/types';

const HALF_STRIP = 160;

export const linearGradientNode: NodeDefinition = {
  type: 'linear-gradient',
  label: 'Linear Gradient',
  layer: 'geometry',
  inputs: [
    { name: 'amplitude', type: PortType.SCALAR, defaultValue: 1.0 },
  ],
  outputs: [
    { name: 'out', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-linear-gradient',

  evaluate(inputs, _params, _state, _dt, _controlBus) {
    const amplitude = (inputs.get('amplitude') as number) ?? 1.0;

    const out = new Float32Array(HALF_STRIP);
    for (let i = 0; i < HALF_STRIP; i++) {
      // Simple ramp: 0 at centre → 1 at edge, scaled by amplitude
      out[i] = amplitude * (i / 159);
    }

    return {
      outputs: new Map([['out', out]]),
      nextState: null,
    };
  },
};
