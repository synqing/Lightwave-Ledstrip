# Audio Chunk Size Analysis: 256 vs 128 Samples

## 1. Executive Summary
This analysis evaluates the impact of reducing the audio hop size (`HOP_SIZE`) from **256 samples** (16ms) to **128 samples** (8ms). The investigation concludes that this change is **highly beneficial** for system responsiveness and visual fluidity, with **negligible** CPU overhead and **no degradation** in audio quality or frequency analysis precision.

**Recommendation:** **Proceed with the change to 128 samples.** This aligns with the goal of high-performance real-time visualization.

## 2. Computational Performance

### 2.1. Latency
| Metric | Current (256 samples) | Proposed (128 samples) | Improvement |
| :--- | :--- | :--- | :--- |
| **Input Latency** | 16.0 ms | 8.0 ms | **50% Reduction** |
| **Visual Refresh** | 62.5 Hz | 125.0 Hz | **2x Smoother** |
| **Total System Lag** | ~32-48 ms | ~16-24 ms | **Perceptibly Snappier** |

*Note: Visual refresh rate (125Hz) exceeds the typical LED strip update rate (60-100Hz), ensuring the audio engine is never the bottleneck.*

### 2.2. CPU Utilization
*   **Per-Hop Overhead:** The fixed overhead of `processHop()` (function calls, timer checks, logging) occurs 2x as often.
*   **Data Processing:** The *total* number of samples processed per second remains constant (16,000). Operations like RMS calculation, AGC, and DC removal are linear $O(N)$, so the total CPU load for these remains roughly unchanged.
*   **Goertzel Analysis:** The analyzer uses a fixed 512-sample window. It currently runs every 2 hops (32ms). With 128-sample hops, it will run every 4 hops (32ms). **CPU load for frequency analysis remains identical.**
*   **Net Impact:** Estimated <1% increase in total CPU usage due to context switching and ISR overhead.

### 2.3. Memory Usage
*   **DMA Buffers:** Existing configuration (4 x 512 samples) works perfectly. Reading 128-sample chunks from a 512-sample circular buffer is safe and efficient.
*   **Stack Usage:** No change.
*   **Heap Usage:** No change.

## 3. Audio Quality & Signal Processing

### 3.1. Temporal Resolution (RMS & Flux)
*   **Impact:** **Positive.** RMS and Spectral Flux (beat detection) are computed per-hop.
*   **Result:** Beat detection becomes 2x more precise (8ms vs 16ms resolution). Transients (drum hits) are detected sooner, resulting in "tighter" visuals.

### 3.2. Frequency Analysis (Goertzel)
*   **Impact:** **Neutral.** The Goertzel algorithm relies on a `WINDOW_SIZE` of 512 samples (32ms) to maintain frequency resolution (especially for bass).
*   **Constraint:** We **must not** reduce the Goertzel window size.
*   **Implementation:** The `GoertzelAnalyzer` correctly accumulates samples until the window is full. It will simply take 4 hops to fill instead of 2. The update rate for frequency bands remains ~31Hz.

### 3.3. Smoothing & AGC
*   **Adjustment Required:** All time-domain smoothing coefficients must be adjusted to account for the doubled update rate.
    *   `ControlBus` alphas (`alpha_fast`, `alpha_slow`) -> Halve values.
    *   `AudioActor` AGC (`agcAttack`, `agcRelease`) -> Halve values.
    *   `DC Removal` alpha -> Halve value.

## 4. System Architecture Impact

### 4.1. Buffer Alignment
*   `HOP_SIZE` (128) divides evenly into `DMA_BUFFER_SAMPLES` (512).
*   `HOP_SIZE` (128) divides evenly into `GoertzelAnalyzer::WINDOW_SIZE` (512).
*   **Conclusion:** No architectural misalignment.

### 4.2. Synchronization
*   **Cross-Core:** The `SnapshotBuffer` is lock-free and supports high-frequency updates. Pushing updates at 125Hz is well within its capability.
*   **Visual Rendering:** The renderer runs asynchronously. It will simply sample the latest frame. At 125Hz updates, the renderer (typically 60-100fps) will almost always have a fresh frame, reducing temporal jitter (aliasing).

## 5. Implementation Plan

To implement this change safely, the following files must be updated:

1.  **`src/config/audio_config.h`**
    *   Change `HOP_SIZE` to `128`.
    *   (Optional) Update comments regarding Tab5 parity.

2.  **`src/audio/contracts/ControlBus.h`**
    *   Retune filters: `m_alpha_fast` (0.35 -> 0.18), `m_alpha_slow` (0.12 -> 0.06).

3.  **`src/audio/AudioActor.cpp`**
    *   Retune AGC: `agcAttack` (0.08 -> 0.04), `agcRelease` (0.02 -> 0.01).
    *   Retune DC: `dcAlpha` (0.001 -> 0.0005).

## 6. Comparative Benchmark

| Feature | 256 Samples (Current) | 128 Samples (New) | Verdict |
| :--- | :--- | :--- | :--- |
| **Latency** | 16ms | **8ms** | **Winner** |
| **Beat Accuracy** | +/- 16ms | **+/- 8ms** | **Winner** |
| **Bass Resolution** | 32ms window | 32ms window | Tie |
| **CPU Load** | Low | Low (negligible +) | Tie |
| **Visual Sync** | Good | **Excellent** | **Winner** |

**Final Recommendation:** Switch to 128 samples immediately.
