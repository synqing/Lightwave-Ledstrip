# LightwaveOS Production Build

Complete, standalone production build of LightwaveOS firmware with integrated palette system.

## Key Features
- **High-Performance Audio Visualization**: 119+ FPS on ESP32-S3
- **33 FastLED Gradient Palettes**: Professional color schemes
- **3 Color Modes**: HSV (original), Palette, and Hybrid blending
- **Optimized for ESP32-S3**: Single-core architecture, no PSRAM
- **Serial Command Interface**: Full control via USB serial

## Quick Start

### Prerequisites
- PlatformIO Core or IDE
- ESP32-S3 DevKit with 16MB flash
- USB cable

### Build and Upload
```bash
# Navigate to this folder
cd LightwaveOS-Production

# Build firmware
pio run -e esp32-s3-16mb

# Upload to device (update port in platformio.ini)
pio run --target upload -e esp32-s3-16mb

# Monitor serial output
pio device monitor
```

## Palette System Usage

### Serial Commands
- `P` - Cycle through all 33 palettes
- `p` - Cycle backwards through palettes  
- `P!` - Toggle between HSV/Palette/Hybrid color modes
- `set_palette=N` - Set specific palette (0-32)
- `color_mode=palette` - Switch to palette mode
- `palette_blend=128` - Set hybrid blend amount (0-255)

### Available Palettes
The system includes 33 carefully selected gradient palettes:
- Rainbow, Ocean, Forest themes
- Heat, Lava, Cloud variations  
- Party, Electronic, Retro styles
- And many more professional color schemes

## Configuration

### Serial Port
Update `platformio.ini` lines 32-33 with your device's port:
```ini
upload_port = /dev/cu.usbmodem1401
monitor_port = /dev/cu.usbmodem1401
```

### Performance Settings
- **Target FPS**: 86.6 (optimized for ESP32-S3)
- **Memory**: Internal SRAM only (NO PSRAM)
- **Flash**: 16MB with custom partition table
- **Core Usage**: Single-core (Core 0) for maximum performance

## File Structure
```
LightwaveOS-Production/
├── src/                    # Source code
│   ├── main.cpp           # Main firmware file  
│   ├── led_utilities.h    # LED utilities with palette system
│   ├── globals.h          # Configuration and global variables
│   ├── PalettesData.cpp   # 33 palette definitions
│   └── ...               # Other firmware modules
├── platformio.ini         # Build configuration
├── partitions_16MB_no_ota.csv # Custom partition table
└── README.md             # This file
```

## Recent Integration
- **Palette System**: Complete integration with 33 gradient palettes
- **Color Mode Switching**: Seamless transitions between HSV and palette modes
- **Performance Optimization**: Maintained 119+ FPS with new features
- **Non-Breaking Design**: Defaults to original HSV mode for compatibility

## Build Verification
This production build has been tested and verified:
- ✅ Compiles cleanly with PlatformIO
- ✅ Maintains high performance (119+ FPS)
- ✅ All 33 palettes load correctly
- ✅ Serial commands respond properly
- ✅ Compatible with existing configurations

**Firmware Version**: 4.01.01
**Build Date**: Ready for immediate deployment