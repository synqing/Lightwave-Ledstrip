/**
 * Mach Diamonds (Fixed) — node graph preset
 *
 * Decomposes the REWRITTEN LGPMachDiamondsAREffect into its audio-visual
 * signal chain. This is the correct version with:
 *   - Single-stage EMA smoothing (not double-smoothed)
 *   - Max follower normalisation (dynamic gain)
 *   - Audio drives brightness directly (not simulation coefficients)
 *   - Squared response for perceptual punch
 *   - No brightness floor — silence = dark
 */

import type { GraphPreset } from './index';

export const machDiamondsFixed: GraphPreset = {
  name: 'Mach Diamonds (Fixed)',
  description:
    'Rewritten shock-diamond effect: single-stage EMA, max follower normalisation, ' +
    'squared brightness response, silence = dark. Direct audio-to-brightness chain.',
  category: 'Demo',

  nodes: [
    // =====================================================================
    // Column 0 — Sources (x=0)
    // =====================================================================
    {
      id: 'bass-band',
      definitionType: 'band-source',
      parameters: { index: 0 },
      position: { x: 0, y: 0 },
    },
    {
      id: 'treble-band',
      definitionType: 'band-source',
      parameters: { index: 5 },
      position: { x: 0, y: 120 },
    },
    {
      id: 'beat-strength',
      definitionType: 'beat-strength-source',
      parameters: {},
      position: { x: 0, y: 240 },
    },
    {
      id: 'silent-scale',
      definitionType: 'silent-scale-source',
      parameters: {},
      position: { x: 0, y: 360 },
    },

    // =====================================================================
    // Column 1 — Single-stage EMA smoothing (x=300)
    // =====================================================================
    {
      id: 'ema-bass',
      definitionType: 'ema-smooth',
      parameters: { tau: 0.05 },
      position: { x: 300, y: 0 },
    },
    {
      id: 'ema-treble',
      definitionType: 'ema-smooth',
      parameters: { tau: 0.04 },
      position: { x: 300, y: 120 },
    },

    // =====================================================================
    // Column 2 — Max follower normalisation (x=600)
    // Asymmetric: fast attack (58ms), slow decay (500ms), floor 0.04
    // This is what the original was MISSING — dynamic gain control
    // =====================================================================
    {
      id: 'follower-bass',
      definitionType: 'max-follower',
      parameters: { attackTau: 0.058, decayTau: 0.50, floor: 0.04 },
      position: { x: 600, y: 0 },
    },
    {
      id: 'follower-treble',
      definitionType: 'max-follower',
      parameters: { attackTau: 0.058, decayTau: 0.50, floor: 0.04 },
      position: { x: 600, y: 120 },
    },

    // =====================================================================
    // Column 3 — Geometry (x=900)
    // Triangular wave: spacing and sharpness set to midpoint of C++ ranges.
    // In firmware these are driven dynamically by normBass/normTreble;
    // in the node graph they are fixed parameters for clarity.
    // =====================================================================
    {
      id: 'tri-wave',
      definitionType: 'triangular-wave',
      parameters: { spacing: 0.12, driftSpeed: 1.0, sharpness: 3.0 },
      position: { x: 900, y: 60 },
    },

    // =====================================================================
    // Column 4 — Audio x Geometry multiplication (x=1200)
    // normBass * geometry array = brightness pattern
    // =====================================================================
    {
      id: 'mul-bass-geom',
      definitionType: 'multiply',
      parameters: {},
      position: { x: 1200, y: 30 },
    },

    // =====================================================================
    // Column 5 — Beat modulation (x=1500)
    // beatMod = 0.3 + 0.7 * beatStrength
    //   Scale beatStrength by 0.7, then Add 0.3
    // =====================================================================
    {
      id: 'beat-scale',
      definitionType: 'scale',
      parameters: { factor: 0.7 },
      position: { x: 1500, y: 240 },
    },
    {
      id: 'beat-offset',
      definitionType: 'add',
      parameters: {},
      position: { x: 1500, y: 340 },
    },
    // Constant 0.3 — use a Scale node with factor=0.3 on silentScale
    // (which is 1.0 when active). Alternatively, for a true constant,
    // we chain: silentScale → scale(0.3) as the bias source.
    // A cleaner approach: use a second Scale node on beat-strength
    // and add a fixed bias via a Scale on silent-scale.
    // Actually, the simplest: feed beat-offset input 'a' from beat-scale
    // and input 'b' from a constant. We model the constant 0.3 as a
    // Scale(factor=0.3) applied to silentScale (which is 1.0 when audio active).
    {
      id: 'const-0p3',
      definitionType: 'scale',
      parameters: { factor: 0.3 },
      position: { x: 1350, y: 380 },
    },

    // =====================================================================
    // Column 6 — Multiply brightness by beatMod (x=1800)
    // =====================================================================
    {
      id: 'mul-beat',
      definitionType: 'multiply',
      parameters: {},
      position: { x: 1800, y: 100 },
    },

    // =====================================================================
    // Column 7 — Multiply by silentScale (x=2100)
    // Silence gate: when silent, everything fades to zero
    // =====================================================================
    {
      id: 'mul-silence',
      definitionType: 'multiply',
      parameters: {},
      position: { x: 2100, y: 100 },
    },

    // =====================================================================
    // Column 8 — Squared response (x=2400)
    // brightness *= brightness (perceptual punch, matches BloomRef v*=v)
    // This is what the original was MISSING
    // =====================================================================
    {
      id: 'power-sq',
      definitionType: 'power',
      parameters: { exponent: 2.0 },
      position: { x: 2400, y: 100 },
    },

    // =====================================================================
    // Column 9 — Colour composition (x=2700)
    // HSV->RGB with chroma-driven hue and spatial hue spread
    // =====================================================================
    {
      id: 'hsv-rgb',
      definitionType: 'hsv-to-rgb',
      parameters: { hueOffset: 0, hueSpread: 48 },
      position: { x: 2700, y: 60 },
    },

    // =====================================================================
    // Column 10 — Output (x=3000)
    // Centre-origin mirroring + trail persistence
    // fadeAmount ~30 (midpoint of 15-60 dynamic range in C++)
    // =====================================================================
    {
      id: 'led-out',
      definitionType: 'led-output',
      parameters: { fadeAmount: 30 },
      position: { x: 3000, y: 60 },
    },
  ],

  edges: [
    // Sources → EMA smoothing
    {
      id: 'e1',
      from: { nodeId: 'bass-band', portName: 'out' },
      to: { nodeId: 'ema-bass', portName: 'value' },
    },
    {
      id: 'e2',
      from: { nodeId: 'treble-band', portName: 'out' },
      to: { nodeId: 'ema-treble', portName: 'value' },
    },

    // EMA → Max followers
    {
      id: 'e3',
      from: { nodeId: 'ema-bass', portName: 'out' },
      to: { nodeId: 'follower-bass', portName: 'value' },
    },
    {
      id: 'e4',
      from: { nodeId: 'ema-treble', portName: 'out' },
      to: { nodeId: 'follower-treble', portName: 'value' },
    },

    // normBass → triangular wave amplitude
    {
      id: 'e5',
      from: { nodeId: 'follower-bass', portName: 'normalised' },
      to: { nodeId: 'tri-wave', portName: 'amplitude' },
    },

    // normBass × geometry
    {
      id: 'e6',
      from: { nodeId: 'follower-bass', portName: 'normalised' },
      to: { nodeId: 'mul-bass-geom', portName: 'a' },
    },
    {
      id: 'e7',
      from: { nodeId: 'tri-wave', portName: 'out' },
      to: { nodeId: 'mul-bass-geom', portName: 'b' },
    },

    // Beat modulation: beatStrength → scale(0.7) → add(0.3)
    {
      id: 'e8',
      from: { nodeId: 'beat-strength', portName: 'out' },
      to: { nodeId: 'beat-scale', portName: 'value' },
    },
    {
      id: 'e9',
      from: { nodeId: 'beat-scale', portName: 'out' },
      to: { nodeId: 'beat-offset', portName: 'a' },
    },
    // Constant 0.3 bias (silentScale=1.0 when active, scaled by 0.3)
    {
      id: 'e10',
      from: { nodeId: 'silent-scale', portName: 'out' },
      to: { nodeId: 'const-0p3', portName: 'value' },
    },
    {
      id: 'e11',
      from: { nodeId: 'const-0p3', portName: 'out' },
      to: { nodeId: 'beat-offset', portName: 'b' },
    },

    // brightness × beatMod
    {
      id: 'e12',
      from: { nodeId: 'mul-bass-geom', portName: 'out' },
      to: { nodeId: 'mul-beat', portName: 'a' },
    },
    {
      id: 'e13',
      from: { nodeId: 'beat-offset', portName: 'out' },
      to: { nodeId: 'mul-beat', portName: 'b' },
    },

    // × silentScale
    {
      id: 'e14',
      from: { nodeId: 'mul-beat', portName: 'out' },
      to: { nodeId: 'mul-silence', portName: 'a' },
    },
    {
      id: 'e15',
      from: { nodeId: 'silent-scale', portName: 'out' },
      to: { nodeId: 'mul-silence', portName: 'b' },
    },

    // Power (squared response)
    {
      id: 'e16',
      from: { nodeId: 'mul-silence', portName: 'out' },
      to: { nodeId: 'power-sq', portName: 'value' },
    },

    // HSV→RGB (value array from power, hue from offset param)
    {
      id: 'e17',
      from: { nodeId: 'power-sq', portName: 'out' },
      to: { nodeId: 'hsv-rgb', portName: 'value' },
    },

    // LED Output
    {
      id: 'e18',
      from: { nodeId: 'hsv-rgb', portName: 'out' },
      to: { nodeId: 'led-out', portName: 'colour' },
    },
  ],
};
