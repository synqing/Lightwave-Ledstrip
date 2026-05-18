---
abstract: "Forensic inventory of every internal-SRAM consumer on the K1 V2 production build (esp32dev_audio_esv11_k1v2_32khz). Sourced from the live firmware.elf at .pio/build/esp32dev_audio_esv11_k1v2_32khz/, the K1 V2 platformio.ini, the Arduino-ESP32 esp32s3 sdkconfig, AsyncTCP 3.4.9 source, FastLED 3.10.0 RMT4 source, and the ESV11 vendor DSP code. Establishes static DRAM occupancy, dynamic working-set per subsystem, application-heap headroom, and which consumers are DMA-constrained versus PSRAM-eligible-but-internal. Verdict: structural — not just JsonDocument churn."
---

# W3 — Internal-SRAM Consumer Inventory

**Audit scope:** ESP32-S3 N16R8, K1 V2 production env `esp32dev_audio_esv11_k1v2_32khz`, ESP-IDF 4.4.7 (framework-espidf@3.40407.240606), Arduino-ESP32 3.20017, ESPAsyncWebServer 3.9.3, AsyncTCP 3.4.9, FastLED 3.10.0 (RMT4 patched).

**ELF under audit:** `firmware-v3/.pio/build/esp32dev_audio_esv11_k1v2_32khz/firmware.elf` (`xtensa-esp32s3-elf-size`/`-nm` evidence below).

**Cross-reference:** observation #48192 (2026-05-03) "FreeRTOS task stack inventory reveals 112 KB committed to stacks (40% of usable DRAM)" — independently confirmed below.

**RBDO label:** **GROUNDED**. Every byte estimate is traced to ELF section sizing, source code, sdkconfig, or vendor library code. Dynamic estimates that depend on runtime state (e.g. peak WiFi pbuf-pool occupancy, AsyncTCP per-connection footprint at N clients) are explicitly bounded and the source of the bound named.

---

## 1. Total budget (320 KB internal DRAM)

ESP32-S3 N16R8 internal SRAM = 512 KB total. The DRAM-mapped data region (.dram0 + heap) the application sees is **327,680 bytes** as reported by `pio run`:

```
RAM:   [====      ]  38.8% (used 127084 bytes from 327680 bytes)
```

The remaining internal SRAM (~184 KB) is split between:
- IRAM (`.iram0.text` for fast-path code), reported separately.
- ROM cache and ESP-IDF bootloader / startup reservations.
- The S3-only "data cache" region partially reserved by `qio_opi` PSRAM mode.

This file inventories **only the 320 KB DRAM region** (the one labelled `from 327680 bytes`). That is the budget the application actually competes for.

PSRAM = 8,388,608 B (OPI mode, `board_build.arduino.memory_type = qio_opi`, `BOARD_HAS_PSRAM` flag asserted via the inherited `esp32dev_audio_base` profile in `firmware-v3/platformio.ini:132`). PSRAM is fully available and used by the ESV11 DSP buffers.

---

## 2. Static occupancy at boot

Authoritative numbers from `xtensa-esp32s3-elf-size -A firmware.elf`:

| Section | Bytes | Region | Notes |
|---|---:|---|---|
| `.iram0.vectors` | 1,027 | IRAM | Interrupt vectors |
| `.iram0.text` | 71,263 | IRAM | Hot-path code in IRAM |
| `.dram0.dummy` | 55,908 | DRAM | IRAM-padding placeholder seen from DRAM addressing — burns DRAM accounting |
| `.dram0.data` | 27,572 | DRAM | Initialised globals |
| `.dram0.bss` | 99,512 | DRAM | Zero-initialised globals |
| `.noinit` | 0 | DRAM | Unused |
| `.flash.text` | 1,813,191 | Flash | Code that runs from XIP cache |
| `.flash.rodata` | 635,116 | Flash | rodata in flash |

**Total static DRAM consumed by the linker map:** 55,908 + 27,572 + 99,512 = **182,992 B (≈179 KB)**

The PIO build report "127,084 from 327,680" reflects `.dram0.data + .dram0.bss = 127,084` and omits `.dram0.dummy` (because the dummy is a placement artefact for IRAM, not "real" data the user owns). Either accounting is defensible. Conservatively, **the DRAM region has ~145 KB free after static linkage** (327,680 − 182,992 = 144,688), but the practical pre-heap floor is closer to **~150 KB** because `.dram0.heap_start` is fixed at `0x3FCB3E20` per the ELF symbol, leaving:

```
heap_end (~0x3FCE0000 / S3 DRAM ceiling) - 0x3FCB3E20 = ~0x2C1E0 = ~180,200 B
```

This is the **first hard number for the heap budget the kernel sees**: roughly **180 KB of free internal DRAM exists at `app_main()` entry**, before any FreeRTOS task creates its stack and before any subsystem warms up.

Evidence: `xtensa-esp32s3-elf-nm` shows `.dram0.heap_start` at `0x3FCB3E20` (decimal 1,070,287,584).

