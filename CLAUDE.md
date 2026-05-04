# LightwaveOS

## RBDO Gate — Mandatory Before Tactical Output

**Applies to every agent on this repository (Claude Code, Codex CLI, any sub-agent or tooling that emits tactical output). No exceptions. Silent omission of the label below is itself a violation of this gate.**

Every tactical output (recommendation, decision, code change, plan, edit, commit, response to Captain) MUST be labelled with one of three states.

### Labels

**GROUNDED** — every premise traced to an upstream fact, evidence cited (file:line, commit hash, measurement, documented decision). Defensible without further qualification.

**DEGRADED-MODE** — operating under explicit calibration debt. The output MUST disclose all five fields:

- **Unresolved assumption** — the upstream fact that has not been calibrated.
- **Risk if wrong** — what happens to downstream behaviour if the assumption is incorrect.
- **Fallback** — what the output reverts to / becomes if the risk materialises.
- **Revisit trigger** — the concrete event that obligates re-auditing this output.
- **Debt count / affected outputs** — how many other tactical outputs depend on this same unresolved fact.

**REFUSED** — the output cannot be emitted under either GROUNDED or DEGRADED-MODE without violating a hard stop. Withhold the output; surface the blocker.

### Hard stops (REFUSE if any are true)

1. Emitting would violate a protected invariant (K1 hard constraints, R1–R5 governance in `AGENTS.md`, hardware-test-before-commit, audio-playback safety, audit-chain integrity).
2. The unresolved upstream fact already affects more than 3 tactical outputs without resolution. Resolve before adding a fourth dependent.
3. The output proposes a firmware behaviour change without Captain hardware sign-off attestation.
4. Sandbox-to-integration loss has been detected in the current session (the `6b1a222f` pattern). Surface and ask; do not continue.
5. The output cannot be independently audited by Captain — i.e. the calibration debt is so large that disclosure becomes hand-waving rather than risk-bounding.

### Captain-decision-menu rule

**No Captain decision menu is allowed until the agent first lists the upstream facts that make the options decidable.** Presenting tactical-preference options (a/b/c/d/e) without first surfacing the upstream facts that gate the choice is the face-value pattern that produced the 2026-04-27 drift. The right output when upstream is uncalibrated is "this question depends on facts F1, F2, F3 — added to `BACKLOG.md` § Critical — Upstream Calibration Debt", not a multiple-choice form.

### Reference

- The live calibration-debt ledger: `BACKLOG.md` § Critical — Upstream Calibration Debt.
- Full doctrine + anti-pattern catalogue + degradation ladder: `~/.claude/plans/shit-got-fucked-but-groovy-neumann.md` and the post-doctrine session transcript that authored this gate.
- Governance rules R1–R5: `AGENTS.md` § Workflow Discipline.

---

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

**Subagent token budget:** Each subagent MUST target completion in under 30K tokens. If scope requires more, split further into smaller subagents. Prefer narrow, focused scopes: one struct audit, one call-chain trace, one doc lookup — not "readers + blast radius" as a single scope.

**Stay in main context:** direct edits, single-file reads, iterative design work, synthesis of subagent returns.

For detailed subsystem costs, delegation scenarios, and context traps: [CONTEXT_GUIDE.md](firmware-v3/docs/CONTEXT_GUIDE.md)

## Tool Enforcement (HARD RULES — NOT SUGGESTIONS)

**Full routing table:** [docs/WORKFLOW_ROUTING.md](docs/WORKFLOW_ROUTING.md) — 29 skills, 13 MCP server groups, 47 tools. Read it on first session.

### Session Start (MANDATORY — every session, no exceptions)

Before your first action, answer this question honestly:

> **Do I have ALL the context I need to complete this task correctly — architecture, prior decisions, state machine behaviour, recent session history, and hard constraints — or am I about to guess?**

If the answer is anything other than an unqualified YES, use the tools below to fill the gaps BEFORE writing code or making changes. The cost of one tool call is negligible. The cost of a confident mistake is an entire session.

