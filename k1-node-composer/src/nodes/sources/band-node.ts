import { PortType, type NodeDefinition } from '../../engine/types';

export const bandNode: NodeDefinition = {
  type: 'band-source',
  label: 'Band',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [
    { name: 'index', min: 0, max: 7, step: 1, defaultValue: 0, label: 'Band Index (0=sub-bass, 7=air)' },
  ],
  hasState: false,
  cppTemplate: 'node-band',

  evaluate(_inputs, params, _state, _dt, controlBus) {
    const idx = Math.round(params.get('index') ?? 0);
    const value = controlBus?.bands[idx] ?? 0;
    return {
      outputs: new Map([['out', value]]),
      nextState: null,
    };
  },
};
