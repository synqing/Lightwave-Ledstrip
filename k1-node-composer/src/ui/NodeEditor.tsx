/**
 * Main React Flow canvas for the K1 Node Composer.
 *
 * Registers ComposerNode as the custom node type.
 * Handles connections (with type validation), node deletion,
 * and click-to-add from the NodePalette.
 *
 * Pre-loads a demo graph:
 *   RMS -> EMA Smooth -> Scale -> Gaussian -> HSV->RGB -> LED Output
 */

import { useCallback, useEffect, createContext, useRef } from 'react';
import {
  ReactFlow,
  Background,
  Controls,
  useNodesState,
  useEdgesState,
  addEdge,
  MarkerType,
} from '@xyflow/react';
import type {
  Node,
  Edge as RFEdge,
  Connection,
  OnConnect,
  NodeTypes,
} from '@xyflow/react';
import '@xyflow/react/dist/style.css';

import { ComposerNode } from './nodes/ComposerNode';
import type { ComposerNodeData } from './nodes/ComposerNode';
import { getNodeDefinition } from '../engine/engine';
import { isTypeCompatible } from '../engine/types';
import type { OutputSubscriber } from './hooks/useGraphEngine';
import type { GraphEngineHandle } from './hooks/useGraphEngine';
import type { GraphPreset } from '../presets';

// ---------------------------------------------------------------------------
// Engine context — provides subscribe() to ComposerNode viz components
// ---------------------------------------------------------------------------

export interface EngineContextValue {
  subscribe: (callback: OutputSubscriber) => () => void;
}

export const EngineContext = createContext<EngineContextValue | null>(null);

// ---------------------------------------------------------------------------
// Node type registry for React Flow
// ---------------------------------------------------------------------------

const nodeTypes: NodeTypes = {
  composer: ComposerNode,
};

// ---------------------------------------------------------------------------
// Demo graph factory
// ---------------------------------------------------------------------------

type ComposerNode_RF = Node<ComposerNodeData, 'composer'>;

function createDemoGraph(
  engineHandle: GraphEngineHandle,
  onParamChange: (nodeId: string, paramName: string, value: number) => void,
  onToggleBypass?: (nodeId: string) => void,
): { nodes: ComposerNode_RF[]; edges: RFEdge[] } {
  const demoNodes: { type: string; id: string; x: number; y: number }[] = [
    { type: 'rms-source',  id: 'demo-rms',      x: 50,  y: 200 },
    { type: 'ema-smooth',  id: 'demo-ema',       x: 300, y: 200 },
    { type: 'scale',       id: 'demo-scale',     x: 550, y: 200 },
    { type: 'gaussian',    id: 'demo-gaussian',  x: 800, y: 200 },
    { type: 'hsv-to-rgb',  id: 'demo-hsv',       x: 1050, y: 200 },
    { type: 'led-output',  id: 'demo-led-out',   x: 1350, y: 200 },
  ];

  const rfNodes: ComposerNode_RF[] = [];

  for (const entry of demoNodes) {
    const def = getNodeDefinition(entry.type);
    if (!def) continue;

    // Build parameters map
    const params = new Map<string, number>();
    const paramsRecord: Record<string, number> = {};
    for (const p of def.parameters) {
      params.set(p.name, p.defaultValue);
      paramsRecord[p.name] = p.defaultValue;
    }

    // Add to engine
    engineHandle.addNode({
      id: entry.id,
      definitionType: entry.type,
      parameters: params,
      position: { x: entry.x, y: entry.y },
    });

    // Create React Flow node
    rfNodes.push({
      id: entry.id,
      type: 'composer',
      position: { x: entry.x, y: entry.y },
      data: {
        definitionType: entry.type,
        parameters: paramsRecord,
        onParamChange,
        onToggleBypass,
      },
    });
  }

  // Define connections for the demo chain
  const demoEdges: { from: string; fromPort: string; to: string; toPort: string }[] = [
    { from: 'demo-rms',      fromPort: 'out',   to: 'demo-ema',      toPort: 'value' },
    { from: 'demo-ema',      fromPort: 'out',   to: 'demo-scale',    toPort: 'value' },
    { from: 'demo-scale',    fromPort: 'out',   to: 'demo-gaussian', toPort: 'amplitude' },
    { from: 'demo-gaussian', fromPort: 'out',   to: 'demo-hsv',      toPort: 'value' },
    { from: 'demo-hsv',      fromPort: 'out',   to: 'demo-led-out',  toPort: 'colour' },
  ];

  const rfEdges: RFEdge[] = [];

  for (const e of demoEdges) {
    const edgeId = `${e.from}:${e.fromPort}->${e.to}:${e.toPort}`;

    engineHandle.connect({
      id: edgeId,
      from: { nodeId: e.from, portName: e.fromPort },
      to: { nodeId: e.to, portName: e.toPort },
    });

    rfEdges.push({
      id: edgeId,
      source: e.from,
      sourceHandle: e.fromPort,
      target: e.to,
      targetHandle: e.toPort,
      markerEnd: { type: MarkerType.ArrowClosed, width: 12, height: 12 },
      style: { strokeWidth: 2 },
    });
  }

  return { nodes: rfNodes, edges: rfEdges };
}

