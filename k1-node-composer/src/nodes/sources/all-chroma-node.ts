/**
 * All Chroma source — outputs all 12 pitch class energies as ARRAY_12.
 * Essential for chroma-driven colour mapping where effects need the
 * full pitch profile, not just a single bin.
 */

import { PortType, type NodeDefinition } from '../../engine/types';

export const allChromaNode: NodeDefinition = {
  type: 'all-chroma-source',
  label: 'All Chroma',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.ARRAY_12, defaultValue: new Float32Array(12) },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-all-chroma',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const chroma = controlBus?.chroma ?? new Float32Array(12);
    return {
      outputs: new Map([['out', chroma]]),
      nextState: null,
    };
  },
};
