/**
 * Mach Diamonds (Original — Broken) — node graph preset
 *
 * Decomposes the ORIGINAL broken MachDiamonds signal chain to show
 * exactly where it diverges from the fixed version:
 *
 *   1. DOUBLE smoothing: updateSignals layer (tau=0.20) + effect smoothTo (tau=0.15)
 *      — the audio signal is sluggish, bass peaks are attenuated by ~70%
 *   2. NO max follower: raw smoothed value used as coefficient, not normalised
 *      — quiet content produces tiny coefficients, loud content clips
 *   3. Audio drives simulation COEFFICIENTS, not brightness directly
 *      — the geometry runs at a fixed brightness floor, audio only modulates shape
 *   4. Fixed brightness floor (0.25 base + 0.25 * audio)
 *      — silence is NOT dark, minimum 25% brightness always visible
 *   5. NO squared response: linear brightness mapping
 *      — no perceptual punch, flat visual response
 *
 * When loaded alongside the Fixed preset, the broken signal chain is
 * immediately visible in the graph topology.
 */

import type { GraphPreset } from './index';

export const machDiamondsOriginal: GraphPreset = {
  name: 'Mach Diamonds (Original — Broken)',
  description:
    'Original broken signal chain: double-smoothed audio, no normalisation, ' +
    'fixed brightness floor, linear response. Shows WHERE the effect fails.',
  category: 'Demo',

  nodes: [
    // =====================================================================
    // Column 0 — Source (x=0)
    // Only bass used — treble was not independently tracked in original
    // =====================================================================
    {
      id: 'bass-band',
      definitionType: 'band-source',
      parameters: { index: 0 },
      position: { x: 0, y: 0 },
    },
    {
      id: 'beat-strength',
      definitionType: 'beat-strength-source',
      parameters: {},
      position: { x: 0, y: 200 },
    },
    {
      id: 'silent-scale',
      definitionType: 'silent-scale-source',
      parameters: {},
      position: { x: 0, y: 320 },
    },

    // =====================================================================
    // Column 1 — FIRST smoothing layer (x=300)
    // updateSignals() applied a heavy EMA (tau=0.20) BEFORE the effect
    // even saw the data. This is the "helper extraction layer" that the
    // rewrite eliminated.
    // =====================================================================
    {
      id: 'ema-heavy',
      definitionType: 'ema-smooth',
      parameters: { tau: 0.20 },
      position: { x: 300, y: 0 },
    },

    // =====================================================================
    // Column 2 — SECOND smoothing layer (x=600)
    // The effect's own smoothTo() call applied ANOTHER EMA (tau=0.15)
    // on top of the already-smoothed signal. Two cascaded EMAs with
    // tau 200ms + 150ms = effective response time ~350ms.
    // A bass transient that should arrive in 50ms takes 7x longer.
    // =====================================================================
    {
      id: 'ema-effect',
      definitionType: 'ema-smooth',
      parameters: { tau: 0.15 },
      position: { x: 600, y: 0 },
    },

    // =====================================================================
    // Column 3 — NO max follower (x=900)
    // The original had NO dynamic gain normalisation.
    // Instead, the double-smoothed value was used directly as a
    // coefficient to the geometry. This means:
    //   - Quiet music: coefficient ~0.05, geometry barely visible
    //   - Loud music: coefficient ~0.8, geometry over-driven
    //   - No adaptation to volume level
    //
    // (This column is intentionally empty — the gap shows the missing
    // normalisation stage that the fixed version adds.)
    // =====================================================================

    // =====================================================================
    // Column 3 — Geometry (x=900)
    // Triangular wave runs with FIXED parameters, not audio-driven.
    // The double-smoothed bass goes into amplitude, but because there is
    // no normalisation, quiet content barely registers.
    // =====================================================================
    {
      id: 'tri-wave',
      definitionType: 'triangular-wave',
      parameters: { spacing: 0.12, driftSpeed: 1.0, sharpness: 2.0 },
      position: { x: 900, y: 60 },
    },

    // =====================================================================
    // Column 4 — Fixed brightness floor (x=1200)
    // Instead of audio driving brightness directly, the original applied
    // a FIXED FLOOR: brightness = 0.25 + 0.25 * audioCoefficient
    // This means silence still shows 25% brightness — never dark.
    //
    // Scale the double-smoothed bass by 0.25...
    // =====================================================================
    {
      id: 'scale-coeff',
      definitionType: 'scale',
      parameters: { factor: 0.25 },
      position: { x: 1200, y: 0 },
    },
    // ...and add a constant floor of 0.25
    // (modelled as silentScale scaled by 0.25 — silentScale=1.0 when active)
    {
      id: 'const-floor',
      definitionType: 'scale',
      parameters: { factor: 0.25 },
      position: { x: 1200, y: 320 },
    },
    {
      id: 'add-floor',
      definitionType: 'add',
      parameters: {},
      position: { x: 1200, y: 140 },
    },

    // =====================================================================
    // Column 5 — Apply coefficient to geometry (x=1500)
    // The floored coefficient multiplies the geometry.
    // Because the floor is 0.25, even in silence the geometry is 25% bright.
    // =====================================================================
    {
      id: 'mul-geom',
      definitionType: 'multiply',
      parameters: {},
      position: { x: 1500, y: 60 },
    },

    // =====================================================================
    // Column 6 — Silence gate (x=1800)
    // silentScale still applied, but the floor means it barely helps
    // =====================================================================
    {
      id: 'mul-silence',
      definitionType: 'multiply',
      parameters: {},
      position: { x: 1800, y: 60 },
    },

    // =====================================================================
    // Column 7 — NO Power node (x=2100)
    // The original had LINEAR brightness mapping — no squared response.
    // This is another missing stage. The signal goes straight to colour.
    // (This column is intentionally empty — the gap shows the missing
    // squared response that gives the fixed version its punch.)
    // =====================================================================

    // =====================================================================
    // Column 7 — Colour composition (x=2100)
    // HSV->RGB — same as fixed, but receiving a much weaker, floored signal
    // =====================================================================
    {
      id: 'hsv-rgb',
      definitionType: 'hsv-to-rgb',
      parameters: { hueOffset: 0, hueSpread: 48 },
      position: { x: 2100, y: 60 },
    },

    // =====================================================================
    // Column 8 — Output (x=2400)
    // =====================================================================
    {
      id: 'led-out',
      definitionType: 'led-output',
      parameters: { fadeAmount: 30 },
      position: { x: 2400, y: 60 },
    },
  ],

  edges: [
    // Source → FIRST EMA (heavy pre-smoothing from updateSignals layer)
    {
      id: 'e1',
      from: { nodeId: 'bass-band', portName: 'out' },
      to: { nodeId: 'ema-heavy', portName: 'value' },
    },

    // FIRST EMA → SECOND EMA (effect's own smoothTo — double smoothing!)
    {
      id: 'e2',
      from: { nodeId: 'ema-heavy', portName: 'out' },
      to: { nodeId: 'ema-effect', portName: 'value' },
    },

    // Double-smoothed bass → triangular wave amplitude (unnormalised)
    {
      id: 'e3',
      from: { nodeId: 'ema-effect', portName: 'out' },
      to: { nodeId: 'tri-wave', portName: 'amplitude' },
    },

    // Double-smoothed bass → scale by 0.25 (coefficient, not brightness)
    {
      id: 'e4',
      from: { nodeId: 'ema-effect', portName: 'out' },
      to: { nodeId: 'scale-coeff', portName: 'value' },
    },

    // Constant floor: silentScale(1.0) → scale(0.25) → add
    {
      id: 'e5',
      from: { nodeId: 'silent-scale', portName: 'out' },
      to: { nodeId: 'const-floor', portName: 'value' },
    },
    {
      id: 'e6',
      from: { nodeId: 'scale-coeff', portName: 'out' },
      to: { nodeId: 'add-floor', portName: 'a' },
    },
    {
      id: 'e7',
      from: { nodeId: 'const-floor', portName: 'out' },
      to: { nodeId: 'add-floor', portName: 'b' },
    },

    // Floored coefficient × geometry
    {
      id: 'e8',
      from: { nodeId: 'add-floor', portName: 'out' },
      to: { nodeId: 'mul-geom', portName: 'a' },
    },
    {
      id: 'e9',
      from: { nodeId: 'tri-wave', portName: 'out' },
      to: { nodeId: 'mul-geom', portName: 'b' },
    },

    // × silentScale (minimal effect due to brightness floor)
    {
      id: 'e10',
      from: { nodeId: 'mul-geom', portName: 'out' },
      to: { nodeId: 'mul-silence', portName: 'a' },
    },
    {
      id: 'e11',
      from: { nodeId: 'silent-scale', portName: 'out' },
      to: { nodeId: 'mul-silence', portName: 'b' },
    },

    // Straight to HSV→RGB — NO squared response
    {
      id: 'e12',
      from: { nodeId: 'mul-silence', portName: 'out' },
      to: { nodeId: 'hsv-rgb', portName: 'value' },
    },

    // LED Output
    {
      id: 'e13',
      from: { nodeId: 'hsv-rgb', portName: 'out' },
      to: { nodeId: 'led-out', portName: 'colour' },
    },
  ],
};
