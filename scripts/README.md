# Scripts

Utility scripts for building, uploading, and monitoring LightwaveOS firmware.

## Command Reference

| Script | Purpose | Usage |
|--------|---------|-------|
| `build.sh` | Compile firmware | `./scripts/build.sh [clean]` |
| `upload.sh` | Upload firmware to ESP32 | `./scripts/upload.sh` |
| `uploadfs.sh` | Upload LittleFS filesystem | `./scripts/uploadfs.sh` |
| `monitor.sh` | Serial monitor (115200 baud) | `./scripts/monitor.sh` |
| `install.sh` | Install K1 SDK dependencies | `./scripts/install.sh` |

## Script Details

### build.sh

Compiles the LightwaveOS firmware without uploading.

```bash
# Normal build
./scripts/build.sh

# Clean build (removes build artifacts first)
./scripts/build.sh clean
```

Shows memory usage summary on successful build.

### upload.sh

Compiles and uploads firmware to ESP32-S3, then starts serial monitor.

```bash
./scripts/upload.sh
```

- Default port: `/dev/cu.usbmodem1101`
- Automatically starts monitor after successful upload
- Press `Ctrl+C` to exit monitor

### uploadfs.sh

Uploads the LittleFS filesystem (web interface files) to ESP32.

```bash
./scripts/uploadfs.sh
```

- Default port: `/dev/cu.usbmodem1101` (v2 ESP32-S3 device)
- Uses `esp32dev_audio` environment
- Web interface accessible at `http://lightwaveos.local` after upload

### monitor.sh

Connects to ESP32 serial output for debugging.

```bash
./scripts/monitor.sh
```

- Baud rate: 115200
- Port: `/dev/cu.usbmodem1101`
- Press `Ctrl+C` to exit

### install.sh

Installs K1 Lightwave SDK and pattern development tools.

```bash
./scripts/install.sh
```

Installs:
- `k1lightwave` Python package
- `k1-pattern-sdk` npm package

Run `k1-pattern-wizard` after installation to create patterns.

## USB Port Configuration

**Locked-in device ports:**
- **v2 ESP32-S3 device**: `/dev/cu.usbmodem1101` (used by `upload.sh`, `uploadfs.sh`, `monitor.sh`)
- **Tab5.encoder device**: `/dev/cu.usbmodem101` (see `firmware/Tab5.encoder/README.md` for upload commands)

If you need to override the port, use PlatformIO directly:

```bash
# Find available ports
ls /dev/cu.usb*

# Upload v2 firmware with custom port
cd firmware/v2 && pio run -t upload --upload-port /dev/cu.usbmodemXXXX

# Upload Tab5 firmware with custom port
cd firmware/Tab5.encoder && pio run -e tab5 -t upload --upload-port /dev/cu.usbmodemXXXX
```

## PlatformIO Direct Commands

For more control, use PlatformIO directly from the `firmware/v2` directory:

```bash
cd firmware/v2

# Build default environment
pio run

# Build with WiFi
pio run -e esp32dev_wifi

# Upload
pio run -t upload

# Upload filesystem
pio run -t uploadfs

# Monitor
pio device monitor -b 115200

# Clean
pio run -t clean
```

## Related Documentation

- [../firmware/v2/platformio.ini](../firmware/v2/platformio.ini) - Build configuration
- [../CLAUDE.md](../CLAUDE.md) - Build commands and environments
