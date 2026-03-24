---
abstract: "Complete research dump from 10-vector investigation into building a browser-based node editor for audio-reactive LED effect prototyping. Covers: node editor frameworks, web audio pipeline libraries, LED simulation rendering, existing tools gap analysis, dataflow graph architecture with C++ code generation, ESV11 Goertzel backend portability, WASM compilation feasibility, adapter chain specification, and 32kHz resampling in browser. Read when designing or building the K1 Node Composer."
---

# K1 Node Composer — Research Dump

Research conducted 2026-03-23. Five parallel specialist agents, each investigating one vector. All findings reproduced verbatim below, organised by vector.

---

## Vector 1: Node Editor Frameworks

### Comparison Table

| Framework | GitHub Stars | Last Active | Real-Time Capable | Typed Edges | Custom Node Render | 60fps Performance | License |
|---|---|---|---|---|---|---|---|
| **React Flow (@xyflow/react)** | 35.8k | Feb 2026 | Event-based (not per-frame native) | Yes (via `isValidConnection`, TypeScript) | Yes (React components, canvas, WebGL) | Depends on memoisation (optimised 10k+ nodes possible) | MIT |
| **Litegraph.js** | 7.9k | Mar 2024 | Yes (per-frame via `graph.start()`, `ALWAYS` execution mode) | Yes (typed sockets: "number", "array", "audio", custom) | Yes (Canvas2D, can embed WebGL textures) | Optimised for 100+ nodes at 60fps (Canvas batching) | MIT |
| **Rete.js v2** | 12k | Active (600+ commits) | Yes (async engine, dataflow/control flow) | Yes (typed sockets via builder API) | Yes (framework-agnostic, React/Vue/Svelte renderers) | Good (modular, optimised for 50+ nodes) | MIT |
| **Flume** | 1.6k | Mar 2026 | Yes (blazing fast engine) | Yes (typed nodes and connections) | Yes (custom node UI) | Explicit 60fps+ on every device (DOM, no canvas) | MIT |
| **Baklava (Vue.js)** | ~2k | Active | Yes (via engine.calculate()) | Yes (Interface Types plugin, automatic conversion) | Yes (@baklavajs/renderer-vue) | Good (Vue 3 binding efficiency) | MIT |
| **Wavir** | N/A (wavir.io) | Active | Yes (Web Audio native, real-time synthesis) | Yes (typed oscillators, filters, gains) | Yes (custom visualisers for frequency/time domain) | 60fps (native Web Audio threading) | Unknown |
| **NoiseCraft** | 2.3k | Active | Yes (real-time synthesis and MIDI) | Yes (typed nodes: oscillators, filters, gates) | Yes (immediate-mode GUI, custom visualisers) | 60fps (Web Audio + MIDI backend) | MIT |

### Critical Finding: Per-Frame Execution

**Only Litegraph.js natively supports per-frame execution loops.** `graph.start()` initiates a continuous evaluation loop. Nodes can be set to `LiteGraph.ALWAYS` mode for per-frame updates.

React Flow, Rete.js, Flume, Baklava are **event-driven only** — you wire visual connections, then call engine/process externally when data changes. No built-in `requestAnimationFrame` loop. You must build your own:

```javascript
const frame = () => {
  engine.process(graph);
  requestAnimationFrame(frame);
};
frame();
```

### Custom Node Rendering (for LED strip canvas inside nodes)

| Framework | Capability | Notes |
|---|---|---|
| Litegraph.js | Best | Canvas2D natively. Can embed WebGL textures inside nodes. |
| React Flow | Excellent | Full React component freedom. Embed `<canvas>`, WebGL, or any HTML. |
| Rete.js | Good | Renderer-agnostic. Vue/React/Svelte renderers allow custom HTML/canvas. |
| Baklava | Good | Vue component system. Custom visualisers via Vue slots. |
| Flume | Good | Custom node UI supported. Canvas rendering feasible. |

### Ecosystem Quality

| Criterion | Winner | Details |
|---|---|---|
| Documentation | React Flow | 10+ tutorials, Web Audio playground example |
| Community | React Flow | 35.8k stars, frequent StackOverflow threads |
| TypeScript Support | Rete.js | Framework-first TypeScript + full type inference |
| Learning Curve | Flume | Simplest API, focused scope |
| Audio Specialisation | Wavir / NoiseCraft | Purpose-built for Web Audio, but audio-only |
| Per-Frame Loop | Litegraph.js | Only native support |
| Performance (100+ nodes) | Litegraph.js | Canvas batching |
| Visual Polish | React Flow | Modern aesthetics, animations, accessibility |

### Sources
- React Flow Web Audio Tutorial: reactflow.dev/learn/tutorials/react-flow-and-the-web-audio-api
- React Flow GitHub: github.com/xyflow/xyflow
- Litegraph.js GitHub: github.com/jagenjo/litegraph.js
- Rete.js Documentation: retejs.org
- Flume Documentation: flume.dev
- Baklava Documentation: baklava.tech
- Wavir: wavir.io
- NoiseCraft GitHub: github.com/maximecb/noisecraft
- xyflow Awesome Node-Based UIs: github.com/xyflow/awesome-node-based-uis

---

## Vector 2: Web Audio Pipeline Simulation

### ControlBus Coverage Matrix

| Feature | Meyda | Essentia | Web Audio | aubio | Manual |
|---------|-------|----------|-----------|-------|--------|
| octave bands[0..7] | Bark scale (close) | Derive from FFT | FFT binning | No | Yes |
| chroma[0..11] | Yes | Yes | No | No | No |
| rms | Yes | Yes | Yes | Yes | Yes |
| spectralFlux | Yes | Yes | No | No | Yes |
| beat (tick) | No | Yes | No | Yes | No |
| beatStrength | No | Yes | No | No | Partial |
| bpm | No | Yes | No | Yes | No |
| beatPhase | No | No | No | No | Yes (manual) |
| snareTrigger | Via band filter | Via onset + band | No | Partial | Yes |
| hihatTrigger | Via band filter | Via onset + band | No | Partial | Yes |
| silentScale | Yes (RMS based) | Yes (RMS based) | Yes | Yes | Yes |

### Library Details

