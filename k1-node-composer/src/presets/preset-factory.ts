/**
 * Preset factory — generates Fixed and Original preset variants for all
 * 10 rewritten 5L-AR effects programmatically.
 *
 * All effects share the same audio chain; only the geometry node differs.
 * The Fixed pattern is the correct rewrite (18 nodes, 18 edges).
 * The Original pattern is the broken chain (13 nodes, 13 edges).
 */

import type { GraphPreset, PresetNodeEntry } from './index';

// ---------------------------------------------------------------------------
// Geometry specification
// ---------------------------------------------------------------------------

export interface EffectGeometry {
  /** Node definition type from registry (e.g. 'triangular-wave'). */
  nodeType: string;
  /** Numeric parameters for the geometry node. */
  params: Record<string, number>;
  /** Code string for code-block nodes only. */
  code?: string;
}

// ---------------------------------------------------------------------------
// Edge helper
// ---------------------------------------------------------------------------

interface EdgeSpec {
  id: string;
  from: { nodeId: string; portName: string };
  to: { nodeId: string; portName: string };
}

function edge(id: string, fromNode: string, fromPort: string, toNode: string, toPort: string): EdgeSpec {
  return { id, from: { nodeId: fromNode, portName: fromPort }, to: { nodeId: toNode, portName: toPort } };
}

// ---------------------------------------------------------------------------
// Node helper
// ---------------------------------------------------------------------------

function node(
  id: string,
  definitionType: string,
  parameters: Record<string, number>,
  x: number,
  y: number,
  initialState?: Record<string, unknown>,
): PresetNodeEntry {
  const entry: PresetNodeEntry = { id, definitionType, parameters, position: { x, y } };
  if (initialState) entry.initialState = initialState;
  return entry;
}

// ---------------------------------------------------------------------------
// Fixed preset factory (18 nodes, 18 edges)
// ---------------------------------------------------------------------------

