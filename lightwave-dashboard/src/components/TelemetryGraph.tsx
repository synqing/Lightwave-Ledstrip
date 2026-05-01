import React, { useEffect, useRef } from 'react';

/**
 * TelemetryGraph Component
 * 
 * Renders a line graph visualization of telemetry data (FPS, CPU, RAM, Network).
 * Displays a time-series line chart with automatic scaling.
 * 
 * @component
 * 
 * @param {number[]} data - Array of numeric values to plot (time-series data)
 * @param {string} color - Stroke color for the line (hex or CSS color)
 * @param {string} className - Additional CSS classes
 * @param {string} label - Label text displayed above the graph
 * @param {string|number} value - Current value displayed next to label
 * @param {React.ReactNode} unit - Unit suffix (e.g., "FPS", "ms", "%")
 * 
 * @remarks
 * ## Transform Reset Pattern
 * 
 * This component uses the same critical transform reset pattern as LGPVisualizer:
 * 
 * ```typescript
 * ctx.setTransform(dpr, 0, 0, dpr, 0, 0);  // Scale by device pixel ratio
 * // ... drawing operations ...
 * ctx.setTransform(1, 0, 0, 1, 0, 0);     // Reset to identity
 * ```
 * 
 * **Why this matters**: Resetting the transform ensures clean state for subsequent
 * renders and prevents accumulation issues.
 * 
 * ## Device Pixel Ratio Handling
 * 
 * Canvas is sized at `width * dpr` and `height * dpr` for high-DPI support.
 * Transform is applied during drawing, then reset after.
 * 
 * ## Data Scaling
 * 
 * - Graph uses 80% of available height for visual padding
 * - X-axis maps data indices to canvas width
 * - Y-axis scales to maximum value in data array
 * - Handles empty/single-value arrays gracefully
 * 
 * ## Performance
 * 
 * - Only redraws when `data` or `color` changes
 * - Resizes canvas only when dimensions actually change
 * - Minimal DOM updates (canvas only)
 * 
 * @example
 * ```tsx
 * <TelemetryGraph
 *   data={[120, 118, 122, 119, 121]}
 *   color="#6ee7f3"
 *   label="Frame Rate"
 *   value={120}
 *   unit="FPS"
 * />
 * ```
 */
interface TelemetryGraphProps {
  data: number[];
  color: string;
  className?: string;
  label?: string;
  value?: string | number;
  unit?: React.ReactNode;
}

const TelemetryGraph: React.FC<TelemetryGraphProps> = ({ 
  data, 
  color, 
  className,
  label,
  value,
  unit 
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    
    // Only resize if dimensions changed
    if (canvas.width !== rect.width * dpr || canvas.height !== rect.height * dpr) {
      canvas.width = rect.width * dpr;
      canvas.height = rect.height * dpr;
    }
    
    const w = canvas.width / dpr;
    const h = canvas.height / dpr;
    
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, w, h);

    if (data.length < 2) return;

    const max = Math.max(...data, 1);
    
    ctx.beginPath();
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.5;
    
    data.forEach((val, i) => {
      // data is expected to be array of recent values
      // We map i (0 to length-1) to x (0 to w)
      const x = (i / (data.length - 1)) * w;
      const y = h - (val / max) * h * 0.8; // Use 80% height
      
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    });
    
    ctx.stroke();
    ctx.setTransform(1, 0, 0, 1, 0, 0);

  }, [data, color]);

  return (
    <div className={`flex flex-col gap-1 ${className}`}>
      <div className="flex items-baseline justify-between">
        <span className="text-xs text-text-secondary uppercase tracking-wider font-semibold">{label}</span>
        <span className="text-lg font-display text-text-primary tabular-nums tracking-wide">
          {value}
          {unit && <span className="text-xs text-text-secondary ml-0.5 font-sans font-normal">{unit}</span>}
        </span>
      </div>
      <div className="h-12 w-full bg-surface/30 rounded border border-white/5 overflow-hidden">
        <canvas ref={canvasRef} className="w-full h-full block" />
      </div>
    </div>
  );
};

export default TelemetryGraph;
