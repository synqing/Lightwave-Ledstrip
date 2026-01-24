# Context Pack Prompt

<!-- OpenAI Prompt Caching: Stable prefix (above) cached for ≥1024 tokens. Keep prefix unchanged for cache hits. -->

---

## 1. Stable Context (Cache This)

# LightwaveOS LLM Context

**Purpose**: Stable prefix file for LLM prompts. Include this instead of re-explaining the project.

**Version**: 1.0  
**Last Updated**: 2026-01-18

---

## Project Summary

LightwaveOS is ESP32-S3 firmware for a dual-strip Light Guide Plate (LGP) LED system. It drives 320 LEDs (2×160) with centre-origin effects, audio reactivity, and zone composition.

---

## Glossary

| Term | Definition |
|------|------------|
| **LGP** | Light Guide Plate — optical waveguide that creates interference patterns |
| **Centre origin** | Effects must originate from LEDs 79/80 and propagate outward |
| **IEffect** | Native effect interface (`init()`, `render()`, `cleanup()`, `getMetadata()`) |
| **Zone** | Configurable LED segment with independent effect/palette |
| **Actor** | Concurrent processing unit (RendererActor, AudioActor, etc.) |
| **TOON** | Token-Oriented Object Notation — compact format for LLM prompts |

---

## Architecture Map

```
firmware/v2/src/
├── core/actors/          # Actor system (Renderer, Audio, etc.)
├── effects/              # Legacy function effects
│   └── ieffect/          # Native IEffect classes
├── network/              # WebServer, WiFi, REST handlers
├── audio/                # Audio pipeline, beat detection
├── plugins/api/          # IEffect.h, EffectContext.h
└── palettes/             # Palette definitions
```

---

## Invariants (Non-Negotiable)

1. **Centre origin only**: All effects originate from LEDs 79/80, propagate outward (or converge inward)
2. **No rainbows**: No rainbow cycling or full hue-wheel sweeps
3. **No heap in render**: No `new`, `malloc`, `std::vector` growth, `String` concat in `render()` hot paths
4. **120 FPS target**: Keep per-frame work predictable
5. **British English**: Use British spelling (centre, colour, initialise)

---

## Constraints

| Constraint | Value |
|------------|-------|
| LED count | 320 (2×160) |
| Centre point | LEDs 79/80 |
| Target FPS | 120+ |
| ESP32-S3 SRAM | ~280KB available |
| Max effect state | Stack-allocated, no heap |

---

## Key Paths

| Purpose | Path |
|---------|------|
| Effects (IEffect) | `firmware/v2/src/effects/ieffect/` |
| Effect API | `firmware/v2/src/plugins/api/IEffect.h` |
| Renderer | `firmware/v2/src/core/actors/RendererActor.*` |
| WebServer | `firmware/v2/src/network/WebServer.*` |
| REST handlers | `firmware/v2/src/network/webserver/handlers/` |
| Dashboard | `lightwave-dashboard/src/` |
| Constraints | `constraints.md` |
| Architecture | `architecture.md` |

---

## Build Commands

```bash
# Build firmware (audio-enabled)
cd firmware/v2 && pio run -e esp32dev_audio

# Build + upload
cd firmware/v2 && pio run -e esp32dev_audio -t upload

# Serial monitor
pio device monitor -b 115200
```

---

## Serial Commands

| Command | Action |
|---------|--------|
| `s` | Print FPS/CPU status |
| `l` | List effects |
| `n/N` | Next/prev effect |
| `,/.` | Prev/next palette |
| `z` | Toggle zone mode |
| `validate <id>` | Run effect validation |

---

## Acceptance Check Rubric

When implementing or modifying effects, verify:

- [ ] **Centre origin**: Effect originates from LEDs 79/80
- [ ] **No rainbow**: No full hue-wheel cycling
- [ ] **No heap alloc**: No dynamic allocation in `render()`
- [ ] **Palette-aware**: Effect uses `ctx.palette`, not hardcoded colours
- [ ] **Zone-aware**: Effect respects `ctx.ledCount` and `ctx.zoneStart` if applicable
- [ ] **FPS maintained**: `validate <id>` shows no FPS regression
- [ ] **British English**: Comments use British spelling
- [ ] **Build passes**: `pio run -e esp32dev_audio` succeeds without errors

