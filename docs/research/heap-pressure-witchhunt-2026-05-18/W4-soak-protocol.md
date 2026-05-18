---
abstract: "SSA-W4 soak protocol. Flash diagnostic firmware to K1 V2 (MAC b4:3a:45:a5:87:f8), run 10-minute browser-driven soak with web app open, capture serial to file, then analyse [HEAP-FORENSICS] log lines for shed-latch timing, block-size histograms, PSRAM utilisation, and UDP ENOMEM clustering. Diagnostic-only firmware — no production logic changes."
---

# W4 Soak Protocol — Heap Pressure Witch-Hunt

**Date:** 2026-05-18
**Branch:** `ssa-w4-diagnostic-build`
**Base commit:** `06cc6e76` (universal authority inversion) reachable
**Hardware:** K1 V2, MAC `B4:3A:45:A5:87:F8`
**Mission:** Capture comprehensive heap-state evidence during a 10-minute browser-driven soak so the architectural decision on internal-SRAM pressure can be anchored to data, not to inference.

---

## 1 — What the diagnostic firmware adds

All changes are diagnostic-only. No production logic, allocator, PSRAM migration, or wire-format edits.

| Site | When it fires | Reason tag | Shape |
|---|---|---|---|
| `WebServer::update()` (after begin, once) | First tick after `m_running == true` | `boot` | verbose |
| `WebServer::update()` | First inbound WS client (`m_lastClientConnectMs != 0`) | `http.first` | verbose |
| `WebServer::update()` | Every 60 000 ms | `periodic` | verbose |
| `WebServer::update()` | Every 1 000 ms (except the tick the 60 s verbose fires) | `periodic.scalar` | scalar |
| `WebServer::updateLowHeapShedState()` | `m_lowHeapShed` flips `false→true` | `shed.enable` | verbose |
| `WebServer::updateLowHeapShedState()` | Clean release (`heapRecovered`) | `shed.release` | verbose |
| `WebServer::updateLowHeapShedState()` | Max-latch force-clear in hysteresis band | `shed.force` | verbose |
| `UdpStreamer::sendPacket()` | `endPacket() != 1` (rate-limited 1 Hz) | `udp.enomem` | verbose |
| `SerialCLI` `dbg memory verbose` | Operator-triggered | `ondemand` | verbose |

**Two-cadence design (Phase 0.5, 2026-05-18).** Scalar shape is Blocks 1-3 (~5 serial lines per call — wall-clock anchor + region-1 internal DRAM scalars + aggregate-internal cross-region max + RTC_FAST scalars + PSRAM scalars). Verbose shape adds Block 4 (full `heap_caps_print_heap_info` histograms across MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL, MALLOC_CAP_SPIRAM, MALLOC_CAP_RTCRAM — typically 40-80 lines). The 1 Hz scalar tick gives bisection-grade trajectory resolution for `region1.largest` without flooding serial during sustained load. The 60 s verbose tick captures the fragmentation profile.

**Region-aware split (Branch 1, 2026-05-19).** Captain's diagnostic correction: the previous aggregate `internal.largest` reading was misleading because it was the cross-region max across regions 1+2+3, which let a small high-water bucket in region 2 or 3 mask region-1 fragmentation. The scalar block now emits three separate concerns:

- `region1.*` — primary DMA-capable internal DRAM heap (~216 KB on K1 V2 at 0x3fcb4af0, isolated via `MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`). The live app heap where lwIP, AsyncTCP, AsyncWebSocket and effect statics actually allocate. **This is now the primary survival scalar.**
- `aggregate_internal.*` — cross-region max across regions 1+2+3 (the historical `internal.*` reading). Preserved for backwards-comparable logs and to surface region-2/3 high-water masking when it occurs. NOT a reliable region-1 proxy.
- `rtc_fast.*` — RTC_FAST region 4 at 0x600fe000 (~8 KB) via `MALLOC_CAP_RTCRAM`. Captain's correction: this is RTC fast memory, not RTC slow.

IDF 4.4.7 does NOT expose per-region enumeration APIs (`heap_caps_get_info_by_addr`, `heap_caps_walk_registered_heaps`, public `multi_heap_handle_t` registry) — region 1 isolation is cap-mask-based. The verbose Block 4 dump for `MALLOC_CAP_INTERNAL` remains the cross-check; operators compare `region1.free` against the per-region totals printed by `heap_caps_print_heap_info`.

