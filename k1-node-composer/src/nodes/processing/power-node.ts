import { PortType, type NodeDefinition, type PortValue } from '../../engine/types';

const HALF_STRIP = 160;

export const powerNode: NodeDefinition = {
  type: 'power',
  label: 'Power',
  layer: 'processing',
  inputs: [
    { name: 'value', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
  ],
  outputs: [
    { name: 'out', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
  ],
  parameters: [
    { name: 'exponent', min: 0.1, max: 5.0, step: 0.1, defaultValue: 2.0, label: 'Exponent' },
  ],
  hasState: false,
  cppTemplate: 'node-power',

  evaluate(inputs, params, _state, _dt, _controlBus) {
    const value = inputs.get('value');
    const exponent = params.get('exponent') ?? 2.0;

    const out = new Float32Array(HALF_STRIP);

    if (value instanceof Float32Array) {
      for (let i = 0; i < HALF_STRIP; i++) {
        out[i] = Math.pow(Math.abs(value[i] ?? 0), exponent);
      }
    } else {
      const v = Math.pow(Math.abs((value as number) ?? 0), exponent);
      out.fill(v);
    }

    return {
      outputs: new Map<string, PortValue>([['out', out]]),
      nextState: null,
    };
  },
};
