# LightwaveOS Closed-Loop Quality Evaluation & Optimisation System

**Design Document v0.1 — March 2026**
**Status: DRAFT — For Discussion**

---

## 1. Core Problem

LightwaveOS has 350+ visual effects rendering at 120 FPS on dual WS2812 strips (320 LEDs), driven by a sophisticated audio analysis pipeline (8-band Goertzel, beat tracking, chord detection, style classification). There is currently no automated way to:

1. **Measure** whether a visual effect "looks good" in response to music
2. **Compare** two firmware builds or parameter sets with objective evidence
3. **Iterate** on effect parameters or rendering code with a feedback signal
4. **Regress-detect** when a change degrades visual quality

The capture streaming protocol (v2, 1008 bytes/frame) digitises the LED output, closing the perception gap between physical hardware and automated evaluation. This design exploits that to build a closed-loop system: **CAPTURE → EVALUATE → PROPOSE → APPLY → RE-CAPTURE**.

---

## 2. System Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                        ORCHESTRATOR (Python)                        │
│                                                                     │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────────────┐ │
│  │ CAPTURE  │──→│ EVALUATE │──→│ PROPOSE  │──→│ APPLY & VERIFY   │ │
│  │  Engine  │   │  Engine  │   │  Engine  │   │                  │ │
│  └────┬─────┘   └────┬─────┘   └────┬─────┘   └────────┬─────────┘ │
│       │              │              │                   │           │
│       │   ┌──────────┴──────────┐   │          ┌───────┴────────┐  │
│       │   │   METRIC PIPELINE   │   │          │  MODIFICATION  │  │
│       │   │                     │   │          │    CHANNELS    │  │
│       │   │  L0: Frame Metrics  │   │          │                │  │
│       │   │  L1: Temporal       │   │          │  Serial cmds   │  │
│       │   │  L2: Audio-Visual   │   │          │  Effect params │  │
│       │   │  L3: Perceptual     │   │          │  Source patches │  │
│       │   │  L4: Aesthetic      │   │          │  Build+Flash   │  │
│       │   └─────────────────────┘   │          └────────────────┘  │
│       │                             │                              │
└───────┼─────────────────────────────┼──────────────────────────────┘
        │                             │
        │  USB Serial (1008 B/frame)  │  USB Serial (commands)
        ▼                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     ESP32-S3 (LightwaveOS Firmware)                 │
│                                                                     │
│  AudioActor ──→ ControlBus ──→ RendererActor ──→ FastLedDriver     │
│  (Core 0)       (lock-free)    (Core 1, 120fps)  (WS2812 output)   │
│                                      │                              │
│                              Capture Taps A/B/C                     │
│                              (v2 binary stream)                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 3. Stage 1: CAPTURE Engine

### 3.1 Purpose

Ingest the binary capture stream from the ESP32, decode frames, and produce structured frame sequences for evaluation.

### 3.2 Firmware-Side (Already Exists)

The capture streaming is implemented in `RendererActor::onTick()` with three tap points:

| Tap | Location | Use Case |
|-----|----------|----------|
| A | Post-effect, pre-colour-correction | Raw effect output evaluation |
| B | Post-colour-correction, pre-show | Final visual output (recommended default) |
| C | Pre-WS2812 DMA | Hardware verification |

**Binary Protocol (v2):**
- 16-byte header: sync (0xFD), version, tap, effectId, paletteId, brightness, speed, frameIndex (u32), timestampUs (u32), frameLen (u16)
- 960-byte RGB payload: 320 LEDs × 3 bytes
- 32-byte metrics trailer: showUs (u16), rms (u16 fixed-point), bands[8] (u8 each), reserved[20]

**Control commands** (serial):
- `capture stream b 30` — Start streaming Tap B at 30 FPS
- `capture fps 60` — Change rate
- `capture stop` — Stop streaming
- `capture format v2` — Ensure metrics trailer

### 3.3 Python-Side (To Build: `led_capture.py`)