All output goes to Serial via `Serial.printf` and `heap_caps_print_heap_info()` — bypasses `LW_LOG_*` so dumps remain visible at any log level.

## 2 — Flash command (MAC-verified)

The canonical PlatformIO upload path is mandatory (per `feedback_k1v2_flash_procedure.md`). NEVER substitute manual `esptool.py`.

```sh
# 1. List USB devices and find the K1 V2 port
ls /dev/tty.usbmodem* 2>/dev/null

# 2. Verify MAC before flashing — wrong-device flash incident 2026-03-24 is the
#    reason this step is non-optional.
esptool.py --port /dev/tty.usbmodem<N> chip_id
#    EXPECT: MAC b4:3a:45:a5:87:f8 (K1 V2). If you see b4:3a:45:a5:89:b4 that
#    is K1 V1 — abort, find the K1 V2 port.

# 3. Flash from worktree
cd /tmp/agent_ssa_w4_<timestamp>/firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/tty.usbmodem<N>
```

## 3 — 10-minute soak procedure

Open a fresh shell to capture serial BEFORE flashing finishes so the post-boot baseline is recorded.

```sh
# Tee serial to file. Adjust port. Append timestamp for run separation.
pio device monitor -b 115200 --port /dev/tty.usbmodem<N> \
    | tee /tmp/w4-soak-$(date +%Y%m%dT%H%M%S).log
```

Once K1 is up (look for `WebServer started`, AP IP banner):

| Minute | Captain action |
|---|---|
| 0:00 | Connect laptop to `LightwaveK1-...` SoftAP. |
| 0:30 | Open the web app at `http://192.168.4.1/`. Confirm dashboard loads. |
| 1:00 | Browse effect catalogue. Select 3 effects in quick succession, ~10 s each. |
| 2:00 | Cycle through 4 palettes back-to-back. |
| 3:00 | Toggle zones (Zone 1, Zone 2, Zone 3) on / off twice. |
| 4:00 | Trigger an effect transition (cross-fade) two or three times. |
| 5:00 | Send a serial `dbg memory verbose` over the second USB shell, if available, to anchor a known-moment snapshot at the soak midpoint. |
| 5–9:00 | Leave the web app on, switch effects every ~30 s, occasionally refresh the page (forces a fresh WS connect → heap allocation pressure). |
| 9:00 | Final `dbg memory verbose`. |
| 10:00 | Stop the serial capture (`Ctrl-]` in `pio device monitor`). |

**Goal:** Reproduce the conditions that historically pushed `m_lowHeapShed` true. If no shed event fires in the 10 minutes, the soak is still valid — we just learn that the dashboard alone is insufficient to drive the latch.

## 4 — Expected log shape

Each scalar `[HEAP-FORENSICS]` dump produces ~5 lines (verbose dumps add the IDF histogram payload, prefixed with the reason tag). Example boot snapshot (verbose):

```
[HEAP-FORENSICS] boot millis=4321 uptime_s=4 shed=0 latched_ms=0
[HEAP-FORENSICS] boot region1.free=132480 region1.largest=110592 region1.min_free=130112 region1.alloc_blocks=420 region1.free_blocks=8
[HEAP-FORENSICS] boot aggregate_internal.free=156432 aggregate_internal.largest=131072
[HEAP-FORENSICS] boot rtc_fast.free=8176 rtc_fast.largest=8176
[HEAP-FORENSICS] boot psram.total=8204288 psram.largest=8126464 psram.min_free=8204288 psram.alloc_blocks=18 psram.free_blocks=4
[HEAP-FORENSICS] boot heap_caps_print MALLOC_CAP_8BIT BEGIN
Heap summary for capabilities 0x00000004:
  At 0x3fcb4af0: len 0x... bytes, free 0x... bytes, largest free block 0x...
  ... (histogram lines from heap_caps_print_heap_info) ...
  Totals: free 156432 allocated 32096 minimum_free 152104 ...
[HEAP-FORENSICS] boot heap_caps_print MALLOC_CAP_8BIT END
[HEAP-FORENSICS] boot heap_caps_print MALLOC_CAP_INTERNAL BEGIN
  ... (regions 1+2+3 aggregate view — cross-check the `region1.*` and
       `aggregate_internal.*` scalars against the per-region lines here) ...
[HEAP-FORENSICS] boot heap_caps_print MALLOC_CAP_INTERNAL END
[HEAP-FORENSICS] boot heap_caps_print MALLOC_CAP_SPIRAM BEGIN
  ... (PSRAM regions; expect mostly idle on K1 V2) ...
[HEAP-FORENSICS] boot heap_caps_print MALLOC_CAP_SPIRAM END
[HEAP-FORENSICS] boot heap_caps_print MALLOC_CAP_RTCRAM BEGIN
  At 0x600fe000: len 0x1ff0 bytes, free 0x... bytes, largest free block 0x...
  Totals: free 8176 allocated 0 minimum_free 8176 ...
[HEAP-FORENSICS] boot heap_caps_print MALLOC_CAP_RTCRAM END
```

