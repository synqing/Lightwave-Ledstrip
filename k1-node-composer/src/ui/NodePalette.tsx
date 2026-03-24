/**
 * Side panel listing available node types grouped by layer.
 *
 * Each entry is a click-to-add button that calls the onAddNode callback.
 * Entries are grouped: Sources, Processing, Geometry, Composition, Output.
 */

import { getNodesByLayer } from '../nodes/registry';
import type { NodeDefinition } from '../engine/types';

// ---------------------------------------------------------------------------
// Layer display config
// ---------------------------------------------------------------------------

const LAYER_CONFIG: {
  key: keyof ReturnType<typeof getNodesByLayer>;
  label: string;
  colour: string;
}[] = [
  { key: 'source', label: 'Sources', colour: '#00bcd4' },
  { key: 'processing', label: 'Processing', colour: '#ffc107' },
  { key: 'geometry', label: 'Geometry', colour: '#4caf50' },
  { key: 'composition', label: 'Composition', colour: '#9c27b0' },
  { key: 'output', label: 'Output', colour: '#f44336' },
];

// ---------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------

interface NodePaletteProps {
  onAddNode: (definitionType: string) => void;
}

export function NodePalette({ onAddNode }: NodePaletteProps) {
  const grouped = getNodesByLayer();

  return (
    <div className="node-palette">
      <div className="node-palette__title">Nodes</div>
      {LAYER_CONFIG.map(({ key, label, colour }) => {
        const nodes = grouped[key] as readonly NodeDefinition[];
        if (nodes.length === 0) return null;

        return (
          <div key={key} className="node-palette__group">
            <div
              className="node-palette__group-label"
              style={{ borderLeftColor: colour }}
            >
              {label}
            </div>
            {nodes.map((nodeDef) => (
              <button
                key={nodeDef.type}
                className="node-palette__item"
                onClick={() => onAddNode(nodeDef.type)}
                title={`Add ${nodeDef.label} node`}
              >
                <span
                  className="node-palette__dot"
                  style={{ backgroundColor: colour }}
                />
                {nodeDef.label}
              </button>
            ))}
          </div>
        );
      })}
    </div>
  );
}
