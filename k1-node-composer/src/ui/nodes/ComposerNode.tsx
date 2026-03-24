/**
 * Universal custom React Flow node component for the K1 Node Composer.
 *
 * Reads the NodeDefinition from the registry, renders:
 * - Colour-coded header by layer
 * - Input handles (left side) with port type colour coding
 * - Output handles (right side) with port type colour coding
 * - Parameter sliders inline
 * - Mini-visualisation for each output port
 *
 * Uses React.memo to prevent re-renders from the 60fps engine loop.
 * Viz updates are driven imperatively via refs.
 */

import { memo, useRef, useCallback, useEffect, useContext } from 'react';
import { Handle, Position } from '@xyflow/react';
import type { NodeProps, Node } from '@xyflow/react';
import { getNodeDefinition } from '../../engine/engine';
import { PortType } from '../../engine/types';
import type { PortDefinition, PortValue, PortName, NodeId } from '../../engine/types';
import { ScalarViz } from '../viz/ScalarViz';
import type { ScalarVizHandle } from '../viz/ScalarViz';
import { ArrayViz } from '../viz/ArrayViz';
import type { ArrayVizHandle } from '../viz/ArrayViz';
import { CrgbViz } from '../viz/CrgbViz';
import type { CrgbVizHandle } from '../viz/CrgbViz';
import { BoolViz } from '../viz/BoolViz';
import type { BoolVizHandle } from '../viz/BoolViz';
import { EngineContext } from '../NodeEditor';

// ---------------------------------------------------------------------------
// Colour maps
// ---------------------------------------------------------------------------

const LAYER_COLOURS: Record<string, string> = {
  source: '#00bcd4',
  processing: '#ffc107',
  geometry: '#4caf50',
  composition: '#9c27b0',
  output: '#f44336',
};

const PORT_COLOURS: Record<string, string> = {
  [PortType.SCALAR]: '#aaa',
  [PortType.ARRAY_160]: '#4488ff',
  [PortType.CRGB_160]: '#ff44ff',
  [PortType.ANGLE]: '#ff8800',
  [PortType.BOOL]: '#44ff44',
  [PortType.INT]: '#888',
};

// ---------------------------------------------------------------------------
// Data type carried by each React Flow node
// ---------------------------------------------------------------------------

export type ComposerNodeData = Record<string, unknown> & {
  definitionType: string;
  parameters: Record<string, number>;
  bypassed?: boolean;
  onParamChange?: (nodeId: string, paramName: string, value: number) => void;
  onToggleBypass?: (nodeId: string) => void;
};

type ComposerNodeType = Node<ComposerNodeData, 'composer'>;

// ---------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------

function ComposerNodeInner({ id, data }: NodeProps<ComposerNodeType>) {
  const def = getNodeDefinition(data.definitionType);
  const engineCtx = useContext(EngineContext);

  // Viz refs per output port
  const vizRefsMap = useRef<Map<string, ScalarVizHandle | ArrayVizHandle | CrgbVizHandle | BoolVizHandle>>(new Map());

  // Subscribe to engine outputs for imperative viz updates
  useEffect(() => {
    if (!engineCtx) return;

    const unsub = engineCtx.subscribe((outputs) => {
      const nodeOutputs = outputs.get(id);
      if (!nodeOutputs || !def) return;

      for (const outPort of def.outputs) {
        const vizRef = vizRefsMap.current.get(outPort.name);
        const value = nodeOutputs.get(outPort.name);
        if (!vizRef || value === undefined) continue;

        switch (outPort.type) {
          case PortType.SCALAR:
          case PortType.ANGLE:
          case PortType.INT:
            (vizRef as ScalarVizHandle).update(value as number);
            break;
          case PortType.ARRAY_160:
            (vizRef as ArrayVizHandle).update(value as Float32Array);
            break;
          case PortType.CRGB_160:
            (vizRef as CrgbVizHandle).update(value as Uint8Array);
            break;
          case PortType.BOOL:
            (vizRef as BoolVizHandle).update(value as boolean);
            break;
        }
      }
    });

    return unsub;
  }, [engineCtx, id, def]);

  if (!def) {
    return (
      <div className="composer-node composer-node--error">
        <div className="composer-node__header" style={{ backgroundColor: '#f44' }}>
          Unknown: {data.definitionType}
        </div>
      </div>
    );
  }

  const headerColour = LAYER_COLOURS[def.layer] ?? '#666';
  const isBypassed = data.bypassed ?? false;

  const handleBypassClick = useCallback((e: React.MouseEvent) => {
    e.stopPropagation();
    data.onToggleBypass?.(id);
  }, [id, data]);

  return (
    <div className={`composer-node ${isBypassed ? 'composer-node--bypassed' : ''}`}>
      {/* Header */}
      <div
        className="composer-node__header"
        style={{ backgroundColor: isBypassed ? '#444' : headerColour }}
      >
        <span className="composer-node__header-label">{def.label}</span>
        {/* Bypass toggle — source and output nodes can't be bypassed */}
        {def.layer !== 'source' && def.layer !== 'output' && (
          <button
            className={`composer-node__bypass-btn ${isBypassed ? 'composer-node__bypass-btn--active' : ''}`}
            onClick={handleBypassClick}
            title={isBypassed ? 'Bypassed — click to re-enable' : 'Click to bypass'}
          >
            BP
          </button>
        )}
      </div>

      {/* Body */}
      <div className="composer-node__body">
        {/* Input ports */}
        {def.inputs.map((port, i) => (
          <InputPort key={port.name} port={port} index={i} totalInputs={def.inputs.length} />
        ))}

        {/* Parameters */}
        {def.parameters.map((param) => (
          <ParamSlider
            key={param.name}
            nodeId={id}
            paramName={param.name}
            label={param.label}
            min={param.min}
            max={param.max}
            step={param.step}
            value={data.parameters[param.name] ?? param.defaultValue}
            onChange={data.onParamChange}
          />
        ))}

        {/* Output ports with viz */}
        {def.outputs.map((port, i) => (
          <OutputPort
            key={port.name}
            port={port}
            index={i}
            totalOutputs={def.outputs.length}
            vizRefsMap={vizRefsMap}
          />
        ))}
      </div>
    </div>
  );
}

