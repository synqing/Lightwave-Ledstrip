# 🌈 Advanced Light-Guide-Plate (LGP) Interference Pattern Catalogue

> **Document scope** – Extend our original *LGP Interference Discovery* with a library of new shapes that leverage wave-guide physics. Each entry contains the optical concept, the LED-domain math, encoder mappings, and implementation notes.

---

## 1. Diamond-Lattice (Cross-Wave)  
**Optical concept:** Two oblique travelling-wave sets (±45 °) produce a rhombic interference grid inside the LGP.  
**Visible result:** Crystalline diamond tiles that slide diagonally towards centre.

| Param | Encoder | Range | Effect |
|-------|---------|-------|--------|
| Wave count | Complexity | 2-10 | Number of diamond rows |
| Glide speed | Speed | 10-255 | Diagonal motion rate |
| Lattice tilt | Variation | 0-255 | ±30 ° skew via phase mask |

```cpp
// pseudo-code kernel
float diagPhase = (leftPhase * i + rightPhase * (LEN-i));
uint8_t idx = gHue + scale8(diagPhase, 4);
leds[i] = ColorFromPalette(pal, idx, brightness);
```

---

## 2. Moiré Curtains  
**Optical concept:** Two slightly mismatched spatial frequencies create slow-moiré beat envelopes.

*Left strip freq:* `fL = base + Δ/2`  
*Right strip freq:* `fR = base − Δ/2`

`Δ` controlled by **Variation**.  Beat envelope period ≈ `1/Δ`, giving drifting light "curtains."

---

## 3. Radial Ripple (Virtual Circles)  
Generate concentric rings that seem to expand from virtual centre behind the LGP.

`brightness = sin16((dist² – t) * k) >> 8;`

Square wave thresholding (duty via Intensity) creates crisp ring edges → perceived circles.

---

## 4. Holographic Vortex  
Combine azimuthal phase ramp (`θ`) with radial chirp → spiral interference.

`phase = kθ + m·r – ωt`  
`hue   = gHue + phase/32`

`k` (spiral count) on **Complexity**, `m` (tightness) on **Intensity**.

---

## 5. Modal Cavity Resonance  
Excite discrete wave-guide modes `M_n`, `n ∈ ℕ`.  LED index mapped to physical position `x`; activate mode via `sin(n·π·x/L)` both sides.

Result: standing bright zones (# of boxes = `n`), but *non-equidistant* when using cosine taper.

---

## 6. Evanescent Drift  
Fade amplitude exponentially from both edges; moving interference only near boundaries – good for subtle ambience.

```cpp
brightness = expf(-alpha * distEdge) * sin(phase);
```

`alpha` on **Intensity**; phase speed on **Speed**.

---

## 7. Chromatic Shear  
Left emits hue ramp H, Right emits H+120°.  Linear shear velocity produces sliding color planes that merge to white at centre.

**Palette** continuously rotates (Palette encoder) giving rainbow shearing.

---

## Implementation Notes  
1. **Shader Template** – add `lgpShader(int idx, uint32_t t, const Params&)` returning `CRGB`. Each pattern is one shader.
2. **Dual-Buffer Render** – compute strip1 & strip2 separately, then copy to `leds[]` for TransitionEngine compatibility.
3. **Anti-Phase Option** – checkbox in serial menu toggles `phase += π` on right strip for stronger standing waves.
4. **Performance** – all equations pre-scaled to integer domain; avoid `expf` by LUT (E-table 256). Target <1.4 ms for 320 LEDs.

---

## Test & Calibration Procedure  
1. Set room lighting low, mount LGP vertically.  
2. Flash *Diamond-Lattice*; turn **Complexity** until exactly 6 diamonds across length; note value.  
3. Switch to *Moiré Curtains*; increase **Variation** until curtain rate ≈ 0.5 Hz; record Δ.
4. Capture 120 fps video for optical analysis; use edge-detect to measure actual box count vs LED phase.

---

## Roadmap
| Week | Task |
|------|------|
| 1 | Implement & bench Diamond-Lattice, Moiré Curtains |
| 2 | Add Radial Ripple & Vortex; integrate serial-menu controls |
| 3 | Optical calibration tool (Python OpenCV) to auto-derive spatial constants from video |

---

**Author:** LightwaveOS R&D – *Optical Pattern Working Group*  
*Revision 0.1 · committed via Captain* 