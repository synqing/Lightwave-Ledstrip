/**
 * GraphEngine — pull-based dataflow execution engine.
 *
 * Evaluates the node graph once per frame in topological order.
 * No caching (audio data changes every frame). No lazy evaluation.
 * Matches the firmware's render() call pattern exactly.
 *
 * Supports per-zone execution where stateful nodes maintain
 * separate state per zone ID (matching firmware's kMaxZones = 4).
 */

import type {
  NodeId,
  PortName,
  PortValue,
  Edge,
  NodeInstance,
  NodeDefinition,
  ControlBusFrame,
} from './types';
import { isTypeCompatible } from './types';
import { topoSort } from './topo-sort';
import { broadcast } from './broadcast';
import { StateManager } from './state';
import { ZoneStateManager, ZONE_GLOBAL } from './zone-state';

// ---------------------------------------------------------------------------
// Node registry (type string → definition)
// ---------------------------------------------------------------------------

const nodeRegistry = new Map<string, NodeDefinition>();

export function registerNodeType(def: NodeDefinition): void {
  nodeRegistry.set(def.type, def);
}

export function getNodeDefinition(type: string): NodeDefinition | undefined {
  return nodeRegistry.get(type);
}

export function getRegisteredTypes(): NodeDefinition[] {
  return [...nodeRegistry.values()];
}

// ---------------------------------------------------------------------------
// Graph Engine
// ---------------------------------------------------------------------------

export class GraphEngine {
  private nodes = new Map<NodeId, NodeInstance>();
  private edges: Edge[] = [];
  private sortedIds: NodeId[] | null = null;
  private stateManager = new StateManager();
  private zoneStateManager = new ZoneStateManager();

  /** Whether per-zone state execution is enabled. */
  private _zoneAware = false;

  /** Last tick's output values for each node (for mini-visualisations). */
  private lastOutputs = new Map<NodeId, Map<PortName, PortValue>>();

  // -----------------------------------------------------------------------
  // Zone configuration
  // -----------------------------------------------------------------------

  /** Enable or disable zone-aware execution mode. */
  setZoneAware(enabled: boolean): void {
    this._zoneAware = enabled;
  }

  /** Check if zone-aware execution is enabled. */
  get zoneAware(): boolean {
    return this._zoneAware;
  }

  // -----------------------------------------------------------------------
  // Graph mutation
  // -----------------------------------------------------------------------

  addNode(instance: NodeInstance): void {
    const def = nodeRegistry.get(instance.definitionType);
    if (!def) throw new Error(`Unknown node type: ${instance.definitionType}`);

    // Ensure bypassed flag defaults to false
    if (instance.bypassed === undefined) {
      instance.bypassed = false;
    }

    this.nodes.set(instance.id, instance);

    // Initialise state if stateful
    if (def.hasState && def.getInitialState) {
      this.stateManager.init(instance.id, def.getInitialState);
      this.zoneStateManager.initAll(instance.id, def.getInitialState);
    }

    this.invalidateSort();
  }

  /** Toggle bypass state for a node. Returns the new bypass state. */
  toggleBypass(nodeId: NodeId): boolean {
    const instance = this.nodes.get(nodeId);
    if (!instance) return false;
    instance.bypassed = !instance.bypassed;
    return instance.bypassed;
  }

  /** Check if a node is bypassed. */
  isBypassed(nodeId: NodeId): boolean {
    return this.nodes.get(nodeId)?.bypassed ?? false;
  }

  removeNode(nodeId: NodeId): void {
    this.nodes.delete(nodeId);
    this.stateManager.remove(nodeId);
    this.zoneStateManager.removeAll(nodeId);
    this.edges = this.edges.filter(
      (e) => e.from.nodeId !== nodeId && e.to.nodeId !== nodeId,
    );
    this.lastOutputs.delete(nodeId);
    this.invalidateSort();
  }

