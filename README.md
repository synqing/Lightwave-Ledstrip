# Light Crystals - Modular LED Control System

A professional ESP32-based LED control system for 81 WS2812 LEDs with advanced effects, smooth transitions, and comprehensive performance monitoring.

## Features

- **16+ Visual Effects**: From basic gradients to advanced reaction-diffusion simulations
- **Smooth Transitions**: Professional fade, wipe, and blend transitions between effects
- **Performance Monitoring**: Real-time FPS, CPU usage, and timing breakdown
- **Modular Architecture**: Easy to extend with new effects and features
- **Hardware Optimized**: ESP32-specific optimizations for maximum performance

## Project Structure

```
LC_SelfContained/
├── platformio.ini          # PlatformIO configuration
├── src/                    # Source files
│   ├── main.cpp           # Main application
│   ├── config/            # Configuration files
│   │   ├── hardware_config.h    # Pin definitions, LED count
│   │   ├── network_config.h     # WiFi settings
│   │   └── features.h           # Feature flags
│   ├── core/              # Core functionality  
│   │   ├── FxEngine.h     # Effect management system
│   │   └── PaletteManager.h     # Color palette handling
│   ├── effects/           # Visual effects
│   │   ├── basic/         # Simple effects
│   │   ├── advanced/      # Complex effects
│   │   └── pipeline/      # Pipeline-based effects
│   ├── hardware/          # Hardware interfaces
│   │   ├── PerformanceMonitor.h # Performance tracking
│   │   └── HardwareOptimizer.h  # ESP32 optimizations
│   ├── network/           # Web interface (optional)
│   └── utils/             # Utilities
│       ├── SerialMenu.h   # Serial control interface
│       └── StripMapper.h  # LED position mapping
├── lib/                   # External libraries
├── data/                  # Web interface files
└── docs/                  # Documentation
```

## Building and Flashing

### Prerequisites
- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- ESP32 board package
- USB cable for programming

### Build Instructions

1. Clone the repository
2. Open in PlatformIO (VS Code recommended)
3. Build and upload:
   ```bash
   pio run -t upload
   ```

### Configuration

Edit `src/config/hardware_config.h` to match your setup:
```cpp
constexpr uint8_t LED_DATA_PIN = 6;     // LED data pin
constexpr uint16_t NUM_LEDS = 81;       // Number of LEDs
constexpr uint8_t BUTTON_PIN = 0;       // Button input
```

Enable/disable features in `src/config/features.h`:
```cpp
#define FEATURE_WEB_SERVER 0        // Web interface
#define FEATURE_SERIAL_MENU 1       // Serial control
#define FEATURE_PERFORMANCE_MONITOR 1 // Performance tracking
```

## Usage

### Physical Controls
- **Button**: Press to cycle through effects with different transitions

### Serial Commands
Connect at 115200 baud and type 'h' for help menu.

Common commands:
- `e` - Effects menu
- `p` - Palette selection
- `b <value>` - Set brightness (0-255)
- `f <value>` - Set fade amount (0-255)
- `s <value>` - Set speed (1-50)

### Performance Monitoring
The system provides real-time performance metrics:
- FPS and frame timing
- CPU usage percentage
- Memory usage and fragmentation
- Effect processing time breakdown

## Adding New Effects

1. Create a new effect class in `src/effects/`:
```cpp
class MyEffect : public EffectBase {
public:
    MyEffect() : EffectBase("My Effect", 128, 10, 20) {}
    
    void render() override {
        // Your effect code here
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            leds[i] = CRGB::Red;
        }
    }
};
```

2. Register it in the appropriate effects collection file.

## Feature Flags

The project uses compile-time feature flags to optimize binary size:
- `FEATURE_BASIC_EFFECTS`: Simple visual effects
- `FEATURE_ADVANCED_EFFECTS`: Complex computational effects
- `FEATURE_PIPELINE_EFFECTS`: Modular pipeline system
- `FEATURE_WEB_SERVER`: Web control interface
- `FEATURE_PERFORMANCE_MONITOR`: Performance tracking

## License

This project is provided as-is for educational and personal use.

## Acknowledgments

Built with:
- [FastLED](https://github.com/FastLED/FastLED) - LED control library
- [ArduinoJson](https://arduinojson.org/) - JSON parsing
- ESP32 Arduino Core