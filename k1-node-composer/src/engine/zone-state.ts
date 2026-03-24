/**
 * Per-zone state management for the dataflow engine.
 *
 * When zone-aware execution is enabled, each stateful node maintains
 * separate state per zone ID. This matches the firmware's per-zone
 * render pattern where effects maintain independent state per zone.
 *
 * Zone IDs map to the firmware's zone system:
 *   0..3 = individual zones
 *   0xFF = global render (no zone)
 *
 * The ZoneStateManager wraps per-zone StateManager instances.
 */

import type { NodeId, NodeState } from './types';
import { StateManager } from './state';

/** Maximum number of zones supported (matches firmware kMaxZones = 4). */
export const MAX_ZONES = 4;

/** Special zone ID for global (non-zone) rendering. */
export const ZONE_GLOBAL = 0xFF;

export class ZoneStateManager {
  /** Per-zone state managers. Index 0..MAX_ZONES-1 for zone states. */
  private zoneStates: StateManager[] = [];

  /** Global (non-zone) state manager — used when zoneId is ZONE_GLOBAL or zone mode is off. */
  private globalState = new StateManager();

  constructor() {
    for (let i = 0; i < MAX_ZONES; i++) {
      this.zoneStates.push(new StateManager());
    }
  }

  /** Get the appropriate state manager for a zone ID. */
  private getManager(zoneId: number): StateManager {
    if (zoneId >= 0 && zoneId < MAX_ZONES) {
      return this.zoneStates[zoneId]!;
    }
    return this.globalState;
  }

  /** Get current state for a node in a specific zone. */
  get(nodeId: NodeId, zoneId: number): NodeState | null {
    return this.getManager(zoneId).get(nodeId);
  }

  /** Update state for a node in a specific zone. */
  set(nodeId: NodeId, zoneId: number, state: NodeState): void {
    this.getManager(zoneId).set(nodeId, state);
  }

  /** Initialise state for a node across all zones. */
  initAll(nodeId: NodeId, factory: () => NodeState): void {
    this.globalState.init(nodeId, factory);
    for (const zoneState of this.zoneStates) {
      zoneState.init(nodeId, factory);
    }
  }

  /** Remove state for a node across all zones. */
  removeAll(nodeId: NodeId): void {
    this.globalState.remove(nodeId);
    for (const zoneState of this.zoneStates) {
      zoneState.remove(nodeId);
    }
  }

  /** Reset all states across all zones. */
  resetAll(factories: Map<NodeId, () => NodeState>): void {
    this.globalState.resetAll(factories);
    for (const zoneState of this.zoneStates) {
      zoneState.resetAll(factories);
    }
  }

  /** Take a snapshot of the global state (for undo/redo compatibility). */
  snapshot(): Map<NodeId, NodeState> {
    return this.globalState.snapshot();
  }

  /** Restore global state from a snapshot (for undo/redo compatibility). */
  restore(snap: Map<NodeId, NodeState>): void {
    this.globalState.restore(snap);
  }
}
