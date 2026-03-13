# Pre-Landing Review Checklist — Embedded Firmware (ESP32 / FreeRTOS)

## Instructions

Review the `git diff origin/main` output for the issues listed below. Be specific — cite `file:line` and suggest fixes. Skip anything that's fine. Only flag real problems.

**Two-pass review:**
- **Pass 1 (CRITICAL):** Run all CRITICAL categories first. These block `/ship`.
- **Pass 2 (INFORMATIONAL):** Run all remaining categories. These are included in the PR body but do not block.

**Output format:**

```
Pre-Landing Review: N issues (X critical, Y informational)

**CRITICAL** (blocking /ship):
- [file:line] Problem description
  Fix: suggested fix

**Issues** (non-blocking):
- [file:line] Problem description
  Fix: suggested fix
```

If no issues found: `Pre-Landing Review: No issues found.`

Be terse. For each issue: one line describing the problem, one line with the fix. No preamble, no summaries, no "looks good overall."

---

## Review Categories

### Pass 1 — CRITICAL

#### GPIO & Pin Contention
- Two subsystems claiming the same GPIO (e.g., I2S BCLK vs AMOLED_RST on GPIO 38). Check `chip_*.h`, `platformio.ini` overrides, and any `gpio_set_direction()` calls. If a pin is defined in the chip config AND toggled elsewhere, flag it.
- `gpio_set_direction()` or `gpio_set_level()` called on a pin owned by a peripheral driver (SPI, I2S, UART, I2C). Once a pin is assigned to a peripheral via bus config, direct GPIO manipulation is undefined behaviour.
- Missing `gpio_reset_pin()` before reassigning a pin to a different function.
- Pin numbers hardcoded as magic integers instead of referencing `chip::gpio::` constants.

#### FreeRTOS Concurrency & Race Conditions
- Shared state accessed from multiple tasks/cores without mutex, atomic, or single-writer guarantee. Check any global or static mutable variable accessed in `onTick()`, `onMessage()`, or ISR contexts.
- `xSemaphoreTake()` without timeout or with `portMAX_DELAY` in a time-critical path (audio pipeline, render loop). Will deadlock if the holder crashes.
- Priority inversion: low-priority task holds a mutex needed by a high-priority task. Use `xSemaphoreCreateMutex()` (which supports priority inheritance) not `xSemaphoreCreateBinary()` for cross-priority locking.
- Task notification or queue send from ISR without the `FromISR` variant (`xQueueSendFromISR`, `xTaskNotifyFromISR`). Will corrupt the scheduler.
- `vTaskDelay()` in an ISR context. ISRs must not block.
- Actor `onTick()` or `onMessage()` performing blocking I2C/SPI transactions that could stall the pinned core's task loop.

#### Memory Safety & DMA
- Stack-allocated buffer passed to DMA transfer (`spi_device_polling_transmit`, `i2s_write`). DMA requires the buffer to persist until transfer completes. Use heap allocation or static buffers.
- DMA from flash-resident `static const` data without `SPI_TRANS_USE_TXDATA` for small payloads or `heap_caps_malloc(MALLOC_CAP_DMA)` for large ones. ESP32 DMA cannot read directly from flash on all bus types.
- Buffer size mismatch: pixel buffer allocated for X pixels but `pushPixels()` called with Y > X. Check `MAX_TRANSFER_BYTES` vs actual transfer sizes.
- `memcpy` into `SPI_TRANS_USE_TXDATA` (`tx_data[4]`) with `len > 4`. Overflows the 4-byte inline buffer.
- `malloc` / `new` without checking for `nullptr` return on a memory-constrained target.
- PSRAM allocation (`heap_caps_malloc(MALLOC_CAP_SPIRAM)`) for time-critical DMA buffers — PSRAM is 4-8x slower than internal SRAM.

#### I2C / SPI Bus Safety
- I2C transaction (`Wire.beginTransmission` / `Wire.endTransmission`) without checking return code. I2C peripherals (CH422G, RTC, touch controller) can NACK or hang the bus.
- Multiple tasks accessing the same I2C bus without a mutex. `Wire` is not thread-safe. If the display driver and touch driver share SDA/SCL, they need serialisation.
- SPI bus initialised with wrong host (SPI2_HOST vs SPI3_HOST) for the pin mapping. ESP32-S3 SPI2 and SPI3 have specific pin routing constraints.
- `spi_bus_add_device()` without matching `spi_bus_remove_device()` in destructor/error path. Leaks the SPI device slot (max 3 per host).
- Clock speed exceeding peripheral spec. Check `SPI_FREQ_HZ` against the display datasheet max and I2C clock against 400kHz (CH422G) or 100kHz (slower peripherals).

#### Init Sequence & Power Dependencies
- Peripheral init attempted before its power supply is enabled. AMOLED panel requires CH422G EXIO1 (AMOLED_EN) HIGH before QSPI commands will be acknowledged. I2S codec requires power rail stable before configuration.
- Init ordering violation: subsystem B depends on subsystem A being initialised first, but `ActorSystem` starts them in the wrong order or in parallel.
- Hardware reset pin toggled for a peripheral that shares the pin with another active subsystem (GPIO contention — see above).
- Missing delay after power-on or reset. Displays typically need 50-150ms after reset release before accepting commands. Check datasheet minimums.
- `Wire.begin()` called multiple times with different SDA/SCL pins — reinitialises the bus and disrupts any in-flight transactions from other tasks.

### Pass 2 — INFORMATIONAL

