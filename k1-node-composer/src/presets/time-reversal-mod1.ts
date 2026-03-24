/**
 * LGP Time-Reversal Mirror Mod1 — preset for the K1 Node Composer.
 *
 * 1D damped wave simulation with periodic centre impulses driven by audio.
 * Uses Frame Block node for the wave physics (runs once per frame with
 * full array access for neighbour-dependent Verlet integration).
 */

import type { GraphPreset } from './index';

export const timeReversalMod1Preset: GraphPreset = {
  name: 'Time-Reversal Mirror Mod1',
  description: 'Damped wave simulation with audio-driven centre impulses. Forward propagation + reverse playback.',
  category: 'Effects',
  nodes: [
    { id: 'tr-rms', definitionType: 'rms-source', parameters: {}, position: { x: 0, y: 0 } },
    { id: 'tr-beat', definitionType: 'beat-strength-source', parameters: {}, position: { x: 0, y: 120 } },
    { id: 'tr-silent', definitionType: 'silent-scale-source', parameters: {}, position: { x: 0, y: 240 } },
    { id: 'tr-smooth', definitionType: 'ema-smooth', parameters: { tau: 0.05 }, position: { x: 250, y: 0 } },
    { id: 'tr-follower', definitionType: 'max-follower', parameters: { attackTau: 0.058, decayTau: 0.5, floor: 0.04 }, position: { x: 500, y: 0 } },
    { id: 'tr-env', definitionType: 'envelope', parameters: { peak: 0.8, decayTau: 0.22, threshold: 0.1 }, position: { x: 250, y: 120 } },
    { id: 'tr-add', definitionType: 'add', parameters: {}, position: { x: 500, y: 60 } },
    { id: 'tr-wave', definitionType: 'frame-block', parameters: {}, position: { x: 750, y: 0 } },
    { id: 'tr-mul-sil', definitionType: 'multiply', parameters: {}, position: { x: 1000, y: 0 } },
    { id: 'tr-palette', definitionType: 'palette-lookup', parameters: { indexOffset: 0, indexSpread: 36 }, position: { x: 1250, y: 0 } },
    { id: 'tr-out', definitionType: 'led-output', parameters: { fadeAmount: 15 }, position: { x: 1500, y: 0 } },
  ],
  edges: [
    { id: 'tre1', from: { nodeId: 'tr-rms', portName: 'out' }, to: { nodeId: 'tr-smooth', portName: 'value' } },
    { id: 'tre2', from: { nodeId: 'tr-smooth', portName: 'out' }, to: { nodeId: 'tr-follower', portName: 'value' } },
    { id: 'tre3', from: { nodeId: 'tr-beat', portName: 'out' }, to: { nodeId: 'tr-env', portName: 'trigger' } },
    { id: 'tre4', from: { nodeId: 'tr-follower', portName: 'normalised' }, to: { nodeId: 'tr-add', portName: 'a' } },
    { id: 'tre5', from: { nodeId: 'tr-env', portName: 'out' }, to: { nodeId: 'tr-add', portName: 'b' } },
    { id: 'tre6', from: { nodeId: 'tr-add', portName: 'out' }, to: { nodeId: 'tr-wave', portName: 'a' } },
    { id: 'tre7', from: { nodeId: 'tr-wave', portName: 'out' }, to: { nodeId: 'tr-mul-sil', portName: 'a' } },
    { id: 'tre8', from: { nodeId: 'tr-silent', portName: 'out' }, to: { nodeId: 'tr-mul-sil', portName: 'b' } },
    { id: 'tre9', from: { nodeId: 'tr-mul-sil', portName: 'out' }, to: { nodeId: 'tr-palette', portName: 'brightness' } },
    { id: 'tre10', from: { nodeId: 'tr-palette', portName: 'out' }, to: { nodeId: 'tr-out', portName: 'colour' } },
  ],
};
