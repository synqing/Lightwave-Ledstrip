import { describe, expect, it } from 'vitest';
import { auditRules } from '../src/catalogues/rules';
import type { RuleInput } from '../src/catalogues/rules';

function baseInput(): RuleInput {
  return {
    designIntent: {
      effectIdentity: 'Ephemeral kick-spawned Fresnel lens',
      dominantMusicalEvent: 'Reliable kick onset',
      silenceBehaviour: 'Dark in silence',
      forbiddenForThisEffect: ['pendulum', 'ambient carrier', 'ctx.gHue'],
    },
    visualStory: {
      sentence1: 'Each reliable kick creates a lens at the centre pair.',
      sentence2: 'In silence the strip is dark.',
    },
    selectedAudio: {
      dominant: 'onset.kick.fired',
      modifiers: ['chroma'],
    },
    selectedVisual: {
      primitive: 'centre-pulse',
      motionRule: 'centre outward',
      decayRule: 'monotonic',
      flags: [],
    },
  };
}

describe('rules audit', () => {
  it('allows the intended Track B 0x1B04 pairing', () => {
    const audit = auditRules(baseInput());
    expect(audit.blocked).toBe(false);
  });

  // Provisional reconstructed iteration fixture: replace with verbatim iteration 1 once recovered.
  it('catches reconstructed iteration 1: original pendulum/RMS/gHue failure pattern', () => {
    const input = baseInput();
    input.selectedAudio.modifiers = ['rms'];
    input.selectedVisual.flags = ['reversingPosition', 'ctx.gHue'];
    const ids = auditRules(input).findings.map((finding) => finding.id);
    expect(ids).toContain('R-AUDIO-002');
    expect(ids).toContain('R-MOTION-001');
    expect(ids).toContain('R-COLOUR-001');
  });

  // Provisional reconstructed iteration fixture: replace with verbatim iteration 2 once recovered.
  it('catches reconstructed iteration 2: deletion-patch no-dominant-event failure pattern', () => {
    const input = baseInput();
    input.selectedAudio.dominant = '';
    const ids = auditRules(input).findings.map((finding) => finding.id);
    expect(ids).toContain('R-AUDIO-001');
  });

  // Provisional reconstructed iteration fixture: replace with verbatim iteration 3 once recovered.
  it('catches reconstructed iteration 3: kick-ring carrier/pool-3 performance failure pattern', () => {
    const input = baseInput();
    input.selectedAudio.dominant = 'isOnBeat';
    input.selectedVisual.flags = ['ambientCarrier', 'reversingPosition', 'ctx.gHue'];
    input.renderUsMax = 2331;
    const ids = auditRules(input).findings.map((finding) => finding.id);
    expect(ids).toContain('R-AUDIO-004');
    expect(ids).toContain('R-SILENCE-001');
    expect(ids).toContain('R-MOTION-001');
    expect(ids).toContain('R-COLOUR-001');
    expect(ids).toContain('R-PERF-001');
  });
});
