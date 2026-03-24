import { PortType, type NodeDefinition } from '../../engine/types';

export const fluxNode: NodeDefinition = {
  type: 'flux-source',
  label: 'Flux',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-flux',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const value = controlBus?.flux ?? 0;
    return {
      outputs: new Map([['out', value]]),
      nextState: null,
    };
  },
};