```python
class CaptureEngine:
    """Receives, decodes, and stores LED frame streams."""

    def connect(self, port: str, baud: int = 921600) -> None
    def start_stream(self, tap: str = 'b', fps: int = 30) -> None
    def stop_stream(self) -> None

    # Frame access
    def get_frame(self) -> CapturedFrame | None          # Latest frame
    def get_sequence(self, n: int) -> list[CapturedFrame] # Last N frames
    def record(self, duration_s: float) -> Recording      # Timed capture

    # Audio injection (deterministic input)
    def inject_audio_file(self, wav_path: str) -> None    # Future: WAV injection
```

**CapturedFrame** data model:
```python
@dataclass
class CapturedFrame:
    index: int                    # Frame counter from firmware
    timestamp_us: int             # Firmware microsecond timestamp
    tap: str                      # 'a', 'b', or 'c'
    effect_id: int
    palette_id: int
    brightness: int
    speed: int
    rgb: np.ndarray               # Shape (320, 3), dtype uint8
    metrics: FrameMetrics | None  # v2 trailer (rms, bands, showUs)
    capture_time: float           # Host-side time.monotonic()
```

**Recording** is a sequence of CapturedFrames with metadata (effect, duration, audio source hash).

### 3.4 Deterministic Audio Injection (Phase 2)

[HYPOTHESIS] For reproducible evaluation, we need deterministic audio input. Three strategies identified during firmware inspection:

| Strategy | Mechanism | Effort | Determinism |
|----------|-----------|--------|-------------|
| WAV file injection | New `WavFileAudioCapture` implementing `IAudioCapture` | ~100 LOC | Perfect |
| I2S slave loopback | ESP32 I2S TX → I2S RX on same chip | Medium | High |
| Acoustic injection | Speaker → mic in enclosure | Zero firmware | Low |

**Recommended: WAV file injection** via a new HAL implementation. The `IAudioCapture` interface (`captureHop(int16_t* buffer)`) is a clean seam — a `WavFileAudioCapture` reads from SPIFFS/LittleFS and serves hop-sized chunks at the correct rate. Serial command: `audio inject /test_tracks/kick_pattern.wav`.

### 3.5 Reference Audio Library

A curated set of test audio files covering distinct musical characteristics:

| Track ID | Description | Tests |
|----------|-------------|-------|
| `silence.wav` | Digital silence | Baseline, idle behaviour, silence detection |
| `60bpm_kick.wav` | 60 BPM kick drum only | Beat alignment, pulse response |
| `120bpm_edm.wav` | 120 BPM four-on-floor | Full-spectrum reactivity |
| `sine_sweep.wav` | 20 Hz → 8 kHz sweep | Band response, frequency tracking |
| `pink_noise.wav` | Calibrated pink noise | Energy distribution, smoothing behaviour |
| `jazz_trio.wav` | Complex harmonic content | Chord detection, style classification |
| `metal_blast.wav` | High-energy broadband | Clipping resilience, dynamic range |
| `ambient_pad.wav` | Slow evolving texture | Smooth response, drift behaviour |

---

## 4. Stage 2: EVALUATE Engine (Metric Pipeline)

### 4.1 Metric Hierarchy

Metrics are layered by computational cost and information density. Lower layers are cheap and fast; higher layers provide richer evaluation but cost more.

#### L0: Frame-Level Metrics (Per-Frame, Real-Time)

Computed from individual frames with zero temporal context. These are the foundation — if L0 is broken, nothing above matters.

| Metric | Formula | What It Catches | Source |
|--------|---------|-----------------|--------|
| **Mean brightness** | `mean(R+G+B) / 3` | Dead LEDs, blown-out frames | Basic |
| **Colour entropy** | Shannon entropy of RGB histogram | Monotone vs. rich palette use | Basic |
| **Spatial variance** | `std(brightness across 320 LEDs)` | Flat fills vs. structured patterns | Basic |
| **Centre-edge gradient** | `mean(centre 40 LEDs) - mean(edge 40 LEDs)` | Centre-origin pattern verification | LightwaveOS-specific |
| **Strip symmetry** | `corr(strip1[0:160], strip2[0:160])` | Dual-strip coherence | LightwaveOS-specific |
| **Black frame ratio** | `count(all_zeros) / total_frames` | Rendering failures, blank output | Basic |

