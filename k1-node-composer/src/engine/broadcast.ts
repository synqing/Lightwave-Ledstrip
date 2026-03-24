/**
 * Type promotion and broadcasting utilities.
 *
 * When a SCALAR output connects to an ARRAY_160 input,
 * broadcast the scalar to all 160 elements.
 */

import { PortType, type PortValue } from './types';

const HALF_STRIP = 160;

/** Broadcast a PortValue to match the expected PortType. */
export function broadcast(value: PortValue, fromType: PortType, toType: PortType): PortValue {
  // Same type — pass through
  if (fromType === toType) return value;

  // SCALAR → ARRAY_160: fill 160-element array with the scalar
  if (fromType === PortType.SCALAR && toType === PortType.ARRAY_160) {
    const arr = new Float32Array(HALF_STRIP);
    arr.fill(value as number);
    return arr;
  }

  // SCALAR → ARRAY_8: fill 8-element array
  if (fromType === PortType.SCALAR && toType === PortType.ARRAY_8) {
    const arr = new Float32Array(8);
    arr.fill(value as number);
    return arr;
  }

  // SCALAR → ARRAY_12: fill 12-element array
  if (fromType === PortType.SCALAR && toType === PortType.ARRAY_12) {
    const arr = new Float32Array(12);
    arr.fill(value as number);
    return arr;
  }

  // SCALAR ↔ ANGLE: both are numbers, pass through
  if (
    (fromType === PortType.SCALAR && toType === PortType.ANGLE) ||
    (fromType === PortType.ANGLE && toType === PortType.SCALAR)
  ) {
    return value;
  }

  // INT → SCALAR
  if (fromType === PortType.INT && toType === PortType.SCALAR) {
    return value;
  }

  // ARRAY → SCALAR: reduce to centre value (index 0 = centre-origin dist 0)
  if (
    (fromType === PortType.ARRAY_160 || fromType === PortType.ARRAY_8 || fromType === PortType.ARRAY_12) &&
    toType === PortType.SCALAR
  ) {
    if (value instanceof Float32Array) {
      return value[0] ?? 0;
    }
    return 0;
  }

  // Unsupported promotion — return as-is (connection validator should prevent this)
  return value;
}

/** Element-wise multiply two ARRAY_160 values. */
export function mulArrays(a: Float32Array, b: Float32Array): Float32Array {
  const out = new Float32Array(HALF_STRIP);
  for (let i = 0; i < HALF_STRIP; i++) {
    out[i] = (a[i] ?? 0) * (b[i] ?? 0);
  }
  return out;
}

/** Element-wise add two ARRAY_160 values. */
export function addArrays(a: Float32Array, b: Float32Array): Float32Array {
  const out = new Float32Array(HALF_STRIP);
  for (let i = 0; i < HALF_STRIP; i++) {
    out[i] = (a[i] ?? 0) + (b[i] ?? 0);
  }
  return out;
}

/** Scale an ARRAY_160 by a scalar. */
export function scaleArray(arr: Float32Array, scalar: number): Float32Array {
  const out = new Float32Array(HALF_STRIP);
  for (let i = 0; i < HALF_STRIP; i++) {
    out[i] = (arr[i] ?? 0) * scalar;
  }
  return out;
}