// ---------------------------------------------------------------------------
// NodeEditor component
// ---------------------------------------------------------------------------

interface NodeEditorProps {
  engineHandle: GraphEngineHandle;
  /** Called before every graph mutation so the caller can snapshot for undo. */
  onBeforeMutation?: () => void;
}

export function NodeEditor({ engineHandle, onBeforeMutation }: NodeEditorProps) {
  const [nodes, setNodes, onNodesChange] = useNodesState<ComposerNode_RF>([]);
  const [edges, setEdges, onEdgesChange] = useEdgesState<RFEdge>([]);
  const nodeCounterRef = useRef(0);
  const initialisedRef = useRef(false);

  // Param change handler — updates both engine and React Flow node data
  const onParamChange = useCallback((nodeId: string, paramName: string, value: number) => {
    onBeforeMutation?.();

    // Update engine
    const engineNode = engineHandle.engine.getNodes().get(nodeId);
    if (engineNode) {
      engineNode.parameters.set(paramName, value);
    }

    // Update React Flow node data
    setNodes((nds) =>
      nds.map((n) => {
        if (n.id === nodeId) {
          return {
            ...n,
            data: {
              ...n.data,
              parameters: {
                ...n.data.parameters,
                [paramName]: value,
              },
            },
          };
        }
        return n;
      }),
    );
  }, [engineHandle, setNodes, onBeforeMutation]);

  // Bypass toggle handler — updates both engine and React Flow node data
  const onToggleBypass = useCallback((nodeId: string) => {
    const newState = engineHandle.toggleBypass(nodeId);

    setNodes((nds) =>
      nds.map((n) => {
        if (n.id === nodeId) {
          return {
            ...n,
            data: {
              ...n.data,
              bypassed: newState,
            },
          };
        }
        return n;
      }),
    );
  }, [engineHandle, setNodes]);

  // Load demo graph on first render
  useEffect(() => {
    if (initialisedRef.current) return;
    initialisedRef.current = true;

    const demo = createDemoGraph(engineHandle, onParamChange, onToggleBypass);
    setNodes(demo.nodes);
    setEdges(demo.edges);
    nodeCounterRef.current = demo.nodes.length;
  }, [engineHandle, onParamChange, setNodes, setEdges]);

  // Connection handler with type validation
  const onConnect: OnConnect = useCallback((connection: Connection) => {
    const { source, sourceHandle, target, targetHandle } = connection;
    if (!source || !target || !sourceHandle || !targetHandle) return;

    // Find port types from definitions
    const sourceNode = engineHandle.engine.getNodes().get(source);
    const targetNode = engineHandle.engine.getNodes().get(target);
    if (!sourceNode || !targetNode) return;

    const sourceDef = getNodeDefinition(sourceNode.definitionType);
    const targetDef = getNodeDefinition(targetNode.definitionType);
    if (!sourceDef || !targetDef) return;

    const sourcePort = sourceDef.outputs.find((p) => p.name === sourceHandle);
    const targetPort = targetDef.inputs.find((p) => p.name === targetHandle);
    if (!sourcePort || !targetPort) return;

    if (!isTypeCompatible(sourcePort.type, targetPort.type)) {
      console.warn(
        `[NodeEditor] Type mismatch: ${sourcePort.type} cannot connect to ${targetPort.type}`,
      );
      return;
    }

    const edgeId = `${source}:${sourceHandle}->${target}:${targetHandle}`;

    onBeforeMutation?.();

    const connected = engineHandle.connect({
      id: edgeId,
      from: { nodeId: source, portName: sourceHandle },
      to: { nodeId: target, portName: targetHandle },
    });

    if (!connected) {
      console.warn('[NodeEditor] Engine rejected connection');
      return;
    }

    setEdges((eds) =>
      addEdge(
        {
          ...connection,
          id: edgeId,
          markerEnd: { type: MarkerType.ArrowClosed, width: 12, height: 12 },
          style: { strokeWidth: 2 },
        },
        eds,
      ),
    );
  }, [engineHandle, setEdges, onBeforeMutation]);

  // Node deletion
  const onNodesDelete = useCallback((deleted: ComposerNode_RF[]) => {
    onBeforeMutation?.();
    for (const node of deleted) {
      engineHandle.removeNode(node.id);
    }
  }, [engineHandle, onBeforeMutation]);

  // Edge deletion
  const onEdgesDelete = useCallback((deleted: RFEdge[]) => {
    onBeforeMutation?.();
    for (const edge of deleted) {
      engineHandle.disconnect(edge.id);
    }
  }, [engineHandle, onBeforeMutation]);

  // Add node from palette
  const addNodeFromPalette = useCallback((definitionType: string) => {
    const def = getNodeDefinition(definitionType);
    if (!def) return;

    onBeforeMutation?.();

    const id = `node-${Date.now()}-${nodeCounterRef.current++}`;
    const position = { x: 200 + Math.random() * 400, y: 100 + Math.random() * 300 };

    const params = new Map<string, number>();
    const paramsRecord: Record<string, number> = {};
    for (const p of def.parameters) {
      params.set(p.name, p.defaultValue);
      paramsRecord[p.name] = p.defaultValue;
    }

    engineHandle.addNode({
      id,
      definitionType,
      parameters: params,
      position,
    });

    const newNode: ComposerNode_RF = {
      id,
      type: 'composer',
      position,
      data: {
        definitionType,
        parameters: paramsRecord,
        onParamChange,
        onToggleBypass,
      },
    };

    setNodes((nds) => [...nds, newNode]);
  }, [engineHandle, onParamChange, setNodes, onBeforeMutation]);

  // Load a preset graph — clears the current graph and replaces it entirely
  const loadPreset = useCallback((preset: GraphPreset) => {
    onBeforeMutation?.();

    // 1. Clear all existing nodes from the engine (edges removed automatically)
    const existingNodeIds = [...engineHandle.engine.getNodes().keys()];
    for (const nodeId of existingNodeIds) {
      engineHandle.removeNode(nodeId);
    }

    // 2. Reset engine state
    engineHandle.reset();

    const rfNodes: ComposerNode_RF[] = [];
    const rfEdges: RFEdge[] = [];

    // 3. Create new nodes from preset
    const stateOverrides = new Map<string, Record<string, unknown>>();

    for (const presetNode of preset.nodes) {
      const def = getNodeDefinition(presetNode.definitionType);
      if (!def) {
        console.warn(
          `[NodeEditor] Preset "${preset.name}": unknown node type "${presetNode.definitionType}", skipping`,
        );
        continue;
      }

      // Build parameters map — start with defaults, then apply preset overrides
      const params = new Map<string, number>();
      const paramsRecord: Record<string, number> = {};
      for (const p of def.parameters) {
        const presetValue = presetNode.parameters[p.name];
        const value = presetValue !== undefined ? presetValue : p.defaultValue;
        params.set(p.name, value);
        paramsRecord[p.name] = value;
      }

      engineHandle.addNode({
        id: presetNode.id,
        definitionType: presetNode.definitionType,
        parameters: params,
        position: presetNode.position,
      });

      // Collect state overrides for stateful nodes (e.g. code-block code)
      if (presetNode.initialState && def.hasState) {
        stateOverrides.set(presetNode.id, presetNode.initialState);
      }

      rfNodes.push({
        id: presetNode.id,
        type: 'composer',
        position: presetNode.position,
        data: {
          definitionType: presetNode.definitionType,
          parameters: paramsRecord,
          onParamChange,
        onToggleBypass,
        },
      });
    }

    // 3b. Apply initial state overrides (e.g. code-block code strings)
    if (stateOverrides.size > 0) {
      const currentSnap = engineHandle.engine.snapshot();
      for (const [nodeId, overrides] of stateOverrides) {
        const existing = currentSnap.get(nodeId) ?? {};
        currentSnap.set(nodeId, { ...existing, ...overrides });
      }
      engineHandle.engine.restore(currentSnap);
    }

    // 4. Create edges from preset
    for (const presetEdge of preset.edges) {
      const connected = engineHandle.connect({
        id: presetEdge.id,
        from: presetEdge.from,
        to: presetEdge.to,
      });

      if (!connected) {
        console.warn(
          `[NodeEditor] Preset "${preset.name}": engine rejected edge "${presetEdge.id}"`,
        );
        continue;
      }

      rfEdges.push({
        id: presetEdge.id,
        source: presetEdge.from.nodeId,
        sourceHandle: presetEdge.from.portName,
        target: presetEdge.to.nodeId,
        targetHandle: presetEdge.to.portName,
        markerEnd: { type: MarkerType.ArrowClosed, width: 12, height: 12 },
        style: { strokeWidth: 2 },
      });
    }

    // 5. Update React Flow state
    setNodes(rfNodes);
    setEdges(rfEdges);
    nodeCounterRef.current = rfNodes.length;
  }, [engineHandle, onParamChange, setNodes, setEdges, onBeforeMutation]);

  // Edge colour based on source port type
  const getEdgeStyle = useCallback((_source: string, _sourceHandle: string) => {
    return { stroke: '#4488ff', strokeWidth: 2 };
  }, []);

  // Accessors for undo/redo — return current nodes/edges arrays
  const getNodes = useCallback((): Node[] => nodes, [nodes]);
  const getEdges = useCallback((): RFEdge[] => edges, [edges]);

  /**
   * Apply a snapshot from undo/redo.
   *
   * Rebuilds the engine graph from scratch (clear → add nodes → connect edges)
   * then restores engine state and React Flow visual state. The onParamChange
   * callback is re-attached to each restored node's data.
   */
  const applySnapshot = useCallback(
    (snapshotNodes: Node[], snapshotEdges: RFEdge[], engineState: Map<string, Record<string, unknown>>) => {
      // 1. Clear the engine
      const existingNodeIds = [...engineHandle.engine.getNodes().keys()];
      for (const nodeId of existingNodeIds) {
        engineHandle.removeNode(nodeId);
      }

      // 2. Re-add nodes to the engine from snapshot
      for (const rfNode of snapshotNodes) {
        const defType = (rfNode.data as ComposerNodeData).definitionType;
        const paramsRecord = (rfNode.data as ComposerNodeData).parameters;
        const def = getNodeDefinition(defType);
        if (!def) continue;

        const params = new Map<string, number>();
        for (const [key, val] of Object.entries(paramsRecord)) {
          params.set(key, val);
        }

        engineHandle.addNode({
          id: rfNode.id,
          definitionType: defType,
          parameters: params,
          position: { ...rfNode.position },
        });
      }

      // 3. Re-connect edges in the engine
      for (const rfEdge of snapshotEdges) {
        if (rfEdge.source && rfEdge.sourceHandle && rfEdge.target && rfEdge.targetHandle) {
          engineHandle.connect({
            id: rfEdge.id,
            from: { nodeId: rfEdge.source, portName: rfEdge.sourceHandle },
            to: { nodeId: rfEdge.target, portName: rfEdge.targetHandle },
          });
        }
      }

      // 4. Restore engine state (stateful node internals like EMA accumulators)
      engineHandle.engine.restore(engineState);

      // 5. Re-attach onParamChange callback and update React Flow state
      const restoredNodes = snapshotNodes.map((n) => ({
        ...n,
        data: {
          ...(n.data as ComposerNodeData),
          onParamChange,
        onToggleBypass,
        },
      })) as ComposerNode_RF[];

      setNodes(restoredNodes);
      setEdges([...snapshotEdges]);
    },
    [engineHandle, onParamChange, setNodes, setEdges],
  );

  return {
    canvas: (
      <EngineContext.Provider value={{ subscribe: engineHandle.subscribe }}>
        <ReactFlow
          nodes={nodes}
          edges={edges}
          onNodesChange={onNodesChange}
          onEdgesChange={onEdgesChange}
          onConnect={onConnect}
          onNodesDelete={onNodesDelete}
          onEdgesDelete={onEdgesDelete}
          nodeTypes={nodeTypes}
          fitView
          minZoom={0.2}
          maxZoom={2}
          defaultEdgeOptions={{
            style: { strokeWidth: 2, stroke: '#4488ff' },
            markerEnd: { type: MarkerType.ArrowClosed, width: 12, height: 12 },
          }}
          proOptions={{ hideAttribution: true }}
        >
          <Background color="#222" gap={20} />
          <Controls position="bottom-left" showInteractive={false} />
        </ReactFlow>
      </EngineContext.Provider>
    ),
    addNodeFromPalette,
    loadPreset,
    getEdgeStyle,
    getNodes,
    getEdges,
    applySnapshot,
  };
}
