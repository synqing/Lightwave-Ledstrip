/**
 * Preset index — exports all graph presets for the K1 Node Composer.
 *
 * Each preset is a serialisable snapshot of a node graph: the nodes,
 * their parameters, positions, and all edge connections. Loading a preset
 * into the GraphEngine recreates the full audio-visual signal chain.
 */

import { machDiamondsFixed } from './mach-diamonds-fixed';
import { machDiamondsOriginal } from './mach-diamonds-original';
import { FIXED_PRESETS, ORIGINAL_PRESETS } from './preset-factory';
import { timeReversalMod1Preset } from './time-reversal-mod1';

// ---------------------------------------------------------------------------
// Preset type definition
// ---------------------------------------------------------------------------

export interface PresetNodeEntry {
  id: string;
  definitionType: string;
  parameters: Record<string, number>;
  position: { x: number; y: number };
  /** Optional initial state override (e.g. code-block code string). */
  initialState?: Record<string, unknown>;
}

export interface GraphPreset {
  name: string;
  description: string;
  category?: string;
  nodes: PresetNodeEntry[];
  edges: Array<{
    id: string;
    from: { nodeId: string; portName: string };
    to: { nodeId: string; portName: string };
  }>;
}

// ---------------------------------------------------------------------------
// All presets
// ---------------------------------------------------------------------------

export const ALL_PRESETS: GraphPreset[] = [
  ...FIXED_PRESETS,
  ...ORIGINAL_PRESETS,
  timeReversalMod1Preset,
  machDiamondsFixed,
  machDiamondsOriginal,
];

/** @deprecated Use ALL_PRESETS instead. Kept for backwards compatibility. */
export const PRESETS = ALL_PRESETS;
