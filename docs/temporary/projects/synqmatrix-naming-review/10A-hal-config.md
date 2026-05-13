# Subsystem: HAL + Config

> Scope: `firmware-v3/src/hal/`, `firmware-v3/src/config/`
> Files inspected: 37
> Generated 2026-05-13 for SynqMatrix naming review.

## Notable conventions

- **Config:** mostly `constexpr` / `inline constexpr` in nested namespaces (`lightwaveos::audio`, `lightwaveos::limits`, `chip`, `chip::gpio`, `chip::i2s`, `chip::task`, `chip::perf`, `chip::display`, `lightwaveos::config::NetworkConfig`). Only `features.h`, `chip_config.h`, `version.h`, and `Trace.h` use traditional `#define` macros. `effect_ids.h` is auto-generated via `gen_effect_ids.py`.
- **Macros vs constants:** `FEATURE_*` (compile-time toggles), `CHIP_*` (platform detection), `K1_*` (board pin overrides), `FIRMWARE_VERSION_*`, `TRACE_*`, `WIFI_*` / `AP_*` / `OTA_*` / `API_*` (build-flag-overrideable network credentials). Constexpr identifiers are `SCREAMING_SNAKE_CASE` for values, `PascalCase` for types/enums.
- **HAL:** PascalCase classes, `I*` interface prefix (`IAudioCapture`, `ILedDriver`, `INetworkDriver`), `m_` member prefix, `k*` for static-class constants, `lightwaveos::hal` namespace (with `display` and `chip` sub-namespaces). Two competing `ILedDriver` interfaces coexist (see Anomalies).
- **Implementations:** `LedDriver_<chip>` suffix (`_S3`, `_P4`, `_P4_RMT`), `FastLedDriver` (chip-agnostic legacy wrapper). All derive from one of the two `ILedDriver`s.

---

## File: `firmware-v3/src/config/audio_config.h`

Namespace: `lightwaveos::audio`. Guarded by `FEATURE_AUDIO_SYNC`.

### Enums
- `enum class MicType : uint8_t { SPH0645, IM69D130 }`

### Constants (constexpr)
- `MICROPHONE_TYPE`
- I2S pins: `I2S_BCLK_PIN`, `I2S_DIN_PIN`, `I2S_DOUT_PIN`, `I2S_LRCL_PIN`, `I2S_MCLK_MULTIPLE`
- Sample / hop: `SAMPLE_RATE`, `HOP_SIZE`, `ESV11_CHUNK_SIZE`, `FFT_SIZE`, `GOERTZEL_WINDOW`, `HOP_DURATION_MS`, `HOP_RATE_HZ`
- DMA: `DMA_BUFFER_COUNT`, `DMA_BUFFER_SAMPLES`
- Format: `I2S_BITS_PER_SAMPLE`
- Bands: `NUM_BANDS`, `BAND_CENTER_FREQUENCIES[]`
- Staleness: `STALENESS_THRESHOLD_MS`
- Actor: `AUDIO_ACTOR_PRIORITY`, `AUDIO_ACTOR_CORE`, `AUDIO_ACTOR_STACK_WORDS`, `AUDIO_ACTOR_TICK_MS`

---

## File: `firmware-v3/src/config/chip_amoled241.h`

Namespace: `chip` (with `gpio`, `i2s`, `task`, `perf`, `display` sub-namespaces). Waveshare 2.41" AMOLED rig.

### Macros
- None — header is pure constexpr.

### Constexpr constants (top-level `chip`)
- `CPU_FREQ_MHZ`, `CPU_CORES`, `CPU_ARCH`, `HAS_INTEGRATED_WIFI`, `HAS_BLUETOOTH`, `HAS_ETHERNET`, `RMT_CHANNELS`, `GPIO_COUNT`, `SRAM_SIZE_KB`, `PSRAM_MAX_MB`, `MIN_FREE_HEAP_KB`

### `chip::gpio`
- `LED_STRIP1_DATA`, `LED_STRIP2_DATA`, `I2S_BCLK`, `I2S_DOUT`, `I2S_LRCL`, `I2C_SDA`, `I2C_SCL`, `TTP223`, `AMOLED_CS`, `AMOLED_CLK`, `AMOLED_D0`, `AMOLED_D1`, `AMOLED_D2`, `AMOLED_D3`, `AMOLED_RST`, `TOUCH_SDA`, `TOUCH_SCL`, `TOUCH_RST`

### `chip::display`
- `WIDTH`, `HEIGHT`, `PIXEL_COUNT`, `SPI_FREQ_HZ`, `BYTES_PER_PIXEL`, `DMA_BUF_PIXELS`

### `chip::i2s`, `chip::task`, `chip::perf`
- Same identifier set as `chip_esp32s3.h` (see below).

---

## File: `firmware-v3/src/config/chip_config.h`

Auto-detects platform and dispatches to `chip_esp32s3.h`, `chip_amoled241.h`, or `chip_esp32p4.h`.

### Macros
- `CHIP_ESP32_P4`, `CHIP_ESP32_S3`, `CHIP_NAME`

### Free functions (namespace `chip`)
- `inline const char* getChipName()`
- `inline constexpr bool isESP32S3()`
- `inline constexpr bool isESP32P4()`

---

## File: `firmware-v3/src/config/chip_esp32p4.h`

Namespace: `chip` (with `gpio`, `i2s`, `task`, `perf`).

