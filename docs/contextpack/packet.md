# Context Pack Packet

**Date**: YYYY-MM-DD  
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

## Response Format

Please respond with:
1. **Patch**: Code changes tied to acceptance checks
2. **Reasoning**: Why each change addresses the acceptance checks
3. **Verification**: How to confirm the fix works
