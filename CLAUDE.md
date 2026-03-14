# LightwaveOS

## Context Management

This CLAUDE.md is loaded into every conversation. Keep main context for decisions and outcomes only.

**Core rule:** Before your first file read or tool call, classify the task:
- **Single-target** (edit 1 file, answer about 1 function) → stay in main context.
- **Multi-target** (trace a call chain, audit a subsystem, compare backends) → delegate. No exceptions.

This codebase has 802 source files (5.84 MB) — unmanaged exploration destroys working memory.

**Delegate by default — these subsystems are NEVER single-target:**
- Effect audits (349 files) → subagent
- Audio chain investigation (65 files across actors, adapters, contracts) → subagent
- Network/API exploration (130 files) → subagent
- Crash investigation, cross-backend comparison → subagent
- Any doc search spanning 3+ documents → subagent

**Multi-subsystem tasks MUST be split into parallel subagents.** If a task touches 2+ of the above subsystems, spawn one subagent per subsystem. Do NOT investigate sequentially in main context.

**Subagent token budget:** Each subagent MUST target completion in under 30K tokens. If a subagent's scope requires more than 30K tokens, split it further into smaller subagents rather than letting a single subagent sprawl. A 3-subagent investigation that burns 240K tokens total has FAILED — the delegation saved nothing if each agent explored wastefully. Prefer narrow, focused subagent scopes: one struct audit, one call-chain trace, one doc lookup — not "readers + blast radius" as a single scope.

**Stay in main context:** direct edits, single-file reads, iterative design work, synthesis of subagent returns.

For detailed subsystem costs, delegation scenarios, and context traps: [CONTEXT_GUIDE.md](firmware-v3/docs/CONTEXT_GUIDE.md)

## Tool Enforcement (HARD RULES — NOT SUGGESTIONS)

**Full routing table:** [docs/WORKFLOW_ROUTING.md](docs/WORKFLOW_ROUTING.md) — 29 skills, 13 MCP server groups, 47 tools. Read it on first session.

### Session Start (MANDATORY — every session, no exceptions)

Before your first action, answer this question honestly:

> **Do I have ALL the context I need to complete this task correctly — architecture, prior decisions, state machine behaviour, recent session history, and hard constraints — or am I about to guess?**

If the answer is anything other than an unqualified YES, use the tools below to fill the gaps BEFORE writing code or making changes. Proceeding without sufficient context wastes tokens, introduces bugs, and forces rework. The cost of one tool call is negligible. The cost of a confident mistake is an entire session.

**Context tools available:**
- `mcp__plugin_claude-mem_mem-search__search("[topic]")` — prior session decisions, bugfixes, and outcomes
- `mcp__plugin_claude-mem_mem-search__timeline()` — chronological session history
- `mcp__auggie__codebase-retrieval("[query]")` — semantic codebase search
- Reference files (see table below) — pre-extracted architecture, dependencies, FSMs

**You are not expected to know everything from memory.** You ARE expected to know what you don't know and to look it up before acting.

### Agent Readback Protocol (MANDATORY — every session, every agent)

Before your FIRST tool call, output a READBACK block. **No readback = no work.** If you skip this, the orchestrator or user will halt your session.

```
READBACK:
- Confidence: [do I have sufficient context? YES / NO — if NO, what am I missing and what will I look up?]
- Task scope: [single-target | multi-target → delegating]
- Subsystem: [audio | effects | network | core | ios | tab5 | cross-cutting]
- Reference files: [list which docs/reference/ files I will read first]
- Hard constraints: [list applicable constraints — see table below]
- Tool routing: [which tools I will use FIRST — clangd/QMD/Context7/subagent]
```

**Constraint readback table — read back ALL that apply to your task:**

| If task touches... | Read back these constraints |
|---|---|
| Any C++ source | clangd FIRST for symbols, grep for text only |
| Audio / effects | Centre origin 79/80 outward, no heap in render(), 2.0ms ceiling, no rainbows |
| Network / WiFi | K1 is AP-ONLY — NEVER enable STA mode |
| Multi-file exploration | Delegate to subagent, 30K token budget per agent |
| Documentation | QMD FIRST, Read as fallback |
| External library APIs | Context7 FIRST, not training data |
| Any code changes | British English in comments/docs/logs/UI |

