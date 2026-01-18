# Context Pack Pipeline

**Purpose**: Generate minimal, token-efficient prompt bundles for LLM interactions.

---

## Overview

The Context Pack pipeline implements **delta-only prompting** — sending only changes, trimmed logs, and targeted fixtures to LLMs instead of full files or complete context.

### Core Principles

1. **Stable prefix**: Reusable LLM context file (`docs/LLM_CONTEXT.md`)
2. **Delta-only**: Send diffs, not whole files
3. **Trimmed logs**: Only the relevant window
4. **Targeted fixtures**: TOON for uniform arrays, CSV for flat tables, JSON for nested
5. **Acceptance checks**: Explicit pass/fail targets

---

## Delta-Only Prompting Discipline

Every prompt follows this structure:

1. "Here is the stable context file."        → `docs/LLM_CONTEXT.md`
2. "Here is the diff."                       → `diff.patch`
3. "Here are trimmed logs."                  → `logs.trimmed.txt`
4. "Here are fixtures."                      → `fixtures/*.toon`, `*.csv`, `*.json`
5. "Here are acceptance checks."             → In `packet.md`
6. "Respond with patch + reasoning."

---

## Generated Bundle Structure

```
contextpack/
├── packet.md           # Goal, symptom, acceptance checks (fill in)
├── diff.patch          # Git diff output
├── logs.trimmed.txt    # Trimmed log window (optional)
├── token_report.md     # Token savings stats (auto-generated)
└── fixtures/
    ├── README.md       # Format selection guide
    ├── *.json          # Source JSON fixtures (you add these)
    └── *.toon          # Auto-generated TOON versions
```

---

## Quick Start

### Using the Generator Script

```bash
# Basic generation
python tools/contextpack.py

# Custom output directory
python tools/contextpack.py --out /tmp/contextpack

# Include trimmed logs
python tools/contextpack.py --logs build.log --lines 100

# Custom diff command
python tools/contextpack.py --diff "git diff --cached"

# Skip TOON conversion
python tools/contextpack.py --no-toonify
```

### Using Make

```bash
cd tools

# Generate context pack with TOONified fixtures
make contextpack

# Custom output directory
make contextpack OUT_DIR=/tmp/my-pack

# Clean generated files
make contextpack-clean

# Show help
make help
```

---

## TOONify: Automatic JSON → TOON Conversion

The generator automatically converts eligible JSON fixtures to TOON format for token efficiency.

### How It Works

1. Add JSON fixtures to `contextpack/fixtures/`
2. Run `python tools/contextpack.py` (or `make contextpack`)
3. Eligible JSON files are converted to `.toon` alongside the original
4. Token savings are reported in `token_report.md`

### Eligibility Criteria

JSON is converted to TOON only when:
- Top-level is an **array** (list)
- Each element is an **object** (dict)
- Keys are **consistent** across all elements
- Array has **≥5 items** (configurable via `--toon-min-items`)

### Example Conversion

**Input** (`effects_list.json`):
```json
[
  {"id": 1, "name": "Breathing", "category": "organic", "speed": 50},
  {"id": 2, "name": "Pulse", "category": "organic", "speed": 75},
  {"id": 3, "name": "Wave", "category": "geometric", "speed": 60}
]
```

**Output** (`effects_list.toon`):
```toon
[3]{id,name,category,speed}:
  1,Breathing,organic,50
  2,Pulse,organic,75
  3,Wave,geometric,60
```

### Token Report

After conversion, `token_report.md` shows savings:

```markdown
| Source (JSON) | Output (TOON) | JSON Tokens | TOON Tokens | Savings |
|---------------|---------------|-------------|-------------|---------|
| effects_list.json | effects_list.toon | 336 | 62 | 81.5% |
```

---

## TOON CLI (Pinned)

The TOON CLI is pinned to version **2.1.0** in `tools/package.json` for reproducible builds.

### Manual Usage

```bash
# Install (one-time)
npm --prefix tools install

# Convert JSON to TOON
npm --prefix tools exec -- toon input.json -o output.toon

# Get token stats
npm --prefix tools exec -- toon input.json --stats
```

---

## Format Selection Guide

| Data Shape | Format | When to Use |
|------------|--------|-------------|
| **Large uniform arrays** | TOON | Effects lists, palettes, zones (~40-80% fewer tokens) |
| **Purely flat tabular** | CSV | Simple key-value data, logs |
| **Nested / non-uniform** | JSON | Config objects, complex structures |

**Rule**: JSON is the source of truth. TOON is a derived view for LLM input.

---

## Conversation Checkpointing

To prevent context bloat in long conversations:

1. Every N turns (or after a milestone), generate a **checkpoint summary**
2. Start a fresh thread with:
   - `docs/LLM_CONTEXT.md` (stable prefix)
   - Latest `packet.md`
   - Latest `diff.patch`
   - The checkpoint summary

---

## Prompt Caching (API Users)

If using API-based agents (Claude-Flow, scripts):

- **OpenAI Prompt Caching**: Caches repeated prefixes (≥1024 tokens)
- **Anthropic Prompt Caching**: Same concept for Claude API

Keep `docs/LLM_CONTEXT.md` stable to maximise cache hits.

---

## See Also

- [`docs/LLM_CONTEXT.md`](../LLM_CONTEXT.md) - Stable LLM prefix file
- [`docs/architecture/TOON_FORMAT_EVALUATION.md`](../architecture/TOON_FORMAT_EVALUATION.md) - TOON decision document
- [`tools/contextpack.py`](../../tools/contextpack.py) - Generator script
- [`tools/Makefile`](../../tools/Makefile) - Make targets
- [`tools/package.json`](../../tools/package.json) - TOON CLI pinned version