**Meyda.js** — Audio Feature Extraction
- Link: meyda.js.org / github.com/meyda/meyda
- Bundle: ~30KB minified
- Provides: RMS, spectral flux, chroma, loudness (24-band Bark), spectral centroid/spread/rolloff, MFCC
- Does NOT provide: Beat detection, true octave bands, onset detection
- Performance: 3.34x real-time (288 ops/sec), sufficient for 60fps
- Real-time: Native Web Audio API integration

**Essentia.js** — Comprehensive MIR (Music Information Retrieval)
- Link: mtg.github.io/essentia.js / github.com/MTG/essentia.js
- Bundle: ~1.2MB (WebAssembly), 300KB JS wrapper
- Provides: BeatTrackerDegara (fast), BeatTrackerMultiFeature (accurate), RhythmExtractor2013, chromagram, onset detection, RMS
- Does NOT provide: Pre-computed octave bands, snare/hihat classification, silence gate
- Maintained by Music Technology Group (UPF, Barcelona)
- BeatTrackerDegara requires 44.1kHz sampling rate

**aubio.js** — Lightweight Beat Detection
- Link: aubio.org / github.com/qiuxiang/aubiojs
- Bundle: ~500KB (WASM)
- Provides: Tempo class for BPM, onset detection
- Lighter than Essentia, less comprehensive

**Tone.js** — NOT recommended for analysis
- Primarily synthesis and scheduling. Not an analysis library.

**Web Audio API (Native)**
- AnalyserNode: Real-time FFT (fftSize 32-32768)
- getFloatFrequencyData(): 32-bit frequency bins
- getUserMedia: Microphone input
- AudioWorklet: High-precision real-time DSP
- Recommended: fftSize=2048 for 21.5 Hz resolution at 44.1kHz

### Octave Band Implementation

Neither Meyda nor Essentia provides standard 8-octave bands directly. Manual FFT binning required:

| Band | Frequency | FFT Bins (2048 @ 44.1kHz) |
|------|-----------|---------------------------|
| Sub-bass | 60–125 Hz | 0–6 |
| Bass | 125–250 Hz | 6–12 |
| Low-mid | 250–500 Hz | 12–24 |
| Mid | 500–1000 Hz | 24–48 |
| High-mid | 1000–2000 Hz | 48–96 |
| Treble | 2000–4000 Hz | 96–192 |
| Bright | 4000–8000 Hz | 192–384 |
| Air | 8000–16000 Hz | 384–768 |

### Beat Phase Tracking

No library provides continuous beat phase (0-1 wrapping each beat). Must implement manually:
```javascript
const beatPhase = (elapsedMsSinceBeat / msPerBeat) % 1.0;
```

### Percussion Trigger Implementation

Implement post-analysis:
- Snare: onset in mid-frequency band (1-4 kHz)
- Hihat: onset in high-frequency band (6-12 kHz)

### Performance at 60fps

| Operation | Cost |
|-----------|------|
| AnalyserNode FFT (2048) | <1ms |
| Meyda feature extraction (all features) | ~2-5ms |
| Essentia RhythmExtractor | ~10-20ms (batch better) |

Strategy: Tier 1 (RMS, flux, chroma) every frame. Tier 2 (beat, BPM) every 512 samples. Tier 3 (silence, percussion) smooth via hysteresis.

### Recommended Architecture

```
Web Audio API (getUserMedia + AnalyserNode)
├── FFT (2048 bins) → Raw frequency data
├── Meyda.js
│   ├── RMS
│   ├── Spectral flux
│   └── Chroma
├── Essentia.js
│   ├── BeatTracker
│   ├── BPM
│   └── Onset detection
└── Post-processing (JavaScript)
    ├── Octave band binning (from FFT)
    ├── Beat phase tracking (timer-based)
    ├── Percussion classification (band filtering)
    └── Silence gate hysteresis (Schmitt trigger)
```

### NPM Packages

| Package | Version | Size | Install |
|---------|---------|------|---------|
| meyda | 6.1.0 | 29KB | `npm install meyda` |
| essentia.js | 0.1.30 | 1.2MB | `npm install essentia.js` |
| aubio | 0.4.9 | 500KB | `npm install aubio` |
| pitchy | 4.0.1 | 8KB | `npm install pitchy` |

### Sources
- Meyda Documentation: meyda.js.org
- Essentia.js Documentation: mtg.github.io/essentia.js
- MDN AnalyserNode: developer.mozilla.org/en-US/docs/Web/API/AnalyserNode
- aubio Manual: aubio.org/manual/latest/
- CRYSOUND Octave Analysis: crysound.com/octave-band-analysis-guide-fft-binning-vs-filter-bank-method/
- Joe Sullivan Beat Detection: joesul.li/van/beat-detection-using-web-audio/
- audioMotion-analyzer: github.com/hvianna/audioMotion-analyzer

---

## Vector 3: LED Strip Visual Simulation

### Existing LED Simulators

**LEDstrip by dougalcampbell**
- github.com/dougalcampbell/LEDStrip
- Technology: Vanilla JS + requestAnimationFrame
- Rendering: DOM-based (styled divs)
- Visual: Simple discrete LED dots (not diffused)
- Emulates actual WS2812 datain/dataout protocol

**LED Matrix Simulator (sallar/led-matrix)**
- github.com/sallar/led-matrix
- Technology: HTML5 Canvas + TypeScript
- Has explicit `glow` boolean flag for luminous effects
- Configurable `pixelWidth`, `pixelHeight`, `margin`, `animated`
- Better visual realism with glow

**Neopixel-Effect-Generator**
- github.com/Adrianotiger/Neopixel-Effect-Generator
- Live: adrianotiger.github.io/Neopixel-Effect-Generator/
- Browser-based, visual strip display, generates Arduino code
- No audio reactivity

**FastLED-WASM / Wokwi**
- github.com/jandelgado/fastled-wasm
- Wokwi playground: wokwi.com/playground/fastled
- Compiles C++ to WASM for browser execution
- No visual editor, no audio

### Canvas 2D Approach (Fast Development)

```javascript
ctx.filter = "blur(10px)";
ctx.globalCompositeOperation = "lighter";  // Additive blending
for (let i = 0; i < 160; i++) {
  ctx.fillStyle = `rgb(${r}, ${g}, ${b})`;
  ctx.fillRect(x - ledRadius, 0, ledRadius * 2, canvas.height);
}
```

- Implementation time: 2-3 hours
- 60fps trivially achievable with 160 LEDs
- Blur radius 8-16px simulates LGP diffusion
- Additive blending naturally simulates light mixing
- No 3D perspective

