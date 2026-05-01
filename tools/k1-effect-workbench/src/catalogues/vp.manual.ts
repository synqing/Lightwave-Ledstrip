import type { CatalogueEntry } from '../types';

export const visualPrimitiveCatalogue: CatalogueEntry[] = [
  {
    key: 'drawDot',
    label: 'Render primitive: drawDot',
    status: 'supported',
    source: 'firmware-v3/src/effects/render/RenderPrimitives.h:57-94',
    rationale: 'Shared centre-pair/edge-normalised dot primitive.',
  },
  {
    key: 'drawSpriteScrolled',
    label: 'Render primitive: drawSpriteScrolled',
    status: 'supported',
    source: 'firmware-v3/src/effects/render/RenderPrimitives.h:96-134',
    rationale: 'Shared centre-origin outward/inward sub-pixel scroll primitive.',
  },
  {
    key: 'fillFromBins',
    label: 'Render primitive: fillFromBins',
    status: 'supported',
    source: 'firmware-v3/src/effects/render/RenderPrimitives.h:136-170',
    rationale: 'Maps bin 0 to centre pair and last bin to edges.',
  },
  {
    key: 'centre-pulse',
    label: 'Centre-origin pulse',
    status: 'supported',
    source: 'firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md:353-355',
    rationale: 'Onset/percussion maps naturally to centre burst consequences.',
  },
  {
    key: 'outward-caustic',
    label: 'Outward caustic front',
    status: 'supported',
    source: 'firmware-v3/src/effects/PatternRegistry.cpp:250',
    rationale: 'Matches 0x1B04 Fresnel caustic identity while preserving centre-origin behaviour.',
  },
  {
    key: 'captured-byte-strip',
    label: 'Captured byte strip',
    status: 'supported',
    source: 'firmware-v3/testbed/evaluation/frame_parser.py:30-37',
    rationale: 'Actual captured frames can be replayed without claiming LGP perceptual truth.',
  },
  {
    key: 'linear-sweep',
    label: 'Linear sweep',
    status: 'blocked',
    source: 'firmware-v3/docs/EFFECT_FRAMEWORK_STANDARD.md:62-66',
    rationale: 'Violates centre-origin geometry.',
  },
  {
    key: 'rainbow-wheel',
    label: 'Hue wheel / rainbow',
    status: 'blocked',
    source: 'firmware-v3/docs/EFFECT_FRAMEWORK_STANDARD.md:70-78',
    rationale: 'Violates no-rainbow and chromagram hue doctrine.',
  },
  {
    key: 'pendulum-focus',
    label: 'Pendulum focus',
    status: 'blocked',
    source: 'firmware-v3/src/effects/ieffect/LGPFresnelCausticSweepEffect.cpp:180-198',
    rationale: 'The 0x1B04 repair rejects reversing focus position in pixel space.',
  },
  {
    key: 'bespoke-full-strip-loop',
    label: 'Bespoke per-pixel render loop',
    status: 'blocked',
    source: 'firmware-v3/docs/research/spazz_redesign_2026-04-30/PIPELINE_REFORM.md:205-216',
    rationale: 'Workbench directives should prefer shared primitives where available.',
  },
];
