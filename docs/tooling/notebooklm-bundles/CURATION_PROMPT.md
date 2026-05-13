# NotebookLM Curation & Bundling — Lightwave-Ledstrip

> **Run this from a CC CLI session** in the Lightwave-Ledstrip project directory (`~/Workspace_Management/Software/Lightwave-Ledstrip/`). This is a self-contained prompt — no external context required. Copy this file to `.claude/notebooklm_curation_prompt.md` before running.

## Objective

Create a sterilised NotebookLM bundle of the Lightwave-Ledstrip project. Output to:
`~/Workspace_Management/Software/Lightwave-Ledstrip/docs/tooling/notebooklm-bundles/lightwave_ledstrip/`

**This is the largest and most complex curation job.** 802 source files, 5.84 MB of code across 5 subprojects: firmware-v3 (ESP32-S3), lightwave-ios-v2 (Swift/SwiftUI), tab5-encoder (M5Stack/LVGL), k1-composer (web), lightwave-dashboard (web). Plus extensive documentation.

## Sterilisation Doctrine (NON-NEGOTIABLE)

**No document enters a NotebookLM bundle unless its canonical status has been POSITIVELY VERIFIED.**

NotebookLM treats every uploaded source as equally authoritative. A single stale document becomes a source of confidently wrong answers. This project is especially dangerous because it has YEARS of engineering history — deprecated WiFi modes, abandoned backends, superseded audio chains. The current canonical state is precisely documented in CLAUDE.md. Anything that contradicts CLAUDE.md is excluded unless it has an explicit historical disclaimer.

**Key canonical truths (from CLAUDE.md — the authority):**
- K1 is AP-ONLY. NEVER enable STA mode. STA has never worked.
- Audio backend: ESV11 at 32 kHz, 125 Hz frame rate. The `_32khz` envs are canonical.
- Actor model: AudioActor (Core 0) | RendererActor (Core 1) | ShowDirectorActor | CommandActor | PluginManagerActor
- Centre origin: all effects from LED 79/80 outward
- No heap alloc in render()
- 120 FPS target, 2.0ms per-frame ceiling
- British English everywhere

## NotebookLM constraints

- Accepts: `.md`, `.txt`, `.pdf` only
- Max 300 sources per notebook
- ~500K words / 200MB per source
- Code files (`.cpp`, `.h`, `.swift`, `.ts`, `.js`, `.yaml`, `.json`) must be wrapped in `.txt` bundles

## Bundle format

### For .md files
Copy directly with flattened path: `firmware-v3/docs/api/api-v1.md` → `firmware-v3_docs_api_api-v1.md`

### For code files
Create `.txt` bundles with separator headers:
```
================================================================================
FILE: relative/path/to/file.cpp
DESCRIPTION: Brief description
================================================================================

<full file contents>
```

**Bundling strategy for code:** Do NOT bundle all 802 source files. Bundle only the architecturally significant files — contracts, interfaces, key implementations. The goal is to give NotebookLM enough to understand the architecture, not to replicate the IDE.

### NEVER modify source files. All operations on COPIES only.

## INCLUDE heuristics

### Tier 1: Root authority documents
- `CLAUDE.md` — project master instructions (THE authority document)
- `AGENTS.md` — agent governance
- `BACKLOG.md` — current work tracking
- `README.md`, `CONTRIBUTING.md`, `CHANGELOG.md`
- `.pre-commit-config.yaml` — as .txt bundle

### Tier 2: Architecture & reference documentation
- `firmware-v3/docs/reference/codebase-map.md` — firmware architecture map
- `firmware-v3/docs/reference/fsm-reference.md` — state machines
- `lightwave-ios-v2/docs/reference/codebase-map.md` — iOS architecture map
- `lightwave-ios-v2/docs/reference/fsm-reference.md` — iOS state machines
- `tab5-encoder/docs/reference/codebase-map.md` — Tab5 architecture map
- `tab5-encoder/docs/reference/fsm-reference.md` — Tab5 state machines
- `tab5-encoder/docs/reference/lvgl-component-reference.md` — LVGL UI reference

### Tier 3: Technical documentation
- `docs/WORKFLOW_ROUTING.md` — tool & skill routing (29 skills, 13 MCP groups, 47 tools)
- `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md` — effect development rules
- `firmware-v3/docs/CQRS_STATE_ARCHITECTURE.md` — state management architecture
- `firmware-v3/docs/audio-visual/audio-visual-semantic-mapping.md` — audio-reactive protocol
- `firmware-v3/docs/api/api-v1.md` — full REST API reference (**NOTE: 2,124 lines — check if it exceeds NotebookLM word limit; may need splitting**)
- `firmware-v3/docs/debugging/MABUTRACE_GUIDE.md` — tracing guide
- `firmware-v3/docs/debugging/TRACE_INSTRUMENTATION_SPEC.md` — trace spec
- `firmware-v3/CONSTRAINTS.md` — timing & memory budgets
- `docs/protocol/k1-ws-contract.yaml` — WebSocket contract (as .txt)
- `docs/protocol/k1-rest-contract.yaml` — REST contract (as .txt)
- Any other `.md` files in `firmware-v3/docs/` — scan and include if they document current architecture