The illustrative numbers above are first-order — actual figures vary by build (ESV11 + 32 kHz + K1 V2 effect statics typically leave region 1 at ~130 KB free / ~110 KB largest at boot, with `aggregate_internal` ~25 KB higher due to regions 2+3 contribution).

Shed-edge entries replace `boot` with `shed.enable` / `shed.release` / `shed.force` and include the actual `shed=1 latched_ms=…` fields.

UDP ENOMEM lines are rate-limited to one dump per second (per the wrapper in `UdpStreamer::sendPacket`), so a burst of failures collapses into a single annotated snapshot.

## 5 — Post-soak capture commands

```sh
# Extract only the forensics lines (drop the noisy heap_caps_print histograms
# if needed) for a first-pass quick read.
grep '^\[HEAP-FORENSICS\]' /tmp/w4-soak-*.log > /tmp/w4-forensics.tsv

# Count dumps by reason tag.
grep -oE '\[HEAP-FORENSICS\] [a-z.]+' /tmp/w4-soak-*.log \
    | awk '{print $2}' \
    | sort | uniq -c | sort -nr

# Pull region-1 heap timeseries (free + largest block — the primary survival
# scalar). This is the post-Branch-1 region-aware shape; previous logs from
# the pre-2026-05-19 firmware used `internal.total` / `internal.largest`
# instead. Grep both forms when comparing across firmware versions.
grep '^\[HEAP-FORENSICS\] .* region1.free=' /tmp/w4-soak-*.log \
    | sed -E 's/.*\[HEAP-FORENSICS\] ([a-z.]+) region1\.free=([0-9]+) region1\.largest=([0-9]+).*/\1\t\2\t\3/' \
    > /tmp/w4-region1-timeseries.tsv

# Pull aggregate-internal cross-region max timeseries (the legacy reading —
# preserved so the masking effect between region 1 and aggregate can be seen
# in the same capture).
grep '^\[HEAP-FORENSICS\] .* aggregate_internal.free=' /tmp/w4-soak-*.log \
    | sed -E 's/.*\[HEAP-FORENSICS\] ([a-z.]+) aggregate_internal\.free=([0-9]+) aggregate_internal\.largest=([0-9]+).*/\1\t\2\t\3/' \
    > /tmp/w4-aggregate-timeseries.tsv

# Pull all shed transitions with their millis anchors.
grep -E '\[HEAP-FORENSICS\] shed\.(enable|release|force)' /tmp/w4-soak-*.log
```

## 6 — What to look for in the captured log

1. **First shed-latch event timing.** Find the first `shed.enable` block. Note `millis=`, `uptime_s=`, and which `periodic` / `http.first` dump preceded it. If the gap between `http.first` and the first `shed.enable` is sub-30 s, dashboard activity alone is sufficient to drive the latch. If it is multi-minute, the latch is being driven by a slower accumulator.

2. **Block-size histogram pattern at shed.** Inside the `shed.enable` block, the `heap_caps_print_heap_info(MALLOC_CAP_INTERNAL)` dump prints `largest free block 0x...` per region. Compare region 1 (0x3fcb4af0) specifically to the boot snapshot's region-1 largest block. If region-1 `largest free block` has collapsed to ~3-4 KB while `region1.free` is still ≥ shed threshold, this is the fragmentation-latch signature (`firmware_wifi_architecture.md` and the 2026-05-01 hysteresis-band fix were both grounded in this exact pattern). **Watch for masking:** if `aggregate_internal.largest` is materially higher than `region1.largest`, regions 2 or 3 are hiding region-1 pressure from the legacy reading — this is the exact case Captain's 2026-05-19 diagnostic correction targets.

3. **Whether `largest_free_block` correlates with specific allocator events.** Diff `region1.largest` between consecutive `periodic.scalar` dumps. A monotonic decay points to fragmentation accumulation (lwIP pbufs, AsyncTCP buffers, AsyncWebSocket queue). A step-change at a `http.first` correlates the dashboard's initial WS upgrade with the first big allocation hit. Compare against `aggregate_internal.largest` in the same line bundle — if region-1 drops faster than the aggregate, the pressure is concentrated in the live app heap and PSRAM relocation / structural recovery is the lever.

