# Revert I-3 Fix + Investigate K1 Bloom V1 + Phase 5 Prep

## RBDO Gate

**GROUNDED** — premises traced to: memory archaeology (#35056, #35041 confirming dt×60 was deliberately chosen), blast-radius grep (47 sites using powf(*60.0f) as project convention), full source reads of SbK1BaseEffect.h/.cpp + SbK1BloomEffect.cpp + SbK1BloomV2Effect.cpp, and Captain's visual A/B results showing the I-3 fix (066c3fbd) did not fix K1 Bloom 0x1301, Plasma, Ocean, Wave Collision Enhanced, or Spectral Spread.

## Context

The I-3 calibration fix at commit 066c3fbd was based on a wrong premise. The `dt * 60` reference rate in dtDecay was DELIBERATELY chosen as the project convention, not an accidental mismatch. The fix helped Snapwave coincidentally but didn't fix any of the actually-broken effects because their problems are structural (triple-multiplication phase rate, Spring instability, binary triggers), not trail-decay calibration. The fix also created a convention inconsistency: fadeToBlackByDt now uses 120 FPS reference while the rest of the codebase (47 sites, 24 files) uses 60 FPS.

K1 Bloom 0x1301's "almost static" symptom has a DIFFERENT root cause that needs investigation. The m_dt hypothesis has been REJECTED — m_dt is properly initialised (default 1/120) and set every frame via baseProcessAudio(). At dt=1/120, blendAlpha ≈ 0.24 per frame — the centre injection is responsive. The bug is elsewhere in V1's render path.

## Hard Constraints

- esptool-direct flash only (NOT `pio run -t upload`)
- Read K1v2 flash procedure from memory files before flashing
- Verify MAC (K1v2: `b4:3a:45:a5:87:f8`) before flash
- Centre origin, no heap in render(), 2.0ms ceiling, British English

## Task 1: Revert 066c3fbd

```bash
cd /path/to/Lightwave-Ledstrip
git revert 066c3fbd --no-edit
```

Verify `git log --oneline -3` shows the revert commit on top. Verify `git diff HEAD~1 -- firmware-v3/src/effects/PersistenceHelpers.h` shows only the revert (rate squaring removed, original rate conversion restored).

Build:
```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz
```

Must succeed. Do NOT flash yet — wait until Task 2 is complete so we flash once.

## Task 2: Investigate K1 Bloom V1 (0x1301)

### What we know (do NOT re-investigate)

- m_dt is NOT the bug. It's properly set every frame via `m_dt = ctx.getSafeDeltaSeconds()` in baseProcessAudio(). blendAlpha ≈ 0.24 per frame at 120 FPS — responsive.
- V2 (0x1303) WORKS. It shares the same base class (SbK1BaseEffect) and audio pipeline. The bug is in V1-specific code.
- V1 uses sub-pixel scroll accumulator (`m_scrollAccum`), full RGB centre injection with EMA smoothing, additive chromagram colour synthesis.
- V2 uses integer scroll (1 or 2 whole pixels), grayscale centre injection (direct assignment), dominant-bin colour at output stage.

### What you need to find

Read the FULL `SbK1BloomEffect.cpp` render path (the `renderEffect()` method, every line). You're looking for:

1. **Scroll mechanism:** How is `m_scrollAccum` computed? What feeds it (audio energy, speed parameter, constant)? Is the scroll amount plausibly near-zero under normal conditions? A scroll of 0 with centre injection working would produce a slowly-colour-shifting static blob — close to "almost static."

2. **Scroll execution:** How does the sub-pixel scroll physically move pixels in the buffer? Is it a memmove/shift-register, a scatter, or a re-index? Is there an off-by-one or a direction error that causes the scroll to cancel itself?

3. **Audio feed:** V1 builds `bloomColor` from chromagram. If the chromagram is zero or near-zero (e.g., because of a silence gate or a normalisation bug), the centre injection would inject black — even with blendAlpha=0.24, blending toward black produces a fade-out, not motion.

4. **Buffer management:** V1 uses `workBuf` and `prevBuf`. How are they swapped? If prevBuf is never updated (or always equals workBuf), the EMA blend becomes a no-op.

5. **I-3 migration impact:** Did the I-3 migration (da33b3e6) touch SbK1BloomEffect.cpp? If so, what fadeToBlackBy sites were converted? Check: `git show da33b3e6 -- firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BloomEffect.cpp`

6. **Comparison with V2:** V2 works. The critical architectural difference is that V2 uses integer scroll with direct grayscale injection. V1 uses sub-pixel scroll with EMA-smoothed RGB injection. The EMA smoothing is not the bug (blendAlpha is fine). Focus on whether the sub-pixel scroll itself has a defect — a fractional scroll that rounds to zero, or a scroll direction that fights itself on a centre-origin strip.

### Decision logic

**If you find a simple, isolated bug** (uninitialised variable, off-by-one, scroll direction error, wrong buffer swap): fix it, add a comment explaining the fix, and commit separately. This is a bugfix, not a Phase 5 port.

**If the problem is structural** (the entire V1 render architecture is flawed — e.g., the sub-pixel scroll + EMA smoothing + additive chromagram synthesis interact to produce instability under certain audio conditions): do NOT attempt a fix. Report the finding with file:line evidence and flag it as Phase 5 scope. V1 would then be ported to `drawSpriteScrolled` in Phase 5 alongside the other broken effects.

**If you cannot determine the root cause after reading the full render path:** report what you examined, what you ruled out, and what remains unexplored. Do NOT guess or apply speculative fixes.

## Task 3: Build and Flash

After Task 1 (revert) and Task 2 (investigation + possible fix):

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz
```

Must succeed with zero errors.

Flash K1v2 using esptool-direct (read `feedback_k1v2_flash_procedure.md` for exact command). Verify MAC before flash.

## Task 4: Report

```
TASK 1: REVERT
  Commit hash: [revert commit hash]
  Build: [success/failure]

TASK 2: K1 BLOOM V1 INVESTIGATION
  Root cause: [simple bug / structural / undetermined]
  Evidence: [file:line references]
  Fix applied: [yes — describe / no — Phase 5 scope / no — undetermined]
  Fix commit hash: [if applicable]

  Specific findings:
    Scroll mechanism: [how m_scrollAccum works, what feeds it, plausible zero?]
    Scroll execution: [shift-register / scatter / re-index, any defects?]
    Audio feed: [bloomColor source, any zero/silence path?]
    Buffer management: [workBuf/prevBuf swap logic, any staleness?]
    I-3 impact: [was V1 touched by da33b3e6? which lines?]

TASK 3: FLASH
  K1v2 flashed: [yes/no — if no, why]
  Boot clean: [yes/no]

HARDWARE VALIDATION (Captain to perform):
  K1 Bloom 0x1301: [pending Captain visual test]
  0x1303 K1 Bloom V2: [pending — control case, should still work]

OPEN ISSUES: [any]
```

## What NOT To Do

1. Do NOT re-investigate m_dt. It's confirmed working (default 1/120, set every frame, blendAlpha=0.24).
2. Do NOT modify dtDecay or dtDecay3. The 60 FPS convention is correct and deliberate.
3. Do NOT modify any effect OTHER than SbK1BloomEffect (and only if a simple isolated bug is found).
4. Do NOT attempt a Phase 5 port of V1 — that's separate scope with its own prompt.
5. Do NOT use `pio run -t upload` to flash.
6. Do NOT apply speculative fixes. Evidence-backed only.
