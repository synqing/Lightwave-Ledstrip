---
abstract: "Pipeline partitioning for the K1v2 audio→visual system. Defines the Core 0 / Core 1 boundary, the actor-model task layout, and what crosses the SnapshotBuffer cross-core bridge. Used by Panels 1 (System Overview) and 4 (Cross-Core Publication). Per Captain Gate 1 Amendment Z: zone numbering of any kind (Zone 0, 1, 2, 3, 4) is STRUCK from the entire infographic series."
---

# 02 — Pipeline Partitioning

## Top-level partitioning

K1v2 firmware is organised as a FreeRTOS **actor model** running on the ESP32-S3's two cores. The cores have orthogonal responsibilities and communicate exclusively through a single lock-free contract:

```
   ┌─────────────────────────────┐         ┌─────────────────────────────┐
   │  Core 0                     │         │  Core 1                     │
   │  ─────                      │         │  ─────                      │
   │  AudioActor                 │ Snapshot│  RendererActor              │
   │  • I2S DMA capture          │  Buffer │  • 120 FPS render loop      │
   │  • Spectral analysis        │ ──────▶ │  • EffectContext build      │
   │  • Tempo / beat tracking    │  (lock- │  • Centre-origin LED map    │
   │  • Publishes ControlBusFrame│   free) │  • RMT4 LED output          │
   └─────────────────────────────┘         └─────────────────────────────┘
```

Core 0 is the **audio analysis engine**; Core 1 is the **visual rendering engine**. The two cores share state only through `SnapshotBuffer` — a lock-free, double-buffered cross-core bridge that publishes one `ControlBusFrame` per audio analysis frame (every 8 ms).

## Core 0 — AudioActor

**Task home:** Core 0 (pinned)
**Tick cadence:** 125 Hz (one frame per 8 ms audio hop, matching the ESV11 backend at 32 kHz / 256-sample hop)
**Responsibilities:**

1. I2S DMA capture from the MEMS microphone.
2. Per-frame audio conditioning (DC block, normalisation, windowing).
3. Spectral analysis:
   - 512-sample Hann-windowed FFT yielding 256 magnitude bins (62.5 Hz spacing at 32 kHz).
   - 64-bin Goertzel bank for sparse-spectrum measurement.
   - 12-class chroma vector (one per pitch class).
   - 8-band octave energy summation.
4. Tempo and beat tracking (Goertzel-based with novelty-envelope decay 0.999/frame).
5. Onset detection and percussion-trigger extraction.
6. Population of `ControlBusFrame` and atomic publication to `SnapshotBuffer`.

**Output:** One `ControlBusFrame` published at 125 Hz, ≤5120 bytes per frame.

## Core 1 — RendererActor

**Task home:** Core 1 (pinned)
**Tick cadence:** 120 FPS (8.33 ms per frame)
**Responsibilities:**

1. Acquire latest stable `ControlBusFrame` from `SnapshotBuffer` via lock-free read.
2. Visual interpolation between consecutive audio frames (audio is 125 Hz, render is 120 FPS — frame rates are intentionally close but not synchronous; the renderer interpolates to mask the small phase offset).
3. Build per-frame `EffectContext` containing the LED buffer, frame delta-time, render-context flags, and the audio snapshot.
4. Run active effect's `render()` against the `EffectContext`. Effects MUST complete in under 2.0 ms and MUST NOT allocate heap.
5. Centre-origin LED mapping: effects render outward from / inward to the seam between LEDs 79 and 80 (`CENTER_POINT = 80`).
6. Push framebuffer to LED hardware via the patched FastLED RMT4 path. The RMT peripheral DMA-equivalent — CPU returns immediately after queuing the buffer; LED wire time runs in parallel on the peripheral.

**Output:** One full LED frame on the dual 160-LED strips (320 LEDs total) per render tick.

## The cross-core bridge — SnapshotBuffer

The contract that connects the two actors:

| Property | Value | Source |
|---|---|---|
| Mechanism | Lock-free double buffer (`m_buf[2]`) | `SnapshotBuffer.h:27–52` |
| Synchronisation | `std::atomic<uint32_t>` for active-index and sequence | Same |
| Write fence | `memory_order_release` on publish | Same |
| Read fence | `memory_order_acquire` on snapshot acquire | Same |
| Publication direction | Core 0 (writer) → Core 1 (reader) | Actor task pinning |
| Publication cadence | 125 Hz (one frame per audio hop) | Driven by AudioActor tick |
| Read cadence | 120 FPS (one read per render tick) | Driven by RendererActor tick |
| Frame size | ≤5120 bytes (`static_assert` upper bound) | `ControlBus.h:261-262` |

**Critical property:** the reader **never blocks**. A render tick that fires between two audio publications simply reads the last fully-published frame. Publication is sequence-numbered; partial writes are invisible to readers because the `m_active` swap is a single atomic operation.

## What crosses the boundary

The `ControlBusFrame` payload (≤5120 bytes) carries the entire audio-derived state visible to the renderer:

- **Magnitude spectrum:** `bins256[]` — 256 FFT magnitude bins.
- **Pitch information:** `chroma[12]` — one slot per pitch class.
- **Octave energy:** `bands[8]` — summed energy in 8 octave bands.
- **Loudness / dynamics:** RMS, peak, normalised loudness.
- **Rhythm:** `beat`, `onset`, tempo estimate, percussion triggers.
- **Aggregate statistics:** spectral centroid, spectral flux, novelty.

Audio AGC followers also live inside the frame, but their internal partitioning is **struck from this infographic series in its entirety** per Captain Gate 1 Amendment Z. The strike now extends to ALL zone numbering — not just audio-AGC partitioning — and there is no longer an exception for user/API-facing identifiers. Any panel that would otherwise reference zone numbering must instead refer to the spectral analysis in aggregate (FFT + Goertzel + chroma + octave bands) and to the render API surface in topology-neutral terms (LED buffer, delta-time, render-context flags, audio snapshot).

## What does NOT cross the boundary

- Raw audio samples (stay on Core 0).
- LED framebuffers (stay on Core 1).
- I2S DMA buffer pointers (Core 0 only).
- RMT peripheral state (Core 1 only).
- Effect-internal state (private to each effect, allocated as static buffers in render-class scope).

The deliberate narrowness of the cross-core surface is what makes the system safe at 120 FPS. Adding fields to `ControlBusFrame` is governed; it is not a free dumping ground.

## Why this partitioning works

Two factors:

1. **Asymmetric workload.** Core 0 runs at 125 Hz with bursty FFT cost; Core 1 runs at 120 FPS with steady per-frame effect cost. Splitting them avoids any single-core schedulability conflict.
2. **One-way data flow.** The renderer reads a snapshot; the audio analyser doesn't care what the renderer does. Removing back-pressure removes the largest source of cross-core jitter.

The lock-free SnapshotBuffer is the small, sharp contract that makes (1) and (2) achievable without mutex contention or queue-pressure pathologies.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | Claude (claude-opus-4-7) | Created. Documents Core 0 / Core 1 partitioning, actor-model task layout, SnapshotBuffer cross-core bridge mechanics, and the contents of ControlBusFrame payload. Audio AGC zone count deferred per BACKLOG F-6. |
| 2026-05-04 | Claude (claude-opus-4-7) | Captain Gate 1 Amendment Z: strike expanded from audio-AGC zone count to ALL zone numbering across the infographic series. EffectContext composition rephrased from "zone identifier" to "render-context flags" — the API-surface description no longer invokes user/API-facing zone numbering (1/2/3). Abstract updated to reflect the broader strike. |
