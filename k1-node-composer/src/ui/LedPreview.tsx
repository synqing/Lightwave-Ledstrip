/**
 * LED Strip Preview — Canvas 2D component that renders 320 LEDs
 * with LGP diffusion simulation via blur + additive blending.
 *
 * Uses requestAnimationFrame for rendering, NOT React state.
 * Exposes an imperative update(leds) method for the engine loop.
 */

import {
  useRef,
  useEffect,
  useImperativeHandle,
  forwardRef,
  useCallback,
} from 'react';
import './LedPreview.css';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const TOTAL_LEDS = 320;
const STRIP_1_COUNT = 160;
const LED_WIDTH = 3;
const STRIP_HEIGHT = 20;
const CANVAS_WIDTH = STRIP_1_COUNT * LED_WIDTH;       // 480px (strips stacked vertically)
const CANVAS_HEIGHT = STRIP_HEIGHT * 2;               // 40px (two strips)
const DIVIDER_Y = STRIP_HEIGHT;

// ---------------------------------------------------------------------------
// Public handle type
// ---------------------------------------------------------------------------

export interface LedPreviewHandle {
  /** Push a new frame of LED data. Uint8Array(960) = 320 x RGB. */
  update(leds: Uint8Array): void;
}

// ---------------------------------------------------------------------------
// Props
// ---------------------------------------------------------------------------

interface LedPreviewProps {
  /** CRGB_320 data: Uint8Array(960) = 320 x RGB */
  leds: Uint8Array | null;
}

// ---------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------

export const LedPreview = forwardRef<LedPreviewHandle, LedPreviewProps>(
  function LedPreview({ leds }, ref) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const ledsRef = useRef<Uint8Array | null>(null);
    const rafRef = useRef<number>(0);

    // Keep ledsRef in sync with prop for the initial/prop-driven path
    useEffect(() => {
      ledsRef.current = leds ?? null;
    }, [leds]);

    // Imperative update — called from engine loop, avoids React re-render
    const update = useCallback((data: Uint8Array) => {
      ledsRef.current = data;
    }, []);

    useImperativeHandle(ref, () => ({ update }), [update]);

    // ---------------------------------------------------------------------------
    // Render loop
    // ---------------------------------------------------------------------------

    useEffect(() => {
      const canvas = canvasRef.current;
      if (!canvas) return;

      const ctx = canvas.getContext('2d', { alpha: false });
      if (!ctx) return;

      function draw() {
        if (!ctx) return;

        const data = ledsRef.current;

        // Clear to black
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);

        if (data && data.length >= TOTAL_LEDS * 3) {
          // Match led_capture.py render_strip_frame(): discrete coloured blocks,
          // no blur, no additive blending. Faithful to hardware output.
          for (let i = 0; i < TOTAL_LEDS; i++) {
            const r = data[i * 3] ?? 0;
            const g = data[i * 3 + 1] ?? 0;
            const b = data[i * 3 + 2] ?? 0;

            ctx.fillStyle = `rgb(${r},${g},${b})`;

            if (i < STRIP_1_COUNT) {
              ctx.fillRect(i * LED_WIDTH, 0, LED_WIDTH, STRIP_HEIGHT);
            } else {
              const x = (i - STRIP_1_COUNT) * LED_WIDTH;
              ctx.fillRect(x, STRIP_HEIGHT, LED_WIDTH, STRIP_HEIGHT);
            }
          }
        }

        // --- Divider line between strip 1 and strip 2 ---
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.08)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(0, DIVIDER_Y);
        ctx.lineTo(CANVAS_WIDTH, DIVIDER_Y);
        ctx.stroke();

        rafRef.current = requestAnimationFrame(draw);
      }

      rafRef.current = requestAnimationFrame(draw);

      return () => {
        cancelAnimationFrame(rafRef.current);
      };
    }, []);

    return (
      <div className="led-preview-container">
        <canvas
          ref={canvasRef}
          width={CANVAS_WIDTH}
          height={CANVAS_HEIGHT}
          className="led-preview-canvas"
        />
      </div>
    );
  },
);