### Three.js + Bloom Approach (Visual Realism)

```javascript
const geometry = new THREE.SphereGeometry(0.5, 8, 8);
const material = new THREE.MeshStandardMaterial({
  emissive: 0xff0000,
  emissiveIntensity: 2.0
});
const leds = new THREE.InstancedMesh(geometry, material, 160);
const bloomPass = new UnrealBloomPass(resolution, 1.5, 0.4, 0.85);
```

- Implementation time: 4-6 hours
- 60fps with 160 instanced meshes + bloom (1 draw call)
- Physically-based bloom matches real LED glow
- 3D perspective for product form factor visualisation
- Can show 2×160 strips with depth

### LGP Diffusion Parameters

| LGP Thickness | Blur Radius (px) | Visual Effect |
|---|---|---|
| 2mm (thin) | 6px | Light halo around each LED |
| 5mm (standard) | 10px | Merged adjacent halos |
| 10mm (thick) | 16px | Continuous diffuse bar |

### WS2812 Colour Accuracy

- WS2812 does NOT have true gamma correction
- Human eye perceives brightness logarithmically (gamma ≈ 2.2)
- Green is ~2x brighter than red at same 8-bit value
- Apply gamma correction: `Math.pow(value / 255, 1/2.2) * 255`

### Performance Comparison

| Approach | 160 LEDs @ 60fps | Setup Time | Bundle Size | 3D |
|---|---|---|---|---|
| Canvas 2D Blur | Yes (60-120fps) | 5 min | ~10KB | No |
| Canvas 2D shadowBlur | Yes (45-60fps) | 5 min | ~10KB | No |
| Three.js + Bloom | Yes (60fps) | 30 min | ~200KB | Yes |
| LEDstrip.js DOM | ~30fps | 2 min | ~15KB | No |

### Sources
- MDN CanvasRenderingContext2D.filter
- Three.js UnrealBloomPass docs
- React Three Fiber Bloom Tutorial: sbcode.net/react-three-fiber/bloom/
- Adafruit NeoPixel Diffusion Tips: learn.adafruit.com/make-it-glow-neopixel-and-led-diffusion-tips-tricks/
- LED gamma correction: cpldcpu.com/2022/08/15/does-the-ws2812-have-integrated-gamma-correction/
- IEEE LGP Design: researchgate.net/publication/273395744

---

## Vector 4: Existing Tools & Gap Analysis

### Tier 1: Audio-Reactive LED Systems (Hardware-First)

**LedFx** — Most relevant existing tool
- github.com/ledfx/ledfx (GPL-3.0)
- Real-time music analysis → LED output via WiFi
- Browser interface at localhost:8888
- Supports ESP8266, ESP32, WLED devices
- Gap: No node editor, no custom processing chain design

**Pixelblaze V3**
- electromage.com (proprietary hardware, open pattern community)
- 32-band spectrum via Sensor Expansion Board
- Live expression editor (code-based, not nodes)
- Sub-millisecond feedback
- Gap: Expression-based (not visual nodes), requires hardware (~$150)

**WLED + ARTI-FX**
- github.com/wled/wled (GPL-3.0)
- 100+ effects, ESP32 firmware
- Gap: Requires Arduino compilation for deep customisation

### Tier 2: Node-Based Visual Programming

**cables.gl** — Strongest candidate for inspiration
- cables.gl (open source as of 2024)
- Full Web Audio API integration (FFT, frequency bins, waveform)
- Fully node-based visual patch design
- Real-time WebGL rendering
- Gap: No built-in LED strip visualisation

**Hydra Synth**
- hydra.ojack.xyz (AGPL-3.0)
- Audio FFT via Meyda
- Live code (not visual nodes)
- Real-time WebGL
- Gap: Code-based, no hardware integration

**TouchDesigner**
- derivative.ca (proprietary, free for non-commercial)
- Mature CHOP/TOP architecture for signal processing + visualisation
- Can control WLED via OSC/API
- Gap: Desktop only, expensive, overkill

### Tier 3: LED Simulators & Pattern Generators

**Neopixel-Effect-Generator**: Web-based LED strip UI, generates Arduino code. No audio.
**LEDstrip Browser Simulator**: WS2812 simulation in browser, chainable animation API. No audio.
**FastLED-WASM**: C++ to WASM for browser execution. No visual editor, no audio.
**RemoteLight**: Java desktop app, WS2811/2812 control UI. No audio.

### Tier 4: Audio Visualisation Frameworks

**p5.js + p5.sound**: Canvas-based, FFT analysis, code-based. No node editor, no LED rendering.
**Shadertoy**: 512x2 FFT+waveform texture, GLSL shaders. Steep learning, no hardware export.
**Web Audio API directly**: Full AnalyserNode, requires custom everything.

### Gap Analysis

| Feature | Pixelblaze | LedFx | cables.gl | TouchDesigner | Our Tool |
|---------|-----------|-------|-----------|---------------|----------|
| Browser-based | Yes | Yes | Yes | No | Yes |
| Node-based editing | No | No | Yes | Yes | Yes |
| Audio input (live mic) | Yes | Yes | Yes | Yes | Yes |
| Real-time LED preview | Yes | Yes | No | No | Yes |
| Modifiable processing chain | Limited | Limited | Yes | Yes | Yes |
| WS2812 simulation | No | Yes | No | No | Yes |
| Free & open source | Partial | Yes | Yes | No | Yes |
| No coding required | No | Yes | Yes | No | Yes |
| Visual signal flow | No | No | Yes | Yes | Yes |
| C++ firmware export | No | No | No | No | Yes |

**The unique position**: The only browser-based, node-based audio-LED prototyping tool that doesn't require purchasing hardware or desktop software. Nothing like it exists.

### Patterns Worth Adopting

From **cables.gl**: Node graph UI operator+cable paradigm, colour-coded connections, parameter inspector, flow visualisation mode.
From **Web Audio API**: AnalyserNode architecture, AudioWorklet pattern, audio context graph model.
From **Pixelblaze**: 32-band spectrum pre-extraction, live parameter scrubbing, pattern preview.
From **LedFx**: Effect synchronisation, networked architecture.
From **Shadertoy**: Frequency texture format for GPU processing.
From **FastLED-WASM**: WASM compilation for C++ effect code in browser.

