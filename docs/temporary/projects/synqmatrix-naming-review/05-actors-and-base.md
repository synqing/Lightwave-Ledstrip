---
abstract: "SynqMatrix naming-review extraction N05 — Actor base infrastructure plus non-Renderer actors in core/actors/. Catalogues namespaces, enums, structs, classes, methods, message types, and cross-actor send patterns for Actor base, ActorSystem singleton, ShowDirectorActor, and compatibility stubs (NodeOrchestrator, RendererNode). Read when planning rename of any actor-layer symbol or auditing message wiring."
---

# SSA-N05 — Actors & Base Infrastructure (excluding RendererActor)

Read-only naming extraction. Scope: every `.h` and `.cpp` under `firmware-v3/src/core/actors/` except `RendererActor.{h,cpp}` (covered by N04). Names + signatures only.

Files in scope:
- `firmware-v3/src/core/actors/Actor.h`
- `firmware-v3/src/core/actors/Actor.cpp`
- `firmware-v3/src/core/actors/ActorSystem.h`
- `firmware-v3/src/core/actors/ActorSystem.cpp`
- `firmware-v3/src/core/actors/ShowDirectorActor.h`
- `firmware-v3/src/core/actors/ShowDirectorActor.cpp`
- `firmware-v3/src/core/actors/NodeOrchestrator.h`
- `firmware-v3/src/core/actors/RendererNode.h`

Out of scope (other dirs): `firmware-v3/src/audio/AudioActor.{h,cpp}` (N02), `firmware-v3/src/hal/display/DisplayActor.{h,cpp}`, `firmware-v3/src/plugins/PluginManagerActor.{h,cpp}`, `firmware-v3/src/sync/SyncManagerActor.{h,cpp}`. No `CommandActor` exists in the tree.

---

## Namespace structure

| Namespace | Where introduced | Contents |
|---|---|---|
| `lightwaveos` | top-level | enclosing namespace for all actor symbols |
| `lightwaveos::actors` | `Actor.h` | `MessageType`, `Message`, `ActorConfig`, `Actor`, `SystemState`, `SystemStats`, `ActorSystem`, `ShowDirectorActor` |
| `lightwaveos::actors::ActorConfigs` | `Actor.h` | predefined config factories `Renderer`, `Network`, `Hmi`, `StateStore`, `SyncManager`, `PluginManager` |
| `lightwaveos::nodes` | `NodeOrchestrator.h`, `RendererNode.h` | legacy aliases `NodeOrchestrator = actors::ActorSystem`, `RendererNode = actors::RendererActor` |
| anonymous in `ShowDirectorActor.cpp` (guarded by `FEATURE_AUDIO_SYNC`) | `ShowDirectorActor.cpp` | `TrinitySegmentIntent` struct, `hashLabel16`, `intentForLabelHash`, `toNarrativePhase` |
| anonymous in `ActorSystem.cpp` (guarded by `FEATURE_AUDIO_SYNC`) | `ActorSystem.cpp` | `kDefaultStimulusFrame` const |

External symbols touched: `lightwaveos::audio::AudioActor`, `lightwaveos::audio::ControlBusFrame`, `lightwaveos::audio::SnapshotBuffer<T>`, `lightwaveos::display::DisplayActor`, `lightwaveos::effects::NarrativePhase` (`PHASE_BUILD`, `PHASE_HOLD`, `PHASE_RELEASE`, `PHASE_REST`), `lightwaveos::synqmatrix::SynqMatrix`, `lightwaveos::actors::bus::MessageBus`, `lightwaveos::actors::narrative::NarrativeEngine`, `lightwaveos::actors::shows::CueScheduler`, `lightwaveos::actors::shows::ParameterSweeper`. Free-floating types referenced: `EffectId`, `ParamId` (`PARAM_BRIGHTNESS`, `PARAM_SPEED`, `PARAM_INTENSITY`, `PARAM_SATURATION`, `PARAM_COMPLEXITY`, `PARAM_VARIATION`), `ZONE_GLOBAL`, `ShowDefinition`, `ShowCue`, `ShowChapter`, `ShowPlaybackState`, `BUILTIN_SHOWS`, `BUILTIN_SHOW_COUNT`, `SHOW_PHASE_BUILD`, `SHOW_PHASE_HOLD`, `SHOW_PHASE_RELEASE`, `SHOW_PHASE_REST`, `CUE_EFFECT`, `CUE_PARAMETER_SWEEP`, `CUE_PALETTE`, `CUE_NARRATIVE`, `CUE_TRANSITION`, `CUE_ZONE_CONFIG`, `CUE_MARKER`, `LedConfig::TARGET_FPS`.

---

## Block 1 — `enum class MessageType : uint8_t` (Actor.h:50–157)

Full enumerator inventory (canonical hex assignments):

