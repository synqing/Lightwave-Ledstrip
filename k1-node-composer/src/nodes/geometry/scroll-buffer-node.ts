import { PortType, type NodeDefinition, type NodeState } from '../../engine/types';

const HALF_STRIP = 160;

export const scrollBufferNode: NodeDefinition = {
  type: 'scroll-buffer',
  label: 'Scroll Buffer',
  layer: 'geometry',
  inputs: [
    { name: 'value', type: PortType.SCALAR, defaultValue: 0 },
  ],
  outputs: [
    { name: 'out', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
  ],
  parameters: [
    { name: 'rate', min: 1, max: 60, step: 1, defaultValue: 10, label: 'Scroll Rate (px/s)' },
  ],
  hasState: true,
  cppTemplate: 'node-scroll-buffer',

  getInitialState(): NodeState {
    return {
      buffer: new Float32Array(HALF_STRIP),
      fractional: 0,
    };
  },

  evaluate(inputs, params, state, dt, _controlBus) {
    const value = (inputs.get('value') as number) ?? 0;
    const rate = params.get('rate') ?? 10;

    // Retrieve or initialise the buffer
    let buffer: Float32Array;
    if (state?.buffer instanceof Float32Array && state.buffer.length === HALF_STRIP) {
      buffer = new Float32Array(state.buffer);
    } else {
      buffer = new Float32Array(HALF_STRIP);
    }

    // Accumulate fractional pixel shift
    let fractional = (state?.fractional as number) ?? 0;
    fractional += rate * dt;

    // Shift outward by whole pixels
    const shift = Math.floor(fractional);
    fractional -= shift;

    if (shift > 0) {
      // Move existing values toward the edge
      const clampedShift = Math.min(shift, HALF_STRIP);
      for (let i = HALF_STRIP - 1; i >= clampedShift; i--) {
        buffer[i] = buffer[i - clampedShift] ?? 0;
      }
      // Fill newly exposed centre positions with the input value
      for (let i = 0; i < clampedShift && i < HALF_STRIP; i++) {
        buffer[i] = value;
      }
    }

    // Copy buffer to output (separate array to avoid aliasing)
    const out = new Float32Array(buffer);

    return {
      outputs: new Map([['out', out]]),
      nextState: { buffer, fractional },
    };
  },
};
