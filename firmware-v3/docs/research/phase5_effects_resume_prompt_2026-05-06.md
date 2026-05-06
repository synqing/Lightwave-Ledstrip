# Phase 5 Effects Resume Prompt - 2026-05-06

> **RBDO status:** GROUNDED for repo/source/doc facts and the known hardware-test outcome. DEGRADED-MODE for any future visual-quality conclusion until Captain records explicit row-level visual answers on K1 hardware.

> **Purpose:** This is the context-rich prompt for the next agent. It resumes the Phase 5 effects / visual-quality task that was interrupted by the K1v2 heap incident. It is deliberately not a `.claude/handoff*.md` file because this repo forbids forward task lists in `.claude/`.

## Copy/Paste Prompt For Next Agent

You are resuming the LightwaveOS K1 firmware effects task in:

```text
/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip
```

You are **not** resuming the SRAM/PSRAM reclaim task. That workstream has been handed to another team. Do not continue it, do not inspect it unless needed to avoid staging its files, and do not present it as the active task.

Your active task is:

```text
Resume Phase 5 effect visual-quality work after the heap incident.
RTS / 0x2100 is parked unless Captain explicitly reopens it.
Focus on making the good light shows better, especially 0x2101 and 0x2102.
```

## Mandatory First Read

Read these in order before touching source or hardware:

1. `CLAUDE.md` top RBDO gate and context-management rules.
2. `AGENTS.md` hard constraints and Workflow Discipline.
3. `BACKLOG.md` C-1 through C-5 and Synergy-Topology Phase 5 rows.
4. `firmware-v3/docs/research/c5_phase5_timestamped_observables_2026-05-06.md`.
5. `firmware-v3/docs/research/c5_phase5_hardware_sweep_2026-05-06.md`.
6. `firmware-v3/docs/audit/phase_5_visual_sign_off_2026-04-28.md`.
7. `firmware-v3/docs/audit/VP_VALIDATION_PROTOCOL_2026-05-06.md` if you touch colour correction, silence policy, buffer ownership, EdgeMixer, capture validation, or any visible default.

Do not use stale `.claude/handoff*.md` files as forward authority.

## Hard Constraints To Read Back

You must read back at session start:

- RBDO label discipline: tactical output is GROUNDED, DEGRADED-MODE, or REFUSED.
- K1 is AP-only for this task. Do not use REST/STA assumptions, pure STA, or concurrent AP+STA.
- Serial-only hardware control is preferred for this workflow.
- Centre origin is mandatory: LEDs 79/80 outward.
- No rainbows or full hue-wheel sweeps.
- No heap allocation in `render()` or render-transitive functions.
- 120 FPS / 2.0 ms effect-code ceiling.
- Delta-time-correct smoothing only.
- Sub-8 ms audio-to-visual latency remains a system constraint.
- British English in comments, docs, logs, and UI strings.
- Hardware-test-before-commit applies to any firmware change that affects render, memory, audio, or effect behaviour.

## Current Repo State At This Prompt

Known current head when this prompt was written:

```text
7d7d9bf5 docs(firmware-v3): hand off k1v2 sram reclaim pass
63a4b392 fix(firmware-v3): restore k1v2 heap headroom
2b8ec5e6 docs(firmware-v3): record c5 hardware sweep
149d9dc5 docs(firmware-v3): close c5 observable matrix
c7d20a8b docs(firmware-v3): close c2 dwell matrix
65489e49 docs(firmware-v3): add vp validation protocol
```

The active branch at the prior handoff was:

```text
feature/synergy-topology-resume-2026-05-05
```

Verify current branch and HEAD before acting.

Known dirty tree at this prompt included unrelated governance/memory docs and two SRAM-workstream artefacts created during the wrong resume path:

```text
M AGENTS.md
M CHANGELOG.md
M CLAUDE.md
M docs/WORKFLOW_ROUTING.md
M docs/tooling/claude-mem-usage-optimisation-2026-05-02.md
?? firmware-v3/docs/research/k1v2_sram_psram_reclaim_run_2026-05-06.md
?? firmware-v3/scripts/check_k1v2_batch_a_sram_reclaim.py
?? instructions/changelog/2026-05-05--repo--claude-mem-search-routing-fallbacks.md
```