  connect(edge: Edge): boolean {
    const fromNode = this.nodes.get(edge.from.nodeId);
    const toNode = this.nodes.get(edge.to.nodeId);
    if (!fromNode || !toNode) return false;

    const fromDef = nodeRegistry.get(fromNode.definitionType);
    const toDef = nodeRegistry.get(toNode.definitionType);
    if (!fromDef || !toDef) return false;

    const fromPort = fromDef.outputs.find((p) => p.name === edge.from.portName);
    const toPort = toDef.inputs.find((p) => p.name === edge.to.portName);
    if (!fromPort || !toPort) return false;

    if (!isTypeCompatible(fromPort.type, toPort.type)) return false;

    // Remove existing connection to this input port (single connection per input)
    this.edges = this.edges.filter(
      (e) => !(e.to.nodeId === edge.to.nodeId && e.to.portName === edge.to.portName),
    );

    this.edges.push(edge);
    this.invalidateSort();
    return true;
  }

  disconnect(edgeId: string): void {
    this.edges = this.edges.filter((e) => e.id !== edgeId);
    this.invalidateSort();
  }

  // -----------------------------------------------------------------------
  // Execution
  // -----------------------------------------------------------------------

  /**
   * Evaluate the entire graph for one frame.
   *
   * @param dt Delta time in seconds.
   * @param controlBus Audio/beat data for this frame.
   * @param zoneId Optional zone ID (0..3). When zone-aware mode is enabled
   *               and zoneId is provided, stateful nodes use per-zone state.
   *               Default: ZONE_GLOBAL (0xFF) — uses global state.
   * @returns Per-node output values (for mini-visualisations and LED preview).
   */
  tick(
    dt: number,
    controlBus: ControlBusFrame | null,
    zoneId: number = ZONE_GLOBAL,
  ): Map<NodeId, Map<PortName, PortValue>> {
    // Rebuild topological sort if invalidated
    if (!this.sortedIds) {
      const nodeIds = new Set(this.nodes.keys());
      this.sortedIds = topoSort(nodeIds, this.edges);
    }

    // Per-node output cache for this frame (used to resolve edge values)
    const frameOutputs = new Map<NodeId, Map<PortName, PortValue>>();

    for (const nodeId of this.sortedIds) {
      const instance = this.nodes.get(nodeId);
      if (!instance) continue;

      const def = nodeRegistry.get(instance.definitionType);
      if (!def) continue;

      // Resolve input values from connected edges (or use defaults)
      const inputValues = new Map<PortName, PortValue>();
      for (const inputPort of def.inputs) {
        const edge = this.edges.find(
          (e) => e.to.nodeId === nodeId && e.to.portName === inputPort.name,
        );

        if (edge) {
          const sourceOutputs = frameOutputs.get(edge.from.nodeId);
          const sourceValue = sourceOutputs?.get(edge.from.portName);
          if (sourceValue !== undefined) {
            // Get source port type for broadcasting
            const sourceNode = this.nodes.get(edge.from.nodeId);
            const sourceDef = sourceNode ? nodeRegistry.get(sourceNode.definitionType) : undefined;
            const sourcePort = sourceDef?.outputs.find((p) => p.name === edge.from.portName);
            const broadcasted = sourcePort
              ? broadcast(sourceValue, sourcePort.type, inputPort.type)
              : sourceValue;
            inputValues.set(inputPort.name, broadcasted);
          } else {
            inputValues.set(inputPort.name, inputPort.defaultValue);
          }
        } else {
          inputValues.set(inputPort.name, inputPort.defaultValue);
        }
      }

      // Get current state — use zone state if zone-aware mode is on
      let currentState: NodeState | null = null;
      if (def.hasState) {
        if (this._zoneAware) {
          currentState = this.zoneStateManager.get(nodeId, zoneId);
        } else {
          currentState = this.stateManager.get(nodeId);
        }
      }

      // Always evaluate — even bypassed nodes compute for ghost visualisation
      const result = def.evaluate(
        inputValues,
        instance.parameters,
        currentState,
        dt,
        controlBus,
      );

      // Sanitise outputs — NaN/Infinity from one node corrupts the entire graph
      for (const [_portName, value] of result.outputs) {
        if (typeof value === 'number' && !Number.isFinite(value)) {
          result.outputs.set(_portName, 0);
        } else if (value instanceof Float32Array) {
          for (let i = 0; i < value.length; i++) {
            if (!Number.isFinite(value[i]!)) value[i] = 0;
          }
        }
      }

      // Update state (even when bypassed — keeps stateful nodes in sync)
      if (def.hasState && result.nextState) {
        if (this._zoneAware) {
          this.zoneStateManager.set(nodeId, zoneId, result.nextState);
        } else {
          this.stateManager.set(nodeId, result.nextState);
        }
      }

      if (instance.bypassed) {
        // BYPASS: pass first input directly to first output.
        // Ghost outputs stored separately for visualisation.
        const ghostOutputs = result.outputs;
        const bypassedOutputs = new Map<PortName, PortValue>();

        // Copy first input to first output (signal passthrough)
        const firstInput = def.inputs[0];
        const firstOutput = def.outputs[0];
        if (firstInput && firstOutput) {
          const passValue = inputValues.get(firstInput.name) ?? firstOutput.defaultValue;
          bypassedOutputs.set(firstOutput.name, passValue);
        }
        // Any additional outputs get their evaluated values (no bypass for secondary outputs)
        for (const outPort of def.outputs.slice(1)) {
          bypassedOutputs.set(outPort.name, ghostOutputs.get(outPort.name) ?? outPort.defaultValue);
        }

        frameOutputs.set(nodeId, bypassedOutputs);
        // Store ghost outputs for viz: prefixed with 'ghost:'
        const ghostMap = new Map<PortName, PortValue>();
        for (const [k, v] of ghostOutputs) {
          ghostMap.set(k, v);
          ghostMap.set(`ghost:${k}`, v); // Ghost version for viz
        }
        for (const [k, v] of bypassedOutputs) {
          ghostMap.set(k, v); // Active (bypassed) version
        }
        frameOutputs.set(nodeId, ghostMap);
      } else {
        // Normal: store evaluated outputs
        frameOutputs.set(nodeId, result.outputs);
      }
    }

    this.lastOutputs = frameOutputs;
    return frameOutputs;
  }

