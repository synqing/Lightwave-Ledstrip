---
abstract: "M1 measurement protocol — empirically validates LGP fringe-coherence on K1 V2 hardware before V1.0 ships. Gates F4 Cross-Strip Wave Interference launch and the marketing-copy claim 'physical interference'. Defines equipment (calibrated photometer + 8–12 viewer panel), test pattern set (5 phase offsets via FringeCoherenceGenerator), A/B forced-choice procedure, pass criteria (≥75% identification for F4 launch; ≥90% for marketing copy), failure mitigation (demote F4 to V1.1, reword copy to 'interference-pattern visualisation'), and unit-to-unit variance considerations. Read before commissioning the campaign or accepting/rejecting the moat claim."
---

# M1 — LGP Fringe-Coherence Empirical Validation

**Status:** SPEC ONLY. Pattern generator scaffolded at `firmware-v3/src/test_modes/m1_fringe_coherence.{h,cpp}` (build-flag-guarded, NOT included by default). Captain commissions the actual campaign separately by adding a dedicated platformio.ini env with `-D ENABLE_M1_FRINGE_COHERENCE` and dispatch wiring.

**Owner:** Captain (commission), measurement-campaign lead (execute).

**Last updated:** 2026-04-27.

---

## 1. Purpose

This protocol gates two launch decisions:

1. **F4 Cross-Strip Wave Interference inclusion in V1.0.** Per `Topology_Reconciliation.md` §3 [C-5] and §6 item 7, the F4 effect family ships in V1.0 only if K1 V2 LGP optics produce visible interference fringes at typical install conditions. Without this evidence, F4 demotes to V1.1 candidate.
2. **V1.0 marketing-copy claim "physical interference."** Per `PASS_4_ADVERSARIAL_STRESS_TEST.md` §1 [A-05] and §4.1, the T-01 ABSOLUTE moat claim is conditional on this measurement. Marketing copy may use "physical interference" only if the empirical pass threshold is met under the specified conditions.

The adversarial argument from PASS 4 §4.1 is direct: *"a high-opacity diffuser averages out fine fringe patterns into uniform brightness. The K1 LGP may be in this regime — turning T-01 from 'physical interference' into 'two-strip pattern on diffused panel that looks somewhat like interference.'"* This protocol resolves that question with hardware evidence, not theoretical argument.

**Failure of this measurement is not a failure of the product.** It is a failure to substantiate one specific marketing claim. The mitigation in §7 retains F4 as a V1.1 effect family with reworded copy.

---

## 2. Equipment

| Item | Specification | Sourcing |
|---|---|---|
| Calibrated photometer | Sekonic L-358 or equivalent (cosine-corrected, 1 cd/m² resolution, scotopic/photopic switchable) | Rentable ~£200/day (UK rental houses, e.g. ProAV) |
| Ambient lighting | Controlled-room target ~50 lux (typical living-room ambient). Secondary trial at 100 lux (well-lit room). | Photographer's softbox + dimmer; lux meter to verify |
| Viewing distance | ~2 m from K1 panel (typical desk/console install distance) | Tape measure; mark the floor |
| Test rig | K1 V2 unit on tripod or stable mount, panel facing viewer, optical axis level with viewer eye | Standard tripod with hot-shoe mount |
| Reference light source | Photometer-calibrated tungsten 100 W incandescent at 1 m (for photometer warm-up + sanity calibration each session) | Off-the-shelf |
| Recording | Photometer reading + viewer response logged per trial; spreadsheet template per §4 | Laptop or paper logsheet |
| Optional | Three K1 V2 units from different production lots (see §8 variance considerations) | Pull from production line |

**Calibration sequence at session start:** point photometer at the reference tungsten source, verify reading matches manufacturer-published luminance for that bulb at 1 m, and note ambient lux at the viewer position. Repeat every 90 minutes during long sessions to catch ambient drift.

---

## 3. Viewer Panel

- **Panel size:** 8–12 viewers. Below 8, statistical power drops too far. Above 12, scheduling friction outweighs marginal precision.
- **Composition:** mixed ages (recommend at least 2 viewers per decade across 20–60). Mixed gender. **Viewers must be naïve to test purpose** — do not tell them the test is about interference fringes or LGP optics. Tell them they are evaluating "two LED patterns" and which they find more visually structured.
- **Colour vision:** include 1–2 colour-blind viewers if available (red-green deuteranomaly is most common; protan and tritan welcome). Their data is recorded in a separate row but combined with the panel total — colour-blindness should not affect amplitude-modulated white fringe identification, and confirming this is itself useful evidence.
- **Briefing script (read verbatim to each viewer):**
  > *"You will see a series of paired patterns on the K1 panel. For each pair, the panel will show one pattern, then a second pattern. Tell me which pattern looks like it has more visible structure or texture, or whether the two look the same to you. There is no right answer — your honest first impression is what we want."*
- **Discard a viewer's data only if** they report being unable to focus, the room lighting changed mid-session, or they ask leading questions that suggest they have inferred the test purpose. Note any discarded data in the logsheet.

---

## 4. Procedure — A/B Forced-Choice

