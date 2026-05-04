# SSA4 Partial Manifest — lightwave-ios-v2 docs + Swift architecture bundle

Scope: lightwave-ios-v2 only. Output dir: `docs/tooling/notebooklm-bundles/lightwave_ledstrip/`.
Generated: 2026-05-04. British English throughout.

## Part A — iOS docs

### INCLUDE

| Source | Output filename | Reason |
|---|---|---|
| `lightwave-ios-v2/docs/reference/codebase-map.md` | `lightwave-ios-v2_docs_reference_codebase-map.md` | Mandatory. Canonical 111-file directory map (12K LOC) — primary navigation aid for any iOS-side query. |
| `lightwave-ios-v2/docs/reference/fsm-reference.md` | `lightwave-ios-v2_docs_reference_fsm-reference.md` | Mandatory. 10 state machines (ConnectionState, WebSocket reconnect, UDP fallback, etc.). Highest-density architectural reference. |
| `lightwave-ios-v2/docs/CLAUDE.md` | `lightwave-ios-v2_docs_CLAUDE.md` | Mandatory. Project-level CLAUDE.md scoped to iOS — encodes the hard constraints (`@MainActor @Observable`, `actor` for network services, 150ms debounce, AP-only). |
| `lightwave-ios-v2/docs/DESIGN_SPEC.md` | `lightwave-ios-v2_docs_DESIGN_SPEC.md` | Canonical typography + spacing + colour token spec. Maps directly to `Theme/DesignTokens.swift`. Reflects current shipped UI direction. No STA/deprecated markers. |

### EXCLUDE

| Source | Reason |
|---|---|
| `lightwave-ios-v2/docs/AUDIT_REPORT.md` | Dated audit (3 February 2026). Lists "risks" + "recommendations" against current architecture (e.g. "split AppViewModel"), most of which have not been actioned. NotebookLM would treat the recommendations as fact and contradict the canonical CLAUDE.md hard constraints. Excluded per dated-audit rule. |
| `lightwave-ios-v2/docs/EDGEMIXER_INTEGRATION_RESEARCH.md` | Integration research dated 2026-03-25 — proposes 3 new Swift files + 2 modified files for an EdgeMixer Play tab feature that has not yet shipped. Speculative architectural prose. Excluded per "integration research that hasn't shipped" rule. |

## Part B — `_BUNDLE_ios_architecture.txt`

Filename: `_BUNDLE_ios_architecture.txt` (88 KB, 2,586 lines)

### Files packed (5)

| Path | Role |
|---|---|
| `lightwave-ios-v2/LightwaveOS/App/LightwaveOSApp.swift` | App entry — `@main` root, environment wiring, AppViewModel hand-off (58 lines). |
| `lightwave-ios-v2/LightwaveOS/Network/RESTClient.swift` | Actor-isolated REST client — single canonical HTTP path; 150ms debounce policy; AP-only base URL (1,175 lines). |
| `lightwave-ios-v2/LightwaveOS/Network/WebSocketService.swift` | Actor-isolated WS service — bidirectional control/telemetry; reconnection state machine (849 lines). |
| `lightwave-ios-v2/LightwaveOS/Network/UDPStreamReceiver.swift` | Actor-isolated UDP receiver — high-frequency LED stream ingest (147 lines). |
| `lightwave-ios-v2/LightwaveOS/Network/DeviceDiscoveryService.swift` | K1 discovery via Bonjour/mDNS on the local AP network (318 lines). |

### Files flagged (not packed)

| Path | Reason for omission |
|---|---|
| `lightwave-ios-v2/LightwaveOS/ViewModels/AppViewModel.swift` (776 lines) | Audit identifies it as a monolith — would dilute the architectural signal in the bundle and bias NotebookLM toward the current overloaded pattern rather than the actor-decomposition model. The `fsm-reference.md` already captures its observable surface. |
| `lightwave-ios-v2/LightwaveOS/Views/ContentView.swift` | Out of scope — bundle is architecture-focused (network actors + entry), not view layer. The codebase-map.md gives sufficient pointers. |
| `lightwave-ios-v2/LightwaveOSTests/ConnectionStateTests.swift` | Test code, not architecture. |

## STA scan results

- `grep -iE "(STA mode\|wifi_sta\|station mode\|ESP_WIFI_MODE_STA)"` across all candidate iOS docs and across the final bundle: **0 hits**.
- `grep -inE "\.sta\b"` across bundled Swift + included docs: **0 hits**.
- Result: AP-ONLY architecture preserved across every artefact. K1 is the AP; iOS connects to it.

## Confidence + unresolved flags

- **Confidence: high.** All four included docs verified to exist, classified per the include/exclude rule, and free of STA-mode contamination. All five Swift files verified by `find` before bundling. Bundle assembled by streaming `cat` directly to disk — no Swift content entered the agent context, preserving token budget.
- **Unresolved flags:** none. AppViewModel exclusion is a deliberate curation call (motivated by AUDIT_REPORT findings); flagging here so the orchestrator can override if NotebookLM users need ViewModel surface area.
- **Documents written:** 4 markdown docs + 1 architecture bundle + this manifest = 6 files total in this SSA4 contribution.
