# K1 Spec Recommendations — Pathmode `spectrasynq/k1`

**Source:** Read-only audit of `firmware-v3/` against the Pathmode product spec at <https://pathmode.io/spectrasynq/k1>.
**Method:** Four parallel subagents (Performance + Audio DSP, Render contract + Effects, Hardware + Reliability, Spec/Reality reconciliation), each <30K tokens, sandboxed read-only. Synthesised + dedupelicated below.
**Status:** Draft for Captain review. Approved items get pushed to Pathmode as draft IntentSpecs against productId `efc1a976-9527-4578-8401-634a7f86096d` (K1).
**Confidence convention:** every claim cites file paths or symbols; `[UNVERIFIED]` flags items the subagents did not directly inspect.

---

## Captain decisions resolved 2026-04-26

The five flagged decisions at the end of the original draft have been answered. Affected recommendations have been updated in place; this section is the audit trail.

1. **North Star (REC-RECONCILE-2):** Pursue parallel RMT channels to halve the FastLED wire-time floor and keep `<8 ms` defensible. This is now a hardware-firmware coordinated workstream rather than a marketing redefinition.
2. **BeatTracker (REC-PERF-2):** The rebuild has already landed — comb-tooth (OSS ring → CBSS peak detection → BPM density histogram with σ=2 BPM Gaussian → argmax winner with bidirectional subharmonic enhancement 0.5/0.33 and log-Gaussian prior at `tempoPriorBpm`), Captain-approved 2026-03-01, shipped in commit `fab1802d` on 2026-03-20. The recommendation has been restated as "lock this implementation behind a versioned constant + regression gate + Captain-re-approval-required constraint." ACF is rejected per HANDOVER §2. Goertzel `TempoTracker` (`firmware/v2/.../TempoTracker.h`, 48–143 BPM) is retained as documented fallback only.
3. **PipelineCore (REC-PERF-5, REC-RECONCILE-4):** Deprecate from production builds now. ESV11 becomes the sole production audio backend; PipelineCore retained only in `native_test` for offline regression.
4. **Inversion Bypass Protocol (REC-RENDER-3):** Enforce via paired-CI gate. Both markers (`@spatial-mapping: inverted` comment + `[INVERTED]` registry prefix) required; CI rejects mismatched pairs.
5. **silentScale enforcement (REC-PERF-4, REC-RECONCILE-3):** Move from convention to framework-enforced via a post-render multiplier in `RendererActor`, gated by a `SilenceBehaviour` enum field on each effect's static `EffectDescriptor`. Default = `FrameworkFade` (framework multiplies output by `controlBus.silentScale`); explicit opt-outs are `InternalFade` (effect owns its own non-linear curve) and `IntentionallyPersistent` (status/OOBE/idle pulse — must stay visible). Both opt-outs are framework no-ops; they differ only in code-review intent and must be greppable so the opt-out list stays auditable. Lint and runtime sampler are layered insurance, not the primary mechanism.

   **Captain's reasoning (verbatim):**
   - *Aligns with stated workspace pattern* — "Reactive silence should use silentScale/global silence behaviour rather than ad-hoc per-effect gates."
   - *Removes both human failure modes at once* — "forgot to opt in" disappears (framework opts every effect in); "forgot to multiply" disappears (framework does the multiply). Remaining failure mode — "intentionally persistent effect didn't tag itself opt-out" — is visible: the effect goes dark at silence, and the author notices in five seconds of hardware testing.
   - *Compile-time decidability* — opt-out is a `constexpr` field in the descriptor, not a runtime branch. Audit by grepping the tag value. No virtual call, no per-frame cost beyond one multiply per LED (640 floats × 1 mul ≈ 5 µs at 240 MHz, well under the 2.0 ms ceiling).
   - *Repositions the 12 currently-correct effects* — they become optional reviewers, not gatekeepers. They can keep their internal fades (tag `InternalFade`) or strip them (tag `FrameworkFade`); either is correct.

   **Honest tradeoffs:** linear fade only at the framework layer (effects wanting gamma-corrected or exponential curves declare `InternalFade` and own the maths); doesn't catch bugs *inside* the backend's `silentScale` computation (that's a separate contract — see runtime sampler reframing below); migration is one CL, not 140.

   **Layered safety nets (additive, not enforcement):**
   - Lint check (~30 lines Python) flags any effect descriptor missing the `silence` field, OR any effect whose `render()` multiplies by `silentScale` while declaring `FrameworkFade` (double-fade smell).
   - Runtime sampler reframed: not "did each effect fade?" but "is `controlBus.silentScale` actually reaching 0 within the documented window during real silence?" — backend correctness check on the audio output, instrumented once, not 140 times.

---

## How to read this document

Each recommendation is shaped to drop into Pathmode's IntentSpec schema (title / objective / outcomes / constraints / edgeCases / verification). Severity ranks: **critical** (must reconcile before constitution publication), **high**, **medium**, **low**.

There are three categories of finding:

1. **Spec-text reconciliations** — places where the current Pathmode wording contradicts firmware reality. These need wording changes on Pathmode itself. Listed first.
2. **Net-new intents** — recommendations to add to Pathmode as new draft IntentSpecs. Grouped by subsystem, ranked by severity.
3. **Existing draft intent revision** — the current draft (`c3c356bc-...` "canonical product constitution") has at least one outcome that's incompatible with firmware reality and needs revision before approval.

---

## Part 1 — Spec-text reconciliations (critical)

These must land in Pathmode wording before any deeper intents are built on top, otherwise downstream intents will inherit broken premises.

### REC-RECONCILE-1 — Distinguish K1-the-device from peripheral control surfaces (WiFi/app conflation)
- **Severity:** high
- **Current Pathmode wording (verbatim):** *"K1 is a dedicated hi-fi instrument for music visualization. We bet that by stripping away 'smart' distractions—apps, cloud, and latency—we can create a physical medium where light is as immediate and high-fidelity as the audio itself."* Combined with target audience: *"…over app-controlled smart home gimmicks."*
- **Firmware reality:** K1 firmware is intentionally a Wi-Fi **Access Point** (`firmware-v3/src/network/WiFiManager.h` — AP-only, STA explicitly disabled and never to be enabled per `CLAUDE.md`). The K1 device itself is appless and cloudless. **But** `lightwave-ios-v2` and `tab5-encoder` are first-class control surfaces that connect *to* K1 over its local AP. Pathmode's current language conflates "no apps/cloud" (true of the device) with "no companion software" (false — Tab5 and iOS are intentional peripherals on a strictly local network).
- **Proposed reconciliation (replacement product vision wording):**
  > "K1 is a dedicated hi-fi instrument for music visualisation. The device itself strips away cloud and third-party app dependencies — eliminating the latency, vendor lock-in, and privacy compromises of smart-home accessories. Control is local-only: tactile encoders on the device, plus optional companion peripherals (Tab5 desktop controller, iOS remote) connected through K1's own Wi-Fi access point. K1 never reaches the public internet and exposes no cloud APIs."
- **Why this matters:** Without this clarification, the existing draft intent (c3c356bc) — which forbids `WiFi.h` references in firmware PRs — would block the production network stack on the next PR. See REC-EXISTING-1 below.

### REC-RECONCILE-2 — Frame the <8 ms North Star as a measured budget, with parallel RMT as the path to make it defensible
- **Severity:** critical
- **Current Pathmode wording (verbatim):** *"End-to-end latency < 8ms (Audio-to-Photon)."*
- **Firmware reality:** `firmware-v3/CONSTRAINTS.md` and `RendererActor.cpp` only commit to a **2.0 ms per-frame effect render ceiling**. The remaining stages — I2S DMA capture, ESV11 50 Hz hop processing, ControlBus cross-core handoff, and the FastLED/RMT WS2812 wire transfer (~6.3 ms documented for the LED-show step alone) — are not budgeted publicly. Total wall-clock latency is plausibly 8–10 ms today and untested at p99. With a single RMT channel driving both 160-LED strips serially, the 6.3 ms transfer is the dominant cost.
- **Captain decision (2026-04-26):** Pursue parallel RMT channels — drive each 160-LED strip on a dedicated RMT channel — to halve the wire-time floor and keep `<8 ms` defensible. This is a coordinated firmware-hardware workstream, not a marketing redefinition.
- **Proposed reconciliation (replacement North Star wording):**
  > "End-to-end latency (microphone sample → first photon emitted) targets ≤ 8 ms at the 99th percentile under steady-state operation. Decomposition: I2S capture ≤ 1 ms, ESV11 DSP hop ≤ 1 ms (50 Hz cadence), cross-core publish ≤ 100 µs, effect render ≤ 2 ms, FastLED/RMT WS2812 transmit ≤ 3.2 ms (parallel RMT channels per strip — required to meet the budget). The render ceiling is enforced per-frame; remaining stages are measured continuously and exported via the device telemetry endpoint."
- **Why this matters:** A North Star that can't be measured can't be defended. With current single-channel RMT the spec is aspirational. The parallel-RMT path makes the budget reachable: 320-LED transfer drops from 6.3 ms to ~3.2 ms (theoretical floor for 160 LEDs @ 800 kHz WS2812 ≈ 3.2 ms), leaving headroom for the audio chain. REC-PERF-1's instrumentation then proves it at runtime.

### REC-RECONCILE-3 — Reframe the silentScale silence contract as framework-enforced (post-render multiplier with descriptor opt-out)
- **Severity:** high
- **Current Pathmode wording (verbatim):** *"REACTIVE Pattern Contract: Patterns must respect the silence contract (fade to black via silentScale)."* And: *"Silence Gate Logic: Single RMS test (clamp01(rmsUngated) < threshold) + sustain timer + EMA fade-to-black."*
- **Firmware reality:** `silentScale` exists as a `ControlBus` field (verified at `ControlBus.h:217` — `silentScale` field, `:218` — `isSilent`). Per-effect opt-in via `needsSilenceGate()` in `effect_ids.h` covers ~12 of ~140+ audio-reactive effects. There is **no static lint, no runtime assert, and no CI gate** today. The spec promises a contract the firmware does not keep for the majority. `[UNVERIFIED: whether `PatternRegistry.cpp` already applies a global silentScale multiplier post-render — file too large for verification subagent's budget; corroborating CONSTRAINTS.md references the gate but doesn't describe a global multiplier.]`
- **Proposed reconciliation (replacement principle wording, per Captain decision 2026-04-26):**
  > "Audio-reactive patterns honour a silence contract enforced by the rendering framework: after each effect's `render()` returns, `RendererActor` multiplies the output buffer by `controlBus.silentScale` unless the effect's static `EffectDescriptor` declares `SilenceBehaviour::InternalFade` (effect owns a non-linear curve) or `SilenceBehaviour::IntentionallyPersistent` (status / OOBE / idle pulse). Default is `SilenceBehaviour::FrameworkFade`. The opt-out enum value is a `constexpr` descriptor field — auditable by grep, with no virtual-call cost. Lint and a backend-side runtime sampler (verifying that `silentScale` actually reaches 0 within the documented window during real silence) provide layered insurance but are not the primary mechanism."
- **Why this matters:** The current Pathmode wording asks every effect author to opt in, fetch `silentScale`, and apply it consistently across all output paths — three failure points each. 140 non-compliant effects prove the convention model doesn't scale. Captain's framework-multiplier model removes both human failure modes at once: "forgot to opt in" disappears (framework opts every effect in); "forgot to multiply" disappears (framework does the multiply). The remaining failure — "intentionally persistent effect didn't tag itself opt-out" — is visible (effect goes dark at silence; author notices in 5 s of hardware testing). See REC-PERF-4 for the implementation intent.

### REC-RECONCILE-4 — Add ESV11 as the named production audio backend; declare PipelineCore deprecated
- **Severity:** high
- **Current Pathmode wording:** silent on audio backend.
- **Firmware reality:** Top-level `CLAUDE.md` is explicit: *"`esp32dev_audio_esv11` — ACTIVE / PRODUCTION (64-bin Goertzel, stable audio processing). `esp32dev_audio_pipelinecore` — BROKEN / DO NOT USE."* `HANDOVER_BeatTracker.md` independently corroborates: beat tracking on PipelineCore is non-functional after the Goertzel→FFT migration. Subagents 1 and 4 both surfaced this gap; the verification subagent confirmed the corroboration.
- **Proposed reconciliation (new key decision):**
  > "Audio analysis backend: ESV11 (64-bin Goertzel, 32 kHz sampling, 50 Hz hop). PipelineCore is deprecated for production builds and retained only for offline regression harnesses. New audio-feature work targets ESV11 exclusively until the PipelineCore beat-tracker regression is resolved with passing real-audio tests."
- **Why this matters:** The Pathmode spec is currently mute on which backend ships. A reader can't tell from the spec whether new effects can rely on PipelineCore-only fields (`bins256[]`, etc.). Without this clarification, downstream intents will assume universality and fail.

---

## Part 2 — Performance + Audio DSP intents (5 net-new)

### REC-PERF-1 — Instrument and enforce the audio-to-photon latency budget
- **Severity:** critical
- **Objective:** Make the <8 ms North Star measurable at runtime so regressions surface in CI and in the field rather than at customer-facing benchmarks. Today the chain has no per-stage timestamping, so a regression in any single stage is invisible until total latency is wrong.
- **Outcomes:**
  - I2S capture-to-first-sample latency continuously sampled and exposed via `/api/system/perf-snapshot`.
  - ESV11 hop latency (per-50Hz cycle) tracked with p50/p95/p99.
  - AudioActor → RendererActor publish latency measured (SnapshotBuffer lock-free handoff).
  - Per-effect render time tracked (max + p99 over rolling 100-frame window, segregated from FastLED.show()).
  - End-to-end timestamp emitted in a single telemetry frame: `{capture_ts, dsp_done_ts, publish_ts, render_done_ts, photon_ts}`.
  - CI gate: build fails if any p99 stage exceeds its budget on a known reference workload.
- **Constraints:**
  - No heap allocation in render path for instrumentation.
  - `esp_timer_get_time()` clock alignment between Core 0 and Core 1 (verify drift).
  - <100 µs total telemetry overhead per frame.
  - Must work on both `esp32dev_audio_esv11` and (deprecated) `pipelinecore` envs.
- **Edge cases:**
  - Audio dropout → stage marked stale, not last-known-value.
  - Trinity sync override → photon timestamp anchors to transport, not detector.
  - Render exceeds budget occasionally → histogram captures max + percentile, not just average.
- **Verification:** Unit test with mock actors and known timestamps. E2E: 5-min capture on K1 v2 hardware, plot p99 latency, must hold ≤ 8 ms.
- **Evidence:** `firmware-v3/src/core/actors/RendererActor.cpp` (header comment notes "typical 2–4ms render, 6.3ms LED show"); `firmware-v3/CONSTRAINTS.md` (frame budget table); `firmware-v3/src/audio/contracts/AudioTime.h` (class exists, no per-stage logging).

### REC-PERF-2 — Lock the landed comb-tooth BeatTracker behind a regression gate, version constant, and Captain re-approval requirement
- **Severity:** critical
- **Objective:** Prevent another silent overwrite of `BeatTracker.cpp` like the Feb 2026 ACF regression. The comb-tooth rebuild has already landed (Captain-approved 2026-03-01, commit `fab1802d`, 2026-03-20) — this recommendation is about *locking* that implementation, not relitigating the algorithm choice.
- **Captain-approved algorithm (verbatim, 2026-03-01):** OSS ring → CBSS peak detection → BPM density histogram (σ=2 BPM Gaussian) → argmax winner with bidirectional subharmonic enhancement (0.5/0.33) and log-Gaussian prior at `tempoPriorBpm`. Implementation landed in commit `fab1802d` (2026-03-20). ACF is rejected per `HANDOVER_BeatTracker.md` §2; corroborating evidence in claude-mem #35423, #35484, #35333, #37915. Goertzel `TempoTracker` (`firmware/v2/.../TempoTracker.h`, 48–143 BPM) is retained as documented fallback only — not the chosen path.
- **Outcomes:**
  - `BeatTracker.cpp` carries a `kBeatTrackerAlgoVersion` constant and a header comment block stating the comb-tooth approach is in use and why ACF was rejected (cross-references HANDOVER §2).
  - `test_spine16k_acceptance` must achieve **≥ 9/12 coherent** on the 12-clip real-drum-loop suite before any release tag (gate retained from the Feb regression learning).
  - Pre-commit hook blocks commits that touch `BeatTracker.cpp` without a corresponding test-result artefact.
  - `Pathmode.health_metrics`: BPM lock confidence reported via telemetry.
- **Constraints:**
  - DSP Spine v0.1 frozen parameters (Fs=16 kHz, hop=128, FFT=512).
  - 60–240 BPM target range.
  - <50 ms tempo-update latency (hop ≈ 8 ms, full update every ≈ 1.5 s acceptable).
  - **Algorithm change requires Captain re-approval. Any future swap away from comb-tooth (e.g. to Goertzel or hybrid) must update `HANDOVER_BeatTracker.md` and bump `kBeatTrackerAlgoVersion`.** *(Captain directive 2026-04-26.)*
- **Edge cases:**
  - Weak/ambiguous beat (jazz, classical) → confidence reports low, no guess.
  - Octave error (60 detected as 120) → CBSS phase tracker + log-Gaussian prior at `tempoPriorBpm` disambiguates.
  - Tempo change at drop → re-acquires within 4 bars without oscillation.
- **Verification:** `pio test -e native_test_esv11_music -f test_spine16k_acceptance`. Manual smoke: K1 hardware locks 120 BPM kick within 2–3 kicks. Algorithm version constant present and matches the documented value in HANDOVER.
- **Evidence:** `firmware-v3/HANDOVER_BeatTracker.md` (algorithm decision + ACF rejection); commit `fab1802d` (2026-03-20) implementation; `firmware-v3/src/audio/pipeline/BeatTracker.cpp`; `firmware-v3/test/test_spine16k/test_spine16k_acceptance.cpp`; claude-mem cross-references #35423, #35484, #35333, #37915.

### REC-PERF-3 — Document and version the ControlBus field-population contract
- **Severity:** high
- **Objective:** ControlBusFrame has 40+ fields populated by different paths. Effects can't reason about which fields are valid under which backend, when fields are stale, or what "stale" means. Make it a versioned, navigable contract so consumer effects can be safely written.
- **Outcomes:**
  - `firmware-v3/docs/audio-visual/control-bus-field-manifest.md` — a table of every field with: producer (ESV11 / PipelineCore / both), update cadence, valid range, invalidation rule, primary consumers.
  - `ControlBus.h` annotates each field group with a `// ESV11 only:` / `// Both:` tag.
  - Doxygen comments on `ControlBusFrame` declare warm-up window (first 5 frames invalid; STM warm after 100 frames).
  - Frame-version field stamps each published frame so consumers can detect format drift.
  - Linter rule: a new field added to `ControlBus.h` without a manifest entry fails CI.
- **Constraints:**
  - No breaking changes to binary frame layout (field offsets stable).
  - Manifest must be queryable by field, by backend, by consumer.
  - Must remain current as new fields are added (e.g. motion-semantics: `timing_jitter`, `syncopation_level`).
- **Edge cases:**
  - ESV11 backend with chord detection unavailable → `chordState.confidence=0`, `type=NONE` explicitly nulled, never stale.
  - Audio dropout → `audioConfidence` decays; `rms` retains last value; `isSilent` explicitly true.
  - Trinity transport override → onset reflects transport, `reliable=false`.
- **Verification:** Manual code audit of effects consuming ControlBus; harness verifies all manifest-declared fields populated under both backends through dropout/recovery.
- **Evidence:** `firmware-v3/src/audio/contracts/ControlBus.h` (lines 52–237); `firmware-v3/docs/audio-visual/audio-visual-contract-surface.md`; `firmware-v3/src/audio/backends/esv11/EsV11Adapter.h`.

### REC-PERF-4 — Enforce the silentScale silence contract by framework post-render multiplier with descriptor opt-out (Captain decision 2026-04-26)
- **Severity:** high
- **Objective:** Move the silence contract from convention to framework-level guarantee. The current `needsSilenceGate()` model requires every effect author to do three things correctly (opt in, fetch `silentScale`, apply consistently); 140 non-compliant effects prove that model doesn't scale. Make `RendererActor` enforce silence behaviour by default; relegate per-effect customisation to a `constexpr` descriptor field that's compile-time decidable and grep-auditable.
- **Outcomes:**
  - `EffectDescriptor` gains a `constexpr SilenceBehaviour silence{SilenceBehaviour::FrameworkFade};` field. Enum:
    - `FrameworkFade` (default) — RendererActor multiplies output by `controlBus.silentScale` post-render, gated on `PatternRegistry::isAudioReactive(effectId)`. Non-audio-reactive effects are inert under this tag — they retain full brightness during silence (matches existing `needsSilenceGate()` precedent at `effect_ids.h:596–598` which gates on `effectFamily(id) >= FAMILY_SHAPE_BANGERS`).
    - `InternalFade` — effect owns its own (non-linear, gamma-corrected, exponential) fade; framework leaves output alone.
    - `IntentionallyPersistent` — effect must remain visible during silence (status, OOBE, idle pulse); framework leaves output alone.
  - `RendererActor` post-render hook applied between `effect->render(ctx)` and `FastLED.show()`:
    ```cpp
    if (descriptor.silence == SilenceBehaviour::FrameworkFade &&
        PatternRegistry::isAudioReactive(effectId)) {
      const float s = ctx.controlBus.silentScale;
      for (int i = 0; i < kLedCount; ++i) ctx.leds[i].nscale8(uint8_t(s * 255.0f));
    }
    ```
  - Migration is one CL plus per-effect tagging only for the handful claiming `InternalFade` or `IntentionallyPersistent`. Most effects need no change.
  - `needsSilenceGate()` deprecated; the ~12 currently-correct effects either keep their internal fade (tag `InternalFade`) or strip it and rely on the framework (tag `FrameworkFade`).
  - **Layered insurance (additive, not enforcement):**
    - Lint check (~30 lines Python, runs in CI): flags any effect descriptor missing the `silence` field, OR any effect whose `render()` multiplies by `silentScale` while declaring `FrameworkFade` (double-fade smell).
    - Runtime sampler reframed: instead of "did each effect fade?" the sampler asks "is `controlBus.silentScale` actually reaching 0 within the documented window during real silence?" — this is a backend correctness check on the audio output, instrumented once, not per-effect.
- **Constraints:**
  - Framework multiplier ≤ 5 µs at 240 MHz on 320 LEDs (640 floats × 1 mul ≈ well under 2.0 ms ceiling).
  - `silenceBehaviour` must be `constexpr` — no runtime branch, no virtual call.
  - Both opt-out variants (`InternalFade`, `IntentionallyPersistent`) must be greppable so the auditable opt-out list stays current.
  - Algorithm change (e.g. moving from linear to gamma-corrected at framework layer) requires Captain re-approval and an updated `kSilenceFadeFrameworkVersion` constant.
- **Edge cases:**
  - Effect declares `InternalFade` but doesn't actually fade → goes bright in silence; visible failure during 5 s of hardware testing; not caught at compile-time. Lint warns if such an effect's `render()` doesn't reference `silentScale`.
  - Effect declares `FrameworkFade` AND multiplies by `silentScale` internally → double-fade (output drops to silence too aggressively); lint flags as smell.
  - Audio backend bug holds `silentScale = 1.0` during actual silence → every effect glares regardless of behaviour tag; runtime sampler catches this at the *backend* contract, not the effect contract.
  - Boot-time or mic-disconnect leaves `silentScale = 1.0` (the field's init value at `ControlBus.h:217`) → first frames after power-on or after I2S hop count freezes show full-brightness audio-reactive effects against silent input. Mitigation: ControlBus init forces `silentScale = 0.0f` until first audio frame validated; AudioActor watchdog forces `silentScale → 0` if hop count freezes ≥ 500 ms (pair with REC-OPS-3 SPH0645 documentation).
  - Silence-boundary chatter during quiet musical passages → rapid 1.0 ↔ 0.0 oscillation strobes effects. Mitigation: 10 s decay hysteresis already in place at `ControlBus.h:217–222`; PipelineAdapter Schmitt trigger (open ≥ 0.02, close ≤ 0.005, 200 ms hold) prevents re-arm chatter. Acceptance: < 5 transitions/s during a 30 s "quiet music" capture.
  - Zone Composer + Transition Engine layer ordering → multiplier applied to wrong layer; cross-fades show jarring brightness steps. Mitigation: framework multiplier applies in `RendererActor` AFTER ZoneComposer composite AND AFTER Transition Engine blend, immediately before `FastLED.show()`. Verification: single-effect, multi-zone, and transitioning capture all show identical silentScale-aware output.
- **Verification:** Compile-time: descriptor field present in every effect; lint passes. Runtime: capture LED output during a recorded silence window; `FrameworkFade` effects must extinguish to ≤ ε within τ; `IntentionallyPersistent` effects retain brightness; `InternalFade` effects honour their declared curve. Backend sampler verifies `silentScale → 0` within documented window during a known-silent recording.
- **Evidence:**
  - **Contract surface:** `firmware-v3/src/audio/contracts/ControlBus.h:217` (`silentScale` field declaration, "0.0=silent, 1.0=active (multiply with brightness)" + 10 s hysteresis comment at line 222); `firmware-v3/src/plugins/api/EffectContext.h:261` (effect-side accessor `ctx.audio.silentScale()`; line 623 default fallback `return 1.0f`).
  - **Current convention sites (to be retired/migrated):** `firmware-v3/src/config/effect_ids.h:596` (current opt-in registry `inline bool needsSilenceGate(EffectId id)`, family-range check on `FAMILY_SHAPE_BANGERS` and above); `firmware-v3/src/core/actors/RendererActor.cpp:1818, :2034` (only two consumers of `needsSilenceGate()`; post-render hook insertion points for the new framework multiplier).
  - **Effects already reading `silentScale()` internally (lint/migration audit list — at least 14):** `SbK1WaveformEffect.cpp`, `SbK1WaveformHybridEffect.cpp`, `LGPSchlierenFlowAREffect.cpp`, `LGPTalbotCarpetAREffect.cpp`, `LGPSpirographCrownAREffect.cpp`, `LGPMachDiamondsAREffect.cpp`, `LGPWaterCausticsAREffect.cpp`, `LGPCymaticLadderAREffect.cpp`, `LGPSuperformulaGlyphAREffect.cpp`, `LGPSpectrumBarsEffect.cpp`, `LGPBassBreathEffect.cpp`, `HeartbeatEsTunedEffect.cpp`, `BeatPulseBloomEffect.cpp`, `BeatPulseTransportCore.h:247` (`outGain01` parameter), `GeneratedAiryCometEffect.h:55`.
  - **Documented contract:** `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md:693` (item 3 of "Three systems silently modify post-processing": *"Silence Gate — Audio-reactive effects fade to black during silence"*); cross-refs `firmware-v3/CONSTRAINTS.md § Effect Behavioral Gates`.
  - **Detection-side mechanism:** PipelineAdapter Schmitt trigger — open at `rmsUngated ≥ 0.02`, close at `< 0.005` with 25-hop (~200 ms) hold (recorded in `MEMORY.md` "Silence Gate Hysteresis Fix").
  - **Verification tooling:** `tools/led_capture/` — frame-format v2 (1009 B) carries `silentScale` and audio metrics in the trailer; sustained 30–40 FPS capture supports empirical fade-curve verification.
  - **Workspace memory cross-references:** claude-mem #46016 (2026-04-26) silence-gating architecture observation; claude-mem #45984 (2026-04-25) centre-origin compliance audit (signposts the broader compliance methodology); workspace pattern rule: *"Reactive silence should use silentScale/global silence behavior rather than ad-hoc per-effect gates."*

### REC-PERF-5 — Make ESV11 the sole production audio backend; deprecate PipelineCore from production builds
- **Severity:** high
- **Objective:** Eliminate the cost of carrying two divergent DSP paths in production firmware. Documented status (`CLAUDE.md`: ESV11 = production, PipelineCore = broken) is not reflected in build envs, ControlBus dual-paths, or test matrix; new contributors waste time debugging PipelineCore behaviour that will never ship.
- **Outcomes:**
  - PipelineCore build env removed from production CI; retained in `native_test` only.
  - `AudioActor.{h,cpp}`: PipelineCore conditional removed; ESV11 sole compile path.
  - ControlBus field documentation aligns to ESV11 reality (50 Hz hop, Goertzel spectrum).
  - `firmware-v3/docs/audio/k1-audio-architecture.md` — single source of truth (ESV11 @ 32 kHz, 256-hop, Goertzel tempo, onset detector, bins64 spectrum, published @ 50 Hz).
  - Migration note for any user content that relied on PipelineCore-only fields (e.g. `bins256[]`).
- **Constraints:**
  - `native_test_pipelinecore` may remain for offline regression.
  - Frame layout binary-compatible (no offset changes).
  - ESV11 must support every ControlBus field PipelineCore consumers used (verify: tempo, beat, onset, spectrum, chroma).
- **Edge cases:**
  - Show file referencing a PipelineCore-only field → graceful zero-fill + warning at load time.
  - Lip-sync demo running PipelineCore in v3 → migration note in changelog.
- **Verification:** Build `esp32dev_audio_esv11_k1v2_32khz`, smoke-test all effects; `native_test_esv11_music` suite passes.
- **Evidence:** `firmware-v3/CLAUDE.md` (audio backends section); `firmware-v3/HANDOVER_BeatTracker.md`; `firmware-v3/platformio.ini`; `firmware-v3/src/audio/AudioActor.h` (dual conditional backends).

---

## Part 3 — Render contract + Effects intents (5 net-new)

### REC-RENDER-1 — Per-effect render-budget regression harness (combined with REC-PERF-1's instrumentation)
- **Severity:** high
- **Objective:** Detect 2.0 ms render-ceiling violations per effect, not just globally. Today only `RenderStats` aggregate exists; an individual effect can blow budget under specific parameter combinations and the only signal is degraded UX.
- **Outcomes:**
  - Build/CI step profiles each effect at canonical `ctx.speed=25` and reports baseline + variance.
  - CI gate fails on any effect exceeding 2.0 ms p99.
  - `RendererActor` debug captures (TAP_A/TAP_B) record per-effect timing histograms.
  - Public ranking report (slowest first) maintained per release.
  - Regression DB tracks per-effect timing across firmware versions.
- **Constraints:**
  - Measurement excludes `FastLED.show()` and colour correction (they're not the effect's responsibility).
  - Variable-speed effects: report at `ctx.speed=25` baseline, variance range published.
  - <10 µs profiling overhead in hot path.
- **Edge cases:**
  - Effect with PSRAM allocation in `init()` → baseline excludes allocation.
  - Frame-dependent variance (1.2–1.8 ms flicker) → report variance, hard ceiling 2.0 ms.
  - Transition effects compose multiple effects → measure total composed.
- **Verification:** Profile 20 effects (10 fast <1.0 ms, 10 slow 1.5–2.0 ms) on ESP32-S3 hardware, confirm ceiling.
- **Evidence:** `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md` Part 1; `firmware-v3/CONSTRAINTS.md` (Timing Budget); `firmware-v3/src/core/actors/RendererActor.h` (`RenderStats`).

### REC-RENDER-2 — Validate the colour-correction skip-list per effect family
- **Severity:** high
- **Objective:** Prevent colour correction from corrupting physics-based effects (interference, wavelength-accurate, organic optical). The skip list (`isLGPSensitive`, `isStatefulEffect`) is informally maintained — a new physics-family addition without the flag will silently render with wrong colours.
- **Outcomes:**
  - `PatternMetadata.skip_color_correction: bool` + free-text `reason` field.
  - Audit report: every physics-family effect listed with skip status and justification.
  - CI check: any effect tagged in an Interference/Quantum/Advanced-Optical family without skip flag fails.
  - AU library effects retain backward compatibility (skip list is additive).
- **Constraints:**
  - Audio-reactive effects must NOT skip colour correction (they need exposure control).
  - Tone-mapping list (`needsToneMap()`) unaffected — orthogonal concern.
- **Edge cases:**
  - Effect blends physics + audio in different layers → flag layers independently.
  - Effect manually disables correction → registry override + documented reason.
- **Verification:** 5 interference + 5 organic + 5 quantum effects parity test (correction on/off).
- **Evidence:** `firmware-v3/src/effects/PatternRegistry.cpp` (`shouldSkipColorCorrection()` switch); `firmware-v3/src/effects/PatternRegistry.h` (`PatternMetadata`); `firmware-v3/CONSTRAINTS.md` (Effect Behavioural Gates).

### REC-RENDER-3 — Validate the Inversion Bypass Protocol with paired CI checks (or demote to guideline)
- **Severity:** medium
- **Objective:** Either turn the protocol into an enforced rule or stop calling it a hard rule in the spec. Today: documented as MUST in Pathmode, but Subagent 4's grep returned **zero** instances of `@spatial-mapping: inverted` and **zero** `[INVERTED]` registry entries. The protocol is currently a ghost.
- **Outcomes:**
  - Reference inverted test effect added (proves bypass mechanism actually works).
  - CI gate: an effect with `[INVERTED]` registry prefix MUST contain `@spatial-mapping: inverted` comment, and vice-versa.
  - Pre-commit hook surfaces missing pairs locally.
  - Activation logged at runtime when an inverted effect is loaded (auditable).
  - Audit of all 349 effects for unintended inversions that *should* have used the bypass.
- **Constraints:**
  - Centre-origin remains default; bypass is opt-in only.
  - Both markers required (one without the other → CI fail) — prevents accidental inversions.
- **Edge cases:**
  - Author wants to invert one segment of a dual-strip effect → split into two effects.
  - Legacy effect accidentally inverted during v3 migration → add markers + regression test rather than silently fix.
- **Verification:** Reference inverted effect renders correctly; CI test rejects mismatched-marker effects.
- **Evidence:** `firmware-v3/CONSTRAINTS.md` (Forbidden Patterns); `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md` § 2.3; `firmware-v3/src/config/effect_ids.h`. `[UNVERIFIED: Subagent 4 grep returned zero matches; this conclusion needs the verification subagent to confirm.]`

### REC-RENDER-4 — Static checker enforcing the no-heap-in-render policy (PSRAM + render path)
- **Severity:** medium
- **Objective:** Catch `new`, `malloc`, `String`, `std::vector` push-back, etc. in render-reachable functions at build time rather than at runtime memory exhaustion. Policy is documented in `IEffect.h`'s header block but enforced only by code review.
- **Outcomes:**
  - Static checker (clang AST query or scripted clangd lookup) flags allocation calls in any function transitively reachable from `render()`.
  - `IEffect.h` template includes the canonical PSRAM pattern (pointer member; `heap_caps_malloc` in `init()`; `free` in cleanup).
  - Build emits warnings; CI gates can be set to fail.
  - Audit report: every effect with PSRAM buffer + size.
- **Constraints:**
  - Static/constexpr buffers OK.
  - Allocation in `init()`/`cleanup()` OK.
  - Stack arrays <256 bytes OK.
  - `std::vector` etc. flagged unless pre-reserved in `init()`.
- **Edge cases:**
  - Subpixel renderer / smoothing primitives are pre-allocated → checker skip-list with rationale.
  - Effect creates temporary `SmallBuffer` in render → flag, require static or PSRAM.
- **Verification:** Run on 10 PSRAM-using + 10 simple effects → no false positives.
- **Evidence:** `firmware-v3/src/plugins/api/IEffect.h` (PSRAM ALLOCATION POLICY block); `firmware-v3/src/effects/ieffect/RippleEffect.cpp`; `FireEffect.cpp`; `CONSTRAINTS.md` Memory Budget.

### REC-RENDER-5 — Audit and document hue-sweep ("rainbow") compliance across the 349-effect catalogue
- **Severity:** low
- **Objective:** Confirm the "no rainbows" constraint is actually upheld. Subagent 2 sampled but did not exhaustively audit; manual scan of 349 effects is impractical. A scripted check + spot audit gives high coverage at low cost.
- **Outcomes:**
  - Script greps for hue-sweep anti-patterns (unbounded hue increment, full-range `CHSV(hue, …)` with hue wrapping).
  - Documented safe patterns: palette-driven, narrow-range, chroma-anchored.
  - Lint flags `getColor()` calls with unclamped hue cycling 0–255.
  - Per-effect compliance report.
- **Constraints:**
  - Palette-driven 0–255 lookup is safe by design.
  - Chroma-driven hue (12 notes × 21 hue units → near-full sweep) acknowledged + accepted with rationale.
  - Spatial-position-based hue is intended.
- **Edge cases:**
  - Unbounded `hueOffset` → safe only if wrapped within narrow range; lint detects.
  - Legacy effect cycles hue → convert to time-based fade or mark `LEGACY_LINEAR` exempt.
- **Verification:** Grep + spot-check 10 risky effects.
- **Evidence:** `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md` § 2.4; `firmware-v3/CONSTRAINTS.md` (Forbidden Patterns).

---

## Part 4 — Hardware + Reliability intents (6 net-new)

### REC-OPS-1 — Enable brownout detector with safe-mode recovery to factory preset
- **Severity:** high
- **Objective:** K1 lives in critical-listening environments where mains sag is plausible. A brownout mid-NVS-write today leaves the device undefined. Enable the ESP32-S3 brownout detector and persist a safe-mode flag so the next boot recovers cleanly.
- **Outcomes:**
  - Brownout detector armed at ≥ 2.8 V threshold.
  - Safe-mode boot loads factory preset (Prism) when brownout flag is set.
  - Telemetry: `{"event":"sys.brownout.detected","ts_mono_ms":…}`.
  - Recovery 100% autonomous (no USB / cloud step required — K1 is appless in the field).
- **Constraints:**
  - No false positives during peak LED current draw (~5 A transient).
  - Recovery within 2 s; AP live within 5 s.
  - Compile-time configurable via `platformio.ini`.
- **Edge cases:**
  - Mains sag during NVS commit → flag persists, NVS rolls back, factory preset.
  - Repeating brownout (every 30 s) → distinctive amber LED indicator (per Subagent 3).
- **Verification:** Variable-PSU bench down to 4.5 V; assert recovery + telemetry.
- **Evidence:** `firmware-v3/platformio.ini` (no brownout config); `firmware-v3/src/core/SystemInit.cpp` (no brownout handler); `firmware-v3/CONSTRAINTS.md` (5 A practical limit).

### REC-OPS-2 — Persist panic / WDT logs to NVS with REST retrieval
- **Severity:** high
- **Objective:** Without USB access, Captain has no way to diagnose a unit that became unresponsive in the field. Persist the last 5 panic events to NVS and expose via REST.
- **Outcomes:**
  - WDT trip, assert, panic handler all write to NVS panic ring (256 B/event × 5).
  - REST GET `/api/system/panic-history` returns JSON array.
  - Captures: timestamp, reason, free heap, task name.
  - Ring overwrites oldest when full.
- **Constraints:**
  - No render-loop blocking (async NVS or interrupt-safe queue).
  - <100 ms panic-to-commit latency.
  - Survives reboot.
- **Edge cases:**
  - NVS itself corrupted → write fails silently, boot logs `panic_nvs_failed`.
  - Five panics in 60 s → most recent 5 retained.
- **Verification:** Mock-hang triggers WDT, panic surfaces in `/api/system/panic-history`.
- **Evidence:** `firmware-v3/src/core/system/OtaBootVerifier.h` (telemetry pattern); `firmware-v3/src/core/persistence/NVSManager.h` (NVS API); no current `panic_log` code.

### REC-OPS-3 — Document SPH0645 microphone placement and SNR floor specification
- **Severity:** high
- **Objective:** Audio reactivity is core to K1, but the spec is silent on mic placement and SNR. Different rooms produce vastly different signal quality; the spec must set expectations for both Captain (QA) and customer (setup).
- **Outcomes:**
  - `firmware-v3/docs/audio/k1-mic-placement.md` — distance from speaker (≥ 30 cm), angle, mounting orientation.
  - SNR floor spec: ≥ 40 dB A-weighted in typical hi-fi room (1 kHz, 94 dB SPL reference).
  - AGC explicitly disabled by design (silence gate fades effect during quiet passages instead).
  - Frequency response: 50 Hz–16 kHz ± 3 dB confirmed empirically.
- **Constraints:**
  - Spec applies to K1v1 and K1v2 (different GPIO maps).
  - No hardware redesign — placement-and-software only.
- **Edge cases:**
  - Mic placed on speaker cabinet → spec warns of feedback risk; recommends ≥ 30 cm clearance.
  - Untreated reflective room → spec notes 3–6 dB SNR degradation expected; suggests soft furnishings.
- **Verification:** Calibrated SPL meter measurement in reference room.
- **Evidence:** `firmware-v3/src/audio/backends/esv11/vendor/microphone.h` (DC blocker @ 5 Hz, pin defs); no SNR spec or placement guide today.

### REC-OPS-4 — Build a factory test mode (per-LED + mic loopback + encoder sweep + serial logging)
- **Severity:** medium
- **Objective:** Each unit must be QA'd before shipping. `test_strip_hw.cpp` is a demo, not a comprehensive test. A built-in factory mode (long-press encoder or Serial command) gives the factory floor a deterministic pass/fail.
- **Outcomes:**
  - Entered via 5-second encoder press once AP is live (or Serial command).
  - LED sweep: 320 LEDs × R/G/B individually verified.
  - Mic loopback: 2 s white noise, RMS > -40 dB.
  - Encoder sweep: 80 position reports across full rotation.
  - JSON output to Serial: PASS/FAIL + serial number + timestamp.
  - Result persisted at `/api/system/factory-test-result` for QR-code/label printing.
  - Disabled in production builds via `FEATURE_FACTORY_TEST=1` flag.
- **Constraints:**
  - Total runtime ≤ 2 minutes.
  - Graceful exit if AP not yet live.
- **Edge cases:**
  - LED #N open-circuit → "LED N FAIL (no response)" + halt.
  - Mic under-range → "MIC FAIL (RMS < -40 dB, check connection)".
- **Verification:** Pre-prod K1 with known-good hardware passes; LED unplug triggers fail.
- **Evidence:** `firmware-v3/src/test_strip_hw.cpp` (demo only); no factory mode in `main.cpp`; no encoder position logging.

### REC-OPS-5 — Publish PSU specification and add hardware inrush limiting
- **Severity:** medium
- **Objective:** Power budgeting today is enforced only in firmware (`setMaxPowerInVoltsAndMilliamps(5V, 2000mA)`). One bad config flips the device to flicker/brownout. Document the PSU floor and add a passive inrush limiter so the system is robust to firmware drift.
- **Outcomes:**
  - PSU spec: 5 V @ 3 A continuous, 5 A transient (20 ms peaks), 85–264 VAC input.
  - NTC thermistor (10 Ω @ 25 °C, 1206/1210) in series with main 5 V rail.
  - Firmware brightness cap retained as secondary safeguard (max 160/255).
  - `setMaxPowerInVoltsAndMilliamps(5, 3000)` aligned to 3 A continuous rating.
  - Hardware design doc updated.
- **Constraints:**
  - NTC drop ≤ 0.1 V under full LED load.
  - Allow factory-test full-white sweep without thermal trip.
- **Edge cases:**
  - Undersized 1 A PSU → LEDs dim, no crash.
  - PSU short → NTC limits fault current; PSU thermal shutdown engages.
- **Verification:** Calibrated ammeter under full load; PSU vendor datasheet review.
- **Evidence:** `firmware-v3/src/test_strip_hw.cpp` (FastLED 5V/2A cap); `firmware-v3/CONSTRAINTS.md` (5 A practical max).

### REC-OPS-6 — Extend OTA validation window to 90 s with secondary perf checks
- **Severity:** medium
- **Objective:** Current 30 s OTA validation only checks heap + AP + WebServer. A subtly bad firmware passes those, then degrades under real workload. Extend validation and add async perf checks (frame drops, average render latency) before marking the partition valid.
- **Outcomes:**
  - `VALIDATION_TIMEOUT_MS` 30 000 → 90 000.
  - Async perf check at 60 s queries `/api/system/perf-snapshot` (REC-PERF-1 instrumentation).
  - If frame drops > 0 in 60 s OR avg render > 4 ms → telemetry warning (does not auto-rollback; allows human intervention).
  - Telemetry: `{"event":"ota.boot.perf_check","frame_drops":N,"latency_avg_ms":X,"status":"ok|warning"}`.
- **Constraints:**
  - Async checks must not delay AP startup.
  - Don't fail boot for benign causes (heavy USB logging during dev).
- **Edge cases:**
  - First-effect startup overhead causes 1–2 frame drops → warning logged, boot continues.
  - Pathological 70 s hang → out of validation window; caught by panic log (REC-OPS-2) instead.
- **Verification:** Simulated render stall unit test; OTA a heavy build and watch telemetry.
- **Evidence:** `firmware-v3/src/core/system/OtaBootVerifier.h` (30 s timeout); `firmware-v3/src/core/actors/RendererActor.cpp` (timing available, not exposed to validator today).

---

## Part 5 — Spec gaps Pathmode is silent on (3 net-new)

### REC-GAP-1 — Add the canonical Frequency-Spatial Map as a referenced artefact
- **Severity:** medium
- **Objective:** Pathmode states "Bass = centre, Treble = edges" but doesn't define which frequency bands map where. The firmware has `FrequencyMap.h` with KICK / SNARE / HIHAT / etc. semantic bands; the spec must reference this so effect designers don't hard-code bin indices.
- **Outcomes:**
  - Pathmode key decision added: "Frequency-to-space mapping uses canonical named bands (KICK→centre, SNARE→inner, HIHAT→outer); effects obtain energy via FrequencyMap named-band queries."
  - `FrequencyMap.h` boundaries published in `firmware-v3/docs/audio-visual/`.
- **Constraints:** No effect may hard-code bin indices; named-band queries only.
- **Verification:** Lint catches `bands[0]`-style direct indexing in effects.
- **Evidence:** `firmware-v3/src/audio/contracts/FrequencyMap.h`. `[UNVERIFIED: subagent referenced this file but did not deeply read it.]`

### REC-GAP-2 — Clarify the encoder-routing enforcement boundary (firmware vs Tab5)
- **Severity:** medium
- **Objective:** Pathmode says "Unit A is HARD-CODED for global performance, Unit B is contextual." Reality: K1 firmware exposes 16 generic parameter slots; the *hard-coded global* and *contextual* distinction is enforced by Tab5 UI (`ParameterHandler.cpp`, `ParameterMap.h`), not by K1 firmware. A future Tab5 refactor could break the contract without K1 noticing.
- **Outcomes:**
  - Pathmode wording explicitly states the enforcement layer is Tab5, not K1.
  - Tab5-side IntentSpec mirrors the constraint with its own validation requirement.
  - Cross-link between the two.
- **Constraints:** None (clarification only).
- **Edge cases:** Tab5 firmware update changes encoder routing → must follow constitution amendment process.
- **Verification:** Read of `tab5-encoder/src/ui/` parameter routing confirms enforcement.
- **Evidence:** `firmware-v3/src/encoders/DualEncoderService.h`; `tab5-encoder/src/ui/` (`[UNVERIFIED: subagent 4 referenced but did not deeply audit this layer]`).

### REC-GAP-3 — Add Tab5 local diagnostics panel (no cloud, no USB) for field troubleshooting
- **Severity:** low
- **Objective:** Without a cloud back-end, Captain has no remote diagnostic surface for field units. Surface the existing REST/WS data as a Tab5 read-only diagnostics panel.
- **Outcomes:**
  - `/api/system/diagnostics` returns: uptime, free heap + fragmentation %, last 5 panics (REC-OPS-2), AP SSID + clients, audio snapshot (RMS, silence state, onset), OTA partition + rollback availability, encoder positions, NVS usage.
  - Tab5 "System" → "Diagnostics" reads endpoint, refresh ≈ 5 s.
- **Constraints:**
  - Read-only, no destructive operations.
  - No personal data (no IPs, no usernames).
- **Edge cases:** NVS at 98 % capacity → dashboard warns + suggests restart.
- **Verification:** Tab5 manual usage; payload <1 s on a healthy AP.
- **Evidence:** `firmware-v3/src/network/webserver/V1ApiRoutes.cpp` (REST pattern); `firmware-v3/src/core/system/HeapMonitor.h`; `WsGateway.cpp`.

---

## Part 6 — Existing draft intent: revision required

### REC-EXISTING-1 — Revise draft intent `c3c356bc-2373-47fc-b1d1-f15f605c5035` before approval
- **Severity:** high
- **Why this can't be approved as-is:** Outcome wording reads *"100% of firmware PRs are automatically rejected if they lack the @spatial-mapping tag or reference forbidden 'smart' libraries (e.g., **WiFi.h**, Cloud-init)."* WiFi.h IS used legitimately by `WiFiManager.h` for K1's AP-only network stack. Approving this outcome verbatim would block the production network stack on the next PR.
- **Proposed outcome rewrite:**
  1. *"100% of firmware PRs in `src/effects/` are automatically rejected if they lack a `@spatial-mapping` tag (where applicable) or reference forbidden cloud/remote libraries (e.g. cloud SDKs, third-party telemetry, public-internet HTTP clients). The local AP/WiFi stack used for Tab5 + iOS communication is explicitly permitted in `src/network/`."*
  2. (Keep) *"Linter validates that no new UI tab or encoder assignment exceeds a '1-tap' depth for performance controls."*
  3. (Keep) *"Static analysis confirms every audio-reactive pattern consumes the `silentScale` variable within its primary render loop, OR declares opt-out with rationale."* — pending REC-RECONCILE-3 / REC-PERF-4 implementation.
- **Outcomes (verbatim Pathmode-ready):** see proposed rewrite above.
- **Verification:** revised draft re-validated against `WiFiManager.cpp`, `HttpClient.cpp` (if exists), and the effect catalogue.
- **Evidence:** `firmware-v3/src/network/WiFiManager.h`; `firmware-v3/CLAUDE.md` (K1 AP-only constraint).

---

## Suggested execution order

If you want a single ordered to-do list to drive the next planning cycle:

1. **Spec text first** — REC-RECONCILE-1, 2, 3, 4 + REC-EXISTING-1. None of these need code; they're wording fixes that prevent downstream intents from inheriting broken premises.
2. **Latency instrumentation + BeatTracker gate** — REC-PERF-1 + REC-PERF-2. These are critical and unblock evidence-based North Star defence.
3. **ControlBus manifest + ESV11 sole-backend** — REC-PERF-3 + REC-PERF-5. Foundation for every audio-reactive intent that follows.
4. **Silence contract + render-budget regression + colour-correction skip list** — REC-PERF-4, REC-RENDER-1, REC-RENDER-2. Three high-leverage enforcement layers.
5. **Reliability core** — REC-OPS-1 (brownout) + REC-OPS-2 (panic log) + REC-OPS-3 (mic spec). Field-unit robustness.
6. **Inversion Bypass + PSRAM enforcement** — REC-RENDER-3, REC-RENDER-4. Either enforce or demote to guideline.
7. **Manufacturing + ops** — REC-OPS-4 (factory test), REC-OPS-5 (PSU + inrush), REC-OPS-6 (OTA validation window).
8. **Spec-clarity tail** — REC-RENDER-5 (rainbow audit), REC-GAP-1, REC-GAP-2, REC-GAP-3.

---

## Methodology + caveats

- **Subagent budget:** 4 read-only Explore agents, each capped at 30K tokens; sandboxed against `firmware-v3/` (with one cross-reference to `tab5-encoder/` for encoder routing).
- **Tools used:** Read, Grep, Glob, clangd (where available), QMD (where indexed). No source modification, no compilation, no test execution.
- **Items flagged `[UNVERIFIED]` in this report:** the verification subagent (next stage) is asked to spot-check these specifically.
  - Whether `PatternRegistry.cpp` applies a global `silentScale` multiplier post-render.
  - Whether `.github/workflows/` or `.pre-commit-config.yaml` already enforces any of these rules (lint precedent could change recommendations).
  - Whether any effect in the catalogue actually uses `[INVERTED]` (Subagent 4 grep returned zero, but the search may have been incomplete).
  - Actual measured audio-to-photon latency on K1 v2 hardware (no bench report consulted).
  - `FrequencyMap.h` band boundary values (referenced but not deeply read).
  - `tab5-encoder/src/ui/` parameter routing details.
  - `PatternRegistry.cpp` post-render silentScale application (file too large for verification subagent's budget; cross-references in `CONSTRAINTS.md` corroborate the gate exists, exact implementation not directly inspected).
- **Verification pass results:** Independent subagent spot-checked 15 critical claims against the codebase. Result: 13 PASS, 0 FAIL, 2 INCONCLUSIVE (both items already flagged `[UNVERIFIED]` above). No corrections required.
- **Out of scope (per Captain):** Network + Control Surface intents (WS/REST contract drift, parameter debounce parity, security model on local AP). Available as a follow-up audit if desired.
- **Captain decisions resolved 2026-04-26** (full rationale in the section near the top):
  1. North Star: parallel RMT channels to halve wire-time floor and keep `<8 ms` defensible. (See REC-RECONCILE-2.)
  2. BeatTracker: comb-tooth (already landed, commit `fab1802d`, 2026-03-20). Locked behind version constant + regression gate + change-requires-Captain-approval. (See REC-PERF-2.)
  3. PipelineCore: deprecate from production builds now; ESV11 sole production backend. (See REC-PERF-5, REC-RECONCILE-4.)
  4. Inversion Bypass: enforce via paired-CI gate. (See REC-RENDER-3.)
  5. silentScale: framework post-render multiplier with `SilenceBehaviour` descriptor opt-out; lint and runtime sampler are layered insurance, not primary. (See REC-PERF-4, REC-RECONCILE-3.)

---

*Generated 2026-04-26 from four sandboxed read-only audits of `firmware-v3/` against Pathmode `spectrasynq/k1` workspace state retrieved 2026-04-25.*
