# Tab5.encoder UI Simulator

SDL2-based simulator for testing the Tab5.encoder UI without physical hardware.

## Prerequisites

### macOS
```bash
brew install sdl2 cmake
```

### Ubuntu/Debian
```bash
sudo apt install libsdl2-dev cmake build-essential
```

## Building

```bash
cd tab5-encoder/simulator
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Running

```bash
./tab5_ui_sim
```

## Features

- **DisplayUI Main Page**: Renders the full UI with 8 gauge widgets and preset slots
- **Animated Test Data**: Encoder values animate automatically with sine waves
- **Connection State Simulation**: WiFi/WebSocket/Encoder connection states
- **Battery Simulation**: Simulated battery level and charging state
- **Real-time Updates**: UI updates at ~60 FPS

## Controls

- **Close Window**: Click the window close button or press ESC to exit
- **Automatic Updates**: Encoder values and connection states update automatically

## Architecture

The simulator uses:
- **SDL2**: Window management and rendering
- **M5GFX Mock**: Drop-in replacement for M5GFX API
- **Hardware Abstraction Layer (HAL)**: Abstracts ESP32-specific calls
- **Conditional Compilation**: Same UI code works in both hardware and simulator

## Limitations

- Text rendering is simplified (rectangles as placeholders)
- Font rendering not fully implemented (would need SDL_ttf)
- Touch input not simulated (can be added)
- Some hardware-specific features not available

## Future Enhancements

- Proper text rendering with SDL_ttf
- Touch input simulation (mouse events)
- Screenshot capture
- Multiple display size testing
- Visual regression testing