### Tier 4: Key source code bundles (architecturally significant only)

Create these `.txt` bundles:

**`_BUNDLE_firmware_contracts.txt`** — the interfaces and data contracts:
- `firmware-v3/src/audio/contracts/ControlBus.h` — shared audio state
- `firmware-v3/src/plugins/api/EffectContext.h` — effect context
- `firmware-v3/src/plugins/api/EffectBase.h` — effect base class
- `firmware-v3/src/plugins/api/RenderContext.h` — render context (if separate from EffectContext)

**`_BUNDLE_firmware_actors.txt`** — actor model entry points (just the header/class declarations, not full implementations if files are huge):
- `firmware-v3/src/audio/AudioActor.h`
- `firmware-v3/src/rendering/RendererActor.h`
- `firmware-v3/src/show/ShowDirectorActor.h`
- `firmware-v3/src/commands/CommandActor.h`

**`_BUNDLE_protocol_contracts.txt`** — the YAML contracts:
- `docs/protocol/k1-ws-contract.yaml`
- `docs/protocol/k1-rest-contract.yaml`

**`_BUNDLE_ios_architecture.txt`** — key Swift interfaces:
- Select 3-5 key Swift files that define the architecture (ViewModels, Services, NetworkClient)

**`_BUNDLE_tab5_architecture.txt`** — key Tab5 interfaces:
- Select 3-5 key files that define the Tab5 architecture

### Tier 5: Instructions & governance
- `instructions/*.md` — all governance docs in this directory
- `docs/*.md` — any remaining docs at the docs root level

## EXCLUDE heuristics

- `_archive/` — historical/superseded artifacts (CN3 vendor SDK, deprecated boards)
- `docs/tooling/notebooklm-bundles/` — our own output directory (do NOT re-bundle bundled content)
- `.pio/` — PlatformIO build cache and vendor libraries
- `.git/`, `.github/` — version control
- `.claude/worktrees/` — agent sandboxes
- `node_modules/` — any JS vendor dependencies
- **All 100+ effect implementations** — individual effect `.cpp` files are operational code, not architecture. The EFFECT_DEVELOPMENT_STANDARD.md documents the patterns. Including all effects would blow the 300-source limit and add noise.
- **Test files** — `tests/`, `test/`, `*_test.cpp`, `*_test.swift` — operational
- Build configuration files (`platformio.ini`, `project.yml`, `Package.swift`) — include ONE bundle if they encode important configuration, otherwise exclude
- `harness/` — feasibility test harnesses (voice recognition, hardware probes) — exclude unless docs exist
- `k1-composer/` and `lightwave-dashboard/` — web tools. Include docs if they exist; exclude source code unless architecturally significant
- Any file referencing STA mode WiFi as a viable approach — this is the single most dangerous stale knowledge in the project
- `scripts/` — utility scripts. Exclude unless they encode important operational knowledge.
- `tools/` — evaluation tools. Same as scripts.

## WiFi mode safety check

**CRITICAL:** Grep the entire output directory for "STA", "station mode", "WiFi client", "wifi_sta". If any result suggests STA mode is viable or planned, EXCLUDE that file or add a prominent disclaimer: "K1 is AP-ONLY. STA mode has never worked (driver-level auth failures, 6+ failed mitigations). Do NOT implement STA."

## Manifest requirement

Create `MANIFEST.md` listing:
- Every INCLUDE file with source path, bundle membership (if in a .txt bundle), and fixes applied
- Every EXCLUDE decision with reason
- Total counts
- Confidence assessment
- Unresolved flags for Captain review
- WiFi safety check results

## Estimated output

~50-70 files. The reference docs and codebase maps carry the heavy architectural knowledge. The code bundles provide enough interface-level detail for NotebookLM to understand the system without drowning in implementation. The protocol contracts give exact API surface.

## Validation

1. No `.cpp`, `.h`, `.swift`, `.ts`, `.js`, `.yaml`, `.json`, `.py` files exist unwrapped
2. No reference to STA mode as viable
3. MANIFEST.md is complete
4. Every file in output is `.md` or `.txt`
5. Total file count < 300 (NotebookLM limit)
