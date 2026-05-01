# Phase 1+2 Commit + I-3 fadeToBlackByDt Calibration Fix

## RBDO Gate

**GROUNDED** — all premises traced to verbatim source reads of PersistenceHelpers.h (lines 57–124), the I-3 migration diff (da33b3e6, 97 files, 136 fadeToBlackBy→fadeToBlackByDt sites), the Phase 1+2 implementation files (RenderPrimitives.h/.cpp, FrameBlend.h/.cpp, test_render_primitives.cpp), and a blast-radius grep confirming 0 external callers of dtDecay3 and 47 sites using `powf(... * 60.0f)` as the project-wide 60 FPS convention.

Captain has reviewed the Phase 1+2 code line-by-line and approved commit. Captain has reviewed the I-3 regression analysis and approved the scoped fix.

## Context

Two tasks in one prompt:

**Task A:** Commit the Phase 1+2 render primitives + frame blend substrate. These files are already written, tested, and compilation-verified. The ELF link-graph proves they are inert (zero callers in the linked binary). Pure additive — no existing effects modified.

**Task B:** Fix the I-3 regression in `fadeToBlackByDt`. The I-3 migration (commit da33b3e6) bulk-converted 136 `fadeToBlackBy` call sites to `fadeToBlackByDt`, preserving K values. The K values encode per-frame survival calibrated at 120 FPS (K1's frame rate). But `dtDecay3` uses a 60 FPS reference (`powf(rate, dt * 60.0f)`). At 120 FPS this produces `sqrt(rate)` per frame instead of `rate` — trails persist ~2× longer than intended. The symptom: effects like K1 Bloom 0x1301 appear nearly static, Ocean/Plasma are visually degraded, LGP Wave Collision Enhanced is broken.

## Hard Constraints

- **esptool-direct flash only** — do NOT use `pio run -t upload`. Read `~/.claude/projects/*/memory/feedback_k1v2_flash_procedure.md` for the exact esptool command before flashing.
- **Verify MAC before flash** — K1v2 MAC is `b4:3a:45:a5:87:f8`. Confirm via serial before flashing.
- **Centre origin, no heap in render(), 2.0ms ceiling, British English** — all standard constraints apply.
- **Pre-commit confidence gate** — constraints / tests / scope / British English verified before each commit.

## Reference Files (READ BEFORE STARTING)

1. `firmware-v3/src/effects/PersistenceHelpers.h` — the file you will modify (lines 118–124 specifically).
2. `firmware-v3/src/effects/render/RenderPrimitives.h` — verify files exist and are unchanged.
3. `firmware-v3/src/effects/render/RenderPrimitives.cpp` — verify files exist and are unchanged.
4. `firmware-v3/src/effects/render/FrameBlend.h` — verify files exist and are unchanged.
5. `firmware-v3/src/effects/render/FrameBlend.cpp` — verify files exist and are unchanged.
6. `firmware-v3/test/test_render_primitives/test_render_primitives.cpp` — verify exists and tests pass.
7. Read the K1v2 flash procedure from memory files before attempting to flash.

## Task A: Commit Phase 1+2

### Step 1 — Verify unit tests pass

```bash
cd firmware-v3
pio test -e native_test_render_primitives
```

All 14 tests must pass. If any fail, STOP and report — do not commit.

### Step 2 — Verify production build compiles

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz
```

Must succeed with zero errors. Note any warnings but proceed.

### Step 3 — Commit Phase 1+2

```bash
cd /path/to/Lightwave-Ledstrip
git add firmware-v3/src/effects/render/RenderPrimitives.h
git add firmware-v3/src/effects/render/RenderPrimitives.cpp
git add firmware-v3/src/effects/render/FrameBlend.h
git add firmware-v3/src/effects/render/FrameBlend.cpp
git add firmware-v3/test/test_render_primitives/test_render_primitives.cpp
git add firmware-v3/platformio.ini
```

Verify `git diff --cached --stat` shows ONLY these 6 files. Nothing else. If other files appear in the staging area, unstage them.

Commit message:
```
feat(effects): Phase 1+2 — Layer 4 render primitives + Layer 5 frame blend

Additive substrate for the visual pipeline reform. Three centre-origin
render primitives (drawDot, drawSpriteScrolled, fillFromBins) and the
mood-controlled frame post-process (applyFrameBlending). Unwired — no
existing effects call these functions yet; integration is Phase 5.

14 unit tests pass on native. Production build (esp32dev_audio_esv11_k1v2_32khz)
compiles clean. ELF link-graph confirms zero callers in the linked binary.

Refs: PIPELINE_REFORM.md §2 Layer 4+5
```

## Task B: Fix the I-3 Regression

### Step 4 — Apply the fix

Edit `firmware-v3/src/effects/PersistenceHelpers.h`. The current code at lines 118–124:

```cpp
static inline void fadeToBlackByDt(CRGB* leds, size_t n,
                                    uint8_t fadeBy, float dt) {
    const float rate60fps = (256.0f - static_cast<float>(fadeBy)) / 256.0f;
    for (size_t i = 0; i < n; ++i) {
        dtDecay3(leds[i], rate60fps, dt);
    }
}
```

Replace with:

```cpp
static inline void fadeToBlackByDt(CRGB* leds, size_t n,
                                    uint8_t fadeBy, float dt) {
    // K values (fadeBy 0–255) were calibrated at K1's native 120 FPS with
    // frame-coupled fadeToBlackBy. dtDecay3 uses a 60 FPS reference
    // (powf(rate, dt * 60)). To preserve the visual time-constant that
    // effects were tuned at, convert the 120 FPS per-frame survival to a
    // 60 FPS equivalent: rate60 = rate120² (one 60 FPS frame = two 120 FPS
    // frames). Verified: at 120 FPS, powf(rate120², dt*60) = rate120.
    const float rate120 = (256.0f - static_cast<float>(fadeBy)) / 256.0f;
    const float rate60fps = rate120 * rate120;
    for (size_t i = 0; i < n; ++i) {
        dtDecay3(leds[i], rate60fps, dt);
    }
}
```

Also update the docstring above the function (lines 105–117). Replace:

```cpp
/**
 * @brief Dt-correct fade-to-black over an LED strip segment.
 *
 * Drop-in replacement for FastLED's frame-coupled `fadeToBlackBy(leds, n, X)`.
 * `fadeBy` is on the same 0–255 scale; internally converts to a dt-correct
 * per-channel survival rate: rate = (256 - fadeBy) / 256.
 *
 * Migration: s/fadeToBlackBy(p, n, X)/fadeToBlackByDt(p, n, X, ctx.getSafeDeltaSeconds())/g
 *
 * @param leds    Pointer to the first CRGB element.
 * @param n       Number of LEDs to process.
 * @param fadeBy  Fade amount 0–255 (FastLED convention). 0 = no fade, 255 = instant black.
 * @param dt      Actual frame interval in seconds (use ctx.getSafeDeltaSeconds()).
 */
```

With:

```cpp
/**
 * @brief Dt-correct fade-to-black over an LED strip segment.
 *
 * Drop-in replacement for FastLED's frame-coupled `fadeToBlackBy(leds, n, X)`.
 * `fadeBy` is on the same 0–255 scale (FastLED convention).
 *
 * The K values across the codebase were calibrated at K1's native 120 FPS
 * with frame-coupled `fadeToBlackBy`, where `fadeBy=32` produced a per-frame
 * survival of `(256-32)/256 = 0.875`. To preserve that visual behaviour
 * under dt-correction (which uses a 60 FPS reference internally), the
 * per-frame survival is squared: `rate60fps = rate120fps²`. This ensures
 * `powf(rate60fps, dt*60)` evaluates to `rate120fps` when dt = 1/120 s.
 *
 * @param leds    Pointer to the first CRGB element.
 * @param n       Number of LEDs to process.
 * @param fadeBy  Fade amount 0–255 (FastLED convention). 0 = no fade, 255 = instant black.
 * @param dt      Actual frame interval in seconds (use ctx.getSafeDeltaSeconds()).
 */
```

### Step 5 — Verify the math (sanity check in the commit message)

For fadeBy=32:
- rate120 = (256-32)/256 = 224/256 = 0.875
- rate60fps = 0.875² = 0.765625
- At 120 FPS (dt=1/120): powf(0.765625, (1/120)*60) = powf(0.765625, 0.5) = 0.875 ✓ (matches original frame-coupled)
- At 60 FPS (dt=1/60): powf(0.765625, (1/60)*60) = 0.765625 (correct dt-independence)
- Per-second decay: 0.875^120 = 0.765625^60 ✓ (frame-rate independent)

### Step 6 — Build

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz
```

Must succeed with zero errors.

### Step 7 — Commit the I-3 fix

```bash
cd /path/to/Lightwave-Ledstrip
git add firmware-v3/src/effects/PersistenceHelpers.h
```

Verify `git diff --cached --stat` shows ONLY PersistenceHelpers.h. Nothing else.

Commit message:
```
fix(effects): I-3 calibration — fadeToBlackByDt rate120→rate60 conversion

The I-3 migration (da33b3e6) preserved fadeBy K values when converting
136 fadeToBlackBy sites to fadeToBlackByDt. K values were calibrated at
K1's native 120 FPS with frame-coupled decay, but dtDecay3 uses a 60 FPS
reference (powf(rate, dt*60)). At 120 FPS this produced sqrt(rate) per
frame instead of rate — trails persisted ~2x longer than intended.

Fix: square the per-frame survival when converting from K value to 60 FPS
reference: rate60fps = rate120fps². This preserves the visual time-constant
at 120 FPS while gaining correct dt-independence at other frame rates.

Verified: fadeBy=32 → rate120=0.875 → rate60=0.766 → at 120 FPS: 0.875 ✓
Blast radius: scoped to fadeToBlackByDt only. dtDecay/dtDecay3 signature
unchanged. ChromaUtils.h scalar dtDecay (16 files, 20 sites) unaffected.
47 powf(*60.0f) sites unaffected.
```

### Step 8 — Flash K1v2 and verify

**Before flashing:**
1. Read the K1v2 flash procedure from memory files (`feedback_k1v2_flash_procedure.md`).
2. Verify K1v2 is connected: `ls /dev/cu.usbmodem*` — should show a device.
3. Verify MAC via serial monitor: `pio device monitor -b 115200`, check startup output for MAC `b4:3a:45:a5:87:f8`. Close the monitor before flashing.
4. Use esptool-direct to flash. Do NOT use `pio run -t upload`.

**If K1v2 is not connected** (ls /dev/cu.usbmodem* returns empty):
- STOP. Report that hardware is not available.
- Both commits (Phase 1+2 and I-3 fix) are safe to push without hardware validation because:
  - Phase 1+2 is proven inert (ELF link-graph, zero callers).
  - The I-3 fix is a mathematical identity at 120 FPS (rate120² through sqrt = rate120).
- But the visual regression validation requires hardware. Flag this as pending.

**If K1v2 IS connected:**
After flashing, test these specific effects and report the visual result for each:

1. **K1 Bloom 0x1301** — previously "completely fucked, almost static". Should now show visible motion/animation.
2. **Plasma** — previously "visually degraded". Should look normal (dynamic, colourful).
3. **Ocean** — previously "visually degraded". Should look normal.
4. **LGP Wave Collision Enhanced** — previously "fucked". Should show travelling wave collision pattern.
5. **0x130B K1 Bloom Spectral Spread** — previously "broken". Should animate.
6. **0x1303 K1 Bloom V2** — previously "looks possibly better". Confirm it still looks good (control case).

For each effect, report: effect name, visual state (working/degraded/broken), and a one-sentence description of what you see.

## What NOT To Do

1. **Do NOT change dtDecay or dtDecay3 function signatures.** The fix is scoped to fadeToBlackByDt's rate conversion only.
2. **Do NOT change ChromaUtils.h.** The 16 scalar dtDecay callers are a separate investigation.
3. **Do NOT change any of the 47 powf(*60.0f) sites.** Those are the project-wide convention; they're not broken by this fix.
4. **Do NOT modify any effect files.** This fix is in PersistenceHelpers.h only.
5. **Do NOT use `pio run -t upload`** to flash K1. esptool-direct only.
6. **Do NOT commit both changes as a single commit.** Two separate commits: Phase 1+2 first, then I-3 fix. Clean history.

## Confidence Gate (before each commit)

1. Correct files staged? (`git diff --cached --stat` — only the expected files)
2. Build succeeds? (`pio run -e esp32dev_audio_esv11_k1v2_32khz` — zero errors)
3. Tests pass? (Phase 1+2 commit only: `pio test -e native_test_render_primitives` — 14/14)
4. No heap allocation in render path? (Phase 1+2: verified by test 14)
5. British English? (Comments use centre, colour, behaviour)
6. Scope clean? (No accidental file inclusions)

## Stale Memory Fix (minor, do alongside)

Edit `~/.claude/projects/*/memory/feedback_hardware_test_before_commit.md`:
Replace any reference to `pio run -e ... -t upload --upload-port <port>` with a note that K1v2 uses esptool-direct flashing per `feedback_k1v2_flash_procedure.md`. One-line change. Not a separate commit — this is a memory file, not source.

## Return Format

```
TASK A: PHASE 1+2 COMMIT
  Tests: [14/14 pass / X failures]
  Build: [success / failure + warnings]
  Commit hash: [hash]
  Files committed: [list]

TASK B: I-3 FIX
  Edit applied: [yes/no]
  Build: [success / failure]
  Commit hash: [hash]
  Math verification: fadeBy=32 → rate120=0.875 → rate60=0.766 → at 120 FPS: [value] (expect 0.875)

HARDWARE VALIDATION (if K1v2 connected):
  K1 Bloom 0x1301: [working/degraded/broken] — [description]
  Plasma: [working/degraded/broken] — [description]
  Ocean: [working/degraded/broken] — [description]
  LGP Wave Collision Enhanced: [working/degraded/broken] — [description]
  0x130B Spectral Spread: [working/degraded/broken] — [description]
  0x1303 K1 Bloom V2: [working/degraded/broken] — [description]

STALE MEMORY FIX: [applied / skipped]

OPEN ISSUES: [any]
```
