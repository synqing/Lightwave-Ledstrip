/**
 * Timeline types for ShowsTab drag-and-drop functionality
 * Aligned with firmware zone system (4 zones + global track)
 */

export interface TimelineScene {
  id: string;
  zoneId: number;              // 0=Global, 1-4=Zones
  effectName: string;
  startTimePercent: number;    // 0-100 position
  durationPercent: number;     // 0-100 width
  accentColor: string;         // Tailwind color class (e.g., 'accent-cyan')
}

export interface Show {
  id: string;
  name: string;
  durationSeconds: number;
  scenes: TimelineScene[];
  isSaved: boolean;
}

export interface DragState {
  isDragging: boolean;
  sceneId: string | null;
  startX: number;              // Initial pointer X
  offsetX: number;             // Offset from scene left edge
  currentZoneId: number | null;
  originalStartPercent: number;
  originalZoneId: number;
}

// Zone color mapping (matches hardware configuration)
export const ZONE_COLORS: Record<number, string> = {
  0: 'primary',      // Global track
  1: 'accent-cyan',  // Zone 1
  2: 'accent-green', // Zone 2
  3: 'text-secondary', // Zone 3
  4: 'primary',      // Zone 4
};

// Snap to 1% increments for smoother editing
export const GRID_SNAP_PERCENT = 1.0;

export const snapToGrid = (percent: number): number => {
  return Math.round(percent / GRID_SNAP_PERCENT) * GRID_SNAP_PERCENT;
};

export const clampPercent = (percent: number, duration: number): number => {
  return Math.max(0, Math.min(100 - duration, percent));
};
