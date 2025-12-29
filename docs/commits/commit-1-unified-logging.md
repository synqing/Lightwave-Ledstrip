# Commit 1: feat(logging): Add unified logging system with colored output

## Original Problem/Requirement

The codebase previously used inconsistent logging approaches:
- ESP-IDF `ESP_LOG*` macros in some components
- Direct `Serial.println()` calls in others
- Inline ANSI color codes scattered throughout
- No compile-time log level control
- No consistent timestamp or component tagging

This made debugging difficult, especially when:
- Filtering logs by component
- Controlling verbosity for production builds
- Maintaining consistent formatting across modules
- Supporting native builds (unit tests) without ESP-IDF

## Solution Design Rationale

**Unified Logging System (`utils/Log.h`):**
- Single header file providing `LW_LOGI`, `LW_LOGE`, `LW_LOGW`, `LW_LOGD` macros
- Automatic timestamps using `millis()` (Arduino) or mock timer (native)
- Component tags via `LW_LOG_TAG` define
- Compile-time log level filtering via `LW_LOG_LEVEL` build flag
- Preserves existing ANSI color conventions (green for effects, yellow for hardware, cyan for audio)

**Build Configuration:**
- `LW_LOG_LEVEL` flag in `platformio.ini` (0=None, 1=Error, 2=Warn, 3=Info, 4=Debug)
- Default: Level 3 (Info) for debug builds, Level 2 (Warn) for release
- Per-environment overrides for verbose debugging

**Feature Flag:**
- `FEATURE_UNIFIED_LOGGING` in `features.h` (default: enabled)
- Allows disabling if needed for minimal builds

## Technical Considerations

**Performance:**
- Zero overhead when logs are disabled (compiler eliminates dead code)
- `LW_LOG_LEVEL` compile-time filtering prevents string formatting in disabled paths
- No runtime overhead for log level checks

**Memory:**
- Header-only implementation (no runtime memory allocation)
- Format strings stored in flash (Arduino) or code section
- No dynamic string concatenation

**Platform Support:**
- Arduino/ESP-IDF: Uses `Serial.printf()` and `millis()`
- Native builds: Uses `printf()` and mock timer for unit tests
- Conditional compilation handles platform differences

**Color Preservation:**
- Existing color codes preserved as constants (`LW_CLR_GREEN`, `LW_CLR_YELLOW`, etc.)
- Title-only coloring pattern maintained for high-frequency logs
- Backward compatible with existing inline ANSI sequences

## Dependencies

**This commit provides:**
- Foundation for all subsequent logging migrations
- Required by: Commits 2-10 (all logging migration commits)

**This commit depends on:**
- None (foundational infrastructure)

## Testing Methodology

**Build Verification:**
```bash
# Default build (should compile with LW_LOG_LEVEL=3)
pio run -e esp32dev

# Debug build (should compile with LW_LOG_LEVEL=4)
pio run -e esp32dev_debug

# Native build (should compile with mock timer)
pio run -e native
```

**Runtime Verification:**
1. Flash firmware and monitor serial output
2. Verify log format: `[timestamp][LEVEL][TAG] message`
3. Verify colors appear correctly in terminal
4. Test log level filtering by changing `LW_LOG_LEVEL` and rebuilding

**Expected Output:**
```
[1234][INFO][Main] Initializing Actor System...
[1235][WARN][WiFi] Connection timeout
[1236][ERROR][Audio] Failed to initialize audio capture
[1237][DEBUG][Renderer] Registered effect 0: Fire
```

## Known Limitations

1. **No runtime log level changes:** Log level is compile-time only. Runtime verbosity control requires separate mechanism (see Commit 4 for audio-specific solution).

2. **Single output stream:** All logs go to Serial. Future enhancement could support multiple sinks (file, network, etc.).

3. **No log buffering:** Logs are printed immediately. High-frequency logging could impact real-time performance (mitigated by compile-time filtering).

4. **Color support:** Requires ANSI-capable terminal. Non-ANSI terminals will show escape sequences (acceptable for development).

## Migration Notes

**Breaking Changes:**
- None (new infrastructure, no existing code modified yet)

**Configuration Changes:**
- `platformio.ini` now includes `LW_LOG_LEVEL` flags per environment
- Default log level is 3 (Info) for debug builds
- Production builds should use level 2 (Warn) or lower

**Future Migrations:**
- Components will migrate from `ESP_LOG*`/`Serial.println()` to `LW_LOG*` in subsequent commits
- Existing inline color codes can remain or migrate to `LW_CLR_*` constants

## Code References

- Implementation: `v2/src/utils/Log.h`
- Build config: `v2/platformio.ini` (lines 44-46, 80-81, 89-90, 135-136)
- Feature flag: `v2/src/config/features.h` (lines 151-156)

