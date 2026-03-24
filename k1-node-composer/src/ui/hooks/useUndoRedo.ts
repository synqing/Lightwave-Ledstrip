/**
 * Undo/redo hook for the K1 Node Composer.
 *
 * Maintains two stacks of graph snapshots (undo + redo, max 50 each).
 * Each snapshot captures React Flow nodes/edges and engine state so that
 * the entire visual + computational graph can be restored atomically.
 *
 * Keyboard shortcuts (Ctrl+Z / Ctrl+Shift+Z) are handled externally
 * by the App component to avoid duplicate listeners.
 */

import { useCallback, useRef, useState } from 'react';
import type { Node, Edge as RFEdge } from '@xyflow/react';
import type { GraphEngine } from '../../engine/engine';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

/** A complete snapshot of the graph for undo/redo purposes. */
export interface UndoRedoSnapshot {
  nodes: Node[];
  edges: RFEdge[];
  engineState: Map<string, Record<string, unknown>>;
}

export interface UndoRedoHandle {
  /** Save the current graph state onto the undo stack. */
  pushSnapshot: (nodes: Node[], edges: RFEdge[]) => void;
  /** Restore the previous state. Returns the snapshot to apply, or null. */
  undo: (currentNodes: Node[], currentEdges: RFEdge[]) => UndoRedoSnapshot | null;
  /** Re-apply a previously undone state. Returns the snapshot to apply, or null. */
  redo: (currentNodes: Node[], currentEdges: RFEdge[]) => UndoRedoSnapshot | null;
  /** Whether the undo stack is non-empty. */
  canUndo: boolean;
  /** Whether the redo stack is non-empty. */
  canRedo: boolean;
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const MAX_HISTORY = 50;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Deep-clone nodes for snapshot storage.
 *
 * Position and id are plain objects/primitives and clone cheaply.
 * We structuredClone the data payload — callback references (onParamChange)
 * are not cloneable, but they are re-attached when the snapshot is restored
 * into the React Flow state by the NodeEditor.
 *
 * We deliberately avoid cloning Float32Array / Uint8Array fields inside
 * node data because typed arrays live in engine outputs, not in RF node data.
 */
function cloneNodes(nodes: Node[]): Node[] {
  return nodes.map((n) => ({
    ...n,
    position: { ...n.position },
    // Strip non-cloneable callbacks before cloning, keep only serialisable data
    data: cloneNodeData(n.data as Record<string, unknown>),
  }));
}

/**
 * Clone node data, preserving serialisable fields and stripping functions.
 * Functions (like onParamChange) are re-attached during restore.
 */
function cloneNodeData(data: Record<string, unknown>): Record<string, unknown> {
  const cloned: Record<string, unknown> = {};
  for (const key of Object.keys(data)) {
    const value = data[key];
    if (typeof value === 'function') continue;
    cloned[key] = structuredClone(value);
  }
  return cloned;
}

function cloneEdges(edges: RFEdge[]): RFEdge[] {
  return edges.map((e) => ({ ...e }));
}

// ---------------------------------------------------------------------------
// Hook
// ---------------------------------------------------------------------------

export function useUndoRedo(engine: GraphEngine): UndoRedoHandle {
  const undoStackRef = useRef<UndoRedoSnapshot[]>([]);
  const redoStackRef = useRef<UndoRedoSnapshot[]>([]);

  // Revision counter triggers re-render so canUndo/canRedo stay current.
  // We only need the setter — the value itself is unused in render output.
  const [, setRevision] = useState(0);
  const bump = useCallback(() => setRevision((r) => r + 1), []);

  const pushSnapshot = useCallback(
    (nodes: Node[], edges: RFEdge[]) => {
      const snapshot: UndoRedoSnapshot = {
        nodes: cloneNodes(nodes),
        edges: cloneEdges(edges),
        engineState: engine.snapshot(),
      };

      const stack = undoStackRef.current;
      stack.push(snapshot);

      // Enforce maximum history depth
      if (stack.length > MAX_HISTORY) {
        stack.splice(0, stack.length - MAX_HISTORY);
      }

      // Any new mutation clears the redo stack
      redoStackRef.current = [];
      bump();
    },
    [engine, bump],
  );

  const undo = useCallback(
    (currentNodes: Node[], currentEdges: RFEdge[]): UndoRedoSnapshot | null => {
      const undoStack = undoStackRef.current;
      if (undoStack.length === 0) return null;

      // Save current state onto redo stack before restoring
      const currentSnapshot: UndoRedoSnapshot = {
        nodes: cloneNodes(currentNodes),
        edges: cloneEdges(currentEdges),
        engineState: engine.snapshot(),
      };
      redoStackRef.current.push(currentSnapshot);

      const previous = undoStack.pop()!;
      bump();

      return {
        nodes: cloneNodes(previous.nodes),
        edges: cloneEdges(previous.edges),
        engineState: previous.engineState,
      };
    },
    [engine, bump],
  );

  const redo = useCallback(
    (currentNodes: Node[], currentEdges: RFEdge[]): UndoRedoSnapshot | null => {
      const redoStack = redoStackRef.current;
      if (redoStack.length === 0) return null;

      // Save current state onto undo stack before applying redo
      const currentSnapshot: UndoRedoSnapshot = {
        nodes: cloneNodes(currentNodes),
        edges: cloneEdges(currentEdges),
        engineState: engine.snapshot(),
      };
      undoStackRef.current.push(currentSnapshot);

      const next = redoStack.pop()!;
      bump();

      return {
        nodes: cloneNodes(next.nodes),
        edges: cloneEdges(next.edges),
        engineState: next.engineState,
      };
    },
    [engine, bump],
  );

  return {
    pushSnapshot,
    undo,
    redo,
    canUndo: undoStackRef.current.length > 0,
    canRedo: redoStackRef.current.length > 0,
  };
}