export function createFixedPreset(
  name: string,
  effectId: string,
  geometry: EffectGeometry,
): GraphPreset {
  const p = effectId; // prefix for node IDs

  // Build geometry node — code-block has no numeric params, code lives in state
  const geomParams: Record<string, number> = geometry.nodeType === 'code-block' ? {} : { ...geometry.params };
  const geomState = geometry.code ? { code: geometry.code } : undefined;

  const nodes: PresetNodeEntry[] = [
    // Column 0 — Sources (x=0)
    node(`${p}-bass-band`,     'band-source',          { index: 0 }, 0,   0),
    node(`${p}-treble-band`,   'band-source',          { index: 5 }, 0, 120),
    node(`${p}-beat-strength`, 'beat-strength-source',  {},           0, 240),
    node(`${p}-silent-scale`,  'silent-scale-source',   {},           0, 360),

    // Column 1 — Single-stage EMA smoothing (x=250)
    node(`${p}-ema-bass`,   'ema-smooth', { tau: 0.05 }, 250,   0),
    node(`${p}-ema-treble`, 'ema-smooth', { tau: 0.04 }, 250, 120),

    // Column 2 — Max follower normalisation (x=500)
    node(`${p}-follower-bass`,   'max-follower', { attackTau: 0.058, decayTau: 0.50, floor: 0.04 }, 500,   0),
    node(`${p}-follower-treble`, 'max-follower', { attackTau: 0.058, decayTau: 0.50, floor: 0.04 }, 500, 120),

    // Column 3 — Geometry (x=750)
    node(`${p}-geom`, geometry.nodeType, geomParams, 750, 60, geomState),

    // Column 4 — Audio x Geometry (x=1000)
    node(`${p}-mul-bass-geom`, 'multiply', {}, 1000, 30),

    // Column 5 — Beat modulation (x=1250)
    node(`${p}-beat-scale`, 'scale', { factor: 0.7 }, 1250, 240),
    node(`${p}-beat-offset`, 'add', {}, 1250, 340),
    node(`${p}-const-0p3`, 'scale', { factor: 0.3 }, 1100, 380),

    // Column 6 — Multiply by beatMod (x=1500)
    node(`${p}-mul-beat`, 'multiply', {}, 1500, 100),

    // Column 7 — Multiply by silentScale (x=1750)
    node(`${p}-mul-silence`, 'multiply', {}, 1750, 100),

    // Column 8 — Squared response (x=2000)
    node(`${p}-power-sq`, 'power', { exponent: 2.0 }, 2000, 100),

    // Column 9 — Colour (x=2250)
    node(`${p}-hsv-rgb`, 'hsv-to-rgb', { hueOffset: 0, hueSpread: 48 }, 2250, 60),

    // Column 10 — Output (x=2500)
    node(`${p}-led-out`, 'led-output', { fadeAmount: 30 }, 2500, 60),
  ];

  const edges: EdgeSpec[] = [
    // Sources -> EMA
    edge(`${p}-e1`,  `${p}-bass-band`,   'out', `${p}-ema-bass`,   'value'),
    edge(`${p}-e2`,  `${p}-treble-band`,  'out', `${p}-ema-treble`, 'value'),

    // EMA -> Max followers
    edge(`${p}-e3`,  `${p}-ema-bass`,   'out', `${p}-follower-bass`,   'value'),
    edge(`${p}-e4`,  `${p}-ema-treble`, 'out', `${p}-follower-treble`, 'value'),

    // normBass -> geometry amplitude
    edge(`${p}-e5`,  `${p}-follower-bass`, 'normalised', `${p}-geom`, geometry.nodeType === 'code-block' ? 'a' : 'amplitude'),

    // normBass x geometry
    edge(`${p}-e6`,  `${p}-follower-bass`, 'normalised', `${p}-mul-bass-geom`, 'a'),
    edge(`${p}-e7`,  `${p}-geom`,          'out',        `${p}-mul-bass-geom`, 'b'),

    // Beat modulation: beat -> scale(0.7) -> add(+0.3)
    edge(`${p}-e8`,  `${p}-beat-strength`, 'out', `${p}-beat-scale`, 'value'),
    edge(`${p}-e9`,  `${p}-beat-scale`,    'out', `${p}-beat-offset`, 'a'),
    edge(`${p}-e10`, `${p}-silent-scale`,  'out', `${p}-const-0p3`,   'value'),
    edge(`${p}-e11`, `${p}-const-0p3`,     'out', `${p}-beat-offset`,  'b'),

    // brightness x beatMod
    edge(`${p}-e12`, `${p}-mul-bass-geom`, 'out', `${p}-mul-beat`, 'a'),
    edge(`${p}-e13`, `${p}-beat-offset`,   'out', `${p}-mul-beat`, 'b'),

    // x silentScale
    edge(`${p}-e14`, `${p}-mul-beat`,     'out', `${p}-mul-silence`, 'a'),
    edge(`${p}-e15`, `${p}-silent-scale`, 'out', `${p}-mul-silence`, 'b'),

    // Power (squared)
    edge(`${p}-e16`, `${p}-mul-silence`, 'out', `${p}-power-sq`, 'value'),

    // HSV -> RGB
    edge(`${p}-e17`, `${p}-power-sq`, 'out', `${p}-hsv-rgb`, 'value'),

    // LED Output
    edge(`${p}-e18`, `${p}-hsv-rgb`, 'out', `${p}-led-out`, 'colour'),
  ];

  return {
    name: `${name} (Fixed)`,
    description:
      `Rewritten ${name}: single-stage EMA, max follower normalisation, ` +
      'squared brightness response, silence = dark.',
    category: '5L-AR (Fixed)',
    nodes,
    edges,
  };
}

// ---------------------------------------------------------------------------
// Original (broken) preset factory (13 nodes, 13 edges)
// ---------------------------------------------------------------------------