The pattern generator (`FringeCoherenceGenerator` in `m1_fringe_coherence.h`) supports five static phase offsets:

| Pattern index | Δφ = φ_B − φ_A | Expected appearance |
|---|---|---|
| 0 | 0 | Constructive — both strips identical, uniform reinforcement |
| 1 | π/4 | Slight diagonal phase (subtle structure) |
| 2 | π/2 | Quadrature — diagonal phase, mid-range structure |
| 3 | 3π/4 | Near-destructive — strong asymmetry |
| 4 | π | Destructive — alternating dark/light fringes (strongest theoretical signal) |

Plus a **uniform-brightness control** (`kPatternUniformControl`) — both strips constant at the base brightness, no modulation. This is the comparison target.

### 4.1 Trial structure

Each trial presents two stimuli sequentially:
- **A:** the fringe pattern under test (one of indices 0–4)
- **B:** the uniform-brightness control

Stimulus order is **counterbalanced** — half the trials show fringe-then-uniform, half show uniform-then-fringe. The viewer reports one of three responses:

1. *"Left has structure"* (i.e. the first stimulus shown)
2. *"Right has structure"*
3. *"Indistinguishable"*

A response counts as a **correct identification** if the viewer's "structure" choice matches the slot in which the fringe pattern was presented.

### 4.2 Trial count

- **12 trials per phase offset** (6 fringe-first, 6 uniform-first), counterbalanced.
- Total per viewer: 12 × 5 = **60 trials**, plus 12 calibration trials at Δφ = 0 (which should be near-chance because Δφ = 0 is the constructive uniform-reinforcement case — both strips identical, so it visually approaches uniform).
- Estimated session time: **~25 minutes per viewer** at 25 s per trial including dwell + response.

### 4.3 Dwell time

Each pattern is held for **≥10 s** before the viewer is asked to respond. This is enforced by the pattern generator (auto-cycle dwell is 15 s by default; manual dispatch via REST/serial holds indefinitely until `setPattern` is called again). Dwell time longer than 15 s is fine; shorter than 10 s is not.

### 4.4 Conditions matrix

Run the full 60-trial protocol per viewer at:

- **50 lux ambient** (primary condition — typical living-room install)
- **100 lux ambient** (secondary condition — well-lit room; only if 50-lux pass criterion is met)

Viewing distance is fixed at **2 m** for both conditions. No distance variation in v1 of this protocol; that is a follow-up if the primary condition passes.

---

## 5. Pass Criterion — F4 Launch Inclusion

F4 Cross-Strip Wave Interference is included in V1.0 if and only if:

> **≥75% identification rate (fringe distinguishable from uniform) under at least 4 of the 5 phase offsets, at 50 lux ambient and 2 m viewing distance.**

"Identification rate" = correct responses / (correct + incorrect). "Indistinguishable" responses are excluded from the denominator (they count toward neither pass nor fail), but if more than 30% of any phase offset's responses are "indistinguishable" the offset fails regardless of the correct/incorrect ratio. This guards against a high apparent identification rate driven by a tiny sample of decisive viewers.

If the 75% threshold is met on 3 of 5 offsets, F4 enters a **conditional pass**: the launch demo selects only the passing offsets for hero-effect rendering, and the failing offsets are excluded from the V1.0 catalogue. If <3 offsets pass, F4 fails outright.

---

## 6. Marketing-Copy Threshold — "Physical Interference"

The phrase "physical interference" (or any equivalent claim that K1 produces real optical interference, as distinct from a mathematical pattern rendered on a diffused panel) is permitted in V1.0 marketing copy only if:

> **≥90% identification rate under 50–100 lux ambient × at least 3 of 5 phase offsets, both ambient conditions met simultaneously.**

This is a stricter bar than the launch threshold because the marketing claim must hold across realistic ambient lighting variation, not just the single primary condition. If the threshold is met at 50 lux but not at 100 lux, marketing copy may not use "physical interference"; it may use the mitigation phrasing in §7.

---

## 7. Failure Mitigation

Three failure tiers, with decreasing severity of mitigation:

### 7.1 Marketing-copy threshold fails (§6 not met) but launch threshold passes (§5 met)

- **F4 ships in V1.0** as planned.
- **Marketing copy reverts** from "physical interference" to **"interference-pattern visualisation"** or **"dual-strip wave-pattern composition"**. These claims are technically accurate (the maths-and-geometry produces an interference-shaped brightness pattern; whether the optics produce *physical* interference at the panel surface is a separate question that the data did not resolve in our favour).
- Update `Topology_Reconciliation.md` §3 [C-5] verdict from "ABSOLUTE pending measurement" to "STRONG, perceptual-only".

### 7.2 Launch threshold fails (§5 not met — fewer than 3 offsets at 75%)

