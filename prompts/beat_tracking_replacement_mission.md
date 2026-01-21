# Mission: Operation Heartbeat Transplant (Phase 2 & 3)

## 1. Executive Summary
**Objective:** Validate and finalize the replacement of the legacy `TempoTracker` with the newly implemented `EmotiscopeEngine` (v2.0 Hybrid Architecture).
**Status:**
*   **Phase 1 (Implementation):** COMPLETE. `EmotiscopeEngine` is implemented and running alongside `TempoTracker` in `AudioNode.cpp`.
*   **Current State:** Both engines are active. `AudioNode` logs a comparison of their outputs. `ControlBus` is still driven by `TempoTracker`.

**Target Architecture:** Emotiscope v2.0 Core (Dynamic Scaling, Silent Suppression, Quartic Scaling) + Emotiscope v1.1 Hybrid Input (Novelty + VU).

## 2. Resource Inventory
*   **New System (Implemented):**
    *   `firmware/v2/src/audio/tempo/EmotiscopeEngine.h` (v2.0 Hybrid Design)
    *   `firmware/v2/src/audio/tempo/EmotiscopeEngine.cpp`
*   **Integration Point:**
    *   `firmware/v2/src/audio/AudioNode.cpp` (Both engines initialized and updated)
*   **Victim (Legacy):**
    *   `firmware/v2/src/audio/tempo/TempoTracker.cpp`
    *   `firmware/v2/src/audio/tempo/TempoTracker.h`

## 3. Swarm Directives

### Agent 1: Quality Assurance (Validation)
**Goal:** Verify the `EmotiscopeEngine` behaves correctly before making it the primary driver.
*   **Tasks:**
    1.  Review `firmware/v2/src/audio/tempo/EmotiscopeEngine.cpp` for any remaining logic errors (specifically in `updateTempo` phase unwrapping and `calculateScaleFactors`).
    2.  Verify `AudioNode.cpp` passes the correct `raw.bins64` data to `EmotiscopeEngine`.
    3.  **Action:** Compile and flash. Monitor logs for `BeatCompare:` lines.
        *   *Expected:* `V2` BPM should be stable and match music. `Legacy` might be erratic.
        *   *Check:* If `V2` confidence stays 0, check `goertzel64Triggered` logic in `AudioNode.cpp`.

### Agent 2: The Switch (Hot-Swap)
**Goal:** Make `EmotiscopeEngine` the authority.
*   **Tasks:**
    1.  Modify `AudioNode.cpp`:
        *   Update `m_lastTempoOutput` to use `m_emotiscope.getOutput()` instead of `m_tempo.getOutput()`.
        *   Ensure `ControlBus` receives the new beat data.
    2.  **Constraint:** Do NOT remove `TempoTracker` yet. Keep it running in background for one more validation cycle if needed, or disable its update to save CPU.
    3.  **CPU Optimization:** If CPU usage is high, disable `m_tempo.updateTempo()` calls but keep the object for safety until Phase 3.

### Agent 3: Cleanup Crew (Deprecation)
**Goal:** Remove the legacy system.
*   **Tasks:**
    1.  Once Agent 2 is confirmed stable:
        *   Remove `TempoTracker m_tempo;` from `AudioNode.h`.
        *   Remove all `m_tempo` calls from `AudioNode.cpp`.
        *   Delete `firmware/v2/src/audio/tempo/TempoTracker.cpp` and `.h`.
    2.  Scan codebase for any other includes of `TempoTracker.h` and replace with `EmotiscopeEngine.h` (or remove if unused).

## 4. Execution Prompt (For Next Steps)
"Phase 1 is complete. `EmotiscopeEngine` is live. Proceed immediately to Phase 2: Switch the `AudioNode` output to use `m_emotiscope` as the source of truth for `m_lastTempoOutput`. Monitor the `BeatCompare` logs to ensure the new engine is performing as expected (stable BPM, high confidence on beats). If stable, proceed to Phase 3: Delete the legacy `TempoTracker` files and clean up the code."
