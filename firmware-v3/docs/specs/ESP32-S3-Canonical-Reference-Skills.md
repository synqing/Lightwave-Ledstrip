# ESP32-S3 Canonical Reference Skills — RTFM Artefact

Four SKILL.md files for engineering-agent consumption. Each is the canonical silicon-truth reference paired with ESP-IDF/Arduino-ESP32 API wrappers. TRM citations refer to **ESP32-S3 Technical Reference Manual v1.8** (https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf). Errata cited against the unified **ESP Chip Errata** project (https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32s3/) — the standalone `esp32-s3_errata_en.pdf` URL now redirects to this project.

---

# FILE 1 — `skills/esp32s3-memory-architecture/SKILL.md`

```
skills/esp32s3-memory-architecture/
├── SKILL.md                 (this file)
├── reference/
│   ├── address-map.md       (full Table 4.3-1 / 4.3-2 reproduction)
│   ├── pms-registers.md     (PMS_CORE_X_* register field decoder)
│   └── linker-sections.md   (memory.ld.in + sections.ld.in annotated)
```

## SKILL.md

**Purpose.** The authoritative ESP32-S3 internal/external memory model, the PMS partitioning that decides what is IRAM vs DRAM at link time, the cache model that steals from SRAM0/SRAM2, the GDMA-access constraints, and the ESP-IDF heap-caps API surface that exposes all of this. Required reading before any allocation diagnostic, any PSRAM decision, or any DMA buffer placement.

**The doctrine.** `MALLOC_CAP_INTERNAL` is not one heap — it is **five disjoint regions** with different bus-access rules, different DMA eligibility, and different cache-stealing behaviour. `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)` returns the max across all regions; if one region is idle (RTC SLOW often is) the number is misleading. Always use `heap_caps_print_heap_info()` and read per-region.

### 1. The address map (TRM §4.3.1, Table 4.3-1, Figure 4.2-1)

| Region | Bus | Range | Size | Properties |
|---|---|---|---|---|
| Internal ROM 0 | IBUS | `0x4000_0000`–`0x4003_FFFF` | 256 KB | Boot ROM, RX, not heap |
| Internal ROM 1 | IBUS | `0x4004_0000`–`0x4005_FFFF` | 128 KB | ROM continuation (TRM §15.3.1.1 Table 15.3-1). **Total ROM = 384 KB**. |
| **Internal SRAM 0** | **IBUS only** | `0x4037_0000`–`0x4037_7FFF` | 32 KB | Blocks 0+1. Splittable CPU IRAM ↔ ICache via `PMS_INTERNAL_SRAM_USAGE_1_REG` (TRM §15.3.2.2). **Unreachable to GDMA.** |
| **Internal SRAM 1** (I-view) | IBUS | `0x4037_8000`–`0x403D_FFFF` | 416 KB | Blocks 2–8. Executable. |
| **Internal SRAM 1** (D-view) | DBUS | `0x3FC8_8000`–`0x3FCE_FFFF` | 416 KB | Same physical bytes as I-view. Word-aligned data view. **DMA-capable.** Splittable into up to 6 regions via `IRam0_DRam0_split_line` + sub-split lines (TRM §15.3.2.3). |
| **Internal SRAM 2** | **DBUS only** | `0x3FCF_0000`–`0x3FCF_FFFF` | 64 KB | Blocks 9+10. Splittable CPU/GDMA ↔ DCache via `PMS_INTERNAL_SRAM_USAGE_1_REG` (TRM §15.3.2.4). **DMA-capable for the CPU/GDMA portion only.** |
| **RTC FAST** | IBUS + DBUS | `0x600F_E000`–`0x600F_FFFF` | 8 KB | CPU-only. Retained across deep sleep. **Not DMA-reachable.** |
| **RTC SLOW** | DBUS + peripheral-view alias | `0x5000_0000`–`0x5000_1FFF`, alias `0x6002_1000`–`0x6002_2FFF` | 8 KB | CPU + ULP coprocessor. Retained across deep sleep. **Not DMA-reachable.** TRM §15.3.4.1 Table 15.3-14. |
| External Flash/PSRAM (I) | IBUS | `0x4200_0000`–`0x43FF_FFFF` | 32 MB virtual | XIP via ICache + MMU (TRM §4.3.3.1) |
| External Flash/PSRAM (D) | DBUS | `0x3C00_0000`–`0x3DFF_FFFF` | 32 MB virtual | DCache + MMU. **GDMA-capable** with burst alignment + manual cache flush (TRM §3.4.9). |

**Total on-die memory = 384 KB ROM + 512 KB SRAM + 16 KB RTC.** Net heap after default 32 KB ICache + 32 KB DCache + bootloader reservations ≈ 380 KB usable.

### 2. PMS — the silicon contract for IRAM vs DRAM (TRM Chapter 15, §15.3)

Internal SRAM1 is the only region that is physically split between executable (IRAM) and data (DRAM) tenants via Permission Control. The relevant registers (TRM §15.10 register summary):

- `PMS_CORE_0_IRAM0_DRAM0_DMA_SPLIT_LINE_CONSTRAIN_0_REG` through `_4_REG` (and the same set for `CORE_1`) — five 32-bit registers per core defining the split lines.
- `PMS_INTERNAL_SRAM_USAGE_1_REG` — selects CPU vs ICache split (SRAM0) and CPU/GDMA vs DCache split (SRAM2).

Field semantics (TRM §15.10):
- **`SPLITADDR[7:0]`** — 8-bit address. Granularity = 256 bytes (the 8 bits encode address bits `[15:8]` of the SRAM offset). **Hence the 256-byte SPLITADDR alignment requirement.**
- **`CATEGORY_n[1:0]`** for `n ∈ 0..6` — per-sub-region access category: `00`=invalid, `01`=IRAM, `10`=DRAM, `11`=reserved.
- **`IRam0_DRam0_split_line`** — the canonical line: addresses below = IRAM (via `0x4037_8000`+), addresses above = DRAM (via `0x3FC8_8000`+).

This is the silicon mechanism the linker's `.dram0.dummy` section mirrors at build time (see §6).

### 3. Cache (TRM §4.3.3.2–§4.3.3.3)

| | Sizes | Associativity | Block sizes |
|---|---|---|---|
| **ICache** | 16 KB / 32 KB (steals from SRAM0) | 4-way / 8-way | 16 B / 32 B |
| **DCache** | 32 KB / 64 KB (steals from SRAM2) | 4-way / 8-way | 16 B / 32 B / 64 B |

Features (TRM §4.3.3.3): Auto-Preload, Manual-Preload, Lock/Unlock (pin lines), invalidate, write-back (DCache only). Dual-core arbiter for shared access.

