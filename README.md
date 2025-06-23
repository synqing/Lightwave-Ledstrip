# Light Crystals - Modular LED Control System

A professional ESP32-S3 based LED control system supporting multiple hardware configurations:
- **Matrix Mode**: 9x9 matrix (81 LEDs) with button control
- **Strips Mode**: Dual 160-LED strips (320 LEDs total) with M5Stack 8encoder control
- **Light Guide Plate Mode**: Dual-edge injection into optical waveguide for advanced interference effects

## Features

### Core System
- **28+ Visual Effects**: 16 base effects plus 12 strip-specific effects
- **Smooth Transitions**: Professional fade, wipe, and blend transitions between effects
- **Performance Monitoring**: Real-time FPS, CPU usage, and timing breakdown
- **Modular Architecture**: Easy to extend with new effects and features
- **Hardware Optimized**: ESP32-S3 specific optimizations for maximum performance

### Light Guide Plate Capabilities
- **Interference Pattern Effects**: Standing waves, moiré patterns, constructive/destructive zones
- **Depth Illusion Effects**: Volumetric displays, parallax effects, Z-depth mapping
- **Physics Simulations**: Plasma fields, magnetic field lines, particle collisions, wave propagation
- **Interactive Features**: Proximity sensing, gesture recognition, touch-reactive surfaces
- **Advanced Optical Effects**: Edge coupling resonance, holographic patterns, energy visualization

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
    ├── LIGHT_GUIDE_PLATE.md  # Light guide plate effects guide
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

1. **Select Mode**: Edit `src/config/features.h`:
```cpp
#define LED_STRIPS_MODE 1  // Set to 1 for strips, 0 for matrix
#define LED_MATRIX_MODE 0  // Set to 1 for matrix, 0 for strips
#define FEATURE_LIGHT_GUIDE_MODE 1  // Enable light guide effects
```

2. **Hardware Pins** (automatically configured based on mode):

**Matrix Mode**:
```cpp
constexpr uint8_t LED_DATA_PIN = 6;     // LED data pin
constexpr uint16_t NUM_LEDS = 81;       // Number of LEDs
constexpr uint8_t BUTTON_PIN = 0;       // Button input
```

**Strips Mode**:
```cpp
constexpr uint8_t STRIP1_DATA_PIN = 11;  // First strip
constexpr uint8_t STRIP2_DATA_PIN = 12;  // Second strip
constexpr uint8_t I2C_SDA = 13;          // M5Stack 8encoder
constexpr uint8_t I2C_SCL = 14;          // M5Stack 8encoder
```

**Light Guide Plate Mode** uses the same pins as Strips Mode but with specialized effects for optical waveguide applications.

Enable/disable features in `src/config/features.h`:
```cpp
#define FEATURE_WEB_SERVER 0        // Web interface
#define FEATURE_SERIAL_MENU 1       // Serial control
#define FEATURE_PERFORMANCE_MONITOR 1 // Performance tracking
#define FEATURE_LIGHT_GUIDE_MODE 1  // Light guide effects
```

## Usage

### Physical Controls
- **Matrix Mode**: Button to cycle through effects with different transitions
- **Strips/Light Guide Mode**: M5Stack 8encoder for comprehensive control:
  - Encoder 0: Effect selection
  - Encoder 1: Palette selection  
  - Encoder 2: Speed control
  - Encoder 3: Fade amount
  - Encoder 4: Brightness
  - Encoder 5: Sync mode (strips/interference pattern)
  - Encoder 6: Propagation mode
  - Encoder 7: Light guide specific parameters

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

## Light Guide Plate Mode

### Overview
The Light Guide Plate mode transforms the system into a sophisticated optical waveguide display. LEDs at opposing edges (329mm apart) fire light into an acrylic plate, creating unique interference patterns and depth effects impossible with traditional LED arrangements.

### Key Features
- **Edge-lit Waveguide**: 160 LEDs per edge firing into optical plate
- **Interference Patterns**: Mathematical wave interference between opposing edges
- **Depth Illusions**: 3D objects appearing to float within the plate
- **Physics Simulations**: Realistic plasma, magnetic fields, and particle effects
- **Interactive Capabilities**: Proximity sensing and gesture recognition

### Effect Categories
1. **Interference Effects**: Standing waves, moiré patterns, constructive/destructive zones
2. **Depth Effects**: Volumetric displays, parallax illusions, Z-depth mapping
3. **Physics Simulations**: Plasma fields, magnetic field visualization, particle collisions
4. **Interactive Effects**: Touch-reactive surfaces, gesture-controlled visuals
5. **Advanced Optical**: Holographic patterns, energy transfer visualization

See [docs/LIGHT_GUIDE_PLATE.md](docs/LIGHT_GUIDE_PLATE.md) for complete technical documentation.

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
- `FEATURE_LIGHT_GUIDE_MODE`: Light guide plate effects
- `FEATURE_STRIP_EFFECTS`: Strip-specific effects
- `FEATURE_INTERFERENCE_CALC`: Wave interference calculations
- `FEATURE_INTERACTIVE_SENSING`: Proximity and gesture detection

## License

This project is provided as-is for educational and personal use.

## Acknowledgments

Built with:
- [FastLED](https://github.com/FastLED/FastLED) - LED control library
- [ArduinoJson](https://arduinojson.org/) - JSON parsing
- ESP32 Arduino Core