| Block | Enumerator | Value | Notes |
|---|---|---|---|
| Effect commands 0x00–0x1F | `SET_EFFECT` | `0x00` | |
| | `SET_BRIGHTNESS` | `0x01` | |
| | `SET_SPEED` | `0x02` | |
| | `SET_PALETTE` | `0x03` | |
| | `SET_SATURATION` | `0x04` | |
| | `SET_INTENSITY` | `0x05` | |
| | `SET_COMPLEXITY` | `0x06` | |
| | `SET_VARIATION` | `0x07` | |
| | `SET_HUE` | `0x08` | |
| | `SET_MOOD` | `0x09` | Sensory Bridge mood |
| | `SET_FADE_AMOUNT` | `0x0A` | |
| | `SET_EDGE_MIXER_MODE` | `0x0B` | |
| | `SET_EDGE_MIXER_SPREAD` | `0x0C` | |
| | `SET_EDGE_MIXER_STRENGTH` | `0x0D` | |
| | `SAVE_EDGE_MIXER_NVS` | `0x0E` | |
| | `SET_EDGE_MIXER_SPATIAL` | `0x0F` | |
| | `SET_EDGE_MIXER_TEMPORAL` | `0x10` | |
| Input Merge Layer 0x11–0x1F | `MERGE_SUBMIT` | `0x11` | param1=sourceId, param2=paramIndex, param3=value |
| | `SET_LED_DITHERING` | `0x12` | |
| Zone commands 0x20–0x3F | `ZONE_ENABLE` | `0x20` | |
| | `ZONE_DISABLE` | `0x21` | |
| | `ZONE_SET_EFFECT` | `0x22` | |
| | `ZONE_SET_PALETTE` | `0x23` | |
| | `ZONE_SET_BRIGHTNESS` | `0x24` | |
| | `ZONE_SET_COUNT` | `0x25` | |
| Transition commands 0x40–0x5F | `TRIGGER_TRANSITION` | `0x40` | |
| | `SET_TRANSITION_TYPE` | `0x41` | |
| | `SET_TRANSITION_TIME` | `0x42` | |
| | `CANCEL_TRANSITION` | `0x43` | |
| | `START_TRANSITION` | `0x44` | param1=effectId, param2=transitionType, param4=durationMs |
| System commands 0x60–0x7F | `SHUTDOWN` | `0x60` | |
| | `HEALTH_CHECK` | `0x61` | default `Message()` ctor type |
| | `RESET_STATE` | `0x62` | |
| | `SAVE_STATE` | `0x63` | |
| | `LOAD_STATE` | `0x64` | |
| | `PING` | `0x65` | |
| | `PONG` | `0x66` | |
| Sync commands 0x68–0x6F | `SYNC_REQUEST` | `0x68` | |
| | `SYNC_RESPONSE` | `0x69` | |
| | `SYNC_STATE` | `0x6A` | |
| Show control 0x70–0x7F | `SHOW_LOAD` | `0x70` | |
| | `SHOW_START` | `0x71` | |
| | `SHOW_STOP` | `0x72` | |
| | `SHOW_PAUSE` | `0x73` | |
| | `SHOW_RESUME` | `0x74` | |
| | `SHOW_SEEK` | `0x75` | param4 = timeMs |
| | `SHOW_UNLOAD` | `0x76` | |
| Events 0x80–0xFF | `EFFECT_CHANGED` | `0x80` | |
| | `FRAME_RENDERED` | `0x81` | |
| | `STATE_UPDATED` | `0x82` | |
| | `PALETTE_CHANGED` | `0x83` | |
| | `ZONE_CHANGED` | `0x84` | |
| | `TRANSITION_COMPLETE` | `0x85` | |
| | `ERROR_OCCURRED` | `0x86` | |
| | `HEALTH_STATUS` | `0x87` | |
| Degraded-mode events 0x88–0x8F (DEC-011) | `AUDIO_FAILURE_DETECTED` | `0x88` | |
| | `AUDIO_FAILURE_RECOVERED` | `0x89` | |
| HMI events 0x90–0x9F | `ENCODER_ROTATED` | `0x90` | |
| | `ENCODER_PRESSED` | `0x91` | |
| | `ENCODER_RELEASED` | `0x92` | |
| Network events 0xA0–0xAF | `CLIENT_CONNECTED` | `0xA0` | |
| | `CLIENT_DISCONNECTED` | `0xA1` | |
| | `COMMAND_RECEIVED` | `0xA2` | |
| Show events 0xB0–0xBF | `SHOW_STARTED` | `0xB0` | |
| | `SHOW_STOPPED` | `0xB1` | |
| | `SHOW_PAUSED` | `0xB2` | |
| | `SHOW_RESUMED` | `0xB3` | |
| | `SHOW_CHAPTER_CHANGED` | `0xB4` | |
| | `SHOW_COMPLETED` | `0xB5` | |
| Audio events 0xC0–0xCF (Phase 2) | `AUDIO_TEMPO_ESTIMATE` | `0xC0` | param4 = bpm × 100 fixed-point |
| | `AUDIO_BEAT_OBSERVATION` | `0xC1` | param1=strength, param2=is_downbeat |
| | `AUDIO_BANDS_UPDATED` | `0xC2` | |
| | `AUDIO_ERROR` | `0xC3` | param1 = error code |
| Trinity sync 0xD0–0xDF | `TRINITY_BEAT` | `0xD0` | param1=bpm_hi, param2=bpm_lo, param3=phase, param4=flags |
| | `TRINITY_MACRO` | `0xD1` | param1–4 packed |
| | `TRINITY_SYNC` | `0xD2` | param1=action, param4=position_ms |
| | `TRINITY_SEGMENT` | `0xD3` | param1=index, param2-3=labelHash16, param4=startMs, _reserved=endMs |
| Stimulus | `STIMULUS_SET_MODE` | `0xE0` | |
| | `STIMULUS_CLEAR` | `0xE1` | |

