---
abstract: K1 V2 IDF 4.4.7 addendum to ESP32-S3-Canonical-Reference-Skills.md. Re-states the legacy-API surface for I2S / RMT / GDMA / cache that K1 V2 actually compiles against, and corrects File 4's SPH0645 textbook I2S config against K1's verified-on-hardware MSB+RIGHT_LEFT+>>10 init. Read alongside the canonical spec — silicon truth in the spec applies; API code blocks for IDF 5.x do not, and File 4 §10's Philips+ONLY_LEFT+>>14 SPH0645 recipe does not work on K1 V2. Signatures verified against `framework-espidf@3.40407.240606` (ESP_IDF_VERSION 4.4.7); K1 I2S config + sample extraction verified from `src/audio/AudioCapture.cpp` and `src/config/audio_config.h`. Audited 2026-05-19.
---

# ESP32-S3 Canonical Reference — K1 IDF 4.4.7 Addendum

Companion to [`ESP32-S3-Canonical-Reference-Skills.md`](./ESP32-S3-Canonical-Reference-Skills.md). The canonical spec is silicon-truth-correct (TRM citations verified verbatim across 8 spot-checks, content matches TRM v1.8). Its **API code blocks**, however, target **ESP-IDF 5.x**. K1 V2 production firmware is pinned to **IDF 4.4.7** per [`firmware_build_envs`](../../../.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip/memory/firmware_build_envs.md) — `esp32dev_audio_esv11_k1v2_32khz`. This file restates the relevant API surface using legacy headers that actually exist in 4.4.7.

All signatures below were grepped verbatim from `~/.platformio/packages/framework-espidf@3.40407.240606/components/` on 2026-05-19. Version banner confirmed `ESP_IDF_VERSION_MAJOR 4 / MINOR 4 / PATCH 7`.

## Trust map

