# Tab5.encoder

M5Stack Tab5 (ESP32-P4) firmware for the M5ROTATE8 8-encoder unit. Controls LightwaveOS LED strip parameters via WebSocket.

## Hardware Requirements

- **M5Stack Tab5** (ESP32-P4 based)
- **M5ROTATE8** 8-encoder unit
- **Grove Cable** connecting ROTATE8 to Tab5's Grove Port.A

## Wiring

Connect the M5ROTATE8 to the Tab5's **Grove Port.A** (the external I2C port):

```
Tab5 Grove Port.A    M5ROTATE8
─────────────────    ─────────
GND (black)     ──── GND
5V  (red)       ──── 5V
SDA (yellow)    ──── SDA (GPIO 53)
SCL (white)     ──── SCL (GPIO 54)
```

**Important:** Use Grove Port.A specifically. This is the external I2C bus that's isolated from Tab5's internal peripherals (display, touch, audio).

## I2C Configuration

| Parameter | Value |
|-----------|-------|
| SDA Pin | GPIO 53 |
| SCL Pin | GPIO 54 |
| M5ROTATE8 Address | 0x41 |
| Frequency | 100 kHz |
| Timeout | 200 ms |

## Build Instructions

### Option 1: Arduino IDE (Recommended)

Arduino IDE is the guaranteed development path for Tab5.

1. **Add Board Manager URL:**
   - File → Preferences → Additional Board Manager URLs
   - Add: `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`

2. **Install Board:**
   - Tools → Board → Board Manager
   - Search "M5Stack", install latest
   - Select: Tools → Board → **M5Tab5**

3. **Install Libraries:**
   - Sketch → Include Library → Manage Libraries
   - Install:
     - M5Unified (v0.2.7+)
     - M5GFX (v0.2.8+)
     - M5ROTATE8 (v0.4.1+)
     - ArduinoJson (v7.0.0+)
     - WebSockets (v2.4.0+)

4. **Build & Upload:**
   - Connect Tab5 via USB-C
   - Select port (Tools → Port)
   - Click Upload

### Option 2: PlatformIO (Experimental)

PlatformIO support for ESP32-P4 is experimental and uses a community fork.

```bash
# From the Tab5.encoder directory:
pio run -e tab5

# Upload:
pio run -e tab5 -t upload

# Monitor serial:
pio device monitor -b 115200
```

**Note:** If PlatformIO fails, fall back to Arduino IDE.

## WiFi Configuration

For network features (Milestone E):

1. Copy `wifi_credentials.ini.template` to `wifi_credentials.ini`
2. Edit with your network credentials:
   ```ini
   [wifi_credentials]
   build_flags =
       -DWIFI_SSID=\"YourNetworkName\"
       -DWIFI_PASSWORD=\"YourNetworkPassword\"
   ```
3. The file is gitignored - your credentials stay private

## Expected Serial Output

### Milestone A (I2C Scan)

```
============================================
  Tab5.encoder - Milestone A
  Boot + Port.A I2C Proof
============================================

[INIT] M5Stack Tab5 initialized
[INIT] Tab5 External I2C pins - SDA:53 SCL:54
[INIT] Wire initialized at 100000 Hz, timeout 200 ms

=== Scanning External I2C (Grove Port.A) ===
  Found device at 0x41 (M5ROTATE8)
  Total: 1 device(s)

[OK] M5ROTATE8 detected at 0x41
[OK] Ready for Milestone B: ROTATE8 bring-up

============================================
  Setup complete
============================================

[STATUS] M5ROTATE8: OK | heap:XXXXXX
```

### Milestone D (Full Encoder Service)

```
=== Tab5.encoder Starting ===
Tab5 Ex_I2C - SDA:53 SCL:54
Found M5ROTATE8 at 0x41
EncoderService initialized
Encoder 0: 128 → 129
Encoder 0: 129 → 130
Encoder 0 (button): reset to 128
[Status] 8Enc:OK heap:XXXXXX
```

## Troubleshooting

### "M5ROTATE8 not found"

1. **Check wiring:** Is the Grove cable connected to Port.A?
2. **Check cable orientation:** Grove connectors are keyed but verify colors match
3. **Power:** Is the ROTATE8 getting power? LEDs should briefly flash on boot
4. **I2C address:** Default is 0x41. If changed, update `Config.h`

### "probe device timeout" Error

1. Try lower I2C frequency: Edit `I2C::FREQ_HZ` in `Config.h` to 50000
2. Check for loose connections
3. Ensure no other device is using the same I2C address

### Display Not Working

The Tab5 has hardware revisions with different display ICs:
- Older units: ILI9881C
- Newer units: ST7123

M5Unified v0.2.7+ auto-detects both. If display issues persist, update M5Unified and M5GFX libraries.

## Safety Notes

This firmware does **NOT** use aggressive I2C recovery patterns that could break Tab5's internal peripherals:

- **No** `periph_module_reset(PERIPH_I2C0_MODULE)`
- **No** `i2cDeinit(0)`
- **No** aggressive bus clear pin twiddling

The external I2C on Grove Port.A is isolated from the internal bus, but we maintain safe practices for system stability.

## Project Structure

```
firmware/Tab5.encoder/
├── platformio.ini              # PlatformIO build config (experimental)
├── README.md                   # This file
├── wifi_credentials.ini.template
└── src/
    ├── main.cpp               # Entry point (197 lines)
    ├── config/
    │   └── Config.h           # I2C pins, parameter definitions (157 lines)
    └── input/
        ├── Rotate8Transport.h # Clean M5ROTATE8 wrapper (224 lines)
        ├── EncoderProcessing.h # Detent debounce, wrap/clamp (253 lines)
        └── EncoderService.h   # Full 8-channel service (310 lines)
```

**Total:** ~1,343 lines of new code

## Milestones

- [x] **A:** Boot + I2C scan proof
- [x] **B:** ROTATE8 basic read/write (Rotate8Transport.h)
- [x] **C:** Port encoder processing logic (EncoderProcessing.h)
- [x] **D:** Full EncoderService with callbacks (EncoderService.h)
- [ ] **D.5:** Display UI + LED feedback (basic UI works, full module optional)
- [ ] **E:** Network control plane (WiFi + WebSocket)

## Features

- **8 Rotary Encoders:** Each controls a LightwaveOS parameter
- **Detent Debouncing:** Normalizes M5ROTATE8's step-size quirk (2 per detent)
- **Button Reset:** Press encoder button to reset to default value
- **LED Feedback:** Green flash on turn, cyan flash on reset
- **Status Display:** Tab5's 5" screen shows all 8 parameter values
- **Wrap/Clamp:** Effect and Palette wrap around, others clamp to range

## Parameters

| Encoder | Parameter   | Range   | Default |
|---------|-------------|---------|---------|
| 0       | Effect      | 0-95    | 0       |
| 1       | Brightness  | 0-255   | 128     |
| 2       | Palette     | 0-63    | 0       |
| 3       | Speed       | 1-100   | 25      |
| 4       | Intensity   | 0-255   | 128     |
| 5       | Saturation  | 0-255   | 255     |
| 6       | Complexity  | 0-255   | 128     |
| 7       | Variation   | 0-255   | 0       |

## License

Part of the LightwaveOS project.
