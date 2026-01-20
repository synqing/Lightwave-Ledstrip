# Tab5.encoder

M5Stack Tab5 (ESP32-P4) firmware with professional LVGL 9.3.0 UI for controlling LightwaveOS LED strip parameters. Features dual M5ROTATE8 encoder units (16 encoders total), 8-bank preset system, and WebSocket synchronization.

## Hardware Requirements

- **M5Stack Tab5** (ESP32-P4 RISC-V based, 360MHz, 500KB RAM, 16MB flash)
- **Two M5ROTATE8** 8-encoder units
- **Grove Cable** or **Grove Hub** for connecting both units to Tab5's Grove Port.A

## Hardware Architecture

```
M5Stack Tab5 (ESP32-P4)
├── 5" Display (800x480, ILI9881C/ST7123)
├── ESP32-C6 WiFi Co-processor (SDIO)
├── Grove Port.A (External I2C)
│   ├── Unit A @ 0x42 (Encoders 0-7)  → Global Parameters
│   └── Unit B @ 0x41 (Encoders 8-15) → Preset Management
└── WebSocket Client → LightwaveOS v2
```

## Wiring

Connect **BOTH** M5ROTATE8 units to Tab5's **Grove Port.A** using a Grove hub or daisy-chain:

```
Tab5 Grove Port.A         Grove Hub              M5ROTATE8 Units
─────────────────────────────────────────────────────────────────
GND (black)     ──────┬─── Hub ───┬──── Unit A @ 0x42 (GND)
5V  (red)       ──────├─── Hub ───┼──── Unit A @ 0x42 (5V)
SDA (yellow)    ──────├─── Hub ───┼──── Unit A @ 0x42 (SDA)
SCL (white)     ──────├─── Hub ───┼──── Unit A @ 0x42 (SCL)
                      │           │
                      ├─── Hub ───┼──── Unit B @ 0x41 (GND)
                      ├─── Hub ───┼──── Unit B @ 0x41 (5V)
                      ├─── Hub ───┼──── Unit B @ 0x41 (SDA)
                      └─── Hub ───┴──── Unit B @ 0x41 (SCL)
```

**Important:**
- Use Grove Port.A specifically (external I2C bus isolated from internal peripherals)
- **Unit A must be reprogrammed to address 0x42** (register 0xFF)
- Unit B uses factory default address 0x41

## I2C Configuration

| Parameter | Value |
|-----------|-------|
| SDA Pin | GPIO 53 |
| SCL Pin | GPIO 54 |
| Unit A Address | 0x42 (reprogrammed) |
| Unit B Address | 0x41 (factory) |
| Frequency | 100 kHz |
| Timeout | 200 ms |

## Build Instructions

### ⚠️ CRITICAL BUILD REQUIREMENTS

The Tab5 uses **ESP32-P4 (RISC-V architecture)**. You MUST use the exact build commands below.

---

### Device Port Mapping

**CRITICAL: Device Port Assignments**
- **ESP32-S3 (v2 firmware)**: `/dev/cu.usbmodem1101`
- **Tab5 (encoder firmware)**: `/dev/cu.usbmodem101`

Always use these specific ports for uploads and monitoring.

---

### ⛔ FORBIDDEN - These Commands WILL FAIL:

```bash
# ❌ WRONG - Missing PATH isolation, will use wrong toolchain
pio run -e tab5 -d firmware/Tab5.encoder

# ❌ WRONG - Never cd into the directory
cd firmware/Tab5.encoder && pio run -e tab5

# ❌ WRONG - Never add hardcoded toolchain paths
PATH="...:$HOME/.platformio/packages/toolchain-riscv32-esp/..." pio run ...

# ❌ WRONG - Never specify wrong upload port (Tab5 is usbmodem101, NOT usbmodem1101)
pio run -e tab5 -t upload --upload-port /dev/cu.usbmodem1101
```

---

### ✅ CORRECT BUILD COMMANDS

**Run these from the REPOSITORY ROOT (`Lightwave-Ledstrip/`):**

```bash
# Build only:
PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin" pio run -e tab5 -d firmware/Tab5.encoder

# Build + Upload (Tab5 on usbmodem101):
PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin" pio run -e tab5 -t upload --upload-port /dev/cu.usbmodem101 -d firmware/Tab5.encoder

# Monitor serial output (Tab5 on usbmodem101):
pio device monitor -p /dev/cu.usbmodem101 -d firmware/Tab5.encoder -b 115200
```

---

### Why This Specific Command?

1. **`PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin"`** - Clean PATH. The pre-build hook (`scripts/pio_pre.py`) adds the RISC-V toolchain automatically.