**Reference files — read BEFORE exploring source code:**

| Project | Codebase Map | FSM Reference |
|---|---|---|
| firmware-v3 | `firmware-v3/docs/reference/codebase-map.md` | `firmware-v3/docs/reference/fsm-reference.md` |
| lightwave-ios | `lightwave-ios-v2/docs/reference/codebase-map.md` | `lightwave-ios-v2/docs/reference/fsm-reference.md` |
| tab5-encoder | `tab5-encoder/docs/reference/codebase-map.md` | `tab5-encoder/docs/reference/fsm-reference.md` |

These files contain pre-extracted codebase structure, frameworks, dependencies, entrypoints, and all state machine definitions. Reading them costs ~200 lines. Re-discovering the same information by exploring source files costs 20,000+ tokens. **Read the reference files.**

### C++ Symbol Navigation — clangd FIRST, grep NEVER (for symbols)

**Gate rule:** When you need to find a C++ function definition, caller, reference, or type — you MUST call clangd. Do NOT grep/glob for C++ symbol names. If clangd fails, STOP and report per the Tool Failure Protocol. Do NOT silently fall back to grep.

| I need to find... | Call this | NOT this |
|---|---|---|
| Where a function is defined | `mcp__clangd__find_definition` | ~~grep "void MyFunc"~~ |
| Who calls a function | `mcp__clangd__get_call_hierarchy` | ~~grep "MyFunc("~~ |
| All references to a symbol | `mcp__clangd__find_references` | ~~grep "MySymbol"~~ |
| All symbols in a file | `mcp__clangd__get_document_symbols` | ~~reading the whole file~~ |
| A symbol by name (fuzzy) | `mcp__clangd__workspace_symbol_search` | ~~glob + grep~~ |
| Type/class hierarchy | `mcp__clangd__get_type_hierarchy` | ~~grep "class.*:"~~ |
| Virtual method overrides | `mcp__clangd__find_implementations` | ~~manual search~~ |
| Compiler errors without building | `mcp__clangd__get_diagnostics` | ~~pio run just to see errors~~ |
| Type info / docs for a symbol | `mcp__clangd__get_hover` | ~~reading header files~~ |

**Why:** clangd resolves against 510 indexed compilation units (331 src/ files). One call returns the exact answer. grep returns noise, partial matches, and burns tokens scanning 802 files.

**Prerequisite:** `compile_commands.json` must exist in `firmware-v3/`. If missing: `pio run -e esp32dev_audio_esv11_k1v2_32khz --target compiledb`

**When grep IS appropriate:** searching for string literals, log messages, comments, config values, or non-C++ files. grep is for TEXT. clangd is for CODE SYMBOLS.

### Documentation Search — QMD FIRST, file reading LAST

**Gate rule:** When you need to find information in project documentation, query QMD before reading files. QMD searches across all indexed collections (firmware docs, War Room, landing page docs) with semantic matching.

| I need to... | Call this | NOT this |
|---|---|---|
| Find docs about a topic | `mcp__qmd__qmd_search` or `mcp__qmd__qmd_vector_search` | ~~reading files one by one~~ |
| Deep multi-step doc retrieval | `mcp__qmd__qmd_deep_search` | ~~loading 200+ line docs into context~~ |
| Get a specific known doc | `mcp__qmd__qmd_get` | ~~Read tool (acceptable fallback)~~ |
| Check what's indexed | `mcp__qmd__qmd_status` | ~~guessing~~ |

**Why:** 1,459 markdown files in this project. Reading them sequentially destroys context. QMD returns the relevant chunks without loading entire documents.

**Fallback:** If QMD returns nothing or `qmd_status` shows zero collections, STOP and report: `[TOOL FAIL: QMD — not indexed]`. Ask the user whether to index it now or fall back to grep/Read. Do NOT silently switch.

### Library APIs — Context7, not training data

