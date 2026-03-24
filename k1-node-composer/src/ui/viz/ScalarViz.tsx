/**
 * Canvas-based sparkline for SCALAR port values.
 *
 * Maintains a ring buffer of the last 60 values and draws a line chart
 * with the current numeric value overlaid. Uses useRef + direct canvas
 * API to avoid React state churn at 60fps.
 *
 * Exposes an imperative update(value) method called from the rAF loop.
 */

import { useRef, useEffect, useImperativeHandle, forwardRef } from 'react';

const WIDTH = 120;
const HEIGHT = 24;
const BUFFER_SIZE = 60;

export interface ScalarVizHandle {
  update: (value: number) => void;
}

export const ScalarViz = forwardRef<ScalarVizHandle>(function ScalarViz(_props, ref) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const bufferRef = useRef<Float32Array>(new Float32Array(BUFFER_SIZE));
  const headRef = useRef(0);
  const countRef = useRef(0);
  const currentRef = useRef(0);

  useImperativeHandle(ref, () => ({
    update(value: number) {
      const buf = bufferRef.current;
      buf[headRef.current] = value;
      headRef.current = (headRef.current + 1) % BUFFER_SIZE;
      if (countRef.current < BUFFER_SIZE) countRef.current++;
      currentRef.current = value;
      draw();
    },
  }));

  const draw = () => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const buf = bufferRef.current;
    const head = headRef.current;
    const count = countRef.current;

    // Clear
    ctx.clearRect(0, 0, WIDTH, HEIGHT);

    // Background
    ctx.fillStyle = '#1a1a1a';
    ctx.fillRect(0, 0, WIDTH, HEIGHT);

    if (count < 2) return;

    // Find min/max for auto-scaling
    let min = Infinity;
    let max = -Infinity;
    for (let i = 0; i < count; i++) {
      const idx = (head - count + i + BUFFER_SIZE) % BUFFER_SIZE;
      const v = buf[idx] ?? 0;
      if (v < min) min = v;
      if (v > max) max = v;
    }
    if (max - min < 0.001) {
      min -= 0.5;
      max += 0.5;
    }
    const range = max - min;

    // Draw sparkline
    ctx.strokeStyle = '#66aaff';
    ctx.lineWidth = 1;
    ctx.beginPath();

    const plotWidth = WIDTH - 36; // Reserve space for numeric readout
    for (let i = 0; i < count; i++) {
      const idx = (head - count + i + BUFFER_SIZE) % BUFFER_SIZE;
      const v = buf[idx] ?? 0;
      const x = (i / (count - 1)) * plotWidth;
      const y = HEIGHT - 2 - ((v - min) / range) * (HEIGHT - 4);
      if (i === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    }
    ctx.stroke();

    // Draw current value text
    ctx.fillStyle = '#ccc';
    ctx.font = '10px monospace';
    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    const display = currentRef.current;
    const text = Math.abs(display) >= 100
      ? display.toFixed(0)
      : Math.abs(display) >= 1
        ? display.toFixed(1)
        : display.toFixed(3);
    ctx.fillText(text, WIDTH - 2, HEIGHT / 2);
  };

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