---

## See Also

- [AGENTS.md](../AGENTS.md) - Full agent workflow guide
- [CLAUDE.md](../CLAUDE.md) - Claude Code project instructions
- [constraints.md](../constraints.md) - Hard limits
- [architecture.md](../architecture.md) - Quick reference
- [docs/contextpack/README.md](contextpack/README.md) - Context Pack pipeline


<!-- DO NOT EDIT ABOVE THIS LINE - CACHE BARRIER -->

---

## 2. Semi-Stable: Packet Template

# Context Pack Packet

**Date**: 2026-01-19  
**Author**: [Your name]

---

## Goal

<!-- 1 paragraph: What you're trying to achieve -->

[Describe the objective clearly. What should be different when this is done?]

---

## What Changed

<!-- 1 paragraph: Recent modifications that led to this state -->

[Summarise recent changes. What was modified? What was the intent?]

---

## Current Symptom + Repro

<!-- Tight description of the problem -->

**Symptom**: [What's broken or unexpected?]

**Repro steps**:
1. [Step 1]
2. [Step 2]
3. [Step 3]

**Expected**: [What should happen]  
**Actual**: [What actually happens]

---

## Acceptance Checks

<!-- Explicit pass/fail targets -->

- [ ] [Check 1: Specific, measurable condition]
- [ ] [Check 2: Specific, measurable condition]
- [ ] [Check 3: Specific, measurable condition]

---

## Attachments

| File | Description |
|------|-------------|
| `diff.patch` | Git diff of changes |
| `logs.trimmed.txt` | Relevant log window (if applicable) |
| `fixtures/*.toon` | TOON fixtures for uniform arrays (if applicable) |
| `fixtures/*.csv` | CSV fixtures for flat tables (if applicable) |
| `fixtures/*.json` | JSON fixtures for nested structures (if applicable) |

---

## Context Reference

Stable context file: [`docs/LLM_CONTEXT.md`](../LLM_CONTEXT.md)

---

## Progressive Streaming

For large context packs or complex tasks, use progressive streaming to send context in chunks:

### Chunked Response Pattern

1. **Initial Request**: Send minimal context (packet.md + summary)
2. **Follow-up Chunks**: Send diff parts or fixture files as needed
3. **Resume Capability**: LLM can request specific chunks by name (e.g., "show me diff_part_02.patch")

### Streaming Guidelines

- **Start small**: Begin with `packet.md` and `summary_1line.md` (or `summary_5line.md`)
- **Add context incrementally**: Attach diff parts or fixtures only when LLM requests them
- **Use diff index**: If using multi-part diff, include `diff_index.md` for chunk overview
- **Progressive detail**: Use summary ladder (1-line → 5-line → 1-para) as needed

### Example Progressive Flow

```
Turn 1: Send packet.md + summary_1line.md
Turn 2: LLM requests "show me the diff" → Send diff_part_01.patch
Turn 3: LLM requests "show me effects fixtures" → Send fixtures/effects.toon
```

### Chunked Diff Usage

When diff is split into parts (`diff_part_*.patch`):

- **First request**: Send `diff_index.md` to show available parts
- **On demand**: LLM can request specific parts (e.g., "show me diff_part_02.patch")
- **Full context**: Include all parts only if LLM explicitly requests full diff

---

## Response Format

Please respond with:
1. **Patch**: Code changes tied to acceptance checks
2. **Reasoning**: Why each change addresses the acceptance checks
3. **Verification**: How to confirm the fix works


## Domain Constraints

### Firmware Effects
- centre-origin (LEDs 79/80)
- no heap allocation in render loops
- 120+ FPS target

### Network Api
- protected file protocols
- additive JSON only
- thread safety

### Documentation
- British English (centre/colour/initialise)
- spelling consistency


---

## 3. Volatile: Diff + Fixtures

**Diff size**: 172.5 KB
**Fixtures**: 0 files

See attached files:
- `diff.patch` (or `diff_part_*.patch` if split)
- `fixtures/*.toon` / `fixtures/*.json`
- `token_report.md` (if fixtures were converted)

---

## 4. Your Ask

<!-- Replace this with your specific request -->

[Describe what you need help with]

---

*Generated by `tools/contextpack.py`*