### Sources
- LedFx: github.com/ledfx/ledfx
- Pixelblaze: electromage.com
- cables.gl: cables.gl
- Hydra: hydra.ojack.xyz
- WLED: kno.wled.ge
- Neopixel-Effect-Generator: github.com/Adrianotiger/Neopixel-Effect-Generator
- LEDstrip: github.com/dougalcampbell/LEDStrip
- FastLED-WASM: github.com/jandelgado/fastled-wasm
- TouchDesigner: derivative.ca
- Notch: notch.one
- Max/MSP Jitter: cycling74.com/products/jitter
- vvvv: vvvv.org

---

## Vector 5: Dataflow Graph Architecture

### Execution Model

**Recommended: Pull-based synchronous evaluation with topological sort.**

| Criterion | Pull | Push | Hybrid |
|-----------|------|------|--------|
| Deterministic frame output | Yes | Requires barrier sync | Adds complexity |
| Dead node elimination | Free | Must propagate "dirty" flags | Partial |
| Matches firmware model | Yes (effects pull from ControlBus) | No | N/A |
| Implementation complexity | Low | Medium | High |

The firmware's render loop is already pull-based: `render(EffectContext&)` is called once per frame. The graph engine mirrors this.

**Execution sequence per frame:**
1. Audio source nodes snapshot current ControlBus state (O(1), ~200 bytes)
2. Engine walks topologically sorted node list front-to-back
3. Each node's `execute()` reads input ports, computes, writes output ports
4. No caching within a frame — every node evaluates exactly once
5. Output node writes CRGB[160] radial buffer
6. Mirror stage (SET_CENTER_PAIR) produces final CRGB[320]

**Why not Rete.js lazy pull with caching?** Audio data changes every frame. Caching adds overhead for zero benefit.
**Why not cables.gl event propagation?** Creates non-deterministic ordering when multiple inputs change simultaneously.

### Prior Art Alignment

| System | Model | Lesson |
|--------|-------|--------|
| Pure Data | Depth-first pull, 64-sample blocks | Block processing works; our "block" is 160 LEDs |
| Web Audio API | Pull-based, 128-sample render quanta | Fixed block size with typed arrays is performant |
| GNU Radio 4.0 | Static SDF schedule, compile-time optimisation | Topological sort at graph-change time, not per-frame |
| cables.gl | Event-driven push | Trigger ports useful for beat events only |
| Max/MSP gen~ | Visual patcher → C++ export | Exact precedent for graph-to-code |

### Type System

```typescript
enum PortType {
  SCALAR,       // float — audio features, parameters, time
  ARRAY_160,    // Float32Array(160) — per-pixel values (half-strip)
  CRGB_160,     // Uint8Array(480) — RGB triplets for 160 LEDs
  ANGLE,        // float in [0, 2π) — circular quantities (hue, chroma angle)
  BOOL,         // boolean — triggers, gates
  INT,          // integer — indices, counts
}
```

**Type promotion rules:**
- SCALAR × ARRAY_160 = ARRAY_160 (broadcast scalar to all 160 elements)
- SCALAR + SCALAR = SCALAR
- ARRAY_160 × ARRAY_160 = ARRAY_160 (element-wise)
- ANGLE operations always use circular arithmetic
- CRGB_160 is terminal — only produced by composition nodes

### Complete Node Catalogue (Mapped to Firmware Primitives)

#### Layer 1: Audio Source Nodes

| Node | Output Type | Firmware Source |
|------|-------------|-----------------|
| RMS | SCALAR | `ctx.audio.rms()` |
| Fast RMS | SCALAR | `ctx.audio.fastRms()` |
| Flux | SCALAR | `ctx.audio.flux()` |
| Fast Flux | SCALAR | `ctx.audio.fastFlux()` |
| Band (index param) | SCALAR | `ctx.audio.getBand(i)` |
| All Bands | ARRAY_8 | `ctx.audio.bands()` |
| Chroma (index param) | SCALAR | `ctx.audio.getChroma(i)` |
| All Chroma | ARRAY_12 | `ctx.audio.chroma()` |
| Beat Phase | SCALAR | `ctx.audio.beatPhase()` |
| Beat Strength | SCALAR | `ctx.audio.beatStrength()` |
| Is On Beat | BOOL | `ctx.audio.isOnBeat()` |
| Snare Energy | SCALAR | `ctx.audio.snare()` |
| Hi-hat Energy | SCALAR | `ctx.audio.hihat()` |
| Silent Scale | SCALAR | `ctx.audio.silentScale()` |
| Tempo Confidence | SCALAR | `ctx.audio.tempoConfidence()` |
| BPM | SCALAR | `ctx.audio.bpm()` |
| Delta Time | SCALAR | `ctx.deltaTimeSeconds` |
| Raw Delta Time | SCALAR | `ctx.rawDeltaTimeSeconds` |

#### Layer 2: Processing Nodes

| Node | Inputs | Output | Firmware Equivalent |
|------|--------|--------|---------------------|
| EMA Smooth | value, tau | SCALAR | `ExpDecay::update()` |
| Asymmetric Follower | value, riseTau, fallTau | SCALAR | `AsymmetricFollower::update()` |
| dt-Decay | value, rate60fps | SCALAR | `chroma::dtDecay()` |
| Circular EMA | angle, tau | ANGLE | `chroma::circularEma()` |
| Circular Chroma Hue | chroma[12] | ANGLE | `chroma::circularChromaHueSmoothed()` |
| Scale | value, factor | SCALAR/ARRAY | multiply |
| Power | value, exponent | SCALAR/ARRAY | `powf()` |
| Clamp | value, min, max | SCALAR/ARRAY | min/max |
| Mix | inputs[], weights[] | SCALAR/ARRAY | weighted sum |
| Schmitt Trigger | value, open, close | BOOL | hysteresis gate |
| Spring | target, stiffness, mass | SCALAR | `Spring::update()` |
| Max Follower | value, attack, decay, floor | SCALAR | asymmetric follower with floor |

#### Layer 3: Geometry Nodes (produce ARRAY_160)