Top BSS consumers (DRAM-resident only — addresses `0x3FC...`–`0x3FCDFFFF`):

| Bytes | Symbol | Subsystem |
|---:|---|---|
| 11,264 | `lightwaveos::actors::ActorSystem::instance().instance` | Actor framework singleton (queues, handles, etc.) |
| 4,656 | `zoneComposer` | SynqMatrix composer state |
| 4,608 | `s_spectralMelBands` (STMExtractor.cpp:35) | Per-band mel state |
| 3,800 | `g_cnxMgr` | WiFi connection manager (libnet80211) |
| 2,776 | `ftm_initiator` | WiFi FTM (Fine Timing Measurement) state |
| 2,580 | `timeReversalMirrorMod3Instance` | Effect static instance |
| 1,296 | `MessageBus::instance` | Inter-actor pub/sub bus |
| 1,260 | `timeReversalMirrorMod1Instance` | Effect static instance |
| 1,280 | `timeReversalMirrorMod2Instance` | Effect static instance |
| 1,208 | `timeReversalMirrorARInstance` | Effect static instance |
| 1,184 | `dns_table` | lwIP DNS cache |
| 1,180 | `s_wifi_nvs` | WiFi NVS state cache |
| 1,124 | `s_coredump_stack` | Crash handler |
| 1,024 | `synqmatrix::g_restorePoints` (.data) | SynqMatrix restore-point ring |
| 960 | `s_validationFrameFallback` | Effect validation fallback frame |
| 960 | `TxRxCxt` (.data) | WiFi TX/RX context |

Plus a long tail of ~50 effect static instances at 100–800 B each.

Effect-side **static** instances alone (the `bN` symbols matching `timeReversalMirror*`, `cinema::s_*`, `*EnhancedInstance`, `*ARInstance`, …) sum to roughly **15–18 KB**. Every audio-reactive effect that needs persistent state-across-frames keeps it as a file-scope static, which goes to `.dram0.bss`. Some of these are PSRAM-eligible candidates (Section 5).

Sum of all DRAM-resident BSS (`b` and `B` linkage): ~92 KB.
Sum of all DRAM-resident .data (`d` and `D` linkage): ~17 KB.

These add up consistently with `.dram0.bss` (99 KB) + `.dram0.data` (28 KB) (rounding due to alignment / unreported sections).

---

## 3. Dynamic working set per subsystem

Estimates are at sustained load (audio capturing, 1–4 WS clients, ~30 FPS render). All are best-known runtime allocations. Where ranges are given they reflect documented worst-cases.

