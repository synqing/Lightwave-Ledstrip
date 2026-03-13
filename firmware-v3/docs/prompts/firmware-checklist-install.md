# Firmware Review Checklist — Install Instructions

The firmware-specific review checklist has been written to:
`.claude/skills/review/checklist.md` (project-level)

## For the CC CLI agent

The gstack `/review` skill reads `checklist.md` from `.claude/skills/review/checklist.md`
relative to the project root. If gstack was installed globally only, the CC CLI agent
needs to ensure the project-level checklist is used.

### Option A: Project-level override (recommended)

The checklist is already at `.claude/skills/review/checklist.md` in the firmware repo.
If the CC CLI agent has gstack installed globally, it will still check for a project-local
checklist first (per gstack's SKILL.md: "Read `.claude/skills/review/checklist.md`").

No further action needed — the file is in place.

### Option B: Also install globally as fallback

```bash
cp .claude/skills/review/checklist.md ~/.claude/skills/gstack/review/checklist.md
```

This replaces the stock Rails checklist with the firmware version globally. Only do this
if ALL your projects are firmware — otherwise keep the project-level override approach.

## What was changed

The stock gstack checklist covers Rails/web-app concerns (SQL injection, N+1 queries,
LLM trust boundaries, XSS). The firmware checklist replaces ALL categories with:

### CRITICAL (blocks /ship):
- GPIO & Pin Contention
- FreeRTOS Concurrency & Race Conditions
- Memory Safety & DMA
- I2C / SPI Bus Safety
- Init Sequence & Power Dependencies

### INFORMATIONAL (in PR body):
- Actor Lifecycle & Architecture
- ControlBusFrame & Audio Contract
- PlatformIO Build Configuration
- Display Layout & Rendering
- Volatile & Compiler Optimisation
- Dead Code & Consistency
- Stack & Heap

The structure (two-pass, file:line citations, suppressions) is identical to gstack stock.

## Updating

Edit `.claude/skills/review/checklist.md` directly. The CC CLI agent reads it fresh on
every `/review` invocation.
