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

# Use semantic chunking (groups files by directory)
python tools/contextpack.py --chunk-strategy semantic

# Minify prompt.md (remove redundant whitespace/comments)
python tools/contextpack.py --minify-prompt

# Generate hierarchical summaries (1-line, 5-line, 1-para)
python tools/contextpack.py --summaries
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

## Token-Saving Features

The generator includes three token-optimisation features:

### 1. Adaptive Chunking (Semantic Boundaries)

Group related files by directory when splitting large diffs:

```bash
python tools/contextpack.py --chunk-strategy semantic
```

**Benefits**: Related files (same directory) stay together in chunks, improving context coherence.

### 2. Token-Aware Minification

Remove redundant whitespace and comment blocks from `prompt.md`:

```bash
python tools/contextpack.py --minify-prompt
```

**Benefits**: Reduces token count in assembled prompts without affecting source diffs.

### 3. Hierarchical Summarisation Ladder

Generate three summarised views for ultra-short prompts:

```bash
python tools/contextpack.py --summaries
```

**Outputs**:
- `summary_1line.md`: One-line summary (goal + stats)
- `summary_5line.md`: Five-line summary (goal, symptom, changes, fixtures)
- `summary_1para.md`: One-paragraph summary (complete overview)

**Benefits**: Quick context injection for follow-up prompts without full packet reload.

---

## Advanced Features

The generator includes six advanced features for optimising context pack generation and managing token budgets:

### 1. Semantic Anchors (`--anchors`)

Extracts and injects key symbols (effect IDs, interfaces, changed modules) into `packet.md` without including full file contents.

**Module**: `tools/anchor_extractor.py`

**Usage**:
```bash
python tools/contextpack.py --anchors
```

**What Gets Extracted**:
- Effect registry (effect IDs and names from PatternRegistry.cpp)
- Interface methods (IEffect signatures, EffectContext fields)
- Changed modules (directories with modifications)
- Unchanged modules (omitted from diff for clarity)

**Example Output in packet.md**:
```markdown
## Semantic Anchors

### Effect Registry (IDs stable)

- 0: Fire
- 1: Ocean
- 2: Plasma
...

### Key Interfaces

- **IEffect**: init, render, cleanup
- **EffectContext**: leds, ledCount, centerPoint, palette, brightness, speed, gHue, mood

### Changed Modules

- `firmware/v2/src/effects`
```

**When to Use**:
- Effect development (need effect IDs without full PatternRegistry)
- API work (need interface signatures without full headers)
- Multi-file changes (want module overview)

**Benefits**: Provides semantic context anchors (~500-1000 tokens) instead of full files (10K+ tokens), saving 90%+ on interface/registry content.

---

### 2. Delta Ledger (`--session`)

Tracks content hashes across sessions to skip unchanged files, avoiding redundant content in follow-up prompts.

**Module**: `tools/delta_ledger.py`

**Usage**:
```bash
python tools/contextpack.py --session dev-session-001
```

**How It Works**:
- First run: Records content hash for each file in `.contextpack_ledger.json`
- Subsequent runs: Skips files with matching hashes from previous sessions
- Output: Generates `diff_delta.patch` (only changed files) instead of `diff.patch`

**Session Persistence**:
- Ledger stored in `.contextpack_ledger.json` (gitignored)
- Sessions tracked with timestamps and content hashes
- Global hash registry for cross-session lookup

**Example Output**:
```
  Processing diff...
  Using delta ledger (session: dev-session-001)
    Skipped 12 files (unchanged since session dev-session-001)
  Created: diff_delta.patch (45.2 KB, 8 files)
```

**When to Use**:
- Multi-session development (same branch, incremental changes)
- Long-running features (weeks of work on same files)
- Frequent context pack generation (daily workflows)

**Benefits**: Reduces diff size by 30-70% in subsequent runs, saving tokens on unchanged files.

---

### 3. Fragment Cache (`--cache`)

Caches generated fragments (anchors, metadata, effect registry) for instant reuse across multiple context pack generations.

**Module**: `tools/fragment_cache.py`

**Usage**:
```bash
python tools/contextpack.py --cache
```

**What Gets Cached**:
- Semantic anchors output (effect registry, interface signatures)
- Effect metadata summaries
- Palette summaries (if generated)
- Any repeated fragment generation

**Cache Behaviour**:
- **TTL**: 1 hour default (cache expires after 1 hour)
- **Invalidation**: Source file changes (checks file hashes)
- **Location**: `.contextpack_cache/` directory (gitignored)

**Example Output**:
```
  Using fragment cache (.contextpack_cache)
  Extracting semantic anchors...
    Used cached anchors (cache key: anchors_markdown)
```

**When to Use**:
- Multiple context packs in short timeframe (< 1 hour)
- Repeated anchor extraction (effect development workflows)
- Script automation (frequent generation)