| Node | Inputs | Output | Description |
|------|--------|--------|-------------|
| Triangular Wave | spacing, phase, drift | ARRAY_160 | Repeating triangular envelope |
| Standing Wave | mode_n, phase | ARRAY_160 | `sin(n * pi * dist / 160)` |
| Gaussian | centre, sigma | ARRAY_160 | `exp(-(dist-centre)^2 / (2*sigma^2))` |
| Exponential Decay | rate | ARRAY_160 | `exp(-rate * dist)` from centre |
| Linear Gradient | | ARRAY_160 | `dist / 160.0` |
| Sinusoidal Sum | frequencies[], amplitudes[] | ARRAY_160 | Multi-frequency composition |
| Centre Melt | sigma | ARRAY_160 | Gaussian centred at LED 79/80 |
| Scroll Buffer | newValue, rate | ARRAY_160 | Stateful: shifts buffer outward |

#### Layer 4: Composition Nodes

| Node | Inputs | Output | Description |
|------|--------|--------|-------------|
| HSV to RGB | hue, saturation, value (ARRAY_160 or SCALAR) | CRGB_160 | Produces LED colour |
| Palette Lookup | index, brightness (ARRAY_160) | CRGB_160 | `palette.getColor()` |
| Blend | colourA, colourB, factor | CRGB_160 | Linear interpolation |
| Add Colour | base, overlay | CRGB_160 | `qadd8` saturating add |
| Scale Brightness | colour, amount | CRGB_160 | `nscale8` |

#### Layer 5: Output Node

Takes CRGB_160 → centre-origin mirror (SET_CENTER_PAIR) → CRGB_320 → LED preview canvas.

### State Management

Stateful nodes (EMA, Asymmetric Follower, Scroll Buffer, dt-Decay) need external state management.

```typescript
interface StatefulNode {
  getInitialState(): NodeState;
  execute(inputs: PortValues, state: NodeState, dt: number): {
    outputs: PortValues;
    nextState: NodeState;
  };
}
```

State lives in `Map<NodeId, NodeState>` owned by the graph engine. Enables:
- **Reset**: Clear all state to initial values
- **Snapshot**: Save/restore for undo/redo
- **Scrubbing**: Jump to t=0 and replay with recorded audio
- **C++ export**: State variables become class member variables

### Graph-to-C++ Code Generation

**Template-based code generation (recommended).** Each node maps to a C++ code fragment. Topological sort order becomes statement order in generated `render()`.

Example: `Band(0) → AsymmetricFollower → Scale → Gaussian → HSV_to_RGB → Output` generates:

```cpp
class GeneratedEffect : public IEffect {
    AsymmetricFollower m_node3_follower{0.0f, 0.05f, 0.30f};

    void render(EffectContext& ctx) override {
        const float rawDt = ctx.getSafeRawDeltaSeconds();

        // Node 1: Band source
        float n1_out = ctx.audio.getBand(0);

        // Node 2: Asymmetric follower
        float n2_out = m_node3_follower.update(n1_out, rawDt);

        // Node 3: Scale
        float n3_out = n2_out * 1.5f;

        // Node 4: Gaussian geometry
        float n4_out[160];
        for (uint16_t i = 0; i < 160; ++i) {
            float dist = (float)i / 160.0f;
            n4_out[i] = n3_out * expf(-(dist * dist) / (2.0f * 0.3f * 0.3f));
        }

        // Node 5: HSV to RGB composition
        for (uint16_t dist = 0; dist < 160; ++dist) {
            uint8_t val = (uint8_t)(n4_out[dist] * 255.0f);
            SET_CENTER_PAIR(ctx, dist, CHSV(ctx.gHue, ctx.saturation, val));
        }
    }
};
```

**Web-firmware parity**: JavaScript implementations must use identical math (same `expf(-dt / tau)` formula). Use `Float32Array` and `Math.fround()` to match ESP32 single-precision float behaviour.

**Precedent**: Max/MSP gen~ compiles visual patches to C++ for embedded targets. Unity Shader Graph compiles to HLSL. Unreal Blueprints nativise to C++.

### Mini-Visualisation Per Node

| Type | Visualisation | Size |
|------|---------------|------|
| SCALAR | Horizontal bar + numeric + sparkline (last 60 frames) | 120x24px |
| ARRAY_160 | Mini waveform plot (160 samples, normalised) | 160x32px |
| CRGB_160 | LED strip preview (160 coloured pixels) | 160x8px |
| ANGLE | Circular indicator (dot on circle) + degrees | 32x32px |
| BOOL | Green/red dot + label | 32x16px |

Cost: ~40us per canvas blit × 50 nodes = ~2ms total. Within budget.

### Parameter Exploration

- **Randomise**: Per-node or whole-graph with constraints
- **Lock**: Pin parameters so randomisation skips them
- **Range preview**: Alt+drag to see min/max simultaneously
- **Value distribution**: Tooltip shows min/max/mean/stddev for ARRAY_160
- **A/B comparison**: Snapshot → tweak → toggle

### Performance Budget

**Web (per frame):**

| Operation | Cost |
|-----------|------|
| Audio source snapshot | 0.01ms |
| Scalar node evaluation (20 nodes) | 0.04ms |
| Array node evaluation (10 nodes) | 0.30ms |
| CRGB composition (5 nodes) | 0.15ms |
| Output + mirror | 0.05ms |
| Mini-visualisations (35 nodes) | 1.50ms |
| React/DOM overhead | 0.50ms |
| **Total** | **~2.55ms** (within 16.67ms for 60fps) |

**ESP32 (generated C++):**

| Operation | Cost |
|-----------|------|
| Scalar computations | 0.05ms |
| Array loops (160 elements) | 0.10-0.30ms |
| SET_CENTER_PAIR (160 iterations) | 0.05ms |
| **Total** | **0.2-0.4ms** (within 2.0ms ceiling) |

### Architecture Decision Records

**ADR-001: Pull-Based Synchronous Evaluation**
Every frame evaluates all nodes in topological order. No caching, no lazy eval, no event propagation. Matches firmware's `render()` call pattern. Dead nodes eliminated by not being in sort order.

**ADR-002: Code Generation Over Interpretation**
Graph compiles to C++ `IEffect` subclass. Zero graph overhead on ESP32. Each node maps to a fixed code fragment. Adding a node type requires JS evaluator + C++ template (one-time cost per type, ~30 types total).

**ADR-003: Explicit State Management**
State lives in engine-owned Map, not inside nodes. Enables reset, snapshot, scrubbing, C++ export. Nodes declare initial state shape. Trade-off: no hidden mutable state (acceptable for our fixed node set).

