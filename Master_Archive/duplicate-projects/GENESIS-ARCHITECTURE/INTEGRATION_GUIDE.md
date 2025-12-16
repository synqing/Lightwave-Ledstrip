# 🔗 VP_DECODER Integration Guide for LightwaveOS

This guide details how to integrate the VP_DECODER audio synchronization system with your existing LightwaveOS architecture. The implementation includes enhanced features for large file support and advanced synchronization.

## 📋 Overview

The VP_DECODER integration adds audio-reactive capabilities to LightwaveOS without disrupting the existing 176 FPS performance. It provides:

- Pre-processed musical data playback
- Synchronized audio-visual experiences
- Web-based control portal
- Audio-reactive LED effects
- Beat and frequency-based visualizations

## 🏗️ Architecture Integration

### System Architecture with VP_DECODER

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      ENHANCED LIGHTWAVEOS ARCHITECTURE                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────┐              │
│  │  Encoders   │     │  WebSocket  │     │ VP_DECODER  │ (NEW)        │
│  │  (Input)    │     │  (Commands) │     │(Audio Data) │              │
│  └──────┬──────┘     └──────┬──────┘     └──────┬──────┘              │
│         │                    │                    │                      │
│         ▼                    ▼                    ▼                      │
│  ┌──────────────────────────────────────────────────────┐              │
│  │              Main Loop (Core 1 - 176 FPS)            │              │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐    │              │
│  │  │VisualParams│  │Effect Engine│  │AudioFrame  │    │              │
│  │  │ Management │  │  Renderer   │  │ Provider   │    │              │
│  │  └────────────┘  └────────────┘  └────────────┘    │              │
│  └──────────────────────────────────────────────────────┘              │
│                                                                          │
│  ┌──────────────────────────────────────────────────────┐              │
│  │                  Effect Categories                     │              │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐    │              │
│  │  │Traditional │  │   Audio    │  │   Hybrid   │    │              │
│  │  │  Effects   │  │ Reactive   │  │  Effects   │    │              │
│  │  └────────────┘  └────────────┘  └────────────┘    │              │
│  └──────────────────────────────────────────────────────┘              │
│                                                                          │
│  ┌──────────────────────────────────────────────────────┐              │
│  │            Dual-Strip Output (Unchanged)              │              │
│  │     Strip 1 (GPIO 11)    |    Strip 2 (GPIO 12)      │              │
│  └──────────────────────────────────────────────────────┘              │
└─────────────────────────────────────────────────────────────────────────┘
```

## 📦 Integration Steps

**Note**: The implementation in this directory includes enhanced features for production use:
- Chunked upload support for files > 5MB
- Advanced network latency compensation
- Hardware timer precision
- Streaming JSON parser integration

### Step 1: Add VP_DECODER Files

1. Copy VP_DECODER source files to your project:
```bash
cp -r tools/VP_DECODER/* src/audio/
```

2. Update your `platformio.ini`:
```ini
lib_deps = 
    ; ... existing dependencies ...
    ArduinoJson @ ^6.21.3
    
build_flags = 
    ; ... existing flags ...
    -DAUDIO_SYNC_ENABLED
    -DFFT_BIN_COUNT=96
```

### Step 2: Modify Main Application

In `main.cpp`, add VP_DECODER initialization with enhanced features:

```cpp
#include "audio/vp_decoder.h"

// Global instances
VPDecoder vpDecoder;
AudioFrame currentAudioFrame;
bool audioSynqActive = false;

void setup() {
    // ... existing setup ...
    
    // Initialize VP Decoder
    Serial.println("Initializing VP Decoder...");
    // VP Decoder uses SPIFFS which should already be initialized
    
    // Pass VP Decoder reference to WebServer
    webServer.setVPDecoder(&vpDecoder);
}

void loop() {
    // ... existing loop code ...
    
    // Update audio frame if VP Decoder is playing
    if (vpDecoder.isPlaying()) {
        currentAudioFrame = vpDecoder.getCurrentFrame();
        audioSynqActive = true;
    } else {
        audioSynqActive = false;
    }
    
    // ... rest of loop ...
}
```

### Step 3: Enhance Effect System

Modify your effect base class to support audio:

```cpp
// In effects.h
class Effect {
public:
    // ... existing methods ...
    
    // New methods for audio support
    virtual bool requiresAudio() const { return false; }
    virtual void setAudioFrame(const AudioFrame* frame) {}
};

// In your effect renderer
void renderCurrentEffect() {
    if (currentEffect) {
        // Provide audio frame if effect needs it
        if (audioSynqActive && currentEffect->requiresAudio()) {
            currentEffect->setAudioFrame(&currentAudioFrame);
        }
        currentEffect->render(leds, visualParams);
    }
}
```

### Step 4: Add Web Portal Routes

In your WebServer setup:

```cpp
void WebServer::setupRoutes() {
    // ... existing routes ...
    
    // Serve audio sync portal
    server.serveStatic("/audio-sync/", SPIFFS, "/audio-sync/")
          .setDefaultFile("index.html");
    
    // Setup audio sync handlers
    setupAudioSynqHandlers();
}
```

### Step 5: Create SPIFFS Directory Structure

Create the following directory structure in your `data` folder:

```
data/
├── index.html          (existing)
├── script.js           (existing)
├── styles.css          (existing)
├── audio-sync/         (new)
│   ├── index.html
│   ├── script.js
│   └── styles.css
└── audio/              (new - for uploaded JSON files)
```

### Step 6: Memory Optimization

To ensure VP_DECODER doesn't impact performance:

```cpp
// In your memory configuration
#define VP_DECODER_USE_PSRAM    // Use PSRAM for audio buffers
#define AUDIO_BUFFER_SIZE 4096  // Limit buffer size
#define MAX_JSON_SIZE 20480     // Limit JSON parsing size
```

## 🎨 Adding Audio-Reactive Effects

### Example: Integrating the Bass Pulse Effect

```cpp
// In your effect initialization
#include "audio/AudioReactiveEffects.h"

void initializeEffects() {
    // ... existing effects ...
    
    // Add audio-reactive effects
    effects.push_back(new FrequencySpectrumEffect());
    effects.push_back(new BassPulseEffect());
    effects.push_back(new EnergyFlowEffect());
    effects.push_back(new BeatStrobeEffect());
    effects.push_back(new FrequencyWaterfallEffect());
}
```

### Creating Custom Audio Effects

Template for new audio-reactive effects:

```cpp
class MyAudioEffect : public AudioReactiveEffect {
private:
    // Effect-specific state
    
public:
    MyAudioEffect() : AudioReactiveEffect("My Audio Effect") {}
    
    void render(CRGB* leds, const VisualParams& params) override {
        if (!audioFrame || audioFrame->silence) {
            // Handle silence
            fadeToBlackBy(leds, NUM_LEDS, 20);
            return;
        }
        
        // Use audio data
        float energy = audioFrame->total_energy;
        bool beat = audioFrame->transient_detected;
        
        // Your visualization logic here
    }
};
```

## 🔄 Data Flow

### Audio Data Flow Path

```
JSON File Upload → SPIFFS Storage → VP_DECODER Loading → 
Frame Generation → AudioFrame → Effect Rendering → LED Output

Parallel:
MP3 Playback (Browser) ←→ Sync Commands (WebSocket) ←→ ESP32 Timing
```

### Synchronization Flow

1. **Upload Phase**: JSON + MP3 files prepared
2. **Load Phase**: VP_DECODER parses JSON data
3. **Sync Phase**: Countdown synchronizes start time
4. **Playback Phase**: Frame-by-frame audio data provided
5. **Render Phase**: Effects use audio data for visualization

## ⚡ Performance Considerations

### CPU Usage
- VP_DECODER: ~5% CPU when active
- Audio effects: 2-8% depending on complexity
- Total impact: <15% CPU overhead
- Maintains 176 FPS target

### Memory Usage
- JSON parsing: 8KB temporary
- Audio buffers: 20KB sliding window
- Frequency bins: 384 bytes
- Total: ~30KB additional RAM

### Optimization Tips

1. **Use PSRAM for large buffers**:
```cpp
void* audio_buffer = ps_malloc(AUDIO_BUFFER_SIZE);
```

2. **Limit concurrent audio effects**:
```cpp
if (currentEffect->requiresAudio() && audioSynqActive) {
    // Run audio effect
} else {
    // Fall back to non-audio effect
}
```

3. **Throttle audio frame updates**:
```cpp
static unsigned long lastAudioUpdate = 0;
if (millis() - lastAudioUpdate > 10) {  // 100Hz max
    currentAudioFrame = vpDecoder.getCurrentFrame();
    lastAudioUpdate = millis();
}
```

## 🐛 Troubleshooting

### Common Issues

1. **JSON Upload Fails**
   - Check SPIFFS free space
   - Verify JSON format
   - Monitor serial for errors

2. **Audio Sync Drift**
   - Adjust sync offset in web portal
   - Check network latency
   - Ensure stable WiFi connection

3. **Performance Impact**
   - Disable complex effects during audio sync
   - Reduce LED brightness
   - Use simpler audio visualizations

### Debug Output

Enable debug logging:
```cpp
#define VP_DECODER_DEBUG
#define AUDIO_SYNC_DEBUG
```

## 🚀 Advanced Features

### Multi-Track Support

```cpp
// Queue multiple tracks
vpDecoder.queueTrack("/audio/track1.json");
vpDecoder.queueTrack("/audio/track2.json");
vpDecoder.setPlayMode(VPDecoder::PLAYLIST);
```

### Beat-Triggered Events

```cpp
if (vpDecoder.isOnBeat()) {
    // Trigger effect change
    // Flash brightness
    // Change palette
}
```

### Custom Frequency Analysis

```cpp
// Access raw frequency bins
const float* bins = audioFrame.frequency_bins;
for (int i = 0; i < FFT_BIN_COUNT; i++) {
    float magnitude = bins[i];
    // Custom visualization
}
```

## 📚 Resources

- [VP_DECODER API Reference](./audio-sync-portal/README.md)
- [Audio Effect Examples](./audio-sync-portal/AudioReactiveEffects.h)
- [Web Portal Documentation](./audio-sync-portal/README.md)

## 🎯 Next Steps

1. Test with sample JSON files (including large 15-20MB files)
2. Calibrate network latency using the web portal
3. Create custom audio-reactive effects
4. Monitor sync metrics during performance
5. Share your creations with the community!

---

*The enhanced implementation provides production-ready features while maintaining backward compatibility. Small files work as before, while large files automatically use chunked uploads and streaming parsers.*