- **F4 demotes from V1.0 hero to V1.1 candidate.**
- V1.0 marketing thesis ("Liquid Light. Music made visible.") leans harder on F1 (centre origin), F2 (centre-phase pendulum), F3 (Liquid Stillness), F5 (palette doctrine), and F6 (First-Light Ignition) — none of which depend on this measurement.
- The Phase-3 substrate (PS-05 Reflective Twin contract, GEO-13 InterStripPhaseDelay) still ships, because it has independent value for V1.1 effects beyond F4 itself.
- Marketing copy uses **"dual-strip composition"** or **"cross-strip layering"** — no interference language at all.
- File a follow-up measurement with a low-opacity diffuser sample (Captain decision: is it cheaper to revisit LGP optics or to cut the interference moat claim?).

### 7.3 Systematic disagreement across phase offsets (e.g. some pass at 90%, others fail below 50%)

- Likely a **viewing-angle or polarisation artefact**. Flag for engineering follow-up before any V1.0 commitment.
- Re-run the failing offsets at three viewing distances (1 m, 2 m, 3 m) and three viewing angles (head-on, 30°, 60°) to characterise the dependence. This is a separate sub-protocol; do not block V1.0 on it indefinitely — set a 2-week resolution deadline, then default to §7.2 if unresolved.

---

## 8. Variance Considerations

LGP diffuser opacity has a known production tolerance per K1 V2 manufacturing run. This protocol must account for it.

### 8.1 Suggested unit count

If three or more K1 V2 units are available, run the **primary condition (50 lux, 2 m)** on **3 separate units** drawn from different production lots. Use the same viewer panel where logistically possible.

If unit-to-unit variance exceeds **±10 percentage points** on identification rate at any phase offset, this is a quality-control finding, not a launch finding. Flag it to manufacturing and re-run with diffuser opacity binned by production-lot QC measurement.

### 8.2 Single-unit variance

If only one K1 V2 unit is available:

- Note this loudly in the report. Single-unit results bound the *best case* but not the *typical case*.
- Treat the §5 / §6 thresholds as conservative — if the single unit barely passes at 75%, the population mean could be below 75%. If the single unit passes comfortably at 85%+, the population mean is more credibly above 75%.
- Do not claim "physical interference" in marketing copy on single-unit data alone. Single-unit may gate F4 launch inclusion (§5) but cannot gate the marketing-copy threshold (§6).

### 8.3 Diffuser-opacity correlation (optional follow-up)

If unit variance is detected and engineering wants to investigate, measure each tested unit's diffuser opacity with a transmissometer (off-the-shelf, ~£150) at three points across the panel. Correlate opacity reading against viewer identification rate. If correlation is strong (Pearson |r| > 0.7), this becomes a design input for V2 LGP specification.

---

## 9. Reporting

The campaign produces **one report** stored at `firmware-v3/docs/measurement_protocols/m1_lgp_fringe_results_<YYYY-MM-DD>.md` (results doc, not this protocol — this protocol is the procedure spec, not the data store).

Required sections in the results doc:

1. Date, location, ambient conditions verified
2. Equipment used, photometer calibration confirmation
3. Viewer panel composition (count, age range, colour-vision notes)
4. Per-phase-offset identification rate at 50 lux (and 100 lux if measured)
5. Per-unit identification rate if multiple units tested
6. Photometer luminance readings per phase offset (max/min across the panel)
7. Pass/fail verdict against §5 and §6 thresholds
8. Captain's marketing-copy ratification (or reversion to §7 mitigation language)
9. Raw trial-by-trial data appendix (CSV)

The verdict in §7 of the results doc directly amends `Topology_Reconciliation.md` §3 [C-5] and `PASS_4_ADVERSARIAL_STRESS_TEST.md` §1 [A-05]. Update those sections by appending a "Resolution — YYYY-MM-DD" subsection rather than editing the original verdicts; the protocol's audit trail depends on the original adversarial argument remaining visible.

---

## 10. Build & Dispatch Wiring (Captain's Call)

The pattern generator is scaffolded but NOT wired to a build env. To commission a campaign:

1. Add a new platformio.ini env (e.g. `[env:esp32dev_audio_esv11_k1v2_32khz_m1]`) extending the canonical K1 V2 env, with:
   - `-D ENABLE_M1_FRINGE_COHERENCE=1` in `build_flags`
   - `+<test_modes/m1_fringe_coherence.cpp>` retained in `build_src_filter` (default-on with `+<*>`)
   - A small dispatch shim — either a serial command (e.g. `m1set <0..4|254>`) or a REST endpoint (e.g. `POST /api/v1/test_modes/m1/pattern { "index": 2 }`) — calling `FringeCoherenceGenerator::setPattern()`.
2. Override the default render path while the M1 mode is active — bypass effect dispatch and call `FringeCoherenceGenerator::render()` directly into the dual-strip CRGB buffer.
3. Verify the centre-origin invariant is preserved by inspection: both strips' LUTs are sampled by identical index, so the visible fringe is symmetric about LED 79/80 by construction.

None of this is in the current commit. Captain decides the dispatch surface (serial vs REST vs auto-cycle) when commissioning.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-27 | agent:SSA-3 | Created. Defines M1 LGP fringe-coherence measurement protocol gating F4 launch and "physical interference" marketing copy. Pattern generator at firmware-v3/src/test_modes/m1_fringe_coherence.{h,cpp} (build-flag-guarded, not auto-included). |