**Footgun (OPI PSRAM requires 64-byte DCache lines).** Default Arduino-core build that sets `CONFIG_ESP32S3_DATA_CACHE_LINE_32B` against an N16R8 (Octal PSRAM) corrupts data beyond the cache footprint — see [arduino-esp32#12480](https://github.com/espressif/arduino-esp32/issues/12480). Always pair `CONFIG_SPIRAM_MODE_OCT=y` with `CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y`.

**Errata CACHE-126.** All ESP32-S3 silicon revisions (v0.0–v0.2). Cache write-back can return stale/duplicate data if another CPU/ISR/DMA touches the same cache line during writeback. **Mitigated in software** by ESP-IDF v4.4.6+, v5.0.4+, v5.1.1+, v5.2+ via cache freeze. No silicon fix scheduled.

### 4. GDMA address space (TRM §4.3.4, §3.4.8–§3.4.11)

GDMA can reach:
- Internal SRAM1 via D-view `0x3FC8_8000`–`0x3FCE_FFFF`
- Internal SRAM2 via `0x3FCF_0000`–`0x3FCF_FFFF`
- External PSRAM `0x3C00_0000`–`0x3DFF_FFFF` (burst mode + manual cache sync required)

GDMA **cannot** reach:
- Internal SRAM0 (IBUS-only; reserved for CPU/ICache)
- ROM, RTC FAST, RTC SLOW
- Any portion of SRAM0/SRAM2 currently consumed by ICache/DCache

**Cache coherency is software's responsibility** for PSRAM-backed DMA buffers. Use `esp_cache_msync(buf, size, ESP_CACHE_MSYNC_FLAG_DIR_C2M)` before DMA-out and `…_DIR_M2C` before reading after DMA-in (TRM §3.4.9; ESP-IDF `mm_sync.html`). DMA *descriptors themselves* must live in internal DRAM.

### 5. ESP-IDF heap-caps API (`components/heap/include/esp_heap_caps.h`)

Verified signatures (master):

```c
void  *heap_caps_malloc(size_t size, uint32_t caps);
void  *heap_caps_calloc(size_t n, size_t size, uint32_t caps);
void  *heap_caps_realloc(void *ptr, size_t size, uint32_t caps);
void  *heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps);
void   heap_caps_free(void *ptr);

size_t heap_caps_get_total_size(uint32_t caps);
size_t heap_caps_get_free_size(uint32_t caps);
size_t heap_caps_get_minimum_free_size(uint32_t caps);
size_t heap_caps_get_largest_free_block(uint32_t caps);
size_t heap_caps_get_allocated_size(void *ptr);
void   heap_caps_get_info(multi_heap_info_t *info, uint32_t caps);
void   heap_caps_print_heap_info(uint32_t caps);   /* per-region histogram */

bool   heap_caps_check_integrity(uint32_t caps, bool print_errors);
bool   heap_caps_check_integrity_all(bool print_errors);
void   heap_caps_dump(uint32_t caps);
void   heap_caps_dump_all(void);

esp_err_t heap_caps_register_failed_alloc_callback(esp_alloc_failed_hook_t cb);
```

**Capability → silicon-region map** (from `components/heap/port/esp32s3/memory_layout.c`):

| Cap flag | Bit | Maps to |
|---|---|---|
| `MALLOC_CAP_EXEC` | `1<<0` | IRAM slice of SRAM0 + DIRAM view of SRAM1 + RTC FAST |
| `MALLOC_CAP_32BIT` | `1<<1` | All DIRAM/DRAM + IRAM-only region. **Cannot hold floats** (S3 FP instructions can't access IRAM). |
| `MALLOC_CAP_8BIT` | `1<<2` | DRAM-accessible SRAM1/2 + PSRAM |
| `MALLOC_CAP_DMA` | `1<<3` | SRAM1 DIRAM + SRAM2 only. **Excludes PSRAM.** |
| `MALLOC_CAP_SPIRAM` | `1<<10` | External PSRAM |
| `MALLOC_CAP_INTERNAL` | `1<<11` | All internal SRAM + RTC FAST |
| `MALLOC_CAP_DEFAULT` | `1<<12` | What `malloc()` returns; includes PSRAM when `CONFIG_SPIRAM_USE_MALLOC=y` |
| `MALLOC_CAP_IRAM_8BIT` | `1<<13` | **Empty on S3** (only S2 has unaligned IRAM) |
| `MALLOC_CAP_RTCRAM` | `1<<15` | RTC FAST `0x600F_E000` |

**The diagnostic doctrine.** `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)` returns the max across **all** internal regions. If RTC FAST is idle with 7,668 B free and SRAM1 D-view is under heavy pressure with 4 KB largest, the function returns **7,668** — *the wrong number for diagnosing allocation failure of a 6 KB SRAM1 buffer.* Always call `heap_caps_print_heap_info(MALLOC_CAP_INTERNAL)` and read each `At 0x… len … largest_free_block …` line individually. Also subtract `MULTI_HEAP_BLOCK_OWNER_SIZE()` (≈12 B with poisoning, 4 B without) to get the true allocatable max.

### 6. Attribute macros and linker sections

Macros (`esp_attr.h`):

| Macro | Section | Lands in |
|---|---|---|
| `IRAM_ATTR` | `.iram0.text` | SRAM1 IRAM region |
| `DRAM_ATTR` | `.dram0.data`/`.bss` | SRAM1 DRAM region |
| `EXT_RAM_BSS_ATTR` | `.ext_ram.bss` | PSRAM (requires `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y`) |
| `EXT_RAM_NOINIT_ATTR` | `.ext_ram_noinit` | PSRAM, not zeroed |
| `RTC_DATA_ATTR` | `.rtc.data` | RTC SLOW (or FAST if `CONFIG_ESP32S3_RTCDATA_IN_FAST_MEM=y`) |
| `RTC_FAST_ATTR` | `.rtc.force_fast` | RTC FAST always |
| `RTC_SLOW_ATTR` | `.rtc.force_slow` | RTC SLOW always |
| `DMA_ATTR` | `.dram1.<sym>` 4B-aligned | SRAM1 DRAM, GDMA-suitable |

Linker sections (`components/esp_system/ld/esp32s3/sections.ld.in`):
- `.iram0.vectors`, `.iram0.text` → `iram0_0_seg`
- `.dram0.data`, `.dram0.bss` → `dram0_0_seg`
- `.flash.text` → `iram0_2_seg` (IROM at `0x4200_0020`)
- `.flash.rodata` → `drom0_0_seg` (DROM at `0x3C00_0020`)
- `.ext_ram.bss`, `.ext_ram_noinit` → `extern_ram_seg`
- `.rtc.*` → `rtc_iram_seg`, `rtc_data_seg`, `rtc_slow_seg`

**`.dram0.dummy` — the IRAM-padding artefact.** SRAM1 is mapped at both `0x4037_8000` (IBUS) and `0x3FC8_8000` (DBUS) to the *same physical bytes*. `.iram0.text` consumes physical SRAM1 from the bottom; `.dram0.dummy (NOLOAD)` pads the start of `dram0_0_seg` by `_iram_end - _diram_i_start` bytes so DRAM begins **above** IRAM in physical SRAM1. This is the linker-script analogue of the hardware `IRam0_DRam0_split_line`.

The canonical link-time error when IRAM overflows:
```
ASSERT failed: IRAM0 segment data does not fit.
```

### 7. sdkconfig — N16R8 canonical settings

| Option | Required value | Why |
|---|---|---|
| `CONFIG_SPIRAM` | `y` | 8 MB PSRAM |
| `CONFIG_SPIRAM_MODE_OCT` | `y` | N16R8 is Octal, not Quad |
| `CONFIG_ESP32S3_DATA_CACHE_LINE_64B` | `y` | Required for OPI |
| `CONFIG_SPIRAM_SPEED_80M` | `y` | Match flash speed |
| `CONFIG_SPIRAM_USE_MALLOC` | `y` | Route `malloc()` through caps allocator |
| `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` | tune (default 16384) | Requests larger than this prefer PSRAM. **This is the heap shed threshold.** |
| `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL` | tune (default 32768) | Internal-only reserve for `MALLOC_CAP_INTERNAL`/`_DMA` |
| `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY` | `y` | Enables `EXT_RAM_BSS_ATTR` |
| `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP` | consider `y` | Moves Wi-Fi/lwIP to PSRAM where possible |
| PlatformIO: `board_build.arduino.memory_type=qio_opi` | required | Bootloader flash QIO + PSRAM OPI |

### 8. What most agents get wrong

- **Treating "MALLOC_CAP_INTERNAL is fragmented" as the failure mode** when in fact the request hits a region constraint (DMA-only, IRAM-only). Always look at per-region info.
- **Assuming PSRAM is DMA-capable.** Silicon: yes, with burst+coherency. Driver-level: lwIP pbufs, Wi-Fi RX, RMT non-DMA mode, FreeRTOS task stacks all require internal RAM. Per-driver constraints dominate.
- **Allocating with `MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM`** → returns `NULL` (mutually exclusive). For PSRAM DMA payload, use `heap_caps_aligned_alloc(64, size, MALLOC_CAP_SPIRAM)` and manage cache sync manually.
- **Putting DMA descriptors in PSRAM.** Espressif explicitly forbids this in `external-ram.html`; descriptors must be in internal DRAM even if the payload is in PSRAM.
- **`MALLOC_CAP_32BIT` for floats.** S3 FP instructions cannot access IRAM, and `MALLOC_CAP_32BIT` can return IRAM memory.
- **Heap shed threshold equal to structural floor.** If `MALLOC_CAP_INTERNAL` floor under load is 7 KB (the idle RTC SLOW), and the resume-at-28 KB shed threshold is also measured against that aggregate number, the shed logic never resumes. **The heap shed must be measured against the SRAM1 D-view region specifically.**

### 9. Real-world footgun catalogue

- [esp-idf#3529](https://github.com/espressif/esp-idf/issues/3529) — "Heap space is sufficient, but malloc fails" — the canonical region-vs-largest-free-block misdiagnosis.
- [esp-idf#9533](https://github.com/espressif/esp-idf/issues/9533) — `readdir()` consumes ~170 KB internal during scans of >380-entry directories.
- [esp-idf#13049](https://github.com/espressif/esp-idf/issues/13049) — `esp_psram_init()` returns `259` when bootloader already initialised; don't double-init.
- [esp-idf#13285](https://github.com/espressif/esp-idf/issues/13285) — v5.1 consumes ~30 KB more internal SRAM than v4.4 with OPI PSRAM.
- [esp-idf#14266](https://github.com/espressif/esp-idf/issues/14266) — "Virtual address not enough for PSRAM, 4MB mapped" on 8 MB N16R8 when DROM consumes the DBUS window. Mitigate with `CONFIG_SPIRAM_RODATA=y`.
- [esp-idf#14835](https://github.com/espressif/esp-idf/issues/14835) — `MALLOC_CAP_EXEC` on S3 may return RTC FAST through its DBUS alias which is not executable; `esp_ptr_executable()` returns false.
- [arduino-esp32#10664](https://github.com/espressif/arduino-esp32/issues/10664) — genuine lwIP leak in `mem_malloc` via `sys_timeout_abs` on Wi-Fi reconnect storms (3.0.7 / 3.1.0-RC1).
- AsyncWebSocket "tcp_alloc … (required to lock TCPIP functionality!)" assert — caused by **two competing AsyncTCP forks** in the libraries directory. Use `Async TCP by ESP32Async` (3.4.x) + `ESP Async WebServer by ESP32Async` (1.2.x) and delete `dvarrel/AsyncTCP`.

### 10. Diagnostic commands

**Runtime:**
```c
heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);   /* per-region histogram */
heap_caps_print_heap_info(MALLOC_CAP_SPIRAM);
heap_caps_print_heap_info(MALLOC_CAP_DMA);
heap_caps_dump_all();                              /* every block, every region */
heap_caps_check_integrity_all(true);               /* canary/poisoning check */

multi_heap_info_t info;
heap_caps_get_info(&info, MALLOC_CAP_DMA);
ESP_LOGI("MEM","DMA free=%u largest=%u min=%u",
   info.total_free_bytes, info.largest_free_block, info.minimum_free_bytes);
```

**Per-task accounting** (requires `CONFIG_HEAP_TASK_TRACKING=y` + poisoning):
```c
heap_caps_print_all_task_stat(stdout);        /* IDF v5+ */
heap_caps_get_per_task_info(&params);          /* legacy */
```

**Forensic ELF analysis:**
```
idf.py size                 # high-level used/total per region
idf.py size-components      # per-component IRAM/DRAM/Flash
idf.py size-files           # per source file
xtensa-esp32s3-elf-size -A build/<app>.elf
xtensa-esp32s3-elf-nm --size-sort --reverse build/<app>.elf | head -50
xtensa-esp32s3-elf-objdump -h build/<app>.elf
```

### 11. SpectraSynq-specific implications

- **lwIP pbufs are `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL`** — Wi-Fi MAC RX DMA constraint. Pressure on SRAM1 D-view *directly* causes UDP `ENOMEM`. Mitigation: `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y`, reduce `CONFIG_LWIP_TCP_SND_BUF_DEFAULT`.
- **AsyncWebSocketSharedBuffer** (the `shared_ptr<vector<uint8_t>>` per-client payload) is held in internal DRAM until ACK. Either cap clients, or relocate the payload constructor via custom allocator to PSRAM (acceptable because TCP TX doesn't require DMA buffer in the user-data side).
- **ArduinoJson v7 `JsonDocument`** allocates internal by default. Use the `SpiRamAllocator` pattern:
  ```cpp
  struct SpiRamAllocator : ArduinoJson::Allocator {
    void* allocate(size_t n) override { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM); }
    void  deallocate(void* p) override { heap_caps_free(p); }
    void* reallocate(void* p, size_t n) override { return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM); }
  };
  ```
- **Effect static instances + mel-band lookup tables** in `.dram0.bss` should be moved with `EXT_RAM_BSS_ATTR` (requires `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y`).
- **Renderer/Audio task stacks** allocated by `xTaskCreate` are in internal heap and **cannot be moved to PSRAM** (FreeRTOS constraint unless `CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM=y`, which conflicts with cache-disable code paths).
- **Heap shed threshold** must be measured against the SRAM1 D-view region (the limiting DMA pool), not against `MALLOC_CAP_INTERNAL` aggregate. The 7,668 B floor that misled the diagnostic was the *RTC FAST* allocation reported through `largest_free_block`.

---

# FILE 2 — `skills/esp32s3-gdma/SKILL.md`

```
skills/esp32s3-gdma/
├── SKILL.md
├── reference/
│   ├── descriptor-format.md     (12-byte dma_descriptor_t bit layout)
│   ├── peripheral-matrix.md     (Fig 4.3-2 reproduced; 10 peripherals / 5+5 channels)
│   └── cache-sync-recipes.md    (esp_cache_msync patterns for PSRAM-backed DMA)
```

## SKILL.md

**Purpose.** The General DMA controller silicon model. Required reading before allocating any DMA buffer, configuring any I2S/SPI/LCD/Camera/RMT-DMA peripheral, or debugging cache-coherency issues with PSRAM payloads.

**Cross-references.** Memory layout: `skills/esp32s3-memory-architecture`. RMT-DMA path: `skills/esp32s3-rmt` (RMT optionally uses one GDMA channel via `with_dma=true`). I2S-DMA: `skills/esp32s3-i2s` (always uses GDMA on S3).

### 1. Architecture (TRM §3.1–§3.3, §4.3.4)

- **Single AHB-GDMA group.** `SOC_AHB_GDMA_SUPPORTED=1`, `SOC_AXI_GDMA_SUPPORTED` undefined for S3 (AXI is P4-only).
- **5 TX + 5 RX = 10 independent channels.** Each TX/RX pair shares an interrupt source (`GDMA_LL_AHB_TX_RX_SHARE_INTERRUPT=1`).
- **10 peripherals can use GDMA** (TRM Figure 4.3-2): SPI2 (ID 0), SPI3 (1), UHCI0 (2), I2S0 (3), I2S1 (4), LCD_CAM (5), AES (6), SHA (7), ADC (8), **RMT (9, S3-specific)**. `GDMA_LL_INVALID_PERIPH_ID=0x3F` = unconnected.
- **Any-to-any matrix.** No fixed peripheral↔channel binding; any of the 5 TX or 5 RX channels can drive any peripheral. Conflicts arise only when more than 5 TX-using or 5 RX-using peripherals are simultaneously active.

### 2. Descriptor format (TRM §3.4.1, Figure 3.4-1)

```c
typedef struct dma_descriptor_s {
    uint32_t dw0;        /* see bitfield below */
    void    *buffer;     /* 4-byte aligned for internal RAM; burst-aligned for PSRAM */
    struct dma_descriptor_s *next;   /* NULL terminates chain */
} dma_descriptor_t;

/* dw0 layout */
/* bits  0..11  size      max 4095 (mult of 4 for RX) */
/* bits 12..23  length    bytes used (HW-set for RX, SW-set for TX) */
/* bits 24..29  reserved */
/* bit  30      suc_eof   1 = last in transaction */
/* bit  31      owner     0=CPU, 1=DMA (DMA_DESCRIPTOR_BUFFER_OWNER_DMA) */
```

**MAX 4095 bytes per descriptor.** Larger transfers chain descriptors.

**Alignment (TRM Tables 3.4-2, 3.4-3, 3.4-4):**
- Internal RAM: buffer 4-byte aligned, RX `size` multiple of 4.
- External PSRAM: buffer **and** size aligned to burst block (16/32/64 B per `psram_trans_align`).

**Ownership handshake.** Software fills descriptor, sets `owner=1`. DMA processes, optionally clears `owner=0` if `auto_update_desc=true`. If `owner_check=true`, a non-DMA-owned descriptor stops the channel with `DSCR_ERR`.

### 3. Transfer modes (TRM §3.4.2–§3.4.7)

- **P2M / M2P** (§3.4.2): TX channel reads memory, pushes to peripheral FIFO; RX writes peripheral data to memory.
- **M2M** (§3.4.3): pair TX+RX of the *same channel index*. Set `out_conf0.mem_trans_en=1`. ESP-IDF: `GDMA_TRIG_PERIPH_M2M` with `flags.reserve_sibling=1`.
- **Channel buffer** (§3.4.4): per-channel FIFO; queryable via `gdma_ll_tx_get_fifo_bytes()`.
- **Enabling** (§3.4.5): write descriptor head to `OUT_LINK_CHn.addr`, set `OUT_LINK_CHn.start=1`.
- **Linked-list reading** (§3.4.6): DMA fetches descriptor, checks owner, transfers, follows `next`. NULL `next` + `suc_eof=0` → `IN_DSCR_EMPTY`; software may `gdma_append()` more and resume.
- **EOF semantics** (§3.4.7): `OUT_EOF` = descriptor with `suc_eof=1` consumed; `OUT_TOTAL_EOF` = all bytes drained out the FIFO; `IN_SUC_EOF` = peripheral asserted EOF (I2S frame, LCD vsync); `IN_ERR_EOF` = peripheral terminated abnormally.

### 4. Internal RAM access (TRM §3.4.8)

GDMA reaches **SRAM1 D-view + SRAM2 only**. SRAM0 is shared exclusively between CPU and ICache; DMA buffers placed via `IRAM_ATTR` are **silently unreachable**.

Per-peripheral access is gated by `PMS_DMA_APBPERI_<PERI>_PMS_CONSTRAIN_1_REG` (TRM §15.3.2.3/4, Tables 15.3-7/8/10). Default-after-reset = full access. Unauthorised access fires interrupts per TRM §15.6.4 Table 15.6-4.

### 5. External RAM access — the cache-coherency contract (TRM §3.4.9–§3.4.11)

**Silicon truth:** any TX/RX channel can access PSRAM in `0x3C00_0000`–`0x3DFF_FFFF`. Constraints:

1. **Burst mode mandatory.** Set `gdma_transfer_ability_t.psram_trans_align = 16/32/64` (must match DCache line size).
2. **Cache flush mandatory.** ESP32-S3 has no hardware-coherent interconnect. Software must:
   - Before DMA-out (CPU wrote, DMA will read): `esp_cache_msync(buf, size, ESP_CACHE_MSYNC_FLAG_DIR_C2M)`.
   - After DMA-in (DMA wrote, CPU will read): `esp_cache_msync(buf, size, ESP_CACHE_MSYNC_FLAG_DIR_M2C)`.
3. **Descriptors stay in internal DRAM**, even when payload is in PSRAM.

**The PSRAM-DMA myth.** "PSRAM can't host DMA buffers on ESP32-S3" is **false at the silicon level**. It is true *per-driver*:
- lwIP pbufs (Wi-Fi MAC's internal DMA, not GDMA) need internal RAM.
- RMT DMA RX buffers currently need internal RAM.
- I2S TX/RX descriptors and (in practice) buffers go in internal RAM.
- LCD `esp_lcd_panel_io` accepts PSRAM framebuffers with `psram_trans_align=64` (OPI) / 16 (QSPI).

**N16R8 contention.** GDMA-PSRAM bandwidth is round-robin shared with both CPUs and cache fills, capping practical throughput at roughly half the PSRAM bus rate. The HUB75 LED-matrix driver hardcodes 13 MHz LCD clock on N8R8 boards for this reason.

### 6. Arbitration (TRM §3.4.12)

Two per-channel schemes:
- **Fixed priority.** `OUT_PRI_CHn.tx_pri = 0..15` / `IN_PRI_CHn.rx_pri = 0..15`. Higher = higher priority.
- **Weighted round-robin.** `OUT_WIGHT_CHn.tx_weight` / `IN_WIGHT_CHn.rx_weight`. (The register-name "WIGHT" typo is in TRM/headers verbatim.)

ESP-IDF: `gdma_apply_strategy()` for owner/auto-update flags; `gdma_set_priority()` (v5.3+) for priority value.

### 7. Interrupts (TRM §3.5)

Per channel (`DMA_IN_INT_*_CHn_REG` / `DMA_OUT_INT_*_CHn_REG`):

| Event | Bit | Meaning |
|---|---|---|
| `IN_DONE` | 0 | RX descriptor finished |
| `IN_SUC_EOF` | 1 | Peripheral asserted EOF |
| `IN_ERR_EOF` | 2 | Erroneous EOF |
| `IN_DSCR_ERR` | 3 | Owner mismatch / malformed descriptor |
| `IN_DSCR_EMPTY` | 4 | Chain exhausted, transfer incomplete |
| `OUT_DONE` | 0 | TX descriptor finished |
| `OUT_EOF` | 1 | TX `suc_eof=1` descriptor consumed |
| `OUT_DSCR_ERR` | 2 | Descriptor error |
| `OUT_TOTAL_EOF` | 3 | All bytes drained from FIFO to peripheral |

Mask `GDMA_LL_RX_EVENT_MASK = 0x1F`, `GDMA_LL_TX_EVENT_MASK = 0x0F`.

Debug-gold registers: `IN_SUC_EOF_DES_ADDR_CHn` and `IN_ERR_EOF_DES_ADDR_CHn` hold the address of the descriptor that triggered the EOF — walk backwards to recover length.

### 8. Programming sequence (TRM §3.6)

1. **Clock + reset (§3.6.1):** `SYSTEM_PERIP_CLK_EN1_REG.SYSTEM_DMA_CLK_EN=1`; pulse `SYSTEM_PERIP_RST_EN1_REG.SYSTEM_DMA_RST`. Per-channel: pulse `OUT_CONF0_CHn.out_rst` / `IN_CONF0_CHn.in_rst`.
2. **TX (§3.6.2):** reset → configure `OUT_CONF0_CHn` (data burst, descriptor burst, FIFO threshold) → set `OUT_PERI_SEL_CHn.sel` → `OUT_LINK_CHn.addr` = descriptor head → `OUT_LINK_CHn.start=1` → enable interrupts.
3. **RX (§3.6.3):** symmetric.
4. **M2M (§3.6.4):** same channel index N for TX and RX; set `OUT_CONF0_CHN.mem_trans_en=1`; peripheral select irrelevant.

### 9. ESP-IDF API (`components/esp_hw_support/dma/include/esp_private/gdma.h`)

```c
/* v5.4+ (and master): split by bus type */
esp_err_t gdma_new_ahb_channel(const gdma_channel_alloc_config_t *cfg,
                               gdma_channel_handle_t *ret_chan);
/* Deprecated in v5.4, removed in v6.0 */
esp_err_t gdma_new_channel(const gdma_channel_alloc_config_t *cfg,
                           gdma_channel_handle_t *ret_chan)
    __attribute__((deprecated));

esp_err_t gdma_del_channel(gdma_channel_handle_t);
esp_err_t gdma_get_channel_id(gdma_channel_handle_t, int *id);
esp_err_t gdma_connect(gdma_channel_handle_t, gdma_trigger_t trig);
esp_err_t gdma_disconnect(gdma_channel_handle_t);
esp_err_t gdma_set_transfer_ability(gdma_channel_handle_t, const gdma_transfer_ability_t*);
esp_err_t gdma_config_transfer(gdma_channel_handle_t, const gdma_transfer_config_t*);  /* v5.3+ */
esp_err_t gdma_apply_strategy(gdma_channel_handle_t, const gdma_strategy_config_t*);
esp_err_t gdma_register_tx_event_callbacks(gdma_channel_handle_t,
                                           gdma_tx_event_callbacks_t*, void *user_data);
esp_err_t gdma_register_rx_event_callbacks(gdma_channel_handle_t,
                                           gdma_rx_event_callbacks_t*, void *user_data);
esp_err_t gdma_start(gdma_channel_handle_t, intptr_t desc_base);
esp_err_t gdma_stop(gdma_channel_handle_t);
esp_err_t gdma_append(gdma_channel_handle_t);
esp_err_t gdma_reset(gdma_channel_handle_t);
esp_err_t gdma_get_free_m2m_trigger_id_mask(gdma_channel_handle_t, uint32_t *mask);
```

Trigger enum (`hal/gdma_types.h`):
```c
typedef enum {
    GDMA_TRIG_PERIPH_INVALID = -1,
    GDMA_TRIG_PERIPH_M2M, GDMA_TRIG_PERIPH_UART, GDMA_TRIG_PERIPH_SPI,
    GDMA_TRIG_PERIPH_I2S, GDMA_TRIG_PERIPH_AES, GDMA_TRIG_PERIPH_SHA,
    GDMA_TRIG_PERIPH_ADC, GDMA_TRIG_PERIPH_LCD, GDMA_TRIG_PERIPH_CAM,
    GDMA_TRIG_PERIPH_RMT,
} gdma_trigger_peripheral_t;
#define GDMA_MAKE_TRIGGER(p,i) ((gdma_trigger_t){.periph=(p),.instance_id=(i)})
```

`gdma_transfer_ability_t`:
```c
typedef struct {
    size_t sram_trans_align;    /* 4/8/16/32/64 */
    size_t psram_trans_align;   /* 16/32/64; >0 required for PSRAM */
} gdma_transfer_ability_t;
```

**Public-API alternative** for memory-to-memory: `esp_async_memcpy.h`. The GDMA driver itself is officially "private" — but every advanced driver uses it directly.

### 10. Footguns

1. **Channel exhaustion.** SPI2 + SPI3 + I2S0 + I2S1 + LCD_CAM = 5 channels used; further `gdma_new_ahb_channel` returns `ESP_ERR_NOT_FOUND`.
2. **`gdma_disconnect: no peripheral connected`** ([esp32-camera#750](https://github.com/espressif/esp32-camera/issues/750)) — caused by writing `peri_sel.sel` directly bypassing `gdma_connect`. Always pair direct register writes with the API.
3. **Descriptors in stack memory.** Reachable but overwritten when the function returns. Use `static` or `heap_caps_malloc(MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL)`.
4. **EOF never fires** ([esp-idf#12919](https://github.com/espressif/esp-idf/issues/12919)) — `suc_eof=1` must be on the last descriptor; circular chains without it only emit per-descriptor `OUT_DONE`.
5. **PSRAM corruption from missing cache sync.** Random LCD noise, ghost frames. Always `esp_cache_msync`.
6. **PSRAM DMA silently fails without burst mode** ([esp-rs/esp-hal#954](https://github.com/esp-rs/esp-hal/issues/954)) — `psram_trans_align` must be ≥ 16.
7. **`user context not in internal RAM`** ([esp-idf#11004](https://github.com/espressif/esp-idf/issues/11004)) — IRAM-safe callbacks reject PSRAM `user_data`. Allocate with `MALLOC_CAP_INTERNAL`.
8. **M2M requires `flags.reserve_sibling=1`** to ensure paired channels are in the same index.
9. **Owner-check + circular chain stalls** at the first non-DMA-owned descriptor. Disable owner-check via `gdma_apply_strategy` or initialise every descriptor with `owner=1` + `auto_update_desc=true`.
10. **SRAM0 unreachable.** `IRAM_ATTR` on a data array silently breaks DMA. Use `DRAM_ATTR` or `MALLOC_CAP_DMA`.

### 11. Errata (none GDMA-specific)

- **CACHE-126** — relevant to GDMA+PSRAM workflows (see Memory skill). Mitigated in IDF.
- **ADC-183** — SAR ADC2 DIG_ADC controller's GDMA path is unusable; use ADC1 or RTC ADC.
- **RMT-176** — RMT idle-level bug affects RMT-via-GDMA chains; workaround applied in IDF ≥ v5.0.
- **LCD-239** — LCD module unreliable at certain clock dividers; affects LCD_CAM via GDMA.

### 12. Diagnostics

```c
for (int ch = 0; ch < 5; ch++) {
    printf("CH%d  in.peri_sel=%u out.peri_sel=%u\n", ch,
           GDMA.channel[ch].in.peri_sel.sel,
           GDMA.channel[ch].out.peri_sel.sel);
    printf("      in.link=0x%08lx out.link=0x%08lx\n",
           GDMA.channel[ch].in.link.val, GDMA.channel[ch].out.link.val);
    printf("      IN_INT_RAW=0x%lx OUT_INT_RAW=0x%lx\n",
           GDMA.in_intr[ch].raw.val, GDMA.out_intr[ch].raw.val);
}
```

`peri_sel.sel == 0x3F` → unconnected. Use `gdma_get_channel_id()` to map handles to physical channel numbers.

### 13. SpectraSynq implications

- **I2S0 microphone (RX-only)** consumes **1 RX channel**. ESV11 buffer: 4 descriptors × 1024 B each = 4 KB in `MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL`.
- **FastLED's default RMT mode does NOT use GDMA** — it uses the RMT peripheral's internal symbol-RAM + the threshold IRQ refill. Setting `with_dma=true` would consume 1 TX channel and dramatically reduce IRQ load.
- **SPI as LED driver path** is viable — `spi_bus_initialize(..., SPI_DMA_CH_AUTO)` consumes 1 TX (and optionally 1 RX) channel. Single transfer up to 4092 B per descriptor, multi-descriptor chains up to 32 KB+ on S3.
- **Channel budget.** I2S RX (1) + FastLED-RMT-no-DMA (0) = 1 channel used. Plenty of headroom. Adopting SPI-LED (+1) or RMT-DMA (+1) is comfortable.

---

# FILE 3 — `skills/esp32s3-rmt/SKILL.md`

```
skills/esp32s3-rmt/
├── SKILL.md
├── reference/
│   ├── ws2812b-timing.md     (canonical pulse-encoder bit0/bit1 calc)
│   ├── fastled-flags.md      (FASTLED_RMT_* compile-time matrix)
│   └── irq-budget.md         (frame-time arithmetic for various pixel counts)
```

## SKILL.md

**Purpose.** RMT peripheral silicon model for driving WS2812B addressable LED strips — FastLED's default driver path and the Espressif `led_strip` component's primary backend. Required reading before deciding RMT-vs-SPI-vs-LCD for LED output, sizing IRQ budgets, or debugging colour glitches under Wi-Fi load.

**Cross-references.** RMT memory is a *dedicated peripheral RAM* — see `skills/esp32s3-memory-architecture` for how it does NOT come from main DRAM. RMT-DMA path uses GDMA — see `skills/esp32s3-gdma` for channel budgeting.

### 1. Architecture (TRM §37.1–§37.3.1)

- **8 channels total, asymmetric: 4 TX (channels 0–3) + 4 RX (channels 4–7)**. Unlike the original ESP32 (8 bidirectional channels) — confirmed by `SOC_RMT_TX_CANDIDATES_PER_GROUP=4`, `SOC_RMT_RX_CANDIDATES_PER_GROUP=4`, `SOC_RMT_CHANNELS_PER_GROUP=8`.
- Each channel is an independent FSM driving a shared RAM block via an arbiter.
- The S3 is the **only ESP32 family chip with `SOC_RMT_SUPPORT_DMA=1`** — a single channel may operate in DMA mode via `with_dma=true`.

### 2. Peripheral RAM (TRM §37.3.2.1)

- **Total = 384 × 32-bit words = 1,536 bytes**, partitioned as 48 words per channel × 8 channels by default.
- `SOC_RMT_MEM_WORDS_PER_CHANNEL = 48` (S3) — **smaller than the original ESP32's 64** ([esp-idf#14736](https://github.com/espressif/esp-idf/issues/14736)).
- This RAM is **dedicated peripheral RAM, NOT main DRAM** — it does not show up in `heap_caps_get_free_size()`. Mirrored at non-FIFO and FIFO addresses; access is **word-aligned only**.

### 3. RAM block claiming (TRM §37.3.2.2)

- Each channel's `RMT_MEM_SIZE_CHn` field (in `CHnCONF0_REG` for TX, `chmconf[m].conf0.mem_size_m` for RX) selects 1..N adjacent blocks.
- **Claiming `mem_size>1` consumes the NEXT channel's block.** Channel 0 with `mem_size=4` blocks out channels 1–3.
- ESP-IDF v5: `rmt_tx_channel_config_t.mem_block_symbols` must be a **multiple of 48** on S3 — non-multiples silently round up and consume an extra block ([esp-idf#14736](https://github.com/espressif/esp-idf/issues/14736), [FastLED#1768](https://github.com/FastLED/FastLED/issues/1768)).
- FastLED's `FASTLED_RMT_MEM_BLOCKS=2` default → 96-word channel buffer → cuts max concurrent TX channels from 4 to 2 on S3.

### 4. Clock (TRM §37.3.3)

- Source options (in `RMT_SYS_CONF_REG.RMT_SCLK_SEL`): `APB_CLK` (80 MHz, default), `RC_FAST_CLK` (~17.5 MHz, DFS-tolerant), `XTAL_CLK` (40 MHz, DFS-tolerant). **`REF_TICK` is NOT available on S3** (ESP32-original only).
- Group prescaler: `RMT_SCLK_DIV_NUM` (8-bit) + `_DIV_A`/`_DIV_B` (fractional 6/6).
- Per-channel divider: `RMT_DIV_CNT_CHn` (8-bit, 1..255).
- **WS2812B canonical config:** 10 MHz resolution (100 ns tick) via `resolution_hz = 10_000_000`. At APB 80 MHz this is `clk_div=8`. Used by Espressif's `led_strip` component (`RMT_LED_STRIP_RESOLUTION_HZ`).

### 5. TX modes (TRM §37.3.4)

- **§37.3.4.1 Normal TX:** transmit from word 0; terminator = entry with all four fields zero (`duration0=0` ends). Fires `RMT_CHn_TX_END_INT`.
- **§37.3.4.2 Wrap TX (ping-pong):** `RMT_MEM_TX_WRAP_EN` in `RMT_APB_CONF_REG`. Read pointer wraps; CPU refills via `TX_THR_EVENT_INT`. **This is the path FastLED uses.**
- **§37.3.4.3 TX modulation:** carrier wave (IR remote). Must be DISABLED for LEDs (`carrier_en=0`).
- **§37.3.4.4 Continuous TX:** `RMT_TX_CONTI_MODE_CHn` + `RMT_TX_LOOP_NUM_CHn` (≤1023). `SOC_RMT_SUPPORT_TX_LOOP_AUTO_STOP=1` on S3.
- **§37.3.4.5 Simultaneous TX:** `RMT_TX_SIM_EN_CHn` bits in `RMT_TX_SIM_REG` — channels fire on the same clock edge after all are armed. Driver: `rmt_new_sync_manager()`.

### 6. Pulse encoding for WS2812B

```c
typedef union {
    struct { uint32_t duration0:15, level0:1, duration1:15, level1:1; };
    uint32_t val;
} rmt_symbol_word_t;
```

Each WS2812B bit → 1 symbol. 24 bits/LED (G7..G0, R7..R0, B7..B0, MSB-first) → 24 symbols/LED.

WS2812B timings (datasheet v1.0): T0H=0.40 µs, T0L=0.85 µs, T1H=0.80 µs, T1L=0.45 µs, ±150 ns; reset/latch ≥ 50 µs low.

At 100 ns tick: `bit0 = {level0=1, duration0=4, level1=0, duration1=8}`; `bit1 = {1, 8, 0, 4}`.

End-of-frame reset: either append `{0, 500, 0, 0}` symbol (50 µs low) or rely on `eot_level=0` + idle ≥ 50 µs.

### 7. Configuration update (TRM §37.3.6)

Shadow-register pattern: writing a config field updates a shadow; setting `RMT_REG_UPDATE_CHn` atomically copies shadow→live (hardware auto-clears). HAL: `rmt_ll_tx_update_pointers()`.

### 8. Interrupts (TRM §37.4)

Per-channel events in `RMT_INT_RAW_REG` / `_ST` / `_ENA` / `_CLR`:

| Event | Description |
|---|---|
| `RMT_CHn_TX_END_INT` | TX hit EOF marker |
| `RMT_CHm_RX_END_INT` | RX reception ended |
| `RMT_CHn_ERR_INT` / `RMT_CHm_ERR_INT` | Out-of-bounds / overflow |
| **`RMT_CHn_TX_THR_EVENT_INT`** | **TX consumed `RMT_TX_LIM_CHn` symbols since last threshold event — this is FastLED's refill IRQ** |
| `RMT_CHn_TX_LOOP_CNT_INT` | Loop count reached |
| `RMT_CHm_RX_THR_EVENT_INT` | RX threshold |

Threshold value: `RMT_CH_TX_LIM_CHn_REG[8:0]`. With ping-pong on 96-word buffer, set `TX_LIM=48` so IRQ fires every half-drain.

### 9. Power and clock

- Enable: `SYSTEM_PERIP_CLK_EN0_REG[9] = SYSTEM_RMT_CLK_EN`.
- Reset: `SYSTEM_PERIP_RST_EN0_REG[9] = SYSTEM_RMT_RST`.
- Light-sleep retention: `SOC_RMT_SUPPORT_SLEEP_RETENTION=1`, driver flag `allow_pd`.

### 10. ESP-IDF v5 API (`driver/rmt_tx.h`, `driver/rmt_encoder.h`)

```c
typedef struct {
    gpio_num_t          gpio_num;
    rmt_clock_source_t  clk_src;             /* RMT_CLK_SRC_DEFAULT = APB */
    uint32_t            resolution_hz;       /* e.g. 10_000_000 */
    size_t              mem_block_symbols;   /* MUST be multiple of 48 on S3 */
    size_t              trans_queue_depth;
    int                 intr_priority;
    struct {
        uint32_t invert_out:1, with_dma:1, io_loop_back:1, io_od_mode:1, allow_pd:1;
    } flags;
} rmt_tx_channel_config_t;

esp_err_t rmt_new_tx_channel(const rmt_tx_channel_config_t*, rmt_channel_handle_t*);
esp_err_t rmt_enable(rmt_channel_handle_t);
esp_err_t rmt_disable(rmt_channel_handle_t);
esp_err_t rmt_del_channel(rmt_channel_handle_t);
esp_err_t rmt_new_bytes_encoder(const rmt_bytes_encoder_config_t*, rmt_encoder_handle_t*);
esp_err_t rmt_new_copy_encoder(const rmt_copy_encoder_config_t*, rmt_encoder_handle_t*);
esp_err_t rmt_transmit(rmt_channel_handle_t, rmt_encoder_handle_t,
                       const void *payload, size_t bytes, const rmt_transmit_config_t*);
esp_err_t rmt_tx_register_event_callbacks(rmt_channel_handle_t,
                                          const rmt_tx_event_callbacks_t*, void *user_data);
esp_err_t rmt_tx_wait_all_done(rmt_channel_handle_t, int timeout_ms);
esp_err_t rmt_new_sync_manager(const rmt_sync_manager_config_t*, rmt_sync_manager_handle_t*);
```

`rmt_transmit_config_t`: `loop_count` (0=single, -1=infinite), `eot_level`, `queue_nonblocking`.

Kconfig (cache-safety): `CONFIG_RMT_TX_ISR_CACHE_SAFE=y` keeps ISR running during flash writes. Place custom encoder in `IRAM_ATTR`.

### 11. Espressif `led_strip` component (https://components.espressif.com/components/espressif/led_strip)

```c
led_strip_config_t strip_cfg = {
    .strip_gpio_num = LED_GPIO, .max_leds = 320,
    .led_model = LED_MODEL_WS2812,
    .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
};
led_strip_rmt_config_t rmt_cfg = {
    .clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = 10*1000*1000,
    .mem_block_symbols = 48,                /* multiple of 48 */
    .flags = { .with_dma = false },
};
led_strip_handle_t strip;
led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &strip);
```

Component README explicit: *"if the RMT hardware can't be assisted by DMA, the driver will go into interrupt very frequently … if the interrupt is delayed (e.g. Wi-Fi on same core), the RMT transaction will be corrupted and LEDs display incorrect colours."* Recommends DMA or SPI backend for long strips.

### 12. FastLED specifics

- **RMT5 backend** (`src/platforms/esp/32/rmt_5/strip_rmt.cpp`) wraps `led_strip_new_rmt_device()` for ESP-IDF v5.x.
- **RMT4 backend** (`esp32_idf4_rmt_impl.cpp`) for ESP-IDF v4.x — hand-rolled ping-pong ISR.
- Key compile-time flags:
  - `FASTLED_RMT_MAX_CHANNELS` (default 4 on S3).
  - `FASTLED_RMT_MEM_BLOCKS` (default 2 → 96 symbol words, halves usable channels).
  - `FASTLED_RMT_BUILTIN_DRIVER` (0=FastLED ISR, 1=IDF translator — more memory but more robust under Wi-Fi load).
  - `FASTLED_RMT_USE_DMA` (S3-specific, recent master; **only ONE channel may have DMA on S3**).

Open known issues: FastLED #1768 / #1923 / #2156 (RMT5+S3 channel allocation, DMA mem_block_symbols miscalculation).

### 13. Errata RMT-176

"Idle State Signal Level Might Run into Error in RMT Continuous TX Mode" — all silicon revisions. In continuous TX the post-final-loop idle level is the last looped data level, not the end-marker's. **Workaround: `RMT_IDLE_OUT_EN_CHn=1`. Bypassed in ESP-IDF ≥ v5.0.** Not relevant to single-shot WS2812B writes.

### 14. Footguns

1. **`mem_block_symbols` non-multiple of 48** → silent over-allocation → "no free tx channels" after 2 strips.
2. **Wi-Fi preempts RMT ISR** → TX_THR refill late → repeated/glitched pulses → wrong colours/flicker. Mitigations: pin RMT task to the non-Wi-Fi core; use XTAL clock source (DFS-tolerant); widen `mem_block_symbols`; enable `RMT_TX_ISR_CACHE_SAFE`.
3. **Cache-disable bursts** (flash writes, OTA) → ISR deferred → corrupt frame. Enable `CONFIG_RMT_TX_ISR_CACHE_SAFE=y` and `IRAM_ATTR` the encoder.
4. **Long strips overrun threshold ISR.** At 1.25 µs/bit × 48 = 60 µs between threshold IRQs; any blocking work > 60 µs corrupts the stream. Widen blocks or move to DMA.
5. **50 µs reset pulse missed on back-to-back frames** → entire frame shifts by N pixels. Use `rmt_tx_wait_all_done()` + `esp_rom_delay_us(80)` between frames, or explicit terminator symbol.
6. **GPIO 19/20 conflict with USB-Serial-JTAG.** Avoid for RMT.
7. **Custom ISR not supported on S3 in IDF v4.4** ([esp-idf#11478](https://github.com/espressif/esp-idf/issues/11478)) — must use driver's default ISR.

### 15. Diagnostics

Compile-time:
```c
_Static_assert(SOC_RMT_MEM_WORDS_PER_CHANNEL == 48, "S3 expected");
_Static_assert(SOC_RMT_TX_CANDIDATES_PER_GROUP == 4, "S3 has 4 TX");
_Static_assert(SOC_RMT_SUPPORT_DMA == 1, "S3 supports RMT-DMA");
```

Runtime: enable `CONFIG_RMT_ENABLE_DEBUG_LOG=y` + `esp_log_level_set("rmt", ESP_LOG_VERBOSE)`. Use `vTaskGetRunTimeStats()` after enabling `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y` to see RMT ISR/task CPU consumption. Physical: scope DOUT at ≥ 100 MS/s to confirm timings.

### 16. SpectraSynq IRQ budget (canonical arithmetic)

For 320 pixels per strip:

- **Raw frame time** = 320 × 24 bits × 1.25 µs/bit = **9,600 µs** + 50 µs reset = **9.65 ms**.
- **Max sustainable refresh** for one 320-pixel strip = ~ **103 FPS**. **120 FPS is therefore not achievable with a single serial strip of 320** — you MUST split into 2 parallel strips of 160 LEDs each (4.85 ms/strip → 120 FPS feasible).
- With `mem_block_symbols=96` (FastLED default, `MEM_BLOCKS=2`), `TX_LIM=48`, threshold IRQs per frame per strip = ⌈7680/48⌉ = **160**.
- At 120 FPS × 2 strips: **2 × 160 × 120 = 38,400 IRQs/s**. Plus TX_END (240/s). Each ISR ~3–10 µs → ~38% CPU just on RMT ISRs.
- I2S audio at 44.1 kHz / 512-frame DMA adds only ~86 IRQs/s. Wi-Fi is the dangerous contention.

**Mitigations in preference order:**
1. **Move to SPI driver** (`led_strip_new_spi_device` or FastLED's `clockless_spi_esp32`). One DMA transfer per frame → 2 IRQs total. CPU load drops below 1%. 320 LEDs × 24 bits × 3 SPI bits = 2,880 bytes per frame, easy DMA.
2. **Enable RMT-DMA** (`flags.with_dma=true`). Single channel on S3, similar low-IRQ profile. FastLED master has known bugs ([#2156](https://github.com/FastLED/FastLED/issues/2156)) — the Espressif `led_strip` component path is more reliable.
3. **Stay on RMT non-DMA** but widen `mem_block_symbols` to 144 or 192 (consumes 3–4 hardware blocks per channel, limits to 1–2 channels) to reduce IRQ rate by 2–4×.
4. **ESP32-S3 LCD/Parlio in parallel mode** drives up to 16 strips per DMA — out of scope here but worth flagging (see Tessera, CLEDController_LCD_S3).

---

# FILE 4 — `skills/esp32s3-i2s/SKILL.md`

```
skills/esp32s3-i2s/
├── SKILL.md
├── reference/
│   ├── sph0645-alignment.md   (>>14 derivation from datasheet)
│   ├── clock-derivation.md    (160 MHz PLL → BCLK fractional divider for 32/44.1/48 kHz)
│   └── new-vs-legacy-api.md   (migration table from i2s.h to i2s_std.h)
```

## SKILL.md

**Purpose.** I2S Controller silicon model for the SPH0645LM4H MEMS microphone driving SpectraSynq's FFT/Kuramoto audio pipeline. Required reading before changing sample rates, adjusting DMA buffering, debugging "all zeros from the mic", or migrating between legacy and new I2S drivers.

**Cross-references.** DMA channel allocation: `skills/esp32s3-gdma`. DMA buffer memory placement: `skills/esp32s3-memory-architecture` (`MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL`).

### 1. Architecture (TRM §28.1–§28.4)

- **Two independent controllers I2S0 and I2S1**, each with fully independent TX and RX submodules (separate clocks, separate roles, separate formats, separate GPIOs).
- Supports: TDM Philips, TDM MSB, TDM PCM Short, TDM PCM Long, PDM (I2S0 only).
- BCLK range: 10 kHz – 40 MHz.
- **LCD parallel mode is NOT on I2S** on S3 — it moved to the dedicated LCD_CAM peripheral (TRM Chapter 29). On S3, I2S is purely audio.
- **Dedicated GDMA channel** (no legacy in-peripheral DMA like ESP32 original).

### 2. Reset (TRM §28.7)

Bits in `I2Sn_TX_CONF_REG` / `I2Sn_RX_CONF_REG`: `I2S_TX_RESET`, `I2S_RX_RESET`, `I2S_TX_FIFO_RESET`, `I2S_RX_FIFO_RESET`. Pulse high, then low. GDMA is reset via GDMA channel registers (no `I2S_LC_CONF_REG` on S3).

Canonical sequence (from `i2s_hal_*_reset()`): stop GDMA → set submodule + FIFO reset → clear → reset GDMA channel.

### 3. Master/Slave (TRM §28.8)

`I2S_TX_SLAVE_MOD` / `I2S_RX_SLAVE_MOD` in conf registers. Master drives BCK + WS; slave receives them. For SPH0645LM4H the ESP32-S3 is **RX master**.

Loopback: `I2S_SIG_LOOPBACK` or shared DIN/DOUT GPIO.

### 4. Audio standards (TRM §28.5)

- **§28.5.1 TDM Philips** — data shifted one BCLK after WS edge. **The format SPH0645LM4H uses** (datasheet: "Data Format is I2S, 24-bit, 2's compliment, MSB first"). API macro: `I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG`.
- **§28.5.2 TDM MSB** — same but no shift.
- **§28.5.3 TDM PCM Short** — single-BCLK WS pulse.
- **§28.5.4 PDM** — 1-bit oversampled. **NOT used by SPH0645LM4H** (which is multi-bit I2S, not PDM).

### 5. Clock (TRM §28.6)

- **Source** (`I2Sn_TX_CLKM_CONF_REG.I2S_TX_CLK_SEL`): `PLL_F160M` (default; `I2S_CLK_SRC_PLL_160M`/`_DEFAULT`), `XTAL_CLK` (40 MHz, `I2S_CLK_SRC_XTAL`), `EXTERNAL` MCLK in (S3 HW v2 feature).
- **APLL is NOT available to I2S on ESP32-S3** — `esp-adf#907` confirms runtime warning *"APLL not supported on current chip, use I2S_CLK_D2CLK as default"*. Any code setting `use_apll=true` on S3 is a no-op.
- **MCLK fractional divider** (N + B/A encoding in `I2Sn_TX_CLKM_DIV_CONF_REG` with `DIV_NUM` integer, `DIV_X`/`Y`/`Z`/`YN1` encoding the fraction).
- **BCLK divider**: `I2S_TX_BCK_DIV_NUM` (6-bit, ≥ 2). In slave role, ≥ 8 recommended.
- **WS period** = `slot_num × slot_bit_width` BCLK ticks.

**SPH0645LM4H @ 44.1 kHz, 32-bit slot, 2-slot frame:**
- BCLK = 44,100 × 32 × 2 = **2.8224 MHz** ✓ (mic spec 1.024–4.096 MHz)
- MCLK at 256 × Fs = **11.2896 MHz**
- 160 MHz / 11.2896 ≈ 14.174 → fractional divider N=14, B/A≈22/127. **Sample-rate error < 0.1 %** — negligible for FFT visualisation.

**SPH0645LM4H is single-channel**, occupying ONE slot of a 2-slot stereo frame; the other slot is tri-stated.

### 6. Data format (TRM §28.9–§28.10)

Per-direction controls in `I2Sn_TX_CONF1_REG` / `I2Sn_RX_CONF1_REG` and TDM ctrl registers:
- Valid data bit width (§28.9.1.1, §28.10.2.2): 8/16/24/32 bit.
- Slot bit width (§28.9.1.4, §28.10.2.3): for SPH0645 use **32**.
- Bit order (§28.9.1.5, §28.10.2.1): MSB-first (datasheet).
- Endian (§28.9.1.2, §28.10.2.4).
- A-law/µ-law bypass (§28.9.1.3, §28.10.2.5): bypass for linear PCM.
- TDM channel control (§28.9.2.1, §28.10.1.1): `I2S_RX_TDM_TOT_CHAN_NUM=2`, enable slot 0 (left) or 1 (right).

### 7. Interrupts (TRM §28.12)

`I2Sn_INT_*_REG` (`RAW`/`ST`/`ENA`/`CLR`):

| Bit | Name |
|---|---|
| 0 | `I2S_RX_DONE_INT` |
| 1 | `I2S_TX_DONE_INT` |
| 2 | `I2S_RX_HUNG_INT` (BCLK absent in slave mode, etc.) |
| 3 | `I2S_TX_HUNG_INT` |

**FIFO over/underflow and DMA EOF are reported by the GDMA channel's interrupt registers**, NOT the I2S peripheral. The driver surfaces them as `on_recv`, `on_recv_q_ovf`, `on_sent`, `on_send_q_ovf` callbacks.

### 8. ESP-IDF API

**New driver (recommended, v5.x):** `driver/i2s_std.h`, `driver/i2s_pdm.h`, `driver/i2s_tdm.h`.

```c
typedef struct {
    i2s_port_t id;             /* I2S_NUM_0 / I2S_NUM_1 / I2S_NUM_AUTO */
    i2s_role_t role;           /* I2S_ROLE_MASTER / I2S_ROLE_SLAVE */
    uint32_t   dma_desc_num;   /* 2..511, default 6 */
    uint32_t   dma_frame_num;  /* ≥ 8, default 240 */
    bool       auto_clear;     /* TX-only */
    int        intr_priority;
} i2s_chan_config_t;

typedef struct {
    i2s_std_clk_config_t  clk_cfg;
    i2s_std_slot_config_t slot_cfg;
    i2s_std_gpio_config_t gpio_cfg;
} i2s_std_config_t;

esp_err_t i2s_new_channel(const i2s_chan_config_t*, i2s_chan_handle_t *tx, i2s_chan_handle_t *rx);
esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t, const i2s_std_config_t*);
esp_err_t i2s_channel_enable(i2s_chan_handle_t);
esp_err_t i2s_channel_disable(i2s_chan_handle_t);
esp_err_t i2s_channel_read(i2s_chan_handle_t, void *dst, size_t size, size_t *got, uint32_t to_ms);
esp_err_t i2s_channel_register_event_callback(i2s_chan_handle_t, const i2s_event_callbacks_t*, void*);
esp_err_t i2s_del_channel(i2s_chan_handle_t);
```

Helper macros: `I2S_STD_CLK_DEFAULT_CONFIG(rate)`, `I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bits, mode)`, `I2S_STD_MSB_SLOT_DEFAULT_CONFIG`, `I2S_STD_PCM_SLOT_DEFAULT_CONFIG`.

**DMA buffer sizing:** `dma_frame_num × slot_num × slot_bit_width/8` bytes per descriptor × `dma_desc_num` descriptors, all in `MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL`. SpectraSynq ESV11: `dma_desc_num=4`, `dma_frame_num=512`, mono → 4 × 512 × 1 × 4 = **8 KB total** (or 4 KB if mono-mask path halves the read).

**Legacy driver** (`driver/i2s.h`, **deprecated in v5.x**): `i2s_driver_install`, `i2s_set_pin`, `i2s_read`, `i2s_write`. Headers mutually exclusive with new driver.

**Arduino-ESP32:** thin `I2SClass` in `I2S.h`; for v3.0+ (IDF 5.x backend) prefer direct `i2s_std.h` use or the new `ESP_I2S.h` wrapper.

### 9. SPH0645LM4H specifics (Knowles datasheet Rev C)

- **I2S, NOT PDM.** "Operates as an I2S slave."
- **Data:** 24-bit two's complement, MSB-first, left-justified in slot. Of those 24 bits, only the **upper 18 are valid** ("Data Precision is 18 bits, unused bits are zeros").
- **Bit alignment in 32-bit slot (the canonical footgun):** With Philips mode + 32-bit slot, the sign bit lands in bit 31 of the `int32_t` DMA sample, valid data in bits 31..14 (18 bits MSB-aligned), zeros in bits 13..0. **Correct right-shift to sign-extend to 18-bit = `>> 14`**.
  - Community confusion (`>>11`, `>>12`, `>>16`) stems from slot-width or mode mismatches. Standardise on **32-bit Philips + `>>14`** (atomic14 / Knowles datasheet).
  - Note the bottom 1–2 of those 18 valid bits have known DC-offset / undefined behaviour; effectively the mic is ~16-bit usable.
- **DC offset is significant.** First-order HPF (`y[n] = x[n] - x[n-1] + 0.995·y[n-1]`) mandatory before FFT.
- **L/R SELECT pin** (datasheet Rev C):
  - **SEL = GND** → mic drives data when WS=LOW → occupies **LEFT slot**.
  - **SEL = Vdd** → occupies **RIGHT slot**.
  - **Floating SEL = garbage.** Always tie hard.
- **BCLK range Rev C: 1.024–4.096 MHz** → sample rates 16–64 kHz. Spec-compliant performance ≥ 32 kHz.
- Hardware: 100 kΩ pulldown on DATA recommended, 100 nF decoupling on Vdd close to mic.

### 10. Canonical SpectraSynq config

```c
i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
chan_cfg.dma_desc_num  = 4;
chan_cfg.dma_frame_num = 512;
chan_cfg.auto_clear    = false;

i2s_chan_handle_t rx;
ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx));

i2s_std_config_t cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(44100),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                    I2S_DATA_BIT_WIDTH_32BIT,   /* 32-bit slot */
                    I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
        .mclk = I2S_GPIO_UNUSED,
        .bclk = GPIO_NUM_x,
        .ws   = GPIO_NUM_y,
        .dout = I2S_GPIO_UNUSED,
        .din  = GPIO_NUM_z,
    },
};
cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;     /* SEL=GND */

ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx, &cfg));
ESP_ERROR_CHECK(i2s_channel_enable(rx));

int32_t buf[512]; size_t got;
i2s_channel_read(rx, buf, sizeof(buf), &got, pdMS_TO_TICKS(100));
for (int i = 0; i < got/4; i++) f32[i] = (float)(buf[i] >> 14);   /* canonical alignment */
```

### 11. Footguns

1. **Shift confusion `>>11/12/14/16`.** Canonical: 32-bit Philips + `>>14`. Anything else is wrong slot width or wrong mode.
2. **DC offset** → FFT bin 0 dominates everything. HPF before FFT.
3. **Floating SEL** → garbage zeros / intermittent.
4. **`dma_frame_num` too small** → `RX_Q_OVF` event. 512 frames at 44.1 kHz = 11.6 ms cadence — comfortable for FFT 1024-sample frames.
5. **Short `i2s_channel_read` timeout** → fewer bytes than requested. Use large timeout and loop.
6. **Header mixing.** `i2s.h` and `i2s_std.h` are mutually exclusive in one TU.
7. **Sample-rate inexactness.** 44.1 kHz on PLL_F160M without APLL has ~0.07% error; tolerable for visualisation.
8. **`use_apll=true` silently ignored** on S3 (no APLL on I2S).
9. **Slot-mask vs SEL pin mismatch.** `slot_mask=I2S_STD_SLOT_LEFT` with `SEL=Vdd` → all zeros (driver listens to slot mic isn't transmitting on).
10. **24-bit slot with default `mclk_multiple=256`** gives non-integer BCLK ratio — use 384 or multiple of 3.

### 12. Errata

**No I2S-specific silicon errata** on ESP32-S3 (chip rev v0.0–v0.2 + later). Past confusions:
- "WS not generated for TDM9..16" — IDF v4.4 driver bug ([#9645](https://github.com/espressif/esp-idf/issues/9645)), fixed.
- "TDM slave WS sync on reboot" — driver issue ([#9513](https://github.com/espressif/esp-idf/issues/9513)), not silicon.

### 13. Diagnostics

**Sample-validity sweep:**
```c
int32_t min=INT32_MAX, max=INT32_MIN, sum_abs=0;
for (int i = 0; i < n; i++) {
    int32_t s = buf[i] >> 14;
    if (s<min) min=s; if (s>max) max=s;
    sum_abs += abs(s);
}
ESP_LOGI("MIC","min=%ld max=%ld avg|x|=%ld", min, max, sum_abs/n);
```
Healthy: `max-min ≫ 100`, `avg|x| > 10`. All zeros → SEL floating or wrong slot_mask. Constant → BCLK/WS swapped.

**EOF rate sanity check:**
```c
static volatile uint32_t rx_count = 0;
static bool IRAM_ATTR on_recv(i2s_chan_handle_t h, i2s_event_data_t *e, void *c) {
    rx_count++; return false;
}
i2s_event_callbacks_t cbs = { .on_recv = on_recv };
i2s_channel_register_event_callback(rx, &cbs, NULL);
```
Expected = `sample_rate / dma_frame_num` = 44100/512 ≈ 86 Hz. Significantly fewer → BCLK absent.

**Hardware probes** (logic analyser ≥ 25 MS/s): BCLK clean 2.8224 MHz square; WS 44.1 kHz 50% duty; DOUT bursts during one half of WS, tri-state during the other; data MSB-first on falling BCLK edge.

### 14. SpectraSynq pipeline

Mic → I2S RX DMA (32-bit slot, Philips, LEFT mono) → `int32_t buf` → `>>14` → HPF → Hann window → FFT (PipelineCore consumer) → mel bands → Kuramoto oscillator engine → LED frame.

**44.1 kHz vs 32 kHz trade-off** (the `k1v2_32khz` variant):
- 32 kHz: FFT bin width at N=1024 = **31.25 Hz** (slightly finer LF resolution), Nyquist 16 kHz (loses 16–22 kHz, irrelevant on consumer mic with HF roll-off), **~27% less CPU**.
- 44.1 kHz: bin width 43 Hz, full audible bandwidth, higher CPU cost.
- For music reactivity: **32 kHz is the better engineering choice** — SPH0645's specified bandwidth doesn't usefully extend past ~15 kHz anyway.

---

## Cross-skill loading guidance for the engineering agent

- **LED work** → load `esp32s3-rmt` + `esp32s3-memory-architecture`.
- **Audio work** → load `esp32s3-i2s` + `esp32s3-memory-architecture` + (if changing DMA strategy) `esp32s3-gdma`.
- **Heap diagnostic / OOM debug** → load `esp32s3-memory-architecture` only.
- **DMA buffer placement / PSRAM payload debugging** → load `esp32s3-gdma` + `esp32s3-memory-architecture`.
- **New peripheral bring-up (SPI/Camera/AES)** → load `esp32s3-gdma` + `esp32s3-memory-architecture`.

## The doctrine encoded

APIs without silicon truth is like living a lie. You think you know, until shit breaks and you get caught with your pants down and it's cold outside. TRM Chapter 4 + Chapter 15 explains why `MALLOC_CAP_INTERNAL` is five disjoint regions, not one heap. TRM Chapter 3 + Figure 4.3-2 explains which buffer can DMA where. TRM Chapter 37 explains why 320 LEDs at 120 FPS over one RMT channel will burn 38% of one core on IRQs. TRM Chapter 28 explains why SPH0645LM4H samples need `>>14` and a high-pass before they're useful. Read the TRM. Then read the API. Never the other way round.