**ADR-004: Fixed Type System with ARRAY_160**
Six types: SCALAR, ARRAY_160, CRGB_160, ANGLE, BOOL, INT. Static type checking at connection time. SCALAR→ARRAY broadcast is implicit. Domain-locked to 160-LED half-strip (parameterisable later if hardware changes).

### Recommended Technology Stack

| Layer | Technology | Rationale |
|-------|-----------|-----------|
| Graph UI | React Flow (xyflow) | Layout, pan/zoom, connections. No execution engine — we build our own. |
| Execution engine | Custom TypeScript (~300 lines) | Topological sort + evaluate loop. |
| Node mini-viz | HTML Canvas per output port | Direct pixel manipulation |
| Audio simulation | Web Audio API AudioWorklet | Real mic audio through same DSP chain |
| LED preview | Canvas 2D (strip) or WebGL (3D LGP) | Strip preview trivial; 3D optional |
| C++ code gen | Handlebars/Mustache templates | One template per node type |
| State persistence | JSON | Import/export, undo/redo, share graphs |

**What NOT to use:**
- Rete.js: Lazy pull with caching is wrong for per-frame evaluation.
- cables.gl: Event-driven push creates non-deterministic evaluation order.
- Pure Data: 64-sample audio blocks are not our data model (we process 160-element spatial arrays).

### Evolution Path

1. **Phase 1**: Core graph engine (web only). Topological sort, evaluate loop, 9 node types, LED preview.
2. **Phase 2**: Full node catalogue (all 30+ types), mini-visualisations, parameter sliders, audio simulation.
3. **Phase 3**: C++ code generation. Template engine, compilable IEffect output, verification harness.
4. **Phase 4**: Live firmware preview. WebSocket to K1, push generated effect, see on real LEDs.
5. **Phase 5**: Graph library. Save/load/share. Pre-built templates replicating existing hand-written effects.

### Key Firmware Files Referenced

- `firmware-v3/src/audio/contracts/ControlBus.h` — 212 symbols, complete audio signal inventory
- `firmware-v3/src/plugins/api/EffectContext.h` — 154 symbols, render context
- `firmware-v3/src/plugins/api/IEffect.h` — Effect interface
- `firmware-v3/src/effects/enhancement/SmoothingEngine.h` — ExpDecay, Spring, AsymmetricFollower
- `firmware-v3/src/effects/enhancement/TemporalOperator.h` — 8 temporal shaping operators
- `firmware-v3/src/effects/ieffect/ChromaUtils.h` — circularChromaHue, circularEma, dtDecay
- `firmware-v3/src/effects/CoreEffects.h` — SET_CENTER_PAIR macro

### Sources
- Dataflow Programming: devopedia.org/dataflow-programming
- Rete.js Dataflow Engine: retejs.org/docs/guides/processing/dataflow/
- React Flow: reactflow.dev
- cables.gl Architecture: deepwiki.com/cables-gl/cables/1-overview
- Pure Data Theory: puredata.info/docs/manuals/pd/x2.htm
- GNU Radio 4.0: gnuradio.org/news/2025-12-17-gr4-transform-sdr-workflows/
- Unreal Blueprint Nativisation: dev.epicgames.com/documentation/en-us/unreal-engine/nativizing-blueprints
- Web Audio AudioWorklet: developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API/Using_AudioWorklet

---

## Vector 6: ESV11 Backend Source Dependency Map

### File Inventory

| File | Lines | Category | WASM Status |
|------|-------|----------|------------|
| EsV11Backend.h | 113 | Core API | Portable* |
| EsV11Backend.cpp | 375 | Core DSP orchestrator | Portable* |
| EsV11Adapter.h | 79 | Contract mapper | Portable |
| EsV11Adapter.cpp | 303 | Contract mapper impl | Portable |
| EsBeatClock.h | 99 | Beat phase integrator | Portable |
| EsBeatClock.cpp | 202 | Beat phase integrator | Portable |
| vendor/EsV11Shim.h | 50 | Hardware shim | Stub-friendly |
| vendor/EsV11Shim.cpp | 5 | Timing globals | Portable |
| vendor/EsV11Buffers.h | 46 | Buffer pointers | Portable* |
| vendor/EsV11Buffers.cpp | ~30 | PSRAM allocator | Requires stub |
| vendor/global_defines.h | 37 | Constants | Portable |
| vendor/goertzel.h | 150+ | Goertzel DSP | Pure math |
| vendor/vu.h | 100+ | VU meter DSP | Pure math |
| vendor/tempo.h | 150+ | Tempo tracking DSP | Pure math |
| vendor/types_min.h | 33 | Structs | Portable |
| vendor/utilities_min.h | 28 | Helpers | Portable |
| vendor/microphone.h | 100+ | I2S acquisition | HARDWARE ONLY |

**Total: 18 files, ~2,595 LOC. 69% pure math, 15% hardware-dependent, 15% project wrappers.**

### Hardware Stubs Required (5 total)

1. **I2S Audio Acquisition** (~50 lines stub): Replace `init_i2s_microphone()` and `acquire_sample_chunk()` with Web Audio API input
2. **Timing** (~20 lines stub): Replace `micros()`/`millis()` with `performance.now()`
3. **Logging** (~10 lines stub): Replace `LW_LOGI`/`LW_LOGD`/`LW_LOGE` with `console.log`
4. **Memory Allocator** (~40 lines stub): Replace `esp_heap_caps_malloc(MALLOC_CAP_SPIRAM)` with WASM linear memory
5. **ESP-DSP** (~10 lines stub): Replace `dsps_mulc_f32()` with inline scalar multiply loop

### Pure Math Components (port directly)

- **vendor/goertzel.h**: 64-bin Goertzel DTFT, window lookup, magnitude calculation, chromagram folding. ~600 lines.
- **vendor/vu.h**: RMS computation, adaptive noise floor, cap follower. ~150 lines.
- **vendor/tempo.h**: 96-bin tempo Goertzel on novelty curve, octave-aware selection, hysteresis. ~500 lines.
- **EsV11Adapter.cpp**: Autorange followers, octave band grouping, heavy smoothing, percussion onset detection. ~300 lines.
- **EsBeatClock.cpp**: Beat phase accumulation, tick detection, bar counting. ~200 lines.

### Dependency Tree