Anomaly: the values `0x86` (`ERROR_OCCURRED`) and `0xC3` (`AUDIO_ERROR`) both serialise an "error" semantic; the spelling is `ERROR_OCCURRED` vs `AUDIO_ERROR` (no `OCCURRED` suffix on the audio variant). No `STIMULUS_*` block header comment block exists — the values sit in `0xE0`/`0xE1`, breaking the otherwise-consistent block headers (next named block would have been Stimulus 0xE0–0xEF).

---

## Block 2 — `struct Message` (Actor.h:178–214)

Fixed 16-byte layout, `static_assert(sizeof(Message) == 16, ...)`.

Members (in declaration order):

| Field | Type | Width | Note |
|---|---|---|---|
| `type` | `MessageType` | 1 | |
| `param1` | `uint8_t` | 1 | |
| `param2` | `uint8_t` | 1 | |
| `param3` | `uint8_t` | 1 | |
| `param4` | `uint32_t` | 4 | extended parameter |
| `timestamp` | `uint32_t` | 4 | `millis()` at construction |
| `_reserved` | `uint32_t` | 4 | future use / padding |

Constructors:
- `Message()` — defaults to `HEALTH_CHECK`, all params 0.
- `Message(MessageType t, uint8_t p1 = 0, uint8_t p2 = 0, uint8_t p3 = 0, uint32_t p4 = 0)` — timestamp populated from `millis()`.

Methods:
- `bool isCommand() const` — `type < 0x80`
- `bool isEvent() const` — `type >= 0x80`

---

## Block 3 — `struct ActorConfig` (Actor.h:236–260)

Members: `const char* name`; `uint16_t stackSize` (in words ×4 = bytes); `uint8_t priority`; `BaseType_t coreId`; `uint8_t queueSize`; `TickType_t tickInterval`.

Constructors:
- `ActorConfig()` — `name="Actor"`, `stackSize=2048`, `priority=2`, `coreId=0`, `queueSize=16`, `tickInterval=0`.
- `ActorConfig(const char* n, uint16_t stack, uint8_t prio, BaseType_t core, uint8_t qSize, TickType_t tick = 0)`.

### `namespace ActorConfigs` factories (Actor.h:468–590)

All `inline ActorConfig` returning a populated `ActorConfig`. Task name in quotes, then `(stackWords, priority, coreId, queueSize, tickIntervalTicks)`:

| Factory | Task name | Stack words | Priority | Core | Queue | Tick |
|---|---|---|---|---|---|---|
| `Renderer()` | `"Renderer"` | 4096 | 5 | 1 | 32 | 0 (self-clocked) |
| `Network()` | `"Network"` | 3072 | 3 | 0 | 16 | 0 (event-driven) |
| `Hmi()` | `"Hmi"` | 2048 | 2 | 0 | 16 | `pdMS_TO_TICKS(20)` (50 Hz) |
| `StateStore()` | `"StateStore"` | 2048 | 2 | 1 | 16 | 0 (event-driven) |
| `SyncManager()` | `"SyncManager"` | 8192 | 2 | 0 | 16 | `pdMS_TO_TICKS(100)` |
| `PluginManager()` | `"PluginMgr"` | 2048 | 2 | 0 | 16 | 0 |

Anomaly: `PluginManager()` factory returns task name `"PluginMgr"` (abbreviated) — the abbreviation contradicts all the other factories that use the full role name as the FreeRTOS task name.

---

## Block 4 — `class Actor` (Actor.h:281–462, impl Actor.cpp)

Abstract base. Non-copyable (`Actor(const Actor&) = delete`, `operator= = delete`).

### Public methods

| Signature | Purpose |
|---|---|
| `explicit Actor(const ActorConfig& config)` | constructor — creates `m_queue` via `xQueueCreate(queueSize, sizeof(Message))` |
| `virtual ~Actor()` | calls `stop()` if running, deletes queue |
| `bool start()` | creates task via `xTaskCreatePinnedToCore`, parameter is `this`, trampoline `taskFunction` |
| `void stop()` | sends `SHUTDOWN` message, waits up to 100 ms, falls back to `vTaskDelete` |
| `bool isRunning() const` | inline `{ return m_running; }` |
| `bool send(const Message& msg, TickType_t timeout = 0)` | `xQueueSend`; logs `ESP_LOGW` if queue > 80 % full |
| `bool sendFromISR(const Message& msg)` | `xQueueSendFromISR` with `portYIELD_FROM_ISR` on wake |
| `UBaseType_t getQueueLength() const` | `uxQueueMessagesWaiting` |
| `uint8_t getQueueUtilization() const` | `(length * 100) / queueSize` |
| `const char* getName() const` | inline returns `m_config.name` |
| `BaseType_t getCoreId() const` | inline returns `m_config.coreId` |
| `UBaseType_t getStackHighWaterMark() const` | `uxTaskGetStackHighWaterMark(m_taskHandle)` |
| `uint32_t getMessageCount() const` | inline returns `m_messageCount` |

### Protected virtuals (override-points)

| Signature | Default | Note |
|---|---|---|
| `virtual void onStart()` | empty | called once before main loop |
| `virtual void onMessage(const Message& msg) = 0` | pure | mandatory dispatch hook |
| `virtual void onTick()` | empty | only invoked when `tickInterval == 0` or on receive-timeout |
| `virtual void onStop()` | empty | invoked on shutdown after main loop exits |

### Protected utilities

| Signature | Purpose |
|---|---|
| `uint32_t getTickCount() const` | `xTaskGetTickCount()` |
| `void sleep(uint32_t ms)` | `vTaskDelay(pdMS_TO_TICKS(ms))` |
| `const ActorConfig& getConfig() const` | inline |

