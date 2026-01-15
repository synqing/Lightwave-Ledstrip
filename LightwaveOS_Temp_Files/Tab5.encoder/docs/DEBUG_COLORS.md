# Tab5.encoder Debug Log Color Scheme

All debug log categories in the Tab5.encoder firmware have been colorized for better readability during serial monitoring.

## Color Assignments

| Category | Color | Usage |
|----------|-------|-------|
| `[WDT]` | Bright Red | Watchdog timer events |
| `[INIT]` | Bright Green | System initialization |
| `[OK]` | Bright Green | Success markers |
| `[WiFi]` | Cyan | WiFi operations |
| `[NETWORK]` | Blue | Network layer events |
| `[WS]` | Bright Blue | WebSocket communications |
| `[NVS]` | Magenta | Non-volatile storage |
| `[PRESET]` | Bright Magenta | Preset management |
| `[UI]` | Yellow | UI operations |
| `[TOUCH]` | Yellow | Touch events |
| `[Button]` | Blue | Button events |
| `[LED]` | Green | LED feedback |
| `[CoarseMode]` | Bright Cyan | Coarse mode switching |
| `[I2C_RECOVERY]` | Bright Red | I2C recovery operations |
| `[OTA]` | Bright Magenta | Over-the-air updates |
| `[ZoneComposer]` | Yellow | Zone composition |
| `[DEBUG]` | Bright Black (Dim) | Debug traces |
| `[STATUS]` | Cyan | Status reports |
| `[LOOP_TRACE]` | Dim | Main loop traces |
| `[CT_TRACE]` | Dim | Coarse tracking trace |
| `[DisplayUI_TRACE]` | Dim | DisplayUI trace |
| `[ZC_TRACE]` | Dim | ZoneComposer trace |

## Implementation

Colors are defined in `src/config/AnsiColors.h` using ANSI escape codes. The colorization is automatically applied during compilation and requires no runtime overhead.

### Example Output

```
[WiFi] Starting connection...
[NETWORK] WiFi connected! IP: 192.168.1.100
[WS] Connecting to ws://192.168.1.50:8080/ws...
[OK] Both units detected - 16 encoders available!
```

Each category appears in its assigned color for easy visual parsing of serial output.

## Terminal Compatibility

ANSI color codes are supported by:
- PlatformIO Serial Monitor
- Arduino Serial Monitor (2.0+)
- Most terminal emulators (iTerm2, Terminal.app, etc.)
- Screen/tmux sessions

If colors don't appear correctly, ensure your terminal supports ANSI escape sequences.

## Modifying Colors

To change a color assignment:
1. Edit `src/config/AnsiColors.h`
2. Update the `COLOR_XXX` definition
3. Rebuild the firmware

No source code changes are required - all log statements automatically use the color definitions.