Do not stage, delete, edit, or "clean up" those files unless Captain explicitly assigns that separate task.

## Why This Prompt Exists

The previous agent incorrectly resumed the SRAM/PSRAM reclaim work after Captain said "resume". Captain meant: return to the effects work that was interrupted by the heap problem. The wrong-path SRAM mapping was stopped. Your job is to avoid repeating that mistake.

## Effects Scope

Phase 5 cohort:

| ID | Effect | Source anchors | Status |
|---|---|---|---|
| `0x2100` | Radial Time Scope / RTS | `firmware-v3/src/effects/ieffect/RadialTimeScopeEffect.{h,cpp}` | Parked. Do not attempt to salvage unless Captain explicitly reopens it. |
| `0x2101` | Attack-Only Pitch Velocity Field / PVF | `firmware-v3/src/effects/ieffect/AttackOnlyPitchVelocityFieldEffect.{h,cpp}` | Promising target. Continue visual-quality/testing path here. |
| `0x2102` | Beat Parity Sprite / BPS | `firmware-v3/src/effects/ieffect/BeatParitySpriteEffect.{h,cpp}` | Promising target. Continue visual-quality/testing path here. |

Registration/source anchors:

- `firmware-v3/src/config/effect_ids.h` defines:
  - `EID_RADIAL_TIME_SCOPE = 0x2100`
  - `EID_ATTACK_ONLY_PITCH_VELOCITY = 0x2101`
  - `EID_BEAT_PARITY_SPRITE = 0x2102`
- `firmware-v3/src/config/display_order.h` includes those three IDs in order.
- `firmware-v3/src/effects/CoreEffects.cpp` registers all three static instances.

## What Happened Before The Heap Interruption

Captain and prior agents were working through Phase 5 effect testing. The test process had these important outcomes:

- The original timed two-unit subjective A/B workflow was cancelled. Do not revive it.
- Captain found side-by-side manual A/B too hard and asked for manual control.
- Private audio material was used or selected for test windows, but repo artefacts must not include private clip paths, file names, dataset IDs, or track names.
- C-5 timestamped observable rows were created using redacted private corpus labels.
- A K1v2 hardware sweep was run over serial.
- That sweep is trace-complete but visual-incomplete.
- Captain gave one explicit row judgement: `RTS-4` was an extremely poor effect/fixture choice.
- Captain later objected to continuing to mess with `0x2100`; the goal is to improve the good light shows, not salvage poor ones.
- The heap problem interrupted continuation after Captain said to continue testing.

## C-5 Hardware Sweep Facts

`firmware-v3/docs/research/c5_phase5_hardware_sweep_2026-05-06.md` records:

- Device: K1v2 on `/dev/cu.usbmodem2101`, upload MAC `b4:3a:45:a5:87:f8`.
- Firmware for sweep: `esp32dev_audio_esv11_k1v2_32khz_trace`.
- Control path: serial only.
- Trace coverage: 18/18 C-5 rows produced trace JSON and row metadata.
- Visual coverage: only `RTS-4` has an explicit Captain visual judgement.
- All effect-specific render p99 values stayed below 2 ms.
- RMT wire time stayed sane, about 6.1-6.3 ms.
- BPS rows with kicks showed 1:1 `bps_kick_fired` to `bps_sprite_spawn`.

Important interpretation:

```text
C-5 is TRACE-COMPLETE / VISUAL-INCOMPLETE.
Do not claim Phase 5 ship-gate readiness.
Do not treat trace health as visual quality.
Do not use RTS-4 as a pass row.
```

## Current Best Next Step

Do **not** start by editing code. Start by re-establishing the effects testing lane and asking only for the minimum Captain involvement needed.

Recommended sequence:

