# Context Engineer Input Bundle (CEIB) - Legacy Parity Recovery

**Date:** 2026-01-14
**Status:** READY FOR IMPLEMENTATION (Phase 1)
**Source:** Forensic Analysis of `LightwaveOS_Temp_Files` (Reference) vs `firmware/K1.8encoderS3` (Active)

## CEIB.0 Executive Summary
*   **Critical Breakage Confirmed**: The active `firmware/K1.8encoderS3` Node implementation fails to process WebSocket control commands due to a schema mismatch (F-1) and missing `ZoneComposer` initialization (F-2).
*   **Reference Code Identified**: `LightwaveOS_Temp_Files` contains a "Reference" implementation (likely the intended Refactor state) that correctly handles the schema and initializes the zone engine.
*   **Parity Gap**: The Active Node expects `type="effects.changed"` (incorrect) while the Hub sends `effects.setCurrent`. The Active Node also ignores the Legacy `t` key.
*   **Recovery Strategy**: Port the `WsMessageRouter` and `main.cpp` logic from the Reference (`Temp_Files`) to the Active (`firmware/`) codebase to restore Legacy Parity.

## CEIB.1 Legacy Behaviour Spec (Oracle)
*Derived from `LightwaveOS_Temp_Files` (Reference) and System Audit.*

### 1.1 WebSocket Protocol (Hub -> Node)
*   **Transport**: `ws://lightwaveos.local/ws`
*   **Schema Keys**: Must support **`t`** (Legacy) AND **`type`** (New).
*   **Commands**:
    *   `welcome` (Auth)
    *   `effects.setCurrent` (Effect Change)
    *   `parameters.set` (Parameter Update)
    *   `zones.update` (Zone Control)
    *   `state.snapshot` (Full State Sync)

### 1.2 Node Behavior
*   **Readiness**: Node becomes READY upon receiving `welcome`.
*   **Zone Engine**: `ZoneComposer` must be initialized and attached to the Renderer.
*   **Fallback**: UDP `LW_UDP_PARAM_DELTA` is accepted but should not be the primary control plane.

## CEIB.2 Current Behaviour Snapshot (Active Firmware)
*Evidence: `firmware/K1.8encoderS3/src/network/WsMessageRouter.cpp`*

*   **Schema Mismatch**:
    *   Checks for `type` key **ONLY** (ignores `t`).
    *   Expects `effects.changed` and `parameters.changed`.
    *   **RESULT**: Ignores Hub's `effects.setCurrent` and `parameters.set`.
*   **Feature Gap**:
    *   `ZoneComposer` is **missing** from `main.cpp` (Evidence: `firmware/K1.8encoderS3/src/main.cpp`).
    *   No routing logic for `zones.update`.

## CEIB.3 Parity Gap Map

| Feature | Legacy Oracle (Reference) | Current Active (Firmware) | Gap | Required Fix |
| :--- | :--- | :--- | :--- | :--- |
| **Schema Key** | `t` OR `type` | `type` ONLY | Node ignores Legacy `t` | Accept both keys |
| **Effect Cmd** | `effects.setCurrent` | `effects.changed` | Node ignores Hub cmd | Handle `setCurrent` |
| **Param Cmd** | `parameters.set` | `parameters.changed` | Node ignores Hub cmd | Handle `set` |
| **Zone Engine** | Initialized | Missing | Zones inoperable | Init `ZoneComposer` |
| **Zone Cmd** | `zones.update` | Missing | Node ignores Zone cmd | Add routing |

## CEIB.4 Compatibility Window Requirements
1.  **Bi-directional Schema**: Node MUST accept both Legacy (`t`) and New (`type`) keys to support mixed environments (Legacy Hub -> New Node, or New Hub -> New Node with Legacy fallback).
2.  **Command Aliasing**: Node SHOULD map `effects.changed` (if used by any legacy version) to the same handler as `effects.setCurrent`.

## CEIB.5 Test Strategy Requirements
1.  **Golden Trace Replay**: Inject `{"t": "effects.setCurrent", ...}` via Serial/WS-Mock and verify effect change.
2.  **Zone Activation**: Inject `{"t": "zones.update", ...}` and verify `ZoneComposer` active state.
3.  **Disconnect Recovery**: Verify Node reconnects and accepts `state.snapshot` correctly.

## CEIB.6 Deterministic Phased Recovery Plan
*   **Phase 1**: Fix `WsMessageRouter` schema parsing (Accept `t`/`type`, fix command strings).
*   **Phase 2**: Restore `ZoneComposer` in `main.cpp` and `WsMessageRouter`.
*   **Phase 3**: Verify Parity and cleanup UDP dependency.

## CEIB.7 Task Skeleton
1.  `[P0] Refactor WsMessageRouter.cpp` to match Reference logic (dual keys, correct commands).
2.  `[P0] Update main.cpp` to initialize `ZoneComposer`.
3.  `[P1] Update WsMessageRouter.cpp` to route `zones.update`.
4.  `[P2] Add Silent Failure Logging` for unknown message types.

## CEIB.8 "No Silent Failure" Logging
*   Node must log `[WS] Unknown type: <val>` for unhandled messages.
*   Node must log `[WS] Parse error` for malformed JSON.
