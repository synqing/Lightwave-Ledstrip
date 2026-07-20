---
abstract: "Two-tier plan to port LightwaveOS EdgeMixer (rigorous 3x3 hue-matrix colour differentiation, 9 modes) into the K1 production firmware (SensoryBridge lineage, sb_edgemixer_lite, CRGB16/SQ15x16). Product-owner decision + sequenced execution: golden-master oracle first, then Tier 1 signal-independent colour core (6 modes + spread) validated against the oracle and hardware A/B, then a small audio seam, then Tier 2 audio-reactive gated behind a compile flag. STM modes are a hard blocker until K1 grows a spectral-temporal-modulation producer (Work Block WB-3). Sourced from the 23-agent adversarial audit of 2026-07-07. Read before touching sb_edgemixer_lite or planning EdgeMixer parity."
---

# EdgeMixer → K1 (`sb_edgemixer_lite`) — Two-Tier Port Plan

**Owner (this plan):** agent:opus-4-8 acting as product owner / PM, per Captain delegation 2026-07-07 ("you are best positioned to make the onward decisions").
**Evidence base:** 23-agent adversarial audit `edgemixer-audit-v2` (2026-07-07) — 15 confirmed/partial findings, 1 refuted, grounded to file:line. This plan does not re-derive that evidence; it decides and sequences from it.
**Source of truth caveat:** all file:line anchors below are from a snapshot; the K1 repo (`/Users/spectrasynq/SpectraSynq_K1_Firmware/SPECTRASYNQ_K1_FIRMWARE`) moves independently (BLE-OTA work is live on `feat/im73d-ota-bench`). **Re-verify against current HEAD before writing code.**

---

## 1. The decision (what I would do, and why)

**Port it as two independent tiers, oracle-first, and port it *better* than the original — do not reproduce its weaknesses.**

The audit reframed the problem. The EdgeMixer *code* is sound (thread-safe, correct maths, 20x under budget — my two scariest hypotheses, a data race and a hidden divide cost, were both refuted with proof). **All the real risk is cross-lineage impedance**, and that impedance splits cleanly along a Cynefin fault line:

- **The 6 colour-harmony modes are Complicated** — knowable, expert-analysable work. A faithful port is a *fixed-point rewrite* (source `CRGB` uint8 0..255 → target `CRGB16` SQ15x16 0..1), because K1's "same 6 modes" are **name-parity only**: ad-hoc per-channel swaps with a hard-coded green factor (`sb_edgemixer_lite.cpp:64-114`), not the source's rigorous hue-rotation-around-grey-axis + BT.601 matrix (`EdgeMixer.h:520-611`). "We already have 6 modes" is misleading — they are different effects.
- **The two STM modes are Complex/blocked** — they consume `stmSpectral[42]`/`stmTemporalEnergy` (`ControlBus.h:140-145`) that **do not exist anywhere in K1's audio pipeline** (whole-repo grep = 0 hits; `sb_semantic_state.h:47-91` produces only tempo/onset/kick-snare-hihat/chord). A naive zero-fill drives the whole secondary strip toward black *and* breaches K1's own `sb_semantic_state` doctrine ("named, reasoned, absent — do not fabricate 0", `sb_semantic_state.h:29-36`).

Conflating these two is the trap. **TRIZ separation-in-structure** resolves the contradiction ("richer/audio-reactive differentiation" vs "target lacks the signals"): decompose EdgeMixer into a *signal-independent colour core* (ships now) and an *audio-reactive layer* (gated until the signals exist). A bonus TRIZ convert-harm-to-benefit: **do not port the source's centre-gradient LUT** — it has a confirmed off-centre defect (zero at index 76, not 79.5) — **adopt K1's already-correct analytic mask** (`sb_edgemixer_lite.cpp:26-40`) and the defect disappears for free.

**Why oracle-first:** the single largest Tier-1 risk is silent fixed-point corruption (treating a 0..1 SQ15x16 channel as 0..255 is ~256x wrong, and it *compiles*). The cheapest way to kill that risk early — and the repo's own autonomous-build doctrine ("the verification harness IS the product") — is a **golden-master oracle**: freeze the source colour maths as golden input→output vectors, then require the K1 SQ15x16 port to reproduce them within ±1 LSB. The oracle is not a nice-to-have; it is the acceptance gate for the port.

**Why port it *better*, not bug-for-bug:** the audit confirmed the "luminance-preserving" claim is false for every hue-rotation mode (grey-axis rotation preserves the naive R+G+B sum, not perceptual luma — TRIADIC red→green roughly *doubles* perceived brightness). The Execution Standard is explicit: faithful reproduction of a weakness is failure. K1 has ~massive frame headroom, so Tier 1 will expose **both** a faithful (sum-preserving) and a corrected (luma-preserving, e.g. rotation in YCbCr chroma) path as a runtime toggle, and the Captain picks by eye on hardware (the "TEST. THEM. ALL." rule — visual decisions are hardware A/B, never chosen blind).

