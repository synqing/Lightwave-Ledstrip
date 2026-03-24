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
 *
 * When zone-aware mode is enabled, stateful member variables
 * become arrays indexed by kMaxZones with bounds-checked access.
 */

import type { GraphEngine } from '../engine/engine';
import { getNodeDefinition } from '../engine/engine';
import type { NodeId, PortName, Edge } from '../engine/types';
import { makeVarName, makeMemberName } from './variable-namer';
import {
  templateRms,
  templateBand,
  templateBeatStrength,
  templateSilentScale,
  templateEmaSmooth,
  templateMaxFollower,
  templateScale,
  templateMultiply,
  templateMultiplyArray,
  templateAdd,
  templatePower,
  templatePowerArray,
  templateGaussian,
  templateTriangularWave,
  templateHsvToRgb,
  templateLedOutput,
  templateSubpixelRenderer,
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
// Zone-aware member declaration helpers
// ---------------------------------------------------------------------------

/**
 * Generate a member declaration. In zone-aware mode, scalar members
 * become arrays of kMaxZones.
 */
function zoneMember(memberVar: string, initVal: string, zoneAware: boolean): string {
  if (zoneAware) {
    return `    float ${memberVar}[kMaxZones] = {${initVal}};`;
  }
  return `    float ${memberVar} = ${initVal};`;
}

/**
 * Generate an init line. In zone-aware mode, initialise all zone slots.
 */
function zoneInit(memberVar: string, initVal: string, zoneAware: boolean): string {
  if (zoneAware) {
    return `        for (int z = 0; z < kMaxZones; z++) ${memberVar}[z] = ${initVal};`;
  }
  return `        ${memberVar} = ${initVal};`;
}

/**
 * Generate the render-time member access expression.
 * In zone-aware mode, access via the zone index (z).
 */
function zoneMemberAccess(memberVar: string, zoneAware: boolean): string {
  if (zoneAware) {
    return `${memberVar}[z]`;
  }
  return memberVar;
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
 * @param engine The graph engine with the connected node graph.
 * @param effectName Display name for the effect.
 * @param zoneAware When true, generates per-zone state management code.
 * @returns { header, source } where header contains the full class
 *          and source is empty (header-only for POC simplicity).
 */
export function generateCpp(
  engine: GraphEngine,
  effectName: string,
  zoneAware: boolean = false,
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

  // In zone-aware mode, add the zone index variable at the start of render()
  if (zoneAware) {
    renderLines.push(`    // Per-zone state: resolve zone index with bounds check`);
    renderLines.push(`    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;`);
  }

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

      case 'silent-scale-source': {
        const varName = makeVarName(nodeId, 'out', index);
        renderLines.push(templateSilentScale(varName));
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

        memberLines.push(zoneMember(memberVar, '0.0f', zoneAware));
        initLines.push(zoneInit(memberVar, '0.0f', zoneAware));

        // In zone-aware mode, use the zone-indexed member access
        const memberAccess = zoneMemberAccess(memberVar, zoneAware);
        renderLines.push(templateEmaSmooth(varName, inputVar, memberAccess, tau));
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

        memberLines.push(zoneMember(memberFollower, '0.15f', zoneAware));
        initLines.push(zoneInit(memberFollower, '0.15f', zoneAware));

        const memberAccess = zoneMemberAccess(memberFollower, zoneAware);
        renderLines.push(templateMaxFollower(
          normalisedVar, followerVar, inputVar,
          memberAccess, attackTau, decayTau, floor,
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

      case 'multiply': {
        const varName = makeVarName(nodeId, 'out', index);
        const inputA = resolveInputVar(edges, nodeId, 'a', nodeIndexMap, '0.0f');
        const inputB = resolveInputVar(edges, nodeId, 'b', nodeIndexMap, '0.0f');

        // Detect array x scalar (geometry output is float[kHalfStrip])
        const edgeA = findInputEdge(edges, nodeId, 'a');
        const edgeB = findInputEdge(edges, nodeId, 'b');
        const sourceANode = edgeA ? nodes.get(edgeA.from.nodeId) : undefined;
        const sourceBNode = edgeB ? nodes.get(edgeB.from.nodeId) : undefined;
        const sourceADef = sourceANode ? getNodeDefinition(sourceANode.definitionType) : undefined;
        const sourceBDef = sourceBNode ? getNodeDefinition(sourceBNode.definitionType) : undefined;
        const isArrayA = sourceADef?.type === 'gaussian' || sourceADef?.type === 'triangular-wave' || sourceADef?.type === 'multiply';
        const isArrayB = sourceBDef?.type === 'gaussian' || sourceBDef?.type === 'triangular-wave' || sourceBDef?.type === 'multiply';

        if (isArrayA && !isArrayB) {
          renderLines.push(templateMultiplyArray(varName, inputA, inputB));
        } else if (!isArrayA && isArrayB) {
          renderLines.push(templateMultiplyArray(varName, inputB, inputA));
        } else {
          renderLines.push(templateMultiply(varName, inputA, inputB));
        }
        break;
      }

      case 'add': {
        const varName = makeVarName(nodeId, 'out', index);
        const inputA = resolveInputVar(edges, nodeId, 'a', nodeIndexMap, '0.0f');
        const inputB = resolveInputVar(edges, nodeId, 'b', nodeIndexMap, '0.0f');
        renderLines.push(templateAdd(varName, inputA, inputB));
        break;
      }

      case 'power': {
        const varName = makeVarName(nodeId, 'out', index);
        const exponent = instance.parameters.get('exponent') ?? 2.0;
        const inputVar = resolveInputVar(edges, nodeId, 'value', nodeIndexMap, '0.0f');

        // Detect if input is an array
        const inputEdge = findInputEdge(edges, nodeId, 'value');
        const sourceNode = inputEdge ? nodes.get(inputEdge.from.nodeId) : undefined;
        const sourceDef = sourceNode ? getNodeDefinition(sourceNode.definitionType) : undefined;
        const isArray = sourceDef?.type === 'gaussian' || sourceDef?.type === 'triangular-wave' || sourceDef?.type === 'multiply';

        if (isArray) {
          renderLines.push(templatePowerArray(varName, inputVar, exponent));
        } else {
          renderLines.push(templatePower(varName, inputVar, exponent));
        }
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

        memberLines.push(zoneMember(memberPhase, '0.0f', zoneAware));
        initLines.push(zoneInit(memberPhase, '0.0f', zoneAware));

        const memberAccess = zoneMemberAccess(memberPhase, zoneAware);
        renderLines.push(templateTriangularWave(
          varName, amplitudeVar, memberAccess,
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
        const dualMode = Math.round(instance.parameters.get('dualStripMode') ?? 0);

        const colourVar = resolveInputVar(
          edges, nodeId, 'colour', nodeIndexMap, 'nullptr /* unconnected */',
        );
        const colour2Var = dualMode === 1
          ? resolveInputVar(edges, nodeId, 'colour2', nodeIndexMap, colourVar)
          : '';

        renderLines.push(templateLedOutput(colourVar, fadeAmount, dualMode, colour2Var));
        break;
      }

      case 'subpixel-renderer': {
        const spFadeAmount = instance.parameters.get('fadeAmount') ?? 20;
        const spResolution = instance.parameters.get('resolution') ?? 2.0;
        const spDualMode = Math.round(instance.parameters.get('dualStripMode') ?? 0);

        const spColourVar = resolveInputVar(
          edges, nodeId, 'colour', nodeIndexMap, 'nullptr /* unconnected */',
        );
        const spColour2Var = spDualMode === 1
          ? resolveInputVar(edges, nodeId, 'colour2', nodeIndexMap, spColourVar)
          : '';

        renderLines.push(templateSubpixelRenderer(spColourVar, spFadeAmount, spResolution, spDualMode, spColour2Var));
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