#### L1: Temporal Metrics (Window-Based, 0.5–5s windows)

Computed across frame sequences. These capture the "motion" quality of the visualisation.

| Metric | Formula | What It Catches | Source |
|--------|---------|-----------------|--------|
| **Temporal flickering** | Frame-to-frame L2 distance, flag if δ > threshold for >3 consecutive frames | Jitter, unstable rendering | VBench (CVPR 2024) |
| **Motion smoothness** | Autocorrelation of frame-diff magnitude at lag 1–5 | Jerky vs. fluid animation | WCS (arXiv:2508.00144) |
| **Dynamic range** | `max(brightness_over_window) - min(brightness_over_window)` | Flat/lifeless vs. responsive | Basic |
| **Palette drift rate** | Dominant hue shift per second | Colour transitions, stuck palettes | Basic |
| **Temporal entropy** | Shannon entropy of frame-diff distribution | Repetitive vs. varied motion | Adapted from video quality |

#### L2: Audio-Visual Alignment (Cross-Modal, Per-Beat/Per-Hop)

These are the core "does it match the music?" metrics. They require both the RGB stream and the audio metrics trailer.

| Metric | Formula | What It Catches | Priority |
|--------|---------|-----------------|----------|
| **BeatAlign** | Cross-correlation of beat timestamps with brightness peaks, max over ±3 frame offsets | Beat-pulse synchronisation | **HIGHEST** |
| **corr(\|Δ\|)** | Pearson correlation of `abs(diff(audio_rms))` with `abs(diff(frame_brightness))` | Overall audio-visual change correlation | **HIGH** |
| **Band-colour mapping** | Mutual information between 8-band energy and per-region hue | Frequency-to-colour fidelity | Medium |
| **Onset response latency** | Time from audio onset to first brightness peak (should be <2 frames at 120fps = <17ms) | Latency, sluggish response | **HIGH** |
| **Silence behaviour** | Frame variance during silence (should be low, graceful idle) | Noisy idle, failure to detect silence | Medium |

**BeatAlign implementation detail:**
```python
def beat_align(beat_times: np.ndarray, brightness_curve: np.ndarray,
               fps: float, max_offset_frames: int = 3) -> float:
    """
    Cross-correlate beat onsets with brightness peaks.
    Returns: correlation score in [0, 1].
    """
    beat_signal = np.zeros(len(brightness_curve))
    for bt in beat_times:
        frame_idx = int(bt * fps)
        if 0 <= frame_idx < len(beat_signal):
            beat_signal[frame_idx] = 1.0

    brightness_diff = np.abs(np.diff(brightness_curve, prepend=0))
    brightness_diff /= (brightness_diff.max() + 1e-8)

    best_corr = 0.0
    for offset in range(-max_offset_frames, max_offset_frames + 1):
        shifted = np.roll(brightness_diff, offset)
        corr = np.corrcoef(beat_signal, shifted)[0, 1]
        best_corr = max(best_corr, corr)

    return best_corr
```

#### L3: Perceptual Similarity (Comparison Mode)

Used in A/B comparisons and regression detection. Requires a **baseline recording** to compare against.

| Metric | Formula | What It Catches | Source |
|--------|---------|-----------------|--------|
| **Wasserstein distance** | Earth mover's distance between frame RGB distributions | Distribution-level drift between builds | Standard |
| **Frame-level SSIM** | Structural similarity on 320×1 → 320×16 upscaled strip image | Structural pattern changes | Standard |
| **DINOv1 embedding distance** | Cosine distance in DINO feature space (strip rendered as image) | Deep perceptual similarity | DreamSim (arXiv) |

**Strip-to-image rendering** for perceptual metrics:
The 320-LED strip is rendered as a 320×16 pixel image (16 pixel height for DINO/SSIM to have spatial structure). Multiple frames are stacked vertically to create a spatiotemporal image (e.g., 60 frames → 320×60 image = 2 seconds at 30 FPS capture).

#### L4: Aesthetic Judgement (MLLM-as-Judge)

