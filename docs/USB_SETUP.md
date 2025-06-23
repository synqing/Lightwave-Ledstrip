# USB Setup Guide for Light Crystals

## Configuration

### USB Port
- **Upload Port**: `/dev/cu.usbmodem1101`
- **Monitor Port**: `/dev/cu.usbmodem1101`
- **Baud Rate**: 115200

### USB CDC Settings
The project is configured with USB CDC (Communications Device Class) enabled:
- `ARDUINO_USB_CDC_ON_BOOT=1` - Enables USB serial on boot
- `ARDUINO_USB_MODE=1` - Sets USB mode
- DTR and RTS signals enabled for proper reset

## Quick Commands

### Upload Firmware
```bash
# Using the script
./upload.sh

# Or manually
pio run -t upload
```

### Monitor Serial Output
```bash
# Using the script
./monitor.sh

# Or manually
pio device monitor
```

### Clean and Rebuild
```bash
pio run -t clean
pio run
```

## Troubleshooting

### Device Not Found
If `/dev/cu.usbmodem1101` is not found:

1. Check USB cable connection
2. Verify ESP32 is powered on
3. List available devices:
   ```bash
   ls /dev/cu.usb*
   ```
4. Try resetting the ESP32 (press EN/RST button)

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
- **NEVER** upload to usbmodem12201 (as specified in user instructions)
- Always use `/dev/cu.usbmodem1101` for this project
- USB CDC requires proper initialization delay in setup()
- Some ESP32 boards may need BOOT button held during upload

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