**Gate rule:** When referencing external library APIs (FastLED, ArduinoJSON, ESPAsyncWebServer, FreeRTOS, etc.), DSP formulae (FFT windowing, spectral centroid, onset detection, beat tracking), signal processing algorithms, or any domain-specific computation where parameter correctness matters — query Context7 for authoritative docs. Do NOT rely on training data. APIs drift between versions, DSP formulae have implementation-specific variants, and hallucinated parameters cause real bugs.

| I need to... | Call this | NOT this |
|---|---|---|
| Check a FastLED/ArduinoJSON/FreeRTOS API | `mcp__Context7__resolve-library-id` then `mcp__Context7__get-library-docs` | ~~reciting from training data~~ |
| Verify function signatures for an ESP-IDF call | `mcp__Context7__get-library-docs` with topic filter | ~~assuming parameter order~~ |
| Look up a PlatformIO library method | Context7 first, then `mcp__clangd__get_hover` on the call site | ~~reading .pio/libdeps headers~~ |

**When training data IS acceptable:** Standard C/C++ library calls (`memcpy`, `printf`, `std::vector`), basic FreeRTOS primitives (`xTaskCreate`, `xQueueSend`) that have been stable for 10+ years. If in doubt, check Context7 — it costs one tool call.

### Development Lifecycle

Before writing implementation code, follow this sequence: `/brainstorming` → `/software-architecture` → `/test-driven-development` → THEN implement.

**Scope:** This sequence applies to **new features, new effects, new API endpoints, and architectural changes**. It does NOT apply to:
- Bug fixes where the root cause is already identified
- One-line or few-line changes (< 20 LOC across all files)
- Documentation-only changes
- Config/build file edits

For scoped work, start at the appropriate stage. A bug fix with known cause starts at `/test-driven-development` (write the failing test, then fix). A small feature with obvious architecture starts at `/test-driven-development`.

**Non-negotiable:** No production code without a failing test first, regardless of scope.

### Parallel Execution

**3+ independent tasks?** `/dispatching-parallel-agents` is MANDATORY. Not optional.

**Independence test:** Two tasks are independent if task B does not need the output of task A to begin. If unsure, they are dependent — run sequentially.

See [docs/WORKFLOW_ROUTING.md](docs/WORKFLOW_ROUTING.md) for the full skill catalogue (29 skills in `.claude/skills/`).

### Tool Failure Protocol (applies to ALL tools — HARD RULE)

When any MCP tool or required tool call fails (timeout, error, empty result, misconfiguration):

1. **STOP.** Do not silently fall back. Do not continue as if nothing happened.
2. **REPORT immediately** to the user: `[TOOL FAIL: tool_name — error summary — what broke]`
3. **ASK the user:** "Should I (a) attempt to fix the tool, (b) use [specific fallback method], or (c) abort this task?"
4. **Do NOT proceed** until the user responds. A broken tool is a broken workflow — it will stay broken for every future session until someone fixes it.
5. If the user authorises a fallback, mark ALL output derived from it as `[FALLBACK: reason]` so confidence is explicit.
6. If the tool failure is fixable (missing config, missing index, wrong path), **offer to fix it** rather than working around it.

**Why this matters:** An agent that silently falls back from clangd to grep has just downgraded from precise single-call resolution to noisy multi-file scanning that burns 10x the tokens. Every silent fallback is a compounding cost. Surface it, fix it, or get explicit permission to work around it.

### Pre-Commit Confidence Gate

Before running `git add` or `git commit`, answer honestly:

> **Am I confident this change is correct, tested, and does not violate any hard constraint — or am I hoping it works?**

If you cannot answer YES with specific evidence, do NOT commit. Instead:

1. **Check constraints** — re-read the Hard Constraints section below. Does your change touch render()? Verify no heap alloc. Does it affect timing? Measure against 2.0ms ceiling. Does it touch WiFi? Confirm AP-only preserved.
2. **Check tests** — did you run the relevant tests? Do they pass? If no tests exist for this change, write one first.
3. **Check scope** — are you committing only the files you intended? No accidental inclusions?
4. **Check British English** — comments, logs, UI strings all use centre/colour/initialise/behaviour.