The highest layer. Uses multimodal LLM (Claude) to evaluate rendered frame sequences against quality criteria. This is the "shared perception" pathway.

**Input:** Rendered strip images (spatiotemporal montage — 320 pixels wide × N frames tall, colour-coded).

**Evaluation prompt template:**
```
You are evaluating a music visualisation rendered on a 320-LED strip.
The image shows {duration}s of output at {fps} FPS.
The audio characteristics are: {audio_description}.
The effect is: {effect_name} (category: {category}).

Rate the following on a 1-10 scale:
1. Beat responsiveness: Does brightness/colour clearly pulse with beats?
2. Frequency mapping: Do different frequency bands produce distinct visual regions?
3. Motion quality: Is the animation smooth, or does it flicker/jerk?
4. Dynamic range: Does it use the full brightness/colour range appropriately?
5. Aesthetic coherence: Does it "look good" as a unified visual?

Provide scores and a 2-sentence rationale for each.
```

**Cost control:** L4 is expensive (API call per evaluation). It runs only:
- On demand (manual quality review)
- At the end of an optimisation cycle (final judgement)
- For regression detection on flagged builds

---

## 5. Stage 3: PROPOSE Engine

### 5.1 Purpose

Given evaluation results (metrics + optional aesthetic judgement), determine what modifications would improve quality and generate concrete change proposals.

### 5.2 Proposal Types

| Type | Mechanism | Latency | Reversibility |
|------|-----------|---------|---------------|
| **Parameter tweak** | Serial command: `brightness 128`, `speed 50`, `param X Y Z` | Instant (next frame) | Instant |
| **Effect switch** | Serial command: `effect set <id>` | Instant | Instant |
| **Audio mapping change** | Modify `AudioEffectMapping` registry, rebuild + flash | ~30s (compile+flash) | Rebuild |
| **Effect code change** | Modify effect `.h` file, rebuild + flash | ~30s | Rebuild |
| **Pipeline parameter** | Modify `ControlBus` smoothing, attack/release, etc. | ~30s | Rebuild |

### 5.3 Proposal Generation Logic

```python
class ProposalEngine:
    def generate(self, eval_result: EvalResult,
                 baseline: EvalResult | None) -> list[Proposal]:
        proposals = []

        # L0 failures: immediate parameter fixes
        if eval_result.mean_brightness < 10:
            proposals.append(ParamTweak('brightness', 128, reason='Near-black output'))
        if eval_result.black_frame_ratio > 0.1:
            proposals.append(Escalation('effect_crash', 'Effect producing >10% black frames'))

        # L2 failures: audio-visual alignment
        if eval_result.beat_align < 0.3:
            proposals.append(ParamTweak('intensity', 255, reason='Weak beat response'))
            proposals.append(CodeChange(
                target='audio_mapping',
                description='Increase RMS→brightness coupling',
                priority='high'
            ))

        # L1 failures: temporal quality
        if eval_result.temporal_flickering > 0.5:
            proposals.append(ParamTweak('mood', 200, reason='Flickering, increase smoothing'))

        # Regression detection (vs baseline)
        if baseline and eval_result.wasserstein > baseline.wasserstein * 1.5:
            proposals.append(RegressionAlert(
                metric='wasserstein_distance',
                baseline_val=baseline.wasserstein,
                current_val=eval_result.wasserstein
            ))

        return sorted(proposals, key=lambda p: p.priority)
```

### 5.4 Modification Channels (Firmware Integration Points)

These are the "levers" the system can pull:

**Channel 1: Runtime Parameters (Serial Commands)**
```
brightness <0-255>        # Global brightness
speed <1-100>             # Effect animation speed
intensity <0-255>         # Effect intensity
saturation <0-255>        # Colour saturation
complexity <0-255>        # Pattern complexity
mood <0-255>              # Reactive↔smooth (Sensory Bridge parity)
effect set <id>           # Switch active effect
palette set <id>          # Switch colour palette
param <effect> <name> <v> # Effect-specific parameter
```