**Why this order maximises value on the K1→FE critical path:** Tier 1 ships a visible, deterministic upgrade (rigorous, spread-adjustable colour depth) with zero audio dependency and a bounded, testable risk. It does not wait on the STM producer, which is a separate audio-DSP project. We deliver customer-visible value first and defer the Complex/blocked work behind a flag with the blocker documented — instead of stalling the whole feature on the one part that is genuinely blocked.

---

## 2. Sequenced execution

| Step | Deliverable | Repo / risk | Gate |
|------|-------------|-------------|------|
| **0. Oracle** | Native golden-master: generate golden `(mode, spread, in_rgb)→out_rgb` vectors from source `EdgeMixer` colour path; a K1-side `test_edgemixer_parity` asserts the SQ15x16 port matches within ±1 LSB. | LightwaveOS-native + K1-native. **No hardware risk.** | Golden vectors committed; harness proven fault-evident (perturb a matrix coeff → test fails). |
| **1. Tier-1 colour core** | Replace `sb_edge_mix` with the 3x3 hue-matrix stored as `SQ15x16[9]`, add `spread` (0-60), 6 modes; adopt K1's analytic centre mask; expose faithful vs luma-preserving rotation toggle. | K1 `director/`. Firmware behaviour → **hardware-test-before-commit**. | Parity vs oracle (faithful path ±1 LSB) + Captain hardware A/B sign-off. |
| **2. Audio seam** | Extend `sb_edgemixer_lite_apply()` to also carry `AudioSemanticState`/snapshot + the primary strip (`leds_16`), additively, at call site `.ino:1343`. Prereq for any reactive/dual-strip mode. | K1 `director/` + call site. Small, additive. | Builds flag-off unchanged; no behaviour change until a reactive mode uses it. |
| **3. Tier-2a RMS_GATE** | Re-source the temporal gate from `vu_level` (`sb_audio_snapshot.h:60`); **recalibrate** the 850x / 0.15-EMA constants (they are LightwaveOS rms+framerate-specific — recalibrate, do not copy). | K1 audio + director. Behaviour → hardware A/B. | Gate tracks loudness sanely on hardware; Captain sign-off. |
| **4. Tier-2b STM** | **BLOCKED** — needs a spectral-temporal-modulation producer in K1 audio (per-bin modulation history + temporal-modulation energy). Ship gated `#if K1_STM_AVAILABLE 0`. | Work Block **WB-3**. Separate DSP project. | Not started until WB-3 lands + validated (colour modes 0-4 were the only validated set even on source; STM never was). |

Steps 0→2 are the near-term push. Steps 3→4 are deferred and flagged.

### Step 0 — DELIVERED (2026-07-08)