Only after satisfying all four checks: proceed with commit.

**Why:** A bad commit in this codebase has cascading cost. BeatTracker parameter corruption (Feb 2026) regressed 17/17 synthetic tests to 15/17 and cost hours of diagnosis. A 30-second self-review before commit would have caught it.

## Hard Constraints

- **K1 is AP-ONLY. NEVER enable STA mode.** The K1 device runs as a WiFi Access Point. Tab5 and iOS connect TO it. Do NOT attempt to connect K1 to external WiFi routers — STA authentication fails at the 802.11 driver level (AUTH_EXPIRE reason 2, AUTH_FAIL reason 202) and has NEVER been resolved despite 6+ mitigation attempts. This was architecturally resolved in Feb 2026. See `WiFiManager.h` for details. **Do not modify WiFi mode, add STA connection logic, or change AP configuration without explicit user approval.**
- **Centre origin**: All effects originate from LED 79/80 outward (or inward to 79/80). No linear sweeps. This applies to all render modes including zone-specific renders. The only exception is zone ID `0xFF` (global render) where the physical centre is still 79/80.
- **No rainbows**: No rainbow cycling or full hue-wheel sweeps.
- **No heap alloc in render**: No `new`/`malloc`/`String` in `render()` or any function transitively called from `render()`. Use static buffers. This includes helper functions, utility calls, and String concatenation.
- **120 FPS target**: Per-frame effect code MUST complete in under 2.0 ms. Not "approximately" — 2.0 ms is the hard ceiling. Measure with `esp_timer_get_time()` if in doubt.
- **British English** in all comments, docs, logs, and UI strings: centre, colour, initialise, serialise, behaviour, etc.

## Workspace Rules (ENFORCED)

These rules supplement the global workspace hygiene policy. Violations will be flagged and reverted.

### Root Allowlist

Only these entries are permitted at the project root (enforced by CI — see `.github/workflows/repo_hygiene_check.yml`):

**Root files (12):**
- `README.md`, `LICENSE`, `NOTICE`, `CHANGELOG.md`, `CONTRIBUTING.md`, `TRADEMARK.md`
- `CLAUDE.md`, `AGENTS.md`, `BACKLOG.md`
- `.gitignore`, `.pre-commit-config.yaml`, `.mcp.json`, `.worktreeinclude`

**Root directories (10+3 hidden):** `.git`, `.github`, `.claude`, `.codex`, `_archive`, `docs`, `firmware-v3`, `harness`, `instructions`, `k1-composer`, `lightwave-dashboard`, `lightwave-ios-v2`, `scripts`, `tab5-encoder`, `tools`

**Nothing else goes at root.**

- Governance docs → `instructions/`
- Decision registers → `k1-launch-research/`
- Research, brand, media, launch planning → `~/SpectraSynq_K1_Launch_Planning/` (separate repo)
- Temporary working notes → `.claude/`

**Any file not on this list MUST be placed in a subdirectory.** No exceptions. No "keep at root while active." If you need a new root file, get explicit Captain approval first.

### Directory Map

| Directory | Purpose | Examples |
|-----------|---------|---------|
| `firmware-v3/` | ESP32-S3 firmware (LightwaveOS) | Source, docs, tests, configs |
| `lightwave-ios-v2/` | iOS companion app | Swift/SwiftUI source |
| `tab5-encoder/` | M5Stack Tab5 controller | PlatformIO project |
| `harness/` | Feasibility test harnesses | Voice recognition, hardware probes |
| `k1-composer/` | Web compositor/debug tool | HTML/JS/CSS |
| `lightwave-dashboard/` | Web dashboard app | TypeScript/React |
| `docs/` | Technical documentation | Workflow routing, toolchain guides |
| `tools/` | Evaluation and capture tools | EdgeMixer eval, LED capture |
| `scripts/` | Setup and validation scripts | QMD setup, toolchain validation |
| `instructions/` | Governance instructions | Naming policy, repo governance, GOV specs |
| `_archive/` | Historical artifacts | Superseded research, old trackers, vendor SDKs |