2. **`-d firmware/Tab5.encoder`** - Tells PlatformIO where the project is WITHOUT changing directory.

3. **`--upload-port /dev/cu.usbmodem101`** - Tab5 device port (use explicit port to avoid uploading to wrong device).

4. **Pre-build hook** (`scripts/pio_pre.py`) automatically:
   - Finds the RISC-V toolchain (`toolchain-riscv32-esp`)
   - Prepends its `bin/` directory to PATH
   - Adds framework include directories for Network/Ethernet libraries

## WiFi Configuration

### Network Architecture

**Tab5.encoder (client) ↔ WebSocket ↔ LightwaveOS v2 (server)**

### Network Priority

1. **Primary Network (WIFI_SSID):** Your regular home/office WiFi
   - Tab5 attempts this first
   - Best performance when both devices are on the same network
   - Tab5 discovers v2 via mDNS (`lightwaveos.local`)

2. **Secondary Network (WIFI_SSID2):** v2 device's Access Point (`LightwaveOS-AP`)
   - **Automatic fallback** when primary WiFi unavailable
   - v2 device IP: `192.168.4.1` (SoftAP gateway)
   - Tab5 connects directly to v2's AP
   - Uses gateway IP fallback if mDNS fails

### Configuration Steps

1. Copy `wifi_credentials.ini.template` to `wifi_credentials.ini`
2. Edit with your network credentials:
   ```ini
   [wifi_credentials]
   build_flags =
       ; Primary WiFi (regular network - tried first)
       -DWIFI_SSID=\"YourHomeNetworkName\"
       -DWIFI_PASSWORD=\"YourHomeNetworkPassword\"

       ; Secondary WiFi (v2 device AP - automatic fallback)
       -DWIFI_SSID2=\"LightwaveOS-AP\"
       -DWIFI_PASSWORD2=\"SpectraSynq\"
   ```
3. The file is gitignored - your credentials stay private

### Connection Flow

```
Tab5 Boot → Try Primary WiFi → Connected? → Multi-tier Fallback → WebSocket
                ↓ Failed                       ↓
         Try Secondary WiFi → Connected → Fallback Strategy
```

### Network Configuration & Fallback Strategy

Tab5.encoder implements a **multi-tier fallback strategy** to ensure reliable WebSocket connection even when mDNS fails:

**Fallback Priority (in order):**

1. **Compile-time `LIGHTWAVE_IP`** (if defined in `network_config.h`)
   - Highest priority - bypasses mDNS entirely
   - Use for testing or when mDNS is unreliable
   - Uncomment and set: `#define LIGHTWAVE_IP "192.168.0.XXX"`

2. **Manual IP from NVS** (if configured via UI)
   - User-configured IP address stored in NVS
   - Persists across reboots
   - Configure via: Long-press on network status area → Network Configuration screen

3. **mDNS Resolution** (default)
   - Attempts to resolve `lightwaveos.local`
   - Retry interval: 10 seconds
   - Maximum attempts: 6 (60 seconds total)

4. **Timeout-based Fallback** (automatic)
   - Triggers after 60 seconds or 6 failed mDNS attempts
   - Uses manual IP from NVS (if set)
   - Falls back to gateway IP (if on secondary network: `LightwaveOS-AP`)

**Manual IP Configuration:**

1. **Via UI** (recommended):
   - Long-press on WiFi/WebSocket status area in header
   - Enter IP address (e.g., `192.168.0.XXX`)
   - Toggle "Use Manual IP" switch
   - Tap "Save"
   - WebSocket will reconnect with new IP

2. **Via Serial** (advanced):
   - Use NVS commands to set `tab5net.manual_ip` and `tab5net.use_manual`
   - Requires serial access and NVS manipulation

**Troubleshooting mDNS Failures:**

- **Symptom**: `[WiFi] mDNS resolution failed for lightwaveos.local`
- **Solution 1**: Configure manual IP via UI (see above)
- **Solution 2**: Uncomment `LIGHTWAVE_IP` in `network_config.h` and rebuild
- **Solution 3**: Connect to secondary network (`LightwaveOS-AP`) - automatic gateway IP fallback
- **Solution 4**: Verify LightwaveOS v2 device is powered on and on same network

**Network Diagnostics:**

The system logs detailed network status every 5 seconds:
```
[NETWORK DEBUG] WiFi:1 mDNS:0(2/6) manualIP:0 fallback:0 WS:0(Disconnected)
```

