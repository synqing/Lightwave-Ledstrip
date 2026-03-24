/**
 * Kahn's algorithm for topological sorting of the node graph.
 * Returns ordered node IDs (sources first, output last).
 * Throws if a cycle is detected.
 */

import type { NodeId, Edge } from './types';

export function topoSort(nodeIds: ReadonlySet<NodeId>, edges: readonly Edge[]): NodeId[] {
  // Build adjacency and in-degree maps
  const inDegree = new Map<NodeId, number>();
  const adjacency = new Map<NodeId, NodeId[]>();

  for (const id of nodeIds) {
    inDegree.set(id, 0);
    adjacency.set(id, []);
  }

  for (const edge of edges) {
    const fromNode = edge.from.nodeId;
    const toNode = edge.to.nodeId;
    adjacency.get(fromNode)!.push(toNode);
    inDegree.set(toNode, (inDegree.get(toNode) ?? 0) + 1);
  }

  // Seed queue with zero in-degree nodes
  const queue: NodeId[] = [];
  for (const [id, deg] of inDegree) {
    if (deg === 0) queue.push(id);
  }

  const sorted: NodeId[] = [];

  while (queue.length > 0) {
    const current = queue.shift()!;
    sorted.push(current);

    for (const neighbour of adjacency.get(current) ?? []) {
      const newDeg = (inDegree.get(neighbour) ?? 1) - 1;
      inDegree.set(neighbour, newDeg);
      if (newDeg === 0) queue.push(neighbour);
    }
  }

  if (sorted.length !== nodeIds.size) {
    throw new Error(
      `Cycle detected in node graph. Sorted ${sorted.length} of ${nodeIds.size} nodes.`,
    );
  }

  return sorted;
}