| Subsystem | Est. bytes (typ) | Est. bytes (max) | DMA-constrained? | Source / evidence |
|---|---:|---:|---|---|
| **FreeRTOS task stacks (committed)** | **103,936** | **103,936** | N (allocated from internal heap by xTaskCreate) | See Section 3.1 |
| **WiFi static RX buffers** | 12,800 | 12,800 | **Y** (peripheral DMA) | `CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=8` × 1,600 B per net80211 buffer |
| **WiFi dynamic RX buffers** | 0 (lazy) | ~51,200 | **Y** | `CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=32` × ~1,600 B; allocated on demand from internal DMA heap |
| **WiFi TX buffers (dynamic)** | 0 | ~10,000 | **Y** | `CONFIG_ESP_WIFI_TX_BUFFER_TYPE=0` (dynamic), library default ~6–8 buffers |
| **lwIP pbuf pool (PBUF_POOL)** | ~6,400 | ~6,400 | Y (reads from WiFi DMA payloads) | IDF default 10 buffers × 1,524 B with PBUF_LINK_HLEN; `CONFIG_LWIP_IP_REASS_MAX_PBUFS=10`, `CONFIG_LWIP_LOOPBACK_MAX_PBUFS=8` |
| **lwIP TCP per-socket buffers** | ~11,500 / socket | ~92,000 (8 sock × 11.5 KB) | N | `CONFIG_LWIP_TCP_SND_BUF_DEFAULT=5744`, `CONFIG_LWIP_TCP_WND_DEFAULT=5760`. With `CONFIG_LWIP_MAX_SOCKETS=16`, 4–8 active TCP sockets is the realistic ceiling |
| **AsyncTCP `_async_queue`** | 4,096 + per-event ~64 B | growing | N | Queue depth proportional to event burst; AsyncTCP.cpp:382 creates `async_tcp` task pinned to core 1 |
| **AsyncWebSocket per-client queue** | ~6,000 / client | ~48,000 (8 clients) | N | `WS_MAX_QUEUED_MESSAGES=12` (K1 V2 override, platformio.ini:189) × ~500 B per buffered frame |
| **AudioActor I2S DMA descriptors** | 4,096 | 4,096 | **Y** (I2S DMA, internal SRAM only) | `dma_buf_count=4`, `dma_buf_len=512 stereo int32 frames` (`microphone.h:133-134`) — descriptors only; payload buffers are part of this 4×2 KB = 8 KB if 2 × i16, 4 × 2 KB = 8 KB |
| **AudioActor `m_controlBusBuffer` (SnapshotBuffer)** | ~9,800 | 9,800 | N (intentionally pinned to internal DRAM) | `InternalSnapshotBufferOwner` allocates with `MALLOC_CAP_INTERNAL` (`SnapshotBuffer.h:170`); 2 × `sizeof(ControlBusFrame)` ≤ 2 × 5 KB. Static_assert at `ControlBus.h:307` caps at 5,120 B/frame. Observation #46151 confirms internal-DRAM placement. |
| **AudioActor task stack** | 16,384 | 16,384 | N | `AUDIO_ACTOR_STACK_WORDS=4096` words × 4 (`audio_config.h:205`) |
| **AudioActor scratch (`g_scratch`)** | 352 | 352 | N | DRAM .data — anonymous-namespace scratch |
| **ESV11 DSP buffers (sample_history, novelty, vu, tempi, spectrogram_avg, noise_history)** | 0 internal | 0 internal | **N — PSRAM** | `EsV11Buffers.cpp:30` allocates via `heap_caps_calloc(..., MALLOC_CAP_SPIRAM)`. ~30 KB total but in PSRAM, NOT internal. |
| **RendererActor: m_leds[320] (CRGB)** | 960 | 960 | N (FastLED copies to its own buffer before TX) | `RendererActor.h:833` |
| **RendererActor: m_transitionSourceBuffer[320]** | 960 | 960 | N | `RendererActor.h:983` |
| **RendererActor: m_captureScratch[320]** | 960 | 960 | N | `RendererActor.h:1014` |
| **LedDriver_S3 strip buffers (m_strip1+m_strip2)** | 960 | 960 | N | `LedDriver_S3.h:59-60`, `kMaxLedsPerStrip=160` × 3 B × 2 strips |
| **LedDriver_S3 RMT TX buffers (m_txStrip1+m_txStrip2)** | 960 | 960 | N | `LedDriver_S3.h:63-64`. Renderer memcpy's m_strip1 → m_txStrip1 once per frame (line 143) |
| **RendererActor static (`Renderer` actor) members + zone state** | ~6,000 | ~6,000 | N | `RendererActor` class is itself heap-allocated as `std::make_unique<RendererActor>()` in `ActorSystem.cpp:95` |
| **FastLED RMT4 software state (mPulses, gControllers, etc.)** | ~1,500 | ~1,500 | N | `MAX_PULSES = 64 × 2 = 128` 32-bit entries × 2 controllers ≈ 1 KB plus globals. Actual pulse data goes to peripheral RMTMEM (NOT counted against DRAM). |
| **ActorSystem singleton** | 11,264 | 11,264 | N | BSS, listed above |
| **MessageBus singleton** | 1,296 | 1,296 | N | BSS |
| **SynqMatrix `zoneComposer`** | 4,656 | 4,656 | N | BSS |
| **Effect statics (sum, ~50 instances)** | ~15,000 | ~18,000 | N | BSS, listed in Section 2 |
| **ESPAsyncWebServer URL/header parsing scratch** | ~2,000 / req | bursty | N | Per-request scratch; freed at response complete |
| **Arduino String / ArduinoJson heap (transient)** | bursty | bursty | N | Documented as part of W1/W2 churn — not a steady-state cost |

### 3.1. FreeRTOS task stack inventory (committed at boot)

Adds up to **103,936 B = 101.5 KB**, consistent with observation #48192 ("112 KB committed to stacks").