**Channel 2: Build-Time Parameters (Recompile Required)**
- `audio_config.h`: Sample rate, hop size, band frequencies
- `AudioEffectMapping.h`: Audio→parameter mapping rules
- `ControlBus.h`: Smoothing coefficients, attack/release
- `ColorCorrectionEngine`: Tone mapping curves

**Channel 3: Effect Code (Recompile Required)**
- Individual effect `.h` files in `src/effects/ieffect/`
- Render function logic, colour calculations, animation curves
- 350+ effects, each self-contained in a single header

**Channel 4: Firmware Build+Flash Pipeline**
```python
class FirmwarePipeline:
    def build(self, env: str = 'esp32s3') -> BuildResult:
        """PlatformIO build. Returns success/failure + binary path."""

    def flash(self, device: str, binary: str) -> FlashResult:
        """Flash firmware to device over USB serial."""

    def build_and_flash(self, device: str) -> bool:
        """Build + flash + wait for boot confirmation."""
```

---

## 6. Stage 4: APPLY & VERIFY

### 6.1 Application Strategy

Proposals are applied in order of increasing cost/risk:

1. **Parameter tweaks first** — instant, no rebuild, fully reversible
2. **Effect switches** — instant, test alternative effects for the same audio
3. **Code changes** — require rebuild+flash (~30s), test in isolation
4. **Pipeline changes** — require rebuild+flash, affect ALL effects (highest risk)

### 6.2 Verification Protocol

After each modification:

1. Wait for stabilisation (500ms for parameter changes, 3s for flash+boot)
2. Capture a recording of the same duration with the same audio input
3. Run the evaluation pipeline on the new recording
4. Compare metrics against pre-modification baseline
5. Decision: **KEEP** (metrics improved) or **REVERT** (metrics degraded)

```python
class VerificationResult:
    improved_metrics: list[str]   # Metrics that got better
    degraded_metrics: list[str]   # Metrics that got worse
    unchanged_metrics: list[str]  # Within noise threshold
    verdict: str                  # 'keep', 'revert', 'inconclusive'
    confidence: float             # 0-1 based on metric agreement
```

### 6.3 Rollback Strategy

- **Parameter tweaks**: Store previous values, restore on revert
- **Code changes**: Git stash/branch, revert to previous commit
- **Multi-step optimisation**: Checkpoint after each successful step, rollback to last checkpoint on failure

---

## 7. Orchestrator: Tying It All Together

### 7.1 Session Concept

An optimisation session is a bounded run with a specific goal:

```python
@dataclass
class OptimisationSession:
    goal: str                      # e.g., "Improve beat alignment for effect 42"
    target_effect_id: int | None   # None = all effects
    audio_tracks: list[str]        # Reference audio files to test with
    baseline: Recording | None     # Previous best to compare against
    max_iterations: int = 10       # Safety limit
    metric_targets: dict           # e.g., {'beat_align': 0.7, 'temporal_flickering': 0.2}
```

### 7.2 Main Loop

```python
def run_session(session: OptimisationSession) -> SessionReport:
    engine = CaptureEngine(port=session.device_port)
    evaluator = EvaluateEngine(metrics=ALL_METRICS)
    proposer = ProposalEngine()

    # Step 0: Establish baseline
    if session.baseline is None:
        engine.start_stream(tap='b', fps=30)
        for track in session.audio_tracks:
            engine.inject_audio(track)
            baseline_recording = engine.record(duration_s=track.duration)
            session.baseline = evaluator.evaluate(baseline_recording)

    iteration = 0
    best_score = session.baseline.composite_score()

    while iteration < session.max_iterations:
        iteration += 1

        # CAPTURE current state
        recording = engine.record(duration_s=10)
        eval_result = evaluator.evaluate(recording)

        # Check if targets met
        if eval_result.meets_targets(session.metric_targets):
            return SessionReport(status='targets_met', iterations=iteration)

        # PROPOSE modifications
        proposals = proposer.generate(eval_result, session.baseline)
        if not proposals:
            return SessionReport(status='no_proposals', iterations=iteration)

        # APPLY top proposal
        proposal = proposals[0]
        proposal.apply(engine)

        # VERIFY
        time.sleep(proposal.stabilisation_time)
        verify_recording = engine.record(duration_s=10)
        verify_result = evaluator.evaluate(verify_recording)

        if verify_result.composite_score() > best_score:
            best_score = verify_result.composite_score()
            session.baseline = verify_result  # New baseline
        else:
            proposal.revert(engine)  # Rollback

    return SessionReport(status='max_iterations', iterations=iteration)
```

