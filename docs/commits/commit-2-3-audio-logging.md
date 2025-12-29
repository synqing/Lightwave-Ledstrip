# Commits 2-3: refactor(logging): Migrate audio components to unified logging system

## Original Problem/Requirement

Audio components (`AudioActor` and `AudioCapture`) used ESP-IDF `ESP_LOG*` macros, which:
- Required ESP-IDF headers even in native builds
- Had inconsistent formatting with other components
- Could not be filtered at compile time
- Mixed logging concerns with platform-specific code

## Solution Design Rationale

**Migration Strategy:**
- Replace all `ESP_LOG*` calls with `LW_LOG*` equivalents
- Preserve existing colored output conventions:
  - Yellow for hardware/DMA diagnostics
  - Cyan for spectral analysis
  - Green for status messages
- Maintain log frequency and verbosity (no behavior changes)
- Use `LW_LOG_TAG "Audio"` and `LW_LOG_TAG "AudioCapture"` for component identification

**Code Changes:**
- Remove `#include <esp_log.h>` and `static const char* TAG` declarations
- Replace `ESP_LOGI(TAG, ...)` with `LW_LOGI(...)`
- Replace `ESP_LOGE(TAG, ...)` with `LW_LOGE(...)`
- Replace `ESP_LOGW(TAG, ...)` with `LW_LOGW(...)`
- Replace `ESP_LOGD(TAG, ...)` with `LW_LOGD(...)`
- Preserve inline ANSI color codes using `LW_CLR_*` constants

## Technical Considerations

**Performance:**
- No performance impact (same log frequency, same formatting)
- Compile-time log level filtering now available
- Native builds no longer require ESP-IDF logging stubs

**Memory:**
- Slight reduction (removed `TAG` string constants)
- Format strings still in flash/code section

**Cross-Platform:**
- Native builds now work without ESP-IDF logging headers
- Consistent behavior across Arduino and native platforms

**Color Preservation:**
- All existing color conventions maintained
- High-frequency logs (DMA @ 62.5Hz) use title-only coloring
- Spectral analysis logs use cyan coloring

## Dependencies

**These commits depend on:**
- Commit 1 (unified logging system)

**These commits enable:**
- Commit 4 (audio debug verbosity - uses unified logging)

## Testing Methodology

**Build Verification:**
```bash
# Should compile without ESP-IDF logging dependencies
pio run -e esp32dev
pio run -e native
```

**Runtime Verification:**
1. Flash firmware and monitor serial output
2. Verify audio initialization logs appear:
   ```
   [1234][INFO][Audio] AudioActor starting on Core 0
   [1235][INFO][AudioCapture] I2S initialized successfully
   ```
3. Verify colored output matches previous behavior
4. Test error conditions (disconnect mic) and verify error logs

**Expected Output:**
- AudioActor: Initialization, state changes, periodic health checks
- AudioCapture: I2S setup, DMA diagnostics, read errors
- Colors: Yellow for DMA, cyan for spectral, green for status

## Known Limitations

1. **Log frequency unchanged:** High-frequency logs (DMA, Goertzel) still print at same rate. Runtime verbosity control added in Commit 4.

2. **No log aggregation:** Each component logs independently. Future enhancement could aggregate related logs.

## Migration Notes

**Breaking Changes:**
- None (behavior-preserving refactor)

**Code Changes:**
- `AudioActor.cpp`: 120 lines changed (ESP_LOG* → LW_LOG*)
- `AudioActor.h`: No logging changes (header file)
- `AudioCapture.cpp`: 69 lines changed (ESP_LOG* → LW_LOG*)

**Future Enhancements:**
- Commit 4 will add runtime verbosity control for audio logs
- Commit 5 will add K1 debug infrastructure (uses unified logging)

## Code References

- AudioActor: `v2/src/audio/AudioActor.cpp`, `v2/src/audio/AudioActor.h`
- AudioCapture: `v2/src/audio/AudioCapture.cpp`

