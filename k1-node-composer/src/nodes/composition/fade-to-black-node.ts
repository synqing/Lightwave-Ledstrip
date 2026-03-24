import { PortType, type NodeDefinition } from '../../engine/types';

const HALF_STRIP = 160;
const RGB_SIZE = HALF_STRIP * 3;

/**
 * Fade toward black — reduces each channel by a fixed amount per frame.
 * Matches FastLED's fadeToBlackBy: scale = (255 - amount) / 255.
 */
export const fadeToBlackNode: NodeDefinition = {
  type: 'fade-to-black',
  label: 'Fade To Black',
  layer: 'composition',
  inputs: [
    { name: 'colour', type: PortType.CRGB_160, defaultValue: new Uint8Array(RGB_SIZE) },
  ],
  outputs: [
    { name: 'out', type: PortType.CRGB_160, defaultValue: new Uint8Array(RGB_SIZE) },
  ],
  parameters: [
    { name: 'amount', min: 0, max: 60, step: 1, defaultValue: 20, label: 'Fade Amount' },
  ],
  hasState: false,
  cppTemplate: 'node-fade-to-black',

  evaluate(inputs, params, _state, _dt, _controlBus) {
    const colour = inputs.get('colour');
    const amount = params.get('amount') ?? 20;
    const scale = (255 - amount) / 255;

    const out = new Uint8Array(RGB_SIZE);
    const arr = colour instanceof Uint8Array ? colour : new Uint8Array(RGB_SIZE);

    for (let i = 0; i < RGB_SIZE; i++) {
      out[i] = Math.round((arr[i] ?? 0) * scale);
    }

    return {
      outputs: new Map([['out', out]]),
      nextState: null,
    };
  },
};