### Constexpr constants
- Top-level: `CPU_FREQ_MHZ` (400), `CPU_CORES`, `CPU_ARCH` ("RISC-V HP"), `HAS_INTEGRATED_WIFI` (false), `HAS_BLUETOOTH` (false), `HAS_ETHERNET` (false), `RMT_CHANNELS` (4), `GPIO_COUNT` (55), `SRAM_SIZE_KB` (768), `PSRAM_MAX_MB` (32), `MIN_FREE_HEAP_KB` (60)
- `chip::gpio`: `LED_STRIP1_DATA`, `LED_STRIP2_DATA`, `I2S_BCLK`, `I2S_DIN`, `I2S_DOUT`, `I2S_LRCL`, `I2S_MCLK`, `I2C_SDA`, `I2C_SCL`, `AUDIO_PA_EN`, `USB_DP`, `USB_DM`
- `chip::i2s`: `DRIVER_TYPE` ("std"), `PORT`, `SAMPLE_RATE` (16000), `DMA_BUFFER_COUNT`, `DMA_BUFFER_SAMPLES`
- `chip::task`: `RENDERER_CORE`, `AUDIO_CORE`, `NETWORK_CORE`, `STACK_MULTIPLIER` (1.2f)
- `chip::perf`: `TARGET_FPS`, `FRAME_BUDGET_US`, `AUDIO_HOP_RATE` (125), `AUDIO_LATENCY_MS`

---

## File: `firmware-v3/src/config/chip_esp32s3.h`

Namespace: `chip` (with `gpio`, `i2s`, `task`, `perf`). Production K1.

### Constexpr constants
- Top-level: `CPU_FREQ_MHZ` (240), `CPU_CORES`, `CPU_ARCH` ("Xtensa LX7"), `HAS_INTEGRATED_WIFI` (true), `HAS_BLUETOOTH` (true), `HAS_ETHERNET` (false), `RMT_CHANNELS` (8), `GPIO_COUNT` (45), `SRAM_SIZE_KB` (384), `PSRAM_MAX_MB` (8), `MIN_FREE_HEAP_KB` (40)
- `chip::gpio` (all overridable via `K1_*` build flags): `LED_STRIP1_DATA`, `LED_STRIP2_DATA`, `I2S_BCLK`, `I2S_DOUT`, `I2S_LRCL`, `I2C_SDA`, `I2C_SCL`, `TTP223`
- Build-flag override macros: `K1_LED_STRIP1_DATA`, `K1_LED_STRIP2_DATA`, `K1_I2S_BCLK`, `K1_I2S_DOUT`, `K1_I2S_LRCL`, `K1_I2C_SDA`, `K1_I2C_SCL`, `K1_TTP223_PIN`
- `chip::i2s`: `DRIVER_TYPE` ("legacy"), `PORT`, `SAMPLE_RATE` (12800), `DMA_BUFFER_COUNT`, `DMA_BUFFER_SAMPLES`
- `chip::task`: `RENDERER_CORE`, `AUDIO_CORE`, `NETWORK_CORE`, `STACK_MULTIPLIER` (1.0f)
- `chip::perf`: `TARGET_FPS`, `FRAME_BUDGET_US`, `AUDIO_HOP_RATE` (50), `AUDIO_LATENCY_MS`

---

## File: `firmware-v3/src/config/DebugConfig.h`

Namespace: `lightwaveos::config`.

### Enums
- `enum class DebugDomain : uint8_t { AUDIO, RENDER, NETWORK, ACTOR, SYSTEM, MOTION, _COUNT }`
- `enum class DebugLevel : uint8_t { OFF, ERROR, WARN, INFO, VERBOSE, TRACE }`

### Constexpr
- `DEBUG_LEVEL_NAMES[]`, `DEBUG_DOMAIN_NAMES[]`

### Struct `DebugConfig`
- Public fields: `globalLevel`, `audioLevel`, `renderLevel`, `networkLevel`, `actorLevel`, `systemLevel`, `motionLevel`, `statusIntervalSec`, `spectrumIntervalSec`
- Methods: `effectiveLevel(DebugDomain)`, `setDomainLevel(DebugDomain, int8_t)`, `getDomainLevel(DebugDomain)`, `shouldLog(DebugDomain, DebugLevel)`, static `domainName(DebugDomain)`, static `levelName(DebugLevel)`, static `levelName(uint8_t)`

### Free functions
- `DebugConfig& getDebugConfig()`
- `void resetDebugConfig()`
- `void printDebugConfig()`

---

## File: `firmware-v3/src/config/DebugConfig.cpp`

### Free functions / static symbols
- Anonymous namespace: `s_debugConfig` (singleton instance)
- Macro: `DBG_PRINTF(...)` (Arduino vs cstdio split)
- Implementations of header free functions and `DebugConfig::domainName`, `DebugConfig::levelName`

---

## File: `firmware-v3/src/config/display_order.h`

Namespace: `lightwaveos`. Includes `effect_ids.h`.

### Constexpr
- `DISPLAY_ORDER[]` (array of `EffectId` references)
- `DISPLAY_COUNT`

---

## File: `firmware-v3/src/config/effect_ids.h`

Auto-generated from `inventory.json` by `gen_effect_ids.py`. Namespace: `lightwaveos`.

### Type alias
- `using EffectId = uint16_t`

### Family-byte constants (28)
- `FAMILY_CORE`, `FAMILY_INTERFERENCE`, `FAMILY_GEOMETRIC`, `FAMILY_ADVANCED_OPTICAL`, `FAMILY_ORGANIC`, `FAMILY_QUANTUM`, `FAMILY_COLOUR_MIXING`, `FAMILY_NOVEL_PHYSICS`, `FAMILY_CHROMATIC`, `FAMILY_AUDIO_REACTIVE`, `FAMILY_PERLIN_REACTIVE`, `FAMILY_PERLIN_AMBIENT`, `FAMILY_PERLIN_TEST`, `FAMILY_ENHANCED_AUDIO`, `FAMILY_DIAGNOSTIC`, `FAMILY_AUTO_CYCLE`, `FAMILY_ES_REFERENCE`, `FAMILY_ES_TUNED`, `FAMILY_SB_REFERENCE`, `FAMILY_BEAT_PULSE`, `FAMILY_TRANSPORT`, `FAMILY_HOLOGRAPHIC_VAR`, `FAMILY_REACTION_DIFFUSION`, `FAMILY_SHAPE_BANGERS`, `FAMILY_HOLY_SHIT_BANGERS`, `FAMILY_EXPERIMENTAL_AUDIO`, `FAMILY_SHOWPIECE_PACK3`, `FAMILY_FIVE_LAYER_AR`, `FAMILY_RESERVED_START`, `FAMILY_SYSTEM`

