import { PortType, type NodeDefinition } from '../../engine/types';

export const silentScaleNode: NodeDefinition = {
  type: 'silent-scale-source',
  label: 'Silent Scale',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-silent-scale',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const value = controlBus?.silentScale ?? 0;
    return {
      outputs: new Map([['out', value]]),
      nextState: null,
    };
  },
};
