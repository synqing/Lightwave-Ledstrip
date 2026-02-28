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

## Context Management

This is a context-economics optimisation mechanism, not a convenience trick.
LLMs have finite working memory (context window). Every file read consumes tokens.
Use subagents as isolated scratchpads: let them perform broad or noisy exploration, then return only distilled conclusions to main context.

Mental model:
- Main agent = executive brain
- Subagent = disposable research assistant
- Context window = working memory

Without delegation, main context accumulates low-value detail and degrades decision quality. With delegation, main context keeps decisions, constraints, and outcomes.

Context is a constrained resource. Proactively use subagents (Task tool) for exploration, research, and verbose analysis so the main conversation keeps only decisions and outcomes.

**Default to spawning subagents for:**
- Codebase exploration requiring 3+ file reads
- Research tasks (docs, web lookups, behaviour investigation)
- Code review or broad analysis that generates verbose output
- Investigations where only summary conclusions are needed

**Stay in main context for:**
- Direct file edits explicitly requested by the user
- Short, targeted reads (1-2 files)
- Back-and-forth design/implementation iteration
- Tasks where the user needs to see intermediate steps

**Task-scoped constraints when delegating:**
- Pass only constraints relevant to the delegated task
- Keep global repo-wide invariants
- Include effects/render constraints (centre origin, no rainbows, no heap alloc in `render()`, 120 FPS budget) only if the task touches effects, rendering, LED mapping, or visual performance paths

**Subagent return contract:**
- Files inspected
- Concrete findings and recommendations
- Confidence level and open questions

**Rule of thumb:** If the task will read more than ~3 files or produce output the user does not need verbatim, delegate and return a distilled summary.

## Further Docs

Read these when the task requires it:

| Topic | File |
|-------|------|
| Timing & memory budgets | [firmware-v3/CONSTRAINTS.md](firmware-v3/CONSTRAINTS.md) |
| Audio-reactive protocol | [firmware-v3/docs/audio-visual/audio-visual-semantic-mapping.md](firmware-v3/docs/audio-visual/audio-visual-semantic-mapping.md) |
| Full REST API reference | [firmware-v3/docs/api/api-v1.md](firmware-v3/docs/api/api-v1.md) |
| CQRS state architecture | [firmware-v3/docs/CQRS_STATE_ARCHITECTURE.md](firmware-v3/docs/CQRS_STATE_ARCHITECTURE.md) |
| Harness worker mode | [.claude/harness/HARNESS_RULES.md](.claude/harness/HARNESS_RULES.md) |