### Effect ID constants
- 227 `EID_*` `constexpr EffectId` entries (e.g. `EID_FIRE = 0x0100`, `EID_OCEAN`, `EID_PLASMA`, `EID_LGP_HOLOGRAPHIC`, `EID_LGP_AURORA_BOREALIS`, `EID_BEAT_PULSE_RESONANT`, `EID_RIPPLE_ENHANCED`, `EID_BPM_ENHANCED`, `EID_SINELON`, `EID_LGP_HOLOGRAPHIC_ES_TUNED`, `EID_LGP_PERLIN_CAUSTICS`, etc.). Full list not inlined here — see source.

---

## File: `firmware-v3/src/config/factory_presets.h`

Namespace: `lightwaveos`.

### Struct
- `struct FactoryPreset { const char* name; EffectId effectId; uint8_t paletteIndex; uint8_t hue; uint8_t saturation; uint8_t mood; uint8_t trails; uint8_t intensity; uint8_t complexity; uint8_t variation; }`

### Constants
- `FACTORY_PRESET_COUNT` (8)
- `FACTORY_PRESETS[]` — P1 Prism → P8 Hush
- `FACTORY_PRESET_DEFAULT_INDEX`

---

## File: `firmware-v3/src/config/features.h`

Pure-macro header. ~50 `FEATURE_*` flags plus a handful of P4 overrides and one motion-extension toggle.

### Macros (grouped)
- Network: `FEATURE_WEB_SERVER`, `FEATURE_MULTI_DEVICE`
- Core toggles: `FEATURE_ZONE_SYSTEM`, `FEATURE_TRANSITIONS`, `FEATURE_AUDIO_SYNC`, `FEATURE_TRANSLATION_ENGINE`, `FEATURE_AUDIO_HF_SEMANTICS`, `FEATURE_TRANSLATION_DEBUG`
- Audio backend mutex set: `FEATURE_AUDIO_BACKEND_ESV11`, `FEATURE_AUDIO_BACKEND_ESV11_32KHZ`, `FEATURE_AUDIO_BACKEND_PIPELINECORE`, `FEATURE_AUDIO_BACKEND_SPINE16K`
- Audio extras: `FEATURE_AUTO_SPEED`, `FEATURE_MUSICAL_SALIENCY`, `FEATURE_STYLE_DETECTION`, `FEATURE_SB_PARITY_SIDECAR`, `AUDIO_SB_SIDECAR_DECIMATION`, `FEATURE_AUDIO_OA`, `FEATURE_AUDIO_BENCHMARK`, `FEATURE_VRMS_BENCHMARK`, `FEATURE_VRMS_METRICS`
- Streaming / merge: `FEATURE_WEB_STREAMING`, `FEATURE_INPUT_MERGE_LAYER`, `FEATURE_EFFECT_VALIDATION`
- Auth / OTA: `FEATURE_OTA_UPDATE`, `FEATURE_API_AUTH`
- Peripherals: `FEATURE_ROTATE8_ENCODER`, `FEATURE_STATUS_STRIP_TOUCH`, `FEATURE_AR_1C_EXPERIMENTAL`
- Enhancement engines: `FEATURE_COLOR_ENGINE`, `FEATURE_MOTION_ENGINE`, `FEATURE_PATTERN_REGISTRY`
- Monitoring: `FEATURE_HEAP_MONITORING`, `FEATURE_MEMORY_LEAK_DETECTION`, `FEATURE_VALIDATION_PROFILING`, `FEATURE_STACK_PROFILING`
- Debug: `DEBUG`, `FEATURE_MABUTRACE`, `FEATURE_TRACE_AUDIO_HANDOFF`, `FEATURE_TRACE_AUDIO_DSP`
- Other: `HAS_TEMP_SENSOR`, `FEATURE_AMOLED_DISPLAY`, `CONTROLBUS_HAS_TIMING_JITTER`

---

## File: `firmware-v3/src/config/limits.h`

Namespace: `lightwaveos::limits`.

### Constexpr
- `MAX_EFFECTS` (256), `MAX_PALETTES` (75), `MAX_ZONES` (3)

---

## File: `firmware-v3/src/config/network_config.h`

Namespace: `lightwaveos::config::NetworkConfig`. Guarded by `FEATURE_WEB_SERVER`.

### Constexpr
- WiFi: `WIFI_SSID_VALUE`, `WIFI_PASSWORD_VALUE`, `WIFI_SSID_2_VALUE`, `WIFI_PASSWORD_2_VALUE`, `WIFI_SSID_3_VALUE`, `WIFI_PASSWORD_3_VALUE`, `WIFI_ATTEMPTS_PER_NETWORK`
- AP: `AP_SSID` (default "LightwaveOS-AP"), `AP_PASSWORD`
- Server: `WEB_SERVER_PORT`, `WEBSOCKET_PORT`, `WIFI_CONNECT_TIMEOUT_MS`, `WIFI_RETRY_COUNT`
- mDNS: `MDNS_HOSTNAME` ("lightwaveos")
- OTA / API: `OTA_UPDATE_TOKEN`, `MAX_OTA_CHUNK_DECODED_SIZE`, `API_KEY_VALUE`
- WebSocket: `WS_MAX_CLIENTS`, `WS_PING_INTERVAL_MS`
- WiFiManager: `SCAN_INTERVAL_MS`, `RECONNECT_DELAY_MS`, `MAX_RECONNECT_DELAY_MS`