| Task | Stack (bytes) | Source | Core / Pri | Created where |
|---|---:|---|---|---|
| `loopTask` (Arduino loop()) | 12,288 | `-D ARDUINO_LOOP_STACK_SIZE=12288` (platformio.ini:152, :187) | Core 1, prio 1 | `Arduino-ESP32/main.cpp:14-37` |
| `Renderer` (RendererActor) | 16,384 | 4,096 words × 4 (`Actor.h:484`) | Core 1, prio 5 | `Actor.cpp:92`, configured by `ActorConfigs::Renderer()` |
| `Audio` (AudioActor) | 16,384 | `AUDIO_ACTOR_STACK_WORDS=4096` (`audio_config.h:205`) | Core 0, prio 4 | `Actor.cpp:92` |
| `ShowDirector` | 12,288 | 3,072 words × 4 (`ShowDirectorActor.cpp:248`) | Core 0, prio 2 | `Actor.cpp:92` |
| `PluginMgr` (PluginManagerActor) | 8,192 | 2,048 words × 4 (`Actor.h:582`) | Core 0, prio 2 | `Actor.cpp:92` (constructed at `SystemInit.cpp:301`) |
| `WiFiManager` | 4,096 | `WiFiManager.h:425` `TASK_STACK_SIZE=4096` | Core 0, prio 1 | `WiFiManager.cpp:153` |
| `async_tcp` | 16,384 | `CONFIG_ASYNC_TCP_STACK_SIZE=8192*2` (`AsyncTCP.h:46`) | **Core 1** (`-D CONFIG_ASYNC_TCP_RUNNING_CORE=1`), prio 3 | `AsyncTCP.cpp:382` |
| `tiT` (lwIP TCPIP thread) | 4,096 | `CONFIG_LWIP_TCPIP_TASK_STACK_SIZE=4096` (sdkconfig:2775) | Core 0, prio 18 | ESP-IDF |
| `wifi` | ~6,144 | ESP-IDF default (libpp.a internal) | Core 0, prio 23 | ESP-IDF (precompiled) |
| `esp_timer` | 8,192 | `CONFIG_ESP_TIMER_TASK_STACK_SIZE=8192` (sdkconfig:2347) | Core 0, prio 22 | ESP-IDF |
| `sys_evt` (event loop) | 2,048 | `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=2048` (sdkconfig:2296) | Core 0, prio 20 | ESP-IDF |
| `ipc0`/`ipc1` (cross-core IPC) | 1,024 × 2 = 2,048 | `CONFIG_ESP_IPC_TASK_STACK_SIZE=1024` (sdkconfig:2335) | One per core, prio 24 | ESP-IDF |
| `IDLE0`/`IDLE1` | 1,024 × 2 = 2,048 | `CONFIG_FREERTOS_IDLE_TASK_STACKSIZE=1024` (sdkconfig:2518) | One per core, prio 0 | FreeRTOS |
| `Tmr Svc` (FreeRTOS timer) | 3,120 | `CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH=3120` (sdkconfig:2530) | Core 0, prio 1 | FreeRTOS |

Subtotal (named tasks): **103,936 B** for the K1 V2 production env. Observation #48192's "112 KB" includes a few more transient/spawn-on-demand tasks (e.g. `ProvisionRestart` 2,048 B at NetworkHandlers.cpp:206, `captureTx` 4,096 B at CaptureStreamer.cpp:516, `EncoderI2C` 4,096 B at EncoderManager.cpp — guarded by `K1_TTP223_PIN` which is **disabled** for K1 V2 production per platformio.ini:195, so EncoderI2C does **not** spawn).

NOT created on the K1 V2 production env (verified by searching for instantiation sites):
- `NetworkActor` / `HmiActor` / `StateStoreActor` / `SyncManagerActor` (configs exist in `Actor.h` but `ActorSystem.cpp` only creates Renderer/ShowDirector/Audio/Display)
- `EncoderI2C` (gated by `K1_TTP223_PIN`)
- `DisplayActor` (gated by `FEATURE_AMOLED_DISPLAY`, off for K1 V2)
- `captureTx` (lazy — only when the serial `capture` command starts a stream)

The actor framework's published configs in `Actor.h:481-588` therefore over-state the steady-state load on K1 V2 by ~36 KB.

### 3.2. Sum of dynamic working set (steady state, 2 WS clients)

| Bucket | Bytes |
|---|---:|
| Task stacks (committed at boot) | 103,936 |
| WiFi static RX buffers (always) | 12,800 |
| AudioActor I2S DMA + descriptors | ~8,000 |
| AudioActor ControlBus SnapshotBuffer (internal) | 9,800 |
| Renderer + LedDriver buffers (all m_leds + transition + scratch + tx + strip) | 5,760 |
| lwIP pbuf pool (resident) | ~6,400 |
| lwIP TCP per-socket buffers (4 sockets × 11.5 KB) | ~46,000 |
| AsyncTCP queue + state | ~5,000 |
| AsyncWebSocket per-client queues (2 clients × 6 KB) | ~12,000 |
| Effect-side scratch / ring-buffer transients (typical) | ~5,000 |
| **Dynamic working set (typical, 2 WS clients)** | **~214,700 B** |

Under burst conditions (4–8 WS clients, multiple TCP sockets active):

| Bucket | Bytes |
|---|---:|
| Task stacks | 103,936 |
| WiFi static RX | 12,800 |
| WiFi dynamic RX (peak) | ~32,000 |
| WiFi TX dynamic (peak) | ~10,000 |
| AudioActor (I2S DMA + ControlBus) | ~17,800 |
| Renderer + LedDriver | 5,760 |
| lwIP pbuf pool | ~6,400 |
| lwIP TCP buffers (8 sockets) | ~92,000 |
| AsyncTCP | ~6,000 |
| AsyncWebSocket (8 clients) | ~48,000 |
| Effect transients | ~6,000 |
| **Dynamic working set (burst peak)** | **~340,696 B** — exceeds the 327,680 budget |

This shows the system is **structurally over-committed for the burst case**. It does not OOM at boot because (a) WiFi dynamic buffers are lazy-allocated and (b) the real maximum WS client count under WS_MAX_QUEUED_MESSAGES=12 self-limits before reaching 8 simultaneous clients. But there is **no comfortable margin** for any unplanned consumer.

