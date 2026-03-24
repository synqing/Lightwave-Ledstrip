/**
 * C++ code generator for the K1 Node Composer.
 *
 * Takes a GraphEngine with a connected node graph and produces
 * a complete IEffect subclass (header file) that compiles against
 * the LightwaveOS firmware.
 *
 * Constraints enforced in generated code:
 *   - Centre-origin rendering via SET_CENTER_PAIR
 *   - No heap allocation in render()
 *   - bands[] for audio access (backend-agnostic)
 *   - British English in comments
 */

import type { GraphEngine } from '../engine/engine';
import { getNodeDefinition } from '../engine/engine';
import type { NodeId, PortName, Edge } from '../engine/types';
import { makeVarName, makeMemberName } from './variable-namer';
import {
  templateRms,
  templateBand,
  templateBeatStrength,
  templateEmaSmooth,
  templateMaxFollower,
  templateScale,
  templateGaussian,
  templateTriangularWave,
  templateHsvToRgb,
  templateLedOutput,
  templateEffectShell,
} from './templates';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Sanitise an effect name into a valid C++ class name. */
function toClassName(name: string): string {
  const sanitised = name
    .replace(/[^a-zA-Z0-9\s]/g, '')
    .split(/\s+/)
    .map((word) => word.charAt(0).toUpperCase() + word.slice(1).toLowerCase())
    .join('');
  return sanitised + 'Effect';
}

/** Find the edge feeding a specific input port on a node. */
function findInputEdge(
  edges: readonly Edge[],
  nodeId: NodeId,
  portName: PortName,
): Edge | undefined {
  return edges.find(
    (e) => e.to.nodeId === nodeId && e.to.portName === portName,
  );
}

/** Look up the variable name for a connected input, or return a literal fallback. */
function resolveInputVar(
  edges: readonly Edge[],
  nodeId: NodeId,
  portName: PortName,
  nodeIndexMap: Map<NodeId, number>,
  fallback: string,
): string {
  const edge = findInputEdge(edges, nodeId, portName);
  if (!edge) return fallback;

  const sourceIndex = nodeIndexMap.get(edge.from.nodeId);
  if (sourceIndex === undefined) return fallback;

  return makeVarName(edge.from.nodeId, edge.from.portName, sourceIndex);
}

// ---------------------------------------------------------------------------
// Main generator
// ---------------------------------------------------------------------------

export interface GeneratedCode {
  header: string;
  source: string;
}

/**
 * Generate a complete C++ IEffect header from the current node graph.
 *
 * Returns { header, source } where header contains the full class
 * and source is empty (header-only for POC simplicity).
 */