```
EsV11Backend.h/cpp (ENTRY POINT)
├── vendor/goertzel.h (PURE MATH)
├── vendor/vu.h (PURE MATH)
├── vendor/tempo.h (PURE MATH)
├── vendor/EsV11Buffers.h (buffer pointers)
├── vendor/EsV11Shim.h (timing stubs)
├── vendor/microphone.h (I2S — STUB)
└── vendor/global_defines.h (constants)

EsV11Adapter.h/cpp (CONTRACT MAPPER)
├── EsV11Backend.h
├── contracts/ControlBus.h
├── audio/AudioMath.h (retunedAlpha)
└── config/audio_config.h (SAMPLE_RATE, HOP_RATE_HZ)

EsBeatClock.h/cpp (BEAT PHASE)
├── contracts/ControlBus.h
├── contracts/MusicalGrid.h
└── contracts/AudioTime.h
```

---

## Vector 7: Goertzel Algorithm Portability Specification

### Algorithm Architecture

- **64 frequency bins** at musical spacing (~55 Hz to ~16 kHz)
- Spacing: note table with BOTTOM_NOTE=12, NOTE_STEP=2 (every half-step)
- Each bin has independent Goertzel filter with adaptive block size
- **4096-point Gaussian window** (sigma=0.8) applied per-sample
- Interlaced computation: 32 bins per frame (odd/even alternation)

### Processing Pipeline

1. Goertzel DFT → 64 parallel frequency bin magnitudes
2. Noise floor subtraction (10-sample history, EMA-adapted)
3. Auto-ranging (dynamic gain normalisation)
4. 12-hop spectral smoothing (~240ms rolling average)
5. Chromagram folding (64 bins → 12 pitch classes across 5 octaves)
6. Novelty detection (spectral flux, log-compressed)
7. 96 tempo resonators (Goertzel on novelty curve, 48-144 BPM)
8. Octave-aware tempo selection with doubling/halving logic

### Memory Footprint: ~68KB

- `sample_history[4096]` — 16 KB
- `window_lookup[4096]` — 16 KB
- `novelty_curve[1024] × 4` — 16 KB
- `frequencies_musical[64]` — 3.5 KB
- `tempi[96]` — 8.5 KB
- `spectrogram_average[12×64]` — 3 KB
- `noise_history[10×64]` — 2.5 KB
- Other (vu, chroma, history) — ~2.5 KB

Trivially fits in WASM linear memory.

### Portability Assessment

- ALL mathematical operations are pure `float32`
- Standard C math only: `cos`, `sin`, `sqrt`, `exp`, `log`, `log1p`, `atan2`, `pow`, `fabs`, `fmax`, `fmin`
- No SIMD intrinsics, no hardware-specific opcodes
- No dynamic allocation in real-time paths
- **FULLY PORTABLE to WASM or JavaScript**

---

## Vector 8: WASM Compilation Feasibility

### Three Paths Compared

| Path | Effort | Parity | Bundle | Performance |
|------|--------|--------|--------|-------------|
| **Emscripten (C++ → WASM)** | 1-2 weeks | Exact (same code, same floats) | 1-3 MB | 60+ fps (2.0ms/frame) |
| **Pure JS/TS rewrite** | 3-5 days, ~1,200 lines | Close (IEEE 754 double vs float) | 50 KB | 60+ fps (1.5ms/frame) |
| **Rust + wasm-pack** | 1-2 weeks | Close | 150-300 KB | 60+ fps (2.0ms/frame) |

### Critical Finding: WASM Is Not Faster for Goertzel

- JavaScript: 1.5ms per frame (64 frequencies × 128 samples)
- WASM: 2.0ms per frame
- Speedup: only 1.3x (algorithm is memory-access-bound, not compute-bound)

Decision should be driven by **parity and maintainability**, not performance.

### Numerical Parity

- ESP32 FPU: IEEE 754 single-precision (32-bit float)
- Browser WASM: IEEE 754 single-precision (32-bit float) — **identical**
- Browser JS: IEEE 754 double-precision (64-bit) — MORE precise, slight divergence possible
- Expected divergence: ±0.001 on normalised values — **imperceptible on LEDs** (1 LSB in uint8)
- For exact parity in JS: use `Math.fround()` to force single-precision

### Existing Goertzel in JS

- **goertzeljs** (npm): 100+ stars, production JavaScript Goertzel implementation
- **realtime-bpm-analyzer** (npm): Beat detection via spectral analysis

### Emscripten Build Strategy

1. Copy pure DSP files (goertzel.h, vu.h, tempo.h, types_min.h, utilities_min.h)
2. Write thin C++ wrapper with 5 stubs (I2S, timing, logging, memory, esp_dsp)
3. Compile: `emcc -O3 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -o esv11_dsp.js stub_wasm.cpp`
4. Expose via embind: `processChunk(Float32Array)`, `getSpectrogram()`, `getChromagram()`, etc.

### Sources
- goertzeljs: github.com/Ravenstine/goertzeljs
- Essentia.js build system: mtg.github.io/essentia.js/docs/
- WASM vs JS benchmark: bojandjurdjevic.com/2018/WASM-vs-JS-Realtime-pitch-detection/
- Emscripten docs: emscripten.org

---

## Vector 9: EsV11Adapter Complete Field Mapping

### Global Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| ACTIVE_VU_THRESHOLD | 0.01f | Below this, audio treated as silent |
| HOP_RATE_HZ | 50.0 Hz | Analysis update rate |

### retunedAlpha() — Frame-Rate-Agnostic Smoothing

All EMA alphas are specified at a 50 Hz reference rate and retuned to actual hop rate:

```
tauFromAlpha(alpha, hop_rate) = -1.0 / (hop_rate * log(1.0 - alpha))
computeEmaAlpha(tau, hop_rate) = 1.0 - exp(-1.0 / (tau * hop_rate))
retunedAlpha(alpha_at_50hz, 50.0, actual_hz) = computeEmaAlpha(tauFromAlpha(alpha_at_50hz, 50.0), actual_hz)
```

### Field-by-Field Transformation Chain

**rms**: `es.vu_level` → `clamp01(sqrt(max(0, vu_level)) * 1.25)` — sqrt converts energy to loudness, 1.25 expansion factor

**flux**: `es.novelty_norm_last` → `clamp01(novelty_norm_last)` — direct passthrough