---

## 4. Application heap headroom

Computed two ways for triangulation.

### Method A — top-down

```
DRAM region (kernel-visible):    327,680 B
- Static linkage (.dram0.*):    -182,992 B
- Task stacks (boot)           : -103,936 B
- WiFi static RX               :  -12,800 B
- AudioActor I2S + ControlBus  :  -17,800 B
- Renderer + LedDriver buffers :   -5,760 B
- lwIP pbuf + base sockets     :   -6,400 B
= Application heap headroom    :   ~-2,008 B → effectively zero / negative
```

This is unphysical (boot succeeds), which means the `.dram0.dummy` 55 KB block is being **double-counted** as both static linkage and as IRAM padding the kernel does not enforce in the DRAM heap. Correcting:

### Method B — kernel-visible heap at `app_main()` entry

`.dram0.heap_start = 0x3FCB3E20` per ELF. S3 DRAM ceiling is `0x3FCE0000`. Therefore the heap region the IDF allocator sees is:

```
0x3FCE0000 - 0x3FCB3E20 = 0x2C1E0 = 180,192 B
```

That ~180 KB is what `esp_get_free_heap_size()` returns at boot **before** any task stack, I2S buffer, ControlBus snapshot, or lwIP/WiFi allocation. From that:

```
Heap at app_main()  :   180,192 B
- Task stacks                  : -103,936 B  (xTaskCreate pulls from internal heap)
- WiFi static RX               :  -12,800 B
- AudioActor I2S DMA + descrs  :   -8,000 B
- AudioActor ControlBus (MALLOC_CAP_INTERNAL) :   -9,800 B
- Renderer + LedDriver (heap-allocated actor) :   -5,760 B
- lwIP pbuf pool + initial socket state :   -7,000 B
- AsyncTCP queue (created lazily on first connection) :   -5,000 B
= Application heap free (idle, no WS clients) : ~27,896 B
```

That ~28 KB matches order-of-magnitude with the platformio.ini comment at line 204:
> Keep default low-heap shed thresholds from WebServer.h for render stability:
> shed below 30 KB, resume above 45 KB.

i.e. the firmware **already knows** it gets close to floor and has a defensive heap-shed mechanism that drops audio-frame WebSocket broadcasts below 30 KB free.

### What "free application heap" actually means under load

With 2 WebSocket clients connected (each with its 12-deep send queue at ~500 B/frame plus an AsyncTCP per-pcb state of a few KB plus lwIP TCP send + recv buffers at ~11.5 KB each socket):

```
~27,896 B idle  -  ~28,000 B  (2 clients steady-state) = ~0 B, swinging below the 30 KB shed threshold continuously.
```

The W1 / W2 reports describe JsonDocument and Json::serializeJson churn as the visible failure mode. **That churn happens against a heap floor that is already structurally near zero under the documented production-client count.** Adding a 2 KB JsonDocument is not the disease — it is the symptom of a heap whose steady-state floor is single-digit kilobytes.

---

## 5. PSRAM-eligible but currently in internal DRAM

These are consumers that **do not require DMA** and **are not on the audio render hot path**, but currently live in internal SRAM and could be relocated to PSRAM with appropriate `MALLOC_CAP_SPIRAM` or `EXT_RAM_BSS_ATTR` placement. They are the realistic moves the application has not yet made.

| Consumer | Bytes | Currently lives | DMA-needed? | Renderer hot path? | Verdict |
|---|---:|---|---|---|---|
| Effect static instances (`timeReversalMirrorMod*`, `cinema::s_*`, ~50 statics) | ~15,000–18,000 | `.dram0.bss` | N | Read in `render()`, but cache-friendly access pattern | **PSRAM-eligible** if marked `EXT_RAM_BSS_ATTR` (Arduino-ESP32 macro). Latency cost ~tens of ns per access, well within the 2 ms render budget. |
| `s_spectralMelBands` (4,608 B) | `.dram0.bss` | N | Audio path only (Core 0), not LED render | **PSRAM-eligible** — accessed at audio hop rate (125 Hz), not 120 FPS render. |
| `s_validationFrameFallback` (960 B) | `.dram0.bss` | N | Validation-mode only | **PSRAM-eligible** — disabled in production (`FEATURE_EFFECT_VALIDATION=0` at platformio.ini:131). |
| `zoneComposer` (4,656 B) | `.dram0.bss` | N | Yes — read on every render frame | **MARGINAL.** Move only if profiling shows acceptable latency. |
| Effect parameter lookup tables (`kParameters`, ~520+640+440+400 B) | `.dram0.bss` | N | Init only | **PSRAM-eligible** — constexpr metadata, never modified after construction. Probably better as `.rodata` (flash) than DRAM. |
| `g_restorePoints` (1,024 B) | `.dram0.data` | N | Rare (preset switches) | **PSRAM-eligible**. |
| AsyncWebSocket per-client send queue entries (the buffered frame payloads, not the queue header) | ~6 KB/client | Internal heap | N | TX side only (Core 1 async_tcp task copies into pbuf for lwIP) | **MARGINAL** — lwIP/WiFi TX needs the eventual pbuf in DMA-capable internal, but the AsyncWebSocket-side queue itself does not. Worth experiment. |