**NOT in this repo** (product/launch materials live elsewhere):
| Location | Purpose |
|----------|---------|
| `~/SpectraSynq_K1_Launch_Planning/` | K1 product launch: research, brand, media, governance, planning |
| `~/SpectraSynq.LandingPage/` | Next.js + R3F landing page |

### Changelog Maintenance

This project uses CHANGELOG.md (Keep a Changelog format). When making changes:
- Add entries under `## [Unreleased]` with category: Added, Changed, Fixed, Removed
- One line per change, present tense
- Include the subsystem prefix: `firmware:`, `ios:`, `tab5:`, `tools:`, `docs:`, `brand:`

### No Orphan Files

- **Research outputs** go in `research/` or `k1-launch-research/`, not root
- **Agent prompts** go in `brand/` or `docs/agents/`, not root
- **Media files** go in `media/`, not root
- **Decision HTMLs** go in `brand/decisions/`, not root
- **Temporary working notes** go in `.claude/` or `_scratch/`, not root
- **Session handoffs** go in `.claude/`, not root
- If you produce an output that doesn't fit any directory, create an appropriate one. Do NOT use the root as a dumping ground.

## Architecture

ESP32-S3 LED controller for a dual-strip Light Guide Plate. 320 WS2812 LEDs (2x160), 100+ effects, audio-reactive, web-controlled.

**Two audio backends** (conditional compilation, only one active per build):
- `esp32dev_audio_esv11` — **ACTIVE / PRODUCTION** (64-bin Goertzel, stable audio processing)
- `esp32dev_audio_pipelinecore` — **BROKEN / DO NOT USE.** Beat tracking non-functional after Goertzel→FFT migration. Produces unreliable audio data. Do NOT build, flash, or test with PipelineCore unless the user explicitly requests it.

**Actor model** (FreeRTOS tasks): AudioActor (Core 0) | RendererActor (Core 1) | ShowDirectorActor | CommandActor | PluginManagerActor

**Data flow:** Microphone → I2S DMA → AudioActor → PipelineCore/ESV11 → PipelineAdapter → ControlBus → RendererActor → Effects → FastLED → RMT → WS2812 LEDs

**Key abstractions:**
- `ControlBus` — shared audio state. For the full field inventory, use `mcp__clangd__get_document_symbols` on `src/audio/contracts/ControlBus.h`. Key fields include: `bands[0..7]` (octave energy), `chroma[0..11]` (pitch class), `rms`, `beat`, `onset`, `bins256[]`, tempo fields, and percussion triggers. Do NOT assume this list is exhaustive — always check the source via clangd.
- `RenderContext` — per-frame: `leds[]`, `dt`, `zoneId`, `controlBus`. Zone ID `0xFF` = global render.
- Effects inherit `EffectBase`, implement `render(RenderContext&)`. All zone-indexed access must bounds-check: `(ctx.zoneId < kMaxZones) ? ctx.zoneId : 0`

## Build (PlatformIO)

```bash
cd firmware-v3

# Production (ESV11) — use this for all builds
pio run -e esp32dev_audio_esv11_k1v2_32khz    # K1 v2 hardware
pio run -e esp32dev_audio_esv11_32khz          # V1 hardware
pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload

# DO NOT USE PipelineCore — broken audio processing, unreliable data
# pio run -e esp32dev_audio_pipelinecore

# Serial monitor
pio device monitor -b 115200
```

## Build (iOS)

```bash
cd lightwave-ios-v2

# Generate Xcode project (required after adding/removing files)
xcodegen generate

# Build
xcodebuild build -scheme LightwaveOS -destination 'platform=iOS Simulator,name=iPhone 16'

# Run tests
xcodebuild test -scheme LightwaveOS -destination 'platform=iOS Simulator,name=iPhone 16'
```

### iOS Hard Constraints