1. Verify git status and current head.
2. Verify whether K1v2 is currently available and on which serial port. Last known target was `/dev/cu.usbmodem2101`, MAC `b4:3a:45:a5:87:f8`, but this may be stale.
3. Confirm no serial monitor or upload command is hogging the port.
4. Build or use the current K1v2 production/trace firmware only after confirming the active task and target with Captain.
5. Park `0x2100`.
6. Resume with `0x2101` and `0x2102`.
7. For each effect, use the C-5 row questions as the operator-facing visual checks, but batch questions instead of interrupting Captain row-by-row.
8. If the next step requires audio playback coordination, do not launch it immediately. Tell Captain exactly:
   - which effect;
   - which row or phenomenon;
   - what audio type/window is needed;
   - when playback should start/stop;
   - what visual answer you need.
9. If the next step can be automated, automate playback and serial control instead of forcing manual timing.
10. If a visual problem is found, classify whether it is effect-local, VP colour/silence/output-path, audio-feature, or transport.

## Do Not Repeat These Mistakes

- Do not continue the SRAM/PSRAM reclaim workstream.
- Do not touch `firmware-v3/docs/research/k1v2_sram_psram_reclaim_run_2026-05-06.md` or `firmware-v3/scripts/check_k1v2_batch_a_sram_reclaim.py` unless Captain explicitly redirects.
- Do not run a test immediately after Captain offers a track or device unless he explicitly says to start.
- Do not ask Captain to choose between technical options without first listing the upstream facts that make the choice decidable.
- Do not present techno-babble as a Captain action item. If Captain needs to do something, say exactly what he needs to do in plain language.
- Do not ask for visual inspection of identical full-white patterns or hard-to-see diagnostic output.
- Do not use REST/STA. K1 is AP-only for this task and the live validation path is serial.
- Do not revive timed two-unit manual A/B.
- Do not claim a row passed unless Captain explicitly gives PASS / FAIL / DEGRADED-PASS or an equivalent visual judgement.
- Do not claim ship-gate readiness.

## Hardware Conduct

Before any hardware run:

1. Verify the serial target by identity, not port name alone.
2. Tell Captain whether the run needs him to watch the LEDs.
3. If it needs Captain, state the exact thing he should look for in one short sentence.
4. If audio is required, state the exact type of audio and timing.
5. Keep all private media paths out of committed docs.
6. Record only redacted private labels in repo artefacts.

If the device shows low heap again, stop the effects run and report it as a blocker. Do not pivot into the SRAM task unless Captain explicitly reassigns you to that workstream.

## Effect-Specific Guidance

### `0x2101` PVF

Use PVF rows from `c5_phase5_timestamped_observables_2026-05-06.md`.

Primary visual questions:

- Does sustained tonal material read as a continuous radial field rather than isolated dots?
- Do harmonic changes glide instead of snapping or smearing?
- Does dense material remain saturated and musical, not pastel or noisy?
- Does silence clear the field rather than freezing a bright chroma latch?
- Does bass-heavy material avoid flattening the pitch field?

Trace counters to watch:

- `pvf_audio_conf`
- `pvf_silent_scale`
- `pvf_field_zero`
- `pvf_field_max`
- `pvf_field_min`
- `pvf_topk_count`

High-risk failure modes:

- Chroma field reads as "three dots" rather than continuum.
- Harmonic movement is too subtle to perceive.
- Dense material creates haze or visual flattening.
- Silence leaves stale bright colour.
- Fixes accidentally introduce rainbow drift.

### `0x2102` BPS

Use BPS rows from `c5_phase5_timestamped_observables_2026-05-06.md`.

Primary visual questions:

- Does silence produce zero new sprites?
- Do visible kicks create sprites without missed or extra pulses?
- Does syncopated material avoid becoming a metronome?
- Do sprite colours feel stable and intentional?
- Does the effect read as bloom/breath with parity accents rather than mechanical dot-strobe?
- After hard stop, do existing sprites fade without new centre pulses?

Trace counters to watch:

- `bps_silent_scale`
- `bps_active_sprites`
- `bps_silence_gate`
- `bps_kick_trigger`
- `bps_kick_fired`
- `bps_sprite_spawn`
- `bps_tempo_conf`
- `bps_beat_in_bar`

Known positive evidence:

- In the C-5 sweep, BPS kick rows showed 1:1 `bps_kick_fired` to `bps_sprite_spawn` in captured windows where kicks occurred.
- Effect-specific p99 render time stayed below 2 ms in the sweep.

High-risk failure modes:

- Dot-strobe or metronome look instead of bloom/breath.
- False sprite spawns during silence or after stop.
- Missed kick spawns.
- Colour feels like arbitrary hue cycling.
- Centre origin is not visually obvious.

### `0x2100` RTS

Park this unless Captain explicitly reopens it.

Reason:

- `RTS-4` failed as an extremely poor effect/fixture choice.
- Captain objected to continuing to mess with `0x2100` when the goal is improving good light shows.
- Any future RTS work needs a deliberate replacement row, redesign, or explicit product decision.

## If You Need To Make A Firmware Change

Before editing:

1. Read the exact effect source and its native tests.
2. Use `rg` for text search and clangd if available for C++ symbol navigation.
3. Confirm the change is effect-local and not a VP pipeline default.
4. If it changes visible output, define the Captain visual question before coding.
5. Add or adjust focused native tests where possible.
6. Build `esp32dev_audio_esv11_k1v2_32khz`.
7. Hardware-test on K1v2 before commit.
8. Commit only your files, not unrelated dirty files.
9. Commit body must cite `BACKLOG.md` Phase 5 / C-5 and any run report.

Relevant native tests:

- `firmware-v3/test/test_native/test_attack_only_pitch_velocity.cpp`
- `firmware-v3/test/test_native/test_radial_time_scope.cpp`
- BPS has implementation-level source hooks in `BeatParitySpriteEffect.{h,cpp}`; verify exact native coverage before assuming it exists.

Do not add heap allocation to `render()`.

## If You Need A Run Report

Use a new sanitised report under `firmware-v3/docs/research/`, for example:

```text
firmware-v3/docs/research/phase5_effects_visual_resume_run_2026-05-06.md
```

Required fields:

```text
RBDO label:
Device / port / MAC:
Firmware commit:
Build env:
Effect IDs tested:
Rows tested:
Private labels only:
Serial commands:
Audio timing:
Trace counters:
Visual question:
Captain answer:
Result: PASS | FAIL | DEGRADED-PASS
What changed:
What remains unproven:
Next action:
```

Do not include private audio file paths, track names, or dataset IDs.

## Plain-English Captain Action Format

When Captain needs to do something, use this format:

```text
Captain, I need you to do one thing:
Play/stop/watch <exact thing> at <exact time>.
Tell me only this answer: <single visual question>.
```

Examples:

```text
Captain, I need you to watch 0x2102 for one 30-second pass.
I will start the effect, then you start the kick-heavy clip.
Tell me only: do kick hits create centre-born sprites without extra false pulses?
```

```text
Captain, I need you to watch 0x2101 during sustained tonal material.
Tell me only: does it look like a continuous field, or does it collapse into isolated dots?
```

## Success Criteria For This Resume

This resume succeeds when one of these is true:

1. PVF / `0x2101` has a clear Captain visual result plus trace evidence for the next row.
2. BPS / `0x2102` has a clear Captain visual result plus trace evidence for the next row.
3. A specific visual defect is isolated to effect-local source with a proposed patch and test plan.
4. A blocker is surfaced plainly, such as low heap, device unavailable, private audio unavailable, or VP pipeline default risk.

This resume fails if the agent:

- continues SRAM work;
- tries to salvage `0x2100` without explicit Captain authorisation;
- asks Captain to interpret trace jargon;
- launches a hardware/audio run without making the operator action clear;
- claims visual sign-off from trace evidence alone.

## One-Screen TL;DR

Resume Phase 5 effects, not SRAM. C-5 is trace-complete but visual-incomplete. RTS/`0x2100` is parked because `RTS-4` was a bad effect/fixture choice and Captain does not want to salvage bad light shows. Continue with PVF/`0x2101` and BPS/`0x2102`, using serial-only K1v2 hardware checks, redacted private audio labels, and simple Captain visual questions. Do not use REST/STA, do not revive two-unit timed A/B, do not commit private media paths, and do not claim visual pass without Captain row-level judgement.
