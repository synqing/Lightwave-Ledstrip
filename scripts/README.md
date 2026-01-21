# Scripts

Utility scripts for building, uploading, and monitoring LightwaveOS firmware.

## Command Reference

| Script | Purpose | Usage |
|--------|---------|-------|
| `verify.sh` | Pre-push quality gate verification | `./scripts/verify.sh [--skip-build]` |
| `build.sh` | Compile firmware | `./scripts/build.sh [clean]` |
| `upload.sh` | Upload firmware to ESP32 | `./scripts/upload.sh` |
| `uploadfs.sh` | Upload LittleFS filesystem | `./scripts/uploadfs.sh` |
| `monitor.sh` | Serial monitor (115200 baud) | `./scripts/monitor.sh` |
| `install.sh` | Install K1 SDK dependencies | `./scripts/install.sh` |

## Script Details

### verify.sh

**Pre-push quality gate verification** - Comprehensive checks before pushing changes.

```bash
# Run all checks (recommended before push)
./scripts/verify.sh

# Skip build verification (faster, for quick checks)
./scripts/verify.sh --skip-build
```

**What it checks:**

1. **[1/6] Harness Schema Validation** - Validates `feature_list.json` integrity
2. **[2/6] PRD JSON Schema Validation** - Validates `.claude/prd/*.json` files
3. **[3/6] Cross-Reference Integrity** - Verifies PRD references exist
4. **[4/6] Pattern Compliance** - Enforces NO_RAINBOWS and CENTER_ORIGIN constraints
5. **[5/6] Build Verification** - Builds `esp32dev_audio` environment
6. **[6/6] Git Status** - Reports uncommitted changes

**Exit codes:**
- `0` - All checks passed (or passed with warnings)
- `1` - One or more critical checks failed

**Execution time:**
- With build: ~30-60 seconds (incremental builds ~3-5s)
- Without build (`--skip-build`): ~5-10 seconds

**Dependencies:**
- Required: `python3`, `pio`, `git`
- Optional: `jsonschema` (for PRD validation: `pip install jsonschema`)

**Relationship to init.sh:**
- `.claude/harness/init.sh` - Quick project health check (~10s) for agent work
- `scripts/verify.sh` - Comprehensive pre-push quality gate (~30-60s) for code quality

**Git hook integration:**

Add to `.git/hooks/pre-push`:
```bash
#!/bin/bash
./scripts/verify.sh
if [ $? -ne 0 ]; then
    echo "Pre-push verification failed. Fix issues or use --no-verify to skip."
    exit 1
fi
```

Make executable: `chmod +x .git/hooks/pre-push`

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
