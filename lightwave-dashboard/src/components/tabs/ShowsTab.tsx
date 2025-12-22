import React, { useState, useRef, useCallback } from 'react';
import { Play, Plus, SkipBack, SkipForward, Repeat } from 'lucide-react';
import type { TimelineScene, Show } from '../../types/timeline';
import { useTimelineDrag } from '../../hooks/useTimelineDrag';
import { useV2 } from '../../state/v2';

// Initial demo show with draggable scenes
const INITIAL_SCENES: TimelineScene[] = [
  { id: 'scene-1', zoneId: 1, effectName: 'Center Pulse', startTimePercent: 5, durationPercent: 25, accentColor: 'accent-cyan' },
  { id: 'scene-2', zoneId: 1, effectName: 'Wavefront', startTimePercent: 32, durationPercent: 20, accentColor: 'accent-green' },
  { id: 'scene-3', zoneId: 1, effectName: 'Aurora Sweep', startTimePercent: 55, durationPercent: 30, accentColor: 'primary' },
  { id: 'scene-4', zoneId: 2, effectName: 'Ambient Glow', startTimePercent: 10, durationPercent: 35, accentColor: 'text-secondary' },
  { id: 'scene-5', zoneId: 2, effectName: 'Ripple Out', startTimePercent: 48, durationPercent: 40, accentColor: 'accent-cyan' },
  { id: 'scene-6', zoneId: 0, effectName: 'Brightness Envelope', startTimePercent: 0, durationPercent: 88, accentColor: 'primary' },
];

const INITIAL_SHOW: Show = {
  id: 'show-1',
  name: 'Aurora Sequence',
  durationSeconds: 165, // 2:45
  scenes: INITIAL_SCENES,
  isSaved: true,
};

// Zone label configuration
const ZONE_LABELS = [
  { id: 1, label: 'ZONE 1', colorClass: 'text-accent-cyan' },
  { id: 2, label: 'ZONE 2', colorClass: 'text-accent-green' },
  { id: 0, label: 'GLOBAL', colorClass: 'text-primary' },
];

// Scene color mappings for dynamic Tailwind classes
const SCENE_COLORS: Record<string, { bg: string; border: string; text: string }> = {
  'accent-cyan': {
    bg: 'from-accent-cyan/30 to-accent-cyan/15',
    border: 'border-accent-cyan/40',
    text: 'text-accent-cyan',
  },
  'accent-green': {
    bg: 'from-accent-green/30 to-accent-green/15',
    border: 'border-accent-green/40',
    text: 'text-accent-green',
  },
  'primary': {
    bg: 'from-primary/30 to-primary/15',
    border: 'border-primary/40',
    text: 'text-primary',
  },
  'text-secondary': {
    bg: 'from-text-secondary/30 to-text-secondary/15',
    border: 'border-text-secondary/40',
    text: 'text-text-secondary',
  },
};

