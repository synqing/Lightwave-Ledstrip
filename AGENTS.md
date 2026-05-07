# LightwaveOS

## RBDO Gate — Mandatory (canonical text in `CLAUDE.md` top)

The Risk-Bounded Degraded Operation gate at the top of `CLAUDE.md` applies to **every agent on this repository** — Codex CLI, Claude Code, sub-agents, any tooling that emits tactical output. Honour the labelling discipline (GROUNDED / DEGRADED-MODE / REFUSED), the five hard stops, and the Captain-decision-menu rule. The live calibration-debt ledger is `BACKLOG.md` § Critical — Upstream Calibration Debt. Do not emit a tactical output without reading the gate.

ESP32-S3 LED controller for a dual-strip Light Guide Plate. 320 WS2812 LEDs, 100+ effects, audio-reactive, web-controlled.

## Memory And Claude-Mem Routing

For prior-session context, follow root `CLAUDE.md` § Session Start and `docs/WORKFLOW_ROUTING.md` before searching ad hoc. Current claude-mem memory routing is `mcp__plugin_claude-mem_mcp-search__search` → `mcp__plugin_claude-mem_mcp-search__timeline` → `mcp__plugin_claude-mem_mcp-search__get_observations`; do not use stale `mem-search` tool names. Use `$RECALL_CLI` first for exact raw transcript phrases, claude-mem memory search for synthesised observations and decisions, and direct source/DB/process checks for current truth. `smart_search`, `smart_outline`, and `smart_unfold` are Smart Explore code-navigation helpers only: if they return `Transport closed`, unsupported-language, or parser errors, do not treat that as claude-mem memory outage evidence and do not block. Fall back immediately to worker `GET /api/search`, SQLite FTS in `~/.claude-mem/claude-mem.db`, `$RECALL_CLI`, `rg`, or clangd according to whether the need is memory or current source. If claude-mem reports health, version, or backlog warnings, verify live state before trusting recent memory. Do not enable generated folder `CLAUDE.md` files or edit inside `<claude-mem-context>` blocks without an explicit Captain decision.

## Codex clangd Routing

Codex CLI does not load Claude Code's `clangd-lsp` plugin. On this machine, Codex must expose the global `clangd` MCP server in `~/.codex/config.toml`, backed by `/Users/spectrasynq/.local/bin/mcp-language-server-lightwave`, Homebrew clangd, `firmware-v3/compile_commands.json`, `--enable-config`, and an Xtensa query-driver glob matching `toolchain-xtensa-esp32s3*/bin/xtensa-esp32s3-elf-*`. The upstream `mcp-language-server@v0.1.1` diagnostics path is not acceptable here because it requests clangd pull diagnostics; the local `mcp-language-server-lightwave` binary uses pushed diagnostics. Existing Codex sessions must be restarted after MCP config changes. If Claude-style `mcp__clangd__find_definition` names are absent but generic semantic tools such as `definition`, `references`, `diagnostics`, and `hover` are present, use those tools. If no clangd semantic tool is exposed, report a tool failure; do not grep C++ symbols.

`codex mcp get clangd` proves only the current config file. It does not prove that an already-running Codex session is using that config. If clangd fails after a config repair, check live children with `ps -axo pid,ppid,etime,command | rg 'mcp-language-server|clangd.*Lightwave-Ledstrip/firmware-v3'`; any child using `/Users/spectrasynq/.local/bin/mcp-language-server`, missing `--enable-config`, using `--background-index`, or using the non-wildcard `toolchain-xtensa-esp32s3/bin` query-driver is stale and requires a Codex session restart.

`firmware-v3/.clangd` is part of the Codex clangd route. It strips Xtensa GCC-only flags, defines Xtensa preprocessor macros for Homebrew clangd parsing, and suppresses host-only ESP-IDF section-attribute diagnostics. Do not remove it just because PlatformIO builds without it; it is for semantic tooling, not firmware compilation.

Use the MCP `diagnostics` tool as the Codex semantic smoke. Do not treat raw `clangd --check` internal `ExtractFunction` tweak failures as the operational gate when MCP diagnostics returns clean pushed diagnostics.

In a newly restarted Codex session that has not yet called clangd, it is valid to run `tools/codex-clangd-mcp-reset.sh` from that session as pre-smoke hygiene and then immediately run exactly one MCP `diagnostics` smoke. The "poisoned live session" rule applies only after a clangd MCP call in that same session has already returned `Transport closed`.

If a Codex session reports `Transport closed` from the registered `clangd` MCP after the route is visible, treat the live Codex session as poisoned. Do not retry clangd in that same session and do not fall back to grep for C++ symbol claims. Stop tactical C++ symbol work, run `tools/codex-clangd-mcp-reset.sh` from a separate shell or orchestrator context to clear child processes, then restart the Codex session before the next semantic clangd call.

## Build (PlatformIO)

```bash
cd firmware-v3

# Canonical production profile (ESV11 K1v2, 32 kHz; [platformio] default_envs)
pio run -e esp32dev_audio_esv11_k1v2_32khz
pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload

# Legacy/dev-only ESV11 profile
pio run -e esp32dev_audio_esv11

# PipelineCore exists in platformio.ini but is not production-active; do not use it for K1 production work.

# Serial monitor
pio device monitor -b 115200
```