- **All ViewModels** must be `@MainActor @Observable class` — no exceptions
- **All network services** (`RESTClient`, `WebSocketService`, `UDPStreamReceiver`) must be `actor` — thread safety is non-negotiable
- **Task closures** capturing `self` must use `[weak self]` — prevents retain cycles
- **All networking** goes through `RESTClient` or `WebSocketService` — no raw `URLSession` calls elsewhere
- **Parameter slider debounce**: 150ms minimum before sending REST/WS updates — prevents flooding the K1
- **British English** in all comments, logs, and UI strings

### iOS Tool Routing

| I need to... | Use this | NOT this |
|---|---|---|
| Check Swift compilation errors | `xcodebuild build` or IDE diagnostics | ~~clangd (C++ only)~~ |
| Find Swift symbol definitions | Read tool + grep | ~~clangd~~ |
| Verify iOS API usage | Context7 or Apple Developer docs | ~~training data~~ |
| Run iOS tests | `xcodebuild test` | ~~pytest~~ |

### iOS Reference Files

Read BEFORE exploring iOS source code:
- `lightwave-ios-v2/docs/reference/codebase-map.md` — 111 files, 12K LOC, directory structure
- `lightwave-ios-v2/docs/reference/fsm-reference.md` — 10 state machines (ConnectionState, WebSocket, UDP fallback, etc.)

### Subagent Delegation Protocol (MANDATORY)

When spawning a subagent, the orchestrating agent MUST include in the subagent prompt:

1. **Reference files** — which `docs/reference/` files the subagent should read first
2. **Hard constraints** — the specific constraints from this CLAUDE.md that apply to the subagent's scope
3. **Prior context** — any decisions, failed approaches, or session history relevant to the task
4. **Confidence gate** — include this instruction in the subagent prompt:

> "Before starting work, assess: do you have sufficient context to complete this task correctly? If NOT, state what is missing in your response rather than guessing. A wrong result wastes more tokens than asking for clarification."

5. **Scope boundary** — explicitly state what files/directories the subagent may modify, and what it must NOT touch

**Why:** Subagents inherit zero project context by default. A subagent that doesn't know about the 2.0ms render ceiling, the centre-origin rule, or the K1 AP-only constraint will produce code that violates them. The orchestrator pays 50 tokens to include constraints. The alternative is a full session wasted on rework.

**Subagent return contract (MANDATORY format):**
```
Files inspected: [count]
Findings: [distilled — no raw file contents]
Confidence: [high/medium/low + reasoning]
Open questions: [if any]
Token-relevant: [anything the main agent needs to act on]
```
Do NOT return raw file contents, full function bodies, or verbose traces. The main agent needs *conclusions*, not *evidence*.

**Return contract enforcement:** If a subagent return omits any of the mandatory headers above, the main agent MUST note the omission in its synthesis and flag reduced confidence for that subagent's contribution. Do NOT silently accept incomplete returns.

## Parallel Agent Sandboxing (MANDATORY)

When running parallel subagents that modify or build firmware code, **each agent MUST work in an isolated sandbox copy**. This is a hard rule — no exceptions.

**What happened:** Parallel agents modified `BeatTracker.cpp` source directly, corrupting 6 parameters and causing a regression from 17/17 to 15/17 synthetic tests. Hours of work lost to diagnosis and restoration.

**The rule:**
1. Before launch, copy the firmware tree: `cp -r firmware-v3 /tmp/agent_<name>_<timestamp>/`
2. Agent works EXCLUSIVELY in its sandbox — all edits, builds, and tests happen there
3. Agent returns ONLY data (numerical results, findings) — never file modifications
4. The orchestrator compares results across agents, then applies the winning change to the real tree exactly once
5. Agent prompt MUST include: *"Your working directory is `/tmp/<sandbox>/`. Do NOT modify files outside this directory."*

**Why this works:** firmware-v3 is ~15MB, copies in <1s. WAV test files resolve via absolute paths. PlatformIO toolchain is global (~/.platformio/). Each sandbox is self-contained with zero coupling.

**When to use:** Any time 2+ agents will modify source code, build, or run tests on the same codebase concurrently.

