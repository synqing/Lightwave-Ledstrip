import React, { useEffect, useRef } from 'react';

/**
 * LGPVisualizer Component
 * 
 * Renders a real-time visualization of the LED strip, showing either:
 * - Real LED data from firmware (when streaming)
 * - Simulated center-radiating effect (fallback)
 * 
 * @component
 * 
 * @param {number} brightness - Brightness multiplier (0-1), defaults to 192/255
 * @param {number} speed - Animation speed multiplier, defaults to 18
 * @param {string} className - Additional CSS classes
 * @param {boolean} isConnected - Connection status, affects rendering
 * @param {Uint8Array|null} ledData - Real LED data from firmware (960 bytes: 320 LEDs × RGB)
 * @param {boolean} isStreaming - Whether receiving live LED stream
 * 
 * @remarks
 * ## Transform Reset Pattern
 * 
 * This component uses a critical pattern to prevent canvas transform accumulation:
 * 
 * ```typescript
 * ctx.setTransform(1, 0, 0, 1, 0, 0);  // Reset to identity
 * ctx.clearRect(0, 0, canvas.width, canvas.height);
 * ctx.scale(dpr, dpr);  // Apply device pixel ratio
 * ```
 * 
 * **Why this matters**: Without resetting the transform, `ctx.scale()` accumulates
 * on every resize, causing visual artifacts and incorrect rendering.
 * 
 * ## Device Pixel Ratio Handling
 * 
 * The canvas is sized at `width * dpr` and `height * dpr` to support high-DPI displays.
 * All drawing operations are scaled by `dpr` to maintain crisp rendering.
 * 
 * ## roundRect Fallback
 * 
 * The component uses `ctx.roundRect()` when available (modern browsers) and falls
 * back to `ctx.fillRect()` for older browsers. This ensures compatibility while
 * providing better visuals on supported browsers.
 * 
 * ## Performance Considerations
 * 
 * - Uses `requestAnimationFrame` for smooth 60fps animation
 * - LED count adapts to viewport width (80/120/160 LEDs)
 * - Real LED data rendering is optimized for 320 LEDs
 * - Memory: Stores LED data in ref to avoid re-renders
 * 
 * @example
 * ```tsx
 * <LGPVisualizer
 *   brightness={0.75}
 *   speed={20}
 *   isConnected={true}
 *   ledData={ledDataArray}
 *   isStreaming={true}
 * />
 * ```
 */
interface LGPVisualizerProps {
  brightness?: number;
  speed?: number;
  className?: string;
  isConnected?: boolean;
  /** Real LED data from firmware (dual-strip format: top/bottom edges, each 160 LEDs × RGB) */
  ledData?: { top: Uint8Array; bottom: Uint8Array } | null;
  /** Whether receiving live LED stream */
  isStreaming?: boolean;
}

const EDGE_LED_COUNT = 160;

