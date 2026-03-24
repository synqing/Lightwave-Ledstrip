import { PortType, type NodeDefinition } from '../../engine/types';

export const tempoConfidenceNode: NodeDefinition = {
  type: 'tempo-confidence-source',
  label: 'Tempo Confidence',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-tempo-confidence',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const value = controlBus?.tempoConfidence ?? 0;
    return {
      outputs: new Map([['out', value]]),
      nextState: null,
    };
  },
};
