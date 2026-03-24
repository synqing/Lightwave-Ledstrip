/**
 * Core type system for the K1 Node Composer dataflow engine.
 *
 * 6 port types matching the firmware's data model:
 *   SCALAR     — single float (audio features, parameters)
 *   ARRAY_160  — Float32Array(160) per-pixel values (half-strip)
 *   CRGB_160   — Uint8Array(480) RGB triplets for 160 LEDs
 *   ANGLE      — float in [0, 2π) circular quantity (hue, chroma angle)
 *   BOOL       — boolean (triggers, gates)
 *   INT        — integer (indices, counts)
 */

// ---------------------------------------------------------------------------
// Port Types
// ---------------------------------------------------------------------------

export enum PortType {
  SCALAR = 'SCALAR',
  ARRAY_8 = 'ARRAY_8',       // Octave bands (8 floats)
  ARRAY_12 = 'ARRAY_12',     // Chroma pitch classes (12 floats)
  ARRAY_160 = 'ARRAY_160',   // Per-pixel half-strip (160 floats)
  CRGB_160 = 'CRGB_160',     // RGB triplets for 160 LEDs (480 bytes)
  ANGLE = 'ANGLE',           // Circular quantity in [0, 2π)
  BOOL = 'BOOL',
  INT = 'INT',
}

export type PortValue =
  | number           // SCALAR, ANGLE, INT
  | Float32Array     // ARRAY_160
  | Uint8Array       // CRGB_160 (480 bytes = 160 × RGB)
  | boolean;         // BOOL

// ---------------------------------------------------------------------------
// Identifiers
// ---------------------------------------------------------------------------

export type NodeId = string;
export type PortName = string;

export interface PortAddress {
  nodeId: NodeId;
  portName: PortName;
}

// ---------------------------------------------------------------------------
// Port & Parameter Definitions
// ---------------------------------------------------------------------------

export interface PortDefinition {
  name: PortName;
  type: PortType;
  defaultValue: PortValue;
}

export interface ParameterDefinition {
  name: string;
  min: number;
  max: number;
  step: number;
  defaultValue: number;
  label: string;
}

// ---------------------------------------------------------------------------
// Node State (for stateful nodes like EMA Smooth, Max Follower)
// ---------------------------------------------------------------------------

export type NodeState = Record<string, unknown>;

// ---------------------------------------------------------------------------
// Node Definition — the blueprint for a node type
// ---------------------------------------------------------------------------

export interface NodeDefinition {
  /** Unique type identifier (e.g. 'rms-source', 'ema-smooth') */
  type: string;

  /** Display name */
  label: string;

  /** Layer for colour coding */
  layer: 'source' | 'processing' | 'geometry' | 'composition' | 'output';

  /** Input port definitions */
  inputs: PortDefinition[];

  /** Output port definitions */
  outputs: PortDefinition[];

  /** User-tweakable parameters (rendered as sliders) */
  parameters: ParameterDefinition[];

  /** Whether this node has persistent state across frames */
  hasState: boolean;

  /** Initial state factory (called on reset) */
  getInitialState?: () => NodeState;

  /** Per-frame evaluation function */
  evaluate: (
    inputs: ReadonlyMap<PortName, PortValue>,
    params: ReadonlyMap<string, number>,
    state: NodeState | null,
    dt: number,
    controlBus: ControlBusFrame | null,
  ) => {
    outputs: Map<PortName, PortValue>;
    nextState: NodeState | null;
  };

  /** Handlebars template name for C++ code generation */
  cppTemplate: string;
}

// ---------------------------------------------------------------------------
// ControlBusFrame — matches firmware's ControlBusFrame field-for-field
// ---------------------------------------------------------------------------

export interface ControlBusFrame {
  // Core scalars
  rms: number;
  fastRms: number;
  flux: number;
  fastFlux: number;
  liveliness: number;

  // Octave bands (8)
  bands: Float32Array;       // [8]
  heavyBands: Float32Array;  // [8]

  // Chroma (12)
  chroma: Float32Array;      // [12]
  heavyChroma: Float32Array; // [12]

  // Spectrum (64)
  bins64: Float32Array;      // [64]

  // Beat / tempo
  beatPhase: number;
  beatStrength: number;
  beatTick: boolean;
  downbeatTick: boolean;
  bpm: number;
  tempoConfidence: number;

  // Percussion
  snareEnergy: number;
  hihatEnergy: number;
  snareTrigger: boolean;
  hihatTrigger: boolean;

  // Silence
  silentScale: number;
  isSilent: boolean;

  // Hop sequencing
  hopSeq: number;
}

// ---------------------------------------------------------------------------
// Graph Edge
// ---------------------------------------------------------------------------

export interface Edge {
  id: string;
  from: PortAddress;
  to: PortAddress;
}

// ---------------------------------------------------------------------------
// Graph Node Instance (runtime, not definition)
// ---------------------------------------------------------------------------

export interface NodeInstance {
  id: NodeId;
  definitionType: string;
  parameters: Map<string, number>;
  position: { x: number; y: number };
  /** When true, node passes first input → first output without processing. */
  bypassed?: boolean;
}

// ---------------------------------------------------------------------------
// Type compatibility check
// ---------------------------------------------------------------------------

/** Returns true if an output of type `from` can connect to an input of type `to`. */
export function isTypeCompatible(from: PortType, to: PortType): boolean {
  if (from === to) return true;
  // SCALAR can broadcast to any array input
  if (from === PortType.SCALAR && (to === PortType.ARRAY_160 || to === PortType.ARRAY_8 || to === PortType.ARRAY_12)) return true;
  // ARRAY → SCALAR: reduce to centre value (dist=0, index 0)
  if ((from === PortType.ARRAY_160 || from === PortType.ARRAY_8 || from === PortType.ARRAY_12) && to === PortType.SCALAR) return true;
  // SCALAR ↔ ANGLE (both are numbers)
  if (from === PortType.SCALAR && to === PortType.ANGLE) return true;
  if (from === PortType.ANGLE && to === PortType.SCALAR) return true;
  // INT → SCALAR
  if (from === PortType.INT && to === PortType.SCALAR) return true;
  return false;
}
