# LED Simulator Design

## Purpose
A real-time LED simulator enables pattern developers to preview and debug their effects visually, without hardware. It must accurately reflect timing, color, and motion as seen on the actual LED strips.

---

## Requirements
- **Real-time rendering** of 320+ LEDs (dual strip, matrix, etc.)
- **Accurate color and brightness** (gamma correction, color profiles)
- **Frame-accurate timing** (simulate 120+ FPS)
- **Audio-reactive preview** (sync with audio input or file)
- **Parameter control** (sliders, presets)
- **Integration with SDK and Jupyter notebooks**
- **Mobile and desktop support**
- **Export to video/GIF**

---

## Architecture
- **Frontend:**
  - WebGL/Canvas for fast rendering (browser, Electron, or Jupyter widget)
  - UI for parameter adjustment, audio controls, and playback
- **Backend/Core:**
  - Pattern logic runs in WebAssembly (C++), Python, or JS (depending on SDK)
  - Audio analysis pipeline (FFT, beat detection)
  - Timing engine to match device frame rate
- **Integration:**
  - Jupyter: Custom widget using ipywidgets or nbextension
  - SDK: Expose `simulate(pattern)` API for Python/JS

---

## Technology Choices
- **Web:** React + WebGL (Three.js, Pixi.js, or raw Canvas2D)
- **Jupyter:** ipywidgets, p5.js, or custom JS widget
- **Desktop:** Electron app (optional)
- **Pattern Execution:**
  - For C++: Compile to WebAssembly (Emscripten)
  - For Python: Native execution in notebook
  - For JS: Direct execution

---

## Data Flow
1. User loads pattern (source or compiled)
2. Audio input (mic/file) is processed
3. Pattern logic runs per frame, outputs LED buffer
4. Renderer displays LEDs in real time
5. User can adjust parameters, pause/play, export

---

## Example API
```python
import k1lightwave
pattern = k1lightwave.load_pattern('MyPattern.cpp')
k1lightwave.simulate(pattern, audio='song.mp3', params={'speed': 1.2})
```

---

## Challenges
- Matching hardware timing and color fidelity
- Efficient simulation of hundreds of LEDs at high FPS
- Cross-language pattern execution (C++/Python/JS)
- Synchronizing audio and visual output

---

## Next Steps
1. Prototype a minimal WebGL/Canvas LED strip renderer
2. Integrate with Python/JS SDK for pattern execution
3. Develop Jupyter widget for notebook integration
4. Add audio input and parameter controls
5. Validate against real hardware output 