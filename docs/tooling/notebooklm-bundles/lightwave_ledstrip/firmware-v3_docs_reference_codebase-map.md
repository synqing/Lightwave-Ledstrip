# firmware-v3 Codebase Map

**851 files | 141,327 LOC** | C++ 63.6%, Headers 36.2%, Python 0.2%
**Frameworks:** Arduino, ESP-IDF, FreeRTOS, PlatformIO (espressif32@6.9.0)
**Target:** ESP32-S3 (esp32-s3-devkitc-1, esp32-s3-devkitc1-n16r8)

## Dependencies

| Package | Version |
|---------|---------|
| fastled/FastLED | 3.10.0 |
| bblanchon/ArduinoJson | 7.0.4 |
| esp32async/ESPAsyncWebServer | 3.9.3 |
| esp32async/AsyncTCP | 3.4.9 |
| mabuware/mabutrace | ^1.0.0 |
| throwtheswitch/Unity | ^2.5.2 |

## Directory Structure

```
src/                           141,327 LOC total
|-- effects/                    52,980 LOC  (37.5%)
|   |-- ieffect/                  ~42K LOC  (350+ effect implementations)
|   |-- enhancement/               2.5K LOC (TemporalOp, EdgeMixer, Smoothing, ColorCorrection)
|   |-- transitions/               1.3K LOC (TransitionEngine, 12 types)
|   |-- zones/                     1.2K LOC (ZoneComposer, BlendMode)
|-- network/                    27,369 LOC  (19.4%)
|   |-- webserver/                ~12K LOC  (V1ApiRoutes, WsGateway, UdpStreamer, handlers/)
|-- audio/                      15,566 LOC  (11.0%)
|   |-- contracts/                 2.9K LOC (ControlBus, AudioEffectMapping)
|   |-- pipeline/                  1.9K LOC (PipelineCore, BeatTracker, FFT)
|   |-- tempo/                       0.9K LOC (TempoTracker)
|-- core/                       14,634 LOC  (10.4%)
|   |-- actors/                    5.4K LOC (ActorSystem, RendererActor, ShowDirectorActor)
|   |-- persistence/               2.7K LOC (NVS, ZoneConfig, EffectPreset)
|   |-- shows/                     1.6K LOC (CueScheduler, ShowTypes, BuiltinShows)
|   |-- state/                     1.3K LOC (StateStore, Commands, SystemState)
|   |-- system/                    1.7K LOC (OTA, StackMonitor, HeapMonitor)
|   |-- bus/                       0.6K LOC (MessageBus pub/sub)
|   |-- narrative/                 0.5K LOC (NarrativeEngine)
|-- codec/                       9,021 LOC  (6.4%)
|-- hal/                         4,372 LOC  (3.1%)
|   |-- led/                       1.1K LOC (FastLedDriver)
|   |-- display/                   1.1K LOC (DisplayActor, RM690B0)
|   |-- esp32p4/                   1.0K LOC (LedDriver_P4_RMT)
|   |-- interface/                 0.4K LOC (INetworkDriver, ILedDriver, IAudioCapture)
|-- sync/                        3,766 LOC  (2.7%)
|-- plugins/                     2,492 LOC  (1.8%)
|-- config/                      2,641 LOC  (1.9%)
|-- main.cpp                     3,693 LOC
```

## Entrypoints

- `main.cpp` -- Primary firmware entry (setup/loop, C++ main)
- `test_strip_hw.cpp` -- Hardware test entry (Arduino setup/loop)
- `config/gen_effect_ids.py` -- Code generator for effect ID constants

## Build Configuration

**File:** `platformio.ini` (project root)

| Environment | Purpose |
|-------------|---------|
| `esp32dev_audio_esv11` | Production: ESv11 audio backend |
| `esp32dev_audio_esv11_k1v2` | Production: K1v2 variant |
| `esp32dev_audio_pipelinecore` | Production: PipelineCore backend |
| `esp32dev_SSB` | Sensory Bridge board |
| `esp32dev_FH4R2` / `FH4` | Firehose4 board variants |
| `amoled_testrig` | AMOLED display test |
| `native_test` | Desktop unit tests |
| `native_test_esv11_music*` | Audio pipeline offline tests |
| `native_codec_test_*` | Codec serialization tests |
| `test_esp_now_sync_*` | ESP-NOW sync integration tests |

**39 build environments total** (production + test + trace + benchmark)

## Import Graph (Top 15 Most-Imported)

| Header | Imports |
|--------|--------:|
| `config/effect_ids.h` | 210 |
| `effects/CoreEffects.h` | 198 |
| `plugins/api/IEffect.h` | 190 |
| `plugins/api/EffectContext.h` | 188 |
| `config/features.h` | 97 |
| `utils/Log.h` | 57 |
| `network/ApiResponse.h` | 54 |
| `core/actors/RendererActor.h` | 36 |
| `effects/ieffect/ChromaUtils.h` | 35 |
| `effects/enhancement/SmoothingEngine.h` | 33 |
| `network/webserver/WebServerContext.h` | 30 |
| `network/webserver/WsCommandRouter.h` | 27 |
| `core/actors/ActorSystem.h` | 26 |
| `effects/ieffect/AudioReactiveLowRiskPackHelpers.h` | 23 |
| `effects/ieffect/LGPFilmPost.h` | 22 |

## Architecture Notes

- **Actor Model:** FreeRTOS tasks pinned to cores; Core 0 = network/input, Core 1 = rendering
- **Message Bus:** Singleton pub/sub, lock-free publish, mutex-protected subscribe, 16-byte fixed messages
- **Effect System:** 350+ effects via `IEffect` plugin interface, registered in `PatternRegistry`
- **Audio Pipeline:** I2S capture -> FFT -> BeatTracker -> TempoTracker -> ControlBus -> effects
- **Show System:** PROGMEM-stored cue sequences, CueScheduler dispatches timed actions
