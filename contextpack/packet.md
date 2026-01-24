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