export const ComposerNode = memo(ComposerNodeInner);

// ---------------------------------------------------------------------------
// Sub-components
// ---------------------------------------------------------------------------

function InputPort({ port, index, totalInputs }: {
  port: PortDefinition;
  index: number;
  totalInputs: number;
}) {
  const colour = PORT_COLOURS[port.type] ?? '#aaa';
  // Spread handles vertically within the body area
  const topPercent = totalInputs === 1
    ? 50
    : 30 + (index / (totalInputs - 1)) * 40;

  return (
    <div className="composer-node__port composer-node__port--input">
      <Handle
        type="target"
        position={Position.Left}
        id={port.name}
        className="composer-handle"
        style={{
          backgroundColor: colour,
          top: `${topPercent}%`,
        }}
      />
      <span className="composer-node__port-label composer-node__port-label--left">
        {port.name}
      </span>
    </div>
  );
}

function OutputPort({ port, index, totalOutputs, vizRefsMap }: {
  port: PortDefinition;
  index: number;
  totalOutputs: number;
  vizRefsMap: React.MutableRefObject<Map<string, ScalarVizHandle | ArrayVizHandle | CrgbVizHandle | BoolVizHandle>>;
}) {
  const colour = PORT_COLOURS[port.type] ?? '#aaa';
  const topPercent = totalOutputs === 1
    ? 50
    : 30 + (index / (totalOutputs - 1)) * 40;

  const setVizRef = useCallback((handle: ScalarVizHandle | ArrayVizHandle | CrgbVizHandle | BoolVizHandle | null) => {
    if (handle) {
      vizRefsMap.current.set(port.name, handle);
    } else {
      vizRefsMap.current.delete(port.name);
    }
  }, [vizRefsMap, port.name]);

  return (
    <div className="composer-node__port composer-node__port--output">
      <div className="composer-node__viz-container">
        <VizForType type={port.type} ref={setVizRef} />
      </div>
      <span className="composer-node__port-label composer-node__port-label--right">
        {port.name}
      </span>
      <Handle
        type="source"
        position={Position.Right}
        id={port.name}
        className="composer-handle"
        style={{
          backgroundColor: colour,
          top: `${topPercent}%`,
        }}
      />
    </div>
  );
}

// ---------------------------------------------------------------------------
// Viz factory
// ---------------------------------------------------------------------------

import { forwardRef } from 'react';

type AnyVizHandle = ScalarVizHandle | ArrayVizHandle | CrgbVizHandle | BoolVizHandle;

const VizForType = forwardRef<AnyVizHandle, { type: PortType }>(
  function VizForType({ type }, ref) {
    switch (type) {
      case PortType.SCALAR:
      case PortType.ANGLE:
      case PortType.INT:
        return <ScalarViz ref={ref as React.Ref<ScalarVizHandle>} />;
      case PortType.ARRAY_160:
        return <ArrayViz ref={ref as React.Ref<ArrayVizHandle>} />;
      case PortType.CRGB_160:
        return <CrgbViz ref={ref as React.Ref<CrgbVizHandle>} />;
      case PortType.BOOL:
        return <BoolViz ref={ref as React.Ref<BoolVizHandle>} />;
      default:
        return null;
    }
  },
);

// ---------------------------------------------------------------------------
// Parameter slider
// ---------------------------------------------------------------------------

function ParamSlider({ nodeId, paramName, label, min, max, step, value, onChange }: {
  nodeId: string;
  paramName: string;
  label: string;
  min: number;
  max: number;
  step: number;
  value: number;
  onChange?: (nodeId: string, paramName: string, value: number) => void;
}) {
  const handleChange = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
    const newValue = parseFloat(e.target.value);
    onChange?.(nodeId, paramName, newValue);
  }, [nodeId, paramName, onChange]);

  return (
    <div className="composer-node__param">
      <label className="composer-node__param-label">
        <span>{label}</span>
        <span className="composer-node__param-value">{value.toFixed(2)}</span>
      </label>
      <input
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        onChange={handleChange}
        className="composer-node__param-slider nodrag"
      />
    </div>
  );
}

// Re-export types used by NodeEditor
export type { NodeId, PortName, PortValue };
