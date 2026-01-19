# USB Setup Guide for LightwaveOS Hardware

## Device Port Mapping

**CRITICAL: Device Port Assignments**
- **ESP32-S3 (v2 firmware)**: `/dev/cu.usbmodem1101`
- **Tab5 (encoder firmware)**: `/dev/cu.usbmodem101`

Always use these specific ports for uploads and monitoring.

## Configuration

### USB Ports
- **ESP32-S3 Upload Port**: `/dev/cu.usbmodem1101`
- **ESP32-S3 Monitor Port**: `/dev/cu.usbmodem1101`
- **Tab5 Upload Port**: `/dev/cu.usbmodem101`
- **Tab5 Monitor Port**: `/dev/cu.usbmodem101`
- **Baud Rate**: 115200 (both devices)

### USB CDC Settings
The project is configured with USB CDC (Communications Device Class) enabled:
- `ARDUINO_USB_CDC_ON_BOOT=1` - Enables USB serial on boot
- `ARDUINO_USB_MODE=1` - Sets USB mode
- DTR and RTS signals enabled for proper reset

## Quick Commands

### Upload Firmware

**ESP32-S3 (v2 firmware)**:
```bash
cd firmware/v2
pio run -e esp32dev_audio -t upload --upload-port /dev/cu.usbmodem1101
```

**Tab5 (encoder firmware)**:
```bash
cd firmware/Tab5.encoder
pio run -e tab5 -t upload --upload-port /dev/cu.usbmodem101
```

### Monitor Serial Output

**ESP32-S3**:
```bash
pio device monitor -p /dev/cu.usbmodem1101 -b 115200
```

**Tab5**:
```bash
pio device monitor -p /dev/cu.usbmodem101 -b 115200
```

### Clean and Rebuild
```bash
pio run -t clean
pio run
```

## Troubleshooting

### Device Not Found

**ESP32-S3** - If `/dev/cu.usbmodem1101` is not found:
1. Check USB cable connection
2. Verify ESP32-S3 is powered on
3. List available devices:
   ```bash
   pio device list
   ```
4. Try resetting the device (press EN/RST button)

**Tab5** - If `/dev/cu.usbmodem101` is not found:
1. Check USB cable connection
2. Verify Tab5 is powered on
3. List available devices:
   ```bash
   pio device list
   ```
4. Try resetting the device (press EN/RST button)

### Upload Fails
1. Ensure no other program is using the serial port
2. Close any serial monitors
3. Try holding BOOT button while uploading
4. Check USB cable (use data cable, not charge-only)

### Serial Monitor Shows Garbage
1. Verify baud rate is 115200
2. Check USB CDC is properly initialized
3. Reset the device after connecting monitor

## USB CDC Benefits
- Faster communication than traditional UART
- No need for external USB-UART chip
- Direct USB connection to ESP32
- Better reliability for high-speed data

## Important Notes
- **ESP32-S3 (v2 firmware)**: Always use `/dev/cu.usbmodem1101`
- **Tab5 (encoder firmware)**: Always use `/dev/cu.usbmodem101`
- USB CDC requires proper initialization delay in setup()
- Some ESP32 boards may need BOOT button held during upload
- **NEVER** upload to the wrong device port (always verify with `pio device list` first)

## Platform-Specific Notes

### macOS
- Uses `/dev/cu.usbmodem*` naming convention
- May require driver installation for some boards
- Check System Information > USB for device details

### Auto-Reset Circuit
The USB CDC configuration includes auto-reset functionality:
- DTR signal triggers reset
- RTS signal controls boot mode
- No manual button pressing needed for upload