# Wireless Encoder Transmitter

Standalone ESP32-S3 sketch for the wireless encoder device that reads 8 encoders + scroll wheel and transmits data wirelessly to the main LED controller.

## Hardware Requirements

### Core Components
- **ESP32-S3 DevKit** - Main microcontroller
- **M5Stack 8-Encoder Unit** - 8 rotary encoders with buttons
- **M5Unit-Scroll** - 9th encoder (scroll wheel)
- **LiPo Battery** - 3.7V with voltage divider for monitoring
- **Status LEDs** - 3x WS2812B for visual feedback (optional)
- **Haptic Motor** - Vibration feedback (optional)

### Wiring

#### M5Stack 8-Encoder (Main I2C Bus)
```
ESP32-S3    8-Encoder Unit
GPIO 13  →  SDA
GPIO 14  →  SCL
3.3V     →  VCC
GND      →  GND
```

#### M5Unit-Scroll (Secondary I2C Bus)
```
ESP32-S3    Scroll Unit
GPIO 15  →  SDA
GPIO 21  →  SCL
3.3V     →  VCC
GND      →  GND
```

#### Battery Monitoring
```
ESP32-S3    Battery Circuit
GPIO 36  →  Voltage Divider Output
           (Battery+ → 10kΩ → GPIO36 → 10kΩ → GND)
```

#### Optional Components
```
ESP32-S3    Component
GPIO 25  →  Haptic Motor (+)
GPIO 26  →  Status LEDs (DIN)
GND      →  Motor (-), LED GND
3.3V     →  LED VCC
```

## Configuration

### 1. Update Receiver MAC Address
In `src/main.cpp`, update this line with your main device's MAC address:
```cpp
uint8_t receiverMAC[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC}; // UPDATE THIS
```

### 2. Update Upload Port
In `platformio.ini`, change the upload port to match your device:
```ini
upload_port = /dev/cu.usbmodem2101  ; Your transmitter device port
monitor_port = /dev/cu.usbmodem2101
```

### 3. Hardware Pin Configuration
Modify pin assignments in the `HardwareConfig` namespace if needed.

## Features

### Real-time Transmission
- **100Hz update rate** for responsive control
- **Delta encoding** reduces packet size
- **ESP-NOW protocol** for <3ms latency

### Battery Management
- **Voltage monitoring** with percentage calculation
- **Low battery warnings** via status LEDs
- **Power optimization** for extended battery life

### Status Feedback
- **3 Status LEDs**:
  - LED 0: Transmission status (Green=success, Red=fail)
  - LED 1: Connection status (Blue=connected, Red=disconnected)
  - LED 2: Battery status (Green=good, Yellow=medium, Red=low)
- **Haptic feedback** on successful connection

### Serial Commands
- `i` - Show encoder values and system info
- `r` - Restart device

## Usage

### 1. Build and Upload
```bash
cd examples/wireless_encoder_transmitter
pio run -t upload -t monitor
```

### 2. Pairing Process
1. Power on transmitter device
2. Start receiver device with pairing mode
3. Devices will automatically connect
4. Blue LED indicates successful connection

### 3. Operation
- Turn encoders to send data wirelessly
- Press encoder buttons for button events
- Monitor status LEDs for connection health
- Use serial monitor for debugging

## Troubleshooting

### Connection Issues
- Verify MAC addresses are correct
- Check both devices are on same WiFi channel
- Ensure devices are within range (~100m line of sight)
- Check status LEDs for error patterns

### I2C Problems
- Verify wiring connections
- Check I2C addresses (0x41 for 8-encoder, 0x40 for scroll)
- Try different I2C pull-up resistors if needed

### Power Issues
- Check battery voltage and connections
- Verify voltage divider values
- Monitor battery LED for low power warnings

## Performance

- **Latency**: <3ms typical
- **Update Rate**: 100Hz (10ms intervals)
- **Range**: ~100m line of sight
- **Battery Life**: 8-24 hours depending on usage
- **Packet Size**: 57 bytes (23% of ESP-NOW limit)

## Integration Notes

This transmitter is designed to work with the wireless receiver integration in the main LED controller project. The packet format is compatible with the full wireless encoder system implementation.