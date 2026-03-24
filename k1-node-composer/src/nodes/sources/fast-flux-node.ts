import { PortType, type NodeDefinition } from '../../engine/types';

export const fastFluxNode: NodeDefinition = {
  type: 'fast-flux-source',
  label: 'Fast Flux',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-fast-flux',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const value = controlBus?.fastFlux ?? 0;
    return {
      outputs: new Map([['out', value]]),
      nextState: null,
    };
  },
};
