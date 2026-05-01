import type { RepairSession } from '../types';

function lines(items: string[]): string {
  if (items.length === 0) return '- none';
  return items.map((item) => `- ${item}`).join('\n');
}

export function generateDirective(session: RepairSession): string {
  const blocked = session.rulesAudit.findings.filter((finding) => finding.severity === 'blocked');
  const warning = session.rulesAudit.findings.filter((finding) => finding.severity !== 'blocked');
  const capture = session.actualCaptures[0];

  return `# Effect Repair Directive: ${session.effectId} (${session.effectName})

## Sign-off Status

- Rule-check level: ${session.rulesAudit.level}
- Hardware truth available: ${capture ? `yes - ${capture.name}` : 'no'}
- Captain visual approval: no
- Perceptual LGP calibration: NOT PROVEN

## Design Intent

- Visual identity: ${session.designIntent.effectIdentity}
- Dominant musical event: ${session.designIntent.dominantMusicalEvent}
- Silence behaviour: ${session.designIntent.silenceBehaviour}
- Forbidden behaviours:
${lines(session.designIntent.forbiddenForThisEffect)}

## Two-Sentence Visual Story

${session.visualStory.sentence1}

${session.visualStory.sentence2}

## Accepted AP/VP Pairing

- Dominant AP: ${session.selectedAudio.dominant || 'not selected'}
- Secondary modifiers: ${session.selectedAudio.modifiers.length ? session.selectedAudio.modifiers.join(', ') : 'none'}
- Visual primitive: ${session.selectedVisual.primitive || 'not selected'}
- Origin rule: centre 79/80 outward
- Motion rule: ${session.selectedVisual.motionRule || 'not selected'}
- Decay rule: ${session.selectedVisual.decayRule || 'not selected'}

## Blocked Behaviours

${blocked.length ? blocked.map((finding) => `- ${finding.id}: ${finding.message} (${finding.evidence})`).join('\n') : '- none'}

## Evidence Used

- Audio catalogue source: firmware contracts and Phase 0 manual catalogue
- Visual catalogue source: effect standards, behavioural reference, and 0x1B04 source
- Transport catalogue source: YAML plus BACKLOG F-1 runtime-authority decision
- Rule catalogue version: workbench-v0-track-b
- Prediction scenario IDs: none - Track B capture-only

## Prediction Summary

- Scenario: none
- Expected centre behaviour: not predicted in Track B
- Expected silence behaviour: evaluated from directive and actual capture only
- Expected failure signs to watch for: ambient carrier in dark silence, reversing focus motion, hue-wheel drift, render max >= 2000 us

## Implementation Hints

- Likely source areas: ${session.sourceFiles.join(', ')}
- State shape: event-spawned consequences only; freeze event-derived strength/colour at spawn
- Render path constraints: no heap, < 2.0 ms max, centre origin
- Timing constraints: dt-correct smoothing; raw signal time only when preserving musical event timing

## Required Tests

- Catalogue/rules: known 0x1B04 bad choices rejected
- Frame parser: 961-byte and 966-byte LED frames
- Centre-origin mapping: LED 79/80 pair lights first
- Build: firmware build before hardware flash
- Hardware capture: actual K1 capture required before visual approval

## Not Proven

- Physical LGP readability
- Final firmware implementation correctness
- Subjective visual approval
- Perceptual bloom/scatter behaviour

## Non-Blocking Findings

${warning.length ? warning.map((finding) => `- ${finding.id}: ${finding.message}`).join('\n') : '- none'}
`;
}

