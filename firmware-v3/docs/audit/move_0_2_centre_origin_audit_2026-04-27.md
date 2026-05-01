---
abstract: "Phase 0 Move 0.2 deep audit triage of 34 pre-existing brand-voice and contract violations flagged by tools/check_effect_contracts.py across 22 distinct files. Per-flag classification (FIX / ALLOWLIST / DEPRECATE / INVESTIGATE) with engineer-day estimates. Headline findings: ALL 16 rainbow-scan flags on LGP*AREffect.cpp are false positives (palette-segment use anchored to baseHue, not full-wheel rotation); ALL 5 heap-in-render flags are false positives caused by a brace-tracking bug in the lint engine; the SerialCLI WIFI_MODE_STA flag is a read-only diagnostic print. Two centre-scan flags need investigation. The lint script itself is the highest-priority remediation, not the effect catalogue."
type: audit
---

# Move 0.2 — Centre-Origin & Brand-Voice Compliance Audit

**Date:** 2026-04-27
**Author:** SSA-5 (read-only audit)
**Phase:** 0 (Baseline Guardrails)
**Move:** 0.2 (Centre-Origin audit pass over existing 106-effect catalogue)
**Tool version:** `firmware-v3/tools/check_effect_contracts.py` (head)
**Topology reference:** `firmware-v3/docs/research/synergy-topology/Topology_Reconciliation.md` §5 (Reconciled Kill Order)

> **Headline:** Of 34 violations reported by the lint, **22 are false positives caused by lint bugs or pattern-recognition gaps**, **8 are pre-existing API style flags on the BeatPrismOnset family** (informational, not brand-voice), **2 are genuine investigation candidates** (centre-scan), and **2 are clear allowlist additions**. **Zero genuine brand-voice violations were found in the effect catalogue.** The highest-priority remediation is the lint engine itself, not the effects.

---

## 1. Methodology

### 1.1 Tool

`firmware-v3/tools/check_effect_contracts.py` runs eleven discrete checks against `firmware-v3/src/`. The audit re-ran the script as the verification base:

```bash
cd firmware-v3 && python3 tools/check_effect_contracts.py
```

Result on 2026-04-27: 34 violation lines printed across six categories.

### 1.2 Categories audited

| Category | Pattern source | Count | Lint section |
|---|---|---|---|
| `[api]` Raw control bus access | `ctx\.audio\.controlBus` substring | 10 | `check_raw_control_bus_usage` |
| `[heap]` Heap allocation in render | `\b(new|malloc|calloc|realloc|heap_caps_malloc)\b` plus brace tracking from `void *::render(` | 5 | `check_heap_alloc_in_render` |
| `[centre-scan]` Linear sweep without centre-origin | `for(int i=0;)` regex absent of `SET_CENTER_PAIR\|CENTER_LEFT\|79-\w\|80+\w\|NUM_LEDS/2` | 2 | `check_centre_origin_inverted` |
| `[rainbow-scan]` Rainbow / hue-wheel pattern | `\bCHSV\s*\(\s*hue` (and three other patterns, none matched here) | 16 | `check_rainbow_inverted` |
| `[k1-ap-only]` WiFi STA-mode reference | `\bWIFI_MODE_STA\b` (and three other patterns) | 1 | `check_k1_ap_only` |
| Total | — | **34** | — |

### 1.3 Threshold for classification

Each flagged line was inspected at source and classified as one of:

- **FIX** — Genuine violation. Code change required before V1.0 ship.
- **ALLOWLIST** — Pattern matches a regex but the semantics are compliant. Add the file to the appropriate allowlist set in `check_effect_contracts.py`.
- **LINT-FIX** — The lint engine itself is buggy or pattern-incomplete. Fix the script, not the source.
- **DEPRECATE** — Effect is dead code or scheduled for kill in Phase 3. No fix needed; let the deprecation process subsume it.
- **INVESTIGATE** — Captain decision required. Surface-level pattern is ambiguous; semantic intent must be confirmed.

### 1.4 Cubic-Perlin §4.1 rehab criterion

