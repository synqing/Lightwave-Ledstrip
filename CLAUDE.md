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

**Subagent return contract (MANDATORY format):**
```
Files inspected: [count]
Findings: [distilled — no raw file contents]
Confidence: [high/medium/low + reasoning]
Open questions: [if any]
Token-relevant: [anything the main agent needs to act on]
```
Do NOT return raw file contents, full function bodies, or verbose traces. The main agent needs *conclusions*, not *evidence*.

**Return contract enforcement:** If a subagent return omits any of the mandatory headers above (Files inspected, Findings, Confidence, Open questions, Token-relevant), the main agent MUST note the omission in its synthesis and flag reduced confidence for that subagent's contribution. Do NOT silently accept incomplete returns.

For detailed subsystem costs, delegation scenarios, and context traps: [CONTEXT_GUIDE.md](firmware-v3/docs/CONTEXT_GUIDE.md)

## Tool Enforcement (HARD RULES — NOT SUGGESTIONS)

**Full routing table:** [docs/WORKFLOW_ROUTING.md](docs/WORKFLOW_ROUTING.md) — 29 skills, 13 MCP server groups, 47 tools. Read it on first session.

### Session Start (MANDATORY — every session, no exceptions)

1. `mcp__plugin_claude-mem_mem-search__search("recent [topic]")` — check what happened last session
2. `mcp__plugin_claude-mem_mem-search__timeline()` — chronological context
3. If task touches code → `mcp__auggie__codebase-retrieval("[query]")` BEFORE any file reading

### C++ Symbol Navigation — clangd FIRST, grep NEVER (for symbols)

**Gate rule:** When you need to find a C++ function definition, caller, reference, or type — you MUST call clangd. Do NOT grep/glob for C++ symbol names. If clangd fails, log the failure and THEN fall back to grep.

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

**Prerequisite:** `compile_commands.json` must exist in `firmware-v3/`. If missing: `pio run -e esp32dev_audio_pipelinecore --target compiledb`

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

**Fallback:** If QMD returns nothing or `qmd_status` shows zero collections, it hasn't been indexed yet. Fall back to grep/Read and note "QMD not indexed" in your response.

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

### Tool Failure Protocol (applies to ALL tools above)

When any MCP tool call fails (timeout, error, empty result):
1. Note the failure in your response: `[TOOL FAIL: tool_name — error summary]`
2. Fall back to the next-best method (grep for clangd, Read for QMD, training data for Context7)
3. Mark any answer derived from fallback as `[FALLBACK]` so the user knows it's lower confidence
4. Do NOT silently switch methods. Do NOT retry more than once.

## Hard Constraints

- **K1 is AP-ONLY. NEVER enable STA mode.** The K1 device runs as a WiFi Access Point. Tab5 and iOS connect TO it. Do NOT attempt to connect K1 to external WiFi routers — STA authentication fails at the 802.11 driver level (AUTH_EXPIRE reason 2, AUTH_FAIL reason 202) and has NEVER been resolved despite 6+ mitigation attempts. This was architecturally resolved in Feb 2026. See `WiFiManager.h` for details. **Do not modify WiFi mode, add STA connection logic, or change AP configuration without explicit user approval.**
- **Centre origin**: All effects originate from LED 79/80 outward (or inward to 79/80). No linear sweeps. This applies to all render modes including zone-specific renders. The only exception is zone ID `0xFF` (global render) where the physical centre is still 79/80.
- **No rainbows**: No rainbow cycling or full hue-wheel sweeps.
- **No heap alloc in render**: No `new`/`malloc`/`String` in `render()` or any function transitively called from `render()`. Use static buffers. This includes helper functions, utility calls, and String concatenation.
- **120 FPS target**: Per-frame effect code MUST complete in under 2.0 ms. Not "approximately" — 2.0 ms is the hard ceiling. Measure with `esp_timer_get_time()` if in doubt.
- **British English** in all comments, docs, logs, and UI strings: centre, colour, initialise, serialise, behaviour, etc.

## Architecture

ESP32-S3 LED controller for a dual-strip Light Guide Plate. 320 WS2812 LEDs (2x160), 100+ effects, audio-reactive, web-controlled.

**Two audio backends** (conditional compilation, only one active per build):
- `esp32dev_audio_pipelinecore` — primary development (256-bin FFT, beat tracking, onset detection)
- `esp32dev_audio_esv11` — legacy (64-bin Goertzel)

**Actor model** (FreeRTOS tasks): AudioActor (Core 0) | RendererActor (Core 1) | ShowDirectorActor | CommandActor | PluginManagerActor

**Data flow:** Microphone → I2S DMA → AudioActor → PipelineCore/ESV11 → PipelineAdapter → ControlBus → RendererActor → Effects → FastLED → RMT → WS2812 LEDs

**Key abstractions:**
- `ControlBus` — shared audio state. For the full field inventory, use `mcp__clangd__get_document_symbols` on `src/audio/contracts/ControlBus.h`. Key fields include: `bands[0..7]` (octave energy), `chroma[0..11]` (pitch class), `rms`, `beat`, `onset`, `bins256[]`, tempo fields, and percussion triggers. Do NOT assume this list is exhaustive — always check the source via clangd.
- `RenderContext` — per-frame: `leds[]`, `dt`, `zoneId`, `controlBus`. Zone ID `0xFF` = global render.
- Effects inherit `EffectBase`, implement `render(RenderContext&)`. All zone-indexed access must bounds-check: `(ctx.zoneId < kMaxZones) ? ctx.zoneId : 0`

## Build (PlatformIO)

```bash
cd firmware-v3

# Primary (PipelineCore)
pio run -e esp32dev_audio_pipelinecore
pio run -e esp32dev_audio_pipelinecore -t upload

# Legacy (ESV11)
pio run -e esp32dev_audio_esv11

# Serial monitor
pio device monitor -b 115200
```

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
