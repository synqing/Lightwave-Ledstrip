/**
 * State management for the dataflow engine.
 *
 * State lives externally in a Map<NodeId, NodeState>, not inside nodes.
 * This enables reset, snapshot, restore, and C++ export of state variables.
 */

import type { NodeId, NodeState } from './types';

export class StateManager {
  private states = new Map<NodeId, NodeState>();

  /** Get current state for a node (or null if stateless). */
  get(nodeId: NodeId): NodeState | null {
    return this.states.get(nodeId) ?? null;
  }

  /** Update state for a node after evaluation. */
  set(nodeId: NodeId, state: NodeState): void {
    this.states.set(nodeId, state);
  }

  /** Initialise state for a node from its definition's factory. */
  init(nodeId: NodeId, factory: () => NodeState): void {
    this.states.set(nodeId, factory());
  }

  /** Remove state for a deleted node. */
  remove(nodeId: NodeId): void {
    this.states.delete(nodeId);
  }

  /** Reset all states to initial values using provided factories. */
  resetAll(factories: Map<NodeId, () => NodeState>): void {
    this.states.clear();
    for (const [id, factory] of factories) {
      this.states.set(id, factory());
    }
  }

  /** Take a deep snapshot of all state (JSON-serialisable). */
  snapshot(): Map<NodeId, NodeState> {
    const snap = new Map<NodeId, NodeState>();
    for (const [id, state] of this.states) {
      snap.set(id, structuredClone(state));
    }
    return snap;
  }

  /** Restore from a previously taken snapshot. */
  restore(snap: Map<NodeId, NodeState>): void {
    this.states.clear();
    for (const [id, state] of snap) {
      this.states.set(id, structuredClone(state));
    }
  }
}