### 7.3 Composite Scoring

Individual metrics are combined into a single composite score for optimisation direction:

```python
METRIC_WEIGHTS = {
    'beat_align': 0.25,           # Highest priority: does it match the beat?
    'corr_abs_delta': 0.20,       # Audio-visual correlation
    'onset_response_latency': 0.15,
    'temporal_flickering': 0.10,  # Inverted: lower is better
    'motion_smoothness': 0.10,
    'colour_entropy': 0.05,
    'dynamic_range': 0.05,
    'spatial_variance': 0.05,
    'silence_behaviour': 0.05,
}

def composite_score(metrics: dict) -> float:
    """Weighted sum normalised to [0, 1]. Higher is better."""
    score = 0.0
    for name, weight in METRIC_WEIGHTS.items():
        val = metrics.get(name, 0.0)
        if name in INVERTED_METRICS:  # Lower-is-better metrics
            val = 1.0 - val
        score += weight * min(val, 1.0)
    return score
```

---

## 8. Operational Modes

### 8.1 Mode: Regression Gate (CI/CD)

Runs automatically on every firmware build. Binary pass/fail.

```
Trigger: New commit pushed / PR opened
Input:   Reference audio tracks + golden baseline recordings
Process: Build → Flash → Capture → Evaluate → Compare to baseline
Output:  PASS (all metrics within tolerance) / FAIL (regression detected)
Time:    ~2 minutes per device
```

**Pass/fail thresholds (configurable per metric):**
```python
REGRESSION_THRESHOLDS = {
    'beat_align': -0.05,        # Max allowed degradation
    'temporal_flickering': 0.1, # Max allowed increase
    'wasserstein': 0.15,        # Max distribution shift
    'black_frame_ratio': 0.01,  # Max 1% black frames
}
```

### 8.2 Mode: Effect Audit (Batch)

Evaluates all 350+ effects against the reference audio library. Produces a quality report.

```
Trigger: Manual / weekly schedule
Input:   Full audio library × all registered effects
Process: For each (effect, track) pair: set effect → inject audio → capture → evaluate
Output:  Quality matrix (350 effects × 8 tracks × N metrics)
Time:    ~4 hours (350 effects × 8 tracks × 10s each = ~7.8 hours capture, parallelisable with 2 devices)
```

### 8.3 Mode: Optimisation (Interactive)

The full closed-loop. Developer sets a goal, system iterates toward it.

```
Trigger: Developer initiates session
Input:   Target effect + audio tracks + metric targets
Process: Iterative CAPTURE → EVALUATE → PROPOSE → APPLY → VERIFY
Output:  Optimised parameters / code changes + before/after evidence
Time:    5-30 minutes depending on iteration count
```

### 8.4 Mode: A/B Comparison (Dual-Device)

Uses two K1 devices (from the multi-worktree guide) to compare two firmware builds simultaneously.

```
Trigger: Developer wants to compare branches
Input:   Branch A (device 1) + Branch B (device 2) + same audio input
Process: Simultaneous capture from both devices → evaluate both → diff
Output:  Per-metric comparison with statistical significance
Time:    ~2 minutes
```

---

## 9. File & Module Structure

