# Build and Upload Instructions

## Quick Answer: All Agent Work is Already Implemented!

**All agent tickets (A-G) have been implemented** in `firmware/Tab5.8encoder/`. You don't need to apply anything - it's all ready to build!

## Build and Upload (Tab5 on usbmodem21401)

### Option 1: PlatformIO (Recommended for this setup)

```bash
cd firmware/Tab5.8encoder
pio run -t upload
pio device monitor -b 115200
```

The `platformio.ini` is already configured with:
- Upload port: `/dev/tty.usbmodem21401`
- Board: `m5stack-tab5` (may need adjustment)
- All required libraries

**If board not found**, try:
```bash
pio boards | grep -i tab5
pio boards | grep -i esp32-p4
```

Then update `platformio.ini` with the correct board ID.

### Option 2: Arduino IDE

1. Open `firmware/Tab5.8encoder/src/main.cpp` in Arduino IDE
2. Select board: **Tools → Board → ESP32 Arduino → M5Stack Tab5** (or ESP32-P4 variant)
3. Select port: **Tools → Port → /dev/tty.usbmodem21401**
4. Install libraries:
   - M5Unified
   - M5ROTATE8
   - ArduinoJson (v7.x)
   - WebSockets (v2.4.x)
5. Click **Upload** (or Ctrl+U / Cmd+U)
6. Open Serial Monitor at 115200 baud

## Expected Serial Output

After upload, you should see:

```
[BOOT] Tab5.8encoder starting...
[BOOT] M5Unified initialised
[BOOT] M5.Ex_I2C initialised (Port.A: GPIO53/54)

[MILESTONE A] I2C Scan
[I2C SCAN] Starting scan...
[I2C SCAN] Device found at address 0x41
[I2C SCAN] Scan complete. Found 1 device(s).

[MILESTONE B] ROTATE8 Transport
[ROTATE8 TRANSPORT] M5ROTATE8 begin() succeeded
[MILESTONE B] ROTATE8 transport ready. LED 0 should be green.

[MILESTONE C] Encoder Processing
[MILESTONE C] Encoder processing ready

[MILESTONE D] Encoder Service
[ENCODER SERVICE] Service initialised
[MILESTONE D] Encoder service ready

[READY] All milestones complete. Turn encoders to see parameter changes.
```

## Testing

1. **Check LED**: LED 0 on ROTATE8 should be green
2. **Turn encoder**: You should see `[PARAM] Effect changed to X`
3. **Press button**: You should see `[PARAM] Effect reset to 0 (button press)`

## Troubleshooting

- **"Board not found"**: Try different ESP32-P4 board variant, or use Arduino IDE
- **"Port not found"**: Check USB cable, try `ls /dev/tty.usbmodem*` to find port
- **"I2C device not found"**: Check ROTATE8 is connected to Grove Port.A
- **Compilation errors**: Install all required libraries

## What Was Implemented

All agent tickets completed:
- ✅ **Ticket A**: Repo structure, README, docs
- ✅ **Ticket B**: Build configuration (platformio.ini)
- ✅ **Ticket C**: I2C scan (Milestone A)
- ✅ **Ticket D**: ROTATE8 transport (Milestone B)
- ✅ **Ticket E**: Encoder processing (Milestone C)
- ✅ **Ticket F**: Encoder service (Milestone D)
- ✅ **Ticket G**: Network modules (Milestone E - ready but optional)

Everything is in `firmware/Tab5.8encoder/src/` - just build and upload!

