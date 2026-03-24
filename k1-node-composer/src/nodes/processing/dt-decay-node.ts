import { PortType, type NodeDefinition, type NodeState } from '../../engine/types';

export const dtDecayNode: NodeDefinition = {
  type: 'dt-decay',
  label: 'dt-Decay',
  layer: 'processing',
  inputs: [
    { name: 'value', type: PortType.SCALAR, defaultValue: 0 },
  ],
  outputs: [
    { name: 'out', type: PortType.SCALAR, defaultValue: 0 },
  ],
  parameters: [
    { name: 'rate60fps', min: 0.5, max: 0.999, step: 0.001, defaultValue: 0.95, label: 'Rate @60fps' },
  ],
  hasState: true,
  cppTemplate: 'node-dt-decay',

  getInitialState(): NodeState {
    return { value: 0 };
  },

  evaluate(inputs, params, state, dt, _controlBus) {
    const input = inputs.get('value') as number ?? 0;
    const rate = params.get('rate60fps') ?? 0.95;
    const prev = (state?.value as number) ?? 0;

    // Frame-rate independent decay: matches firmware chroma::dtDecay()
    const next = prev * Math.pow(rate, dt * 60.0) + input;

    return {
      outputs: new Map([['out', next]]),
      nextState: { value: next },
    };
  },
};