```
tools/
├── lightwave_eval/                 # Python package
│   ├── __init__.py
│   ├── capture/
│   │   ├── engine.py               # CaptureEngine (serial decode, frame buffer)
│   │   ├── protocol.py             # V2 binary protocol parser
│   │   ├── recording.py            # Recording data model + persistence
│   │   └── audio_injector.py       # Reference audio management
│   ├── evaluate/
│   │   ├── engine.py               # EvaluateEngine (metric orchestration)
│   │   ├── metrics_l0.py           # Frame-level metrics
│   │   ├── metrics_l1.py           # Temporal metrics
│   │   ├── metrics_l2.py           # Audio-visual alignment (BeatAlign, corr|Δ|)
│   │   ├── metrics_l3.py           # Perceptual similarity (Wasserstein, SSIM)
│   │   ├── metrics_l4.py           # MLLM aesthetic judgement
│   │   ├── composite.py            # Weighted composite scoring
│   │   └── strip_renderer.py       # LED buffer → image conversion
│   ├── propose/
│   │   ├── engine.py               # ProposalEngine
│   │   ├── proposals.py            # Proposal types (ParamTweak, CodeChange, etc.)
│   │   └── rules.py                # Heuristic rules for proposal generation
│   ├── apply/
│   │   ├── serial_channel.py       # Runtime parameter commands
│   │   ├── build_channel.py        # PlatformIO build + flash
│   │   └── rollback.py             # Checkpoint + revert logic
│   ├── orchestrator/
│   │   ├── session.py              # OptimisationSession
│   │   ├── modes.py                # Regression gate, audit, optimisation, A/B
│   │   └── report.py               # Session reporting + evidence
│   └── baseline/
│       ├── manager.py              # Golden baseline storage + versioning
│       └── comparator.py           # Baseline diff logic
├── reference_audio/                # Curated test audio files
│   ├── silence.wav
│   ├── 60bpm_kick.wav
│   └── ...
├── baselines/                      # Golden recordings (git-tracked)
│   ├── v3.2.0/
│   │   ├── effect_42_kick.recording
│   │   └── ...
│   └── latest -> v3.2.0
└── scripts/
    ├── regression_gate.py          # CI entrypoint
    ├── effect_audit.py             # Batch audit entrypoint
    ├── optimise.py                 # Interactive optimisation CLI
    └── compare.py                  # A/B comparison CLI
```

---

## 10. Implementation Phases

### Phase 0: Foundation (Week 1)

**Deliverables:** Working capture pipeline, frame rendering, L0 metrics.

1. Build `led_capture.py` — serial protocol parser + CaptureEngine
2. Build `strip_renderer.py` — LED buffer → PNG image conversion
3. Implement L0 metrics (mean brightness, colour entropy, spatial variance, black frame ratio)
4. Manual verification: capture real device output, render images, confirm sanity

**Validation:** Capture 10 seconds from a known effect, render to image, visually confirm the image matches expected output. L0 metrics should be non-zero and sensible.

### Phase 1: Temporal + Audio-Visual Metrics (Week 2)

**Deliverables:** L1 + L2 metrics, baseline management.

1. Implement L1 temporal metrics (flickering, motion smoothness, dynamic range)
2. Implement L2 audio-visual metrics (BeatAlign, corr|Δ|, onset response latency)
3. Build baseline manager (save/load/compare recordings)
4. Build composite scoring with configurable weights

**Validation:** Run against `60bpm_kick.wav` — BeatAlign should be measurably higher for beat-reactive effects (e.g., BeatPulseBloom) than for ambient effects (e.g., PerlinAmbient). If not, the metric is broken.

### Phase 2: Proposal Engine + Serial Channel (Week 3)

**Deliverables:** Working closed-loop for parameter tweaks.

1. Build ProposalEngine with heuristic rules
2. Build serial command channel (parameter tweaks, effect switches)
3. Build verification protocol (apply → wait → capture → compare)
4. Implement simple optimisation session for single-effect parameter tuning

**Validation:** Start with a deliberately suboptimal brightness (e.g., 20). System should propose increasing brightness, apply it, verify improvement, and converge to a better value.

### Phase 3: Build Pipeline + Regression Gate (Week 4)

**Deliverables:** Automated build+flash, regression detection.

1. Build PlatformIO build+flash channel
2. Implement regression gate mode (binary pass/fail)
3. Implement A/B comparison mode (dual-device)
4. Build deterministic audio injection (`WavFileAudioCapture` HAL)

**Validation:** Intentionally break an effect (e.g., comment out beat response), run regression gate — it should FAIL with specific metric degradation identified.

