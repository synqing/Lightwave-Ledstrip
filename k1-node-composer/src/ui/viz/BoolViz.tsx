/**
 * Simple green/red dot indicator for BOOL port values.
 *
 * 32x16px. Green dot when true, red when false.
 * Uses direct canvas API, no React state.
 */

import { useRef, useEffect, useImperativeHandle, forwardRef } from 'react';

const WIDTH = 32;
const HEIGHT = 16;

export interface BoolVizHandle {
  update: (value: boolean) => void;
}

export const BoolViz = forwardRef<BoolVizHandle>(function BoolViz(_props, ref) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const currentRef = useRef(false);

  const draw = () => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    // Background
    ctx.fillStyle = '#1a1a1a';
    ctx.fillRect(0, 0, WIDTH, HEIGHT);

    // Dot
    const colour = currentRef.current ? '#44ff44' : '#ff4444';
    ctx.beginPath();
    ctx.arc(WIDTH / 2, HEIGHT / 2, 5, 0, Math.PI * 2);
    ctx.fillStyle = colour;
    ctx.fill();

    // Glow
    ctx.shadowColor = colour;
    ctx.shadowBlur = 4;
    ctx.beginPath();
    ctx.arc(WIDTH / 2, HEIGHT / 2, 3, 0, Math.PI * 2);
    ctx.fill();
    ctx.shadowBlur = 0;
  };

  useImperativeHandle(ref, () => ({
    update(value: boolean) {
      currentRef.current = value;
      draw();
    },
  }));

  // Initial draw
  useEffect(() => {
    draw();
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