const LGPVisualizer: React.FC<LGPVisualizerProps> = ({
  brightness = 192 / 255,
  speed = 18,
  className,
  isConnected = true,
  ledData = null,
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const ledDataRef = useRef<{ top: Uint8Array; bottom: Uint8Array } | null>(null);

  // Keep ledData in a ref for animation loop access
  useEffect(() => {
    ledDataRef.current = ledData;
  }, [ledData]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let animationFrameId: number;

    // Config for simulated mode (when no real data)
    let LED_COUNT = 120;
    let CENTER_INDEX = (LED_COUNT - 1) / 2;
    
    const palette = [
      { r: 110, g: 231, b: 243 },
      { r: 34, g: 221, b: 136 },
      { r: 255, g: 184, b: 77 },
      { r: 181, g: 189, b: 202 }
    ];

    const lerpColor = (c1: {r:number, g:number, b:number}, c2: {r:number, g:number, b:number}, t: number) => {
      return { 
        r: Math.round(c1.r + (c2.r - c1.r) * t), 
        g: Math.round(c1.g + (c2.g - c1.g) * t), 
        b: Math.round(c1.b + (c2.b - c1.b) * t) 
      };
    };

    const resizeCanvas = () => {
      if (!canvas.parentElement) return;
      const rect = canvas.parentElement.getBoundingClientRect();
      const dpr = window.devicePixelRatio || 1;
      
      // Update LED count based on width
      const width = rect.width;
      LED_COUNT = width < 768 ? 80 : (width < 1024 ? 120 : 160);
      CENTER_INDEX = (LED_COUNT - 1) / 2;

      canvas.width = rect.width * dpr;
      canvas.height = 96 * dpr;
      
      canvas.style.width = `${rect.width}px`;
      canvas.style.height = `96px`;
    };

    // Render real LED data from firmware (dual edge-lit) using the SAME visual style
    // as the original continuous-strip visualiser, but split into two aligned rows.
    const renderRealLeds = () => {
      const dpr = window.devicePixelRatio || 1;
      const width = canvas.width / dpr;
      const height = canvas.height / dpr;

      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      ctx.scale(dpr, dpr);

      const data = ledDataRef.current;
      if (!data || !data.top || !data.bottom || 
          data.top.length < EDGE_LED_COUNT * 3 || 
          data.bottom.length < EDGE_LED_COUNT * 3) return;

      // Background (same as original)
      const bgGradient = ctx.createRadialGradient(width / 2, height / 2, 0, width / 2, height / 2, width / 2);
      bgGradient.addColorStop(0, 'rgba(110, 231, 243, 0.04)');
      bgGradient.addColorStop(1, 'rgba(10, 13, 18, 1)');
      ctx.fillStyle = bgGradient;
      ctx.fillRect(0, 0, width, height);

      // Dual edge-lit rendering (two rows)
      //
      // IMPORTANT:
      // - Each physical edge is 160 LEDs, both start at index 0 (left) and go to 159 (right).
      // - The previous visualiser drew 320 LEDs across the full width (top+bottom concatenated).
      // - To preserve the same per-LED visual resolution/aesthetic, we render each 160-LED edge
      //   at half-width, centred within the canvas. This yields the same per-LED width as before:
      //     old: fullWidth / 320  vs new: (fullWidth/2) / 160  => equal.
      // - Top edge emits DOWNWARD (into plate), bottom edge emits UPWARD (into plate).
      const padX = 10;
      const fullW = width - padX * 2;
      const edgeW = fullW / 2;
      const edgeX = padX + (fullW - edgeW) / 2; // centre the edge span
      const barW = edgeW / EDGE_LED_COUNT;
      const rowGap = 8;
      const rowH = (height - 20 - rowGap) / 2; // two rows inside margins
      const topBaseY = 10 + rowH;
      const bottomBaseY = 10 + rowH + rowGap + rowH;
      const maxBarH = rowH - 6;

      // Clip drawing to the edge span area (prevents visual "dead rail" extending full width)
      ctx.save();
      ctx.beginPath();
      ctx.rect(edgeX, 10, edgeW, height - 20);
      ctx.clip();

      // Draw a single row with configurable emission direction
      const drawRow = (stripData: Uint8Array, baseY: number, emitDownward: boolean) => {
        for (let i = 0; i < EDGE_LED_COUNT; i++) {
          // Both strips: index 0 = left, index 159 = right (no horizontal mirroring)
          const idx = i * 3;
          const r = stripData[idx];
          const g = stripData[idx + 1];
          const b = stripData[idx + 2];

          const luminance = (0.299 * r + 0.587 * g + 0.114 * b) / 255;
          const barH = Math.max(2, luminance * maxBarH);
          const alpha = 0.5 + luminance * 0.5;

          const x = edgeX + i * barW;
          const w = Math.max(1, barW - 0.5);

          if (emitDownward) {
            // Top edge: emit downward (bars project from baseY downward)
            const glow = ctx.createLinearGradient(0, baseY, 0, baseY + barH);
            glow.addColorStop(0, `rgba(${r}, ${g}, ${b}, ${alpha})`);
            glow.addColorStop(1, `rgba(${r}, ${g}, ${b}, 0.05)`);
            ctx.fillStyle = glow;
            ctx.fillRect(x, baseY, w, barH);
          } else {
            // Bottom edge: emit upward (bars project from baseY upward)
            const glow = ctx.createLinearGradient(0, baseY, 0, baseY - barH);
            glow.addColorStop(0, `rgba(${r}, ${g}, ${b}, ${alpha})`);
            glow.addColorStop(1, `rgba(${r}, ${g}, ${b}, 0.05)`);
            ctx.fillStyle = glow;
            ctx.fillRect(x, baseY - barH, w, barH);
          }
        }

        // Centre marker (79/80 split)
        const centreIdx = Math.floor((EDGE_LED_COUNT - 1) / 2);
        const centreX = edgeX + (centreIdx + 0.5) * barW;
        ctx.strokeStyle = 'rgba(110, 231, 243, 0.4)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        if (emitDownward) {
          ctx.moveTo(centreX, baseY - 2);
          ctx.lineTo(centreX, baseY + maxBarH + 2);
        } else {
          ctx.moveTo(centreX, baseY - maxBarH - 2);
          ctx.lineTo(centreX, baseY + 2);
        }
        ctx.stroke();
      };

      // TOP edge (GPIO4): emit DOWNWARD (into plate)
      drawRow(data.top, topBaseY, true);
      // BOTTOM edge (GPIO5): emit UPWARD (into plate)
      drawRow(data.bottom, bottomBaseY, false);

      // Restore clipping region
      ctx.restore();
    };

    // Render simulated LEDs (fallback when no real data)
    const renderSimulatedStrip = (time: number) => {
      const dpr = window.devicePixelRatio || 1;
      const width = canvas.width / dpr;
      const height = canvas.height / dpr;

      // Reset transform before clearing
      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      ctx.scale(dpr, dpr);

      // Handle disconnected state
      if (!isConnected) {
        ctx.fillStyle = '#1a1d26';
        ctx.fillRect(0, 0, width, height);
        ctx.font = '12px Inter';
        ctx.fillStyle = '#ef4444';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText('DISCONNECTED', width / 2, height / 2);
        return;
      }

      const barWidth = (width - 20) / LED_COUNT;
      const maxBarHeight = height - 20;
      const baseY = height - 8;

      // Background
      const bgGradient = ctx.createRadialGradient(width / 2, height / 2, 0, width / 2, height / 2, width / 2);
      bgGradient.addColorStop(0, 'rgba(110, 231, 243, 0.06)');
      bgGradient.addColorStop(1, 'rgba(10, 13, 18, 1)');
      ctx.fillStyle = bgGradient;
      ctx.fillRect(0, 0, width, height);

      // LEDs
      for (let i = 0; i < LED_COUNT; i++) {
        const distanceFromCenter = Math.abs(i - CENTER_INDEX) / CENTER_INDEX;
        const parabolicHeight = 1 - Math.pow(distanceFromCenter, 2);
        const wavePhase = distanceFromCenter * Math.PI * 3 - time * speed * 0.0003;
        const waveAmplitude = Math.sin(wavePhase) * 0.15 * (1 - distanceFromCenter);
        const normalizedHeight = Math.max(0.1, (parabolicHeight + waveAmplitude) * brightness);
        const barHeight = normalizedHeight * maxBarHeight;

        const colorIndex = (i / LED_COUNT * (palette.length - 1) + Math.sin(i * 0.1 + time * 0.002) * 0.5 + 0.5) % (palette.length - 1);
        const idx1 = Math.floor(colorIndex);
        // Safety check for indices
        const c1 = palette[idx1 % palette.length];
        const c2 = palette[(idx1 + 1) % palette.length];
        const color = lerpColor(c1, c2, colorIndex - idx1);

        const alpha = 0.4 + normalizedHeight * 0.6;

        const glowGradient = ctx.createLinearGradient(0, baseY, 0, baseY - barHeight);
        glowGradient.addColorStop(0, `rgba(${color.r}, ${color.g}, ${color.b}, ${alpha})`);
        glowGradient.addColorStop(1, `rgba(${color.r}, ${color.g}, ${color.b}, 0.05)`);

        ctx.fillStyle = glowGradient;
        const maybeRoundRect = (ctx as unknown as { roundRect?: (x: number, y: number, w: number, h: number, radii: number) => void }).roundRect;
        if (typeof maybeRoundRect === 'function') {
          ctx.beginPath();
          maybeRoundRect.call(ctx, 10 + i * barWidth, baseY - barHeight, barWidth - 1, barHeight, 2);
          ctx.fill();
        } else {
          ctx.fillRect(10 + i * barWidth, baseY - barHeight, barWidth - 1, barHeight);
        }
      }

      // Center Line
      const centerX = 10 + CENTER_INDEX * barWidth;
      ctx.strokeStyle = 'rgba(110, 231, 243, 0.5)';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(centerX, 20);
      ctx.lineTo(centerX, height - 8);
      ctx.stroke();
    };

    const animate = (time: number) => {
      // Use real LED data if streaming, otherwise fall back to simulation
      const hasReal =
        !!ledDataRef.current?.top &&
        !!ledDataRef.current?.bottom &&
        ledDataRef.current.top.length >= EDGE_LED_COUNT * 3 &&
        ledDataRef.current.bottom.length >= EDGE_LED_COUNT * 3;
      if (hasReal) {
        renderRealLeds();
      } else {
        renderSimulatedStrip(time);
      }
      animationFrameId = requestAnimationFrame(animate);
    };

    // Initial setup
    resizeCanvas();
    window.addEventListener('resize', resizeCanvas);
    animationFrameId = requestAnimationFrame(animate);

    return () => {
      window.removeEventListener('resize', resizeCanvas);
      cancelAnimationFrame(animationFrameId);
    };
  }, [brightness, speed, isConnected]);

  return (
    <canvas
      ref={canvasRef}
      id="lgpCanvas"
      className={className}
      style={{ height: '6rem' }}
    />
  );
};

export default LGPVisualizer;
