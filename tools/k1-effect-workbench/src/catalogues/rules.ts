import type {
  AudioSignalSelection,
  DesignIntent,
  RuleFinding,
  RulesAudit,
  VisualPrimitiveSelection,
  VisualStory,
} from '../types';

export type RuleInput = {
  designIntent: DesignIntent;
  visualStory: VisualStory;
  selectedAudio: AudioSignalSelection;
  selectedVisual: VisualPrimitiveSelection;
  renderUsMax?: number;
};

const continuousBroadband = new Set(['rms', 'fastRms', 'flux', 'fastFlux']);
const continuousBands = new Set(['bass', 'mid', 'treble', 'heavyBass', 'heavyMid', 'heavyTreble', 'bands']);

function includesAny(values: string[], blocked: Set<string>): boolean {
  return values.some((value) => blocked.has(value));
}

function hasFlag(selection: VisualPrimitiveSelection, flag: string): boolean {
  return selection.flags.includes(flag);
}

export function auditRules(input: RuleInput): RulesAudit {
  const findings: RuleFinding[] = [];
  const audioKeys = [input.selectedAudio.dominant, ...input.selectedAudio.modifiers].filter(Boolean);
  const story = `${input.visualStory.sentence1} ${input.visualStory.sentence2}`.toLowerCase();

  if (!input.designIntent.effectIdentity.trim()) {
    findings.push({
      id: 'R-DESIGN-001',
      severity: 'blocked',
      message: 'Visual identity is required before AP/VP selection.',
      evidence: 'Directive schema gate.',
    });
  }

  if (!input.selectedAudio.dominant.trim()) {
    findings.push({
      id: 'R-AUDIO-001',
      severity: 'blocked',
      message: 'Dominant visible change must have one selected audio event.',
      evidence: '0x1B04 repair doctrine.',
    });
  }

  if (includesAny(audioKeys, continuousBroadband)) {
    findings.push({
      id: 'R-AUDIO-002',
      severity: 'blocked',
      message: 'Broadband continuous fields cannot drive 0x1B04 dominant motion.',
      evidence: 'RMS/flux bridge rejected in Phase 0.',
    });
  }

  if (includesAny(audioKeys, continuousBands)) {
    findings.push({
      id: 'R-AUDIO-003',
      severity: 'blocked',
      message: 'Band magnitudes cannot drive 0x1B04 dominant motion.',
      evidence: 'Band-magnitude continuous bridge block.',
    });
  }

  if (hasFlag(input.selectedVisual, 'linearSweep')) {
    findings.push({
      id: 'R-CENTRE-001',
      severity: 'blocked',
      message: 'Linear sweep violates centre-origin geometry.',
      evidence: 'EFFECT_FRAMEWORK_STANDARD.md:62-66',
    });
  }

  if (hasFlag(input.selectedVisual, 'reversingPosition')) {
    findings.push({
      id: 'R-MOTION-001',
      severity: 'blocked',
      message: 'Sin/cos/triangle/ping-pong position motion is blocked for this repair.',
      evidence: '0x1B04 pendulum failure mode.',
    });
  }

  if (hasFlag(input.selectedVisual, 'ambientCarrier') && story.includes('silence') && story.includes('dark')) {
    findings.push({
      id: 'R-SILENCE-001',
      severity: 'blocked',
      message: 'Silence is dark, so an ambient carrier is not allowed.',
      evidence: 'Design story silence gate.',
    });
  }

  if (hasFlag(input.selectedVisual, 'ctx.gHue') || hasFlag(input.selectedVisual, 'rainbow')) {
    findings.push({
      id: 'R-COLOUR-001',
      severity: 'blocked',
      message: 'Audio-independent hue rotation is blocked.',
      evidence: 'No-rainbow/chromagram doctrine.',
    });
  }

  if (input.renderUsMax !== undefined && input.renderUsMax >= 2000) {
    findings.push({
      id: 'R-PERF-001',
      severity: 'blocked',
      message: 'Effect render max exceeds the 2.0 ms hard ceiling.',
      evidence: `Measured max ${input.renderUsMax} us.`,
    });
  }

  if (input.selectedAudio.dominant === 'isOnBeat' && input.designIntent.dominantMusicalEvent.toLowerCase().includes('kick')) {
    findings.push({
      id: 'R-AUDIO-004',
      severity: 'blocked',
      message: 'Beat is not the selected dominant event for a kick-only repair story.',
      evidence: 'Design intent dominant event mismatch.',
    });
  }

  return {
    level: 'L1',
    findings,
    blocked: findings.some((finding) => finding.severity === 'blocked'),
  };
}