- `WiFi`: 1 = connected, 0 = disconnected
- `mDNS`: Resolution status (attempts/max attempts)
- `manualIP`: 1 = manual IP configured and enabled, 0 = not set
- `fallback`: 1 = timeout exceeded, using fallback, 0 = normal operation
- `WS`: WebSocket connection status

## UI Architecture (LVGL 9.3.0)

This firmware uses **LVGL 9.3.0** for a professional widget-based UI:

```
src/ui/
├── lv_conf.h                  # LVGL configuration
├── lvgl_bridge.h/cpp          # M5GFX ↔ LVGL integration
├── Theme.h                    # SpectraSynq color scheme
├── DisplayUI.h/cpp            # Main UI orchestration
├── LoadingScreen.h/cpp        # Boot animation with PPA
├── LedFeedback.h/cpp          # Connection status LEDs
├── ZoneComposerUI.h/cpp       # Zone control interface
├── widgets/
│   ├── GaugeWidget.h/cpp      # Radial parameter gauges
│   ├── PresetBankWidget.h/cpp # 8-bank preset UI
│   └── ActionRowWidget.h/cpp  # Touch action buttons
└── fonts/
    └── bebas_neue_*.h         # Custom Bebas Neue fonts
```

### UI Features

- **Radial Gauges**: Real-time parameter visualization with color-coded rings
- **Touch Controls**: Long-press parameter cells to reset to defaults
- **Preset Banks**: Visual 8-slot preset manager (save/recall/delete via buttons)
- **Action Row**: Gamma, color mode, auto-exposure, brown guardrail controls
- **Connection Status**: LED feedback for WiFi/WebSocket states
- **PPA Acceleration**: Hardware-accelerated transforms on ESP32-P4

## Parameter Mapping

### Unit A (0x42) - Global Parameters
| Encoder | Parameter   | Range    | Default | Behavior |
|---------|-------------|----------|---------|----------|
| 0       | Effect      | 0-87     | 0       | Wraps    |
| 1       | Palette     | 0-74     | 0       | Wraps    |
| 2       | Speed       | 1-100    | 25      | Clamps   |
| 3       | Mood        | 0-255    | 0       | Clamps   |
| 4       | Fade Amount | 0-255    | 0       | Clamps   |
| 5       | Complexity  | 0-255    | 128     | Clamps   |
| 6       | Variation   | 0-255    | 0       | Clamps   |
| 7       | Brightness  | 0-255    | 128     | Clamps   |

### Unit B (0x41) - Preset Management
| Button  | Function                    |
|---------|----------------------------|
| 8-15    | Single Click: Recall preset |
|         | Double Click: Save preset   |
|         | Long Hold: Delete preset    |

**Note:** Unit B encoders (rotation) are **disabled** - only buttons are used for preset management.

## Project Structure

```
firmware/Tab5.encoder/
├── platformio.ini                  # Build config with LVGL flags
├── README.md                       # This file
├── CLAUDE.md                       # Agent guidance
├── wifi_credentials.ini            # WiFi config (gitignored)
├── wifi_credentials.ini.template   # Template
├── scripts/
│   ├── pio_pre.py                 # RISC-V toolchain injector
│   └── convert_logo_to_rgb565.py  # Logo converter
├── find_device_ip.sh              # mDNS/gateway IP lookup
├── upload_ota.sh                  # OTA update helper
└── src/
    ├── main.cpp                   # Entry point (1987 lines)
    ├── config/
    │   ├── Config.h               # Parameters, I2C, pins
    │   └── network_config.h       # WiFi credentials
    ├── input/
    │   ├── DualEncoderService.h   # 16-encoder unified API
    │   ├── Rotate8Transport.h     # M5ROTATE8 I2C wrapper
    │   ├── EncoderProcessing.h    # Detent debounce, wrap/clamp
    │   ├── ButtonHandler.h        # Preset button logic
    │   ├── TouchHandler.h         # Touch screen reset
    │   ├── I2CRecovery.h          # Software bus recovery
    │   └── ClickDetector.h        # Multi-click detection
    ├── network/
    │   ├── WiFiManager.h/cpp      # Dual-network failover
    │   ├── WebSocketClient.h/cpp  # Bidirectional sync
    │   ├── WsMessageRouter.h      # Incoming message dispatch
    │   └── OtaHandler.h           # Firmware updates
    ├── parameters/
    │   ├── ParameterHandler.h/cpp # Encoder ↔ WebSocket sync
    │   └── ParameterMap.h/cpp     # Dynamic metadata
    ├── storage/
    │   ├── NvsStorage.h/cpp       # Flash persistence
    │   ├── PresetData.h           # Preset struct
    │   └── PresetStorage.h/cpp    # Preset I/O
    ├── presets/
    │   └── PresetManager.h/cpp    # 8-slot preset system
    ├── ui/
    │   └── (see UI Architecture above)
    └── hal/
        └── EspHal.h               # Hardware abstraction
```

