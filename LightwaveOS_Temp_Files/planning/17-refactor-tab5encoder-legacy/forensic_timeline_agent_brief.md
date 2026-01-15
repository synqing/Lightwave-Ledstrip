# Hub↔Node Forensic Timeline (Agent Brief)

This is an agent-optimised companion to `planning/17-refactor-tab5encoder-legacy/forensic_timeline.html`.

Goal: give Claude/Codex agents a **trustable, evidence-linked** view of the Jan 1→14 bring-up history, with emphasis on the **Tab5.encoder WebSocket control path** and the **Jan 13 wireless hub↔node architecture work**.

All times are Perth / AWST (UTC+8).

Scope note: a number of K1.LightwaveOS claude-mem observations have the generic title “Codex assistant reply”. Treat the **timestamp + text body** as the source of truth, not the title.

## Sources (order of trust)

1. **Git history** (highest trust): anchors in the HTML under “Repo evidence (git commits)”.
2. **claude-mem observations/prompts**: links to `http://127.0.0.1:37777/api/observation/<id>` and `.../api/prompt/<id>`.
3. **Narrative summaries**: only as pointers; verify by opening the underlying IDs above.

## Timeline at a glance (phases)

- **Precursor (Jan 1–11)**: WebServer + Tab5.encoder control stack work (WS client, router, rate limiting, ParameterHandler sync); plus performance/debugging incidents.
- **Planning (Jan 12)**: refactor intent captured; MCP setup; hardware correction captured.
- **Wireless architecture (Jan 12–13)**: `K1.LightwaveOS` hub↔node architecture lands; UDP time-sync introduced; fanout disarm; registry callbacks; hub dashboard work.
- **Bringup (Jan 13)**: Phase 1 started; Tab5 UI encoders wired to HubState; reconnect/pathology debugging begins.
- **Stabilise (late Jan 13 → early Jan 14)**: zones override discovery, watchdog mitigation, WS backpressure, snapshot gating, reflashes.
- **Consolidate (Jan 14)**: Codex→claude-mem import + gem promotion tooling.

## What agents must internalise (avoid re-breaking)

- WebSocket traffic can **flood and disconnect** (`AsyncWebSocket.cpp: Too many messages queued`) if not backpressure-safe; changes must preserve reliable encoder UX.
- Zone mode can **mask global parameter changes**; always validate which layer “wins” before declaring control-path failure.
- Watchdog triggers on `async_tcp` indicate heavy work in network callbacks; move work out of callback contexts.
- In debugging: keep serial noise gated; avoid high-frequency spam that blocks diagnosis.

## High-signal evidence pointers

### WS control path (Tab5.encoder, Jan 2–3)

These entries explain how reliable WS must be configured (reconnect, throttling, routing, and parameter sync):

- WS client core: reconnection + rate limiting — `obs#10466`
- WS message router: protocol type dispatch — `obs#10509`
- Tab5.encoder WS client rate limiting — `obs#10533`
- WebSocketClient ported to Tab5.encoder — `obs#10537`
- Tab5.encoder message routing layer — `obs#10539`
- ParameterHandler: encoder→WS synchronisation — `obs#10556`
- ParameterHandler: dual-unit + sync — `obs#10597`
- Milestone F: WiFi + WebSocket integration — `obs#11124`

### Wireless architecture build (hub↔node, Jan 13)

These are the missing “architecture-heavy” milestones (mostly visible via git history, not chat):

- UDP time-sync protocol added — `ref-k1-66f3a06`
- Disarm UDP fanout on WS disconnect — `ref-k1-62f2658`
- Hub registry state transition callbacks — `ref-k1-fb2815e`
- Hub dashboard LVGL monitoring UI — `ref-k1-047cc5c`

### Bringup and failures (Jan 13–14)

- Phase 1 implemented (HubState + batching) — `obs#13675`
- Encoders wired (UI→HubState) — `obs#13676`
- Encoder mapping fixed (legacy order restored) — `obs#13677`
- Hub→node wired and validated end-to-end — `obs#13678`
- Reconnect failure symptoms — `prompt#1233`
- End-to-end trace request (“hub sends, node lags”) — `prompt#1234`
- TS/UDP log gate (silence flood) — `obs#13690`, `obs#13692`
- Time-sync sign fix (applyAt conversion) — `obs#13693`
- WDT mitigation (move work off async_tcp) — `obs#13696`
- Stack mapping evidence — `obs#13697`
- System audit and legacy path notes — `obs#13698`, `obs#13699`
- WS queue overflow and backpressure plan — `obs#13701`, `obs#13702`, `obs#13703`
- Zones opt-in + snapshot gating — `obs#13704`, `obs#13705`
- Local cycle invalid effect bug — `obs#13706`, fix — `obs#13707`

## Agent runbook (do this, not vibes)

When an agent is asked “encoders don’t work / node doesn’t respond”, run this checklist in order:

1. **Confirm the control plane being exercised**
   - Is the user expecting WS-driven control or local serial control?
   - If WS: confirm hub is not closing the socket for queue overflow (see `obs#13701`).

2. **Confirm layer precedence**
   - If node is in **ZONES mode**, global scene changes may be masked (`prompt#1247`). Validate “which layer wins” before changing code.

3. **Confirm time base**
   - If scheduled `applyAt_us` commands land seconds late or in the future, suspect time-sync conversion (`obs#13693`).

4. **Confirm backpressure and batching**
   - Encoder deltas must be batched/throttled; high-frequency per-click sends can still flood if the code path fans out multiple downstream messages (see `obs#10466`, `obs#10556`, `obs#10597`, `obs#13702`).

5. **If watchdog appears**
   - Any WDT on `async_tcp` means heavy work in the callback; move it out (see `obs#13696`), and re-check LVGL re-entrancy issues (`obs#13697`).

## How to use this as an agent (fast procedure)

1. Open `planning/17-refactor-tab5encoder-legacy/forensic_timeline.html` and read the axis left→right once.
2. For any disputed claim, open the link (prompt/obs/commit anchor) and treat the raw artefact as the source of truth.
3. For WS reliability changes, start from the Appendix WS milestones and confirm:
   - throttling (batching/limits),
   - routing correctness (`zones.*` vs typos),
   - backpressure behaviour (drop vs close),
   - no heavy work in callbacks.