**Benefits**: Reduces anchor extraction time by ~200ms per cache hit, skips redundant processing.

---

### 4. Lazy Context Loading (`--lazy`)

Defers file loading until needed, with priority-based ordering (CRITICAL → HIGH → MEDIUM → LOW).

**Module**: `tools/lazy_context.py`

**Usage**:
```bash
python tools/contextpack.py --lazy
```

**Priority Tiers**:
- **CRITICAL**: Core interfaces, headers, essential config (`IEffect.h`, `PatternRegistry.cpp`, `Config.h`)
- **HIGH**: Effect implementations, key business logic (`effects/**/*Effect.cpp`)
- **MEDIUM**: Supporting utilities, helper functions (`utils/**`, `palettes/**`)
- **LOW**: Tests, examples, documentation (`test/**`, `*.md`)

**How It Works**:
- Files loaded by priority (CRITICAL first)
- Large files (>50KB) flagged for deferred loading
- Chunked loading support (load line ranges instead of full files)

**When to Use**:
- Large codebases (many files to process)
- Selective context (only need specific file subsets)
- Token budget constraints (want to prioritize critical files)

**Benefits**: Prioritises critical files, enables selective loading, reduces initial token cost.

---

### 5. Token Budget Enforcement (`--token-budget`)

Enforces token limit by truncating low-priority files when budget exceeded.

**Module**: `tools/file_priority.py`

**Usage**:
```bash
python tools/contextpack.py --token-budget 100000
```

**How It Works**:
1. Ranks files by priority (CRITICAL → HIGH → MEDIUM → LOW)
2. Estimates tokens per file (~4 chars/token heuristic)
3. Includes files in priority order until budget reached
4. Truncates or excludes remaining files

**Priority Enforcement**:
- Always includes CRITICAL files (regardless of budget)
- HIGH → MEDIUM → LOW files included until budget exceeded
- Excluded files listed in output

**Example Output**:
```
  Token budget: 100000 tokens
  Included (CRITICAL/HIGH): 15 files (98K tokens)
  Excluded (LOW): 8 files (45K tokens) - budget exceeded
```

**When to Use**:
- Model context window limits (128K, 200K tokens)
- Large multi-file changes (want to focus on critical files)
- Cost optimization (limit token usage)

**Benefits**: Ensures context pack fits within model limits, prioritises important files, provides predictable token costs.

---

### 6. Semantic Compression (`--compress-docs`)

Compresses documentation and code files by removing verbosity while preserving essential semantic information.

**Module**: `tools/semantic_compress.py`

**Usage**:
```bash
# Light compression (strip comments, normalise whitespace)
python tools/contextpack.py --compress-docs light

# Aggressive compression (signature-only mode)
python tools/contextpack.py --compress-docs aggressive
```

**Compression Levels**:

**`light`** (default if flag specified):
- Removes single-line comments (`// ...`)
- Removes multi-line comments (`/* ... */`)
- Normalises whitespace (max 1 consecutive blank line)
- Preserves: Function/class bodies, docstrings, structure

**`aggressive`**:
- Signature-only mode (function/class signatures only)
- Preserves: Docstrings (brief ones ≤3 lines), `#include` directives, `#define` constants
- Removes: Function body implementations, detailed comments

**Token Savings**:
- **Light**: ~5-15% token reduction
- **Aggressive**: ~40-60% token reduction (signature-only mode)

**Example**:

**Original** (`light`):
```cpp
// Calculate brightness based on distance
uint8_t calculateBrightness(int distance) {
    // Map distance to 0-255 brightness range
    if (distance < 0) return 0;
    return min(255, distance * 2);
}
```

**Compressed** (`aggressive`):
```cpp
uint8_t calculateBrightness(int distance);
```

**When to Use**:
- **Light**: General token savings without losing context
- **Aggressive**: Maximum compression for large codebases, when function bodies less critical

**Benefits**: Reduces documentation verbosity, enables signature-only mode for large files, provides configurable compression levels.

---

### Combined Usage Example

Common workflow combining multiple advanced features:

```bash
# Full-featured context pack generation
python tools/contextpack.py \
  --anchors \              # Extract semantic anchors
  --cache \                # Cache fragments for reuse
  --session dev-001 \      # Track unchanged files
  --token-budget 150000 \  # Enforce token limit
  --compress-docs light    # Light compression
```

**Typical Output**:
```
Generating context pack in: contextpack/
  Using fragment cache (.contextpack_cache)
  Using delta ledger (session: dev-001)
  Processing diff...
    Skipped 8 files (unchanged since session dev-001)
  Created: diff_delta.patch (52.3 KB, 12 files)
  Extracting semantic anchors...
    Used cached anchors (cache key: anchors_markdown)
  Token budget: 150000 tokens
  Included: 20 files (148K tokens)
  Excluded: 5 files (LOW priority, 35K tokens)
```

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