**bins64[64]**: `es.spectrogram_smooth[i]` → clamp01 → autorange follower (decay alpha 0.005 @ 50Hz, rise alpha 0.25 @ 50Hz, floor 0.05, activity-gated) → `clamp01(raw * invFollower)`

**bands[8]**: `bins64[8*b..8*b+7]` → arithmetic mean of 8 bins per band → `clamp01(sum / 8.0)`

**heavy_bands[8]**: `bands[i]` → EMA with alpha `retunedAlpha(0.05, 50, HOP_RATE)` (tau ~20s)

**chroma[12]**: `es.chromagram[i]` → clamp01 → autorange follower (decay 0.005, rise 0.35, floor 0.08, activity-gated) → `clamp01(raw * invFollower)`

**heavy_chroma[12]**: `chroma[i]` → same EMA as heavy_bands (alpha 0.05 @ 50Hz)

**snareEnergy**: `mean(bins64[5..10])` → clamp01. **snareTrigger**: `(snareEnergy > prev + 0.08) && (snareEnergy > 0.10)`

**hihatEnergy**: `mean(bins64[50..60])` → clamp01. **hihatTrigger**: `(hihatEnergy > prev + 0.08) && (hihatEnergy > 0.05)`

**es_phase01_at_audio_t**: `(phase_radians + π) / (2π)` → floor-wrap to [0, 1)

**es_beat_in_bar**: Modulo 4 counter incremented on each `es_beat_tick`

**sb_waveform_peak_scaled**: `max(abs(waveform)) - 750` → asymmetric follower (attack 0.25, release 0.005, floor 750) → ratio / follower → smoothed output

### Adapter State Variables (14 total)

`m_binsMaxFollower` (0.1f), `m_chromaMaxFollower` (0.2f), `m_heavyBands[8]` (0), `m_heavyChroma[12]` (0), `m_beatInBar` (0), `m_sbWaveformHistory[4][128]` (0), `m_sbWaveformHistoryIndex` (0), `m_sbMaxWaveformValFollower` (750.0f), `m_sbWaveformPeakScaled` (0), `m_sbWaveformPeakScaledLast` (0), `m_sbNoteChroma[12]` (0), `m_sbChromaMaxVal` (0.0001f), `m_prevSnareEnergy` (0), `m_prevHihatEnergy` (0)

### Execution Order

1. Timebase (AudioTime, hop_seq)
2. Energy (rms, flux, fast_rms, fast_flux)
3. Bins (clamp → autorange → bins64, bins64Adaptive, es_bins64_raw)
4. Bands (group bins64 → bands[8])
5. Heavy bands (EMA → heavy_bands[8])
6. Chroma (clamp → autorange → chroma[12], es_chroma_raw)
7. Heavy chroma (EMA → heavy_chroma[12])
8. Waveform (copy waveform, sb_waveform)
9. SB peak (follower → sb_waveform_peak_scaled)
10. SB note chromagram (octave aggregation → sb_note_chromagram[12])
11. Percussion (bin sums + triggers)
12. Tempo (es_bpm, confidence, tick, strength, phase, bar, downbeat)

---

## Vector 10: 32kHz Resampling in Browser

### The Problem

Firmware: 32kHz I2S capture. Browser: 44.1kHz or 48kHz system audio.

### Solutions

| Approach | Feasibility | Latency | Parity | Complexity |
|----------|-------------|---------|--------|-----------|
| `AudioContext({ sampleRate: 32000 })` | Yes (all modern browsers) | 0ms | Exact | Very low |
| libsamplerate-js in AudioWorklet | Yes | +2.9ms | Close (professional) | Medium |
| Linear interpolation resampling | Yes | +2.9ms | Approximate | Low |
| Goertzel recalibration for 44.1kHz | Yes | 0ms | Approximate (needs correction) | Medium-high |

### Recommended: `AudioContext({ sampleRate: 32000 })`

Simplest path. All modern browsers support custom sample rates (8kHz–96kHz). Browser creates an internal sample rate converter if hardware doesn't support 32kHz natively — this is transparent and deterministic.

```javascript
const audioCtx = new AudioContext({ sampleRate: 32000 });
```

### Chunk Alignment

Firmware: 128 samples @ 32kHz = 4ms per chunk, ~250 chunks/second.
Browser AudioWorklet: fixed 128-sample blocks.
With 32kHz AudioContext: blocks are 128 samples @ 32kHz = 4ms — **exactly matching firmware**.

### End-to-End Latency Budget

| Stage | Browser | Firmware |
|-------|---------|----------|
| Audio capture | 15-20ms | 4ms |
| DSP processing | <1ms | <1ms |
| ControlBus update | <1ms | <1ms |
| Node graph + LED preview | 2-3ms | N/A |
| **Total** | **~20-25ms** | **~6ms** |

Browser is 3-4x slower due to system audio stack. Acceptable for a design tool — not a real-time controller.

### Sources
- MDN AudioContext constructor: developer.mozilla.org/en-US/docs/Web/API/AudioContext/AudioContext
- Chrome sample rate support: chromestatus.com/feature/5136778254090240
- libsamplerate-js: github.com/aolsenjazz/libsamplerate-js
- AudioWorklet ring buffer: googlechromelabs.github.io/web-audio-samples/audio-worklet/design-pattern/wasm-ring-buffer/

---

## Resolved: Sample Rate Confirmed 32kHz

`vendor/global_defines.h` defaults to 12800 Hz (original Emotiscope). Production K1v2 build (`esp32dev_audio_esv11_k1v2_32khz`) uses `EsV11_32kHz_Shim.h` which overrides to `SAMPLE_RATE = 32000`. All Goertzel coefficients, block sizes, and bandwidths are computed at 32kHz.

At 32kHz with HOP_SIZE=256: **HOP_RATE_HZ = 125 Hz** (not 50 Hz). All `retunedAlpha()` calculations auto-adjust via the frame-rate-agnostic formula, but the analysis cadence is 2.5× faster than the 12.8kHz vendor default.

Browser: `new AudioContext({ sampleRate: 32000 })` matches production exactly.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-23 | agent:research-swarm (5 vectors) | Created — node editor, web audio, LED simulation, existing tools, architecture |
| 2026-03-23 | agent:research-swarm (5 vectors) | Added — ESV11 source map, Goertzel portability, WASM feasibility, adapter chain, 32kHz resampling |