### Build-flag override macros (consumed if defined)
- `WIFI_SSID`, `WIFI_PASSWORD`, `WIFI_SSID_2`, `WIFI_PASSWORD_2`, `WIFI_SSID_3`, `WIFI_PASSWORD_3`, `AP_SSID_CUSTOM`, `AP_PASSWORD_CUSTOM`, `OTA_TOKEN`, `API_KEY`

---

## File: `firmware-v3/src/config/persistence_trigger.h`

Forwarding header only; re-exports `runtime_state.h`. No new symbols.

---

## File: `firmware-v3/src/config/runtime_state.h`

### Forward declarations
- `lightwaveos::persistence::ZoneConfigManager`

### Extern declarations (defined in `main.cpp`)
- `lightwaveos::persistence::ZoneConfigManager* zoneConfigMgr`
- `uint8_t g_factoryPresetIndex`
- `std::atomic<bool> g_externalNvsSaveRequest`

---

## File: `firmware-v3/src/config/Trace.h`

Macro shim around MabuTrace; guarded by `FEATURE_MABUTRACE`.

### Macros
- Real (when enabled): `TRACE_SCOPE` (from `mabutrace.h`), `TRACE_COUNTER`, `TRACE_INSTANT`, `TRACE_BEGIN`, `TRACE_END`, `TRACE_INIT`, `TRACE_FLUSH`, `TRACE_IS_ENABLED`
- No-op stubs (disabled): same names, expand to `do {} while(0)`

### File-local static
- `_lw_trace_h` (`profiler_duration_handle_t`)

---

## File: `firmware-v3/src/config/version.h`

### Macros
- `FIRMWARE_VERSION_MAJOR`, `FIRMWARE_VERSION_MINOR`, `FIRMWARE_VERSION_PATCH`, `FIRMWARE_VERSION_STRING`, `FIRMWARE_VERSION_NUMBER`

### Free function
- `inline uint32_t parseVersionNumber(const char* versionStr)`

---

## File: `firmware-v3/src/hal/HalFactory.h`

Namespace: `lightwaveos::hal`. Compile-time chip dispatch via type aliases.

### Macros
- `USE_FASTLED_DRIVER` (P4-only)

### Type aliases
- `using LedDriver = LedDriver_S3` (S3 path) or `LedDriver = LedDriver_P4` / `LedDriver_P4_RMT` (P4 path)

### Constexpr
- `PLATFORM_NAME`, `HAS_INTEGRATED_WIFI`, `HAS_ETHERNET`, `CPU_FREQ_MHZ`

### Free functions
- `inline const char* getPlatformName()`
- `inline constexpr bool hasIntegratedWiFi()`
- `inline constexpr bool hasEthernet()`
- `inline constexpr uint32_t getCpuFreqMHz()`

---

## File: `firmware-v3/src/hal/display/BitmapFont.h`

Namespace: `lightwaveos::display`.

### Class `BitmapFont`
- Public static constexpr: `GLYPH_WIDTH` (5), `GLYPH_HEIGHT` (7), `ADVANCE` (6)
- Public static methods: `getGlyph(char c)`, `stringWidth(const char* str)`
- Private static: `s_fontData[]` (PROGMEM, 475 bytes, inline definition)

### Macros (NATIVE_BUILD fallback)
- `PROGMEM` (stub), `pgm_read_byte(addr)` (stub)

---

## File: `firmware-v3/src/hal/display/DisplayActor.h`

Namespace: `lightwaveos::display`.

### Forward declarations
- `lightwaveos::actors::RendererActor`

### Sub-namespace `layout` (constexpr)
- Screen: `SCREEN_W` (600), `SCREEN_H` (450)
- Panels: `STRIP1_Y`, `STRIP1_H`, `STRIP2_Y`, `STRIP2_H`, `HEATMAP_Y`, `HEATMAP_H`, `METRICS_Y`, `METRICS_H`, `STATUS_Y`, `STATUS_H`
- LEDs: `LEDS_PER_STRIP`, `TOTAL_LEDS`
- Pixel mapping: `LED_PIXEL_WIDTH`, `HEATMAP_LED_PX`, `HEATMAP_X_OFFSET`, `HEATMAP_LINES`

### Class `DisplayActor : public actors::Actor`
- Public: `DisplayActor(actors::RendererActor*)`, `~DisplayActor()`, deleted copy ctor/assign
- Protected overrides: `onStart()`, `onMessage(const actors::Message&)`, `onTick()`, `onStop()`
- Private renderers: `renderStripPreview(uint16_t yOffset, const CRGB*, uint16_t count)`, `renderHeatmap()`, `renderMetrics()`, `renderStatusLine()`
- Private primitives: `static crgbToRgb565(const CRGB&)`, `fillRect(x, y, w, h, color565)`, `drawChar(x, y, char, fg565, bg565)`, `drawString(x, y, str, fg565, bg565, scale)`, `drawBar(x, y, w, h, value, barColor, bgColor)`
- Private members: `m_renderer`, `m_display` (`RM690B0Driver`), `m_ledSnapshot[]`, `m_heatmapBuf[][]`, `m_heatmapHead`, `m_scanline`, `m_displayFrame`, `m_lastEffectName[32]`, `m_lastStatusText[64]`
- Private static constexpr: `SCANLINE_PIXELS`, RGB565 palette `COLOR_BLACK`, `COLOR_WHITE`, `COLOR_GREEN`, `COLOR_RED`, `COLOR_YELLOW`, `COLOR_CYAN`, `COLOR_GREY`, `COLOR_DKGREY`, `COLOR_ORANGE`