#### Actor Lifecycle & Architecture
- Actor `start()` failure treated as fatal when it should be non-fatal (or vice versa). Display failure should not prevent audio playback.
- Actor `stop()` not called in reverse dependency order in `ActorSystem::shutdown()`. Later actors may depend on earlier ones still running during teardown.
- `getBufferCopy()` or similar cross-actor data access without checking if the source actor is running. Returns uninitialised or stale data.
- Actor pinned to wrong core. Audio pipeline should be on core 0 (or whichever is designated). Display/UI on the other. Check `xTaskCreatePinnedToCore` core parameter.
- Stats counting (`activeActors++`) not updated when a new actor type is added to `ActorSystem`.

#### ControlBusFrame & Audio Contract
- Field name mismatch between `ControlBusFrame` struct definition and usage site. Check against `ControlBus.h` — the canonical field names are: `rms`, `flux`, `fast_rms`, `fast_flux`, `liveliness`, `bands[]`, `chroma[]`, `tempoBpm`, `tempoBeatTick`, `tempoBeatStrength`, `tempoLocked`, `tempoConfidence`, `isSilent`.
- Accessing `ControlBusFrame` fields that may not be populated yet (e.g., `tempoBpm` before the tempo estimator has locked). Check `tempoLocked` before trusting `tempoBpm`.
- RGB565 byte-swap errors. ESP32 SPI sends MSB first; RGB565 pixel data must be big-endian on the wire. If `CRGB` → RGB565 conversion doesn't byte-swap, colours will be wrong.

#### PlatformIO Build Configuration
- `build_unflags` stripping a flag that's needed (e.g., `-std=gnu++14` or `-std=gnu++17`). If `build_unflags` removes `-D FLAG=VALUE`, verify it doesn't also match and strip other flags with overlapping prefixes.
- Macro redefinition without `build_unflags` to strip the inherited value first. Causes compiler warnings that obscure real errors.
- `build_src_filter` excluding a file that's `#include`d elsewhere. Causes linker errors, not compiler errors — harder to diagnose.
- `extends` chain not carrying through a required flag. The child env's `build_flags` replaces (not appends to) the parent's unless `${parent.build_flags}` is explicitly referenced.
- `-D` flag in `platformio.ini` overriding a value defined in a header, or vice versa, with no comment explaining which takes precedence.

#### Display Layout & Rendering
- Panel dimensions hardcoded instead of using `chip::display::` constants. If the display resolution or rotation changes, hardcoded values break silently.
- `setWindow()` called with coordinates outside the physical display bounds. No hardware clipping — writes to invalid GRAM addresses corrupt adjacent regions.
- Pixel buffer not cleared between frames. Stale data from the previous frame bleeds through in regions not explicitly redrawn.
- Brightness set to 0 at end of init but never set to target value, or set to max when it should be user-configurable.

#### Volatile & Compiler Optimisation
- Shared variable between ISR and task not declared `volatile`. Compiler may cache the value in a register and never re-read it.
- `volatile` on a struct member when the entire struct should be atomic or mutex-protected. `volatile` doesn't provide atomicity — it only prevents caching.
- Optimisation-sensitive timing loop without `volatile` or compiler barrier. `ets_delay_us()` or `vTaskDelay()` is preferable to busy-wait.

#### Dead Code & Consistency
- `#if FEATURE_*` guard missing on code that references a feature-gated type or function. Compiles fine with the feature enabled, fails without it.
- Comment describing old behaviour after the code changed (e.g., "SWRESET used" when `hardwareReset()` was restored).
- `#include` added but the included header's types/functions never used in the file.
- Function declared in header but body removed from implementation (linker error if called).

#### Stack & Heap
- Task stack size too small for the call depth. FreeRTOS default is often 4096 bytes. Deep call chains (SPI transaction → DMA setup → callback) can overflow. Use `uxTaskGetStackHighWaterMark()` to verify.
- Large local arrays (>512 bytes) on the stack in a task with a tight stack budget. Move to heap or static.
- `String` (Arduino) or `std::string` concatenation in a loop — repeated heap allocation and fragmentation on a constrained target.

---

## Gate Classification

```
CRITICAL (blocks /ship):                 INFORMATIONAL (in PR body):
├─ GPIO & Pin Contention                 ├─ Actor Lifecycle & Architecture
├─ FreeRTOS Concurrency & Race Conds     ├─ ControlBusFrame & Audio Contract
├─ Memory Safety & DMA                   ├─ PlatformIO Build Configuration
├─ I2C / SPI Bus Safety                  ├─ Display Layout & Rendering
└─ Init Sequence & Power Dependencies    ├─ Volatile & Compiler Optimisation
                                          ├─ Dead Code & Consistency
                                          └─ Stack & Heap
```

---

## Suppressions — DO NOT flag these

- Pin definitions in `chip_*.h` that duplicate a value for documentation clarity (e.g., `AMOLED_RST = 21` alongside `I2S_BCLK = 38` when they're on different GPIOs)
- `vTaskDelay(pdMS_TO_TICKS(N))` where N is a small empirical tuning value — these change during bring-up
- ESP_LOG level differences between environments (debug vs release) — controlled by `CORE_DEBUG_LEVEL`
- `#pragma GCC diagnostic` suppressions for known vendor SDK warnings
- Minor inconsistency in comment style or whitespace formatting
- `static_cast<gpio_num_t>()` wrapping — this is required by ESP-IDF's type system, not unnecessary verbosity
- Feature-flag guards (`#if FEATURE_*`) that look redundant at the call site but protect against compilation without the feature
- `ESP_LOGI` / `ESP_LOGW` in init paths — these are diagnostic, not performance-critical
- Test/stub code guarded by `#ifdef NATIVE_BUILD`
- ANYTHING already addressed in the diff you're reviewing — read the FULL diff before commenting
