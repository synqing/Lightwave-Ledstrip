import { useState, useCallback, useRef, useEffect } from 'react';
import type { TimelineScene, DragState } from '../types/timeline';
import { snapToGrid } from '../types/timeline';

interface UseTimelineDragOptions {
  onSceneMove: (sceneId: string, newStartPercent: number, newZoneId: number) => void;
  trackRefs: React.MutableRefObject<(HTMLDivElement | null)[]>;
}

interface UseTimelineDragReturn {
  dragState: DragState;
  handlePointerDown: (e: React.PointerEvent, scene: TimelineScene) => void;
  handlePointerMove: (e: React.PointerEvent) => void;
  handlePointerUp: (e: React.PointerEvent) => void;
  getGhostStyle: () => React.CSSProperties | null;
}

const INITIAL_DRAG_STATE: DragState = {
  isDragging: false,
  sceneId: null,
  startX: 0,
  offsetX: 0,
  currentZoneId: null,
  originalStartPercent: 0,
  originalZoneId: 0,
};

export function useTimelineDrag({
  onSceneMove,
  trackRefs,
}: UseTimelineDragOptions): UseTimelineDragReturn {
  const [dragState, setDragState] = useState<DragState>(INITIAL_DRAG_STATE);
  const ghostRef = useRef<{ x: number; y: number; width: number; height: number } | null>(null);
  const containerRef = useRef<HTMLDivElement | null>(null);

  // Detect which zone track the cursor is over
  const detectZone = useCallback((clientY: number): number | null => {
    const tracks = trackRefs.current;
    for (let i = 0; i < tracks.length; i++) {
      const track = tracks[i];
      if (track) {
        const rect = track.getBoundingClientRect();
        if (clientY >= rect.top && clientY <= rect.bottom) {
          // Zone IDs: 0=Zone1, 1=Zone2, 2=Global (reverse order in DOM)
          // Map DOM index to zone ID
          return i === 2 ? 0 : i + 1;
        }
      }
    }
    return null;
  }, [trackRefs]);

  // Calculate percentage position from pointer X
  const calculatePercent = useCallback((clientX: number, trackElement: HTMLDivElement | null): number => {
    if (!trackElement) return 0;
    const rect = trackElement.getBoundingClientRect();
    const relativeX = clientX - rect.left;
    const percent = (relativeX / rect.width) * 100;
    return Math.max(0, Math.min(100, percent));
  }, []);

  const handlePointerDown = useCallback((e: React.PointerEvent, scene: TimelineScene) => {
    e.preventDefault();
    e.stopPropagation();

    const target = e.currentTarget as HTMLElement;
    target.setPointerCapture(e.pointerId);

    const rect = target.getBoundingClientRect();
    const offsetX = e.clientX - rect.left;

    // Store ghost dimensions
    ghostRef.current = {
      x: e.clientX - offsetX,
      y: rect.top,
      width: rect.width,
      height: rect.height,
    };

    // Find the container for percentage calculations
    const trackElement = target.closest('[data-track]') as HTMLDivElement;
    containerRef.current = trackElement;

    setDragState({
      isDragging: true,
      sceneId: scene.id,
      startX: e.clientX,
      offsetX,
      currentZoneId: scene.zoneId,
      originalStartPercent: scene.startTimePercent,
      originalZoneId: scene.zoneId,
    });
  }, []);

  const handlePointerMove = useCallback((e: React.PointerEvent) => {
    if (!dragState.isDragging || !ghostRef.current) return;

    // Update ghost position
    ghostRef.current.x = e.clientX - dragState.offsetX;
    ghostRef.current.y = e.clientY - ghostRef.current.height / 2;

    // Detect zone hover
    const hoveredZone = detectZone(e.clientY);
    if (hoveredZone !== null && hoveredZone !== dragState.currentZoneId) {
      setDragState(prev => ({ ...prev, currentZoneId: hoveredZone }));
    }

    // Force re-render for ghost position update
    setDragState(prev => ({ ...prev }));
  }, [dragState.isDragging, dragState.offsetX, dragState.currentZoneId, detectZone]);

  const handlePointerUp = useCallback((e: React.PointerEvent) => {
    if (!dragState.isDragging || !dragState.sceneId) {
      setDragState(INITIAL_DRAG_STATE);
      return;
    }

    (e.currentTarget as HTMLElement).releasePointerCapture(e.pointerId);

    // Calculate final position
    const trackElement = containerRef.current;
    if (trackElement) {
      const rawPercent = calculatePercent(e.clientX - dragState.offsetX, trackElement);
      const snappedPercent = snapToGrid(rawPercent);
      const finalZone = dragState.currentZoneId ?? dragState.originalZoneId;

      // Only trigger move if position actually changed
      if (snappedPercent !== dragState.originalStartPercent || finalZone !== dragState.originalZoneId) {
        onSceneMove(dragState.sceneId, snappedPercent, finalZone);
      }
    }

    ghostRef.current = null;
    containerRef.current = null;
    setDragState(INITIAL_DRAG_STATE);
  }, [dragState, calculatePercent, onSceneMove]);

  const getGhostStyle = useCallback((): React.CSSProperties | null => {
    if (!dragState.isDragging || !ghostRef.current) return null;

    return {
      position: 'fixed',
      left: ghostRef.current.x,
      top: ghostRef.current.y,
      width: ghostRef.current.width,
      height: ghostRef.current.height,
      opacity: 0.7,
      zIndex: 1000,
      pointerEvents: 'none',
    };
  }, [dragState.isDragging]);

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      ghostRef.current = null;
      containerRef.current = null;
    };
  }, []);

  return {
    dragState,
    handlePointerDown,
    handlePointerMove,
    handlePointerUp,
    getGhostStyle,
  };
}