### Private members & implementation

| Symbol | Type | Note |
|---|---|---|
| `static void taskFunction(void* param)` | static | FreeRTOS trampoline → `static_cast<Actor*>(param)->run()` |
| `void run()` | private | main message loop; documented "DRAIN_THRESHOLD = 50", "MAX_MESSAGES_PER_TICK = 4"; calls `esp_task_wdt_reset()` + `taskYIELD()` after each drained message |
| `m_config` | `ActorConfig` | copied in constructor |
| `m_taskHandle` | `TaskHandle_t` | nullptr until `start()` |
| `m_queue` | `QueueHandle_t` | allocated in ctor |
| `m_running` | `volatile bool` | "atomic on ESP32" per comment |
| `m_shutdownRequested` | `volatile bool` | |
| `m_messageCount` | `uint32_t` | diagnostic counter |

### `run()` semantic constants (Actor.cpp:299–300)

| Local constant | Value | Effect |
|---|---|---|
| `DRAIN_THRESHOLD` | `50` | drain loop activates above 50 % queue utilisation |
| `MAX_MESSAGES_PER_TICK` | `4` | bound on messages dispatched per drain cycle (comment notes tightened from 8) |

### `tickInterval` semantics (Actor.cpp:349–364)

| Value | Behaviour |
|---|---|
| `> 0` | periodic tick mode — `xQueueReceive` waits up to `tickInterval`; on timeout calls `onTick()` |
| `== 0` | self-clocked — `xQueueReceive` non-blocking poll; `onTick()` always called (expected to block internally, e.g. `AudioActor` I2S read) |
| `portMAX_DELAY` | "wait forever for messages only" — never reaches `onTick()` |

Anomaly: the conditional at line 383 `if (m_config.tickInterval == 0 || m_config.tickInterval > 0)` is always true for any non-negative tick interval — the documented `portMAX_DELAY` branch can never call `onTick()`, but it cannot anyway (only reached on receive-timeout, which never fires for `portMAX_DELAY`). The redundant disjunction is dead-code-safe but reads as a bug.

---

## Block 5 — `enum class SystemState` (ActorSystem.h:57–63)

`uint8_t`-backed. Enumerators: `UNINITIALIZED = 0`, `STARTING`, `RUNNING`, `STOPPING`, `STOPPED`.

## Block 6 — `struct SystemStats` (ActorSystem.h:68–81)

Members: `uint32_t uptimeMs`, `uint32_t totalMessages`, `uint32_t heapFreeBytes`, `uint32_t heapMinFreeBytes`, `uint32_t spiramFreeBytes`, `uint8_t activeActors`. Default ctor zeroes all fields.

---

## Block 7 — `class ActorSystem` singleton (ActorSystem.h:98–419, impl ActorSystem.cpp)

Singleton — `static ActorSystem& instance()` (function-local static at ActorSystem.cpp:35–39). Non-copyable.

### Lifecycle

| Signature | Note |
|---|---|
| `bool init()` | creates `m_renderer`, `m_showDirector`, optionally `m_audio` (FEATURE_AUDIO_SYNC) and `m_display` (FEATURE_AMOLED_DISPLAY) via `std::make_unique`; transitions state `UNINITIALIZED → STARTING` |
| `bool start()` | auto-invokes `init()` if needed; starts actors in order Renderer → ShowDirector → Audio → Display; wires `m_renderer->setAudioBuffer(&m_audio->getControlBusBuffer())`, `m_renderer->setStimulusBuffer(&m_stimulusControlBusBuffer)`, and on non-ESV11/non-PipelineCore backends `m_renderer->setTempo(&m_audio->getTempoMut())` |
| `void shutdown()` | reverse order: clears renderer's audio/stimulus pointers, stops Audio, Display, ShowDirector, Renderer |
| `SystemState getState() const` | inline |
| `bool isRunning() const` | inline `m_state == RUNNING` |

### Actor accessors

| Signature | Returns |
|---|---|
| `RendererActor* getRenderer()` / `const RendererActor* getRenderer() const` | `m_renderer.get()` |
| `ShowDirectorActor* getShowDirector()` / const overload | `m_showDirector.get()` |
| `lightwaveos::audio::AudioActor* getAudio()` / const overload | `m_audio.get()` (FEATURE_AUDIO_SYNC) |
| `lightwaveos::display::DisplayActor* getDisplay()` | `m_display.get()` (FEATURE_AMOLED_DISPLAY) |

Header comments reserve future getters: `getNetwork()`, `getHmi()`, `getStateStore()`, `getSyncManager()`, `getPluginManager()` — none currently exist.

### Convenience commands → all `bool` returns, all send to `m_renderer` with timeout `pdMS_TO_TICKS(10)`

