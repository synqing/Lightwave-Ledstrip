/**
 * Canvas-based colour strip for CRGB_160 port values.
 *
 * Draws 160 pixel-wide coloured rectangles from a Uint8Array(480)
 * representing 160 RGB triplets. Direct canvas API, no React state.
 */

import { useRef, useEffect, useImperativeHandle, forwardRef } from 'react';

const WIDTH = 160;
const HEIGHT = 8;
const NUM_PIXELS = 160;

export interface CrgbVizHandle {
  update: (data: Uint8Array) => void;
}

export const CrgbViz = forwardRef<CrgbVizHandle>(function CrgbViz(_props, ref) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  const draw = (data: Uint8Array | null) => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    // Background
    ctx.fillStyle = '#0a0a0a';
    ctx.fillRect(0, 0, WIDTH, HEIGHT);

    if (!data || data.length < NUM_PIXELS * 3) return;

    const barWidth = WIDTH / NUM_PIXELS;
    for (let i = 0; i < NUM_PIXELS; i++) {
      const r = data[i * 3] ?? 0;
      const g = data[i * 3 + 1] ?? 0;
      const b = data[i * 3 + 2] ?? 0;
      ctx.fillStyle = `rgb(${r},${g},${b})`;
      ctx.fillRect(i * barWidth, 0, Math.max(barWidth, 0.5), HEIGHT);
    }
  };

  useImperativeHandle(ref, () => ({
    update(data: Uint8Array) {
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