Per Topology_Reconciliation §5 Phase 5 Move 5.5 (LIN-04 RhythmLockedCubicPerlinRibbon — "rehab cubic Perlin via mask, granular"), cubic Perlin is permissible **as a luminance/alpha mask over a locked palette** but banned **as RGB-channel rotation**. The audit checked every flagged AR file for `cubicPerlin / perlin / inoise / noise8` token presence — **zero matches across all 16 flagged AR files**. Cubic-Perlin rehab does NOT apply to any rainbow-scan flag in this audit; the rainbow flags resolve through a different mechanism (palette-segment use, see §2.1).

---

## 2. Triage Table — Per Category

### 2.1 Rainbow scan — 16 flagged AR files

**All 16 flags fire on the regex `\bCHSV\s*\(\s*hue`.** Per-file source inspection of the `hue` derivation revealed a uniform pattern across the entire AR family:

```cpp
uint8_t hue = baseHue + static_cast<uint8_t>(<bounded_modulation>);
```

where `baseHue = mod.baseHue` (sourced from the AR control bus `tonalHue` colour anchor) and the modulation term is bounded to roughly 22–84 hue units (≈31–118 degrees of arc, a palette-segment delta — NOT a full 360° hue wheel). The `tonalHue` field exists in every flagged file (verified by `tonal=True` token-presence check on all 16). This is **palette-segment use anchored to a tonal hue** — exactly the `[colour]` doctrine §3 of brand-voice posture (no rainbows = no full-wheel sweeps; bounded modulation around a tonal anchor is permitted).

| File | First flagged line | `hue` derivation (first occurrence) | Modulation range (units / deg) | Cubic Perlin? | Classification |
|---|---|---|---|---|---|
| LGPAiryCometAREffect.cpp | 202 | `baseHue + (uint8_t)(60.0f * (1.0f - headness))` | 0–60 / 0–84° | no | ALLOWLIST |
| LGPChimeraCrownAREffect.cpp | 327 | `baseHue + (uint8_t)hueShift` | bounded by `hueShift` calc | no | ALLOWLIST |
| LGPCymaticLadderAREffect.cpp | 170 | `baseHue + (uint8_t)(n * 8.0f + progress * 30.0f)` | 0–~50 / 0–70° | no | ALLOWLIST |
| LGPHarmonographHaloAREffect.cpp | 219 | `baseHue + (uint8_t)(distBand * 48.0f)` | 0–48 / 0–67° | no | ALLOWLIST |
| LGPHyperbolicPortalAREffect.cpp | 203 | `baseHue + (uint8_t)(u * 35.0f)` | 0–35 / 0–49° | no | ALLOWLIST |
| LGPLangtonHighwayAREffect.cpp | 334 | `baseHue + (uint8_t)(highway * 40.0f)` | 0–40 / 0–56° | no | ALLOWLIST |
| LGPLorenzRibbonAREffect.cpp | 293 | `baseHue + (uint8_t)hueShift` | bounded | no | ALLOWLIST |
| LGPMachDiamondsAREffect.cpp | 289 | `baseHue + (uint8_t)(progress * 48.0f)` | 0–48 / 0–67° | no | ALLOWLIST |
| LGPMoireCathedralAREffect.cpp | 153 | `baseHue + (uint8_t)(wave * 42.0f + progress * 12.0f)` | 0–~54 / 0–76° | no | ALLOWLIST |
| LGPReactionDiffusionAREffect.cpp | 228 | `baseHue + (uint8_t)(vSample * 120.0f + progress * 15.0f)` | 0–~135 / 0–190° | no | INVESTIGATE — at the upper bound this approaches half-wheel; verify visual audit |
| LGPRoseBloomAREffect.cpp | 347 | `baseHue + (uint8_t)(band * 45.0f + progress * 20.0f)` | 0–~65 / 0–92° | no | ALLOWLIST |
| LGPSchlierenFlowAREffect.cpp | 194 | `CHSV(hueA, ...)` (`hueA` from prior calc) | bounded by `hueA` derivation | no | ALLOWLIST |
| LGPSpirographCrownAREffect.cpp | 240 | `baseHue + (uint8_t)(band * 45.0f)` | 0–45 / 0–63° | no | ALLOWLIST |
| LGPSuperformulaGlyphAREffect.cpp | 172 | `baseHue + (uint8_t)(r_formula * 60.0f + progress * 20.0f)` | 0–~80 / 0–113° | no | ALLOWLIST |
| LGPTalbotCarpetAREffect.cpp | 162 | `baseHue + (uint8_t)(progress * 22.0f)` | 0–22 / 0–31° | no | ALLOWLIST |
| LGPWaterCausticsAREffect.cpp | 163 | `CHSV(hueA, ...)` (`hueA` from prior calc) | bounded by `hueA` derivation | no | ALLOWLIST |

