/**
 * Canvas-based waveform display for ARRAY_160 port values.
 *
 * Draws 160 vertical bars normalised to [0, 1].
 * White bars on dark background. Direct canvas API, no React state.
 */

import { useRef, useEffect, useImperativeHandle, forwardRef } from 'react';

const WIDTH = 160;
const HEIGHT = 32;
const NUM_PIXELS = 160;

export interface ArrayVizHandle {
  update: (data: Float32Array) => void;
}

export const ArrayViz = forwardRef<ArrayVizHandle>(function ArrayViz(_props, ref) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  const draw = (data: Float32Array | null) => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    // Background
    ctx.fillStyle = '#1a1a1a';
    ctx.fillRect(0, 0, WIDTH, HEIGHT);

    if (!data) return;

    // Draw bars
    ctx.fillStyle = 'rgba(255, 255, 255, 0.8)';
    const barWidth = WIDTH / NUM_PIXELS;
    for (let i = 0; i < NUM_PIXELS && i < data.length; i++) {
      const v = Math.max(0, Math.min(1, data[i] ?? 0));
      const barHeight = v * HEIGHT;
      ctx.fillRect(
        i * barWidth,
        HEIGHT - barHeight,
        Math.max(barWidth - 0.2, 0.5),
        barHeight,
      );
    }
  };

  useImperativeHandle(ref, () => ({
    update(data: Float32Array) {
      draw(data);
    },
  }));

  // Initial draw
  useEffect(() => {
    draw(null);
  });

  return (
    <canvas
      ref={canvasRef}
      width={WIDTH}
      height={HEIGHT}
      style={{ width: WIDTH, height: HEIGHT, borderRadius: 2 }}
    />
  );
});
