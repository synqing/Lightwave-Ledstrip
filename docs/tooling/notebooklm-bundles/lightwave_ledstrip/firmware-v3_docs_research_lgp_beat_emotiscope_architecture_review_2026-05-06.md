# LGP Beat / Emotiscope Architecture Review

**Visual characterisation anchor:** `firmware-v3/docs/research/k1_visual_characterisation_database.md` is the top-level product-language database for this lane. Use it to name visual traits/classes and keep Captain visual language tied to source-anchored mechanisms.

**Date:** 2026-05-06
**RBDO label:** GROUNDED for source-anchored findings; DEGRADED-MODE for visual-quality conclusions until Captain hardware judgement exists.
**Scope:** Read-only architecture review of the LGP Beat / Phase 5 effect family, with PVF `0x2101` and BPS `0x2102` as first targets.
**Non-scope:** SRAM/PSRAM reclaim, RTS `0x2100`, AP/STA changes, private audio media, commits, hardware flashing.

## Status

Captain visual judgement for PVF-2 is an explicit FAIL:

- Effect `0x2101` is "100% broken".
- Visible failure is wild, uncontrollable flicker.
- Background appears to flicker.
- Node formation, node count, and expansion logic are not intelligible.

This document records the architecture review lane opened after that judgement. It is not a forward handoff and does not claim ship readiness.

## Dirty Tree Boundary

Current known dirty-tree classes:

- Unrelated governance / memory-routing files: do not stage, edit, delete, or rely on them for this review.
- Existing PVF candidate patch from this session:
  - `firmware-v3/src/effects/ieffect/AttackOnlyPitchVelocityFieldEffect.cpp`
  - `firmware-v3/test/test_native/test_attack_only_pitch_velocity.cpp`
  - `firmware-v3/docs/research/phase5_effects_visual_resume_run_2026-05-06.md`

The PVF patch is uncommitted and hardware-unverified. Treat it as a candidate isolation patch, not as validated behaviour.

## Emotiscope Source Model

The repo already contains a useful Emotiscope audit packet at:

- `firmware-v3/docs/research/spazz_redesign_2026-04-30/canonical_emotiscope_active.md`
- `firmware-v3/docs/research/spazz_redesign_2026-04-30/PORT_PLAN.md`
- `firmware-v3/docs/research/spazz_redesign_2026-04-30/SSA5_audio_signal_smoothness.md`
- `firmware-v3/docs/research/spazz_redesign_2026-04-30/SSA8_percussion_trigger_semantics.md`

Key current-source anchors:

- Emotiscope 1.2 audit identifies Beat Tunnel as a feedback sprite plus per-tempo-bin phase marker effect driven by `tempi[].phase`, `tempi_smooth[]`, and a slow LFO (`canonical_emotiscope_active.md:503-566`).
- The same audit defines the canonical Emotiscope audio-source taxonomy: scalar VU, phase-locked tempo, pre-smoothed spectral arrays, and compositional darkening (`canonical_emotiscope_active.md:740-747`).
- It identifies three motion primitives only: `draw_dot`, `draw_sprite`, and direct spatial fill (`canonical_emotiscope_active.md:749-753`).
- It states smoothing mostly lives upstream or in render primitives, not in ad hoc per-effect audio physics (`canonical_emotiscope_active.md:755-767`).
- The prior port plan states the real architectural gap: K1 has no `tempi[]` bank equivalent today and would need a future ControlBus/API addition for faithful Beat Tunnel / tempo-bank ports (`PORT_PLAN.md:173-180`).

Direct Emotiscope 2.0 reference inspection matches the audit:

- `main/cpu_core.h:29-58` runs microphone acquisition, Goertzel magnitudes, FFT, chromagram, VU, and tempo update on the CPU path.
- `main/gpu_core.h:32-88` updates novelty, advances `tempi` phase, draws the active mode, then applies background, blur, warmth, tone mapping, gamma, and LED transmit.
- `main/tempo.h:217-258` computes and normalises tempo-bin magnitudes.
- `main/tempo.h:371-407` smooths each tempo bin and advances `tempi[tempo_bin].phase`.
- `main/light_modes/active/beat_tunnel.h:14-31` injects colour only when a tempo bin phase is near `0.65`, then scrolls the previous image with decay.