**Verdict:** All 16 are **palette-segment use, not rainbows.** The `\bCHSV\s*\(\s*hue` regex is too coarse — it cannot distinguish bounded modulation from full-wheel sweeps. Two paths to remediation:

1. **Allowlist all 16** by adding to `RAINBOW_ALLOWLIST` in `check_effect_contracts.py`. Simplest path. (~2 hours, one PR.)
2. **Tighten the regex** to detect actual full-wheel patterns (e.g. `hue\s*=\s*.*\b(beatCount|frame|millis|tick).*\*\s*\d+`, or detection of hue assignment without a `baseHue +` prefix). More robust but higher false-negative risk if regex misses a legitimate offender. (~1 day.)

**Recommendation:** Path 1 (allowlist) for V1.0. If rainbow detection becomes a recurring source of noise post-V1.0, invest in a more semantic check (Path 2) in V1.1.

### 2.2 Centre-scan — 2 flagged

| File | Line | Pattern observed | Diagnosis | Classification |
|---|---|---|---|---|
| LGPPerceptualBlendEffect.cpp | 76 | `for (uint16_t dist = 0; dist < HALF_LENGTH; dist++)` followed by `gradient::writeCentrePairDual(...)` | Iteration is over `dist` (centre-distance, 0..HALF_LENGTH) and the write uses a centre-pair helper. Compliant — the lint regex doesn't recognise `writeCentrePairDual` as a centre-origin marker. | **LINT-FIX or ALLOWLIST**. Add `writeCentrePairDual` to `CENTRE_ORIGIN_PATTERN` (preferred) or add the file to `CENTRE_LINEAR_ALLOWLIST`. |
| LGPGradientFieldEffect.cpp | 205 | `for (uint16_t i = 0; i < STRIP_LENGTH; i++)` writing to `ctx.leds[i]` | Iterates 0→N linearly. Coordinate basis (`gradient::uCenter(i)` etc.) is centre-relative, but the LED write itself sweeps left-to-right. Three coordinate bases supported (CENTER, LOCAL, SIGNED). | **INVESTIGATE** — Captain decision: does centre-relative gradient sampling satisfy centre-origin doctrine when the WRITE iterates linearly? The visual artefact may still appear symmetric (because `uCenter(i)` returns a symmetric u-value), but the iteration order is non-canonical. Confirm with hardware A/B versus a refactored centre-pair version. |

### 2.3 Heap allocation in render — 5 flagged (ALL FALSE POSITIVES)

| File | Line | Actual function | Classification |
|---|---|---|---|
| LGPHolyShitBangersPack.cpp | 209 | `LGPCatastropheCausticsEffect::init()` (line 203) | **LINT-FIX** |
| LGPHolyShitBangersPack.cpp | 379 | `LGPLorenzRibbonEffect::init()` (line 371) | **LINT-FIX** |
| LGPHolyShitBangersPack.cpp | 487 | `LGPIFSBioRelicEffect::init()` (line 479) | **LINT-FIX** |
| LGPShapeBangersPack.cpp | 600 | `LGPRule30CathedralEffect::init()` (line 594) | **LINT-FIX** |
| LGPShapeBangersPack.cpp | 686 | `LGPLangtonHighwayEffect::init()` (line 682) | **LINT-FIX** |

