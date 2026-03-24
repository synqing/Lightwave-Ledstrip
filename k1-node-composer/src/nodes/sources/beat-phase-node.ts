import { PortType, type NodeDefinition } from '../../engine/types';

/** Beat phase ramps 0 to 1 over each beat period. */
export const beatPhaseNode: NodeDefinition = {
  type: 'beat-phase-source',
  label: 'Beat Phase',
  layer: 'source',
  inputs: [],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [],
  hasState: false,
  cppTemplate: 'node-beat-phase',

  evaluate(_inputs, _params, _state, _dt, controlBus) {
    const value = controlBus?.beatPhase ?? 0;
    return {
      outputs: new Map([['out', value]]),
      nextState: null,
    };
  },
};