**Context tools available (memory/search ORDER, fastest-narrowest first):**
1. `Bash($RECALL_CLI "[query]")` — **Crispy recall** — exact-match transcript search (FTS5 + semantic) over verbatim past sessions. Best when you remember a phrase, file, or symptom; returns matched-message IDs you then read with `$RECALL_CLI <session> <msg>` (auto-centres ~30/70 around match). Project-scoped by default.
2. `mcp__plugin_claude-mem_mcp-search__search(query, limit=3-5, project="Lightwave-Ledstrip")` — **claude-mem L1 index** — extracted observations (decisions, bugfixes, discoveries) across sessions. Search first; use `type`, `obs_type`, `dateStart`, `dateEnd`, and `orderBy` filters before fetching details.
3. `mcp__plugin_claude-mem_mcp-search__timeline(anchor=<id>, depth_before=3, depth_after=3, project="Lightwave-Ledstrip")` — **claude-mem L2 timeline** — chronological context around a selected search hit.
4. `mcp__plugin_claude-mem_mcp-search__get_observations(ids=[...])` — **claude-mem L3 details** — full narratives/facts/files for filtered IDs only. Batch multiple IDs in one call; never fetch all search hits.
4.5. `mcp__notebooklm-mcp__notebook_query(notebook_id="92d45c0b-83c7-4971-aa9a-2c9ee13b06d4", query="...")` — **NotebookLM knowledge oracle** — pre-indexed architectural knowledge across 127 sterilised sources. Returns structured answers in 5 mandatory sections: ANSWER / CONSTRAINTS / KEY FILES / CROSS-REFS / WARNINGS. Use for "what/why" questions about architecture, constraints, design decisions, and subsystem relationships before reading reference docs. Cuts cold-start cost from ~30K tokens (reading 5+ reference docs) to one API call. Prefer `notebook_query_start` + `notebook_query_status` (async) for broad multi-section questions — synchronous calls may exceed the 60 s socket timeout on whole-corpus retrieval. Do NOT use NotebookLM for code symbol navigation (use clangd), current file contents (use Read), git/session history (use Crispy `$RECALL_CLI` or claude-mem), or anything where freshness against today's HEAD matters (the notebook is a periodic snapshot — verify against current source before any code edit).
5. Current source truth — C++ symbols use clangd FIRST per the C++ gate below. For non-C++ structural code/doc navigation, prefer claude-mem Smart Explore tools when available: `smart_search` → `smart_outline` → `smart_unfold`; full file reads are the last step for large files.
6. `mcp__auggie__codebase-retrieval("[query]")` — semantic codebase search (current source, not history) only if configured; see `docs/WORKFLOW_ROUTING.md` for live status.
7. `mcp__plugin_episodic-memory_episodic-memory__search` — fallback episodic store if current claude-mem is unavailable or returns no useful observations.
8. Reference files (see table below) — pre-extracted architecture, dependencies, FSMs.
9. `~/.claude/projects/<slug>/memory/MEMORY.md` — **file-based auto-memory** — index of topic files (feedback rules, project state, references, user facts). Read its **Memory Protocol** header before writing or updating entries (frontmatter, naming, two-step add procedure, update-don't-duplicate rule, ≤200-line hygiene). Applies to all agents (Claude, Codex, sub-agents).

Crispy returns raw transcripts (what was said). claude-mem returns synthesised observations (what was decided). Source files return current implementation truth. MEMORY.md returns curated rules and state. They are NOT redundant — use `$RECALL_CLI` first when you recall wording; use claude-mem `search → timeline → get_observations` when you only recall the topic.

If claude-mem emits a health/backlog/version warning, treat recent memory as possibly stale until you verify `/api/health`, `/api/version`, worker logs, or direct source/DB state. Do not silently fall back from a missing `mcp-search` tool to older `mem-search` or stale cache paths.

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
- NotebookLM: [will I query the knowledge base before reading reference docs? If yes, the exact question I will ask. If no (e.g. trivial single-file edit), why not.]
```

**Constraint readback table — read back ALL that apply to your task:**

| If task touches... | Read back these constraints |
|---|---|
| Any C++ source | clangd FIRST for symbols, grep for text only |
| Audio / effects | Centre origin 79/80 outward, no heap in render(), 2.0ms ceiling, no rainbows |
| Network / WiFi | K1 is AP-only. Never enable STA mode, AP+STA, STA validation envs, or WiFi-mode rewrites without explicit Captain approval. Historical STA/dual-mode material is referenced context only, not active instruction. |
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

These contain pre-extracted codebase structure, frameworks, dependencies, entrypoints, and state machine definitions. ~200 lines each vs 20,000+ tokens to re-discover. **Read the reference files.**

### Protocol Contract (MANDATORY for network code)

**Gate rule:** Before adding, modifying, or consuming any WebSocket command or REST endpoint, read `docs/protocol/k1-ws-contract.yaml` (WebSocket) and/or `docs/protocol/k1-rest-contract.yaml` (REST). These are the single source of truth for what K1 exposes and what Tab5/iOS consumes. If a command is not in the contract, it does not exist. **Update the contract FIRST, then implement.** Do NOT add WS commands to K1 without updating the YAML. Do NOT consume commands from Tab5 without verifying they exist in the contract and your field names match.

### LVGL Code (MANDATORY before touching tab5-encoder/src/ui/)

**Gate rule:** Before modifying any file in `tab5-encoder/src/ui/`, read `tab5-encoder/docs/reference/lvgl-component-reference.md`. This documents the exact widget tree, colour system, font assignments, layout patterns, and 12 anti-patterns that cause visual bugs, memory leaks, and WDT panics.

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

**Prerequisite:** `compile_commands.json` must exist in `firmware-v3/`. If missing: `pio run -e esp32dev_audio_esv11_k1v2_32khz --target compiledb`

**When grep IS appropriate:** string literals, log messages, comments, config values, or non-C++ files. grep is for TEXT. clangd is for CODE SYMBOLS.

### Documentation Search — QMD FIRST, file reading LAST

**Gate rule:** When you need to find information in project documentation, query QMD before reading files. QMD searches across all indexed collections (firmware docs, War Room, landing page docs) with semantic matching.

| I need to... | Call this | NOT this |
|---|---|---|
| Find docs about a topic | `mcp__qmd__qmd_search` or `mcp__qmd__qmd_vector_search` | ~~reading files one by one~~ |
| Deep multi-step doc retrieval | `mcp__qmd__qmd_deep_search` | ~~loading 200+ line docs into context~~ |
| Get a specific known doc | `mcp__qmd__qmd_get` | ~~Read tool (acceptable fallback)~~ |
| Check what's indexed | `mcp__qmd__qmd_status` | ~~guessing~~ |

**Fallback:** If QMD returns nothing or `qmd_status` shows zero collections, STOP and report: `[TOOL FAIL: QMD — not indexed]`. Ask the user whether to index it now or fall back to grep/Read. Do NOT silently switch.

### Architectural Knowledge — NotebookLM FIRST, reference docs LAST

**Gate rule:** When you need to understand a subsystem's architecture, why a decision was made, what constraints apply across multiple files, or how two subsystems interact — query NotebookLM before reading any `docs/reference/*.md`, `EFFECT_DEVELOPMENT_STANDARD.md`, or cross-project docs. The notebook returns the structured answer in one call; reading the equivalent docs costs ~30K tokens.

| I need to understand... | Call this | NOT this |
|---|---|---|
| A subsystem's architecture before touching code | `mcp__notebooklm-mcp__notebook_query` | ~~Reading 5+ reference docs~~ |
| What constraints apply to a planned change | `mcp__notebooklm-mcp__notebook_query` | ~~Scanning CLAUDE.md sections~~ |
| Why a design decision was made | `mcp__notebooklm-mcp__notebook_query` | ~~Grepping commit history~~ |
| Cross-subsystem interactions (iOS↔firmware↔Tab5) | `mcp__notebooklm-mcp__notebook_query` or `cross_notebook_query` | ~~Spawning 3 subagents to read 3 codebase-maps~~ |
| Where a C++ symbol is defined | `mcp__clangd__find_definition` | ~~notebook_query~~ |
| Current file contents | `Read` | ~~notebook_query~~ |
| What changed since last session | Crispy `$RECALL_CLI` / claude-mem | ~~notebook_query~~ |
| Whole-corpus broad question (likely >60 s) | `notebook_query_start` + `notebook_query_status` | ~~`notebook_query` (will time out)~~ |

**When NotebookLM is NOT acceptable:**
- Any code edit that depends on today's HEAD state — the notebook is a snapshot, not a live mirror. After NotebookLM gives you the architectural answer, verify with clangd / Read before writing code.
- Symbol navigation in C++ — clangd is canonical, NotebookLM is conceptual.
- Git history forensics — use `git log` / `git blame`, not NotebookLM.

**Tool failure:** If `notebook_query` times out or errors and the async fallback also fails, STOP per the Tool Failure Protocol — do NOT silently fall back to reading reference docs without flagging the degradation. The doc-read fallback is acceptable only after explicit Captain approval.

### Library APIs — Context7, not training data

**Gate rule:** When referencing external library APIs (FastLED, ArduinoJSON, ESPAsyncWebServer, FreeRTOS, etc.), DSP formulae (FFT windowing, spectral centroid, onset detection, beat tracking), signal processing algorithms, or any domain-specific computation where parameter correctness matters — query Context7 for authoritative docs. Do NOT rely on training data.

| I need to... | Call this | NOT this |
|---|---|---|
| Check a FastLED/ArduinoJSON/FreeRTOS API | `mcp__Context7__resolve-library-id` then `mcp__Context7__get-library-docs` | ~~reciting from training data~~ |
| Verify function signatures for an ESP-IDF call | `mcp__Context7__get-library-docs` with topic filter | ~~assuming parameter order~~ |
| Look up a PlatformIO library method | Context7 first, then `mcp__clangd__get_hover` on the call site | ~~reading .pio/libdeps headers~~ |

**When training data IS acceptable:** Standard C/C++ library calls (`memcpy`, `printf`, `std::vector`), basic FreeRTOS primitives (`xTaskCreate`, `xQueueSend`) stable for 10+ years. If in doubt, check Context7.

### Development Lifecycle

Before writing implementation code, follow this sequence: `/brainstorming` → `/software-architecture` → `/test-driven-development` → THEN implement.

**Scope:** Applies to **new features, new effects, new API endpoints, and architectural changes**. Does NOT apply to:
- Bug fixes where the root cause is already identified
- One-line or few-line changes (< 20 LOC across all files)
- Documentation-only changes
- Config/build file edits

For scoped work, start at the appropriate stage. A bug fix with known cause starts at `/test-driven-development`.

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
4. **Do NOT proceed** until the user responds. A broken tool is a broken workflow — it stays broken for every future session until someone fixes it.
5. If the user authorises a fallback, mark ALL output derived from it as `[FALLBACK: reason]` so confidence is explicit.
6. If the tool failure is fixable (missing config, missing index, wrong path), **offer to fix it** rather than working around it.

### Pre-Commit Confidence Gate

Before running `git add` or `git commit`, answer honestly:

> **Am I confident this change is correct, tested, and does not violate any hard constraint — or am I hoping it works?**

If you cannot answer YES with specific evidence, do NOT commit. Instead:

1. **Check constraints** — re-read Hard Constraints below. Does your change touch render()? Verify no heap alloc. Timing? Measure against 2.0ms ceiling. WiFi? Confirm AP-only preserved.
2. **Check tests** — did you run the relevant tests? Do they pass? If none exist, write one first.
3. **Check scope** — are you committing only the files you intended? No accidental inclusions?
4. **Check British English** — comments, logs, UI strings all use centre/colour/initialise/behaviour.

Only after satisfying all four checks: proceed with commit.

## Hard Constraints

- **K1 WiFi mode is AP-only.** Current shipping firmware uses `WIFI_AP_ONLY` in canonical ESV11 K1 build environments, and Tab5/iOS connect to K1's AP at `192.168.4.1`. Never enable STA mode, AP+STA mode, STA validation environments, WiFi-mode rewrites, or default changes to `WIFI_AP_ONLY` / `m_forceApOnly` without explicit Captain approval. Historical STA/dual-mode analysis may be kept as referenced forensic context, but it is not active instruction for agents.
- **Audio playback safety**: Never generate, select, or play audio through speakers/headphones unless Captain has explicitly approved that exact source. Approval for one audio file does not authorise other files, synthetic fixtures, white/pink noise, hats, cymbals, speech, generated tones, or any agent-chosen sound. For AFS/runtime audio capture, the approved reference corpus is `/Users/spectrasynq/Workspace_Management/Software/hybrid-beat-tracker/tests/benchmark` unless Captain explicitly names a different source. Before any playback, state the exact file/source, output path/device if known, volume assumption, duration, and stop command. If a capture matrix needs noise or synthetic fixtures, ask first and wait.
- **Centre origin**: All effects originate from LED 79/80 outward (or inward to 79/80). No linear sweeps. Applies to all render modes including zone-specific renders. Exception: zone ID `0xFF` (global render) where the physical centre is still 79/80.
- **No rainbows**: No rainbow cycling or full hue-wheel sweeps.
- **No heap alloc in render**: No `new`/`malloc`/`String` in `render()` or any function transitively called from `render()`. Use static buffers. Includes helper functions, utility calls, and String concatenation.
- **120 FPS target**: Per-frame effect code MUST complete in under 2.0 ms. Hard ceiling — measure with `esp_timer_get_time()` if in doubt.
- **British English** in all comments, docs, logs, and UI strings: centre, colour, initialise, serialise, behaviour, etc.

## Workspace Rules (ENFORCED)

Violations will be flagged and reverted.

### Root Allowlist

Only these entries are permitted at the project root (enforced by CI — see `.github/workflows/repo_hygiene_check.yml`):

**Root files (13):** `README.md`, `LICENSE`, `NOTICE`, `CHANGELOG.md`, `CONTRIBUTING.md`, `TRADEMARK.md`, `CLAUDE.md`, `AGENTS.md`, `BACKLOG.md`, `.gitignore`, `.pre-commit-config.yaml`, `.mcp.json`, `.worktreeinclude`

**Root directories (11+4 hidden):** `.git`, `.github`, `.claude`, `.codex`, `_archive`, `docs`, `firmware-v3`, `harness`, `instructions`, `k1-composer`, `lightwave-dashboard`, `lightwave-ios-v2`, `scripts`, `tab5-encoder`, `tools`

**Nothing else goes at root.** Governance docs → `instructions/`. Decision registers → `k1-launch-research/`. Launch materials → `~/SpectraSynq_K1_Launch_Planning/` (separate repo). Temp notes → `.claude/`. If you need a new root file, get explicit Captain approval first.

### Directory Map

| Directory | Purpose |
|-----------|---------|
| `firmware-v3/` | ESP32-S3 firmware (LightwaveOS) |
| `lightwave-ios-v2/` | iOS companion app (Swift/SwiftUI) |
| `tab5-encoder/` | M5Stack Tab5 controller (PlatformIO) |
| `harness/` | Feasibility test harnesses (voice recognition, hardware probes) |
| `k1-composer/` | Web compositor/debug tool |
| `lightwave-dashboard/` | Web dashboard app (TypeScript/React) |
| `docs/` | Technical documentation |
| `tools/` | Evaluation and capture tools |
| `scripts/` | Setup and validation scripts |
| `instructions/` | Governance instructions |
| `_archive/` | Historical/superseded artifacts |

**External repos:** `~/SpectraSynq_K1_Launch_Planning/` (launch materials), `~/SpectraSynq.LandingPage/` (Next.js + R3F landing page).

### Changelog Maintenance

CHANGELOG.md (Keep a Changelog format): add entries under `## [Unreleased]` with category (Added/Changed/Fixed/Removed), one line per change, present tense, subsystem prefix (`firmware:`, `ios:`, `tab5:`, `tools:`, `docs:`, `brand:`).

### No Orphan Files

No files at root. Research → `research/` or `k1-launch-research/`. Agent prompts → `brand/` or `docs/agents/`. Media → `media/`. Decision HTMLs → `brand/decisions/`. Temp notes → `.claude/` or `_scratch/`. Handoffs → `.claude/`. If nothing fits, create an appropriate directory. Do NOT dump at root.

## Architecture

ESP32-S3 LED controller for a dual-strip Light Guide Plate. 320 WS2812 LEDs (2x160), 100+ effects, audio-reactive, web-controlled.

**Audio backend:** ESV11 at 32 kHz, 125 Hz frame rate. 64-bin Goertzel + 12-note chroma + 8-band octave + tempo/beat tracking. The `_32khz` envs are the only canonical build path — they apply the calibrated tempo/beat-tracking constants via `EsV11_32kHz_Shim.h` (the configuration where beat tracking actually works).

**Actor model** (FreeRTOS tasks): AudioActor (Core 0) | RendererActor (Core 1) | ShowDirectorActor | CommandActor | PluginManagerActor

**Data flow:** Microphone → I2S DMA → AudioActor → ESV11 backend → PipelineAdapter → ControlBus → RendererActor → Effects → FastLED → RMT → WS2812 LEDs

**Key abstractions:**
- `ControlBus` — shared audio state. Use `mcp__clangd__get_document_symbols` on `src/audio/contracts/ControlBus.h` for the full field inventory. Key fields: `bands[0..7]` (octave energy), `chroma[0..11]` (pitch class), `rms`, `beat`, `onset`, `bins256[]`, tempo fields, percussion triggers. Always check source via clangd — do NOT assume exhaustive.
- `RenderContext` — per-frame: `leds[]`, `dt`, `zoneId`, `controlBus`. Zone ID `0xFF` = global render.
- Effects inherit `EffectBase`, implement `render(RenderContext&)`. All zone-indexed access must bounds-check: `(ctx.zoneId < kMaxZones) ? ctx.zoneId : 0`

## Build (PlatformIO)

```bash
cd firmware-v3

# K1 v2 hardware (production target)
pio run -e esp32dev_audio_esv11_k1v2_32khz
pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload

# V1 hardware (non-K1 dev boards)
pio run -e esp32dev_audio_esv11_32khz

# Serial monitor (verify MAC before opening — port name varies)
pio device monitor -b 115200
```

These two `_32khz` envs are the canonical K1 build path — pinned to ESV11 at 32 kHz / 125 Hz frame rate with the calibrated tempo/beat-tracking shim. Other envs in `platformio.ini` exist for benchmarks, native tests, and unrelated boards; do not build them for K1 work without explicit reason.

### Tracing / Profiling — MabuTrace

`TRACE_SCOPE` / `TRACE_COUNTER` / `TRACE_INSTANT` macros across the codebase are no-op stubs unless `FEATURE_MABUTRACE=1` is set. **The canonical K1 build envs do NOT enable tracing.** To capture telemetry:

| Trace target | Env |
|---|---|
| K1 V2 (production hardware) | `esp32dev_audio_esv11_k1v2_32khz_trace` |
| V1 dev boards | `esp32dev_audio_esv11_32khz_trace` |
| PipelineCore | `esp32dev_audio_pipelinecore_trace` |
| Bare ESV11 base | `esp32dev_audio_trace` |

**Build + flash:** `pio run -e <env_trace> -t upload --upload-port /dev/tty.usbmodem<port>`

**Capture (automated, one command):**
```bash
~/.platformio/penv/bin/python3 firmware-v3/tools/capture_trace.py \
    --port /dev/tty.usbmodem2101 --effect 0xNNNN --soak 8 \
    --output /tmp/trace.json --open
```

`capture_trace.py` switches effect, soaks, sends `trace`, strips `[TRACE]` markers, validates JSON, opens `https://ui.perfetto.dev`. Requires exclusive port access — close any other serial monitor first. Events record to a 64 KB on-chip ring buffer; Perfetto UI is Google's open-source viewer (JSON parsed in-browser, no uploads, no project portal). Full workflow + serial protocol: `firmware-v3/docs/debugging/MABUTRACE_GUIDE.md`. Spec for adding NEW TRACE_* points across the system: `firmware-v3/docs/debugging/TRACE_INSTRUMENTATION_SPEC.md`.

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

- **All ViewModels** must be `@MainActor @Observable class`
- **All network services** (`RESTClient`, `WebSocketService`, `UDPStreamReceiver`) must be `actor`
- **Task closures** capturing `self` must use `[weak self]`
- **All networking** goes through `RESTClient` or `WebSocketService` — no raw `URLSession` calls
- **Parameter slider debounce**: 150ms minimum before sending REST/WS updates
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
4. **Confidence gate** — include this instruction:

> "Before starting work, assess: do you have sufficient context to complete this task correctly? If NOT, state what is missing in your response rather than guessing. A wrong result wastes more tokens than asking for clarification."

5. **Scope boundary** — explicitly state what files/directories the subagent may modify, and what it must NOT touch

**Subagent return contract (MANDATORY format):**
```
Files inspected: [count]
Findings: [distilled — no raw file contents]
Confidence: [high/medium/low + reasoning]
Open questions: [if any]
Token-relevant: [anything the main agent needs to act on]
```
Do NOT return raw file contents, full function bodies, or verbose traces. Return *conclusions*, not *evidence*.

**Return contract enforcement:** If a subagent return omits any mandatory header, the main agent MUST note the omission in its synthesis and flag reduced confidence for that contribution. Do NOT silently accept incomplete returns.

## Parallel Agent Sandboxing (MANDATORY)

When running parallel subagents that modify or build firmware code, **each agent MUST work in an isolated sandbox copy**. No exceptions.

**The rule:**
1. Before launch, copy the firmware tree: `cp -r firmware-v3 /tmp/agent_<name>_<timestamp>/`
2. Agent works EXCLUSIVELY in its sandbox — all edits, builds, and tests happen there
3. Agent returns ONLY data (numerical results, findings) — never file modifications
4. The orchestrator compares results across agents, then applies the winning change to the real tree exactly once
5. Agent prompt MUST include: *"Your working directory is `/tmp/<sandbox>/`. Do NOT modify files outside this directory."*

**When to use:** Any time 2+ agents will modify source code, build, or run tests on the same codebase concurrently.

**Worktrunk vs cp -r:** Use Worktrunk (`wt create <name>`) when you need git history in the sandbox (diffing, cherry-picking). Use `cp -r` when you only need to build and test (faster, no git overhead). Default to `cp -r`. The `.worktreeinclude` file at the project root shares `.pio/build/` and `node_modules/` between worktrees to avoid redundant builds.

## RTK Token Compression

RTK (Rust Token Killer) v0.34.2 is a CLI proxy that compresses Bash command output before it reaches the context window, saving 60-90% of tokens on routine dev operations.

**Telemetry:** Disabled via `RTK_TELEMETRY_DISABLED=1` and `[telemetry] enabled = false` in `~/.config/rtk/config.toml`. No data leaves the machine.

**How it works:** A PreToolUse hook rewrites Bash commands (e.g. `git status` becomes `rtk git status`). Transparent — agents do not invoke `rtk` directly.

**What it compresses:** `git` (status, diff, log, show), `ls`, `pio run` / `pio test` build output, `xcodebuild`, `grep`, `find`, `cat`, `npm`, `cargo`, and other verbose CLI tools.

**What it does NOT affect:**
- Built-in tools: Read, Grep, Glob (these bypass Bash entirely)
- MCP tools: clangd, QMD, Context7, autocontext, devkg, episodic memory
- Serial monitor: `pio device monitor` (excluded — needs raw stream)

**Configuration:** `~/.config/rtk/config.toml`. Excluded commands (pass through unmodified): `esptool.py`, `pio device monitor`, `capture`. Meta commands: `rtk gain` (savings report), `rtk verify` (config check), `rtk discover` (missed compression opportunities).

### Cross-Session Continuity Protocol

Two continuity mechanisms serve different purposes. Neither may create `.claude/handoff*.md` forward task lists.

**1. Crispy `/handoff` — IN-SESSION ROTATION** (when context bloat is degrading quality NOW):

```
/crispy:handoff <next-task-summary>
```

Runs three steps automatically: `handoff-prompt-to` (distill into self-contained prompt), `reflect` (verify completeness against codebase), `clear-and-execute` (rotate into a fresh Crispy-managed session with context handed across IPC). Carries the prompt for in-session rotation only.

**2. `BACKLOG.md` — CROSS-SESSION FORWARD WORK**

`BACKLOG.md` is the single source of truth for forward work. Do NOT write `.claude/handoff*.md` files containing next steps, task lists, or future prescriptions. If work is incomplete, update the appropriate `BACKLOG.md` row with current state, blocker, evidence, and next action.

Completed-work postmortems are allowed when they describe what shipped, cite commit hashes or artefact paths, and do not prescribe forward tasks. Failure notes belong in `BACKLOG.md` unless Captain explicitly asks for a separate postmortem document.

**If running long AND work is incomplete:** rotate via `/crispy:handoff` if available, then update `BACKLOG.md` rather than writing a forward handoff file.

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
| MabuTrace tracing & Perfetto | [firmware-v3/docs/debugging/MABUTRACE_GUIDE.md](firmware-v3/docs/debugging/MABUTRACE_GUIDE.md) | ~200 | Capturing on-chip timeline traces; only when telemetry is needed |
| Harness worker mode | [.claude/harness/HARNESS_RULES.md](.claude/harness/HARNESS_RULES.md) | 364 | Harness/test infrastructure |

## NotebookLM Knowledge Base

| Notebook | ID | Sources | Use when... |
|---|---|---|---|
| Lightwave-Ledstrip | `92d45c0b-83c7-4971-aa9a-2c9ee13b06d4` | 127 | Architecture, constraints, design decisions, cross-subsystem questions for firmware-v3, lightwave-ios-v2, tab5-encoder, protocol contracts, audio pipeline, WiFi, governance |

Full SpectraSynq notebook registry (7 notebooks, IDs, source counts, bundle paths): [`notebooklm_bundles/NOTEBOOK_REGISTRY.md`](notebooklm_bundles/NOTEBOOK_REGISTRY.md).

Custom system prompt (configured 2026-05-04, polished 2026-05-04 with WS contract-first gate) mandates a 5-section response format: ANSWER / CONSTRAINTS / KEY FILES / CROSS-REFS / WARNINGS. The prompt enforces British English, AP-only-WiFi sterilisation, no-heap-in-render warnings, centre-origin guidance on every effect-related answer, and the contract-first gate (`docs/protocol/k1-ws-contract.yaml` updated BEFORE implementation; the "regeneratable artefact" framing is reconciliation-only). If a response loses the structure or breaches sterilisation, re-run `chat_configure` per `notebooklm_bundles/CC_CLI_NOTEBOOKLM_INTEGRATION_PROMPT.md`.

### Cross-notebook queries

For questions spanning multiple SpectraSynq projects (e.g. "how does the marketing positioning of K1 align with the technical AP-only constraint?", "where does PRISM.studio reference K1 protocol contracts?"), use:

```
mcp__notebooklm-mcp__cross_notebook_query(
    query="...",
    notebook_names="Lightwave-Ledstrip, SpectraSynq.LandingPage"
)
```

Available notebooks (see `notebooklm_bundles/NOTEBOOK_REGISTRY.md` for IDs, sources, last-sync dates):

- **Lightwave-Ledstrip** — firmware/iOS/Tab5 codebase + protocol + governance (127 sources)
- **K1 Testbed** — testbed/dev hardware + capture rigs (37 sources)
- **War Room** — governance, doctrine, decision history (93 sources)
- **K1 Launch Planning** — launch checklist, demo plans, gating (47 sources)
- **K1 Marketing** — positioning, copy, banned language, audience (91 sources)
- **SpectraSynq.LandingPage** — landing-page Next.js + R3F site (89 sources)
- **PRISM.studio** — PRISM compositor/studio app (66 sources)

Cross-notebook is rate-limited — prefer single-notebook queries when one notebook clearly owns the answer.

## autocontext — Evolved Strategy Scenarios

autocontext MCP (`uv run autoctx mcp-serve`). Two scenarios seeded with LightwaveOS history:

| Scenario | When to use |
|----------|-------------|
| `embedded_effect_design` | New LED effect — iterates against centre-origin, no-heap, dt-correction, audio-reactivity rubric (max 3 rounds, threshold 0.80) |
| `ios_feature_implementation` | New SwiftUI feature — iterates against @Observable, debounce, 44pt, Codable, architecture rubric (max 3 rounds, threshold 0.82) |

Use via `autocontext_run_improvement_loop(scenario_name="...", initial_output="<implementation>", max_rounds=3, quality_threshold=0.80)`. Playbooks accumulate in `knowledge/<scenario>/`.

## gstack

This project uses [gstack](https://github.com/garrytan/gstack) workflow skills: `/plan-ceo-review` (founder/product thinking), `/plan-eng-review` (architecture review), `/review` (paranoid pre-landing code review), `/ship` (automated release), `/browse` (Playwright-based QA), `/retro` (weekly retrospective).

For web browsing tasks, use `/browse`. If skills aren't working: `cd ~/.claude/skills/gstack && ./setup`

## Crispy

Crispy (`the-sylvester.crispy` v0.3.2) is a Cursor extension that ships a Claude Code plugin bundling 12 `crispy:*` skills, 4 CLI binaries, an IPC dispatch host, and a 676 MB SQLite memory database (FTS5 + nomic-embed semantic search). The host runs INSIDE Cursor — sessions launched outside Cursor (raw `claude` CLI, Codex, Warp, etc.) will not have `$CRISPY_SOCK` populated and dispatch-bound skills will fail. Check `$CRISPY_SOCK` before invoking.

CLI binaries (paths exported as env vars by the Crispy host):
- `$RECALL_CLI` (`recall.js`) — transcript search/read
- `$CRISPY_DISPATCH` — IPC dispatch
- `$CRISPY_AGENT` — multi-vendor agent wrapper (claude/codex/opencode)
- `$CRISPY_TRACKER` — Rosie project tracker
- `$CRISPY_SESSION` — session lifecycle script

IPC socket: `$CRISPY_SOCK` (server registry at `~/.crispy/ipc/servers.json`). Memory DB: `~/.crispy/crispy.db`.

### Skill catalogue (12 `crispy:*` skills)

| Skill | When to use |
|-------|-------------|
| `crispy:spec-mode` | Fuzzy ideation — build a feature spec conversationally before any plan exists. Produces `.ai-reference/specs/<feature>.md`. |
| `crispy:reflect` | After a spec/plan is drafted — verify the prompt captures conversation + codebase reality before execution. |
| `crispy:super-implement` | Materialise execution prompts from a finished spec/plan (handoff-prompt generator that fans out to subagents). |
| `crispy:handoff-prompt-to` | Synthesize a self-contained implementation prompt for a specific fresh agent (auto-decomposes if oversized). |
| `crispy:handoff` | IN-SESSION rotation — distill current context, reflect, and rotate into a fresh session. Use when context bloat is degrading quality NOW. |
| `crispy:clear-and-execute` | Clear context and continue with a fresh prompt in the same lane. Use for "fresh slate, same task". |
| `crispy:switch-session` | Switch to an existing session in-place (resume named session). |
| `crispy:recall` | Search/read past session transcripts (raw transcript text, not synthesised observations). |
| `crispy:superthink` | Multi-vendor adversarial review — dispatches parallel child sessions (claude + codex) via IPC. Use INSIDE `/review` for high-risk diffs. |
| `crispy:crispy-agent` | Unified wrapper for IPC dispatch to claude/codex/opencode. Returns `session_id`. |
| `crispy:rosie-tracker` | Inspect/dump Rosie project state. Complements (does not replace) GSD `.planning/` and `BACKLOG.md`. |
| `crispy:backup-transcripts` | Safety-net transcript archival. Do NOT invoke unless the user explicitly asks. |

### Planning workflow — mutually exclusive lanes

Pick ONE planning lane per task. Do not cross-activate. Crispy adds `spec-mode` as a third lane alongside Superpowers and GSD.

| Lane | Entry point | Use when... | Output location |
|------|-------------|-------------|-----------------|
| Crispy spec-mode | `/crispy:spec-mode` | Fuzzy idea, conversational spec-building, PRE-plan exploration | `.ai-reference/specs/<feature>.md` |
| Superpowers | `/brainstorming` → `/writing-plans` | Single-feature structured plan with TDD requirement | `docs/superpowers/` |
| GSD | `/gsd:plan-phase` | Milestone-scale, multi-phase, with verification gates | `.planning/` |

After a spec or plan exists, materialise execution prompts via `/crispy:super-implement` OR `/subagent-driven-development` (Superpowers) OR `/gsd:execute-phase` (GSD). Do NOT mix lanes for the same feature.

### `/crispy:superthink` — adversarial review trigger conditions

Superthink is the multi-vendor (Claude + Codex parallel) adversarial layer. NOT a replacement for `/review` — it is used INSIDE `/review` when the diff warrants extra cost. Trigger when the diff touches:

- Firmware render path (`render()`, anything called from `render()`, RendererActor, FastLED.show())
- Audio chain (AudioActor, ControlBus producers, ESV11 backend, beat tracking, onset detection)
- Network protocol changes (WebSocket commands, REST endpoints, k1-ws-contract.yaml, k1-rest-contract.yaml, WiFi mode)
- Multi-actor concurrency changes (cross-core writes, mutex acquisition order, queue sizing)
- Anything explicitly flagged by `/review` as wide blast radius (per code-review-graph guidance)

For low-risk diffs (docs, comments, single-file refactors with passing tests), `/review` alone is sufficient.

### Health check — Crispy warnings

A SessionStart hook at `~/.claude/hooks/crispy-health-check.sh` runs silently when healthy. If you see a warning:

| Warning | Action |
|---------|--------|
| Missing env vars (`$CRISPY_SOCK`, `$RECALL_CLI`, etc.) | Restart Crispy host inside Cursor (extension command palette → "Crispy: Restart Host"). If still missing, session likely NOT Cursor-launched — see "Crispy is OFF" below. |
| Dead socket (`$CRISPY_SOCK` set but unreachable) | Host crashed. Check `~/.crispy/ipc/servers.json` for stale entries. Restart in Cursor. |
| Missing binaries (`$RECALL_CLI` path does not exist) | Reinstall the Crispy extension in Cursor. |
| Empty DB (`~/.crispy/crispy.db` is 0 bytes or missing) | First-run state. Skip `crispy:recall` until host has indexed at least one session. |
| Broken recall (`$RECALL_CLI` returns errors) | Check FTS5 index integrity; `--rebuild-index` if Crispy CLI supports it; otherwise file a Crispy issue. |

Do NOT silently work around a Crispy warning. Surface it per the Tool Failure Protocol and ask whether to attempt a fix or fall back to non-Crispy memory layers.

### Crispy is OFF — Claude Code session NOT launched by Crispy/Cursor

If `$CRISPY_SOCK` is unset and `$RECALL_CLI` is absent (raw `claude` from a terminal, or inside Codex/Warp/another harness), Crispy is OFF. Do NOT call any `crispy:*` skill. Fall back to:

- Memory search: `mcp__plugin_claude-mem_mcp-search__search` → `mcp__plugin_claude-mem_mcp-search__timeline` → `mcp__plugin_claude-mem_mcp-search__get_observations`; use `episodic-memory` only as fallback (skip the recall layer)
- Planning: Superpowers (`/brainstorming` → `/writing-plans`) or GSD (`/gsd:plan-phase`) — both work without Crispy
- Adversarial review: `/review` alone (no superthink multi-vendor parallel pass)
- Forward work: update `BACKLOG.md`; do not create `.claude/handoff*.md` forward task lists

Note Crispy unavailability in your output so the user can decide whether to relaunch in Cursor.
