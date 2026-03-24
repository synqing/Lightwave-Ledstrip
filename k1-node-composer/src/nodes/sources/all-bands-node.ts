/**
 * All Bands source — outputs all 8 octave bands as ARRAY_8.
 * Allows chroma/band-driven effects to wire entire frequency
 * profiles through a single connection.
 */

import { PortType, type NodeDefinition } from '../../engine/types';

export const allBandsNode: NodeDefinition = {
  type: 'all-bands-source',
  label: 'All Bands',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.ARRAY_8, defaultValue: new Float32Array(8) },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-all-bands',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const bands = controlBus?.bands ?? new Float32Array(8);
    return {
      outputs: new Map([['out', bands]]),
      nextState: null,
    };
  },
};
