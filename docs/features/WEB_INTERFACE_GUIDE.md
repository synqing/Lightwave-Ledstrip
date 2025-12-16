# Light Crystals Web Interface Guide

## Overview

We've created a beautiful, modern web interface for controlling the LED strips in real-time. The interface features a dark theme with smooth animations and comprehensive controls.

## Features Implemented

### 1. **Real-Time Control**
- WebSocket connection for instant updates
- Live LED preview with canvas visualization
- No page refresh needed for any action

### 2. **Effect Management**
- Visual effect grid with emoji icons
- 21 different effects to choose from
- Smooth transitions between effects

### 3. **Control Parameters**
- **Brightness**: 0-255 control
- **Speed**: Animation speed adjustment
- **Intensity**: Effect strength control
- **Saturation**: Color vibrancy control
- **Transition Time**: 100-3000ms adjustable

### 4. **Color Palettes**
- 9 pre-defined color palettes
- Visual preview of each palette
- One-click palette selection

### 5. **Advanced Settings**
- Random transitions toggle
- Optimized effects toggle
- Auto-cycle mode
- Performance metrics display

### 6. **Quick Actions**
- Power on/off toggle
- Next/Previous effect buttons
- Save preset functionality
- Emergency stop button

### 7. **Live Monitoring**
- FPS counter
- Free heap memory display
- Frame time metrics
- CPU usage estimation
- Optimization gain display

## Technical Implementation

### Frontend (data/ folder)
- **index.html**: Modern, responsive layout with Font Awesome icons
- **styles.css**: Dark theme with gradients and smooth animations
- **script.js**: WebSocket communication and UI management

### Backend (src/network/)
- **WebServer.h/cpp**: ESP32 web server with WebSocket support
- Serves files from SPIFFS
- Handles all control commands
- Broadcasts LED data for live preview

## File Structure
```
data/
├── index.html     # Main web interface
├── styles.css     # Beautiful dark theme styling
└── script.js      # WebSocket and UI logic

src/network/
├── WebServer.h    # Web server class definition
└── WebServer.cpp  # Implementation with handlers
```

## How to Enable

1. Update WiFi credentials in `src/config/network_config.h`:
```cpp
constexpr const char* WIFI_SSID = "YourWiFiSSID";
constexpr const char* WIFI_PASSWORD = "YourWiFiPassword";
```

2. Enable web server in `src/config/features.h`:
```cpp
#define FEATURE_WEB_SERVER 1
#define FEATURE_WEBSOCKET 1
```

3. Fix library dependency (current issue):
```ini
; In platformio.ini, ensure these libraries are installed:
lib_deps = 
    me-no-dev/AsyncTCP@^1.1.1
    ESPAsyncWebServer
```

4. Upload SPIFFS data:
```bash
pio run -t uploadfs
```

## Usage

1. Connect ESP32 to WiFi
2. Open browser to `http://[ESP32-IP]` or `http://lightcrystals.local`
3. Control interface loads automatically
4. All changes apply in real-time

## Interface Sections

### Header
- Connection status indicator
- FPS counter
- Free memory display

### Effects Grid
- 21 effects with visual icons
- Click to select
- Active effect highlighted

### Live Preview
- Canvas showing LED colors
- Real-time updates at 20Hz
- Pause/resume capability

### Main Controls
- Sliders for brightness, speed, intensity, saturation
- Real-time value display
- Smooth transitions

### Color Palettes
- Visual preview of each palette
- One-click selection
- Active palette highlighted

### Advanced Settings (Collapsible)
- Transition time control
- Feature toggles
- Performance metrics

### Quick Actions
- Large buttons for common tasks
- Emergency stop for safety

## WebSocket Protocol

### Commands from Client
```json
// Get current state
{"command": "get_state"}

// Set parameter
{"command": "set_parameter", "parameter": "brightness", "value": 128}

// Change effect
{"command": "set_effect", "effect": 5}

// Change palette
{"command": "set_palette", "palette": 3}

// Toggle power
{"command": "toggle_power"}

// Emergency stop
{"command": "emergency_stop"}
```

### Messages from Server
```json
// State update
{
  "type": "state",
  "currentEffect": 5,
  "brightness": 128,
  "fps": 156.2,
  "heap": 346048
}

// LED data for preview
{
  "type": "led_data",
  "leds": [{"r": 255, "g": 0, "b": 0}, ...]
}

// Performance metrics
{
  "type": "performance",
  "frameTime": 6.4,
  "cpuUsage": 45.2,
  "optimizationGain": 1.5
}
```

## Troubleshooting

### Library Installation Issue
Currently facing an issue with ESPAsyncWebServer library. To fix:

1. Try manual installation:
```bash
cd .pio/libdeps/esp32dev
git clone https://github.com/me-no-dev/ESPAsyncWebServer.git
```

2. Or use alternative web server library

### Connection Issues
- Check WiFi credentials
- Verify ESP32 IP address
- Ensure port 80 is not blocked
- Try AP mode if WiFi fails

## Future Enhancements

1. **Audio Reactive Mode**
   - Microphone input visualization
   - Beat detection controls

2. **Preset Management**
   - Save/load presets to SPIFFS
   - Import/export functionality

3. **Advanced Scheduling**
   - Time-based effect changes
   - Sunrise/sunset modes

4. **Multi-Device Sync**
   - Control multiple LED setups
   - Synchronized effects

## Conclusion

The web interface provides a professional, user-friendly way to control the LED strips. With its modern design and real-time capabilities, it transforms the LED control experience from command-line to a beautiful visual interface. 