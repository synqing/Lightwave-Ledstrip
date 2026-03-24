import { PortType, type NodeDefinition, type PortValue } from '../../engine/types';

const HALF_STRIP = 160;

export const scaleNode: NodeDefinition = {
  type: 'scale',
  label: 'Scale',
  layer: 'processing',
  inputs: [
    { name: 'value', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
  ],
  outputs: [
    { name: 'out', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
  ],
  parameters: [
    { name: 'factor', min: 0, max: 10, step: 0.01, defaultValue: 1.0, label: 'Factor' },
  ],
  hasState: false,
  cppTemplate: 'node-scale',

  evaluate(inputs, params, _state, _dt, _controlBus) {
    const value = inputs.get('value');
    const factor = params.get('factor') ?? 1.0;

    const out = new Float32Array(HALF_STRIP);

    if (value instanceof Float32Array) {
      for (let i = 0; i < value.length; i++) {
        out[i] = (value[i] ?? 0) * factor;
      }
    } else {
      out.fill(((value as number) ?? 0) * factor);
    }

    return {
      outputs: new Map<string, PortValue>([['out', out]]),
      nextState: null,
    };
  },
};
