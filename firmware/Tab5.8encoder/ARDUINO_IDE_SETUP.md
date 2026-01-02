# Arduino IDE Setup for Tab5.8encoder

## PlatformIO Issue

PlatformIO has a toolchain compatibility issue with ESP32-P4 (RISC-V extensions not supported). Use Arduino IDE instead.

## Arduino IDE Setup Steps

### 1. Install ESP32 Board Support

1. Open Arduino IDE
2. Go to: **File → Preferences**
3. In "Additional Boards Manager URLs", add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Go to: **Tools → Board → Boards Manager**
5. Search for "esp32" and install **"esp32" by Espressif Systems**
6. Make sure you have a recent version that includes ESP32-P4 support

### 2. Install Required Libraries

Go to: **Sketch → Include Library → Manage Libraries**

Install these libraries:
- **M5Unified** (by M5Stack) - version 0.2.x or later
- **M5ROTATE8** (by Rob Tillaart) - version 0.4.1
- **ArduinoJson** (by Benoit Blanchon) - version 7.x
- **WebSockets** (by Markus Sattler) - version 2.4.x or later

### 3. Select Board and Port

1. **Tools → Board → ESP32 Arduino → M5Stack Tab5** 
   - If Tab5 not listed, try: **ESP32-P4 DevKitM-1** or **ESP32-P4**
2. **Tools → Port → /dev/tty.usbmodem21401**

### 4. Build and Upload

1. Open: `firmware/Tab5.8encoder/src/main.cpp` in Arduino IDE
2. Click **Upload** button (or Ctrl+U / Cmd+U)
3. Wait for compilation and upload
4. Open Serial Monitor at **115200 baud**

## Expected Output

After upload, Serial Monitor should show:

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

## Troubleshooting

- **"Board not found"**: Update ESP32 board package to latest version
- **"Port not found"**: Check USB cable, verify port with `ls /dev/tty.usbmodem*`
- **Compilation errors**: Ensure all libraries are installed
- **Upload fails**: Hold BOOT button during upload, or check port permissions

