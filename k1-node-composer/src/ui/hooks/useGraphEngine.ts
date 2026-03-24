/**
 * React hook wrapping the GraphEngine for React Flow integration.
 *
 * Runs a requestAnimationFrame loop at 60fps calling engine.tick().
 * Outputs are stored in a ref (not state) to avoid re-rendering React
 * on every frame. Consumers that need to paint on output changes
 * subscribe via the subscribe() method.
 *
 * Supports zone-aware execution mode where the engine ticks with
 * a configurable zoneId for per-zone state isolation.
 */

import { useRef, useCallback, useEffect } from 'react';
import { GraphEngine, getNodeDefinition, getRegisteredTypes } from '../../engine/engine';
import { ZONE_GLOBAL } from '../../engine/zone-state';
import { registerAllNodes } from '../../nodes/registry';
import { isTypeCompatible } from '../../engine/types';
import type {
  NodeId,
  PortName,
  PortValue,
  NodeInstance,
  Edge,
  ControlBusFrame,
} from '../../engine/types';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export type OutputSnapshot = Map<NodeId, Map<PortName, PortValue>>;
export type OutputSubscriber = (outputs: OutputSnapshot) => void;

export interface GraphEngineHandle {
  engine: GraphEngine;
  nodeOutputs: React.MutableRefObject<OutputSnapshot>;
  addNode: (instance: NodeInstance) => void;
  removeNode: (nodeId: NodeId) => void;
  connect: (edge: Edge) => boolean;
  disconnect: (edgeId: string) => void;
  toggleBypass: (nodeId: NodeId) => boolean;
  reset: () => void;
  subscribe: (callback: OutputSubscriber) => () => void;
  setAudioProvider: (provider: (() => ControlBusFrame | null) | null) => void;
  setZoneAware: (enabled: boolean) => void;
  setZoneId: (zoneId: number) => void;
  getNodeDefinition: typeof getNodeDefinition;
  getRegisteredTypes: typeof getRegisteredTypes;
}

// ---------------------------------------------------------------------------
// Hook
// ---------------------------------------------------------------------------

let registered = false;

export function useGraphEngine(): GraphEngineHandle {
  const engineRef = useRef<GraphEngine | null>(null);
  const outputsRef = useRef<OutputSnapshot>(new Map());
  const subscribersRef = useRef<Set<OutputSubscriber>>(new Set());
  const audioProviderRef = useRef<(() => ControlBusFrame | null) | null>(null);
  const rafIdRef = useRef<number>(0);
  const lastTimeRef = useRef<number>(0);
  const zoneIdRef = useRef<number>(ZONE_GLOBAL);

  // Lazily create engine singleton
  if (!engineRef.current) {
    if (!registered) {
      registerAllNodes();
      registered = true;
    }
    engineRef.current = new GraphEngine();
  }

  const engine = engineRef.current;

  // Subscribe to output changes (for viz components)
  const subscribe = useCallback((callback: OutputSubscriber): (() => void) => {
    subscribersRef.current.add(callback);
    return () => {
      subscribersRef.current.delete(callback);
    };
  }, []);

  // Graph mutation wrappers
  const addNode = useCallback((instance: NodeInstance) => {
    engine.addNode(instance);
  }, [engine]);

  const removeNode = useCallback((nodeId: NodeId) => {
    engine.removeNode(nodeId);
  }, [engine]);

  const connect = useCallback((edge: Edge): boolean => {
    return engine.connect(edge);
  }, [engine]);

  const disconnect = useCallback((edgeId: string) => {
    engine.disconnect(edgeId);
  }, [engine]);

  const toggleBypass = useCallback((nodeId: NodeId): boolean => {
    return engine.toggleBypass(nodeId);
  }, [engine]);

  const reset = useCallback(() => {
    engine.reset();
  }, [engine]);

  const setAudioProvider = useCallback((provider: (() => ControlBusFrame | null) | null) => {
    audioProviderRef.current = provider;
  }, []);

  const setZoneAware = useCallback((enabled: boolean) => {
    engine.setZoneAware(enabled);
  }, [engine]);

  const setZoneId = useCallback((zoneId: number) => {
    zoneIdRef.current = zoneId;
  }, []);

  // rAF loop: tick engine at ~60fps
  useEffect(() => {
    const loop = (timestamp: number) => {
      if (lastTimeRef.current === 0) {
        lastTimeRef.current = timestamp;
      }

      const dtMs = timestamp - lastTimeRef.current;
      lastTimeRef.current = timestamp;
      const dt = Math.min(dtMs / 1000, 0.1); // Cap at 100ms to prevent spiral

      const controlBus = audioProviderRef.current ? audioProviderRef.current() : null;
      const outputs = engine.tick(dt, controlBus, zoneIdRef.current);

      outputsRef.current = outputs;

      // Notify subscribers (viz components)
      for (const sub of subscribersRef.current) {
        sub(outputs);
      }

      rafIdRef.current = requestAnimationFrame(loop);
    };

    rafIdRef.current = requestAnimationFrame(loop);

    return () => {
      if (rafIdRef.current) {
        cancelAnimationFrame(rafIdRef.current);
      }
    };
  }, [engine]);

  return {
    engine,
    nodeOutputs: outputsRef,
    addNode,
    removeNode,
    connect,
    disconnect,
    toggleBypass,
    reset,
    subscribe,
    setAudioProvider,
    setZoneAware,
    setZoneId,
    getNodeDefinition,
    getRegisteredTypes,
  };
}

// Re-export for convenience
export { isTypeCompatible };