| Signature | MessageType emitted | Param mapping |
|---|---|---|
| `bool setEffect(EffectId effectId)` | `SET_EFFECT` | param1=low byte, param2=high byte. Queue-backpressure rejection at ≥ 90 % |
| `bool startTransition(EffectId effectId, uint8_t transitionType)` | `START_TRANSITION` | param1/2 split effectId, param3=transitionType. 90 % backpressure. |
| `bool setBrightness(uint8_t brightness)` | `SET_BRIGHTNESS` | param1=brightness. 90 % backpressure. |
| `bool setSpeed(uint8_t speed)` | `SET_SPEED` | param1=speed. 90 % backpressure. |
| `bool setPalette(uint8_t paletteIndex)` | `SET_PALETTE` | param1=palette. No backpressure check. Contains agent log instrumentation writing `.cursor/debug.log`. |
| `bool setIntensity(uint8_t intensity)` | `SET_INTENSITY` | param1. No backpressure. |
| `bool setSaturation(uint8_t saturation)` | `SET_SATURATION` | param1. No backpressure. |
| `bool setComplexity(uint8_t complexity)` | `SET_COMPLEXITY` | param1. No backpressure. |
| `bool setVariation(uint8_t variation)` | `SET_VARIATION` | param1. No backpressure. |
| `bool setHue(uint8_t hue)` | `SET_HUE` | param1. |
| `bool setMood(uint8_t mood)` | `SET_MOOD` | param1. |
| `bool setFadeAmount(uint8_t fadeAmount)` | `SET_FADE_AMOUNT` | param1. |
| `bool setEdgeMixerMode(uint8_t mode)` | `SET_EDGE_MIXER_MODE` | param1. |
| `bool setEdgeMixerSpread(uint8_t spread)` | `SET_EDGE_MIXER_SPREAD` | param1. |
| `bool setEdgeMixerStrength(uint8_t strength)` | `SET_EDGE_MIXER_STRENGTH` | param1. |
| `bool setEdgeMixerSpatial(uint8_t spatial)` | `SET_EDGE_MIXER_SPATIAL` | param1. |
| `bool setEdgeMixerTemporal(uint8_t temporal)` | `SET_EDGE_MIXER_TEMPORAL` | param1. |
| `bool saveEdgeMixerToNVS()` | `SAVE_EDGE_MIXER_NVS` | no params. |
| `bool setLedDithering(bool enabled)` | `SET_LED_DITHERING` | param1 = 0/1. |

### Trinity sync commands (guarded by `FEATURE_AUDIO_SYNC`)

| Signature | MessageType | Encoding |
|---|---|---|
| `bool trinityBeat(float bpm, float phase01, bool tick, bool downbeat, int beatInBar)` | `TRINITY_BEAT` | bpm×100 split across param1/2, phase×255→param3, flags packed into param4 (bit0=tick, bit1=downbeat, bits2–3=beatInBar) |
| `bool trinityMacro(float energy, float vocal, float bass, float perc, float bright)` | `TRINITY_MACRO` | each value clamped [0,1] × 255 to uint8_t; param1=energy, param2=vocal, param3=bass, param4 = (perc<<24) \| (bright<<16) |
| `bool trinitySync(uint8_t action, float positionSec, float bpm = 120.0f)` | `TRINITY_SYNC` | param1=action, param2/3=bpm×100 split, param4=positionMs |
| `bool trinitySegment(uint8_t index, uint16_t labelHash16, float startSec, float endSec)` | `TRINITY_SEGMENT` | param1=index, param2/3=labelHash16 hi/lo, param4=startMs, `_reserved`=endMs |

### Stimulus API (guarded by `FEATURE_AUDIO_SYNC`)

| Signature | Notes |
|---|---|
| `uint8_t getStimulusMode() const` | returns `m_stimulusMode` |
| `bool setStimulusMode(uint8_t mode)` | sends `STIMULUS_SET_MODE`; updates `m_stimulusMode` on success |
| `bool clearStimulus()` | mutex-protected; resets `m_stimulusLastFrame` to `kDefaultStimulusFrame`, republishes, sends `STIMULUS_CLEAR` |
| `bool copyStimulusFrame(lightwaveos::audio::ControlBusFrame& out) const` | mutex-protected copy of `m_stimulusLastFrame` |
| `bool publishStimulusFrame(const lightwaveos::audio::ControlBusFrame& frame)` | mutex-protected; publishes into `m_stimulusControlBusBuffer` |
| `const lightwaveos::audio::SnapshotBuffer<lightwaveos::audio::ControlBusFrame>& getStimulusControlBusBuffer() const` | accessor |

### Diagnostics

| Signature | Note |
|---|---|
| `SystemStats getStats() const` | reads `MessageBus::instance().getTotalPublished()`, `esp_get_free_heap_size`, `esp_get_minimum_free_heap_size`, `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)`; counts `isRunning()` actors |
| `void printStatus()` | `Serial.printf` dump including renderer FPS, frame times, `LedDriverStats` fields `avgShowUs`/`maxShowUs`/`showSkips`, ShowDirector status, `MessageBus::dumpSubscriptions()` |
| `uint32_t getUptimeMs() const` | `millis() - m_startTime`; returns 0 unless `m_state == RUNNING` |

### Private members

| Member | Type |
|---|---|
| `m_renderer` | `std::unique_ptr<RendererActor>` |
| `m_showDirector` | `std::unique_ptr<ShowDirectorActor>` |
| `m_audio` | `std::unique_ptr<lightwaveos::audio::AudioActor>` (FEATURE_AUDIO_SYNC) |
| `m_display` | `std::unique_ptr<lightwaveos::display::DisplayActor>` (FEATURE_AMOLED_DISPLAY) |
| `m_state` | `SystemState` |
| `m_startTime` | `uint32_t` |
| `m_stimulusControlBusBuffer` | `lightwaveos::audio::SnapshotBuffer<lightwaveos::audio::ControlBusFrame>` (audio-sync only) |
| `m_stimulusLastFrame` | `lightwaveos::audio::ControlBusFrame` |
| `m_stimulusLastPublishMs` | `uint32_t = 0` |
| `m_stimulusMode` | `uint8_t = 0` |
| `m_stimulusMutex` | `SemaphoreHandle_t = nullptr` |