export function createOriginalPreset(
  name: string,
  effectId: string,
  geometry: EffectGeometry,
): GraphPreset {
  const p = effectId;

  const geomParams: Record<string, number> = geometry.nodeType === 'code-block' ? {} : { ...geometry.params };
  const geomState = geometry.code ? { code: geometry.code } : undefined;

  const nodes: PresetNodeEntry[] = [
    // Column 0 — Source (x=0)
    node(`${p}-bass-band`,     'band-source',         { index: 0 }, 0,   0),
    node(`${p}-beat-strength`, 'beat-strength-source', {},           0, 200),
    node(`${p}-silent-scale`,  'silent-scale-source',  {},           0, 320),

    // Column 1 — FIRST smoothing (heavy pre-smoothing, x=250)
    node(`${p}-ema-heavy`, 'ema-smooth', { tau: 0.20 }, 250, 0),

    // Column 2 — SECOND smoothing (effect smoothTo, x=500)
    node(`${p}-ema-effect`, 'ema-smooth', { tau: 0.15 }, 500, 0),

    // Column 3 — Geometry (x=750)
    node(`${p}-geom`, geometry.nodeType, geomParams, 750, 60, geomState),

    // Column 4 — Fixed brightness floor (x=1000)
    node(`${p}-scale-coeff`, 'scale', { factor: 0.25 }, 1000,   0),
    node(`${p}-const-floor`, 'scale', { factor: 0.25 }, 1000, 320),
    node(`${p}-add-floor`,   'add',   {},               1000, 140),

    // Column 5 — Coefficient x geometry (x=1250)
    node(`${p}-mul-geom`, 'multiply', {}, 1250, 60),

    // Column 6 — Silence gate (x=1500)
    node(`${p}-mul-silence`, 'multiply', {}, 1500, 60),

    // Column 7 — Colour + output (x=1750, x=2000)
    node(`${p}-hsv-rgb`, 'hsv-to-rgb', { hueOffset: 0, hueSpread: 48 }, 1750, 60),
    node(`${p}-led-out`, 'led-output', { fadeAmount: 30 }, 2000, 60),
  ];

  const edges: EdgeSpec[] = [
    // Source -> FIRST EMA
    edge(`${p}-e1`, `${p}-bass-band`, 'out', `${p}-ema-heavy`, 'value'),

    // FIRST -> SECOND EMA (double smoothing)
    edge(`${p}-e2`, `${p}-ema-heavy`, 'out', `${p}-ema-effect`, 'value'),

    // Double-smoothed -> geometry amplitude (unnormalised)
    edge(`${p}-e3`, `${p}-ema-effect`, 'out', `${p}-geom`, geometry.nodeType === 'code-block' ? 'a' : 'amplitude'),

    // Double-smoothed -> scale(0.25) coefficient
    edge(`${p}-e4`, `${p}-ema-effect`, 'out', `${p}-scale-coeff`, 'value'),

    // Floor: silentScale(1.0) -> scale(0.25) -> add
    edge(`${p}-e5`, `${p}-silent-scale`, 'out', `${p}-const-floor`, 'value'),
    edge(`${p}-e6`, `${p}-scale-coeff`,  'out', `${p}-add-floor`,   'a'),
    edge(`${p}-e7`, `${p}-const-floor`,  'out', `${p}-add-floor`,   'b'),

    // Floored coefficient x geometry
    edge(`${p}-e8`, `${p}-add-floor`, 'out', `${p}-mul-geom`, 'a'),
    edge(`${p}-e9`, `${p}-geom`,      'out', `${p}-mul-geom`, 'b'),

    // x silentScale
    edge(`${p}-e10`, `${p}-mul-geom`,     'out', `${p}-mul-silence`, 'a'),
    edge(`${p}-e11`, `${p}-silent-scale`, 'out', `${p}-mul-silence`, 'b'),

    // Straight to HSV -> RGB (NO squared response)
    edge(`${p}-e12`, `${p}-mul-silence`, 'out', `${p}-hsv-rgb`, 'value'),

    // LED Output
    edge(`${p}-e13`, `${p}-hsv-rgb`, 'out', `${p}-led-out`, 'colour'),
  ];

  return {
    name: `${name} (Original)`,
    description:
      `Original broken ${name}: double-smoothed audio, no normalisation, ` +
      'fixed brightness floor, linear response.',
    category: '5L-AR (Original)',
    nodes,
    edges,
  };
}

