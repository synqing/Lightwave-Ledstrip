/**
 * Code Block node — escape hatch for effects that can't be decomposed
 * into the standard node graph (reaction-diffusion, cellular automata,
 * Lorenz attractors, Kuramoto oscillators, etc.).
 *
 * Executes user-provided JavaScript per-pixel. Sits in the graph like
 * any other node — receives audio scalars as inputs, outputs ARRAY_160.
 *
 * The code string is stored in node state and compiled to a Function
 * on first use or when the code changes. Compilation errors are caught
 * and displayed without crashing the graph.
 *
 * User function signature:
 *   (i, dt, inputs, state) => number
 *
 *   i:      pixel index 0-159 (centre-outward)
 *   dt:     delta time in seconds
 *   inputs: { a, b, c } — three scalar audio parameters
 *   state:  Float32Array(160) — persistent per-pixel state buffer
 *
 *   Returns: brightness for this pixel (0-1, clamped by engine)
 */

import { PortType, type NodeDefinition, type NodeState } from '../../engine/types';

const HALF_STRIP = 160;

const DEFAULT_CODE = `// Per-pixel function: (i, dt, inputs, state) => brightness
// i = pixel index (0-159, centre-outward)
// inputs = { a, b, c } from connected audio nodes
// state = Float32Array(160) persistent between frames
return inputs.a * Math.exp(-(i / 159) * 3.0);`;

type PixelFn = (i: number, dt: number, inputs: { a: number; b: number; c: number }, state: Float32Array) => number;

function compileCode(code: string): { fn: PixelFn | null; error: string | null } {
  try {
    // Wrap user code in a function body
    const fn = new Function('i', 'dt', 'inputs', 'state', code) as PixelFn;
    // Test-call to catch immediate errors
    fn(0, 0.016, { a: 0, b: 0, c: 0 }, new Float32Array(HALF_STRIP));
    return { fn, error: null };
  } catch (e) {
    return { fn: null, error: e instanceof Error ? e.message : String(e) };
  }
}

export const codeBlockNode: NodeDefinition = {
  type: 'code-block',
  label: 'Code Block',
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
  cppTemplate: 'node-code-block',

  getInitialState(): NodeState {
    return {
      code: DEFAULT_CODE,
      compiledFn: null as PixelFn | null,
      lastCompiledCode: '',
      compileError: null as string | null,
      pixelState: new Float32Array(HALF_STRIP),
    };
  },

  evaluate(inputs, _params, state, dt, _controlBus) {
    const a = (inputs.get('a') as number) ?? 0;
    const b = (inputs.get('b') as number) ?? 0;
    const c = (inputs.get('c') as number) ?? 0;

    const code = (state?.code as string) ?? DEFAULT_CODE;
    let compiledFn = state?.compiledFn as PixelFn | null;
    const lastCompiledCode = (state?.lastCompiledCode as string) ?? '';
    let compileError = (state?.compileError as string | null) ?? null;
    const pixelState = (state?.pixelState as Float32Array) ?? new Float32Array(HALF_STRIP);

    // Recompile if code changed
    if (code !== lastCompiledCode) {
      const result = compileCode(code);
      compiledFn = result.fn;
      compileError = result.error;
    }

    const out = new Float32Array(HALF_STRIP);
    const inputObj = { a, b, c };

    if (compiledFn) {
      try {
        for (let i = 0; i < HALF_STRIP; i++) {
          const val = compiledFn(i, dt, inputObj, pixelState);
          out[i] = Math.max(0, Math.min(1, typeof val === 'number' ? val : 0));
        }
      } catch (e) {
        compileError = e instanceof Error ? e.message : String(e);
        // Output remains zeroed on runtime error
      }
    }

    return {
      outputs: new Map([['out', out]]),
      nextState: {
        code,
        compiledFn,
        lastCompiledCode: code,
        compileError,
        pixelState,
      },
    };
  },
};
