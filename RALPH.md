# Ralph Deployment Guide

Ralph is an autonomous AI agent loop that runs AI coding tools (Claude Code or Amp) repeatedly until all PRD user stories are complete. Each iteration spawns a fresh AI instance with clean context. Memory persists via git history, `progress.txt`, and `prd.json`.

Based on [Geoffrey Huntley's Ralph pattern](https://ghuntley.com/ralph/).

---

## CRITICAL INVARIANTS

Before using Ralph on this project, understand these **absolute rules**:

| Invariant | Rule | Consequence of Violation |
|-----------|------|--------------------------|
| **CENTER ORIGIN** | All LED positions relative to center (79/80), never edges | Visual artifacts, brand violation |
| **NO RAINBOWS** | No rainbow color schemes or HSV rainbow sweeps | Brand violation |
| **NO MALLOC IN RENDER** | Zero heap allocation in render loop | Watchdog resets, crashes |

Every story's acceptance criteria MUST verify these invariants.

---

## Table of Contents

1. [Quick Start](#1-quick-start)
2. [Prerequisites & Installation](#2-prerequisites--installation)
3. [Core Concepts](#3-core-concepts)
4. [Command-Line Reference](#4-command-line-reference)
5. [PRD Schema Reference](#5-prd-schema-reference)
6. [Skills Reference](#6-skills-reference)
7. [Workflow Phases](#7-workflow-phases)
8. [progress.txt Best Practices](#8-progresstxt-best-practices)
9. [CLAUDE.md / AGENTS.md Updates](#9-claudemd--agentsmd-updates)
10. [Branch Management & Archiving](#10-branch-management--archiving)
11. [Quality Checks Configuration](#11-quality-checks-configuration)
12. [Hardware Testing](#12-hardware-testing)
13. [Troubleshooting](#13-troubleshooting)
14. [Project-Specific Configuration](#14-project-specific-configuration)

[Appendices](#appendices)

---

## 1. Quick Start

```bash
# 1. Create Ralph directory (if not exists)
mkdir -p scripts/ralph
cp /path/to/ralph/{ralph.sh,CLAUDE.md} scripts/ralph/
chmod +x scripts/ralph/ralph.sh

# 2. Create a PRD using the prd skill
# In Claude Code: "Load the prd skill and create a PRD for [feature]"

# 3. Convert PRD to JSON using the ralph skill
# In Claude Code: "Load the ralph skill and convert tasks/prd-[feature].md to prd.json"

# 4. Run Ralph
./scripts/ralph/ralph.sh --tool claude 10

# 5. Monitor progress
cat scripts/ralph/prd.json | jq '.userStories[] | {id, passes}'
```

When all stories complete, Ralph outputs `<promise>COMPLETE</promise>` and exits.

---

## 2. Prerequisites & Installation

### 2.1 Required Tools

| Tool | Installation | Purpose |
|------|--------------|---------|
| **Claude Code** | `npm install -g @anthropic-ai/claude-code` | AI coding tool |
| **jq** | `brew install jq` | JSON parsing |
| **PlatformIO** | `pip install platformio` | Build system |
| **Git** | Pre-installed on macOS | Version control |
| **pre-commit** | `pip install pre-commit` | Git hooks |

Alternative: [Amp CLI](https://ampcode.com) instead of Claude Code.

### 2.2 Project Setup

Create Ralph directory in project:

```bash
mkdir -p scripts/ralph
cp /path/to/ralph/ralph.sh scripts/ralph/
cp /path/to/ralph/CLAUDE.md scripts/ralph/
chmod +x scripts/ralph/ralph.sh
```

### 2.3 Global Skills Installation (Optional)

To use PRD and Ralph skills across all projects:

```bash
# Claude Code
cp -r /path/to/ralph/skills/prd ~/.claude/skills/
cp -r /path/to/ralph/skills/ralph ~/.claude/skills/

# Amp
cp -r /path/to/ralph/skills/prd ~/.config/amp/skills/
cp -r /path/to/ralph/skills/ralph ~/.config/amp/skills/
```

### 2.4 Auto-Handoff Configuration

Enable automatic context handoff when context fills up:

**Claude Code** (`~/.claude/settings.json`):
```json
{
  "experimental.autoHandoff": { "context": 90 }
}
```

**Amp** (`~/.config/amp/settings.json`):
```json
{
  "amp.experimental.autoHandoff": { "context": 90 }
}
```

---

## 3. Core Concepts

### 3.1 The Ralph Loop

```
+------------------------------------------------------------------+
|                         ralph.sh                                  |
|                                                                   |
|   for i in 1..MAX_ITERATIONS:                                    |
|       1. Spawn fresh AI instance (claude or amp)                 |
|       2. AI reads prd.json, progress.txt                         |
|       3. AI picks highest priority story where passes=false      |
|       4. AI implements the single story                          |
|       5. AI runs quality checks (pio run, pre-commit)            |
|       6. If checks pass: commit, set passes=true                 |
|       7. AI appends learnings to progress.txt                    |
|       8. If ALL stories pass: output <promise>COMPLETE</promise> |
|       9. ralph.sh detects COMPLETE -> exit success               |
|      10. Otherwise -> next iteration                             |
+------------------------------------------------------------------+
```

### 3.2 Fresh Context Per Iteration

**Critical**: Each iteration spawns a NEW AI instance with ZERO memory of previous iterations.

| What Persists | How It Works |
|---------------|--------------|
| Git history | Commits from previous iterations are in the repo |
| `progress.txt` | Learnings and patterns read at start of each iteration |
| `prd.json` | Story completion status (`passes: true/false`) |
| `CLAUDE.md` | Reusable knowledge auto-loaded by AI tool |

| What Does NOT Persist |
|-----------------------|
| Conversation history |
| File contents in working memory |
| Debugging sessions |
| Any state not written to files |

**Implication**: Stories must be self-contained. Never write "continue from where we left off."

### 3.3 Memory Persistence Mechanisms

**Git History**: All code changes are committed. Future iterations can `git log` and `git show` to understand what was done.

**progress.txt**: Append-only log with two key sections:
- **Codebase Patterns** (top): Consolidated reusable knowledge
- **Iteration Reports** (chronological): Per-story details and learnings

**prd.json**: Source of truth for what is done (`passes: true`) and what remains (`passes: false`).

### 3.4 Stop Condition

When ALL user stories have `passes: true`, Ralph outputs:

```
<promise>COMPLETE</promise>
```

The `ralph.sh` script detects this and exits with success (code 0).

**Manual Stop**: Press `Ctrl+C` to interrupt. Progress is preserved in git and progress.txt.

---

## 4. Command-Line Reference

```bash
./scripts/ralph/ralph.sh [--tool amp|claude] [max_iterations]
```

### Arguments

| Argument | Default | Description |
|----------|---------|-------------|
| `--tool amp` | amp | Use Amp CLI |
| `--tool claude` | - | Use Claude Code |
| `max_iterations` | 10 | Maximum loop iterations before timeout |

### Examples

```bash
# Claude Code, 20 iterations
./scripts/ralph/ralph.sh --tool claude 20

# Amp (default), 5 iterations
./scripts/ralph/ralph.sh 5

# Claude Code, default iterations (10)
./scripts/ralph/ralph.sh --tool claude
```

### Iteration Sizing Guide

**Rule of thumb:** `iterations = story_count + 2` (buffer for retries/failures)

| Story Count | Recommended Iterations | Notes |
|-------------|------------------------|-------|
| 1-3 stories | 5 | Small bugfixes, minor features |
| 4-8 stories | 8-10 | Typical feature implementation |
| 9-15 stories | 15-18 | Large features, multi-file changes |
| 15+ stories | 20-25 | Major features, consider splitting PRD |

**When to increase iterations:**
- Stories involve complex builds or tests that may fail
- Stories have dependencies on external systems
- First time running Ralph on a new codebase

**When to decrease iterations:**
- Stories are simple/mechanical (docs, config changes)
- You've already completed several stories and few remain
- Resource-constrained environment (many Claude sessions running)

**Check remaining stories:**
```bash
# Count remaining stories
cat scripts/ralph/prd.json | jq '[.userStories[] | select(.passes == false)] | length'

# List remaining story IDs
cat scripts/ralph/prd.json | jq '.userStories[] | select(.passes == false) | .id'
```

---

## 5. PRD Schema Reference

### 5.1 Complete prd.json Schema

```json
{
  "project": "string - Project name",
  "branchName": "string - Git branch (ralph/feature-name)",
  "description": "string - Feature description",
  "userStories": [
    {
      "id": "string - Unique ID (US-001, US-002, ...)",
      "title": "string - Short descriptive title",
      "description": "string - As a [user], I want [feature] so that [benefit]",
      "acceptanceCriteria": [
        "string - Verifiable criterion 1",
        "string - Verifiable criterion 2",
        "pio run -e esp32dev_audio_esv11 compiles"
      ],
      "priority": "number - Execution order (1 = first)",
      "passes": "boolean - false until completed",
      "notes": "string - Optional implementation notes"
    }
  ]
}
```

### 5.2 Story Sizing: THE #1 RULE

**Each story MUST be completable in ONE Ralph iteration (one context window).**

Ralph spawns fresh AI instances with no memory. Large stories exhaust context before completion, producing broken code.

#### Right-Sized Stories (DO)

| Example | Why It Works |
|---------|--------------|
| Add effect header file with class definition | Single file, clear interface |
| Implement render() method for one effect | Isolated implementation |
| Register effect in EffectRegistry | Small integration change |
| Add WebSocket command handler | Single endpoint |

#### Too Big (SPLIT)

| Bad Story | Split Into |
|-----------|------------|
| "Build entire audio pipeline" | Goertzel, FFT, mapping, each separate |
| "Add new effect with web UI" | Header, implementation, registry, web handler |
| "Refactor render system" | One pattern at a time |

**Rule of Thumb**: If you cannot describe the change in 2-3 sentences, it is too big.

### 5.3 Dependency Ordering

Stories execute in `priority` order (1 = first). Order MUST respect dependencies.

**Correct Order:**
1. Header files and interfaces
2. Implementation files
3. Registry/integration
4. Web handlers and UI

**Wrong Order (NEVER):**
```
1. Register effect  <- FAILS: effect class doesn't exist
2. Create effect class
```

### 5.4 Verifiable Acceptance Criteria

Each criterion must be something Ralph can CHECK.

**Good (Verifiable):**
- "Create `src/effects/PulseEffect.h` with render() declaration"
- "Effect uses CENTER ORIGIN coordinate system"
- "No malloc/new in render() implementation"
- "pio run -e esp32dev_audio_esv11 compiles"
- "Effect verified via serial menu on hardware"

**Bad (Vague):**
- "Works correctly"
- "Looks good"
- "Performs well"
- "Handles edge cases"

**Required for ALL stories:**
```
"pio run -e esp32dev_audio_esv11 compiles"
```

**Required for effect stories:**
```
"Uses CENTER ORIGIN coordinate system"
"No malloc/new in render loop"
"No rainbow colors"
```

---

## 6. Skills Reference

### 6.1 PRD Generator Skill

**Trigger phrases:** "create a prd", "write prd for", "plan this feature"

**Usage:**
```
Load the prd skill and create a PRD for [feature description]
```

**What it does:**
1. Asks 3-5 clarifying questions (lettered options)
2. Generates structured PRD with goals, stories, requirements
3. Saves to `tasks/prd-[feature-name].md`

### 6.2 Ralph JSON Converter Skill

**Trigger phrases:** "convert this prd", "create prd.json", "ralph json"

**Usage:**
```
Load the ralph skill and convert tasks/prd-[feature].md to prd.json
```

**What it does:**
1. Parses markdown PRD
2. Splits large features into right-sized stories
3. Orders by dependencies
4. Adds required acceptance criteria
5. Outputs `scripts/ralph/prd.json`

---

## 7. Workflow Phases

### Phase 1: PRD Creation

1. Describe feature to PRD skill
2. Answer 3-5 clarifying questions
3. Review generated PRD at `tasks/prd-[feature].md`
4. Verify effect stories include invariant checks

### Phase 2: JSON Conversion

1. Run Ralph converter skill
2. Review story sizing (split if too large)
3. Verify dependency order
4. Ensure all stories include PlatformIO build check
5. Add hardware verification for effect stories

### Phase 3: Execution Loop

```bash
./scripts/ralph/ralph.sh --tool claude 10
```

**Monitor during execution:**
```bash
# Watch story progress
watch -n 5 'cat scripts/ralph/prd.json | jq ".userStories[] | {id, passes}"'

# Check git commits
git log --oneline -5

# Read learnings
cat scripts/ralph/progress.txt

# Monitor build output
pio run -e esp32dev_audio_esv11 2>&1 | tail -20
```

### Phase 4: Completion & Archive

When Ralph outputs `<promise>COMPLETE</promise>`:
- All stories have `passes: true`
- Code compiles for all environments
- Feature is ready for hardware testing

Next feature run will auto-archive current prd.json and progress.txt.

---

## 8. progress.txt Best Practices

### 8.1 File Structure

```markdown
## Codebase Patterns
- Pattern 1: Use X for Y
- Pattern 2: Always do Z before W
---

## [Date/Time] - US-001
- What was implemented
- Files changed
- **Learnings for future iterations:**
  - Patterns discovered
  - Gotchas encountered
---

## [Date/Time] - US-002
...
```

### 8.2 Codebase Patterns Section

The TOP of progress.txt should contain consolidated, **reusable** patterns.

**Good patterns for this project:**
- "All effects must inherit from BaseEffect"
- "Use static arrays in render(), never malloc"
- "LED coordinates are center-origin (79/80 is center)"
- "Register effects in EffectRegistry::init()"

**Do NOT add:**
- Story-specific implementation details
- Temporary debugging notes

### 8.3 Iteration Report Format

Each iteration APPENDS (never replaces):

```markdown
## 2025-01-20 14:30 - US-003
- Implemented PulseEffect render() method
- Files: src/effects/PulseEffect.cpp, src/effects/PulseEffect.h
- **Learnings:**
  - Goertzel bass detection available at m_audio.bassEnergy()
  - Use map8() for brightness scaling (not map())
  - CRGB::blend() handles smooth transitions
---
```

---

## 9. CLAUDE.md / AGENTS.md Updates

### 9.1 When to Update

Update when you discover knowledge that:
- Applies to a specific directory/module
- Would help future developers/agents
- Is NOT already in progress.txt patterns section

### 9.2 What to Add

**Good additions:**
- "BaseEffect::render() is called at 120 FPS"
- "Audio data refreshes at 62.5 Hz (every ~16ms)"
- "Use m_audio.bassEnergy() for bass-reactive effects"
- "Palette index 0-15 maps to FastLED palette entries"

**Do NOT add:**
- Story-specific implementation details
- Temporary debugging notes
- Information already in progress.txt

### 9.3 Directory-Scoped Knowledge

Place learnings in the CLAUDE.md closest to affected code:

```
firmware/v2/
  CLAUDE.md             <- Firmware-wide patterns
  src/
    effects/
      CLAUDE.md         <- Effect implementation patterns
    audio/
      CLAUDE.md         <- Audio processing patterns
    network/
      CLAUDE.md         <- WebServer patterns
```

---

## 10. Branch Management & Archiving

### 10.1 Branch Naming Convention

```
ralph/[feature-name-kebab-case]
```

**Examples:**
- `ralph/audio-reactive-pulse`
- `ralph/webserver-zone-api`
- `ralph/palette-system-v2`

### 10.2 Automatic Archive Trigger

When `ralph.sh` runs and detects a different `branchName` than the previous run:

1. Creates archive folder: `archive/YYYY-MM-DD-[feature-name]/`
2. Copies current `prd.json` and `progress.txt`
3. Resets `progress.txt` with fresh header
4. Updates `.last-branch` tracking file

### 10.3 Archive Structure

```
scripts/ralph/
  archive/
    2025-01-10-audio-reactive-pulse/
      prd.json
      progress.txt
    2025-01-15-zone-composer/
      prd.json
      progress.txt
```

---

## 11. Quality Checks Configuration

### 11.1 PlatformIO Build Environments

| Environment | Purpose | Command |
|-------------|---------|---------|
| `esp32dev_audio_esv11` | Audio-enabled build (default) | `pio run -e esp32dev_audio_esv11` |
| `esp32dev_audio_esv11` | WiFi + WebServer | `pio run -e esp32dev_audio_esv11` |
| `memory_debug` | Heap tracing | `pio run -e memory_debug` |

### 11.2 Required Quality Checks

**All stories must pass:**

```bash
# Primary build (audio)
cd firmware/v2
pio run -e esp32dev_audio_esv11

# WiFi build (if story touches web)
pio run -e esp32dev_audio_esv11

# Pre-commit hooks
pre-commit run --all-files
```

### 11.3 Add to CLAUDE.md (scripts/ralph/CLAUDE.md)

Ensure the Ralph agent instructions include:

```markdown
## Quality Requirements

Run these checks before committing:

### PlatformIO Builds
- `cd firmware/v2 && pio run -e esp32dev_audio_esv11` - Must compile
- `pio run -e esp32dev_audio_esv11` - Must compile (if touching web code)

### Pre-commit Hooks
- `pre-commit run --all-files` - Formatting and validation

### Invariant Checks
- CENTER ORIGIN: All LED coordinates relative to center (79/80)
- NO RAINBOWS: No HSV rainbow sweeps or rainbow palettes
- NO MALLOC IN RENDER: Zero heap allocation in render()

ALL checks must pass. Do NOT commit code that violates invariants.
```

### 11.4 GitHub Actions Integration

This project has 18 CI workflows. Ensure stories don't break CI:

```bash
# Local validation of key workflows
python tools/contextpack.py --lint
bash tools/check_json_boundary.sh
```

---

## 12. Hardware Testing

### 12.1 When Required

Any story that modifies effects MUST include hardware verification.

### 12.2 Serial Menu Testing

```bash
# Build and flash
cd firmware/v2
pio run -e esp32dev_audio_esv11 -t upload --upload-port /dev/cu.usbmodem1101

# Open serial monitor
pio device monitor -p /dev/cu.usbmodem1101 -b 115200
```

### 12.3 Serial Menu Commands

| Key | Action |
|-----|--------|
| `SPACE` | Next effect |
| `n/N` | Next/prev effect |
| `,/.` | Prev/next palette |
| `P` | List palettes |
| `s` | Print FPS/CPU status |
| `l` | List effects |
| `z` | Toggle zone mode |
| `h` | Help menu |

### 12.4 Effect Validation Command

```
validate <effect_id>
```

Runs automatic checks:
- Centre-origin coordinate verification
- Hue-span analysis (rainbow detection)
- FPS measurement
- Heap delta check

### 12.5 Acceptance Criteria for Effects

```json
{
  "acceptanceCriteria": [
    "Effect inherits from BaseEffect",
    "Uses CENTER ORIGIN coordinate system",
    "No malloc/new in render() method",
    "No rainbow colors or HSV sweeps",
    "pio run -e esp32dev_audio_esv11 compiles",
    "Effect verified via serial menu on hardware"
  ]
}
```

---

## 13. Troubleshooting

### 13.1 Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Story never completes | Story too large | Split into smaller stories |
| Build fails | Missing include or syntax error | Check compiler output |
| Watchdog reset | Malloc in render loop | Remove dynamic allocation |
| Visual artifacts | Edge-origin coordinates | Convert to center-origin |
| Wrong branch | Branch mismatch | Delete `.last-branch`, restart |
| Flash too large | Too many features enabled | Check feature flags |

### 13.2 Debugging Commands

```bash
# Check story status
cat scripts/ralph/prd.json | jq '.userStories[] | {id, title, passes}'

# View learnings
cat scripts/ralph/progress.txt

# Recent commits
git log --oneline -10

# Current branch
git branch --show-current

# Build with verbose output
pio run -e esp32dev_audio_esv11 -v

# Check flash/RAM usage
pio run -e esp32dev_audio_esv11 | grep -E "Flash|RAM"

# Serial monitor (115200 baud)
pio device monitor -b 115200
```

### 13.3 Recovery Procedures

#### Story Stuck in Loop
1. Check progress.txt for repeated errors
2. Manually mark story as `passes: true` if mostly complete
3. Create follow-up story for remaining work

#### Corrupted prd.json
1. Restore from git: `git checkout scripts/ralph/prd.json`
2. Or restore from archive folder

#### Context Exhaustion Mid-Story
1. Interrupt with Ctrl+C
2. Check if partial work was committed
3. Split remaining work into new story
4. Update prd.json manually if needed

#### Build Fails After Partial Implementation
1. Check compiler errors carefully
2. Look for missing includes or forward declarations
3. Ensure all files are properly saved
4. Try clean build: `pio run -t clean && pio run -e esp32dev_audio_esv11`

---

## 14. Project-Specific Configuration

### 14.1 Lightwave-Ledstrip Overview

| Component | Description |
|-----------|-------------|
| **Hardware** | ESP32-S3, 320 WS2812 LEDs (2x160 strips) |
| **Framework** | Arduino + FastLED + ESPAsyncWebServer |
| **Build System** | PlatformIO |
| **Architecture** | Actor-based (Audio, Renderer, ShowDirector) |

### 14.2 Critical Invariants (ENFORCED)

#### CENTER ORIGIN ONLY

All LED positions must be relative to center (LEDs 79/80).

**Valid patterns:**
```cpp
// Expand from center
for (int i = 0; i <= 79; i++) {
    leds[79 - i] = color;  // Left from center
    leds[80 + i] = color;  // Right from center
}
```

**Invalid patterns:**
```cpp
// WRONG: Linear sweep from edge
for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = color;
}
```

#### NO RAINBOWS

No rainbow color schemes or HSV rainbow sweeps.

**Forbidden:**
```cpp
fill_rainbow(leds, NUM_LEDS, hue, 7);  // NEVER
CHSV(hue++, 255, 255);  // Rainbow cycling - NEVER
```

**Allowed:**
```cpp
ColorFromPalette(currentPalette, index);  // Single palette
CRGB::blend(color1, color2, amount);      // Two-color blend
```

#### NO MALLOC IN RENDER

Zero heap allocation in render loop.

**Forbidden:**
```cpp
void render() {
    uint8_t* buffer = new uint8_t[size];  // NEVER
    std::vector<int> values;              // NEVER
    String str = "hello";                 // NEVER (implicit alloc)
}
```

**Required:**
```cpp
class MyEffect : public BaseEffect {
    uint8_t m_buffer[MAX_SIZE];  // Static member allocation

    void render() {
        // Use m_buffer, no new allocations
    }
};
```

### 14.3 Protected Files

These files require HIGH CAUTION when modifying:

| File | Risk | Reason |
|------|------|--------|
| `src/core/actors/RendererActor.*` | CRITICAL | Render loop timing |
| `src/effects/BaseEffect.h` | HIGH | All effects inherit |
| `src/audio/AudioActor.*` | HIGH | Audio pipeline |
| `platformio.ini` | HIGH | Build configuration |
| `src/network/WiFiManager.*` | MEDIUM | FreeRTOS state bug |

### 14.4 Example prd.json

```json
{
  "project": "Lightwave Ledstrip",
  "branchName": "ralph/audio-reactive-pulse",
  "description": "Add audio-reactive pulse effect using Goertzel bass detection",
  "userStories": [
    {
      "id": "US-001",
      "title": "Create PulseEffect header file",
      "description": "As a developer, I need the effect class definition.",
      "acceptanceCriteria": [
        "Create src/effects/PulseEffect.h",
        "Class inherits from BaseEffect",
        "Declare render() method",
        "Declare private members for pulse state",
        "pio run -e esp32dev_audio_esv11 compiles"
      ],
      "priority": 1,
      "passes": false,
      "notes": ""
    },
    {
      "id": "US-002",
      "title": "Implement PulseEffect render method",
      "description": "As a user, I want to see audio-reactive pulses from center.",
      "acceptanceCriteria": [
        "Implement render() in PulseEffect.cpp",
        "Pulse expands FROM CENTER (79/80) outward",
        "Intensity based on m_audio.bassEnergy()",
        "NO malloc/new in render()",
        "NO rainbow colors - single hue with brightness variation",
        "pio run -e esp32dev_audio_esv11 compiles"
      ],
      "priority": 2,
      "passes": false,
      "notes": ""
    },
    {
      "id": "US-003",
      "title": "Register PulseEffect in EffectRegistry",
      "description": "As a user, I want to select the effect from serial menu.",
      "acceptanceCriteria": [
        "Add PulseEffect to EffectRegistry::init()",
        "Effect appears in serial menu under 'Audio Effects'",
        "pio run -e esp32dev_audio_esv11 compiles",
        "Effect verified via serial menu on hardware"
      ],
      "priority": 3,
      "passes": false,
      "notes": ""
    },
    {
      "id": "US-004",
      "title": "Add PulseEffect to WiFi WebSocket API",
      "description": "As a web user, I want to select PulseEffect from dashboard.",
      "acceptanceCriteria": [
        "Effect ID exposed in /api/v1/effects endpoint",
        "WebSocket setEffect command accepts PulseEffect",
        "pio run -e esp32dev_audio_esv11 compiles",
        "Effect selectable from web dashboard"
      ],
      "priority": 4,
      "passes": false,
      "notes": ""
    }
  ]
}
```

### 14.5 Performance Constraints

| Metric | Target | Current |
|--------|--------|---------|
| FPS | 120 | 176 |
| Frame budget | 8.3ms | 5.68ms |
| Effect calc | < 2ms | varies |
| FastLED.show() | ~2.5ms | fixed |
| Available RAM | 280KB | varies |

Monitor with serial command: `s` (status)

---

## Appendices

### Appendix A: Complete prd.json Schema

```typescript
interface PRD {
  /** Project name */
  project: string;

  /** Git branch name, format: ralph/feature-name */
  branchName: string;

  /** Feature description */
  description: string;

  /** Array of user stories */
  userStories: UserStory[];
}

interface UserStory {
  /** Unique identifier (US-001, US-002, ...) */
  id: string;

  /** Short descriptive title */
  title: string;

  /** Full description in "As a [user]..." format */
  description: string;

  /** Array of verifiable acceptance criteria */
  acceptanceCriteria: string[];

  /** Execution priority (1 = highest) */
  priority: number;

  /** Completion status (false until done) */
  passes: boolean;

  /** Optional implementation notes */
  notes: string;
}
```

### Appendix B: progress.txt Template

```markdown
## Codebase Patterns
<!-- Add reusable patterns discovered during iterations -->
<!-- Example: All effects must use CENTER ORIGIN (79/80) -->
<!-- Example: Use m_audio.bassEnergy() for bass-reactive effects -->
---

# Ralph Progress Log
Started: [DATE]
Branch: ralph/[feature-name]
---

<!-- Iteration reports will be appended below -->
```

### Appendix C: Checklist Before Running Ralph

- [ ] prd.json exists at `scripts/ralph/prd.json`
- [ ] All stories are right-sized (completable in one iteration)
- [ ] Stories ordered by dependency (header -> impl -> registry -> web)
- [ ] Every story has "pio run compiles" criterion
- [ ] Effect stories verify CENTER ORIGIN, NO RAINBOWS, NO MALLOC
- [ ] Hardware verification included for effect stories
- [ ] branchName follows convention: `ralph/feature-name`
- [ ] Previous run archived (if switching features)

### Appendix D: Serial Menu Quick Reference

```
SPACE       Next effect
n/N         Next/prev effect
,/.         Prev/next palette
P           List palettes
s           Print FPS/CPU/heap status
l           List all effects
z           Toggle zone mode
Z           Print zone status
h           Help menu
validate X  Validate effect X (centre-origin, hue-span, FPS, heap)
dbg mem     Memory debug info
dbg audio   Audio debug info
```