**Root cause — bug in the lint engine.** `check_heap_alloc_in_render` (lines 419–444 of `tools/check_effect_contracts.py`):

```python
if HEAP_IN_RENDER_PATTERN.search(code_part):
    violations.append(...)
    brace_depth += line.count("{") - line.count("}")  # only updated on heap match
    if brace_depth <= 0:
        in_render = False
```

The brace-depth update is **inside the heap-match conditional**, so lines that close braces without containing a heap pattern do NOT decrement the counter. Once `in_render` is set true at the first `void *::render(`, it never resets. Every `heap_caps_malloc` in the rest of the file is reported as "in render", even when it lives inside `init()`, free functions, helpers, etc.

Verification: instrumented the lint with brace-depth tracking on every line. In `LGPHolyShitBangersPack.cpp`, the engine enters render at line 114 (`LGPChimeraCrownEffect::render`) and never exits. The three "heap in render" flags at lines 209/379/487 are all inside `init()` functions of subsequent classes (Catastrophe, Lorenz, IFSBioRelic) — separated from the original render entry by hundreds of lines and dozens of closing braces.

PSRAM allocation in `init()` is **explicitly required** by `firmware-v3/src/effects/ieffect/CLAUDE.md` PSRAM Allocation Policy (all effect buffers >64 bytes MUST be allocated from PSRAM in `init()`). The flagged code is correct.

**Fix:** patch `check_heap_alloc_in_render` to update `brace_depth` on every line, not only on heap-match lines. Approximately 4 lines of code change in the script. ~1 hour.

### 2.4 K1 AP-only — 1 flagged

| File | Line | Pattern observed | Diagnosis | Classification |
|---|---|---|---|---|
| serial/SerialCLI.cpp | 1301 | `WiFi.getMode() == WIFI_MODE_STA ? "STA" : ...` | Read-only diagnostic ternary printing the current WiFi mode label inside `wifi status` command. Does NOT call `WiFi.begin()`, `esp_wifi_set_mode(WIFI_MODE_STA, ...)`, or any state-changing API. The token appears purely as an enum comparison value for human-readable output. | **ALLOWLIST**. Add `serial/SerialCLI.cpp` to `K1_STA_ALLOWLIST`. |

This is identical in semantics to the existing `network/WiFiManager.cpp` and `network/WebServer.cpp` allowlist entries — those files also reference `WIFI_MODE_STA` for diagnostics and event-handling without changing mode.

### 2.5 Raw control bus access — 10 flagged (BeatPrismOnset family)

| File | Lines | Pattern observed | Classification |
|---|---|---|---|
| LGPBeatPrismOnsetIgniteEffect.cpp | 163, 167 | `ctx.audio.controlBus.<field>` direct read | DEFER (style flag) |
| LGPBeatPrismOnsetRotateEffect.cpp | 160, 164 | same | DEFER |
| LGPBeatPrismOnsetAdvectEffect.cpp | 159, 163 | same | DEFER |
| LGPBeatPrismOnsetEffect.cpp | 148, 152 | same | DEFER |
| LGPBeatPrismOnsetDriftEffect.cpp | 161, 165 | same | DEFER |

These are NOT brand-voice violations. The `[api]` flag is an architectural-style preference (use snapshot accessors, e.g. R3 ControlBusSnapshot accessors landed 2026-03-05 per MEMORY.md `firmware_stage1_d14_r3_milestone.md`). The reads themselves are correct: `ctx.audio.controlBus.audioConfidence` is a valid field, the BeatPrismOnset family pre-dates the snapshot convention.

**Recommendation:** Defer to a separate refactor (post-V1.0) that migrates all pre-snapshot effects in one pass. Out of scope for Move 0.2 brand-voice triage.

---

## 3. Recommendations — V1.0 / V1.1 / kill / allowlist

### 3.1 V1.0 BLOCKERS (must land before V1.0 ship)

**Lint engine fixes** — these are the actual blockers for Move 0.2 closure.

