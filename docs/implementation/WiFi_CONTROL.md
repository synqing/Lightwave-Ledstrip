# WiFi Control - How to Disable That Worthless Shit

WiFi is **DISABLED BY DEFAULT** because it's fucking worthless and wastes resources.

## Current Status
- **Default:** WiFi DISABLED (saves 25KB RAM + 519KB Flash)
- **RAM:** 13.4% usage without WiFi vs 21.2% with WiFi
- **Flash:** 33.4% usage without WiFi vs 65.1% with WiFi

## How to Enable WiFi (if you really want that garbage)

### Method 1: Change the Source Code
Edit `src/config/features.h`:
```cpp
#define FEATURE_WEB_SERVER 1  // Change from 0 to 1
```

### Method 2: Use Build Flag (Recommended)
In `platformio.ini`, uncomment this line:
```ini
-D FEATURE_WEB_SERVER=1  ; Enable WiFi/WebServer
```

### Method 3: Use WiFi Environment
Build with the WiFi-enabled environment:
```bash
pio run -e esp32dev_audio
```

## Available Build Environments

- `esp32dev` - Default, **NO WiFi** (fast, lean, efficient)
- `esp32dev_audio` - WiFi enabled (slow, bloated, worthless)
- `esp32dev_debug` - Debug build, no WiFi
- `memory_debug` - Memory debugging, no WiFi

## Why WiFi is Disabled

1. **Massive resource waste** - 25KB RAM + 519KB Flash
2. **Performance impact** - Degrades LED performance 
3. **Complexity** - More shit to go wrong
4. **Stability** - WiFi causes crashes and interference
5. **Bloat** - Unnecessary network dependencies

## What You Lose Without WiFi

- Web interface
- OTA updates
- Network control
- WebSocket connections
- mDNS discovery

## What You Keep

- **All LED effects work perfectly**
- **Audio sync works perfectly** 
- **Encoders work perfectly**
- **Serial control works perfectly**
- **Maximum performance**
- **Stable operation**

## The Bottom Line

WiFi is fucking worthless for LED control. The system runs better without it.
If you need remote control, use the serial interface or build a proper external controller.