# LightwaveOS

ESP32-S3 LED controller for a dual-strip Light Guide Plate. 320 WS2812 LEDs, 100+ effects, audio-reactive, web-controlled.

## Build (PlatformIO)

```bash
cd firmware-v3

# Primary development profile (PipelineCore backend)
pio run -e esp32dev_audio_pipelinecore
pio run -e esp32dev_audio_pipelinecore -t upload

# Alternative default profile (ESV11 backend; [platformio] default_envs)
pio run -e esp32dev_audio_esv11
pio run -e esp32dev_audio_esv11 -t upload

# Serial monitor
pio device monitor -b 115200
```

## Hard Constraints

- **Centre origin**: All effects originate from LED 79/80 outward (or inward to 79/80). No linear sweeps.
- **No rainbows**: No rainbow cycling or full hue-wheel sweeps.
- **No heap alloc in render**: No `new`/`malloc`/`String` in `render()` paths. Use static buffers.
- **120 FPS target**: Keep per-frame effect code under ~2 ms.
- **British English** in comments and docs (centre, colour, initialise).

## Workflow Discipline (Agents)

These rules were codified after the 2026-04-27/28 orchestration drift (see `~/.claude/plans/shit-got-fucked-but-groovy-neumann.md`). Follow them strictly.

1. **Single source of truth for forward work is `BACKLOG.md`** (root). Do NOT write `.claude/handoff*.md` files containing forward TODO lists. Postmortems describing what shipped (with commit hashes) are fine; forward TODOs in `.claude/` are forbidden because they create re-prescription loops where the next session "applies patches" that already landed in commits the handoff didn't see.

2. **`feat(...)` commits MUST anchor to a Phase Move per the Synergy-Topology programme taxonomy in `BACKLOG.md`**, or be tagged `chore` / `fix` / `docs` / `ci` / `test` / `refactor`. A bare `feat(firmware): add X` body without a Phase Move reference (or without a `Captain visual sign-off:` line for Phase 5+ effect commits) is reviewable. The Phase 5 commit `39406e6b` violated this rule and bypassed the visual sign-off gate; do not repeat.

3. **Anti-redundancy gate before applying any patch**: `grep` the target lines for the proposed change. If the substantive content is already there, ABORT — surface to the operator instead of writing a no-op edit. The 2026-04-27 drift was driven by multiple sessions reading a stale handoff and "applying patches" that were already in HEAD. The Edit tool's `old_string` exact-match discipline is not a sufficient guard; explicit grep before edit is.

4. **Sandbox-to-integration return-receipt**: when dispatching parallel SSAs that return diffs (per `parallel-agent-sandboxing` skill), the orchestrator MUST verify the integrated state against the SSA's claimed deliverables before declaring the SSA complete. Commit `6b1a222f` is a verbatim record of the cost of skipping this step (lost edits, rework session needed). The plan that produced this rule cites the failure pattern.

5. **For every commit, the body cites its plan or BACKLOG row**: `Refs: plan <name>.md Tier X.Y` or `Refs: BACKLOG.md §<section>`. This makes the chain from intention → commit auditable in `git log` alone, no extra tooling needed.

## Visual Pipeline Guardrails (Agents)

- **Treat `FastLED.show()` wire time as a safety invariant** on dual 160-LED strips. Expected average is ~4.8-5.5 ms. If telemetry shows ~1 ms, assume premature return/tearing risk and stop.
- **Do not trust a single FPS field in isolation**. Always cross-check `framesRendered / uptime` and `avgFrameTimeUs`.
- **Renderer cadence must be self-clocked to 120 FPS budget** (`~8333 us` frame pacing), not RTOS tick-quantised.
- **K1v2 must keep status strip disabled** (`FEATURE_STATUS_STRIP_TOUCH=0`); K1v1 may enable it.
- **Large shared snapshots belong in persistent buffers (PSRAM-preferred) allocated at init**, never on `loopTask` stack hot paths.
- **Before upload, verify device identity by MAC** and confirm target env/pin mapping; never rely on serial port name alone.
- **After any render/memory change, run serial `s` checks** and confirm:
  - no panics/RMT errors;
  - `LED show` sane for configured strip length;
  - `showSkips=0` under normal load;
  - stack/heap headroom stable (no progressive collapse).

## Further Docs

Read these when the task requires it:

| Topic | File |
|-------|------|
| Timing & memory budgets | [firmware-v3/CONSTRAINTS.md](firmware-v3/CONSTRAINTS.md) |
| Audio-reactive protocol | [firmware-v3/docs/audio-visual/audio-visual-semantic-mapping.md](firmware-v3/docs/audio-visual/audio-visual-semantic-mapping.md) |
| Full REST API reference | [firmware-v3/docs/api/api-v1.md](firmware-v3/docs/api/api-v1.md) |
| CQRS state architecture | [firmware-v3/docs/CQRS_STATE_ARCHITECTURE.md](firmware-v3/docs/CQRS_STATE_ARCHITECTURE.md) |
| LED incident lessons | [firmware-v3/docs/INCIDENT_LED_STABILITY_POSTMORTEM_2026-03-04.md](firmware-v3/docs/INCIDENT_LED_STABILITY_POSTMORTEM_2026-03-04.md) |
| Harness worker mode | [.claude/harness/HARNESS_RULES.md](.claude/harness/HARNESS_RULES.md) |