export const ShowsTab: React.FC = () => {
  const { state } = useV2();
  const isConnected = state.connection.wsStatus === 'connected' || state.connection.httpOk;
  const [show, setShow] = useState<Show>(INITIAL_SHOW);
  const [playheadPercent] = useState(15); // Demo position
  const [editingScene, setEditingScene] = useState<TimelineScene | null>(null);
  const [focusedSceneId, setFocusedSceneId] = useState<string | null>(null);

  // Track refs for zone detection during drag
  const trackRefs = useRef<(HTMLDivElement | null)[]>([]);
  const sceneRefs = useRef<Map<string, HTMLDivElement>>(new Map());

  // Handle scene position/zone updates
  const handleSceneMove = useCallback((sceneId: string, newStartPercent: number, newZoneId: number) => {
    setShow(prev => ({
      ...prev,
      isSaved: false,
      scenes: prev.scenes.map(scene =>
        scene.id === sceneId
          ? { ...scene, startTimePercent: newStartPercent, zoneId: newZoneId }
          : scene
      ),
    }));
  }, []);

  // Handle scene deletion
  const handleSceneDelete = useCallback((sceneId: string) => {
    setShow(prev => ({
      ...prev,
      isSaved: false,
      scenes: prev.scenes.filter(scene => scene.id !== sceneId),
    }));
    setFocusedSceneId(null);
  }, []);

  // Get zone IDs in order
  const getZoneIds = () => ZONE_LABELS.map(z => z.id);
  const getPreviousZone = (currentZoneId: number) => {
    const zoneIds = getZoneIds();
    const currentIndex = zoneIds.indexOf(currentZoneId);
    return currentIndex > 0 ? zoneIds[currentIndex - 1] : zoneIds[zoneIds.length - 1];
  };
  const getNextZone = (currentZoneId: number) => {
    const zoneIds = getZoneIds();
    const currentIndex = zoneIds.indexOf(currentZoneId);
    return currentIndex < zoneIds.length - 1 ? zoneIds[currentIndex + 1] : zoneIds[0];
  };

  // Keyboard handler for scene editing
  const handleSceneKeyDown = (e: React.KeyboardEvent, scene: TimelineScene) => {
    switch (e.key) {
      case 'Enter':
      case ' ':
        e.preventDefault();
        setEditingScene(scene);
        break;
      case 'ArrowLeft':
        e.preventDefault();
        handleSceneMove(
          scene.id,
          Math.max(0, scene.startTimePercent - (e.shiftKey ? 5 : 1)),
          scene.zoneId
        );
        break;
      case 'ArrowRight':
        e.preventDefault();
        handleSceneMove(
          scene.id,
          Math.min(100 - scene.durationPercent, scene.startTimePercent + (e.shiftKey ? 5 : 1)),
          scene.zoneId
        );
        break;
      case 'ArrowUp':
        e.preventDefault();
        {
          const prevZone = getPreviousZone(scene.zoneId);
          handleSceneMove(scene.id, scene.startTimePercent, prevZone);
        }
        break;
      case 'ArrowDown':
        e.preventDefault();
        {
          const nextZone = getNextZone(scene.zoneId);
          handleSceneMove(scene.id, scene.startTimePercent, nextZone);
        }
        break;
      case 'Delete':
        e.preventDefault();
        if (window.confirm(`Delete scene "${scene.effectName}"?`)) {
          handleSceneDelete(scene.id);
        }
        break;
    }
  };

  const { dragState, handlePointerDown, handlePointerMove, handlePointerUp, getGhostStyle } = useTimelineDrag({
    onSceneMove: handleSceneMove,
    trackRefs,
  });

  // Get scenes for a specific zone
  const getScenesForZone = (zoneId: number) =>
    show.scenes.filter(scene => scene.zoneId === zoneId);

  // Find the scene being dragged for ghost rendering
  const draggedScene = dragState.isDragging
    ? show.scenes.find(s => s.id === dragState.sceneId)
    : null;

  const ghostStyle = getGhostStyle();

  // Format duration as M:SS
  const formatDuration = (seconds: number) => {
    const mins = Math.floor(seconds / 60);
    const secs = seconds % 60;
    return `${mins}:${secs.toString().padStart(2, '0')}`;
  };

  return (
    <div
      className="p-4 sm:p-5 lg:p-6 xl:p-8 animate-slide-down"
      onPointerMove={(e) => { if (isConnected) handlePointerMove(e); }}
      onPointerUp={(e) => { if (isConnected) handlePointerUp(e); }}
    >
      {!isConnected && (
        <div className="mb-4 rounded-xl border border-accent-red/30 bg-accent-red/10 px-4 py-3 text-xs text-text-primary">
          <div className="font-semibold mb-1">Not connected</div>
          <div className="text-text-secondary">
            Timeline controls are disabled. Connect in <span className="text-text-primary">System</span>.
          </div>
        </div>
      )}
      <div className="flex flex-col gap-2 lg:flex-row lg:items-end lg:justify-between mb-5">
        <div>
          <h1 className="font-display text-2xl uppercase tracking-tight leading-none">Timeline Editor</h1>
          <p className="text-xs text-text-secondary mt-1">Build center-radiating show sequences • Drag scenes to reorder</p>
        </div>
        <div className="flex items-center gap-2">
          <button
            disabled={!isConnected}
            className={`h-9 px-3 rounded-lg border border-white/10 text-text-secondary transition-colors flex items-center text-[11px] font-medium touch-target ${
              isConnected ? 'hover:bg-white/5' : 'opacity-50 cursor-not-allowed'
            }`}
          >
            <Plus className="w-3.5 h-3.5 mr-1" />
            <span className="hidden sm:inline">Add Scene</span>
          </button>
          <button
            disabled={!isConnected}
            className={`h-9 px-3 rounded-lg bg-primary text-background transition-all flex items-center text-[11px] font-medium touch-target ${
              isConnected ? 'hover:brightness-110' : 'opacity-50 cursor-not-allowed'
            }`}
          >
            <Play className="w-3.5 h-3.5 mr-1" />
            Preview
          </button>
        </div>
      </div>

      {/* Timeline Container */}
      <div className="rounded-xl p-4 sm:p-5 bg-surface/95 border border-white/10 card-hover">

        {/* Timeline Header */}
        <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-2 mb-4">
          <div className="flex items-center gap-3">
            <span className="font-display text-[13px] uppercase tracking-wide">Show: {show.name}</span>
            <span className={`rounded px-2 py-0.5 text-[9px] ${
              show.isSaved
                ? 'bg-accent-green/10 border border-accent-green/30 text-accent-green'
                : 'bg-primary/10 border border-primary/30 text-primary'
            }`}>
              {show.isSaved ? 'SAVED' : 'MODIFIED'}
            </span>
          </div>
          <div className="flex items-center gap-2">
            <span className="text-[10px] text-text-secondary">Duration:</span>
            <span className="text-[11px] text-text-primary">{formatDuration(show.durationSeconds)}</span>
          </div>
        </div>

        {/* Time Ruler */}
        <div className="relative mb-2 h-6 bg-surface/50 rounded-md">
          <div className="absolute inset-x-4 top-0 bottom-0 flex justify-between items-center text-[8px] text-text-secondary">
            <span>0:00</span>
            <span className="hidden sm:inline">0:30</span>
            <span>1:00</span>
            <span className="hidden sm:inline">1:30</span>
            <span>2:00</span>
            <span className="hidden md:inline">2:30</span>
          </div>
          {/* Playhead */}
          <div
            className="absolute top-0 bottom-0 w-0.5 bg-primary shadow-[0_0_8px_rgba(255,184,77,0.5)] rounded-full z-20"
            style={{ left: `${playheadPercent}%` }}
          />
        </div>

        {/* Timeline Tracks */}
        <div className="space-y-2">
          {ZONE_LABELS.map((zone, index) => (
            <div
              key={zone.id}
              ref={el => { trackRefs.current[index] = el; }}
              data-track={zone.id}
              className={`relative rounded-lg p-2 bg-surface/50 border min-h-[3rem] transition-colors ${
                dragState.isDragging && dragState.currentZoneId === zone.id
                  ? 'border-primary/40 bg-primary/5'
                  : 'border-white/5'
              }`}
            >
              <div className={`absolute left-2 top-1/2 -translate-y-1/2 hidden sm:block text-[8px] ${zone.colorClass} writing-mode-vertical rotate-180`}>
                {zone.label}
              </div>
              <div className="sm:ml-6 relative h-8 flex items-center">
                {getScenesForZone(zone.id).map(scene => {
                  const colors = SCENE_COLORS[scene.accentColor] || SCENE_COLORS['primary'];
                  const isFocused = focusedSceneId === scene.id;
                  return (
                    <div
                      key={scene.id}
                      ref={el => {
                        if (el) {
                          sceneRefs.current.set(scene.id, el);
                        } else {
                          sceneRefs.current.delete(scene.id);
                        }
                      }}
                      onPointerDown={(e) => { if (isConnected) handlePointerDown(e, scene); }}
                      onKeyDown={(e) => { if (isConnected) handleSceneKeyDown(e, scene); }}
                      onFocus={() => setFocusedSceneId(scene.id)}
                      onBlur={() => {
                        // Only clear focus if not moving to another scene
                        setTimeout(() => {
          if (!sceneRefs.current.get(scene.id)?.contains(document.activeElement)) {
            setFocusedSceneId(null);
          }
        }, 0);
                      }}
                      tabIndex={isConnected ? 0 : -1}
                      role="button"
                      aria-label={`Scene: ${scene.effectName}, Zone ${zone.label}, Start: ${scene.startTimePercent.toFixed(1)}%, Duration: ${scene.durationPercent.toFixed(1)}%`}
                      className={`absolute rounded-lg flex items-center justify-center border transition-transform cursor-grab active:cursor-grabbing h-full bg-gradient-to-br focus:outline-none focus:ring-2 focus:ring-primary focus:ring-offset-2 focus:ring-offset-background ${
                        dragState.sceneId === scene.id ? 'opacity-30' : 'hover:scale-105'
                      } ${isFocused ? 'ring-2 ring-primary ring-offset-2' : ''} ${colors.bg} ${colors.border}`}
                      style={{
                        left: `${scene.startTimePercent}%`,
                        width: `${scene.durationPercent}%`,
                      }}
                    >
                      <span className={`truncate px-1 text-[8px] ${colors.text}`}>
                        {scene.effectName}
                      </span>
                      {/* Keyframe diamonds */}
                      <div className="absolute -bottom-1 left-[10%] w-1.5 h-1.5 bg-primary rotate-45 rounded-[1px] hidden sm:block" />
                      <div className="absolute -bottom-1 left-[50%] w-1.5 h-1.5 bg-primary rotate-45 rounded-[1px] hidden sm:block" />
                      <div className="absolute -bottom-1 left-[90%] w-1.5 h-1.5 bg-primary rotate-45 rounded-[1px] hidden sm:block" />
                    </div>
                  );
                })}
              </div>
            </div>
          ))}
        </div>

        {/* Ghost element during drag */}
        {dragState.isDragging && draggedScene && ghostStyle && (
          <div
            className={`timeline-ghost rounded-lg flex items-center justify-center border bg-gradient-to-br from-${draggedScene.accentColor}/30 to-${draggedScene.accentColor}/15 border-${draggedScene.accentColor}/40`}
            style={ghostStyle}
          >
            <span className={`truncate px-1 text-[8px] text-${draggedScene.accentColor}`}>
              {draggedScene.effectName}
            </span>
          </div>
        )}

        {/* Transport Controls */}
        <div className="flex items-center justify-center gap-2 mt-4 pt-4 border-t border-white/5">
          <button className="w-10 h-10 rounded-lg border border-white/10 flex items-center justify-center text-text-secondary hover:bg-white/5 transition-colors touch-target">
            <SkipBack className="w-4 h-4" />
          </button>
          <button className="w-12 h-12 rounded-lg bg-primary text-background flex items-center justify-center hover:brightness-110 transition-all touch-target shadow-[0_0_15px_rgba(255,184,77,0.4)]">
            <Play className="w-5 h-5 fill-current" />
          </button>
          <button className="w-10 h-10 rounded-lg border border-white/10 flex items-center justify-center text-text-secondary hover:bg-white/5 transition-colors touch-target">
            <SkipForward className="w-4 h-4" />
          </button>
          <div className="w-px h-6 bg-white/10 mx-2" />
          <button className="w-10 h-10 rounded-lg border border-white/10 flex items-center justify-center text-text-secondary hover:bg-white/5 transition-colors touch-target">
            <Repeat className="w-4 h-4" />
          </button>
        </div>
      </div>

      {/* Scene Edit Dialog */}
      {editingScene && (
        <div 
          className="fixed inset-0 bg-black/50 flex items-center justify-center z-50 p-4"
          onClick={() => setEditingScene(null)}
          role="dialog"
          aria-modal="true"
          aria-labelledby="scene-edit-title"
        >
          <div 
            className="bg-surface rounded-xl p-6 max-w-md w-full border border-white/10"
            onClick={(e) => e.stopPropagation()}
          >
            <h2 id="scene-edit-title" className="text-xl font-display uppercase tracking-tight mb-4">
              Edit Scene: {editingScene.effectName}
            </h2>
            <div className="space-y-4">
              <div>
                <label className="block text-xs font-medium mb-2">Start Time (%)</label>
                <input
                  type="number"
                  min="0"
                  max={100 - editingScene.durationPercent}
                  value={editingScene.startTimePercent}
                  onChange={(e) => {
                    const newStart = Math.max(0, Math.min(100 - editingScene.durationPercent, parseFloat(e.target.value) || 0));
                    handleSceneMove(editingScene.id, newStart, editingScene.zoneId);
                    setEditingScene({ ...editingScene, startTimePercent: newStart });
                  }}
                  className="w-full bg-[#2f3849] border border-white/10 rounded-lg px-3 py-2 text-sm text-text-primary focus:outline-none focus:ring-2 focus:ring-primary"
                />
              </div>
              <div>
                <label className="block text-xs font-medium mb-2">Duration (%)</label>
                <input
                  type="number"
                  min="1"
                  max={100 - editingScene.startTimePercent}
                  value={editingScene.durationPercent}
                  onChange={(e) => {
                    const newDuration = Math.max(1, Math.min(100 - editingScene.startTimePercent, parseFloat(e.target.value) || 1));
                    handleSceneMove(editingScene.id, editingScene.startTimePercent, editingScene.zoneId);
                    setEditingScene({ ...editingScene, durationPercent: newDuration });
                  }}
                  className="w-full bg-[#2f3849] border border-white/10 rounded-lg px-3 py-2 text-sm text-text-primary focus:outline-none focus:ring-2 focus:ring-primary"
                />
              </div>
              <div>
                <label className="block text-xs font-medium mb-2">Zone</label>
                <select
                  value={editingScene.zoneId}
                  onChange={(e) => {
                    const newZoneId = parseInt(e.target.value);
                    handleSceneMove(editingScene.id, editingScene.startTimePercent, newZoneId);
                    setEditingScene({ ...editingScene, zoneId: newZoneId });
                  }}
                  className="w-full bg-[#2f3849] border border-white/10 rounded-lg px-3 py-2 text-sm text-text-primary focus:outline-none focus:ring-2 focus:ring-primary"
                >
                  {ZONE_LABELS.map(zone => (
                    <option key={zone.id} value={zone.id}>{zone.label}</option>
                  ))}
                </select>
              </div>
              <div className="flex gap-2 justify-end">
                <button
                  onClick={() => setEditingScene(null)}
                  className="px-4 py-2 rounded-lg border border-white/10 text-sm hover:bg-white/5 transition-colors focus:outline-none focus:ring-2 focus:ring-primary"
                >
                  Cancel
                </button>
                <button
                  onClick={() => setEditingScene(null)}
                  className="px-4 py-2 rounded-lg bg-primary text-background text-sm hover:brightness-110 transition-all focus:outline-none focus:ring-2 focus:ring-primary"
                >
                  Done
                </button>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};