// ---------------------------------------------------------------------------
// Effect definitions — geometry varies, audio chain is identical
// ---------------------------------------------------------------------------

const GRAY_SCOTT_CODE = `// Simplified wave simulation
const prev = state[i] ?? 0;
const left = state[Math.max(0, i-1)] ?? 0;
const right = state[Math.min(159, i+1)] ?? 0;
const diffusion = (left + right) * 0.5 - prev;
state[i] = Math.max(0, Math.min(1, prev + diffusion * 0.1 + inputs.a * 0.02 * (i > 75 && i < 85 ? 1 : 0)));
state[i] *= 0.995; // Slow decay
return state[i];`;

const SUPERFORMULA_CODE = `// Superformula glyph
const progress = i / 159;
const m = 6 + 4 * inputs.b;
const phi = progress * Math.PI * 2 + dt;
const cosT = Math.abs(Math.cos(m * phi / 4));
const sinT = Math.abs(Math.sin(m * phi / 4));
const n2 = 1.5 + inputs.a;
const n3 = 1.5 + inputs.b;
const r = Math.pow(Math.pow(cosT, n2) + Math.pow(sinT, n3) + 0.001, -1.0 / (1.0 + inputs.a));
const dist = Math.abs(progress - r * 0.35);
return inputs.a * Math.exp(-dist / 0.12);`;

interface EffectSpec {
  name: string;
  id: string;
  geometry: EffectGeometry;
}

const EFFECT_SPECS: EffectSpec[] = [
  {
    name: 'Mach Diamonds',
    id: 'md',
    geometry: { nodeType: 'triangular-wave', params: { spacing: 0.12, sharpness: 2.0, driftSpeed: 1.0 } },
  },
  {
    name: 'Airy Comet',
    id: 'ac',
    geometry: { nodeType: 'gaussian', params: { sigma: 0.25 } },
  },
  {
    name: 'Cymatic Ladder',
    id: 'cl',
    geometry: { nodeType: 'standing-wave', params: { mode_n: 3, contrast: 2.0, driftSpeed: 0.8 } },
  },
  {
    name: 'Rose Bloom',
    id: 'rb',
    geometry: { nodeType: 'gaussian', params: { sigma: 0.15, centre: 0.3 } },
  },
  {
    name: 'Schlieren Flow',
    id: 'sf',
    geometry: { nodeType: 'sinusoidal-sum', params: { f1: 0.06, f2: 0.145, f3: 0.31 } },
  },
  {
    name: 'Water Caustics',
    id: 'wc',
    geometry: { nodeType: 'sinusoidal-sum', params: { f1: 0.06, f2: 0.12, f3: 0.25 } },
  },
  {
    name: 'Talbot Carpet',
    id: 'tc',
    geometry: { nodeType: 'standing-wave', params: { mode_n: 5, contrast: 2.5, driftSpeed: 0.6 } },
  },
  {
    name: 'Moire Cathedral',
    id: 'mc',
    geometry: { nodeType: 'sinusoidal-sum', params: { f1: 0.08, f2: 0.085, f3: 0.18 } },
  },
  {
    name: 'Reaction Diffusion',
    id: 'rd',
    geometry: { nodeType: 'code-block', params: {}, code: GRAY_SCOTT_CODE },
  },
  {
    name: 'Superformula Glyph',
    id: 'sg',
    geometry: { nodeType: 'code-block', params: {}, code: SUPERFORMULA_CODE },
  },
];

// ---------------------------------------------------------------------------
// Generate all presets
// ---------------------------------------------------------------------------

export const FIXED_PRESETS: GraphPreset[] = EFFECT_SPECS.map((spec) =>
  createFixedPreset(spec.name, spec.id, spec.geometry),
);

export const ORIGINAL_PRESETS: GraphPreset[] = EFFECT_SPECS.map((spec) =>
  createOriginalPreset(spec.name, spec.id, spec.geometry),
);
