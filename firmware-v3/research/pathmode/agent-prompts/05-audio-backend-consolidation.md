You are a Senior Engineer. Implement the following Intent Spec using a sequential workflow.

CRITICAL RULES:
- 'constraints' = constitutional rules you MUST NOT VIOLATE.
- Read the Space Context for WHY.
- Satisfy ALL 'outcomes'.

## Phase 1: Orientation
Identify likely-affected files: src/audio/AudioActor.{h,cpp}, src/audio/contracts/ControlBus.h, src/audio/backends/esv11/* and pipelinecore/*, platformio.ini envs (esp32dev_audio_esv11_*, esp32dev_audio_pipelinecore), CI workflows. Read EXISTING patterns before changes. Use Glob/Grep/Read.

## Phase 2: Plan
List every file + specific changes. Order: env removal first, then conditional cleanup, then ControlBus doc alignment, then new architecture doc, then changelog migration note. Map to outcomes via TodoWrite.

## Phase 3: Implement
Dependency order. After each file: verify no breakage. Respect constraints.

## Phase 4: Validate
**E2E**: `pio run -e esp32dev_audio_esv11_k1v2_32khz` builds; smoke-test all effects on K1 hardware. `native_test_esv11_music` suite passes.

# Intent Spec: Audio backend consolidation: ESV11 sole production; PipelineCore deprecated

**ID**: `fbc867b6-299a-42a8-ae75-81d6dd938cea` | **Status**: validated

## Objective

CLAUDE.md is explicit that ESV11 is production and PipelineCore is broken (beat tracking non-functional after Goertzel→FFT migration), but Pathmode is silent on backends and firmware still carries dual paths. Captain decision 2026-04-26: deprecate PipelineCore from production builds now. This intent combines the wording reconciliation (declare ESV11 sole backend in Pathmode) with the engineering work to remove dual-path machinery from production.

## Success Outcomes

- [ ] New Pathmode key decision: 'Audio analysis backend: ESV11 (64-bin Goertzel, 32 kHz sampling, 50 Hz hop). PipelineCore is deprecated for production builds and retained only for offline regression harnesses.'
- [ ] PipelineCore build environment removed from production CI; retained only in native_test for offline regression compatibility.
- [ ] AudioActor.{h,cpp}: PipelineCore conditional removed; ESV11 sole compile path in production firmware.
- [ ] ControlBus field documentation aligned to ESV11 reality (50 Hz hop, Goertzel spectrum); PipelineCore-only fields tagged // Deprecated with migration notes.
- [ ] firmware-v3/docs/audio/k1-audio-architecture.md created as the single source of truth (ESV11 @ 32 kHz, 256-hop, Goertzel tempo, onset detector, bins64 spectrum, published @ 50 Hz).
- [ ] Migration note in changelog for any user content that relied on PipelineCore-only fields (e.g. bins256[]).

## Constraints & Constitution

- [!] native_test_pipelinecore retained for offline regression compatibility.
- [!] ControlBus frame layout binary-compatible (no offset changes — deployed shows must keep working).
- [!] ESV11 must support every ControlBus field PipelineCore consumers used (verify: tempo, beat, onset, spectrum, chroma).
- [!] British English.
- [!] Precedence: Constraint > Standard > Pattern.

## Edge Cases
- **Show file references PipelineCore-only ControlBus field** → Graceful zero-fill at load time + warning; migration note in changelog.
- **Lip-sync demo running PipelineCore in v3** → Migration note guides operators through the switch.
