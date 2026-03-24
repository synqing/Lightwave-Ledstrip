import { PortType, type NodeDefinition } from '../../engine/types';

const HALF_STRIP = 160;

/**
 * Centre melt — Gaussian centred at LED 79/80 with no offset.
 * Mirrors the firmware pattern: melt = exp(-(dmid*dmid) * 0.0018f)
 * where dmid is the distance from centre in raw pixel indices.
 */
export const centreMeltNode: NodeDefinition = {
  type: 'centre-melt',
  label: 'Centre Melt',
  layer: 'geometry',
  inputs: [
    { name: 'amplitude', type: PortType.SCALAR, defaultValue: 1.0 },
  ],
  outputs: [
    { name: 'out', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
  ],
  parameters: [
    { name: 'decay', min: 0.0005, max: 0.01, step: 0.0001, defaultValue: 0.002, label: 'Decay Factor' },
  ],
  hasState: false,
  cppTemplate: 'node-centre-melt',

  evaluate(inputs, params, _state, _dt, _controlBus) {
    const amplitude = (inputs.get('amplitude') as number) ?? 1.0;
    const decay = params.get('decay') ?? 0.002;

    const out = new Float32Array(HALF_STRIP);
    for (let i = 0; i < HALF_STRIP; i++) {
      // i=0 is centre (LED 79/80), i=159 is the edge
      out[i] = amplitude * Math.exp(-(i * i) * decay);
    }

    return {
      outputs: new Map([['out', out]]),
      nextState: null,
    };
  },
};