4. **PSRAM utilisation.** PSRAM should stay essentially idle through the soak (`psram.total` near 8 MB, `psram.largest` ≥ 7.9 MB). If PSRAM dives in tandem with internal SRAM, the migration argument loses force — the pressure is everywhere, not internal-only.

5. **Whether `udp.enomem` events cluster around specific moments.** Group `udp.enomem` dumps by `millis=`. A cluster co-located with a `shed.enable` confirms the lwIP pbuf-pool exhaustion path that motivated the broadcast suppression. A cluster independent of shed events means UDP failures are an independent failure mode and need their own mitigation.

6. **Force-clear vs clean release ratio.** Count `shed.force` vs `shed.release`. A high force-clear ratio means the hysteresis band recovery gate (added 2026-05-01) is being relied upon — the heap is not naturally returning above the resume ceiling.

## 7 — Phase 1 acceptance gate

Each Phase 1 single-variable soak (Steps 1.1 → 1.4 → 1.2 → 1.3 → 1.6 → 1.5) must clear ALL four criteria below over a 10-minute sustained-load soak driven by the procedure in Section 3.

| # | Criterion | Source | Verification |
|---|---|---|---|
| 1 | `region1.largest` floor ≥ **45,000 B** sustained | Captain's chase-headroom target (Option B, 2026-05-18); region-aware re-anchored 2026-05-19 to the live app-heap DRAM region (NOT the legacy aggregate cross-region max which historically masked region-1 pressure) | Time-series of `region1.largest` from `periodic.scalar` lines never drops below 45 000 across the 10-min window |
| 2 | `m_lowHeapShed` never latches | Phase 1 success means the shed mechanism never fires under load | Zero `shed.enable` entries in the captured log |
| 3 | UDP 80 B `sendto` success rate ≥ 99.9 % | Continuous probe traffic during the soak | Zero or near-zero `udp.enomem` entries; if present, count ≤ 0.1 % of probe packets |
| 4 | Renderer Core 1 P99 frame time ≤ 2.0 ms | Hard render-budget ceiling (CLAUDE.md) | VP Stack telemetry / `vp stack` serial command snapshot at minute 9 of the soak |

**Threshold-bump constraint (Captain, 2026-05-18).** `LW_INTERNAL_HEAP_SHED_BELOW_BYTES` and `LW_INTERNAL_HEAP_RESUME_ABOVE_BYTES` in `firmware-v3/src/network/WebServer.h:133-136` are NOT bumped in the same commit as the Phase 1 structural recoveries. The threshold bump (and the corresponding `platformio.ini:200-204` comment fix) lands as the FINAL Phase 1 commit, gated on a soak that already shows `internal.largest ≥ 45,000 B sustained` with the current 12 KB / 28 KB hysteresis still in place. If interim soaks show 45 KB is not sustainable on Phase 1 alone, the threshold bump is deferred — the gap becomes the Phase 1.5 success criterion (multi-client gate work, AsyncWebSocketSharedBuffer PSRAM relocation).

**Bisect signal.** If a single-variable soak worsens one criterion while improving another, halt the sequence and surface to Captain before continuing. That is a "hidden cost no document predicted" signal — visible, not buried.

## 8 — Constraints honoured

- No production logic changed.
- No allocator changes, no PSRAM migration, no wire-format edits.
- No new FreeRTOS tasks (piggybacks `WebServer::update()`).
- Render path unaffected — dumps only fire on sparse events.
- All log strings British English.
- Build verified clean: `pio run -e esp32dev_audio_esv11_k1v2_32khz` exits 0; `nm` confirms `lightwaveos::diagnostics::dump` and `reasonTag` linked into the final ELF.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-18 | agent:ssa-w4 | Created — soak protocol, expected log shape, capture commands, analysis checklist. |
| 2026-05-18 | agent:claude-opus-4-7 | Phase 0.5 refinement — added 1 Hz `periodic.scalar` cadence (scalar Blocks 1-3 only) alongside 60 s `periodic` verbose tick; updated Section 1 table to reflect dual cadence; inserted Section 7 "Phase 1 acceptance gate" with the 45 KB largest-block target and Captain's threshold-bump-as-final-commit constraint; renumbered "Constraints honoured" to Section 8. |
