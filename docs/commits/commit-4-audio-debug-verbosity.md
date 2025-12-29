# Commit 4: feat(audio): Add runtime-configurable audio debug verbosity

## Original Problem/Requirement

Audio components generate high-frequency debug output (DMA @ 62.5Hz, Goertzel @ 100Hz) that can flood serial output during development. Previous approach:
- Hard-coded log intervals (e.g., every 50 hops, every 620 ticks)
- No way to adjust verbosity without recompiling
- All-or-nothing debug output (either all logs or none)

Need for tiered, runtime-configurable verbosity:
- Level 0: Off (production)
- Level 1: Errors only
- Level 2: Status (10-second health checks)
- Level 3: + DMA diagnostics (~5s interval)
- Level 4: + 64-bin Goertzel (~2s interval)
- Level 5: + 8-band Goertzel (~1s interval)

## Solution Design Rationale

**AudioDebugConfig System:**
- Singleton configuration struct (`AudioDebugConfig`)
- Runtime-accessible via `getAudioDebugConfig()` function
- Tiered verbosity levels (0-5) with derived intervals
- Base interval configurable via CLI (default: 62 frames ≈ 1 second)

**Verbosity Gating:**
- All audio debug logs check `dbgCfg.verbosity >= threshold` before printing
- Thresholds: Status=2, DMA=3, 64-bin=4, 8-band=5
- Intervals calculated from base interval (DMA=5x, 64-bin=0.5x, 8-band=1x)

**CLI Integration:**
- `adbg` command: Show current level and intervals
- `adbg <0-5>`: Set verbosity level
- `adbg interval <N>`: Set base interval in frames

**Implementation:**
- `AudioDebugConfig.h/cpp`: Configuration singleton
- `AudioActor.cpp`: Verbosity gating for periodic logs
- `AudioCapture.cpp`: Verbosity gating for DMA diagnostics
- `main.cpp`: CLI command handlers

## Technical Considerations

**Performance:**
- Minimal overhead: Single struct access per log check
- No string formatting when logs are disabled
- Intervals calculated once per log call (acceptable for low-frequency checks)

**Memory:**
- 2 bytes per component (verbosity + baseInterval)
- Singleton pattern (no per-instance overhead)

**Runtime Flexibility:**
- Can adjust verbosity without recompiling
- Useful for production debugging (set level 2 for status, level 5 for deep dive)
- Intervals can be tuned for different serial speeds

**Integration:**
- Works with unified logging system (Commit 1)
- Complements compile-time `LW_LOG_LEVEL` (runtime override)
- Does not interfere with error logs (always shown)

## Dependencies

**This commit depends on:**
- Commit 1 (unified logging system)
- Commits 2-3 (audio component logging migration)

**This commit enables:**
- Better development experience (controlled verbosity)
- Production debugging (adjustable without rebuild)

## Testing Methodology

**Build Verification:**
```bash
pio run -e esp32dev_audio
```

**Runtime Verification:**
1. Flash firmware and connect serial monitor
2. Test CLI commands:
   ```
   adbg              # Show current settings
   adbg 0            # Disable all debug output
   adbg 2            # Status only (10s intervals)
   adbg 5            # Full verbosity (1s intervals)
   adbg interval 30  # Set base interval to 30 frames
   ```
3. Verify log frequency changes with verbosity level
4. Verify intervals scale correctly (DMA=5x, 64-bin=0.5x, 8-band=1x)

**Expected Behavior:**
- Level 0: No debug output (errors still shown)
- Level 2: Periodic "Audio alive" messages every ~10 seconds
- Level 3: + DMA diagnostics every ~5 seconds
- Level 4: + 64-bin Goertzel every ~2 seconds
- Level 5: + 8-band Goertzel every ~1 second

## Known Limitations

1. **Per-component configuration:** Currently one global config. Future enhancement could support per-component verbosity.

2. **No persistence:** Verbosity resets on reboot. Future enhancement could save to NVS.

3. **Interval granularity:** Intervals are frame-based, not time-based. At 62.5Hz, 62 frames ≈ 1 second, but exact timing depends on actual frame rate.

4. **No WebSocket control:** CLI-only. Future enhancement could expose via REST API.

## Migration Notes

**Breaking Changes:**
- None (additive feature)

**Configuration:**
- Default verbosity: 4 (Medium - includes 64-bin Goertzel)
- Default base interval: 62 frames (~1 second at 62.5Hz)

**Usage:**
```cpp
// In audio processing code:
auto& dbgCfg = getAudioDebugConfig();
if (dbgCfg.verbosity >= 3 && (counter % dbgCfg.intervalDMA()) == 0) {
    LW_LOGI("DMA diagnostic: ...");
}
```

## Code References

- Configuration: `v2/src/audio/AudioDebugConfig.h`, `v2/src/audio/AudioDebugConfig.cpp`
- Integration: `v2/src/audio/AudioActor.cpp` (lines 239, 605, 721), `v2/src/audio/AudioCapture.cpp` (line 193)
- CLI: `v2/src/main.cpp` (lines 258-290)