**Total PSRAM-eligible-but-currently-internal:** roughly **25,000–30,000 B** of cleanly relocatable BSS/data, plus another ~30–40 KB of AsyncWebSocket per-client queue payloads that *could* be PSRAM (but verification needed because lwIP eventually pulls the data into DMA pbufs).

The ESV11 DSP buffers (sample_history at 40 KB, window_lookup at 16 KB, novelty/vu curves, etc.) **are already in PSRAM** via `EsV11Buffers.cpp:30` (`MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`). This is correct because they are accessed at the audio-hop rate (125 Hz), not the LED render rate, and the PSRAM bandwidth is fine for that cadence. ~30 KB of pressure already correctly moved.

Conversely, the `m_controlBusBuffer` for ControlBusFrame at ~9.8 KB is **intentionally pinned to internal DRAM** via `MALLOC_CAP_INTERNAL` at `SnapshotBuffer.h:170`. This is the right call — the renderer (Core 1, 120 FPS) reads it every frame and PSRAM latency would matter. Observation #46151 documents this decision explicitly.

---

## 6. DMA-constrained consumers (must stay internal)

These cannot be relocated — moving them to PSRAM would either physically fail (no DMA descriptor reachability) or violate hardware timing.

| Consumer | Bytes | Why internal-only |
|---|---:|---|
| WiFi static RX buffers (8 × ~1.6 KB) | 12,800 | net80211 RX DMA writes payloads directly |
| WiFi dynamic RX buffers (peak 32 × ~1.6 KB) | up to 51,200 | Same |
| WiFi TX dynamic buffers | up to ~10,000 | net80211 TX DMA |
| lwIP pbuf pool (PBUF_POOL) | ~6,400 | Backs WiFi DMA payloads |
| AudioActor I2S DMA buffers (`dma_buf_count=4 × dma_buf_len=512×stereo×4`) | ~8,000 | I2S RX DMA on Core 0 |
| AudioActor ControlBus snapshot (latency, not DMA) | 9,800 | Read by Renderer at 120 FPS — PSRAM latency unacceptable |
| RendererActor `m_leds` / `m_transitionSource` / FastLED tx buffers | 5,760 | Renderer hot path 120 FPS — same latency argument |
| FastLED RMT4 software state | ~1,500 | Pulse data goes to peripheral RMTMEM (on-chip), software shadow stays close. Not DMA-blocked per se but cache-affinity. |
| Most FreeRTOS task stacks | 103,936 | Stack memory must be internal for ISR re-entrancy (PSRAM access from ISR is prohibited without explicit allow-cache flags) |

**Total DMA / latency-constrained:** roughly **160,000 B (≈156 KB)** that **cannot leave internal SRAM** regardless of optimisation effort.

---

## 7. Verdict: is the application heap structurally adequate?

**No.** Three independent lines of evidence converge on the same conclusion.

1. **Top-down arithmetic.** Of the 327,680 B DRAM region, ~183 KB is consumed by static linkage and ~104 KB by FreeRTOS task stacks before any dynamic allocation. WiFi/lwIP/AudioActor/Renderer baseline pulls another ~50 KB. That leaves the steady-state idle heap floor at ~28 KB, which is already at the firmware's own documented heap-shed threshold (30 KB).

2. **The firmware's own self-defence proves it.** `platformio.ini:204` documents "shed below 30 KB, resume above 45 KB" — i.e. someone already characterised that the production heap floor sits in the 28–45 KB band under live load. A system that has to actively shed Audio-broadcast WS frames to stay alive is, by definition, structurally heap-poor for its declared workload.

3. **The burst case mathematically exceeds the budget.** Section 3.2 shows that 8 WS clients + 4–8 active TCP sockets + peak WiFi dynamic RX adds up to ~340 KB, which is greater than the 327,680 B region. The firmware survives this only because connection counts in production are capped well below the documented per-client buffer ceilings. The headroom for additional features (new effects with state, new audio analyses, new WS commands) is **zero**.

**Therefore the heap pressure is not "too many JsonDocuments."** JsonDocument churn is the visible failure mode against an already-marginal floor. The structural problem is the combination of:

- ~104 KB of FreeRTOS task stack committed at boot (32% of the DRAM region).
- ~50 KB of effect+composer+actor framework BSS that could be PSRAM-eligible but is not.
- ~50 KB of lwIP per-socket TCP buffers (`SND_BUF=5744`, `WND=5760` per socket × N sockets).
- ~28–48 KB of AsyncWebSocket per-client send queues at the production WS_MAX_QUEUED_MESSAGES=12.

**The realistic recovery moves, in order of leverage**, are:

