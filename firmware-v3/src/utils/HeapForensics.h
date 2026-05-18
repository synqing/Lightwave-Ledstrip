/**
 * @file HeapForensics.h
 * @brief Diagnostic heap dump emitter (SSA-W4 — non-production).
 *
 * Emits `[HEAP-FORENSICS]` tagged serial lines summarising internal SRAM and
 * PSRAM state. Used to anchor the WebServer low-heap shedding witch-hunt
 * (`docs/research/heap-pressure-witchhunt-2026-05-18/`) — every shed-latch
 * transition, every once-per-minute tick under sustained load, the first
 * observed HTTP request after boot, and every UDP ENOMEM failure call site
 * emits a snapshot. Greppable by the `[HEAP-FORENSICS]` prefix.
 *
 * Diagnostic only. No production logic depends on this header. All output
 * goes to Serial — no WebSocket payloads, no allocations on the render path.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace lightwaveos {
namespace diagnostics {

/// Reason-of-call labels used to prefix each dump for grep-ability.
enum class HeapDumpReason : uint8_t {
    Boot,                  ///< First post-boot baseline snapshot
    ShedLatched,           ///< Low-heap shedding ENABLED transition
    ShedReleased,          ///< Low-heap shedding DISABLED transition
    ShedForceCleared,      ///< Max-latch force-clear in hysteresis band
    Periodic,              ///< Once-per-minute verbose sustained-load tick
    PeriodicScalar,        ///< 1 Hz scalar-only sustained-load tick (Phase 0.5)
    OnDemand,              ///< Serial command `dbg memory verbose`
    UdpEnomem,             ///< UDP endPacket() ENOMEM failure
    FirstHttpRequest       ///< First inbound WS/HTTP connect after begin()
};

/// Returns a stable short string for the given reason ("shed.enable",
/// "shed.release", "shed.force", "boot", "periodic", "periodic.scalar",
/// "ondemand", "udp.enomem", "http.first"). Used in the dump prefix; the
/// strings are part of the log-format contract — do not edit without updating
/// W4-soak-protocol.md.
const char* reasonTag(HeapDumpReason reason);

/// Emits the full diagnostic dump to Serial. Includes scalar fields PLUS
/// verbose `heap_caps_print_heap_info` block histograms. Format (region-aware,
/// 2026-05-19 — Captain's diagnostic correction split the previous aggregate
/// `internal.*` block into region1 + aggregate_internal + rtc_fast):
///
///     [HEAP-FORENSICS] <reason> millis=... uptime_s=... shed=0|1 latched_ms=...
///     [HEAP-FORENSICS] <reason> region1.free=... region1.largest=...
///       region1.min_free=... region1.alloc_blocks=... region1.free_blocks=...
///     [HEAP-FORENSICS] <reason> aggregate_internal.free=... aggregate_internal.largest=...
///     [HEAP-FORENSICS] <reason> rtc_fast.free=... rtc_fast.largest=...
///     [HEAP-FORENSICS] <reason> psram.total=... psram.largest=...
///     [HEAP-FORENSICS] <reason> heap_caps_print BEGIN
///       <heap_caps_print_heap_info verbose dump — MALLOC_CAP_8BIT,
///        MALLOC_CAP_INTERNAL, MALLOC_CAP_SPIRAM, MALLOC_CAP_RTCRAM>
///     [HEAP-FORENSICS] <reason> heap_caps_print END
///
/// `region1.*` isolates the primary DMA-capable internal DRAM heap
/// (~216 KB on K1 V2, where the live app heap, lwIP, AsyncTCP, AsyncWebSocket,
/// and effect statics all allocate) via `MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`.
/// `aggregate_internal.*` is the historical cross-region max across regions
/// 1+2+3 — kept for backwards-comparable logs but NOT a reliable region-1
/// proxy. `rtc_fast.*` is region 4 at 0x600fe000 (~8 KB) via `MALLOC_CAP_RTCRAM`.
///
/// Approx 14 serial lines per call (plus the verbose IDF histogram payload —
/// typically 40-80 lines). Use for edges (boot, shed transitions, on-demand)
/// and the 60 s periodic verbose tick.
///
/// `shedActive` and `shedLatchedMs` are passed in by the caller so this helper
/// does not need to reach into WebServer state.
void dump(HeapDumpReason reason, bool shedActive, uint32_t shedLatchedMs);

/// Emits ONLY the scalar fields (Blocks 1-3 — wall clock + shed state +
/// internal SRAM scalars + PSRAM scalars). Skips the verbose
/// `heap_caps_print_heap_info` block histograms. ~3 serial lines per call.
///
/// Use this on the 1 Hz periodic tick — bisection-grade scalar resolution
/// without flooding serial bandwidth (which would itself create heap
/// pressure via tx buffering). The 60 s verbose tick continues to use
/// dump() for the full histogram snapshot.
///
/// Same contract as dump() for `shedActive` and `shedLatchedMs`.
void dumpScalar(HeapDumpReason reason, bool shedActive, uint32_t shedLatchedMs);

} // namespace diagnostics
} // namespace lightwaveos
