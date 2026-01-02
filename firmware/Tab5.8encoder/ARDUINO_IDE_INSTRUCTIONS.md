# Arduino IDE Build Instructions - Tab5.8encoder

## ✅ Code is Ready!

All code is implemented and ready. The structure is Arduino IDE compatible.

## Step-by-Step Setup

### 1. Install ESP32 Board Support

1. Open **Arduino IDE**
2. Go to: **File → Preferences**
3. In **"Additional Boards Manager URLs"**, add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Click **OK**
5. Go to: **Tools → Board → Boards Manager**
6. Search for **"esp32"**
7. Install **"esp32" by Espressif Systems**
   - **Important**: Install the latest version that includes ESP32-P4 support
   - Version 3.x or later should work

### 2. Install Required Libraries

Go to: **Sketch → Include Library → Manage Libraries**

Install these libraries (search and install):

1. **M5Unified** (by M5Stack)
   - Version: 0.2.x or later
   - This provides Tab5 support and `M5.Ex_I2C`

2. **M5ROTATE8** (by Rob Tillaart)
   - Version: 0.4.1
   - Encoder driver library

3. **ArduinoJson** (by Benoit Blanchon)
   - Version: 7.x (not 6.x)
   - JSON parsing for WebSocket (if using Milestone E)

4. **WebSockets** (by Markus Sattler)
   - Version: 2.4.x or later
   - WebSocket client (if using Milestone E)

### 3. Open the Project

**Option A: Open main.cpp directly**
1. **File → Open**
2. Navigate to: `firmware/Tab5.8encoder/src/main.cpp`
3. Click **Open**

**Option B: Open the folder (Arduino IDE 2.x)**
1. **File → Open Folder**
2. Navigate to: `firmware/Tab5.8encoder/`
3. Select the folder

### 4. Select Board and Port

1. **Tools → Board → ESP32 Arduino**
   - Look for: **M5Stack Tab5** or **ESP32-P4 DevKitM-1** or **ESP32-P4**
   - If Tab5 not listed, use **ESP32-P4** generic board

2. **Tools → Port → /dev/tty.usbmodem21401**
   - If not listed, check: **Tools → Port → Get Board Info** to verify

3. **Tools → Upload Speed → 921600** (or leave default)

### 5. Build and Upload

1. Click **Verify** (✓) to compile first
   - This checks for errors without uploading
   - Should compile successfully

2. Click **Upload** (→) to build and upload
   - Wait for compilation
   - Wait for upload to complete
   - Should see "Hard resetting via RTS pin..."

### 6. Open Serial Monitor

1. **Tools → Serial Monitor**
2. Set baud rate to **115200**
3. You should see boot messages

## Expected Serial Output

```
[BOOT] Tab5.8encoder starting...
[BOOT] M5Unified initialised
[BOOT] M5.Ex_I2C initialised (Port.A: GPIO53/54)

[MILESTONE A] I2C Scan
[I2C SCAN] Starting scan...
[I2C SCAN] Device found at address 0x41
[I2C SCAN] Scan complete. Found 1 device(s).
[MILESTONE A] Found 1 device(s). Expected ROTATE8 at 0x41

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

1. **Check LED**: LED 0 on ROTATE8 should be **green**
2. **Turn encoder**: You should see:
   ```
   [PARAM] Effect changed to 1
   [PARAM] Brightness changed to 129
   ```
3. **Press button**: You should see:
   ```
   [PARAM] Effect reset to 0 (button press)
   ```

## Troubleshooting

### "Board not found"
- Update ESP32 board package to latest version
- Try **ESP32-P4** generic board instead of Tab5-specific

### "Port not found"
- Check USB cable connection
- Try unplugging and replugging USB
- Check: **Tools → Port → Get Board Info**
- Verify port: `ls /dev/tty.usbmodem*` in terminal

### Compilation Errors

**"M5Unified.h: No such file"**
- Install M5Unified library via Library Manager

**"M5ROTATE8.h: No such file"**
- Install M5ROTATE8 library via Library Manager

**"Config.h: No such file"**
- Make sure you opened `src/main.cpp` or the project folder
- Arduino IDE needs to see the `src/` directory structure

### Upload Errors

**"Failed to connect"**
- Hold **BOOT** button on Tab5
- Click Upload
- Release BOOT button when upload starts

**"Permission denied"**
- On macOS/Linux, you may need to add user to dialout group
- Or use `sudo` (not recommended)

### Runtime Issues

**No serial output**
- Check baud rate is **115200**
- Check port selection
- Try resetting Tab5

**ROTATE8 not detected**
- Check Grove cable is connected to **Port.A** (not other ports)
- Verify ROTATE8 is powered
- Check I2C address (should be 0x41)

## Project Structure

Arduino IDE will compile all files in:
- `src/main.cpp` - Main entry point
- `src/config/` - Configuration
- `src/i2c/` - I2C scanning
- `src/transport/` - ROTATE8 transport
- `src/processing/` - Encoder processing
- `src/service/` - Service integration
- `src/network/` - Network modules (optional, Milestone E)

All files are ready - just open and upload!