Header comments reserve future `m_network`, `m_hmi`, `m_stateStore`, `m_syncManager`, `m_pluginManager` — none declared.

Anomaly: `setPalette` (ActorSystem.cpp:429–468) writes JSON debug records to a hard-coded host path `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.cursor/debug.log` via `fopen`. This is unreachable on-device (no such path) but compiles and silently fails — agent instrumentation residue. It is the only convenience-command function with no 90 %-backpressure check and the only one that opens a `FILE*`.

---

## Block 8 — `class ShowDirectorActor : public Actor` (ShowDirectorActor.h:46–165, impl ShowDirectorActor.cpp)

Config from constructor body: name `"ShowDirector"`, stackSize `3072` (12 KB), priority `2`, coreId `0`, queueSize `16`, tickInterval `pdMS_TO_TICKS(50)` (20 Hz).

### Public lifecycle / accessors

| Signature | Note |
|---|---|
| `ShowDirectorActor()` | constructor — installs static `s_instance = this`, calls `m_state.reset()` |
| `~ShowDirectorActor() override` | clears `s_instance` |
| `bool hasShow() const` | inline `m_currentShow != nullptr` |
| `bool isPlaying() const` | inline `m_state.playing && !m_state.paused` |
| `bool isPaused() const` | inline `m_state.paused` |
| `float getProgress() const` | elapsed / `showCopy.totalDurationMs` |
| `uint8_t getCurrentChapter() const` | inline `m_state.currentChapterIndex` |
| `uint8_t getCurrentShowId() const` | inline `m_state.currentShowId` |
| `uint32_t getElapsedMs() const` | inline forwards to `m_state.getElapsedMs()` |
| `uint32_t getRemainingMs() const` | `totalDurationMs - elapsed` else 0 |

### Protected Actor overrides

| Signature | Behaviour |
|---|---|
| `void onStart() override` | acquires `m_rendererActor` from `ActorSystem::instance().getRenderer()`, grabs `NarrativeEngine::getInstance()`, subscribes to `TRINITY_SEGMENT` on the `MessageBus` (FEATURE_AUDIO_SYNC) |
| `void onMessage(const Message& msg) override` | dispatches `SHOW_LOAD`, `SHOW_START`, `SHOW_STOP`, `SHOW_PAUSE`, `SHOW_RESUME`, `SHOW_SEEK`, `SHOW_UNLOAD`, `SHUTDOWN`, `TRINITY_SEGMENT` (FEATURE_AUDIO_SYNC) |
| `void onTick() override` | if playing & not paused, marks `SynqMatrix::instance().markShowControl(millis())` (FEATURE_AUDIO_SYNC) and calls `updateShow()`; then `m_paramSweeper.update(millis())` |
| `void onStop() override` | unsubscribes from MessageBus (FEATURE_AUDIO_SYNC), `stopShow()`, `unloadShow()` |

### Private show-control methods

`bool loadShow(const ShowDefinition* show)`, `bool loadShowById(uint8_t showId)`, `void unloadShow()`, `void startShow()`, `void stopShow()`, `void pauseShow()`, `void resumeShow()`, `void seekShow(uint32_t timeMs)`.

### Private update methods

`void updateShow()`, `void executeCue(const ShowCue& cue)`, `void updateChapter(uint32_t elapsedMs)`, `void handleShowEnd()`, `uint8_t getChapterForTime(uint32_t timeMs) const`.

`executeCue` dispatches on `cue.type`:
- `CUE_EFFECT` → emits `START_TRANSITION` (if `transitionType != 0`) or `SET_EFFECT` to renderer; same 2-byte effectId encoding as `ActorSystem::setEffect`.
- `CUE_PARAMETER_SWEEP` → `m_paramSweeper.startSweepFromCurrent(ParamId, targetZone, targetValue, durationMs)`.
- `CUE_PALETTE` → emits `SET_PALETTE`.
- `CUE_NARRATIVE` → `modulateNarrative(cue.narrativePhase(), cue.narrativeTempoMs())`.
- `CUE_TRANSITION` → no-op (TODO comment).
- `CUE_ZONE_CONFIG` → no-op (TODO).
- `CUE_MARKER` → no-op.

### Private narrative methods

`void modulateNarrative(uint8_t phase, uint8_t tension)` — maps `tension` 0–255 to tempo `8000ms..2000ms`; calls `m_narrative->setTempo(tempoSeconds)`; maps phase byte to `PHASE_BUILD/HOLD/RELEASE/REST`; tension scales duration `30000..5000 ms` (min 1000 ms); calls `setNarrativePhase`.

`void setNarrativePhase(lightwaveos::effects::NarrativePhase phase, uint32_t durationMs)` — `m_narrative->setPhase(phase, durationMs)`.

### Trinity bridge (FEATURE_AUDIO_SYNC)

`void handleTrinitySegment(const Message& msg)` — gated on `!m_state.playing`; de-dupes via `m_lastTrinitySegmentIndex`/`m_lastTrinitySegmentLabelHash`; looks up `TrinitySegmentIntent` via `intentForLabelHash(labelHash16)`; calls `m_narrative->setTempo(...)`, `setNarrativePhase(toNarrativePhase(intent.showPhase), phaseDurationMs)`, `m_paramSweeper.startSweepFromCurrent(PARAM_BRIGHTNESS, ZONE_GLOBAL, intent.brightness, sweepMs)`, same for `PARAM_SPEED`, then `sendToRenderer(Message(MessageType::SET_MOOD, intent.mood))`.