| Section of canonical spec | Use on 4.4.7? | Notes |
|---|---|---|
| File 1 — Memory architecture: address map (§1), PMS doctrine (§2), cache (§3), GDMA address space (§4), attribute macros & linker sections (§6), N16R8 sdkconfig (§7), what most agents get wrong (§8), footgun catalogue (§9), SpectraSynq implications (§11) | **Yes, as-is.** Silicon truth, doesn't depend on IDF version. |
| File 1 §5 — heap-caps API | **Yes, almost as-is.** 11/12 signatures present in 4.4.7. `heap_caps_print_all_task_stat` is v5+ (use `heap_caps_get_per_task_info` from `esp_heap_task_info.h` instead, which IS in 4.4.7). |
| File 1 §11 — heap-shed against SRAM1 D-view | **Yes, load-bearing.** Applies as-is to active witchhunt. See [§ Heap-shed measurement](#heap-shed-measurement-load-bearing) below for concrete diagnostic. |
| File 2 — GDMA silicon model (§1–§8), errata (§11), diagnostics (§12), SpectraSynq implications (§13) | **Yes, as-is.** Silicon truth. |
| **File 2 §9 — GDMA ESP-IDF API** | **OVERRIDE.** See [§ GDMA (4.4.7)](#gdma-447) below. |
| **File 2 §5 — esp_cache_msync recipe** | **OVERRIDE.** No `esp_mm` component in 4.4.7. See [§ PSRAM cache flush (4.4.7)](#psram-cache-flush-447) below. |
| File 3 — RMT silicon model (§1–§9), errata RMT-176 (§13), footguns (§14), IRQ-budget arithmetic (§16) | **Yes, as-is.** Silicon truth. The 320-LED / 120-FPS / 2×160-strip arithmetic stands verbatim. |
| **File 3 §10 — RMT5 (`driver/rmt_tx.h`) API** | **OVERRIDE.** K1 uses FastLED's RMT4 backend, which talks to `driver/rmt.h` (legacy). See [§ RMT (4.4.7)](#rmt-447) below. |
| **File 3 §11 — Espressif `led_strip` component** | **NOT USED on K1.** Reference only; FastLED owns the LED path on this firmware (see [`project_async_rmt_120fps`](../../../.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip/memory/project_async_rmt_120fps.md)). |
| File 4 — I2S silicon model (§1–§7), SPH0645 specifics (§9), errata (§12), diagnostics (§13), SpectraSynq pipeline (§14) | **Yes, as-is.** Silicon truth. `>>14` shift settlement applies. |
| **File 4 §8 + §10 — I2S new-driver canonical config** | **OVERRIDE (two reasons).** (1) Header `driver/i2s_std.h` does not exist in 4.4.7. (2) **K1 V2 diverges from File 4's SPH0645 textbook config in three coupled ways** — `STAND_MSB` (not Philips), `RIGHT_LEFT` (not `ONLY_LEFT`), `>>10` extraction (not `>>14`) — validated on hardware by bugfix #23164 (2026-01-30). See [§ I2S (4.4.7) — K1 V2 actual init](#i2s-447--k1-v2-actual-init) below. |

---

## I2S (4.4.7) — K1 V2 actual init

Header: `driver/i2s.h`. The struct alias `typedef i2s_driver_config_t i2s_config_t;` exists for source compatibility.

> **K1 V2 diverges from the canonical-spec File 4 SPH0645 datasheet-textbook config in three coupled ways: `STAND_MSB` (not Philips), `RIGHT_LEFT` stereo with data in RIGHT slot (not `ONLY_LEFT`), and `>>10` extraction shift (not `>>14`).** The divergence is intentional — validated by hardware test and recorded as bugfix [#23164 (2026-01-30) — "I2S Audio Capture Legacy Driver Bit Shift Correction"](../../../.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip/memory/MEMORY.md). The textbook Philips+ONLY_LEFT+`>>14` recipe in File 4 §10 of the canonical spec **does not work on K1 V2**; do not propagate it. Source of truth: `src/audio/AudioCapture.cpp:695–748` + `src/config/audio_config.h:44–146`.

```c
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "soc/i2s_reg.h"   /* for register-level tweaks after init */

/* From driver/i2s.h:69, 86 (verified verbatim from IDF 4.4.7) */
typedef struct {
    int mck_io_num;     /* MCK in/out — K1 does not drive MCLK; set I2S_PIN_NO_CHANGE */
    int bck_io_num;     /* BCK pin */
    int ws_io_num;      /* WS  pin */
    int data_out_num;   /* DOUT — I2S_PIN_NO_CHANGE for RX-only */
    int data_in_num;    /* DIN  — mic DOUT line lands here */
} i2s_pin_config_t;

/* K1 V2 actual i2s_config_t (verified from AudioCapture.cpp:695–709) */
i2s_config_t i2sConfig = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = 32000,                         /* SAMPLE_RATE — _32khz env */
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,    /* stereo capture, mic data in one slot */
    .communication_format = I2S_COMM_FORMAT_STAND_MSB,     /* MSB-aligned, NOT Philips */
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 4,                             /* DMA_BUFFER_COUNT */
    .dma_buf_len          = 1024,                          /* DMA_BUFFER_SAMPLES (512) × 2 for stereo */
    .use_apll             = false,                         /* APLL not available to I2S on S3 — silently ignored if true */
    .tx_desc_auto_clear   = false,
    .fixed_mclk           = 0,
    .mclk_multiple        = I2S_MCLK_MULTIPLE_256,
    .bits_per_chan        = I2S_BITS_PER_CHAN_32BIT,
};

esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, NULL);

i2s_pin_config_t pinConfig = {
    .mck_io_num   = I2S_PIN_NO_CHANGE,
    .bck_io_num   = K1_I2S_BCLK,    /* see chip_esp32s3.h — K1 V2 uses GPIO 13 */
    .ws_io_num    = K1_I2S_LRCL,    /* WS — GPIO 14 */
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = K1_I2S_DOUT,    /* mic DOUT lands here — GPIO 11 on K1 V2 */
};
err = i2s_set_pin(I2S_NUM_0, &pinConfig);

/* MANDATORY register tweaks AFTER i2s_set_pin() — mic-type dependent.
   Without these the MSB+RIGHT_LEFT path produces wrong-alignment samples. */
if constexpr (MICROPHONE_TYPE == MicType::IM69D130) {
    /* IM69D130 via ADAU7002: 24-bit data in LEFT slot, MSB-shift compensation */
    REG_SET_BIT(I2S_RX_CONF_REG(I2S_NUM_0),   I2S_RX_MSB_SHIFT);
    REG_CLR_BIT(I2S_RX_CONF_REG(I2S_NUM_0),   I2S_RX_WS_IDLE_POL);
    REG_SET_BIT(I2S_RX_CONF_REG(I2S_NUM_0),   I2S_RX_LEFT_ALIGN);
    REG_SET_BIT(I2S_RX_TIMING_REG(I2S_NUM_0), BIT(9));
} else {
    /* SPH0645 (K1 V2 default): 18-bit data in RIGHT slot, no MSB-shift */
    REG_CLR_BIT(I2S_RX_CONF_REG(I2S_NUM_0),   I2S_RX_MSB_SHIFT);
    REG_CLR_BIT(I2S_RX_CONF_REG(I2S_NUM_0),   I2S_RX_WS_IDLE_POL);
    REG_SET_BIT(I2S_RX_CONF_REG(I2S_NUM_0),   I2S_RX_LEFT_ALIGN);
    REG_SET_BIT(I2S_RX_TIMING_REG(I2S_NUM_0), BIT(9));
}
```

### Sample extraction (verified from `AudioCapture.cpp:266–298`)

DMA buffer is `int32_t[DMA_BUFFER_SAMPLES * 2]` — **stereo**, frame stride 2. The mic data lands in **one of the two slots** depending on mic type. Extraction uses a compile-time channel offset and shift, then a one-pole DC-blocking HPF before downstream FFT.

```c
/* From src/config/audio_config.h:44–49 */
enum class MicType : uint8_t { SPH0645, IM69D130 };
constexpr MicType MICROPHONE_TYPE = MicType::SPH0645;   /* K1 V2 default */

/* From AudioCapture.cpp:273–298 */
constexpr size_t CHANNEL_OFFSET = (MICROPHONE_TYPE == MicType::IM69D130) ? 0 : 1;
/*  SPH0645:  CHANNEL_OFFSET = 1 → RIGHT slot (odd DMA indices)
    IM69D130: CHANNEL_OFFSET = 0 → LEFT  slot (even DMA indices)              */
constexpr int    BIT_SHIFT      = (MICROPHONE_TYPE == MicType::IM69D130) ? 8 : 10;
/*  SPH0645:  >>10  (18-bit data, K1-empirical alignment — NOT >>14)
    IM69D130: >>8   (24-bit data via ADAU7002)                                */

for (size_t i = 0; i < HOP_SIZE; i++) {
    int32_t rawSample = m_dmaBuffer[i * 2 + CHANNEL_OFFSET];
    float   input     = (float)(rawSample >> BIT_SHIFT);

    /* One-pole DC-blocking HPF: y[n] = x[n] - x[n-1] + alpha * y[n-1] */
    float dcBlocked    = input - m_dcPrevInput + DC_BLOCK_ALPHA * m_dcPrevOutput;
    m_dcPrevInput      = input;
    m_dcPrevOutput     = dcBlocked;

    /* Clamp ±131072 (18-bit range), normalise to int16 */
    float  clamped     = std::max(-131072.0f, std::min(131072.0f, dcBlocked));
    int16_t sample     = (int16_t)(clamped * RECIP_SCALE * 32767.0f);
    buffer[i] = sample;
}
```

### Divergence map: canonical spec File 4 → K1 V2 actual

| Field | Canonical File 4 §10 (textbook SPH0645) | K1 V2 actual | Why divergent |
|---|---|---|---|
| `communication_format` | `STAND_I2S` (Philips, 1-BCLK shift after WS edge) | `STAND_MSB` (MSB-aligned, no shift) | Coupled with the post-init register tweaks; K1 hardware works at MSB+tweaks, not Philips |
| `channel_format` | `ONLY_LEFT` (mono read) | `RIGHT_LEFT` (stereo read, data in RIGHT slot) | SPH0645 SEL pin floating/wired such that data lands in RIGHT slot on K1 V2 PCB; mic-type switch handles both via `CHANNEL_OFFSET` |
| `dma_buf_len` | 512 frames | 1024 (= `DMA_BUFFER_SAMPLES × 2`) | Stereo doubles per-frame stride |
| Sample shift | `>>14` (Philips → 18-bit data lands at bits 31..14) | **`>>10`** (MSB+register-tweaks → 18-bit data lands at bits 31..10) | Bugfix #23164 (2026-01-30) corrected this after hardware test; the `>>14` recipe does not produce usable samples on K1 V2 |
| Post-init register tweaks | Not present | **Required.** `I2S_RX_LEFT_ALIGN=1`, `I2S_RX_TIMING_REG BIT(9)=1`, `I2S_RX_MSB_SHIFT` set/cleared by mic type, `I2S_RX_WS_IDLE_POL=0` | Compensates for SPH0645's actual signal timing vs Philips ideal |
| DC-blocking HPF | Mentioned (File 4 §9) | Implemented in `captureHopBlocking()` — one-pole `y[n] = x[n] - x[n-1] + α·y[n-1]` | Canonical and K1 agree |

### Legacy-API translation (v5.x → 4.4.7)

| File 4 §10 (v5.x) | 4.4.7 equivalent |
|---|---|
| `i2s_new_channel(&chan_cfg, NULL, &rx)` | `i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, NULL)` |
| `i2s_chan_handle_t rx` | port number (`I2S_NUM_0` / `I2S_NUM_1`) is the handle |
| `i2s_channel_init_std_mode(rx, &cfg)` | `i2s_set_pin(I2S_NUM_0, &pinConfig)` (config + pins split in legacy API) |
| `i2s_channel_enable(rx)` | implicit on `i2s_driver_install` |
| `i2s_channel_read(rx, buf, n, &got, to)` | `i2s_read(I2S_NUM_0, buf, n, &got, to)` |
| `I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(...)` | `.communication_format = I2S_COMM_FORMAT_STAND_MSB` + post-init register tweaks (see above) |

Headers `driver/i2s_std.h`, `driver/i2s_pdm.h`, `driver/i2s_tdm.h` **do not exist** in 4.4.7. Do not `#include` them. The vendored `src/audio/backends/esv11/vendor/microphone.h` includes `#if __has_include("driver/i2s_std.h")` as a portability shim — that branch is **dead code on 4.4.7**, the legacy `driver/i2s.h` branch runs.

### What NOT to copy from canonical File 4

- **Do not use** the `i2s_new_channel` / `i2s_channel_init_std_mode` / `i2s_chan_handle_t` API surface. Header absent in 4.4.7.
- **Do not use** `I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(...)`. Macro absent; behaviour wrong for K1 even with v5.x equivalent.
- **Do not use** `>>14` shift on K1 V2 raw mic samples. Use `>>10` (SPH0645) or `>>8` (IM69D130) per the compile-time `MicType` switch.
- **Do not omit** the post-init register tweaks (`I2S_RX_*`). Without them the MSB+RIGHT_LEFT path produces wrong-alignment samples.

## RMT (4.4.7)

K1 V2 does not call the IDF RMT driver directly. **FastLED owns the LED path** via its RMT4 backend (`esp32_idf4_rmt_impl.cpp`) plus the async-RMT patch documented in [`project_async_rmt_120fps`](../../../.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip/memory/project_async_rmt_120fps.md). The legacy IDF surface below is reference-only.

Header: `driver/rmt.h`. Public surface verified in 4.4.7:

```c
#include "driver/rmt.h"

/* From driver/rmt.h — rmt_config_t */
typedef struct {
    rmt_mode_t      rmt_mode;        /* RMT_MODE_TX / RMT_MODE_RX */
    rmt_channel_t   channel;         /* RMT_CHANNEL_0..7 (S3: 0–3 TX, 4–7 RX) */
    gpio_num_t      gpio_num;
    uint8_t         clk_div;         /* 80 → 1 MHz tick from 80 MHz APB; for 10 MHz tick (100 ns) use 8 */
    uint8_t         mem_block_num;
    uint32_t        flags;           /* RMT_CHANNEL_FLAGS_* */
    union {
        rmt_tx_config_t tx_config;
        rmt_rx_config_t rx_config;
    };
} rmt_config_t;

esp_err_t rmt_config(const rmt_config_t *rmt_param);
esp_err_t rmt_driver_install(rmt_channel_t channel, size_t rx_buf_size, int intr_alloc_flags);
esp_err_t rmt_write_items(rmt_channel_t channel, const rmt_item32_t *rmt_item,
                          int item_num, bool wait_tx_done);
esp_err_t rmt_wait_tx_done(rmt_channel_t channel, TickType_t wait_time);
```

`rmt_channel_t` is **not removed** from user space in 4.4.7 (the migration-guide note in canonical File 1 §10 about "removed from user space" describes the **5.x destination**, not our current state). `rmt_item32_t` is the canonical symbol unit; **`rmt_symbol_word_t` does not exist in 4.4.7**. Use `rmt_item32_t`:

```c
typedef union {
    struct { uint32_t duration0:15, level0:1, duration1:15, level1:1; };
    uint32_t val;
} rmt_item32_t;
```

For FastLED-on-K1 the relevant compile-time flags (File 3 §12) apply unchanged: `FASTLED_RMT_MAX_CHANNELS`, `FASTLED_RMT_MEM_BLOCKS`, `FASTLED_RMT_BUILTIN_DRIVER`. The async-RMT patch is on `feature/codex-perceptual-translation-v4` (dead branch — needs porting to main per [`project_codex_branch_dead`](../../../.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip/memory/project_codex_branch_dead.md)).

## GDMA (4.4.7)

Header: `esp_private/gdma.h`. K1 does not currently allocate GDMA channels at the application layer — they are claimed internally by `i2s_driver_install` (1 RX channel for SPH0645/ESV11). If/when direct GDMA use is needed (e.g. RMT-DMA mode, custom M2M), the 4.4.7 surface is:

```c
#include "esp_private/gdma.h"

/* Present in 4.4.7 — verified */
esp_err_t gdma_new_channel(const gdma_channel_alloc_config_t *cfg,
                           gdma_channel_handle_t *ret_chan);
esp_err_t gdma_del_channel(gdma_channel_handle_t);
esp_err_t gdma_connect(gdma_channel_handle_t, gdma_trigger_t trig);
esp_err_t gdma_disconnect(gdma_channel_handle_t);
esp_err_t gdma_set_transfer_ability(gdma_channel_handle_t,
                                    const gdma_transfer_ability_t *ability);
esp_err_t gdma_apply_strategy(gdma_channel_handle_t,
                              const gdma_strategy_config_t *config);
esp_err_t gdma_register_tx_event_callbacks(gdma_channel_handle_t,
                                           gdma_tx_event_callbacks_t *cbs,
                                           void *user_data);
esp_err_t gdma_register_rx_event_callbacks(gdma_channel_handle_t,
                                           gdma_rx_event_callbacks_t *cbs,
                                           void *user_data);
esp_err_t gdma_start(gdma_channel_handle_t, intptr_t desc_base);
esp_err_t gdma_stop(gdma_channel_handle_t);
esp_err_t gdma_append(gdma_channel_handle_t);
esp_err_t gdma_reset(gdma_channel_handle_t);

/* Absent in 4.4.7 — DO NOT USE on K1 */
/* gdma_new_ahb_channel             — added in v5.4 (the 5.x bus-split rename) */
/* gdma_config_transfer             — added in v5.3 */
/* gdma_get_free_m2m_trigger_id_mask */
```

Use `gdma_new_channel` (not `gdma_new_ahb_channel`); the AHB/AXI bus split is 5.x-only and AXI doesn't apply to S3 in either version anyway.

## PSRAM cache flush (4.4.7)

The `esp_mm` component (which exposes `esp_cache_msync(buf, size, ESP_CACHE_MSYNC_FLAG_DIR_C2M)`) is **v5.x only**. On 4.4.7 the equivalent surface is split between the ROM cache helpers and the SPIRAM driver:

```c
/* From components/esp_rom/include/esp32s3/rom/cache.h */
int Cache_WriteBack_Addr(uint32_t addr, uint32_t size);   /* CPU-wrote → DMA-will-read; C2M */
int Cache_Invalidate_Addr(uint32_t addr, uint32_t size);  /* DMA-wrote → CPU-will-read; M2C */

/* From components/esp_hw_support/include/soc/esp32s3/spiram.h */
esp_err_t esp_spiram_writeback_cache(void);   /* coarse-grained — entire DCache */
```

Recipe for GDMA-to/from-PSRAM (File 2 §5 doctrine, 4.4.7-equivalent):

```c
/* Before DMA-out: CPU has written to the PSRAM buffer; DMA must read fresh bytes. */
Cache_WriteBack_Addr((uint32_t)psram_buf, buf_size);
/* Kick GDMA */
gdma_start(tx_chan, (intptr_t)desc_head);

/* After DMA-in: DMA has written; CPU must invalidate so it doesn't read stale cache. */
/* (wait for OUT_EOF / IN_SUC_EOF callback first) */
Cache_Invalidate_Addr((uint32_t)psram_buf, buf_size);
```

`esp_spiram_writeback_cache()` is the coarse fallback when address-range tracking is impractical (entire DCache flushed). Prefer the address-range form for known buffer ranges.

**K1 reality check:** K1 doesn't currently move DMA payloads through PSRAM. This section is forward-looking for the [`project_k1_heap_pressure_witchhunt_2026_05_18`](../../../.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip/memory/project_k1_heap_pressure_witchhunt_2026_05_18.md) Phase 1 plan (relocating `AsyncWebSocketSharedBuffer` payloads to PSRAM). Note that AsyncWebSocket payloads are **TCP**, not DMA-touched in user space — the cache-flush concern only arises if the same buffer is also handed to a DMA-capable peripheral.

## Heap-shed measurement (load-bearing)

The single most actionable correction from the canonical spec to our active witchhunt: **the 30 KB heap shed threshold must be measured against the SRAM1 D-view region specifically, not against `MALLOC_CAP_INTERNAL` aggregate.**

Reason (File 1 §5 doctrine, applies to 4.4.7 as-is): `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)` returns the max across ALL internal regions including idle RTC FAST (typically ~7 KB free, never the binding constraint). If SRAM1 D-view (the actual lwIP-pbuf-eligible region) is starved to 4 KB largest, that 4 KB is the truth — but the aggregate function reports the RTC FAST 7 KB.

Diagnostic recipe (4.4.7-compatible):

```c
#include "esp_heap_caps.h"

void k1_log_heap_shed_state(void) {
    /* Per-region histogram — call AT SHED-DECISION BOUNDARIES only,
       never per-frame. heap_caps_print_heap_info acquires the heap mutex
       and contends with Core 1 renderer — see feedback_no_heap_scans_in_high_freq_paths. */
    ESP_LOGI("HEAP", "MALLOC_CAP_INTERNAL per-region:");
    heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);

    /* DMA-capable subset (lwIP pbuf eligibility) — this is the binding region for UDP/TCP. */
    multi_heap_info_t dma_info;
    heap_caps_get_info(&dma_info, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_LOGI("HEAP", "DMA|INTERNAL free=%u largest=%u min_ever=%u",
             dma_info.total_free_bytes,
             dma_info.largest_free_block,
             dma_info.minimum_free_bytes);
    /* Shed decision: compare dma_info.largest_free_block — NOT MALLOC_CAP_INTERNAL aggregate
       — against the 30 KB threshold. */
}
```

The `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL` mask intersects to roughly SRAM1 D-view + SRAM2 (per File 1 §5 capability-to-region map), which is exactly the lwIP-pbuf-eligible region. That `largest_free_block` is the figure the shed logic should gate on. Verify against the per-region output of `heap_caps_print_heap_info(MALLOC_CAP_INTERNAL)` — the `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL` largest should match the SRAM1 D-view region's largest in the per-region print.

Subtract `MULTI_HEAP_BLOCK_OWNER_SIZE()` (≈12 B with poisoning enabled, 4 B without) from `largest_free_block` to get the true allocatable max — the heap allocator reserves owner metadata in each block.

This recipe is the "concrete next step" cited at the close of the canonical spec review. Run on K1 V2 at idle and at the operating point where shed currently fires; if the SRAM1 D-view largest is materially different from the aggregate the witchhunt has been logging, the 28-vs-30 KB framing needs to be redone against the D-view number.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-19 | agent:claude-opus-4-7 | Created. Companion to ESP32-S3-Canonical-Reference-Skills.md. Restates I2S / RMT / GDMA / cache APIs using legacy 4.4.7 headers. All signatures verified verbatim from `framework-espidf@3.40407.240606` (ESP_IDF_VERSION 4.4.7) on 2026-05-19. Heap-shed measurement section (SRAM1 D-view vs MALLOC_CAP_INTERNAL aggregate) flagged as the load-bearing concrete next step for the active heap-pressure witchhunt. |
| 2026-05-19 | agent:claude-opus-4-7 | Audio-pipeline audit correction. Initial I2S code block copied File 4 §10's textbook SPH0645 config (Philips + ONLY_LEFT + >>14) — verified against `src/audio/AudioCapture.cpp:695–748` + `src/config/audio_config.h:44–146` and found K1 V2 actually runs MSB + RIGHT_LEFT + >>10 with required post-`i2s_set_pin` register tweaks (`I2S_RX_LEFT_ALIGN`, `I2S_RX_TIMING_REG BIT(9)`, mic-type-dependent `I2S_RX_MSB_SHIFT`). Divergence validated by bugfix #23164 (2026-01-30 — "I2S Audio Capture Legacy Driver Bit Shift Correction"). Replaced the I2S section with K1-actual config, added sample-extraction pattern (`CHANNEL_OFFSET` + `BIT_SHIFT` compile-time switch on `MicType`), divergence map vs canonical File 4, "what NOT to copy" list. Abstract + trust-map row updated accordingly. |
