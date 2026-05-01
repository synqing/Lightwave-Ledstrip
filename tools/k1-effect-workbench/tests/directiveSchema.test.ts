import { describe, expect, it } from 'vitest';
import { auditRules } from '../src/catalogues/rules';
import { generateDirective } from '../src/directive/generateDirective';
import type { RepairSession } from '../src/types';

describe('directive schema', () => {
  it('emits the required repair directive sections', () => {
    const session: RepairSession = {
      effectId: '0x1B04',
      effectName: 'Fresnel Caustic Sweep',
      sourceFiles: ['firmware-v3/src/effects/ieffect/LGPFresnelCausticSweepEffect.cpp'],
      createdAt: '2026-05-01T00:00:00.000Z',
      designIntent: {
        effectIdentity: 'Ephemeral Fresnel lens',
        dominantMusicalEvent: 'Reliable kick onset',
        silenceBehaviour: 'Dark',
        forbiddenForThisEffect: ['pendulum'],
      },
      visualStory: {
        sentence1: 'A kick creates a lens at centre.',
        sentence2: 'It travels outward and fades.',
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
      rulesAudit: auditRules({
        designIntent: {
          effectIdentity: 'Ephemeral Fresnel lens',
          dominantMusicalEvent: 'Reliable kick onset',
          silenceBehaviour: 'Dark',
          forbiddenForThisEffect: ['pendulum'],
        },
        visualStory: {
          sentence1: 'A kick creates a lens at centre.',
          sentence2: 'It travels outward and fades.',
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
      }),
      actualCaptures: [],
    };

    const directive = generateDirective(session);
    for (const section of [
      '## Sign-off Status',
      '## Design Intent',
      '## Two-Sentence Visual Story',
      '## Accepted AP/VP Pairing',
      '## Blocked Behaviours',
      '## Evidence Used',
      '## Prediction Summary',
      '## Implementation Hints',
      '## Required Tests',
      '## Not Proven',
    ]) {
      expect(directive).toContain(section);
    }
  });
});

