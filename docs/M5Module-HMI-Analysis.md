# M5Module-HMI Internal Firmware Analysis

## Overview
The M5Module-HMI is a human-machine interface module based on STM32F0 microcontroller that provides:
- 1x Scroll wheel encoder with click functionality
- 2x Physical buttons
- 2x LEDs for status indication
- I2C slave interface for communication

## Key Hardware Specifications

### Microcontroller
- STM32F0 series MCU
- Running at 8MHz (HSI internal oscillator)
- Uses Timer3 for encoder quadrature decoding

### I2C Interface
- **Default Address**: 0x41 (same as M5ROTATE8!)
- **Configurable**: Address can be changed and stored in flash
- **Protocol**: 7-bit addressing, slave mode
- **Speed**: Standard mode (timing value 0x2000090E)

### GPIO Mappings
- **PA0**: LED control / SK6812 data output
- **PA1**: LED 2 control (active low)
- **PA3**: Button 1 input
- **PA4**: Button 2 input  
- **PA5**: Encoder button/scroll wheel click
- **PA6**: Encoder Channel A (TIM3_CH1)
- **PA7**: Encoder Channel B (TIM3_CH2)

## I2C Register Map

### Encoder Position Registers (0x00-0x03)
- **Read**: Returns 4-byte signed int32 encoder position
- **Write**: Sets encoder position (5 bytes: register + 4-byte value)

### Increment Counter Registers (0x10-0x13)
- **Read**: Returns 4-byte signed int32 increment count since last read
- **Note**: Reading automatically resets increment counter to 0

### Button State Registers
- **0x20**: Encoder button/scroll wheel click state (0=released, 1=pressed)
- **0x21**: Physical button 1 state
- **0x22**: Physical button 2 state

### LED Control Registers
- **0x30**: LED 1 control (0=off, 1=on)
- **0x31**: LED 2 control (0=off, 1=on)

### System Registers
- **0x40**: Reset counters (write 1 to reset)
- **0xFE**: Firmware version (read only)
- **0xFF**: I2C address (read current or write new address)

## Key Implementation Details

### Encoder Handling
1. Uses STM32 hardware timer in encoder mode (TIM3)
2. Implements overflow handling for 32-bit counter from 16-bit timer
3. Filter setting of 0x08 on both channels for debouncing
4. Rising edge detection on both channels

### Button Handling
- Direct GPIO reads, no hardware debouncing
- Polled in main loop via `encode_update()`
- No interrupt-based button detection

### LED Features
- Supports individual SK6812/WS2812 addressable LEDs via bit-banging
- Also has simple GPIO LED control on PA0/PA1
- LED states are inverted (0=on due to active-low configuration)

### Performance Characteristics
- Main loop continuously polls encoder and buttons
- I2C interrupt-driven for responsive communication
- No FreeRTOS - bare metal implementation

## Key Differences from M5ROTATE8

### Hardware Differences
1. **Single encoder** vs 8 encoders on M5ROTATE8
2. **3 buttons total** (encoder click + 2 physical) vs 8 encoder buttons
3. **2 simple LEDs** vs 9 RGB LEDs per encoder
4. **No RGB support** for main LEDs (but has SK6812 capability)

### Protocol Differences
1. **Same I2C address** (0x41) - conflict potential!
2. **Different register layout** - not compatible
3. **Simpler protocol** - no channel selection needed
4. **32-bit values** vs mixed 8/32-bit in M5ROTATE8

## Integration Considerations

### 1. Address Conflict
- Both devices default to 0x41
- Must change one device's address before using together
- M5Module-HMI supports address change via register 0xFF

### 2. Driver Requirements
Need new driver class that:
- Implements simplified protocol
- Handles single encoder instead of 8
- Manages 3 buttons instead of 8
- Supports basic LED control

### 3. Suggested Architecture
```cpp
class M5ModuleHMI {
private:
    uint8_t _address = 0x41;
    TwoWire* _wire;
    
public:
    // Core encoder functions
    int32_t getAbsCounter();
    bool setAbsCounter(int32_t value);
    int32_t getRelCounter();  // Auto-resets
    
    // Button functions
    bool getEncoderButton();
    bool getButton1();
    bool getButton2();
    
    // LED control
    bool setLED(uint8_t led, bool state);
    
    // System
    bool setAddress(uint8_t address);
    uint8_t getFirmwareVersion();
};
```

### 4. Migration Path
1. Create new `M5ModuleHMI` driver class
2. Add to `EncoderManager` with device type detection
3. Map single encoder to virtual "encoder 0"
4. Map buttons to virtual positions (e.g., button1->encoder1, button2->encoder2)
5. Use simple LED states instead of RGB

### 5. Compatibility Layer
Could create adapter to make HMI module appear like single-encoder M5ROTATE8:
- Map encoder to channel 0
- Map button 1/2 to channels 1/2 button presses
- Ignore remaining channels 3-7
- Convert RGB calls to simple on/off

## Recommendations

1. **Address Management**: Change M5Module-HMI to address 0x42 to avoid conflicts
2. **Driver Development**: Create dedicated driver for cleaner implementation
3. **Feature Parity**: Consider which features are essential vs nice-to-have
4. **Testing**: Verify timing compatibility with existing encoder debouncing
5. **Documentation**: Update user docs to explain module differences

## Next Steps

1. Implement `M5ModuleHMI` driver class
2. Add device detection in `EncoderManager::begin()`
3. Create configuration option to select encoder type
4. Test with actual hardware
5. Update UI to handle mixed encoder configurations