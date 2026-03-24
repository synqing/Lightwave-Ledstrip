/**
 * Frame Block node — runs user code ONCE per frame with access to the
 * entire output array. Unlike Code Block (per-pixel), this executes
 * the function once and the function fills all 160 pixels.
 *
 * This is necessary for simulations where each pixel depends on its
 * neighbours (wave equations, reaction-diffusion, cellular automata)
 * because per-pixel execution reads already-updated neighbours.
 *
 * User function signature:
 *   (out, dt, inputs, state) => void
 *
 *   out:    Float32Array(160) — write your output brightness here
 *   dt:     delta time in seconds
 *   inputs: { a, b, c } — three scalar audio parameters
 *   state:  Float32Array(320) — persistent state (doubled for prev/curr)
 */

import { PortType, type NodeDefinition, type NodeState } from '../../engine/types';

const HALF_STRIP = 160;
const STATE_SIZE = 320; // Double — room for prev + curr buffers

const DEFAULT_CODE = `// Frame function: (out, dt, inputs, state) => void
// out = Float32Array(160) — fill with brightness values
// state = Float32Array(320) — persistent between frames
// state[0..79] = u_curr, state[80..159] = u_prev

const N = 80;
const csq = 0.14;
const damping = 0.035;

// Read previous state into temp arrays (avoid order-dependent reads)
const curr = new Float32Array(N);
const prev = new Float32Array(N);
for (let i = 0; i < N; i++) {
  curr[i] = state[i] ?? 0.5;
  prev[i] = state[i + N] ?? 0.5;
}

// Centre impulse from audio
for (let k = 0; k < 10; k++) {
  curr[k] += inputs.a * 0.19 * Math.exp(-k * k * 0.35);
  if (curr[k] > 1.0) curr[k] = 1.0;
}

// Wave equation (all reads from snapshot, writes to state)
for (let i = 0; i < N; i++) {
  const left = i > 0 ? curr[i-1] : curr[1];
  const right = i < N-1 ? curr[i+1] : curr[i];
  const lap = left - 2 * curr[i] + right;
  const edge = Math.max(0, (i/(N-1) - 0.75) / 0.25);
  const damp = damping + edge * 0.09;
  let next = 2*curr[i] - prev[i] + csq*lap - damp*curr[i];
  next = Math.max(-0.35, Math.min(1.35, next));
  state[i] = next;      // new curr
  state[i + N] = curr[i]; // new prev
}

// Normalise and write output
let fMin = state[0], fMax = state[0];
for (let i = 1; i < N; i++) {
  if (state[i] < fMin) fMin = state[i];
  if (state[i] > fMax) fMax = state[i];
}
const range = Math.max(0.05, fMax - fMin);
for (let i = 0; i < N; i++) {
  const v = Math.max(0, Math.min(1, (state[i] - fMin) / range));
  out[i] = Math.pow(v, 1.35); // peak gamma
}
for (let i = N; i < 160; i++) out[i] = 0;
`;

type FrameFn = (out: Float32Array, dt: number, inputs: { a: number; b: number; c: number }, state: Float32Array) => void;

function compileFrameCode(code: string): { fn: FrameFn | null; error: string | null } {
  try {
    const fn = new Function('out', 'dt', 'inputs', 'state', code) as FrameFn;
    // Test-call
    const testOut = new Float32Array(HALF_STRIP);
    const testState = new Float32Array(STATE_SIZE);
    fn(testOut, 0.016, { a: 0, b: 0, c: 0 }, testState);
    return { fn, error: null };
  } catch (e) {
    return { fn: null, error: e instanceof Error ? e.message : String(e) };
  }
}

export const frameBlockNode: NodeDefinition = {
  type: 'frame-block',
  label: 'Frame Block',
  layer: 'geometry',
  inputs: [
    { name: 'a', type: PortType.SCALAR, defaultValue: 0 },
    { name: 'b', type: PortType.SCALAR, defaultValue: 0 },
    { name: 'c', type: PortType.SCALAR, defaultValue: 0 },
  ],
  outputs: [
    { name: 'out', type: PortType.ARRAY_160, defaultValue: new Float32Array(HALF_STRIP) },
  ],
  parameters: [],
  hasState: true,
  cppTemplate: 'node-frame-block',

  getInitialState(): NodeState {
    // Seed wave field: u_curr = 0.5 + gaussian bump, u_prev = 0.5
    const pixelState = new Float32Array(STATE_SIZE);
    for (let i = 0; i < 80; i++) {
      const d = i / 79;
      pixelState[i] = 0.5 + Math.exp(-d * d * 18) * 0.3; // curr
      pixelState[i + 80] = 0.5; // prev
    }
    return {
      code: DEFAULT_CODE,
      compiledFn: null as FrameFn | null,
      lastCompiledCode: '',
      compileError: null as string | null,
      pixelState,
    };
  },

  evaluate(inputs, _params, state, dt, _controlBus) {
    const a = (inputs.get('a') as number) ?? 0;
    const b = (inputs.get('b') as number) ?? 0;
    const c = (inputs.get('c') as number) ?? 0;

    const code = (state?.code as string) ?? DEFAULT_CODE;
    let compiledFn = state?.compiledFn as FrameFn | null;
    const lastCompiledCode = (state?.lastCompiledCode as string) ?? '';
    let compileError = (state?.compileError as string | null) ?? null;
    const pixelState = (state?.pixelState as Float32Array) ?? new Float32Array(STATE_SIZE);

    if (code !== lastCompiledCode) {
      const result = compileFrameCode(code);
      compiledFn = result.fn;
      compileError = result.error;
    }

    const out = new Float32Array(HALF_STRIP);

    if (compiledFn) {
      try {
        compiledFn(out, dt, { a, b, c }, pixelState);
        // Clamp output
        for (let i = 0; i < HALF_STRIP; i++) {
          if (out[i]! < 0) out[i] = 0;
          if (out[i]! > 1) out[i] = 1;
        }
      } catch (e) {
        compileError = e instanceof Error ? e.message : String(e);
      }
    }

    return {
      outputs: new Map([['out', out]]),
      nextState: { code, compiledFn, lastCompiledCode: code, compileError, pixelState },
    };
  },
};
