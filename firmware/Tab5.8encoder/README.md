# Tab5.8encoder - M5Stack Tab5 Encoder Firmware

Firmware for M5Stack Tab5 (ESP32-P4) controlling an M5Stack ROTATE8 encoder unit via Grove Port.A.

## Hardware Setup

- **Controller**: M5Stack Tab5 (ESP32-P4)
- **Encoder**: M5Stack ROTATE8 (I2C address 0x41)
- **Connection**: Grove Port.A only (SDA=GPIO53, SCL=GPIO54)
- **No custom wiring required** - use standard Grove cable

## Critical Constraints

### I2C Bus Usage

- **MUST use `M5.Ex_I2C`** for Grove Port.A - never use global `Wire`
- Tab5 exposes external I2C as `M5.Ex_I2C` which maps to GPIO53/54
- Initialise with: `M5.Ex_I2C.begin()`

### Forbidden Patterns

The following symbols are **forbidden** and will cause hardware issues on Tab5:

- `periph_module_reset`
- `i2cDeinit0` or `i2cDeinit(0)`
- `PERIPH_I2C0_MODULE`
- Hardware-level I2C peripheral resets

These patterns work on AtomS3 but will break Tab5. All I2C recovery code using these patterns has been removed.

## Development Milestones

### Milestone A: Boot + Ex_I2C Scan
- Tab5 boots successfully
- `M5.Ex_I2C` initialised
- I2C scan finds ROTATE8 at 0x41

### Milestone B: ROTATE8 Comms + LED Proof
- Transport layer communicates with ROTATE8
- Encoder deltas print when knob turns
- LED lights on boot (proof-of-life)

### Milestone C: Processing Port
- Encoder processing logic ported (detent debounce, wrapping, clamping)
- Hardware-agnostic implementation
- No I2C or hardware dependencies in processing layer

### Milestone D: Service Integration
- EncoderService polls 8 channels
- Events emitted via callback interface
- Non-blocking, responsive loop

### Milestone E: Control Plane (Optional)
- WiFi + WebSocket integration
- Parameter synchronisation with LightwaveOS
- Bidirectional parameter sync

## Building

### Arduino IDE (Primary - Recommended)

1. Install ESP32 board support:
   - Board Manager: Search "ESP32" and install "esp32" by Espressif Systems
   - Ensure ESP32-P4 support is included (may require latest version)

2. Install required libraries via Library Manager:
   - **M5Unified** (by M5Stack)
   - **M5ROTATE8** (by Rob Tillaart)
   - **ArduinoJson** (by Benoit Blanchon) - version 7.x
   - **WebSockets** (by Markus Sattler) - version 2.4.x

3. Select board:
   - Tools → Board → ESP32 Arduino → **M5Stack Tab5** (or equivalent ESP32-P4 board)

4. Select port and upload

### PlatformIO (Best-Effort)

PlatformIO support for ESP32-P4/Tab5 may be incomplete. If you encounter issues, fall back to Arduino IDE.

1. Install PlatformIO Core or PlatformIO IDE extension

2. Build from project directory:
   ```bash
   cd firmware/Tab5.8encoder
   pio run
   ```

3. If board ID is unknown:
   ```bash
   pio boards | grep -i tab5
   pio boards | grep -i esp32-p4
   ```
   Update `platformio.ini` with the correct board ID.

4. **If PlatformIO fails**: Use Arduino IDE instead. Document the issue in `docs/known_gotchas.md`.

## Troubleshooting

### I2C Probe Timeout

If you see "probe timeout" errors or the ROTATE8 is not detected:

1. **Check wiring**:
   - Ensure ROTATE8 is connected to Grove Port.A only
   - Verify cable is fully seated
   - Try a different Grove cable

2. **Check power**:
   - Ensure Tab5 is powered adequately
   - ROTATE8 requires stable power

3. **Check I2C configuration**:
   - Verify `M5.Ex_I2C.begin()` is called (not `Wire.begin()`)
   - Check I2C speed/timeout settings in `Config.h`

4. **Check device**:
   - Verify ROTATE8 is functional (test on another device if possible)
   - Check I2C address (should be 0x41)

5. **Check port**:
   - Ensure using Grove Port.A (GPIO53/54)
   - Do not use other ports

### Build Issues

- **PlatformIO board not found**: Fall back to Arduino IDE
- **Library conflicts**: Ensure using correct versions (see Arduino IDE section)
- **Upload failures**: Check USB cable, port selection, boot mode

## Project Structure

```
firmware/Tab5.8encoder/
├── src/
│   ├── main.cpp              # Main application (milestones A-D)
│   ├── i2c/
│   │   └── I2CScan.*         # I2C scanning utility
│   ├── transport/
│   │   └── Rotate8Transport.* # Bus-safe ROTATE8 wrapper
│   ├── processing/
│   │   └── EncoderProcessing.* # Hardware-agnostic processing
│   ├── service/
│   │   └── EncoderService.*   # Poll + process + events
│   ├── network/              # Milestone E (optional)
│   │   ├── WiFiManager.*
│   │   ├── WebSocketClient.*
│   │   ├── WsMessageRouter.*
│   │   ├── ParameterHandler.*
│   │   └── ParameterMap.*
│   └── config/
│       └── Config.h           # Configuration constants
├── platformio.ini            # PlatformIO configuration
├── scripts/
│   ├── clone_refs.sh         # Clone reference repositories
│   └── pio_boards_hint.md    # PlatformIO board discovery hints
├── docs/
│   └── known_gotchas.md      # Tab5-specific gotchas
├── _ref/                     # Reference repos (clone via scripts/clone_refs.sh)
└── README.md                 # This file
```

## Reference Repositories

Reference repositories are cloned to `_ref/` for pattern examination. To clone them:

```bash
cd firmware/Tab5.8encoder
./scripts/clone_refs.sh
```

This will clone:
- `M5Unified/` - Tab5 mapping and Ex_I2C behaviour
- `M5Tab5-UserDemo/` - Factory demo showing Tab5 HAL/components
- `esp-bsp/` - Tab5 BSP reference (pin/bus truth)
- `xiaozhi-esp32/` - Real-world Tab5 support patterns

## Code Style

- **British English** in comments and documentation (centre, colour, initialise, etc.)
- **Centre-origin effects** (if porting effect selection logic)
- **No heap allocation** in render/hot paths
- **Hardware-agnostic** processing layer (no I2C/GPIO dependencies)

## License

See parent repository for license information.