## Lightwave AP-VP Contract Surface

Current LightwaveOS already exposes richer effect-facing semantics than raw amplitude:

- `EffectContext.h:83-89` passes copied `ControlBusFrame`, `MusicalGridSnapshot`, and first-class `OnsetContext` into effects.
- `EffectContext.h:94-100` exposes RMS/fast RMS/flux as raw energy and novelty-style fields.
- `EffectContext.h:134-179` exposes beat phase, beat/downbeat events, BPM, tempo confidence, and beat strength.
- `EffectContext.h:273-287` exposes hop sequence plus raw/heavy chroma.
- `EffectContext.h:300-367` exposes transient/kick/snare/hi-hat onset channels.
- `ControlBus.h:118-246` shows the published frame has RMS, flux, bands, chroma, heavy bands/chroma, saliency, tempo fields, Emotiscope raw parity fields, silence scale, and audio confidence.
- `AudioReactivePolicy.h:50-126` centralises trigger decisions and separates tempo/transient/kick/snare/hi-hat/hybrid modes.

Architectural implication: a coherent K1 AP-VP port should first choose whether the effect is:

- event-launched: kick/transient/downbeat starts a visual state;
- tempo-locked: stable beat/phase drives positions;
- harmonic: chroma/notes choose colour or stable field parameters;
- envelope-only: RMS/energy controls brightness only, never structure;
- diagnostic / direct visualiser: allowed to show raw arrays as an analysis mode, not product visual-quality mode.

## Confirmed Current Failure Patterns

### PVF `0x2101`

Pre-patch production code captured chroma targets on every fresh hop and used raw `fastRms` for a full-strip bed. The candidate patch changed this to onset-gated target capture and confidence/silence-gated bed ownership, with a native regression test. That patch is source-verified but not hardware-verified.

Fault model:

- Raw per-hop chroma is not temporally smoothed on the ESV11/K1 path (`SSA5_audio_signal_smoothness.md:104-113`).
- Updating PVF's top-K field from raw chroma on every hop can reshuffle structure at audio-hop rate.
- A raw RMS/fastRMS bed can read as "background flicker" on the LGP, matching Captain's visual report.

### BPS `0x2102`

Current BPS source explicitly retains an RMS bed and RMS-shaped fade:

- Header says the bed is RMS-driven (`BeatParitySpriteEffect.h:30`, `BeatParitySpriteEffect.h:38-39`).
- Render reads `rmsLevel = ctx.audio.rms()` (`BeatParitySpriteEffect.cpp:120`).
- Bed writes the whole strip from `rmsLevel` before sprite composition (`BeatParitySpriteEffect.cpp:154-167`).
- Fade amount is also shaped directly by `rmsLevel` (`BeatParitySpriteEffect.cpp:169-176`).
- Silence/low-confidence gate happens after the bed and fade work (`BeatParitySpriteEffect.cpp:178-185`).

This means the "background" can still behave like a cheap amplitude visualiser even though the sprites themselves are event-launched from `kickTrigger`.

### Port-Architecture Mismatch

Emotiscope Beat Tunnel depends on a 96-bin tempo-resonator field (`NUM_TEMPI`, `tempi[]`, `tempi_smooth[]`) and per-bin phase windows. BPS uses a single kick boolean plus optional tempo tick/downbeat accent. That is not a faithful Beat Tunnel port; it is a different effect architecture.

If the product goal is "Beat Tunnel-style musical logic", K1 currently lacks the upstream control surface. A redesign must either:

- implement a K1-safe tempo-bank abstraction, or
- explicitly design a new K1-native Beat effect around the AP-VP surfaces we actually have.

## Working Hypothesis

The LGP Beat family is not corrupted because traces are missing. It is corrupted because visible structure is being driven from the wrong semantic layer:

- raw RMS or fast RMS is being used as a persistent visual bed;
- raw hop-rate chroma is being used as structural state without event gating or heavy smoothing;
- one-hop trigger booleans are being used as if they were musical phrases or tempo fields;
- Emotiscope tempo-bank effects were reduced into simpler AP-VP signals without preserving the original phase-bank semantics.

## Redesign Direction To Validate

1. Classify each Beat-family effect by intended music contract before editing:
   - amplitude meter,
   - event-launched sprite/ring,
   - tempo-bank/phase-field,
   - harmonic field,
   - diagnostic spectral display.
2. Remove raw-RMS "always alive" backgrounds from product Beat effects unless the effect is explicitly an amplitude meter.
3. For event effects, derive visual state from a hold-and-decay envelope launched by onset/kick/downbeat, not from per-frame RMS.
4. For chroma effects, snapshot chroma on a musical boundary or consume `heavy_chroma` / a local dt-correct follower; do not reshuffle structure from raw `chroma[]` every hop.
5. For any Beat Tunnel-style target, decide whether to add a tempo-bank bridge or to rename/redesign the effect as K1-native rather than pretending it is a direct Emotiscope port.

## SSA Consensus Integrated

Four read-only SSAs returned and converged on the same architecture fault model:

- Emotiscope versions are not interchangeable systems. Emotiscope 1.2 is the most useful active-mode reference for Beat Tunnel / Bloom / Tempiscope lineage, but it still assumes global `tempi[]`, `tempi_smooth[]`, 128-LED mirror geometry, and global frame-blending lifecycle. Emotiscope 2.0 is not a safer direct source; it has different VU/lifecycle behaviour and some mode-specific uncertainties.
- K1 AP-VP effects should consume `EffectContext` semantics, not raw Emotiscope globals. Events launch/latch/reroute. Envelopes shade, sustain, or gate. Chroma anchors colour and harmonic state. Raw/debug/internal arrays are not product-effect authoring surfaces.
- BPS `0x2102` has multiple confirmed corruption vectors:
  - raw RMS drives a full-strip bed before sprite composition;
  - the bed colour uses `ctx.gHue`, which auto-increments in the renderer and can create non-musical hue rotation;
  - parity currently affects trace/skip reporting more than visual spawn semantics;
  - downbeat accent is tied to any locked `tempoTick`, not explicit bar-downbeat intent;
  - the uniform bed is not a centre-origin visual structure even though the sprites themselves originate at LEDs 79/80.
- PVF `0x2101` is now source-isolated by the dirty candidate patch, but the patch is not hardware-validated and may still need a better harmonic-hold rule if onset-only target capture makes sustained pitch fields stale.
- The wider LGP Beat family likely shares related AP-VP contract drift:
  - LGP Beat Pulse has frame-count timing/smoothing and hand-rolled drum endpoints.
  - BeatPulse / BeatPrism paths can respond to metronome fallback rather than actual musical endpoints.
  - These are review targets, not yet visual-quality findings.

The prior evidence index also confirms that Phase 5 was never visually signed off: the April 28 sign-off is DEGRADED-MODE and explicitly left PVF continuum quality and BPS bloom-breath quality unvalidated.

## Immediate Next Repair Candidate: BPS `0x2102`

Root-cause hypothesis:

- BPS does not mainly fail because sprite traces are missing.
- BPS fails because the always-alive background is still raw RMS plus auto-rotating hue, while the named "parity" / "beat endpoint" semantics do not materially own the visible structure.

RED test set to write before behaviour edits:

1. Sustained RMS with no kick, no downbeat, and high confidence must not create a visible full-strip bed.
2. Silence or low-confidence must suppress bed output before any fade/bed write, not after.
3. Kick or explicit beat endpoint must spawn centre-origin sprites from LEDs 79/80 outward.
4. Background colour must be anchored by a musical or fixed palette choice, not by free-running `gHue`.
5. Parity/downbeat semantics must affect visible sprite selection, lifetime, or accent, not trace labels only.

Candidate behaviour rule:

- Replace the RMS bed with a dt-correct event envelope owned by active sprites or explicit beat/downbeat endpoints.
- Use RMS/energy only as a bounded brightness/detail modifier after an event has created state.
- Keep all geometry centre-origin and keep render paths heap-free.
- Keep PVF parked unless Captain explicitly reopens patched PVF hardware validation.

Do not ask Captain for visual judgement until BPS has a single firmware candidate, an exact audio/timing request, and one plain visual question.

## BPS Candidate Patch Evidence

Implemented candidate source changes:

- Added BPS native render-path tests:
  - `test_sustained_rms_without_kick_keeps_strip_dark`
  - `test_low_confidence_suppresses_rms_bed_before_write`
  - `test_kick_launches_centre_origin_sprite_state`
- Removed the always-alive RMS/gHue full-strip bed from `BeatParitySpriteEffect::render()`.
- Replaced raw-RMS-adaptive whole-strip fade with a fixed dt-correct trail fade.
- Kept RMS only as a bounded brightness accent after a kick event has created sprite state.
- Updated BPS header/metadata text so it no longer documents an RMS-owned ambient bed.

Verification run:

- RED: `pio test -e native_test_phase5` failed on the two intended BPS corruption tests before the production change.
- GREEN: `pio test -e native_test_phase5` passed, 134/134 tests.
- Build: `pio run -e esp32dev_audio_esv11_k1v2_32khz` passed.
- Hygiene: `git diff --check` passed.

Tool boundary:

- `codex mcp get clangd` confirms the registered MCP route uses `mcp-language-server`, the `firmware-v3` workspace, Homebrew LLVM clangd, the PlatformIO compile-commands directory, `--background-index=false`, and the Xtensa query-driver.
- Current Codex session exposes `mcp__clangd__`, but both `hover` and `diagnostics` returned `Transport closed`.
- Therefore this checkpoint makes no whole-codebase clangd symbol/reference/diagnostic claims.

Visual boundary:

- This is a firmware candidate patch only.
- No hardware flash or visual judgement has been run for BPS.
- Do not claim PASS, visual quality, or ship readiness from this evidence.

## BPS-1 Hardware Run

Run boundary:

- Captain explicitly requested: `Start BPS-1`.
- Target port: `/dev/cu.usbmodem2101`.
- Verified target MAC before upload: `b4:3a:45:a5:87:f8`.
- Upload profile: `esp32dev_audio_esv11_k1v2_32khz`.
- Effect row: BPS-1, effect `0x2102`, visual-only silence row, no audio playback.
- Captain BPS-1 silence visual answer: during silence, the effect is dark and unresponsive.
- Captain follow-up visual concern: after a series of tests with music, the effect feels highly responsive for roughly four seconds, then degrades as if a small buffer fills, cannot process new audio, and periodically emits incohesive/random sprites until it recovers.

Serial post-upload status:

- Active effect: `8450` / `0x2102` / Beat Parity Sprite.
- `showSkips=0` in both serial `s` samples.
- Heap stable across the two samples: `8094191` free, min `8087867`.
- SPIRAM stable: `8066247` free.
- Renderer stack watermark stable: `10432` words.

Timing concern:

- Sample 1: FPS `119`, frame avg `8372 us`, LED show avg `6192 us`, drops `2876`.
- Sample 2: FPS `118`, frame avg `8390 us`, LED show avg `6212 us`, drops `3054`.
- This is not a performance PASS. The total frame average is slightly over the 120 FPS frame period and the drop counter increased, even though LED show skips stayed at zero.

RBDO conclusion:

- Hardware run started and serial health captured.
- BPS-1 silence goal is visually confirmed by Captain.
- Wider BPS musical responsiveness remains DEGRADED-MODE pending targeted timing/trigger evidence.
- Performance remains DEGRADED-MODE until the frame/drop behaviour is explained or improved.

## BPS-1 Follow-Up Interpretation

Captain's "small buffer fills" visual model is directionally plausible as a description of what the fixture feels like, but it is not the literal BPS effect mechanism.

Source-grounded facts:

