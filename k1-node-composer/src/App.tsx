/**
 * K1 Node Composer — main application shell.
 *
 * Layout:
 *   Top bar:  LED preview (full width) + title + controls
 *   Left:     NodePalette
 *   Centre:   NodeEditor (React Flow canvas)
 *   Right:    CodeExport panel (toggled)
 */

import { useCallback, useEffect, useRef, useState } from 'react';
import { ReactFlowProvider } from '@xyflow/react';
import { NodeEditor } from './ui/NodeEditor';
import { NodePalette } from './ui/NodePalette';
import { LedPreview, type LedPreviewHandle } from './ui/LedPreview';
import { CodeExport } from './ui/CodeExport';
import { EffectsLibrary } from './ui/EffectsLibrary';
import { useGraphEngine } from './ui/hooks/useGraphEngine';
import { useAudioManager } from './ui/hooks/useAudioManager';
import { useUndoRedo } from './ui/hooks/useUndoRedo';
import './ui/styles/nodes.css';

export function App() {
  const engineHandle = useGraphEngine();
  const audio = useAudioManager();

  const [fps, setFps] = useState(0);
  const [showCodeExport, setShowCodeExport] = useState(false);
  const [showEffectsLibrary, setShowEffectsLibrary] = useState(false);
  const [codeGeneration, setCodeGeneration] = useState(0);
  const frameCountRef = useRef(0);
  const lastFpsTimeRef = useRef(performance.now());
  const ledPreviewRef = useRef<LedPreviewHandle>(null);

  // Wire up audio provider to the engine
  useEffect(() => {
    engineHandle.setAudioProvider(() => {
      audio.tick(1 / 60);
      return audio.getFrame();
    });
  }, [engineHandle, audio]);

  // Auto-start simulated audio on mount
  useEffect(() => {
    audio.startSimulated();
  }, [audio]);

  // Subscribe to engine outputs: drive LED preview + FPS counter
  useEffect(() => {
    const unsub = engineHandle.subscribe((outputs) => {
      // FPS counter
      frameCountRef.current++;
      const now = performance.now();
      const elapsed = now - lastFpsTimeRef.current;
      if (elapsed >= 1000) {
        setFps(Math.round((frameCountRef.current / elapsed) * 1000));
        frameCountRef.current = 0;
        lastFpsTimeRef.current = now;
      }

      // Find the LED output node and push its data to the preview
      for (const [_nodeId, nodeOutputs] of outputs) {
        const leds = nodeOutputs.get('leds');
        if (leds instanceof Uint8Array && leds.length >= 960) {
          ledPreviewRef.current?.update(leds);
          break;
        }
      }
    });
    return unsub;
  }, [engineHandle]);

  // Mic toggle
  const handleMicToggle = useCallback(async () => {
    if (audio.isActive && !audio.isSimulated) {
      audio.stop();
      audio.startSimulated();
    } else {
      audio.stop();
      await audio.start();
    }
  }, [audio]);

  // Code export toggle
  const handleCodeExportToggle = useCallback(() => {
    setShowCodeExport((prev) => !prev);
    setCodeGeneration((prev) => prev + 1);
  }, []);

  // Effects library toggle
  const handleEffectsToggle = useCallback(() => {
    setShowEffectsLibrary((prev) => !prev);
  }, []);

  // Undo/redo
  const undoRedo = useUndoRedo(engineHandle.engine);

  // Stable ref to editor handle so keyboard handler always sees latest state
  const editorRef = useRef<ReturnType<typeof NodeEditor> | null>(null);

  // Callback passed to NodeEditor — snapshots current state before each mutation
  const handleBeforeMutation = useCallback(() => {
    const ed = editorRef.current;
    if (!ed) return;
    undoRedo.pushSnapshot(ed.getNodes(), ed.getEdges());
  }, [undoRedo]);

  const editor = NodeEditor({ engineHandle, onBeforeMutation: handleBeforeMutation });
  editorRef.current = editor;

  // Handle undo action
  const handleUndo = useCallback(() => {
    const ed = editorRef.current;
    if (!ed) return;
    const snapshot = undoRedo.undo(ed.getNodes(), ed.getEdges());
    if (snapshot) {
      ed.applySnapshot(snapshot.nodes, snapshot.edges, snapshot.engineState);
    }
  }, [undoRedo]);

  // Handle redo action
  const handleRedo = useCallback(() => {
    const ed = editorRef.current;
    if (!ed) return;
    const snapshot = undoRedo.redo(ed.getNodes(), ed.getEdges());
    if (snapshot) {
      ed.applySnapshot(snapshot.nodes, snapshot.edges, snapshot.engineState);
    }
  }, [undoRedo]);

  // Keyboard shortcuts: Ctrl+Z (undo), Ctrl+Shift+Z (redo)
  useEffect(() => {
    const onKeyDown = (e: KeyboardEvent) => {
      // Skip when the user is typing in an input, textarea, or contenteditable
      const tag = (e.target as HTMLElement)?.tagName;
      if (tag === 'INPUT' || tag === 'TEXTAREA') return;
      if ((e.target as HTMLElement)?.isContentEditable) return;

      const isMod = e.ctrlKey || e.metaKey;
      if (!isMod || e.key.toLowerCase() !== 'z') return;

      e.preventDefault();

      if (e.shiftKey) {
        handleRedo();
      } else {
        handleUndo();
      }
    };

    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [handleUndo, handleRedo]);

  return (
    <div className="app-container">
      {/* Top bar: title | LED preview (centred) | controls */}
      <header className="app-topbar">
        <span className="app-topbar__title">K1</span>
        <div className="app-topbar__led-wrapper">
          <LedPreview ref={ledPreviewRef} leds={null} />
        </div>
        <div className="app-topbar__controls">
          <button
            className="app-topbar__btn app-topbar__btn--undo"
            onClick={handleUndo}
            disabled={!undoRedo.canUndo}
            title="Undo (Ctrl+Z)"
          >
            &#x2190;
          </button>
          <button
            className="app-topbar__btn app-topbar__btn--redo"
            onClick={handleRedo}
            disabled={!undoRedo.canRedo}
            title="Redo (Ctrl+Shift+Z)"
          >
            &#x2192;
          </button>
          <button
            className={`app-topbar__btn ${audio.isActive && !audio.isSimulated ? 'app-topbar__btn--active' : ''}`}
            onClick={handleMicToggle}
            title={audio.isActive && !audio.isSimulated ? 'Using microphone' : 'Using simulated audio'}
          >
            {audio.isActive && !audio.isSimulated ? 'Mic ON' : 'Mic OFF'}
          </button>
          <button
            className={`app-topbar__btn ${showEffectsLibrary ? 'app-topbar__btn--active' : ''}`}
            onClick={handleEffectsToggle}
            title="Toggle effects library"
          >
            Effects
          </button>
          <button
            className={`app-topbar__btn ${showCodeExport ? 'app-topbar__btn--active' : ''}`}
            onClick={handleCodeExportToggle}
          >
            C++
          </button>
          <span className="app-topbar__fps">{fps} FPS</span>
        </div>
      </header>

      {/* Main area */}
      <div className="app-main">
        {/* Left sidebar */}
        <NodePalette onAddNode={editor.addNodeFromPalette} />

        {/* Centre canvas */}
        <div className="app-canvas">
          {editor.canvas}
        </div>

        {/* Right sidebar — effects library */}
        {showEffectsLibrary && (
          <EffectsLibrary
            onLoadPreset={(preset) => {
              editor.loadPreset(preset);
              setShowEffectsLibrary(false);
            }}
            onClose={() => setShowEffectsLibrary(false)}
          />
        )}

        {/* Right sidebar — code export */}
        {showCodeExport && (
          <CodeExport engine={engineHandle.engine} generation={codeGeneration} />
        )}
      </div>
    </div>
  );
}

export function AppRoot() {
  return (
    <ReactFlowProvider>
      <App />
    </ReactFlowProvider>
  );
}