**Worktrunk vs cp -r:** Use Worktrunk (`wt create <name>`) when you need git history in the sandbox (e.g., diffing, cherry-picking). Use `cp -r` when you only need to build and test (faster, no git overhead). Default to `cp -r` unless git operations are required. The `.worktreeinclude` file at the project root shares `.pio/build/` and `node_modules/` between worktrees to avoid redundant builds.

### Cross-Session Handoff Protocol

When a session ends with incomplete work, or when context compaction is imminent, ask:

> **Does the next agent have everything it needs to continue this work without re-discovering anything I already learned?**

If NO, create a handoff note at `.claude/handoff.md` (overwrite any existing one) containing:

```
## Handoff — [date] — [task summary]

### Current state
[What is done, what is not done, what is partially done]

### Decisions made (and why)
[Architecture choices, approach selected, alternatives rejected — with reasoning]

### Failed approaches (do NOT retry these)
[What was tried and didn't work — save the next agent from repeating it]

### Blocked on
[What is preventing completion — missing info, broken tool, waiting on hardware, etc.]

### Next steps (in order)
[Exactly what the next agent should do first, second, third]

### Files modified
[List of files changed in this session, with one-line description of each change]

### Constraints encountered
[Any hard constraints that affected the work — so the next agent doesn't violate them]
```

**Why:** Context compaction summaries are mechanical — they list tool calls, not decisions. A handoff note captures *why* things were done, *what failed*, and *what to do next*. Without it, the next agent re-explores, re-discovers, and potentially re-tries approaches that already failed. Every re-discovery is wasted tokens. Every repeated failed approach is a wasted session.

**When to write:** Always write a handoff if work is incomplete. If work is complete, a handoff is optional but appreciated for complex multi-session tasks.

**Cleanup:** The next session's agent should delete `.claude/handoff.md` after reading it to prevent stale handoffs from persisting.

## Further Docs

Read **only** when the task requires it — do not load eagerly. Exception: WORKFLOW_ROUTING.md MUST be read on the first session of each day or after a tool/skill change.

| Topic | File | Lines | When to read |
|-------|------|-------|-------------|
| **Tool & skill routing** | [docs/WORKFLOW_ROUTING.md](docs/WORKFLOW_ROUTING.md) | 250 | First session of the day, or after tool/skill changes. Reference thereafter. |
| Context delegation guide | [firmware-v3/docs/CONTEXT_GUIDE.md](firmware-v3/docs/CONTEXT_GUIDE.md) | 80 | Before delegating complex research to subagents |
| Timing & memory budgets | [firmware-v3/CONSTRAINTS.md](firmware-v3/CONSTRAINTS.md) | 170 | Performance work, memory optimisation |
| Audio-reactive protocol | [firmware-v3/docs/audio-visual/audio-visual-semantic-mapping.md](firmware-v3/docs/audio-visual/audio-visual-semantic-mapping.md) | 467 | Writing/debugging audio-reactive effects |
| Effect development standard | [firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md](firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md) | 500 | Creating or modifying effects |
| Full REST API reference | [firmware-v3/docs/api/api-v1.md](firmware-v3/docs/api/api-v1.md) | 2,124 | API endpoint work — use QMD to search, do NOT read in full |
| CQRS state architecture | [firmware-v3/docs/CQRS_STATE_ARCHITECTURE.md](firmware-v3/docs/CQRS_STATE_ARCHITECTURE.md) | 652 | State management, command dispatch |
| Harness worker mode | [.claude/harness/HARNESS_RULES.md](.claude/harness/HARNESS_RULES.md) | 364 | Harness/test infrastructure |

## gstack

This project uses [gstack](https://github.com/garrytan/gstack) workflow skills.

Available skills:
- `/plan-ceo-review` — Founder/product thinking mode
- `/plan-eng-review` — Engineering architecture review mode
- `/review` — Paranoid pre-landing code review
- `/ship` — Automated release workflow (sync, test, push, PR)
- `/browse` — Browser-based QA via Playwright
- `/retro` — Weekly engineering retrospective

For web browsing tasks, use the /browse skill from gstack.
If skills aren't working, run: `cd ~/.claude/skills/gstack && ./setup`