State fields: `uint8_t m_lastTrinitySegmentIndex = 0xFF;` and `uint16_t m_lastTrinitySegmentLabelHash = 0;`.

#### `TrinitySegmentIntent` table (anonymous namespace constants)

Hash labels recognised: `start`, `intro`, `verse`, `chorus`, `solo`, `inst`, `bridge`, `breakdown`, `drop`, `end`. Per-label intent fields `known/showPhase/tension/brightness/speed/mood/sweepMs`.

Helper `uint16_t hashLabel16(const char* label)` — 32-bit FNV-1a folded XOR-fold to 16-bit.

Helper `lightwaveos::effects::NarrativePhase toNarrativePhase(uint8_t showPhase)` — maps `SHOW_PHASE_BUILD/HOLD/RELEASE/REST` to `PHASE_BUILD/HOLD/RELEASE/REST`, default `PHASE_BUILD`.

### Static ParameterSweeper callbacks

`static void applyParamValue(ParamId param, uint8_t zone, uint8_t value)` — switches on `PARAM_BRIGHTNESS/SPEED/INTENSITY/SATURATION/COMPLEXITY/VARIATION` and sends the matching `SET_*` MessageType to `s_instance->sendToRenderer(msg)`. Note: `zone` parameter unused — there is no per-zone routing in the current dispatch.

`static uint8_t getParamValue(ParamId param, uint8_t zone)` — reads back from `RendererActor::getBrightness()`/`getSpeed()`; INTENSITY/SATURATION/COMPLEXITY/VARIATION return literal 128/255/128/0 with `// Future:` comments.

### Helper sends

`void sendToRenderer(const Message& msg)` — marks `SynqMatrix::instance().markShowControl(millis())` when playing+unpaused (FEATURE_AUDIO_SYNC); forwards via `m_rendererActor->send(msg, pdMS_TO_TICKS(10))`.

`void publishShowEvent(MessageType eventType, uint8_t param1 = 0, uint8_t param2 = 0)` — wraps and publishes on `bus::MessageBus::instance().publish(evt)`. Used to emit `SHOW_STARTED`, `SHOW_STOPPED`, `SHOW_PAUSED`, `SHOW_RESUMED`, `SHOW_CHAPTER_CHANGED`, `SHOW_COMPLETED`.

### Member variables

| Member | Type | Note |
|---|---|---|
| `m_currentShow` | `const ShowDefinition*` | PROGMEM pointer; `memcpy_P` copies to local `showCopy` before access |
| `m_state` | `ShowPlaybackState` | external struct |
| `m_cueScheduler` | `shows::CueScheduler` | external |
| `m_paramSweeper` | `shows::ParameterSweeper` | external; ctor takes `applyParamValue` and `getParamValue` callbacks |
| `m_cueBuffer` | `ShowCue[shows::CueScheduler::MAX_CUES_PER_FRAME]` | per-frame staging |
| `m_rendererActor` | `Actor*` | held as base pointer; cast to `RendererActor*` in `getParamValue` |
| `m_narrative` | `narrative::NarrativeEngine*` | non-owning |
| `m_lastTrinitySegmentIndex` | `uint8_t = 0xFF` | audio-sync only |
| `m_lastTrinitySegmentLabelHash` | `uint16_t = 0` | audio-sync only |

File-scope static: `static ShowDirectorActor* s_instance = nullptr;` — used by static `applyParamValue`/`getParamValue` to reach instance state. Single-instance assumption; not guarded.

---

## Block 9 — Compatibility stub headers

### `NodeOrchestrator.h` (19 lines)

```
namespace lightwaveos::nodes {
    using NodeOrchestrator = actors::ActorSystem;
}
```
Pure forward header; includes `ActorSystem.h`. Comment block declares it a backward-compatibility shim for code referencing the old `NodeOrchestrator` name.

### `RendererNode.h` (19 lines)

```
namespace lightwaveos::nodes {
    using RendererNode = actors::RendererActor;
}
```
Mirror shim for `RendererActor`. Includes `RendererActor.h`.

Both stubs imply a historical `nodes::` → `actors::` rename. Any rename of `ActorSystem`/`RendererActor` must also propagate to these `using` aliases or the alias names must be deleted.

---

## Cross-actor send patterns visible from method signatures

Senders inside this slice:

| Sender | Target | MessageType(s) emitted | Vehicle |
|---|---|---|---|
| `ActorSystem::setEffect/setBrightness/setSpeed/setPalette/setIntensity/setSaturation/setComplexity/setVariation/setHue/setMood/setFadeAmount/setEdgeMixerMode/setEdgeMixerSpread/setEdgeMixerStrength/setEdgeMixerSpatial/setEdgeMixerTemporal/saveEdgeMixerToNVS/setLedDithering` | `m_renderer` (RendererActor) | corresponding `SET_*` / `SAVE_EDGE_MIXER_NVS` | direct `m_renderer->send(msg, pdMS_TO_TICKS(10))` |
| `ActorSystem::startTransition` | `m_renderer` | `START_TRANSITION` | direct send |
| `ActorSystem::trinityBeat/Macro/Sync/Segment` | `m_renderer` | `TRINITY_BEAT/MACRO/SYNC/SEGMENT` | direct send (FEATURE_AUDIO_SYNC) |
| `ActorSystem::setStimulusMode` | `m_renderer` | `STIMULUS_SET_MODE` | direct send |
| `ActorSystem::clearStimulus` | `m_renderer` | `STIMULUS_CLEAR` | direct send + writes `m_stimulusControlBusBuffer` |
| `ActorSystem::publishStimulusFrame` | `m_stimulusControlBusBuffer` | — | `SnapshotBuffer::Publish()` (not a queue message) |
| `ShowDirectorActor::sendToRenderer` (called by `applyParamValue`, `executeCue`, `handleTrinitySegment`) | `m_rendererActor` (RendererActor via base `Actor*`) | `SET_BRIGHTNESS`, `SET_SPEED`, `SET_INTENSITY`, `SET_SATURATION`, `SET_COMPLEXITY`, `SET_VARIATION`, `SET_EFFECT`, `START_TRANSITION`, `SET_PALETTE`, `SET_MOOD` | `m_rendererActor->send(msg, pdMS_TO_TICKS(10))` |
| `ShowDirectorActor::publishShowEvent` | `bus::MessageBus` | `SHOW_STARTED`, `SHOW_STOPPED`, `SHOW_PAUSED`, `SHOW_RESUMED`, `SHOW_CHAPTER_CHANGED`, `SHOW_COMPLETED` | bus pub/sub, not direct actor send |
| `Actor::stop` (base) | self | `SHUTDOWN` | self-send via `send(shutdownMsg, pdMS_TO_TICKS(10))` |

Subscribers visible in this slice:

| Subscriber | MessageType subscribed | Subscribe site |
|---|---|---|
| `ShowDirectorActor` | `TRINITY_SEGMENT` (FEATURE_AUDIO_SYNC) | `onStart()` via `bus::MessageBus::instance().subscribe(MessageType::TRINITY_SEGMENT, this)` |
| `ShowDirectorActor` | unsubscribeAll on shutdown | `onStop()` via `bus::MessageBus::instance().unsubscribeAll(this)` |

Touchpoints into other subsystems (signature-visible):

- `m_renderer->setAudioBuffer(&m_audio->getControlBusBuffer())` — pointer wiring.
- `m_renderer->setStimulusBuffer(&m_stimulusControlBusBuffer)` — pointer wiring.
- `m_renderer->setTempo(&m_audio->getTempoMut())` — guarded against ESV11 / PipelineCore backends.
- `lightwaveos::synqmatrix::SynqMatrix::instance().markShowControl(millis())` — called from `ShowDirectorActor::onTick` and `::sendToRenderer` (FEATURE_AUDIO_SYNC).

---

## Anomalies & rename-hazard notes

1. `MessageType` block-header comments stop at `0xCF` but values continue with `STIMULUS_SET_MODE = 0xE0` / `STIMULUS_CLEAR = 0xE1`; no comment line marks the Stimulus block — easy to miss on rename.
2. `ActorConfigs::PluginManager()` task name `"PluginMgr"` is abbreviated; all other factory task names use the full role name. Rename must decide which form is canonical.
3. `NodeOrchestrator` / `RendererNode` aliases in `lightwaveos::nodes` indicate a prior namespace rename — any future rename must update these or delete them.
4. `ShowDirectorActor::s_instance` (file-scope static) is required by the static `ParameterSweeper` callbacks. Renaming `ShowDirectorActor` to anything else must also touch this singleton hookup site; multi-instance is not supported.
5. `ActorSystem::setPalette` writes JSON to `/Users/spectrasynq/.../.cursor/debug.log` — agent debug instrumentation embedded in production code path. Rename audit should flag and decide.
6. `ShowDirectorActor::applyParamValue` ignores its `uint8_t zone` parameter (no `ZONE_SET_*` message ever emitted from sweeps); sweeps always apply globally despite per-cue `targetZone`.
7. `ShowDirectorActor::executeCue` has unreachable TODO arms for `CUE_TRANSITION` and `CUE_ZONE_CONFIG`; both silently no-op despite being valid cue types.
8. `Actor::run`'s self-clocked branch (line 383) contains a tautological condition `tickInterval == 0 || tickInterval > 0`; the documented `portMAX_DELAY` case is unreachable by design but the disjunction is misleading.
9. `MessageType::ERROR_OCCURRED` (`0x86`) vs `AUDIO_ERROR` (`0xC3`) — inconsistent verb form on parallel concepts.
10. `ActorSystem::setPalette` is the only convenience command without queue-backpressure check; all other `setX` commands reject at ≥ 90 % utilisation but palette dispatch always attempts to enqueue.
11. `lightwaveos::audio::ControlBusFrame` cross-namespace dependency from `ActorSystem.h` — any rename of audio types requires header re-include here.
12. `setStimulusMode` updates `m_stimulusMode` outside the `m_stimulusMutex`; reads via `getStimulusMode()` are also unsynchronised. The mutex protects only `m_stimulusLastFrame` and snapshot publish.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-13 | agent:claude-opus-4-7 (1M ctx, SSA-N05) | Created. Extracted names + signatures from Actor.{h,cpp}, ActorSystem.{h,cpp}, ShowDirectorActor.{h,cpp}, NodeOrchestrator.h, RendererNode.h. RendererActor excluded (N04). AudioActor/DisplayActor/PluginManagerActor/SyncManagerActor live outside core/actors/ and are out of N05 scope. |
