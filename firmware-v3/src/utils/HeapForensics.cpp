/**
 * @file HeapForensics.cpp
 * @brief Diagnostic heap dump emitter (SSA-W4 — non-production).
 *
 * Master gate
 * -----------
 * `dump()` and `dumpScalar()` are no-ops unless `LW_HEAP_FORENSICS_ENABLE=1`
 * is set on the build env. Production envs leave it undefined so all 11 call
 * sites (WebServer boot / first-client / 60 s periodic / scalar 1 Hz / shed
 * transitions, UdpStreamer ENOMEM, SerialCLI `dbg memory verbose`) compile
 * but return immediately — no serial output, no `heap_caps_*` mutex traffic.
 * Diagnostic / soak envs opt in with `-D LW_HEAP_FORENSICS_ENABLE=1`.
 *
 * Implementation notes (when enabled)
 * -----------------------------------
 * Output goes straight to `Serial.printf` and `heap_caps_print_heap_info`
 * (which writes to ESP-IDF's `stdout`, normally the same UART as Arduino's
 * `Serial`). Both paths bypass the LW_LOG_* macros so the dumps remain
 * visible at any global log level.
 *
 * No allocations. Every field is read via `heap_caps_get_*` calls which
 * acquire the heap mutex; this is acceptable because dumps fire on:
 *   - boot (once),
 *   - shed-latch transitions (sparse — measured in minutes),
 *   - once per minute under sustained load,
 *   - serial command (operator-triggered, off render hot path),
 *   - UDP ENOMEM failure (already an error path).
 *
 * They DO NOT fire from `render()`, AudioActor, or any high-frequency loop.
 * The previously banned `heap_caps_get_largest_free_block()` mutex contention
 * (feedback_no_heap_scans_in_high_freq_paths.md) does not apply at this
 * cadence.
 */

// Default master gate to OFF if the build env hasn't set it. Diagnostic /
// soak builds re-enable with `-D LW_HEAP_FORENSICS_ENABLE=1`.
#ifndef LW_HEAP_FORENSICS_ENABLE
#define LW_HEAP_FORENSICS_ENABLE 0
#endif

#include "HeapForensics.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

