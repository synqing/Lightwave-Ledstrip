# Milestone: Deferred Runtime Hardening (March 2026)

**Branch:** `milestone/deferred-runtime-hardening-2026-03`
**Branch point:** `stage1-d14-r3-stable-2026-03-05` (`32703522`)
**K1 baseline:** Binary SHA-256 `3160694119cb67d01509bdac96093e567fffde6dfc2dcf6bbfd4e7a920b0b61f`

---

## Hard Runtime Rules (mandatory for ALL batches)

1. No `heap_caps_get_largest_free_block()` or heap walking in `WebServer::update()` paths.
2. Any test build flashed to K1 MUST be reflashed back to stable tag after validation.
3. Verify MAC (`b4:3a:45:a5:87:f8`) before every flash.
4. Opening ESP32-S3 USB-CDC serial port resets the board.
5. K1 is AP-ONLY. No STA mode changes.

---

## Acceptance Gates (per batch)

Every batch must pass ALL gates before the next batch begins:

| Gate | Criteria |
|------|----------|
| **Build** | `pio run -e esp32dev_audio_esv11_k1v2_32khz` succeeds |
| **Visual** | Both strips smooth, no corruption/stutter |
| **Serial `s`** | `showSkips=0`, LED show avg 4.8-5.5ms, FPS 110+, no panics/RMT errors |
| **Stack/heap** | loopTask stack < 95%, no heap-shed flapping |
| **Soak** | 10-minute minimum with effect cycling + AP client connected |
| **Guardrail** | `check_hot_path_safety.sh` passes (0 violations) |
| **Rollback** | Tag created BEFORE batch. Revert if any gate fails. |

---

## Batch 1: Serial/Debug Cleanup (LOW risk, ~2h)

**Rollback point:** `stage1-d14-r3-stable-2026-03-05`

| Item | Description | File(s) | Est |
|------|-------------|---------|-----|
| H-6 | Deduplicate capture dump handler (magic 0xFD protocol) | `main.cpp:1502-1553, 2191-2246` | 1h |
| L-3 | Remove dead `setCurrentLegacyEffectId()` stub | `RendererActor.cpp:104-106` | 10m |
| L-5 | Health check interval magic number → named constant | `main.cpp:3406` | 10m |

**Why first:** Zero runtime behaviour change. Removes dead code and deduplicates copy-pasted protocol. No hot-path edits. Lowest possible risk.

**Dependencies:** None.

---

## Batch 2: Architecture Cleanup (LOW-MEDIUM risk, ~8h)

**Rollback point:** Tag after Batch 1 passes all gates.

| Item | Description | File(s) | Est |
|------|-------------|---------|-----|
| H-7 | Remove extern globals, inject via WebServerContext | `main.cpp`, `WebServer.cpp`, `V1ApiRoutes.cpp` | 6h |
| H-9 | Consolidate dual MAX_ZONES (8 vs 4) | `config/limits.h:76`, `ZoneDefinition.h:20` | 2h |

**Why second:** H-7 removes module boundary violations but requires careful dependency wiring. H-9 resolves a sizing inconsistency that could mask OOB bugs. Neither touches render/audio hot paths.

**Dependencies:** None (independent of Batch 1, but sequenced for risk ordering).

---

## Batch 3: Persistence Safety (MEDIUM risk, ~4h)

**Rollback point:** Tag after Batch 2 passes all gates.

| Item | Description | File(s) | Est |
|------|-------------|---------|-----|
| H-8 | Add portMUX_TYPE to persistence singletons | `core/persistence/*.h` (4 files) | 4h |

**Why third:** Adds cross-core synchronisation to NVS managers currently accessed without mutexes from both cores. Medium risk because incorrect spinlock usage can cause deadlocks. Requires careful testing of effect changes, preset saves, and tuning writes under AP+WS load.

**Dependencies:** None.

---

## Batch 4: Stack Pressure Reduction (MEDIUM risk, ~2h)

**Rollback point:** Tag after Batch 3 passes all gates.

| Item | Description | File(s) | Est |
|------|-------------|---------|-----|
| H-4 | Move `forceOneShotCapture()` 1920-byte stack arrays to member buffers | `RendererActor.cpp:1045-1058` | 2h |

**Why fourth:** Touches RendererActor (Core 1 hot path). The change is mechanical (stack → member) but affects the renderer task. loopTask is at 93% stack usage — this provides headroom for future work.

**Dependencies:** None.

---

## Batch 5: Render Performance (HIGH risk, ~12h)

**Rollback point:** Tag after Batch 4 passes all gates.

| Item | Description | File(s) | Est |
|------|-------------|---------|-----|
| H-1 | Fuse ColorCorrectionEngine 6-pass → single-pass | `ColorCorrectionEngine.cpp:181-217` | 4h |
| M-1 | Cache `validateEffectId()` per frame (triple call elimination) | `RendererActor.cpp:774, 1448, 1645` | 1h |
| M-2 | Transition trig → `sin8()`/`cos8()` LUTs | `TransitionEngine.cpp:477-1048` | 4h |
| M-3 | Merge silence gate dual `nscale8()` passes | `RendererActor.cpp:1632-1688` | 1h |
| H-5 | Begin main.cpp decomposition (SerialCommandHandler extraction) | `main.cpp` | 6h |

**Why last:** These are performance optimisations and structural refactors that directly touch render hot paths. H-1 is the biggest single frame-time win (~300-500us) but involves fusing HSV conversions. H-5 is a large refactor with no performance gain. This batch may be split into sub-batches (5a: M-1/M-3, 5b: H-1/M-2, 5c: H-5) depending on soak results.

**Dependencies:** Batch 4 (stack headroom) should be done before adding render-path complexity.

---

## Deferred to Future Milestone (not in scope)

| Item | Reason |
|------|--------|
| M-4 | AudioActor backend strategy extraction (~8h) — architecture only, no safety gain |
| M-5 | Event-driven I2S / Sprint 2 (~8h) — requires I2S driver rewrite |
| M-7 | volatile globals to .cpp (~2h) — ESV11 legacy, linker dedup works |
| M-10 | Enhanced effect deduplication (~12h) — large refactor, effects work fine |
| M-11 | Common AudioHandlers serialisation (~4h) — cosmetic |

---

## Total Estimated Effort

| Batch | Risk | Effort |
|-------|------|--------|
| 1 | LOW | ~2h |
| 2 | LOW-MEDIUM | ~8h |
| 3 | MEDIUM | ~4h |
| 4 | MEDIUM | ~2h |
| 5 | HIGH | ~16h |
| **Total** | | **~32h** |

---

## Go/No-Go Checklist: Batch 1

- [ ] Worktree at `stage1-d14-r3-stable-2026-03-05` (`32703522`)
- [ ] K1 currently running stable tag (verified by serial `s`)
- [ ] `check_hot_path_safety.sh` passes on current code
- [ ] Operator available for visual gate confirmation
- [ ] Serial monitor script ready at `/tmp/capture_serial_s.py`
- [ ] Port verified (`/dev/cu.usbmodem212401` or check with `ls /dev/cu.usb*`)
- [ ] MAC verified before first flash (`b4:3a:45:a5:87:f8`)

**First batch recommendation:** Batch 1 (H-6, L-3, L-5). Zero runtime behaviour change, removes dead code, ~2h effort. If it passes all gates, proceed to Batch 2.
