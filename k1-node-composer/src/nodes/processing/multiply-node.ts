import { PortType, type NodeDefinition, type PortValue } from '../../engine/types';

const HALF_STRIP = 160;

export const multiplyNode: NodeDefinition = {
  type: 'multiply',
  label: 'Multiply',
  layer: 'processing',
  inputs: [
    { name: 'a', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
    { name: 'b', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
  ],
  outputs: [
    { name: 'out', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-multiply',

  evaluate(inputs, _params, _state, _dt, _controlBus) {
    const a = inputs.get('a');
    const b = inputs.get('b');

    const out = new Float32Array(HALF_STRIP);

    // Both inputs accept SCALAR (broadcast to 160) or ARRAY_160
    const aIsArr = a instanceof Float32Array;
    const bIsArr = b instanceof Float32Array;
    const aScalar = typeof a === 'number' ? a : 0;
    const bScalar = typeof b === 'number' ? b : 0;

    for (let i = 0; i < HALF_STRIP; i++) {
      const av = aIsArr ? (a[i] ?? 0) : aScalar;
      const bv = bIsArr ? (b[i] ?? 0) : bScalar;
      out[i] = av * bv;
    }

    return {
      outputs: new Map<string, PortValue>([['out', out]]),
      nextState: null,
    };
  },
};