---

## File: `firmware-v3/src/hal/display/DisplayActor.cpp`

Free-function-level symbols: none beyond `DisplayActor::*` definitions. Implements the protected overrides and renderers declared in the header.

---

## File: `firmware-v3/src/hal/display/RM690B0Driver.h`

Namespace: `lightwaveos::display`.

### Class `RM690B0Driver`
- Public: default ctor, `~RM690B0Driver()`, deleted copy ctor/assign, `init()`, `isInitialized()`, `setWindow(x0, y0, x1, y1)`, `pushPixels(const uint16_t*, uint32_t count)`, `setBrightness(uint8_t)`, `setDisplayOn(bool)`
- Public static constexpr: `WIDTH` (600), `HEIGHT` (450)
- Private: `writeCommand(uint8_t cmd, const uint8_t* data, uint32_t len)`, `hardwareReset()`, `runInitSequence()`, `setCS()`, `clrCS()`
- Private members: `m_spi` (`spi_device_handle_t`), `m_initialized`
- Private static constexpr: `MAX_TRANSFER_BYTES` (32768)

---

## File: `firmware-v3/src/hal/display/RM690B0Driver.cpp`

Implements all declared methods. No additional free symbols beyond class scope.

---

## File: `firmware-v3/src/hal/esp32p4/LedDriver_P4_RMT.h`

Namespace: `lightwaveos::hal`. Guarded by `CONFIG_IDF_TARGET_ESP32P4` / `CHIP_ESP32_P4`.

### Structs
- `struct DitherError { float r, g, b; }`
- `struct LedStripEncoder { rmt_encoder_t base; rmt_encoder_t* bytes_encoder; rmt_encoder_t* copy_encoder; int state; rmt_symbol_word_t reset_code; }`

### Class `LedDriver_P4_RMT : public ILedDriver`
- Public: ctor, `~LedDriver_P4_RMT()`, full `ILedDriver` override set (`init`, `initDual`, `deinit`, `getBuffer()`, `getBuffer(uint8_t)`, `getTotalLedCount`, `getLedCount(uint8_t)`, `show`, `setBrightness`, `getBrightness`, `setMaxPower`, `clear`, `fill`, `setPixel`, `isInitialized`, `getStats`, `resetStats`), plus driver-specific `setDitheringEnabled(bool)`, `isDitheringEnabled()`
- Private static constexpr: `kMaxLedsPerStrip` (160), `kBytesPerPixel` (3), `kRmtResolutionHz` (10 MHz), `kRmtMemBlockSymbols` (128), `kRmtTransQueueDepth` (4), WS2812 timing `kT0H`, `kT0L`, `kT1H`, `kT1L`, `kResetTicks`, dither `kDitherThreshold`
- Private members: `m_config1`, `m_config2`, `m_stripCounts[2]`, `m_totalLeds`, `m_brightness`, `m_maxMilliamps`, `m_initialized`, `m_dual`, `m_ditheringEnabled`, `m_firstFrame`, `m_strip1[]`, `m_strip2[]`, `m_rawBuffer[]`, `m_ditherError[]`, `m_txChanA`, `m_txChanB`, `m_encoderA`, `m_encoderB`, `m_stripEncoderA`, `m_stripEncoderB`, `m_txConfig`, `m_stats`
- Private methods: `initRmtChannel(uint8_t gpio, rmt_channel_handle_t*)`, `createEncoders()`, `quantizeWithDithering(const CRGB* src, uint8_t* dst, DitherError* err, uint16_t count)`, `quantizeSimple(...)`, `applyBrightness(uint8_t& r, uint8_t& g, uint8_t& b)`, `updateShowStats(uint32_t showUs)`, `initRandomDitherError()`

---

## File: `firmware-v3/src/hal/esp32p4/LedDriver_P4_RMT.cpp`

### File-static RMT encoder callbacks
- `static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t*)`
- `static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t*)`
- (plus `rmt_encode_led_strip` / similar encoder fn implementing the `rmt_encoder_t` interface)

---

## File: `firmware-v3/src/hal/esp32p4/LedDriver_P4.h`

Namespace: `lightwaveos::hal`.

### Class `LedDriver_P4 : public ILedDriver`
- Public: ctor, defaulted dtor, full `ILedDriver` override set as in `LedDriver_P4_RMT`
- Private static constexpr: `kMaxLedsPerStrip`
- Private members: `m_config1`, `m_config2`, `m_stripCounts[2]`, `m_totalLeds`, `m_brightness`, `m_initialized`, `m_dual`, `m_strip1[]`, `m_strip2[]`, `m_ctrl1` (`CLEDController*`), `m_ctrl2`, `m_stats`
- Private methods: `updateShowStats(uint32_t)`, `applyColorCorrection(const LedStripConfig&)`

---

## File: `firmware-v3/src/hal/esp32p4/LedDriver_P4.cpp`

Implements `LedDriver_P4` methods via FastLED `CLEDController`. No additional free functions.

---

## File: `firmware-v3/src/hal/esp32s3/LedDriver_S3.h`

Namespace: `lightwaveos::hal`.