### Phase 4: Perceptual + Aesthetic Layers (Week 5-6)

**Deliverables:** L3 + L4 metrics, effect audit mode.

1. Implement L3 perceptual metrics (Wasserstein, SSIM on strip images)
2. Implement L4 MLLM aesthetic judgement (Claude API)
3. Build effect audit mode (batch evaluation of all 350+ effects)
4. Generate quality report with rankings and recommendations

**Validation:** L4 aesthetic scores should correlate (r > 0.6) with L2 audio-visual metrics. If they diverge wildly, either the prompt or the metrics need calibration.

### Phase 5: Full Optimisation Loop (Week 7+)

**Deliverables:** Code-level proposal generation, full autonomous optimisation.

1. Extend ProposalEngine to generate code changes (effect modifications)
2. Implement git-based rollback for code changes
3. Full optimisation session with code+parameter iteration
4. Effect-specific tuning profiles (optimal parameters per audio genre)

**Validation:** Take a mediocre effect, run optimisation for 10 iterations, measure composite score improvement. Target: >20% improvement or convergence to target.

---

## 11. Hardware Requirements

### Minimum Viable

- 1× ESP32-S3 dev board (K1 v1 or v2) with WS2812 strips (LEDs optional for headless)
- USB serial connection to host machine
- Host machine with Python 3.10+, PlatformIO

### Recommended

- 2× ESP32-S3 dev boards (for A/B comparison)
- Audio injection capability (WAV file via SPIFFS, or I2S loopback)
- LEDs connected on at least one device (for visual sanity checks)

### Headless Operation

[FACT] WS2812 is unidirectional — the protocol sends data TO LEDs, never reads back.
[FACT] `FastLedDriver::show()` calls `syncBuffersToFastLED()` then `FastLED.show()` — the internal RGB buffer is fully populated before either call.
[FACT] Capture taps read from the internal buffer, not from hardware output.
[INFERENCE] LEDs are electrically unnecessary for all automated evaluation. The RMT peripheral generates the waveform identically with nothing connected to the GPIO pins.

---

## 12. Open Questions for Discussion

1. **Composite weight tuning:** The initial weights (BeatAlign 0.25, corr|Δ| 0.20, etc.) are hypotheses. How do we calibrate them? Options: manual tuning against known-good effects, or use L4 (Claude judgement) as ground truth to learn weights.

2. **Audio injection priority:** Should we build `WavFileAudioCapture` in Phase 1 (enables deterministic evaluation from the start) or Phase 3 (defer complexity, use real mic initially)?

3. **Scope of code-level proposals:** Should the system generate actual effect code modifications, or only identify WHAT needs changing (leaving implementation to the developer/Claude)?

4. **Effect audit granularity:** 350 effects × 8 tracks × 10s = ~7.8 hours. Accept this runtime, or sample a subset? Could parallelise across 2 devices for ~4 hours.

5. **Baseline versioning:** Should golden baselines be git-tracked (large binary files) or stored externally with content-hash references?

6. **L4 cost management:** Claude API calls for aesthetic judgement are non-trivial cost at scale. Rate-limit to N calls per session? Only on regression flags?

---

## 13. Risk Register

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| Serial capture drops frames at high FPS | Metric accuracy degraded | Medium | Frame sequence numbers detect gaps; interpolate or discard window |
| BeatAlign metric doesn't correlate with perceived quality | Entire L2 layer unreliable | Medium | Calibrate against L4 (Claude judgement); swap for alternative metric |
| Build+flash cycle too slow for tight loop | Iteration speed bottleneck | Low | Prioritise parameter tweaks (instant); batch code changes |
| Audio injection adds latency vs. real mic | Timing metrics skewed | Low | Measure injection latency, compensate in timestamp alignment |
| Effect render time varies, frame capture misses budget | Inconsistent capture rate | Low | Capture at 30 FPS (33ms budget), render at 120 FPS (8.3ms) — 4× margin |
| MLLM aesthetic judgement inconsistent across calls | L4 unreliable as ground truth | Medium | Multiple evaluations per sample, take median; use structured rubric |

---

*End of design document. Ready for discussion.*