export function generateCpp(
  engine: GraphEngine,
  effectName: string,
): GeneratedCode {
  const sortedIds = engine.getSortedIds();
  const nodes = engine.getNodes();
  const edges = engine.getEdges();

  // Build index map: nodeId => topo-sort index
  const nodeIndexMap = new Map<NodeId, number>();
  sortedIds.forEach((id, index) => {
    nodeIndexMap.set(id, index);
  });

  const memberLines: string[] = [];
  const initLines: string[] = [];
  const renderLines: string[] = [];

  // Walk topo order, emit C++ code per node
  for (const nodeId of sortedIds) {
    const instance = nodes.get(nodeId);
    if (!instance) continue;

    const def = getNodeDefinition(instance.definitionType);
    if (!def) continue;

    const index = nodeIndexMap.get(nodeId) ?? 0;

    switch (def.type) {
      // -------------------------------------------------------------------
      // Sources
      // -------------------------------------------------------------------
      case 'rms-source': {
        const varName = makeVarName(nodeId, 'out', index);
        renderLines.push(templateRms(varName));
        break;
      }

      case 'band-source': {
        const varName = makeVarName(nodeId, 'out', index);
        const bandIndex = Math.round(instance.parameters.get('index') ?? 0);
        renderLines.push(templateBand(varName, bandIndex));
        break;
      }

      case 'beat-strength-source': {
        const varName = makeVarName(nodeId, 'out', index);
        renderLines.push(templateBeatStrength(varName));
        break;
      }

      // -------------------------------------------------------------------
      // Processing
      // -------------------------------------------------------------------
      case 'ema-smooth': {
        const varName = makeVarName(nodeId, 'out', index);
        const memberVar = makeMemberName(def.type, index, 'smoothed');
        const tau = instance.parameters.get('tau') ?? 0.05;

        const inputVar = resolveInputVar(
          edges, nodeId, 'value', nodeIndexMap, '0.0f',
        );

        memberLines.push(`    float ${memberVar} = 0.0f;`);
        initLines.push(`        ${memberVar} = 0.0f;`);
        renderLines.push(templateEmaSmooth(varName, inputVar, memberVar, tau));
        break;
      }

      case 'max-follower': {
        const normalisedVar = makeVarName(nodeId, 'normalised', index);
        const followerVar = makeVarName(nodeId, 'follower', index);
        const memberFollower = makeMemberName(def.type, index, 'follower');
        const attackTau = instance.parameters.get('attackTau') ?? 0.058;
        const decayTau = instance.parameters.get('decayTau') ?? 0.50;
        const floor = instance.parameters.get('floor') ?? 0.04;

        const inputVar = resolveInputVar(
          edges, nodeId, 'value', nodeIndexMap, '0.0f',
        );

        memberLines.push(`    float ${memberFollower} = 0.15f;`);
        initLines.push(`        ${memberFollower} = 0.15f;`);
        renderLines.push(templateMaxFollower(
          normalisedVar, followerVar, inputVar,
          memberFollower, attackTau, decayTau, floor,
        ));
        break;
      }

      case 'scale': {
        const varName = makeVarName(nodeId, 'out', index);
        const factor = instance.parameters.get('factor') ?? 1.0;

        const inputVar = resolveInputVar(
          edges, nodeId, 'value', nodeIndexMap, '0.0f',
        );

        renderLines.push(templateScale(varName, inputVar, factor));
        break;
      }

      // -------------------------------------------------------------------
      // Geometry
      // -------------------------------------------------------------------
      case 'gaussian': {
        const varName = makeVarName(nodeId, 'out', index);
        const sigma = instance.parameters.get('sigma') ?? 0.25;
        const centre = instance.parameters.get('centre') ?? 0.0;

        const amplitudeVar = resolveInputVar(
          edges, nodeId, 'amplitude', nodeIndexMap, '1.0f',
        );

        renderLines.push(templateGaussian(varName, amplitudeVar, sigma, centre));
        break;
      }

      case 'triangular-wave': {
        const varName = makeVarName(nodeId, 'out', index);
        const memberPhase = makeMemberName(def.type, index, 'phase');
        const spacing = instance.parameters.get('spacing') ?? 0.12;
        const driftSpeed = instance.parameters.get('driftSpeed') ?? 1.0;
        const sharpness = instance.parameters.get('sharpness') ?? 2.0;

        const amplitudeVar = resolveInputVar(
          edges, nodeId, 'amplitude', nodeIndexMap, '1.0f',
        );

        memberLines.push(`    float ${memberPhase} = 0.0f;`);
        initLines.push(`        ${memberPhase} = 0.0f;`);
        renderLines.push(templateTriangularWave(
          varName, amplitudeVar, memberPhase,
          spacing, driftSpeed, sharpness,
        ));
        break;
      }

      // -------------------------------------------------------------------
      // Composition
      // -------------------------------------------------------------------
      case 'hsv-to-rgb': {
        const varName = makeVarName(nodeId, 'out', index);
        const hueOffset = instance.parameters.get('hueOffset') ?? 0;
        const hueSpread = instance.parameters.get('hueSpread') ?? 48;

        const valueArrayVar = resolveInputVar(
          edges, nodeId, 'value', nodeIndexMap, 'nullptr /* unconnected */',
        );
        const hueVar = resolveInputVar(
          edges, nodeId, 'hue', nodeIndexMap, '0.0f',
        );
        const saturationVar = resolveInputVar(
          edges, nodeId, 'saturation', nodeIndexMap, '255.0f',
        );

        renderLines.push(templateHsvToRgb(
          varName, valueArrayVar, hueVar, saturationVar,
          hueOffset, hueSpread,
        ));
        break;
      }

      // -------------------------------------------------------------------
      // Output
      // -------------------------------------------------------------------
      case 'led-output': {
        const fadeAmount = instance.parameters.get('fadeAmount') ?? 20;

        const colourVar = resolveInputVar(
          edges, nodeId, 'colour', nodeIndexMap, 'nullptr /* unconnected */',
        );

        renderLines.push(templateLedOutput(colourVar, fadeAmount));
        break;
      }

      default:
        renderLines.push(`    // Unknown node type: ${def.type}`);
        break;
    }
  }

  // Build class
  const className = toClassName(effectName);

  const memberDeclarations = memberLines.length > 0
    ? memberLines.join('\n')
    : '    // No stateful members';

  const initBody = initLines.length > 0
    ? initLines.join('\n')
    : '        // No state to initialise';

  const renderBody = renderLines.join('\n\n');

  const header = templateEffectShell(
    effectName,
    className,
    memberDeclarations,
    initBody,
    renderBody,
  );

  // Source file empty for POC (header-only class)
  const source = `// ${className} is header-only. See ${className}.h\n`;

  return { header, source };
}