### Class `LedDriver_S3 : public ILedDriver`
- Public: ctor, defaulted dtor, full `ILedDriver` override set, plus `setDithering(bool)`, `isDitheringEnabled()`, `isShowInProgress()`
- Private static constexpr: `kMaxLedsPerStrip` (160), `kMinShowGapUs` (250), `kWireTimeUs` (5600)
- Private members: `m_config1`, `m_config2`, `m_stripCounts[2]`, `m_totalLeds`, `m_brightness`, `m_ditheringEnabled`, `m_initialized`, `m_dual`, `m_strip1[]`, `m_strip2[]`, `m_txStrip1[]`, `m_txStrip2[]`, `m_ctrl1` (`CLEDController*`), `m_ctrl2`, `m_showMutex` (`SemaphoreHandle_t`), `m_lastShowStartUs`, `m_lastShowEndUs`, `m_showInProgress` (`std::atomic<bool>`), `m_stats`
- Private methods: `updateShowStats(uint32_t)`, `applyColorCorrection(const LedStripConfig&)`, `syncBuffersToFastLED()`

---

## File: `firmware-v3/src/hal/esp32s3/LedDriver_S3.cpp`

Implements `LedDriver_S3` methods. Hot-path `show()` body resides at lines ~147-187 (per session memory anchor #49449).

---

## File: `firmware-v3/src/hal/esp32s3/StatusStripTouch.h`

Non-namespaced (free C-style API). Guarded by `!NATIVE_BUILD`.

### Enums
- `enum class ButtonEvent : uint8_t { NONE, TAP, LONG_PRESS }`

### Free functions
- `void statusStripTouchSetup()`
- `void statusStripTouchLoop(uint32_t now)`
- `void statusStripShowPalette(uint8_t paletteIndex)`
- `void statusStripNextIdleMode()`
- `ButtonEvent statusStripPollButton(uint32_t now)`
- `void statusStripSetAudioFailure(bool failed)`
- `void statusStripTriggerWhiteFlash()`
- `void statusStripSetLowHeap(bool low)`

---

## File: `firmware-v3/src/hal/esp32s3/StatusStripTouch.cpp`

### File-static constexpr
- `STATUS_STRIP_PIN` (38), `STATUS_LED_COUNT` (30), `STATUS_BRIGHTNESS` (50), `STATUS_CENTRE` (14), `TAP_MAX_MS` (500), `LONG_PRESS_MS` (1000), `MAX_SPARKLES` (3)

### File-static helpers
- `selectModeForPalette(uint8_t paletteIndex)`
- `renderStaticGradient(const CRGBPalette16&, uint8_t bri)`
- `renderScroll(uint32_t now, const CRGBPalette16&, uint8_t bri)`
- `renderComet(uint32_t dt, const CRGBPalette16&, uint8_t bri)`
- `renderPulse(uint32_t now, const CRGBPalette16&, uint8_t bri)`
- `renderTwinkle(uint32_t now, const CRGBPalette16&, uint8_t bri)`
- `applyCentreReveal(uint32_t now, uint8_t bri)`
- `renderAmberBreathing(uint32_t now)`
- `renderWhiteFlash(uint32_t now)`
- `renderDimPulse(uint32_t now)`
- `compensatedBrightness()`

---

## File: `firmware-v3/src/hal/interface/IAudioCapture.h`

Namespace: `lightwaveos::hal`.

### Structs
- `struct CaptureStats { successCount, failCount, overrunCount, lastCaptureUs, dcEstimate, noiseFloor }`
- `struct AudioCaptureConfig { sampleRate, hopSize, bclkPin, doutPin, lrclPin, dmaBufferCount, dmaBufferSize }`

### Enums
- `enum class CaptureResult { Success, Timeout, BufferOverrun, NotInitialized, Error }`

### Interface `IAudioCapture`
- Pure virtual: `init(const AudioCaptureConfig&)`, `deinit()`, `captureHop(int16_t* buffer, uint32_t timeoutMs)`, `isInitialized()`, `getStats()`, `resetStats()`, `getSampleRate()`, `getHopSize()`

---

## File: `firmware-v3/src/hal/interface/ILedDriver.h`

Namespace: `lightwaveos::hal`. **Active HAL interface used by RendererActor / `HalFactory`.**

### Structs
- `struct LedStripConfig { ledCount, dataPin, brightness, reverseOrder, colorCorrection (CRGB) }`
- `struct LedDriverStats { frameCount, showSkips, lastShowUs, avgShowUs, maxShowUs, ledShowFailures, rmtErrors, rmtUnderruns, currentBrightness }`

### Interface `ILedDriver`
- Pure virtual: `init(const LedStripConfig&)`, `initDual(const LedStripConfig&, const LedStripConfig&)`, `deinit()`, `getBuffer()`, `getBuffer(uint8_t)`, `getTotalLedCount()`, `getLedCount(uint8_t)`, `show()`, `setBrightness(uint8_t)`, `getBrightness()`, `setMaxPower(uint8_t volts, uint16_t mA)`, `clear(bool show)`, `fill(CRGB, bool show)`, `setPixel(uint16_t, CRGB)`, `isInitialized()`, `getStats()`, `resetStats()`
- Virtual with defaults: `setDithering(bool)`, `isDitheringEnabled()`, `isShowInProgress()`

---

## File: `firmware-v3/src/hal/interface/INetworkDriver.h`

Namespace: `lightwaveos::hal`.

### Enums
- `enum class NetworkType { None, WiFiStation, WiFiAP, Ethernet, EspHosted }`
- `enum class NetworkState { Disconnected, Connecting, Connected, Failed, APMode }`

### Structs
- `struct NetworkStationConfig { ssid, password, timeoutMs, autoReconnect }`
- `struct NetworkAPConfig { ssid, password, channel, maxConnections }`
- `struct NetworkStats { connectAttempts, successfulConnects, disconnects, rssi, uptimeMs }`

### Aliases
- `using NetworkEventCallback = std::function<void(NetworkState)>`

### Interface `INetworkDriver`
- Pure virtual: `init()`, `deinit()`, `connect(const NetworkStationConfig&)`, `startAP(const NetworkAPConfig&)`, `disconnect()`, `getState()`, `isConnected()`, `getIP(uint8_t*)`, `getIPString(char*, size_t)`, `getType()`, `getMAC(uint8_t*)`, `setEventCallback(NetworkEventCallback)`, `getHostname()`, `setHostname(const char*)`, `getStats()`, `resetStats()`, `process()`
- Virtual with defaults: `scanNetworks()`, `getRSSI()`

---

## File: `firmware-v3/src/hal/led/FastLedDriver.h`

Namespace: `lightwaveos::hal`. **Legacy/parallel interface** — uses the `hal/led/ILedDriver.h` shape, NOT `hal/interface/ILedDriver.h`. See Anomalies.

### Class `FastLedDriver : public ILedDriver` (the `hal/led/` interface)
- Public: ctor `(const LedDriverConfig&)`, `~FastLedDriver()`, deleted copy / move
- Lifecycle: `init()`, `shutdown()`, `isReady()`
- Config: `getLedCount()`, `getCenterPoint()`, `getTopology()`
- Buffer: `setLed(uint16_t, RGB)`, `setLed(uint16_t, r, g, b)`, `getLed(uint16_t)`, `fill(RGB)`, `fillRange(start, count, RGB)`, `clear()`, `getBuffer()`, `getBuffer() const`
- Output: `show()`, `setBrightness(uint8_t)`, `getBrightness()`, `setMaxPower(uint8_t v, uint32_t mA)`
- Perf: `getLastShowTime()`, `getEstimatedFPS()`
- FastLED-specific: `getController(uint8_t stripIndex)`, `setDithering(bool)`, `setColorCorrection(uint32_t)`, `getPhysicalStripBuffer(uint8_t, RGB**, uint16_t*)`
- Private members: `m_config`, `m_buffer`, `m_totalLeds`, `m_stripBuffers[MAX_STRIPS]`, `m_stripStarts[MAX_STRIPS]`, `m_controllers[MAX_STRIPS]`, `m_initialized`, `m_brightness`, `m_powerVoltage`, `m_powerMilliamps`, `m_lastShowTimeUs`, `m_showCount`, `m_totalShowTimeUs`, `m_mutex` (`SemaphoreHandle_t`)
- Private methods: `mapLogicalToPhysical(uint16_t, uint8_t&, uint16_t&)`, `initializeFastLED()`, `syncBuffersToFastLED()`

### Header guard
- `LIGHTWAVEOS_HAL_FASTLED_DRIVER_H`

---

## File: `firmware-v3/src/hal/led/FastLedDriver.cpp`

Implements the methods declared above. No additional free symbols.

---

## File: `firmware-v3/src/hal/led/ILedDriver.h`

Namespace: `lightwaveos::hal`. **Legacy/parallel interface** (different shape from `hal/interface/ILedDriver.h`).

### Structs
- `struct RGB { r, g, b }` with `constexpr` ctors (default, `(r,g,b)`, packed `uint32_t`), `toPacked()`, `operator==`, `operator!=`, `scaled(uint8_t)`, static factories `Black()`, `White()`, `Red()`, `Green()`, `Blue()`, `Yellow()`, `Cyan()`, `Magenta()`
- `struct StripTopology { totalLeds, ledsPerStrip, stripCount, centerPoint, halfLength, isLeftHalf(uint16_t), isRightHalf(uint16_t), distanceFromCenter(uint16_t) }`

### Interface `ILedDriver` (this version)
- Pure virtual: `init()` (no args), `shutdown()`, `isReady()`, `getLedCount()`, `getCenterPoint()`, `getTopology()`, `setLed(uint16_t, RGB)`, `setLed(uint16_t, r, g, b)`, `getLed(uint16_t)`, `fill(RGB)`, `fillRange(start, count, RGB)`, `clear()`, `getBuffer()`, `getBuffer() const`, `show()`, `setBrightness(uint8_t)`, `getBrightness()`, `setMaxPower(uint8_t v, uint32_t mA)`, `getLastShowTime()`, `getEstimatedFPS()`

### Header guard
- `LIGHTWAVEOS_HAL_ILED_DRIVER_H`

---

## File: `firmware-v3/src/hal/led/LedDriverConfig.h`

Namespace: `lightwaveos::hal`.

### Enums
- `enum class ColorOrder : uint8_t { RGB, RBG, GRB, GBR, BRG, BGR }`
- `enum class LedType : uint8_t { WS2812, WS2811, SK6812, SK6812_RGBW, APA102, NEOPIXEL }`

### Structs
- `struct StripConfig { dataPin, clockPin, ledCount, colorOrder, ledType, reversed; }` with three constexpr ctors (default, `(pin, count, order)`, full)
- `struct LedDriverConfig { strips[MAX_STRIPS], stripCount, centerPoint, centerOriginEnabled, defaultBrightness, maxBrightness, powerVoltage, powerMilliamps, totalPowerBudget, targetFPS, enableDithering; }` with constexpr default ctor producing K1 v1 defaults, plus `getTotalLedCount()`, `getStripLedCount(uint8_t)`, `getStripStartIndex(uint8_t)`, `isValid()`

### Constants
- `MAX_STRIPS` (4)
- `LIGHTWAVEOS_V1_CONFIG` (constexpr instance)

### Free functions
- `constexpr LedDriverConfig createSingleStripConfig(uint8_t pin, uint16_t ledCount)`

### Header guard
- `LIGHTWAVEOS_HAL_LED_DRIVER_CONFIG_H`

---

## Anomalies & Naming Inconsistencies

1. **Two parallel `ILedDriver` interfaces.** `firmware-v3/src/hal/interface/ILedDriver.h` (used by `HalFactory`, `LedDriver_S3`, `LedDriver_P4`, `LedDriver_P4_RMT`) and `firmware-v3/src/hal/led/ILedDriver.h` (used by `FastLedDriver`) are entirely separate classes with the same name in the same namespace `lightwaveos::hal`. They differ in method shape (`init()` no-arg vs `init(LedStripConfig)`), buffer type (`RGB` vs `FastLED::CRGB`), config type, and topology API. This is a load-bearing collision waiting to bite — picking the wrong include silently changes which class you implement. Treat as **highest-priority rename target** in any SynqMatrix consolidation.

2. **Two `LedStripConfig` / `LedDriverConfig` shapes coexist.** `hal/interface/ILedDriver.h::LedStripConfig` (per-strip, used by S3/P4 drivers) is unrelated to `hal/led/LedDriverConfig.h::LedDriverConfig` (whole-system, used by FastLedDriver). British/American spelling also mixes: `colorOrder`, `colorCorrection`, `centerPoint`, `centerOriginEnabled` — these are public API. CLAUDE.md mandates British English in comments/docs/logs/UI, but the type names ship `color`/`center` Americanised across HAL. Renaming would be ABI-affecting.

3. **Member-prefix inconsistency.** `LedDriverConfig` (no `m_` prefix, struct of public fields) vs `LedDriver_S3` / `LedDriver_P4_RMT` / `FastLedDriver` / `DisplayActor` / `RM690B0Driver` (all `m_` prefix). Acceptable for plain config structs, but the convention should be documented.

4. **Static-constexpr naming split inside HAL.** `k*` (camelCase) inside `LedDriver_*` (`kMaxLedsPerStrip`, `kT0H`, `kRmtResolutionHz`) vs `SCREAMING_SNAKE_CASE` inside `DisplayActor` (`COLOR_BLACK`, `SCANLINE_PIXELS`) and `BitmapFont` (`GLYPH_WIDTH`, `ADVANCE`). Within a single subsystem this is jarring.

5. **`StatusStripTouch.h` is namespaceless.** Global free-function API (`statusStripTouchSetup`, `statusStripPollButton`, etc.) plus a global `enum class ButtonEvent`. Inconsistent with rest of HAL which is uniformly `lightwaveos::hal` / `lightwaveos::display`. NATIVE_BUILD gate also missing for `ButtonEvent`.

6. **`HalFactory.h` re-declares platform constants** (`PLATFORM_NAME`, `HAS_INTEGRATED_WIFI`, `HAS_ETHERNET`, `CPU_FREQ_MHZ`) that already exist in `chip_esp32s3.h` / `chip_esp32p4.h` under namespace `chip`. Both are reachable via include; pick one source of truth.

7. **Cross-cutting macro families** that span HAL+Config and should be considered together for SynqMatrix:
   - `FEATURE_*` (~50, `features.h`)
   - `CHIP_*` (`chip_config.h` — `CHIP_ESP32_P4`, `CHIP_ESP32_S3`, `CHIP_NAME`)
   - `K1_*` (S3 GPIO build-flag overrides — `K1_LED_STRIP1_DATA`, `K1_I2S_BCLK`, etc.; only present on S3, not P4)
   - `TRACE_*` (MabuTrace shim — same names act as either macros or no-ops)
   - `FIRMWARE_VERSION_*`
   - `WIFI_*` / `AP_*` / `OTA_*` / `API_*` build-flag overrides
   - `LIGHTWAVEOS_HAL_*` header guards (only `hal/led/` uses guards; rest of HAL uses `#pragma once`)

8. **`runtime_state.h` exposes legacy `g_*` globals** (`g_factoryPresetIndex`, `g_externalNvsSaveRequest`, `zoneConfigMgr`) — mixed snake_case + Hungarian-ish prefix, no namespace for `zoneConfigMgr` (which is `lightwaveos::persistence::ZoneConfigManager*`). `persistence_trigger.h` is a forwarding shim that exists purely for backward compatibility.

9. **`MICROPHONE_TYPE` hardcoded to `SPH0645`** in `audio_config.h:49` despite `IM69D130` existing in the enum. Backend choice is compile-time across multiple disjoint code paths in `audio_config.h` (`FEATURE_AUDIO_BACKEND_ESV11`, `_ESV11_32KHZ`, `_PIPELINECORE`, `_SPINE16K`, plus `CHIP_ESP32_P4` overrides). Sample rates and hop sizes diverge per backend. Naming is consistent but the orthogonality of features here is a refactor hazard.

10. **OS3 vs P4 vs S3 splits.** Audio (S3 12.8 kHz / 32 kHz, P4 16 kHz) and chip-specific GPIO assignments live in `chip_esp32s3.h`, `chip_esp32p4.h`, and `chip_amoled241.h` — the latter is a S3 variant that pulls in display-specific constants `AMOLED_*`, `TOUCH_*`. P4 lacks `TTP223`, AMOLED rig has it forced to `-1`. P4 also lacks `K1_*` build-flag override macros entirely (its pins are hard-coded). No `K1_AMOLED_*` override family exists; AMOLED 2.41 rig pins are fixed.

11. **`DisplayActor` is a real Actor class** (inherits `actors::Actor`, has `onStart`/`onMessage`/`onTick`/`onStop`), confirming the upstream note that N05 missed it. It lives under `hal/display/` rather than `core/actors/`, which is unusual — every other Actor implementation is in `core/actors/`.

12. **Anomalous filename.** `LedDriver_P4_RMT.h` includes a hard `#error` if not building for P4; build-system mistake will produce a confusing compile failure rather than a graceful no-op. The mirror file for S3 (`LedDriver_S3.h`) has no such guard.