## Hard Constraints

- **Centre origin**: All effects originate from LED 79/80 outward (or inward to 79/80). No linear sweeps.
- **No rainbows**: No rainbow cycling or full hue-wheel sweeps.
- **No heap alloc in render**: No `new`/`malloc`/`String` in `render()` paths. Use static buffers.
- **120 FPS / 2.0 ms ceiling**: Keep per-frame effect code under 2.0 ms.
- **dt-correct smoothing**: Temporal smoothing must use delta-time, not frame-count assumptions.
- **Sub-8 ms audio-to-visual latency**: Preserve the end-to-end pipeline latency constraint.
- **K1 WiFi mode**: Current shipping firmware is AP-only via `WIFI_AP_ONLY`. Goal-state is dual-mode AP OR STA, never concurrent AP+STA. Do not enable pure-STA validation, WiFi-mode rewrites, or `WIFI_AP_ONLY` / `m_forceApOnly` default changes without explicit Captain approval.
- **British English** in comments and docs (centre, colour, initialise, behaviour).

## Workflow Discipline (Agents)

These rules were codified after the 2026-04-27/28 orchestration drift (see `~/.claude/plans/shit-got-fucked-but-groovy-neumann.md`). Follow them strictly.

1. **Single source of truth for forward work is `BACKLOG.md`** (root). Do NOT write `.claude/handoff*.md` files containing forward task lists. Postmortems describing what shipped (with commit hashes) are fine; forward tasks in `.claude/` are forbidden because they create re-prescription loops where the next session "applies patches" that already landed in commits the handoff didn't see.

2. **`feat(...)` commits MUST anchor to a Phase Move per the Synergy-Topology programme taxonomy in `BACKLOG.md`**, or be tagged `chore` / `fix` / `docs` / `ci` / `test` / `refactor`. A bare `feat(firmware): add X` body without a Phase Move reference (or without a `Captain visual sign-off:` line for Phase 5+ effect commits) is reviewable. The Phase 5 commit `39406e6b` violated this rule and bypassed the visual sign-off gate; do not repeat.

3. **Anti-redundancy gate before applying any patch**: `grep` the target lines for the proposed change. If the substantive content is already there, ABORT — surface to the operator instead of writing a no-op edit. The 2026-04-27 drift was driven by multiple sessions reading a stale handoff and "applying patches" that were already in HEAD. The Edit tool's `old_string` exact-match discipline is not a sufficient guard; explicit grep before edit is.

4. **Sandbox-to-integration return-receipt**: when dispatching parallel SSAs that return diffs (per `parallel-agent-sandboxing` skill), the orchestrator MUST verify the integrated state against the SSA's claimed deliverables before declaring the SSA complete. Commit `6b1a222f` is a verbatim record of the cost of skipping this step (lost edits, rework session needed). The plan that produced this rule cites the failure pattern.

5. **For every commit, the body cites its plan or BACKLOG row**: `Refs: plan <name>.md Tier X.Y` or `Refs: BACKLOG.md §<section>`. This makes the chain from intention → commit auditable in `git log` alone, no extra tooling needed.

## Visual Pipeline Guardrails (Agents)

- **Treat `FastLED.show()` wire time as a safety invariant** on dual 160-LED strips. Expected average is ~4.8-5.5 ms. If telemetry shows ~1 ms, assume premature return/tearing risk and stop.
- **Do not trust a single FPS field in isolation**. Always cross-check `framesRendered / uptime` and `avgFrameTimeUs`.
- **Renderer cadence must be self-clocked to 120 FPS budget** (`~8333 us` frame pacing), not RTOS tick-quantised.
- **K1v2 must keep status strip disabled** (`FEATURE_STATUS_STRIP_TOUCH=0`); K1v1 may enable it.
- **Large shared snapshots belong in persistent buffers (PSRAM-preferred) allocated at init**, never on `loopTask` stack hot paths.
- **Before upload, verify device identity by MAC** and confirm target env/pin mapping; never rely on serial port name alone.
- **After any render/memory change, run serial `s` checks** and confirm:
  - no panics/RMT errors;
  - `LED show` sane for configured strip length;
  - `showSkips=0` under normal load;
  - stack/heap headroom stable (no progressive collapse).

## Further Docs

Read these when the task requires it:

| Topic | File |
|-------|------|
| Timing & memory budgets | [firmware-v3/CONSTRAINTS.md](firmware-v3/CONSTRAINTS.md) |
| Audio-reactive protocol | [firmware-v3/docs/audio-visual/audio-visual-semantic-mapping.md](firmware-v3/docs/audio-visual/audio-visual-semantic-mapping.md) |
| Full REST API reference | [firmware-v3/docs/api/api-v1.md](firmware-v3/docs/api/api-v1.md) |
| CQRS state architecture | [firmware-v3/docs/CQRS_STATE_ARCHITECTURE.md](firmware-v3/docs/CQRS_STATE_ARCHITECTURE.md) |
| LED incident lessons | [firmware-v3/docs/INCIDENT_LED_STABILITY_POSTMORTEM_2026-03-04.md](firmware-v3/docs/INCIDENT_LED_STABILITY_POSTMORTEM_2026-03-04.md) |
| Harness worker mode | [.claude/harness/HARNESS_RULES.md](.claude/harness/HARNESS_RULES.md) |