namespace lightwaveos {
namespace diagnostics {

namespace {

inline uint32_t safeMillis() {
#if defined(ARDUINO)
    return millis();
#else
    return 0;
#endif
}

} // namespace

const char* reasonTag(HeapDumpReason reason) {
    switch (reason) {
        case HeapDumpReason::Boot:               return "boot";
        case HeapDumpReason::ShedLatched:        return "shed.enable";
        case HeapDumpReason::ShedReleased:       return "shed.release";
        case HeapDumpReason::ShedForceCleared:   return "shed.force";
        case HeapDumpReason::Periodic:           return "periodic";
        case HeapDumpReason::PeriodicScalar:     return "periodic.scalar";
        case HeapDumpReason::OnDemand:           return "ondemand";
        case HeapDumpReason::UdpEnomem:          return "udp.enomem";
        case HeapDumpReason::FirstHttpRequest:   return "http.first";
    }
    return "unknown";
}

#if LW_HEAP_FORENSICS_ENABLE
namespace {

// Emits Blocks 1-3 (wall-clock anchor + internal SRAM scalars + PSRAM
// scalars). Shared between dump() and dumpScalar() so the scalar log-format
// contract stays in one place. Skips Block 4 (verbose IDF histograms).
void emitScalarBlocks(const char* tag, bool shedActive, uint32_t shedLatchedMs) {
    const uint32_t nowMs = safeMillis();
    const uint32_t uptimeS = nowMs / 1000U;

    // --- Block 1: caller-supplied shed state + wall-clock anchor ----------
    Serial.printf(
        "[HEAP-FORENSICS] %s millis=%lu uptime_s=%lu shed=%d latched_ms=%lu\n",
        tag,
        static_cast<unsigned long>(nowMs),
        static_cast<unsigned long>(uptimeS),
        shedActive ? 1 : 0,
        static_cast<unsigned long>(shedLatchedMs));

    // --- Block 2a: region 1 — the DMA-capable internal DRAM heap ----------
    //
    // Captain's 2026-05-19 diagnostic correction: the previous aggregate
    // `internal.largest` reading was misleading because it was the cross-
    // region max across regions 1+2+3, which let a small high-water bucket
    // in region 2 or 3 mask region 1 fragmentation. Region 1 is the live
    // app heap (0x3fcb4af0, ~216 KB at boot on K1 V2) where lwIP, AsyncTCP,
    // AsyncWebSocket and effect statics actually allocate.
    //
    // IDF 4.4.7 does NOT expose per-region enumeration (no
    // `heap_caps_get_info_by_addr`, no `heap_caps_walk_registered_heaps`,
    // no public `multi_heap_handle_t` registry) — we use cap-mask isolation.
    // On ESP32-S3 the primary DRAM region carries
    // MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT | MALLOC_CAP_DEFAULT
    // per `soc_memory_layout.c`, while the smaller fragmented regions 2+3
    // (typically IRAM-stack carve-outs / DIRAM aliases) commonly carry
    // MALLOC_CAP_INTERNAL but NOT MALLOC_CAP_DMA. The verbose Block 4 dumps
    // remain the cross-check — operators compare `region1.free` against the
    // per-region totals printed by `heap_caps_print_heap_info`.
    constexpr uint32_t kRegion1Caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
    multi_heap_info_t region1Info = {};
    heap_caps_get_info(&region1Info, kRegion1Caps);
    const size_t region1Free        = heap_caps_get_free_size(kRegion1Caps);
    const size_t region1Largest     = heap_caps_get_largest_free_block(kRegion1Caps);
    const size_t region1MinFree     = heap_caps_get_minimum_free_size(kRegion1Caps);

    Serial.printf(
        "[HEAP-FORENSICS] %s region1.free=%u region1.largest=%u region1.min_free=%u "
        "region1.alloc_blocks=%u region1.free_blocks=%u\n",
        tag,
        static_cast<unsigned>(region1Free),
        static_cast<unsigned>(region1Largest),
        static_cast<unsigned>(region1MinFree),
        static_cast<unsigned>(region1Info.allocated_blocks),
        static_cast<unsigned>(region1Info.free_blocks));

    // --- Block 2b: aggregate internal (cross-region max) ------------------
    //
    // Preserved as cross-region context only. The `aggregate_internal.largest`
    // field is the max free block across regions 1+2+3 and was historically
    // the primary survival scalar — Captain's diagnostic correction is that
    // this is NOT a reliable region-1 proxy. Keep it for backwards-comparable
    // logs and to surface region-2/3 high-water masking when it occurs.
    const size_t aggInternalFree    = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t aggInternalLargest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);

    Serial.printf(
        "[HEAP-FORENSICS] %s aggregate_internal.free=%u aggregate_internal.largest=%u\n",
        tag,
        static_cast<unsigned>(aggInternalFree),
        static_cast<unsigned>(aggInternalLargest));

    // --- Block 2c: RTC_FAST (region 4 — 0x600fe000, ~8 KB) ----------------
    //
    // Captain's correction: 0x600fe000 is RTC_FAST (RTCRAM), NOT RTC slow.
    // MALLOC_CAP_RTCRAM is documented as "Memory must be in RTC fast memory"
    // in esp_heap_caps.h. On ESP32-S3 there is only one RTC fast region at
    // this address, so this isolation is unambiguous.
    const size_t rtcFastFree    = heap_caps_get_free_size(MALLOC_CAP_RTCRAM);
    const size_t rtcFastLargest = heap_caps_get_largest_free_block(MALLOC_CAP_RTCRAM);

    Serial.printf(
        "[HEAP-FORENSICS] %s rtc_fast.free=%u rtc_fast.largest=%u\n",
        tag,
        static_cast<unsigned>(rtcFastFree),
        static_cast<unsigned>(rtcFastLargest));