- BPS has a fixed visual sprite pool of 8 sprites, not an audio buffer.
- Each sprite has a nominal lifetime of `0.6 s`; downbeat accent can extend that to about `0.66 s`.
- The runtime writes new kick sprites by round-robin overwrite of the next sprite slot. There is no "drop-on-full" branch in BPS, so a full sprite pool cannot prevent new sprite state from being written.
- BPS reads by-value audio snapshots from `EffectContext` each render frame. It does not store or process historical audio buffers in the effect.
- The current BPS spawn source is a one-frame kick event from `ctx.audio.isKickHit()`. If that upstream event becomes sparse/noisy, BPS will look sparse/random even when the sprite pool itself is functioning.

Current likely explanation:

- The "buffer fill" sensation is more likely a combination of visible sprite saturation/overwrite, one-frame kick-trigger quality, and renderer timing pressure than a BPS-owned audio buffer.
- The post-upload serial samples already showed timing pressure: FPS below 120, frame average slightly over the 8.33 ms period, and an increasing drop counter.

Evidence gap:

- Need a targeted BPS-2 row that records `bps_kick_trigger`, `bps_sprite_spawn`, `bps_active_sprites`, frame drops, and Captain's audio timing observation over at least the first 8-10 seconds of the same music type.

## 2026-05-07 Design Lock

Captain decision:

- BPS `0x2102` keeps its current identity as an event-sprite class.
- The immediate BPS repair task is complete for this workstream: silence/dark behaviour is visually confirmed, and the candidate patch removes the raw RMS/gHue background corruption.
- BPS is not visually excellent. It works as a class of effect, but it is visually unsatisfactory for the current crown-jewel direction.
- Do not keep iterating BPS in this lane. It may be explored and developed later as its own class, but not right now.
- The desired new class is **Hybrid/V1 Waveform Pull-In**.

Hybrid/V1 Waveform Pull-In definition:

- This is a new audio-reactive effect class, separate from BPS.
- The fixture should read as centre-organised around LEDs 79/80, while the primary visual transport can be edge-to-centre.
- The centre is the focal impact/lock point, not necessarily the transport launch point.
- This class is related to Waveform Hybrid/V1 behaviour and should be treated under the "inward to 79/80" centre-origin allowance, not as arbitrary linear edge sweeping.
- This class should use musical endpoints and waveform/history semantics to own structure; RMS may shade or gate, but must not become a cheap amplitude bed.

Visual references captured from Captain:

- Images supplied on 2026-05-07 show large smooth waveform-like fields, centre-focal colour energy, and plate-scale trails rather than discrete node transport.
- These references are visual direction only; they are not hardware validation evidence for an implementation.

Snapwave consideration:

- Captain identified Snapwave as a potential crown-jewel ancestor/reference before moving on.
- Source references to preserve:
  - `/Users/spectrasynq/Workspace_Management/Software/LightwaveOS_Official/SNAPWAVE_ORIGINAL_IMPLEMENTATION.md`
  - `/Users/spectrasynq/Workspace_Management/Software/LightwaveOS_Official/SNAPWAVE_MODE_DEEP_TECHNICAL_ANALYSIS.md`
  - `/Users/spectrasynq/Workspace_Management/Software/K1.Landing-Page/docs/00-SNAPWAVE-ANALYSIS-INDEX.md`
  - `/Users/spectrasynq/Workspace_Management/Software/K1.Landing-Page/docs/SNAPWAVE-MOTION-ALGORITHM-ANALYSIS.md`
  - `/Users/spectrasynq/Downloads/LightwaveOS_Official/SNAPWAVE_DEBUG_NOTES.txt`
- Snapwave reference traits verified from the supplied docs:
  - history shifting / spatial queue;
  - dynamic trail fading;
  - chromagram-driven time oscillation;
  - `tanh()` snap/normalisation;
  - harmonic colour from pitch-class energy;
  - mirrored centre organisation.

Decision boundary:

- Do not call the future Pull-In class "BPS fixed".
- Do not replace BPS in-place without a dedicated design/implementation task.
- Do not port Snapwave directly without adapting it to K1 AP-VP semantics, centre/inward topology, no-rainbow restraint, render-path heap rules, dt-correct timing, and the 2.0 ms effect-code ceiling.