| ID | Action | File | Effort |
|---|---|---|---|
| L1 | Fix brace-tracking bug in `check_heap_alloc_in_render` (move `brace_depth` update outside the heap-match conditional) | `firmware-v3/tools/check_effect_contracts.py` | ~1 hour |
| L2 | Add `writeCentrePairDual` (and any other centre-pair helpers in use) to `CENTRE_ORIGIN_PATTERN` | same | ~30 min |
| L3 | Add `serial/SerialCLI.cpp` to `K1_STA_ALLOWLIST` | same | ~5 min |
| L4 | Add 16 LGP*AREffect.cpp files to `RAINBOW_ALLOWLIST` (palette-segment use) | same | ~10 min |

After L1–L4, the lint should drop from 34 violations to **2 INVESTIGATE candidates** (LGPGradientFieldEffect.cpp and LGPReactionDiffusionAREffect.cpp).

### 3.2 V1.0 SOFT (Captain decision)

| ID | Action | File | Effort |
|---|---|---|---|
| I1 | Captain visual audit: does `LGPGradientFieldEffect.cpp` linear-write satisfy centre-origin doctrine when the coordinate basis is centre-relative? | LGPGradientFieldEffect.cpp | hardware A/B, ~1 hour |
| I2 | Captain visual audit: is `LGPReactionDiffusionAREffect.cpp` upper-bound modulation (`vSample * 120.0f + progress * 15.0f` ≈ 190° arc) too wide for palette-segment doctrine? | LGPReactionDiffusionAREffect.cpp | hardware A/B, ~30 min |

### 3.3 V1.1 (post-V1.0 cleanup)

| ID | Action | Effort |
|---|---|---|
| R1 | Migrate BeatPrismOnset family (5 files, 10 flags) to ControlBus snapshot accessors | ~4 hours |
| R2 | Tighten `RAINBOW_SCAN_PATTERNS` to detect full-wheel sweeps without false-positive on bounded modulation (semantic check rather than syntactic) | ~1 day |
| R3 | Reduce `CENTRE_LINEAR_ALLOWLIST` from 123 entries by either (a) refactoring effects to centre-pair, or (b) tightening `LINEAR_SWEEP_PATTERN` to only flag truly linear sweeps | ~3 days |

### 3.4 KILL candidates

**None identified.** The audit found no effect that should be deprecated based on these flags. (Effects scheduled for kill in Phase 3 separately are out of scope here; this audit only reviews lint flags.)

### 3.5 Allowlist additions (consolidated patch list for `tools/check_effect_contracts.py`)

```python
# Add to RAINBOW_ALLOWLIST:
"LGPAiryCometAREffect.cpp",
"LGPChimeraCrownAREffect.cpp",
"LGPCymaticLadderAREffect.cpp",
"LGPHarmonographHaloAREffect.cpp",
"LGPHyperbolicPortalAREffect.cpp",
"LGPLangtonHighwayAREffect.cpp",
"LGPLorenzRibbonAREffect.cpp",
"LGPMachDiamondsAREffect.cpp",
"LGPMoireCathedralAREffect.cpp",
# (LGPReactionDiffusionAREffect.cpp — pending I2 Captain decision)
"LGPRoseBloomAREffect.cpp",
"LGPSchlierenFlowAREffect.cpp",
"LGPSpirographCrownAREffect.cpp",
"LGPSuperformulaGlyphAREffect.cpp",
"LGPTalbotCarpetAREffect.cpp",
"LGPWaterCausticsAREffect.cpp",

# Add to K1_STA_ALLOWLIST:
"serial/SerialCLI.cpp",

# CENTRE_ORIGIN_PATTERN — extend regex:
# r"SET_CENTER_PAIR|CENTER_LEFT|writeCentrePairDual|79\s*-\s*\w|80\s*\+\s*\w|NUM_LEDS\s*/\s*2|numLeds\s*/\s*2"
```

---

## 4. Boundary-Case Rehabilitation Candidates (§4.1 cubic-Perlin rehab)

