/**
 * Node type registry — registers all 48 node definitions with the engine.
 */

import { registerNodeType } from '../engine/engine';

// Sources (20)
import { rmsNode } from './sources/rms-node';
import { fastRmsNode } from './sources/fast-rms-node';
import { bandNode } from './sources/band-node';
import { allBandsNode } from './sources/all-bands-node';
import { allChromaNode } from './sources/all-chroma-node';
import { beatStrengthNode } from './sources/beat-strength-node';
import { fluxNode } from './sources/flux-node';
import { fastFluxNode } from './sources/fast-flux-node';
import { livelinessNode } from './sources/liveliness-node';
import { beatPhaseNode } from './sources/beat-phase-node';
import { beatTickNode } from './sources/beat-tick-node';
import { downbeatTickNode } from './sources/downbeat-tick-node';
import { bpmNode } from './sources/bpm-node';
import { tempoConfidenceNode } from './sources/tempo-confidence-node';
import { snareEnergyNode } from './sources/snare-energy-node';
import { hihatEnergyNode } from './sources/hihat-energy-node';
import { snareTriggerNode } from './sources/snare-trigger-node';
import { hihatTriggerNode } from './sources/hihat-trigger-node';
import { silentScaleNode } from './sources/silent-scale-node';
import { deltaTimeNode } from './sources/delta-time-node';

// Processing (13)
import { emaSmoothNode } from './processing/ema-smooth-node';
import { maxFollowerNode } from './processing/max-follower-node';
import { scaleNode } from './processing/scale-node';
import { powerNode } from './processing/power-node';
import { clampNode } from './processing/clamp-node';
import { addNode } from './processing/add-node';
import { multiplyNode } from './processing/multiply-node';
import { mixNode } from './processing/mix-node';
import { asymmetricFollowerNode } from './processing/asymmetric-follower-node';
import { schmittTriggerNode } from './processing/schmitt-trigger-node';
import { circularEmaNode } from './processing/circular-ema-node';
import { dtDecayNode } from './processing/dt-decay-node';
import { envelopeNode } from './processing/envelope-node';
import { codeBlockNode } from './processing/code-block-node';
import { frameBlockNode } from './processing/frame-block-node';

// Geometry (8)
import { gaussianNode } from './geometry/gaussian-node';
import { triangularWaveNode } from './geometry/triangular-wave-node';
import { standingWaveNode } from './geometry/standing-wave-node';
import { exponentialDecayNode } from './geometry/exponential-decay-node';
import { sinusoidalSumNode } from './geometry/sinusoidal-sum-node';
import { centreMeltNode } from './geometry/centre-melt-node';
import { linearGradientNode } from './geometry/linear-gradient-node';
import { scrollBufferNode } from './geometry/scroll-buffer-node';

// Composition (6)
import { hsvToRgbNode } from './composition/hsv-to-rgb-node';
import { paletteLookupNode } from './composition/palette-lookup-node';
import { blendNode } from './composition/blend-node';
import { scaleBrightnessNode } from './composition/scale-brightness-node';
import { addColourNode } from './composition/add-colour-node';
import { fadeToBlackNode } from './composition/fade-to-black-node';

// Output (2)
import { ledOutputNode } from './output/led-output-node';
import { subpixelRendererNode } from './output/subpixel-renderer-node';

const ALL_NODES = [
  // Sources (20)
  rmsNode, fastRmsNode, bandNode, allBandsNode, allChromaNode, beatStrengthNode,
  fluxNode, fastFluxNode, livelinessNode,
  beatPhaseNode, beatTickNode, downbeatTickNode,
  bpmNode, tempoConfidenceNode,
  snareEnergyNode, hihatEnergyNode, snareTriggerNode, hihatTriggerNode,
  silentScaleNode, deltaTimeNode,
  // Processing (13)
  emaSmoothNode, maxFollowerNode, scaleNode, powerNode,
  clampNode, addNode, multiplyNode, mixNode,
  asymmetricFollowerNode, schmittTriggerNode, circularEmaNode, dtDecayNode,
  envelopeNode, codeBlockNode, frameBlockNode,
  // Geometry (8)
  gaussianNode, triangularWaveNode, standingWaveNode, exponentialDecayNode,
  sinusoidalSumNode, centreMeltNode, linearGradientNode, scrollBufferNode,
  // Composition (6)
  hsvToRgbNode, paletteLookupNode, blendNode, scaleBrightnessNode, addColourNode, fadeToBlackNode,
  // Output (2)
  ledOutputNode, subpixelRendererNode,
] as const;

export function registerAllNodes(): void {
  for (const node of ALL_NODES) {
    registerNodeType(node);
  }
}

export function getNodesByLayer() {
  return {
    source: ALL_NODES.filter((n) => n.layer === 'source'),
    processing: ALL_NODES.filter((n) => n.layer === 'processing'),
    geometry: ALL_NODES.filter((n) => n.layer === 'geometry'),
    composition: ALL_NODES.filter((n) => n.layer === 'composition'),
    output: ALL_NODES.filter((n) => n.layer === 'output'),
  };
}