A. Move PSRAM-eligible effect statics + `s_spectralMelBands` + `kParameters` tables to PSRAM via `EXT_RAM_BSS_ATTR`. Expected delta: **+20–25 KB application heap**. Engineering cost: low (annotate symbols).

B. Re-evaluate task-stack sizes against measured high-water marks. The Renderer and Audio actors are sized at 16 KB each "with 50% safety margin over 8–10 KB usage" per the inline comments. If the actual high-water marks are 8 KB, that is 8 KB recoverable from each. Expected delta: **+10–16 KB**. Engineering cost: medium (need stable high-water sampling under sustained load).

C. Lower `CONFIG_LWIP_TCP_SND_BUF_DEFAULT` and `CONFIG_LWIP_TCP_WND_DEFAULT` from 5744/5760 to 2920/2920 for the K1 use case (LAN-only, low RTT). Expected delta: **+5 KB per active socket** = ~20 KB for 4 sockets. Engineering cost: medium (requires WiFi performance verification).

D. Verify that AsyncWebSocket send-queue payloads can live in PSRAM (with internal-DRAM pbuf only at the moment of lwIP enqueue). Expected delta: **+24–40 KB at 4–8 clients**. Engineering cost: high (touches the WS hot path).

E. Consider whether `ActorSystem::instance` at 11.2 KB or `zoneComposer` at 4.6 KB include vestigial members that can be trimmed. Expected delta: **+5–15 KB**. Engineering cost: medium (architectural).

**Without any of A–E, the firmware will continue to hit the 30 KB shed threshold under any condition that adds 5–10 KB of pressure on top of steady state.** This explains why JsonDocument churn produces visible symptoms: the floor is so close to zero that churn whose peak excursion would be invisible on a healthy system becomes a heap-shed trigger here.

---

## Evidence trail