  // -----------------------------------------------------------------------
  // State management
  // -----------------------------------------------------------------------

  reset(): void {
    const factories = new Map<NodeId, () => NodeState>();
    for (const [id, instance] of this.nodes) {
      const def = nodeRegistry.get(instance.definitionType);
      if (def?.hasState && def.getInitialState) {
        factories.set(id, def.getInitialState);
      }
    }
    this.stateManager.resetAll(factories);
    this.zoneStateManager.resetAll(factories);
  }

  snapshot() {
    return this.stateManager.snapshot();
  }

  restore(snap: Map<NodeId, Record<string, unknown>>) {
    this.stateManager.restore(snap);
  }

  // -----------------------------------------------------------------------
  // Queries
  // -----------------------------------------------------------------------

  getLastOutputs(): Map<NodeId, Map<PortName, PortValue>> {
    return this.lastOutputs;
  }

  getNodes(): Map<NodeId, NodeInstance> {
    return this.nodes;
  }

  getEdges(): readonly Edge[] {
    return this.edges;
  }

  getSortedIds(): readonly NodeId[] {
    if (!this.sortedIds) {
      const nodeIds = new Set(this.nodes.keys());
      this.sortedIds = topoSort(nodeIds, this.edges);
    }
    return this.sortedIds;
  }

  // -----------------------------------------------------------------------
  // Internal
  // -----------------------------------------------------------------------

  private invalidateSort(): void {
    this.sortedIds = null;
  }
}

// Import NodeState type for the factories map
import type { NodeState } from './types';