    // --- Block 3: PSRAM scalar fields (for comparison) --------------------
    multi_heap_info_t psramInfo = {};
    heap_caps_get_info(&psramInfo, MALLOC_CAP_SPIRAM);
    const size_t psramTotalFree    = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t psramLargestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    const size_t psramMinFree      = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);

    Serial.printf(
        "[HEAP-FORENSICS] %s psram.total=%u psram.largest=%u psram.min_free=%u "
        "psram.alloc_blocks=%u psram.free_blocks=%u\n",
        tag,
        static_cast<unsigned>(psramTotalFree),
        static_cast<unsigned>(psramLargestBlock),
        static_cast<unsigned>(psramMinFree),
        static_cast<unsigned>(psramInfo.allocated_blocks),
        static_cast<unsigned>(psramInfo.free_blocks));
}

} // namespace
#endif  // LW_HEAP_FORENSICS_ENABLE

void dumpScalar(HeapDumpReason reason, bool shedActive, uint32_t shedLatchedMs) {
#if !LW_HEAP_FORENSICS_ENABLE
    // Master-gate OFF: skip every heap_caps_* call and serial emit. Set
    // `-D LW_HEAP_FORENSICS_ENABLE=1` on the build env to re-enable.
    (void)reason; (void)shedActive; (void)shedLatchedMs;
    return;
#else
    // Scalar-only emission for the 1 Hz periodic tick (Phase 0.5).
    // Skips the verbose IDF histogram payload — at 1 Hz that would flood
    // serial bandwidth and itself add heap pressure via UART tx buffering.
    // Edges (boot, shed transitions, UDP ENOMEM, on-demand) still get the
    // full dump() with histograms.
    emitScalarBlocks(reasonTag(reason), shedActive, shedLatchedMs);
#endif
}

void dump(HeapDumpReason reason, bool shedActive, uint32_t shedLatchedMs) {
#if !LW_HEAP_FORENSICS_ENABLE
    (void)reason; (void)shedActive; (void)shedLatchedMs;
    return;
#else
    const char* tag = reasonTag(reason);

    emitScalarBlocks(tag, shedActive, shedLatchedMs);

    // --- Block 4: verbose IDF block histograms ----------------------------
    // Each `heap_caps_print_heap_info` call walks the relevant heap and
    // prints a human-readable summary including per-region totals and the
    // block-size histogram bucket counts. The output does NOT have our
    // prefix tag, so we bracket it with BEGIN/END markers for the grep.
    Serial.printf("[HEAP-FORENSICS] %s heap_caps_print MALLOC_CAP_8BIT BEGIN\n", tag);
    heap_caps_print_heap_info(MALLOC_CAP_8BIT);
    Serial.printf("[HEAP-FORENSICS] %s heap_caps_print MALLOC_CAP_8BIT END\n", tag);

    Serial.printf("[HEAP-FORENSICS] %s heap_caps_print MALLOC_CAP_INTERNAL BEGIN\n", tag);
    heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);
    Serial.printf("[HEAP-FORENSICS] %s heap_caps_print MALLOC_CAP_INTERNAL END\n", tag);

    Serial.printf("[HEAP-FORENSICS] %s heap_caps_print MALLOC_CAP_SPIRAM BEGIN\n", tag);
    heap_caps_print_heap_info(MALLOC_CAP_SPIRAM);
    Serial.printf("[HEAP-FORENSICS] %s heap_caps_print MALLOC_CAP_SPIRAM END\n", tag);

    // RTC_FAST verbose dump — cross-check for the scalar `rtc_fast.*` block.
    // Confirms region 4 (0x600fe000, ~8 KB) is being read correctly via the
    // RTCRAM cap mask.
    Serial.printf("[HEAP-FORENSICS] %s heap_caps_print MALLOC_CAP_RTCRAM BEGIN\n", tag);
    heap_caps_print_heap_info(MALLOC_CAP_RTCRAM);
    Serial.printf("[HEAP-FORENSICS] %s heap_caps_print MALLOC_CAP_RTCRAM END\n", tag);
#endif  // LW_HEAP_FORENSICS_ENABLE
}

} // namespace diagnostics
} // namespace lightwaveos
