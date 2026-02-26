# K1 Composer — Task List (What’s Next)

**Last updated:** 2026-02-26.  
**Source:** Implementation path ([k1-composer-implementation-path.md](k1-composer-implementation-path.md)), README, implementation prompt, and current codebase.

---

## Current state (done)

- **Phases 1–6:** Contract lock, firmware stream transport, external render mode, lease enforcement, status/telemetry, dashboard transport runtime (lease + stream lifecycle, bufferedAmount + drop-old).
- **Phase 7 (partial):** Live code visualiser (phase highlighting, variable cards, trace), effect source catalogue generated, phase-range mapping, effect selector.
- **Phase 8:** Browser diagnostics (queue, send cadence/jitter, drop ratio 10s/60s).
- **Diagnostic panel:** `k1-diagnostic.html` + `DiagnosticDataAdapter.js` + `proto-combined.jsx` — already wired to **real** WebSocket LED/audio binary streams (own WS, subscribe/unsubscribe, parse 966/464-byte frames).

---

## What’s next (prioritised)

### 1. High value, small scope

| Task | Description | Where |
|------|-------------|--------|
| **Show internal SRAM in dashboard** | Device status API now returns `freeInternalHeap`. Surface it in the main dashboard (e.g. status line or Stream Diagnostics) so users can see WiFi headroom without serial. | `app.js` (status poll / `applyStatusFrame` or a dedicated “device health” line), optionally `index.html` (badge or small readout). |

### 2. Phase 9 — WASM parity rollout

| Task | Description | Where |
|------|-------------|--------|
| Emscripten build path | Add/build Emscripten scaffold and JS loader for effect code. | `wasm/` (README mentions “Emscripten scaffold (optional Tier 1 parity path)”). |
| Tier 1 parity | Curated effect set with deterministic frame parity checks. | Scripts + dashboard integration. |
| Tier 2/3 | Expand effect coverage without protocol change. | Incremental. |

### 3. Phase 10 — Hardening and rollout

| Task | Description | Where |
|------|-------------|--------|
| Burn-in tests | Mixed clients (many observers, one owner). | Test plan / manual or automated. |
| Stale recovery | Validate behaviour when stream goes stale or network impairs. | Firmware + dashboard. |
| Feature defaults | Promote stream/lease defaults once latency and lock correctness are stable. | Config / docs. |

### 4. Implementation prompt checklist (if still in scope)

The [implementation prompt](k1-composer-implementation-prompt.md) describes merging v1 and v3 into **one** `k1-composer.html` and fixing flaws. The **current** setup is **modular** (`index.html` + `app.js` + `k1-diagnostic.html`). Decide:

- **Option A:** Treat `index.html` + `app.js` as canonical; close or reframe prompt checklist items that assume a single file.
- **Option B:** Complete the single-file merge into `k1-composer.html` and satisfy the checklist (timeline phase segments, highlightCodePhase via CSS class, AI panel structure, etc.).

Outstanding checklist items (if you keep the prompt as the spec) include:

- Timeline with **colored phase segments** behind the range input.
- **highlightCodePhase** using a CSS class (e.g. `.code-line.active-line`) instead of inline styles.
- AI Assistant panel structure (placeholder, disabled input).
- Various visual/quality items (dark theme, fonts, responsive, eventBus wiring, etc.).

---

## Suggested “next” in order

1. **Surface `freeInternalHeap`** in the main dashboard (status or diagnostics) — quick win, uses the new API.
2. **Clarify merge vs modular:** Either adopt the implementation prompt’s single-file + checklist as the next sprint, or formalise the current modular stack as the product and archive/reframe the prompt.
3. **Phase 9 (WASM):** Start with Emscripten scaffold and one Tier 1 effect if you want browser-side effect parity.
4. **Phase 10 (Hardening):** After stream/lease behaviour is stable, run burn-in and lock defaults.

If you say which of these you want to tackle first (e.g. “internal SRAM in UI” or “Phase 9 WASM” or “checklist/merge”), the next step can be a concrete implementation plan for that item.