## Features

### Hardware Control
- **16 Rotary Encoders** (Unit A: 8 active, Unit B: buttons only)
- **Detent Debouncing**: Normalizes M5ROTATE8's 2-step-per-detent quirk
- **LED Feedback**: Green flash on turn, cyan on reset, status LEDs
- **Button Reset**: Press encoder button to reset parameter to default

### UI/UX
- **LVGL 9.3.0**: Professional widget-based interface
- **5" Display**: Radial gauges, preset banks, action row
- **Touch Screen**: Long-press to reset parameters
- **Loading Animation**: PPA-accelerated boot screen

### Networking
- **Dual-Network WiFi**: Primary network + v2 AP fallback
- **WebSocket Sync**: Bidirectional parameter updates
- **mDNS Discovery**: Auto-find `lightwaveos.local`
- **Anti-Snapback**: 1-second holdoff prevents echo snapback

### Persistence
- **NVS Storage**: Parameters + presets saved to flash
- **Debounced Writes**: 500ms delay prevents wear
- **8 Preset Slots**: Full scene capture (parameters + zones)

### Advanced
- **I2C Recovery**: Software-level bus recovery (SCL toggling)
- **OTA Updates**: HTTP firmware upload
- **Click Detection**: Single/double/long click on buttons

## Milestones

- [x] **A:** Boot + I2C scan proof
- [x] **B:** M5ROTATE8 basic read/write
- [x] **C:** Encoder processing (detent debounce)
- [x] **D:** EncoderService with callbacks
- [x] **E:** Dual unit support (16 encoders)
- [x] **F:** Network control plane (WiFi + WebSocket)
- [x] **G.1:** NVS parameter persistence
- [x] **G.2:** I2C software recovery
- [x] **G.3:** Touch screen handler
- [x] **H:** LVGL UI with radial gauges
- [x] **Phase 8:** 8-bank preset system

## Troubleshooting

### "M5ROTATE8 not found"

1. **Check wiring:** Are both units connected to Port.A?
2. **Check addresses:** Unit A should be 0x42, Unit B should be 0x41
3. **Reprogram Unit A:** Use register 0xFF to change address to 0x42
4. **Power:** LEDs should briefly flash on boot

### "probe device timeout" Error

1. Try lower I2C frequency: Edit `I2C::FREQ_HZ` in `Config.h` to 50000
2. Check for loose connections
3. Ensure units have different addresses

### Display Not Working

The Tab5 has hardware revisions with different display ICs:
- Older units: ILI9881C
- Newer units: ST7123

M5Unified v0.2.7+ auto-detects both. If display issues persist, update M5Unified and M5GFX libraries.

### Build Fails - "riscv32-esp-elf-g++: command not found"

**YOU ARE USING THE WRONG BUILD COMMAND.**

Use the exact commands from the "CORRECT BUILD COMMANDS" section above. The `PATH` prefix and `-d` flag are **REQUIRED**.

### WebSocket Won't Connect

1. Check that v2 device is broadcasting `LightwaveOS-AP` (not `LightwaveOS`)
2. Verify password is `SpectraSynq` on both devices
3. Ensure v2 is in STA+AP mode (runs AP even when on WiFi)
4. Monitor serial output for WiFi status messages
5. Use `find_device_ip.sh` to verify v2 IP address

## Safety Notes

This firmware does **NOT** use aggressive I2C recovery patterns that could break Tab5's internal peripherals:

- **No** `periph_module_reset(PERIPH_I2C0_MODULE)`
- **No** `i2cDeinit(0)`
- **No** aggressive bus clear pin twiddling

The external I2C on Grove Port.A is isolated from the internal bus, but we maintain safe practices for system stability.

## Development

### Helper Scripts

```bash
# Find v2 device IP (mDNS + gateway fallback)
./find_device_ip.sh

# Upload firmware via OTA (requires device IP)
./upload_ota.sh 192.168.1.100 .pio/build/tab5/firmware.bin
```

### Simulator

The `simulator/` directory contains a desktop LVGL simulator for UI development without hardware.

## License

Part of the LightwaveOS project.

---

**Last Updated:** 2026-01-07
**Architecture:** LVGL 9.3.0 (migrated from PRISM.tab5)
**Firmware Version:** See `src/main.cpp` for version string
