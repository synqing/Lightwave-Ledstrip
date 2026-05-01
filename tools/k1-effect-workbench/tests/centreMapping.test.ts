import { describe, expect, it } from 'vitest';
import { makeCentrePulseFrame } from '../src/capture/frameParser';

describe('centre-origin mapping', () => {
  it('lights the centre pair first on both 160-LED strips', () => {
    const frame = makeCentrePulseFrame();
    const lit = [79, 80, 239, 240].map((index) => {
      const base = index * 3;
      return frame.leds[base] + frame.leds[base + 1] + frame.leds[base + 2];
    });
    expect(lit.every((value) => value > 0)).toBe(true);
  });
});

