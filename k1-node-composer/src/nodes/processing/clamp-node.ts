import { PortType, type NodeDefinition, type PortValue } from '../../engine/types';

export const clampNode: NodeDefinition = {
  type: 'clamp',
  label: 'Clamp',
  layer: 'processing',
  inputs: [
    { name: 'value', type: PortType.ARRAY_160, defaultValue: new Float32Array(160) },
  ],
  outputs: [
    { name: 'out', type: PortType.ARRAY_160, defaultValue: new Float32Array(160) },
  ],
  parameters: [
    { name: 'min', min: 0, max: 1, step: 0.01, defaultValue: 0, label: 'Min' },
    { name: 'max', min: 0, max: 1, step: 0.01, defaultValue: 1, label: 'Max' },
  ],
  hasState: false,
  cppTemplate: 'node-clamp',

  evaluate(inputs, params, _state, _dt, _controlBus) {
    const value = inputs.get('value');
    const lo = params.get('min') ?? 0;
    const hi = params.get('max') ?? 1;

    let out: PortValue;
    if (value instanceof Float32Array) {
      // ARRAY_160 path — clamp each element
      const arr = new Float32Array(value.length);
      for (let i = 0; i < value.length; i++) {
        arr[i] = Math.min(hi, Math.max(lo, value[i] ?? 0));
      }
      out = arr;
    } else {
      // SCALAR path
      out = Math.min(hi, Math.max(lo, (value as number) ?? 0));
    }

    return {
      outputs: new Map([['out', out]]),
      nextState: null,
    };
  },
};