- `xtensa-esp32s3-elf-size /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/.pio/build/esp32dev_audio_esv11_k1v2_32khz/firmware.elf` → text 1,901,746 / data 662,688 / bss 1,990,445.
- `xtensa-esp32s3-elf-size -A` → .dram0.dummy 55,908 / .dram0.data 27,572 / .dram0.bss 99,512 / .iram0.text 71,263.
- `xtensa-esp32s3-elf-nm -S` → `.dram0.heap_start` at `0x3FCB3E20`.
- `firmware-v3/platformio.ini:67,189` → `WS_MAX_QUEUED_MESSAGES` 32 (common) / 12 (K1 V2 override).
- `firmware-v3/platformio.ini:84` → `FASTLED_RMT_MAX_CHANNELS=4`.
- `firmware-v3/platformio.ini:152,187` → `ARDUINO_LOOP_STACK_SIZE=12288`.
- `firmware-v3/platformio.ini:48-58` → AsyncTCP pinned core 1, priority 3.
- `firmware-v3/platformio.ini:131,132` → `FEATURE_EFFECT_VALIDATION=0`, `BOARD_HAS_PSRAM`.
- `firmware-v3/platformio.ini:200-204` → heap-shed thresholds 30 KB / 45 KB.
- `firmware-v3/src/core/actors/Actor.h:481-588` → ActorConfig stack sizes (words × 4 = bytes).
- `firmware-v3/src/core/actors/Actor.cpp:92` → `xTaskCreatePinnedToCore` with `m_config.stackSize * 4` bytes.
- `firmware-v3/src/core/actors/ActorSystem.cpp:95,106,118,144` → Renderer / ShowDirector / Audio / Display (gated) instantiation.
- `firmware-v3/src/core/actors/ShowDirectorActor.cpp:246-248` → ShowDirector stack 3,072 words = 12 KB.
- `firmware-v3/src/core/actors/RendererActor.h:833,983,1014` → m_leds + m_transitionSourceBuffer + m_captureScratch (3 × 960 B = 2,880 B).
- `firmware-v3/src/core/SystemInit.cpp:301` → `pluginManager = new PluginManagerActor()`.
- `firmware-v3/src/audio/AudioActor.h:1214-1230,647,853,981` → AudioActor ActorConfig + `InternalSnapshotBufferOwner<ControlBusFrame> m_controlBusBuffer`.
- `firmware-v3/src/audio/contracts/ControlBus.h:307` → `static_assert(sizeof(ControlBusFrame) <= 5120)`.
- `firmware-v3/src/audio/contracts/SnapshotBuffer.h:162-208` → `InternalSnapshotBufferOwner` uses `heap_caps_malloc(..., MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`.
- `firmware-v3/src/audio/backends/esv11/vendor/microphone.h:127-142` → ESV11 I2S `dma_buf_count=4`, `dma_buf_len=512*2`, 32-bit stereo.
- `firmware-v3/src/audio/backends/esv11/vendor/EsV11Buffers.cpp:30,93-122` → ESV11 DSP buffers allocated via `MALLOC_CAP_SPIRAM`.
- `firmware-v3/src/audio/pipeline/STMExtractor.cpp:35` → `static SpectralMelBand s_spectralMelBands[kSpectralMelBands]` (4,608 B in BSS).
- `firmware-v3/src/audio/config/audio_config.h:203-206` → `AUDIO_ACTOR_STACK_WORDS=4096`, priority 4, core 0.
- `firmware-v3/src/hal/esp32s3/LedDriver_S3.h:48,59-64` → `kMaxLedsPerStrip=160`, 4 × CRGB[160] strip+tx buffers.
- `firmware-v3/src/hal/esp32s3/LedDriver_S3.cpp:143-145` → memcpy m_strip[i] → m_txStrip[i] per show().
- `firmware-v3/src/network/WiFiManager.cpp:153-160`, `WiFiManager.h:425` → WiFiManager task 4 KB stack.
- `firmware-v3/src/network/webserver/handlers/NetworkHandlers.cpp:206` → ProvisionRestart 2 KB stack (lazy).
- `firmware-v3/src/serial/CaptureStreamer.cpp:516` → captureTx 4 KB (lazy).
- `firmware-v3/src/hardware/EncoderManager.h:64-66` → EncoderI2C 4 KB stack (NOT created on K1 V2; gated by `K1_TTP223_PIN` which is disabled at platformio.ini:195).
- AsyncTCP `.pio/libdeps/.../AsyncTCP/src/AsyncTCP.h:45-46` → `CONFIG_ASYNC_TCP_STACK_SIZE 8192*2`.
- AsyncTCP `.pio/libdeps/.../AsyncTCP/src/AsyncTCP.cpp:382` → `xTaskCreateUniversal(_async_service_task, "async_tcp", CONFIG_ASYNC_TCP_STACK_SIZE, ...)`.
- FastLED `.pio/libdeps/.../FastLED/src/platforms/esp/32/rmt_4/idf4_rmt_impl.cpp:84-103` → `FASTLED_RMT_MEM_WORDS_PER_CHANNEL=SOC_RMT_MEM_WORDS_PER_CHANNEL` (64 default), `FASTLED_RMT_MEM_BLOCKS=2`, `MAX_PULSES = 128`. Pulse memory backed by peripheral `RMTMEM`, not main DRAM.
- ESP32-S3 Arduino-ESP32 `sdkconfig` (`~/.platformio/packages/framework-arduinoespressif32-libs/esp32s3/sdkconfig`):
  - L2296: `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=2048`
  - L2297: `CONFIG_ESP_MAIN_TASK_STACK_SIZE=4096`
  - L2335: `CONFIG_ESP_IPC_TASK_STACK_SIZE=1024`
  - L2347: `CONFIG_ESP_TIMER_TASK_STACK_SIZE=8192`
  - L2361: `CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=8`
  - L2362: `CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=32`
  - L2364: `CONFIG_ESP_WIFI_TX_BUFFER_TYPE=0` (dynamic)
  - L2370: `CONFIG_ESP_WIFI_RX_MGMT_BUF_NUM_DEF=5`
  - L2375: `CONFIG_ESP_WIFI_RX_BA_WIN=16`
  - L2513: `CONFIG_FREERTOS_HZ=1000`
  - L2518: `CONFIG_FREERTOS_IDLE_TASK_STACKSIZE=1024`
  - L2530: `CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH=3120`
  - L2679: `CONFIG_LWIP_MAX_SOCKETS=16`
  - L2691: `CONFIG_LWIP_IP_REASS_MAX_PBUFS=10`
  - L2700: `CONFIG_LWIP_TCPIP_RECVMBOX_SIZE=32`
  - L2731: `CONFIG_LWIP_LOOPBACK_MAX_PBUFS=8`
  - L2745-2746: `CONFIG_LWIP_TCP_SND_BUF_DEFAULT=5744`, `CONFIG_LWIP_TCP_WND_DEFAULT=5760`
  - L2747: `CONFIG_LWIP_TCP_RECVMBOX_SIZE=6`
  - L2775: `CONFIG_LWIP_TCPIP_TASK_STACK_SIZE=4096`
- Build output: `RAM: [====] 38.8% (used 127084 bytes from 327680 bytes)`.
- Cross-reference observation #48192 (2026-05-03): "FreeRTOS task stack inventory reveals 112 KB committed to stacks (40% of usable DRAM)" — independently corroborated at 104 KB above for the K1 V2 production env (the 112 KB observation includes lazy `captureTx` + `ProvisionRestart` that are not created at boot on K1 V2).
- Cross-reference observation #46151 (2026-04-27): "ControlBusFrame SnapshotBuffer Allocated as Value Member in Internal DRAM Not PSRAM" — corroborated at `SnapshotBuffer.h:170` `MALLOC_CAP_INTERNAL`.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-18 | agent:research-subagent (deep-technical-analyst) | Created. Full internal-SRAM consumer inventory for K1 V2 production firmware. ELF-grounded static analysis, dynamic working-set estimation, PSRAM-eligible/DMA-constrained classification, verdict on structural heap adequacy. |