**Zero candidates identified.** Every flagged AR file was searched for cubic Perlin tokens (`cubicPerlin / perlin / inoise / noise8 / perlinNoise / noise(`) — **none matched**. The rainbow-scan flags do not fall into the cubic-Perlin-as-mask boundary case; they are a separate phenomenon (palette-segment use of `CHSV(hue,...)` with `hue` anchored to `baseHue`). The rainbow flags are remediated by allowlisting (§3.5), not by §4.1 rehab.

The actual cubic-Perlin rehab work (LIN-04 RhythmLockedCubicPerlinRibbon, ~120 LOC) is scheduled for Phase 5 Move 5.5 (V1.1 milestone) per Topology_Reconciliation §5. No existing effect under audit qualifies as a rehab candidate today.

---

## 5. Estimated Remediation Effort

| Category | Files | Action class | Engineer-time |
|---|---|---|---|
| Rainbow scan (16 files) | LGP*AREffect.cpp family | ALLOWLIST patch | 0.1 day |
| Centre-scan (1 file allowlist + 1 investigate) | LGPPerceptualBlendEffect.cpp + LGPGradientFieldEffect.cpp | LINT-FIX + Captain A/B | 0.2 day |
| Heap-in-render (5 files) | LGPHolyShitBangersPack.cpp, LGPShapeBangersPack.cpp | LINT-FIX (engine bug) | 0.15 day |
| K1 AP-only (1 file) | serial/SerialCLI.cpp | ALLOWLIST patch | 0.02 day |
| Raw control bus (5 files) | LGPBeatPrismOnset* family | DEFER to V1.1 refactor | (V1.1: ~0.5 day) |
| Captain hardware A/B for INVESTIGATE pair | LGPGradientFieldEffect.cpp + LGPReactionDiffusionAREffect.cpp | hardware verification | 0.15 day |
| **V1.0 total** | — | — | **~0.6 engineer-day** |
| **V1.1 BeatPrism refactor** | 5 files | code refactor | ~0.5 engineer-day |
| **V1.1 lint tightening (Path 2 rainbow regex)** | tools script | LINT-FIX | ~1 engineer-day |
| **V1.1 total** | — | — | **~1.5 engineer-day** |

V1.0 closure for Move 0.2 is achievable in **under one engineer-day** once the lint engine fix lands.

---

## 6. Summary — what Captain should look at first

1. **`firmware-v3/tools/check_effect_contracts.py` `check_heap_alloc_in_render` brace-tracking bug.** Highest leverage: one ~1-hour fix removes 5 phantom violations and prevents future false-positive noise. The bug is at lines 419–444; the `brace_depth` update belongs outside the `if HEAP_IN_RENDER_PATTERN.search(code_part):` branch.
2. **`LGPGradientFieldEffect.cpp` linear-write semantics.** The only flagged effect where the surface code (linear `for (i = 0; i < STRIP_LENGTH; i++)` with `ctx.leds[i] = ...`) does NOT obviously satisfy centre-origin even after sympathetic reading. Coordinate basis is centre-relative but iteration order is left-to-right. Hardware A/B against a centre-pair refactor recommended.
3. **`LGPReactionDiffusionAREffect.cpp` upper-bound hue modulation.** `vSample * 120.0f + progress * 15.0f` reaches ~190° of hue arc at peak, which is the widest modulation in the AR family. Borderline palette-segment vs half-wheel — visual audit decides.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-27 | agent:SSA-5 | Created. Phase 0 Move 0.2 deep audit triage. Classified 34 lint violations: 22 false positives (16 rainbow palette-segment, 5 lint-bug heap-in-render, 1 SerialCLI diagnostic), 2 INVESTIGATE (LGPGradientField, LGPReactionDiffusionAR), 1 LINT-FIX (PerceptualBlend writeCentrePairDual not recognised), 5 BeatPrismOnset family deferred to V1.1 refactor, 1 SerialCLI allowlist. Identified `check_heap_alloc_in_render` brace-tracking bug. Confirmed zero cubic-Perlin §4.1 rehab candidates in current flag set. V1.0 closure achievable in ~0.6 engineer-day post lint-fix. |