The golden-master oracle is built, green, and independently verified (orchestrator re-ran it, did not trust the builder's claim).
- **Artefact:** `firmware-v3/test/test_edgemixer_parity/` (`test_edgemixer_parity.cpp` + `edgemixer_golden.csv`) + env `[env:native_test_edgemixer_parity]` in `platformio.ini`. Run: `pio test -d firmware-v3 -e native_test_edgemixer_parity`.
- **Result:** 4/4 Unity cases pass (0.375 s, independently re-run). 810 golden rows (6 colour modes × 5 spreads × 27 probes) generated by driving the **unmodified** `EdgeMixer::process()` at strength 255 / uniform / static (the scale8-independent direct-assign path).
- **Parity:** the Q15.16 fixed-point reference port (the code that transplants to K1's SQ15x16) reproduces every golden within **±1 LSB** (worst case exactly 1 LSB on ANALOGOUS/orange — Q8.8-vs-Q15.16 quantisation residue, not corruption).
- **Fault-evidence proven:** a +4-Q8.8-LSB perturbation of one matrix coefficient breaks parity (RED); restored is bit-exact (GREEN) — the oracle demonstrably fails on injected corruption.
- **Independent spot-checks (orchestrator, physical):** ANALOGOUS@spread0 = exact identity (θ=0); TRIADIC@spread0 maps red→green and leaves white unchanged on the grey axis (confirms the rotation axis is truly (1,1,1)); near-black passes through. `src/` confirmed untouched.
- **Scope note:** gates the SUM_PRESERVING (faithful) path only; the Tier-1 LUMA_PRESERVING toggle is a deliberate divergence needing its own luma-band assertion, not ±1 LSB.
- **Not committed** — working-tree only, pending Captain review. The clangd diagnostics on the test file are a false alarm (ESP32 `compile_commands.json` flags applied to a native-platform TU); the passing host run is the territory.

---

## 3. Tier-1 technical scope (grounded)

Target contract today (verify at HEAD):
- `void sb_edgemixer_lite_apply(CRGB16* secondary, uint16_t count, const SBEdgeMixerConfig& config)` — `sb_edgemixer_lite.h:24`; call site `.ino:1343` passes only the secondary buffer, **no audio, no primary**.
- `struct SBEdgeMixerConfig { bool enabled; SBEdgeMixerMode mode; float strength; }` — `sb_edgemixer_lite.h:16-20` (no spread / spatial / temporal).
- Existing colour maths: ad-hoc swaps, hard-coded 0.35 green factor — `sb_edgemixer_lite.cpp:64-114`.
- Analytic centre mask (KEEP THIS): `sb_edgemixer_lite.cpp:26-40` (0 at 79.5, 1 at edges).
- `SQ15x16` normalised 0..1 — `constants.h:248-252`.

Source to translate:
- `recomputeMatrix()` `EdgeMixer.h:520-611` (Rodrigues rotation about (1,1,1) + BT.601 desaturation, composed `M = D·H`) — port the *float* config-time computation as-is, store result as `SQ15x16[9]` instead of `int16 Q8.8[9]`.
- `computeTransform()` `EdgeMixer.h:490-506` — becomes an `SQ15x16` 3x3 multiply + clamp (no `>>8`, no `/255`).
- Blend by strength — `SQ15x16` lerp, not the uint8 `/255` path.

Tier-1 edits:
1. Extend `SBEdgeMixerConfig` with `uint8_t spreadDegrees` (0-60) and a `rotationSpace` enum {SUM_PRESERVING, LUMA_PRESERVING}.
2. Add `SQ15x16 m_matrix[9]` + a `recompute()` mirroring the source, invoked on config change (guard under the existing `portMUX` so Core-1 render never reads a half-written matrix — see finding `concurrency-singleton-to-loop`).
3. Replace the `sb_edge_mix` body with the matrix multiply + analytic-mask weighting + strength lerp.
4. Leave STM/RMS untouched (Tier 2).

**Do not port:** the `kCentreGradient` LUT (off-centre), `m_spreadHue` (dead field), the singleton/actor plumbing (collapses to K1's free-function + portMUX model).

---

## 4. Golden-master oracle spec (Step 0)

- **Golden generation (LightwaveOS-native):** for each colour mode {ANALOGOUS, COMPLEMENTARY, SPLIT_COMPLEMENTARY, SATURATION_VEIL, TRIADIC, TETRADIC} × spread {0, 15, 30, 45, 60} × a fixed 24-colour probe palette (primaries, secondaries, greys, near-black, skin, saturated audio-reactive hues), record `computeTransform(in)` → `out`. Emit `edgemixer_golden.csv`.
- **Parity test (K1-native, `test/test_edgemixer_parity`):** load the CSV, run the K1 `SQ15x16` path, convert output back to 8-bit, assert `|Δ| ≤ 1` per channel on the faithful (SUM_PRESERVING) path. The LUMA_PRESERVING path is a deliberate divergence — assert it is *within a documented luma band*, not bit-parity.
- **Fault-evidence check (mandatory before trusting green):** perturb one matrix coefficient by 1 LSB and confirm the test goes red. A harness that cannot fail on an injected fault is worse than none (repo law).

This oracle also retro-covers the confirmed **"no automated test"** gap on the source side (finding `no-edgemixer-test`) — the golden generator *is* the source's first unit test.

---

## 5. Risk register (from audit)

| Risk | Class | Mitigation |
|------|-------|-----------|
| STM signals absent on K1 | 🔴 blocker | WB-3; gate modes 7-8 off; never zero-fill |
| SQ15x16 vs uint8 silent corruption | 🟠 high | Oracle ±1 LSB gate is the whole point of Step 0 |
| `apply()` can't express audio/dual-strip | 🟠 high | Step 2 additive seam before any reactive mode |
| RMS_GATE constants mis-scale on `vu_level` | 🟡 med | Recalibrate on hardware, don't copy |
| Luminance jump in hue-rotation modes | 🟡 med (craft) | Ship faithful+luma toggle; Captain A/B |
| K1 has no fixed frame ceiling to check headroom against | 🟡 med | Measure ported cost via K1's existing `ENABLE_VP_PERF_AUDIT` (`.ino:1355-1358`), don't assume |

---

## 6. Out of scope here — logged separately

Source-side (LightwaveOS) defects the audit confirmed, **not** part of the K1 port, logged to `BACKLOG.md` § Technical Debt:
- `kCentreGradient` off-centre (index 76 vs 79.5) — centre-origin breach in `EdgeMixer.h:323`.
- Serial-JSON rejects mode 8 (`SerialJsonGateway.cpp:771`) — #47224 regression on a sibling surface.
- "~22µs" unvalidated perf claim in shipping header (`EdgeMixer.h:22`).
- "Luminance-preserving" doc claim false (`EdgeMixer.h:20/480`).
- `saveToNVS` synchronous flash on the render core; dead `m_spreadHue` / `m_matrixDirty` branch.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-07-08 | agent:opus-4-8 | Created. Two-tier K1 port plan from the 23-agent EdgeMixer adversarial audit; product-owner decision + sequenced execution + golden-master oracle spec + risk register. |
| 2026-07-08 | agent:opus-4-8 | Step 0 (golden-master oracle) DELIVERED + independently verified (4/4 native pass, ±1 LSB, fault-evident); added §2 "Step 0 — DELIVERED" status subsection. |
