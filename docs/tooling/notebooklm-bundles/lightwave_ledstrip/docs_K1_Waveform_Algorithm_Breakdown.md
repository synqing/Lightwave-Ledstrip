---
abstract: "7-stage per-frame algorithm breakdown for K1 Waveform effect (0x1302): chromagram colour synthesis, amplitude-dependent trail fade, dt-corrected scroll, sub-pixel dot rendering, centre mirror. Includes SB 3.0.0 parity audit."
---

 K1 Waveform — Complete Algorithm Breakdown

  Identity

  - ID: 0x1302 (family 0x13 = SB Reference)
  - Class: SbK1WaveformEffect extends SbK1BaseEffect
  - Origin: Port of K1 Lightwave's light_mode_waveform() (lightshow_modes.h:1393-1507)
  - Category: PARTY

  Memory Layout

  PSRAM (allocated in init, freed in cleanup):
  - SbK1BasePsram (base class): specSmooth[96] + spectralHistory[5][96] + magAvg[96] = ~3.4 KB
  - SbK1WaveformPsram (this effect): trailBuffer[160] (CRGB_F = 3 floats each = 1920 bytes) + scrollAccum (4 bytes) = ~1.9 KB
  - Total PSRAM: ~5.3 KB

  DRAM (inline members):
  - Base: m_chromaSmooth[12], m_noveltyCurve[5], hue shift state, VU/peak state = ~200 bytes
  - Effect: m_sensitivity, m_contrast, m_chromaHue = 12 bytes

  Parameters (3, user-tunable via API)

  ┌─────────────┬───────────┬─────────┬────────────────────────────────────────────────────────────────────────────────────┐
  │    Name     │   Range   │ Default │                                      Purpose                                       │
  ├─────────────┼───────────┼─────────┼────────────────────────────────────────────────────────────────────────────────────┤
  │ sensitivity │ 0.01–10.0 │ 1.0     │ Scales dot displacement — higher = more travel for quiet audio                     │
  ├─────────────┼───────────┼─────────┼────────────────────────────────────────────────────────────────────────────────────┤
  │ contrast    │ 0.0–3.0   │ 1.0     │ Contrast curve iterations — squares chromagram bins N times, suppresses weak notes │
  ├─────────────┼───────────┼─────────┼────────────────────────────────────────────────────────────────────────────────────┤
  │ chromaHue   │ 0.0–1.0   │ 0.0     │ Manual hue offset added to auto-shifting hue position                              │
  └─────────────┴───────────┴─────────┴────────────────────────────────────────────────────────────────────────────────────┘

  Per-Frame Pipeline (7 stages)

  The base class render() calls baseProcessAudio(ctx) first, then renderEffect(ctx). The full pipeline per frame:

  ---
  STAGE 0 — Audio Path Selection (base class)

  On ESV11 (production), the CONTROLBUS path is used:
  - m_chromaSmooth[0..11] ← controlBus.chroma[0..11] (backend-agnostic, adapter-normalised)
  - Novelty ← flux (spectral flux from adapter)
  - processColorShift(novelty) — K1's 6-step hue walk algorithm
  - updateWaveformPeak(ctx) — two-stage envelope follower on raw waveform

  The SB parity shortcut is gated out on ESV11 (hardened gate requires sbHue > 0.01 AND chrEnergy > 0.5).

  ---
  STAGE 1 — Colour Synthesis (lines 145-193)

  Two modes controlled by ctx.saturation:

  Chromatic mode (saturation >= 128):
  - Iterate 12 chroma bins
  - Apply contrast curve: bright = applyContrast(bin, m_contrast) — this does bin^(2^contrast), so at contrast=1.0 it squares the bin
  value, suppressing weak notes and emphasising dominant ones
  - Skip bins below 0.05 brightness threshold
  - For each passing bin: look up palette colour at position c/12 scaled by brightness, accumulate additively into dotColor
  - Soft-knee cap: if any RGB channel exceeds 1.0, scale all channels down proportionally (preserves hue ratios, prevents white-out)

  Non-chromatic mode (saturation < 128):
  - Same accumulation loop runs to compute totalMag
  - Then overrides dotColor entirely: single palette lookup at chromaHue + huePosition, brightness = min(totalMag, 1.0)
  - Result: monochromatic dot whose brightness tracks total chromatic energy

  Failsafe (both modes): If totalMag < 0.01 but rms > 0.02 (audio present but chromagram empty), force a palette colour at rms
  brightness. Prevents invisible dot during audio.

  Photons scaling: dotColor *= ctx.brightness / 255.0 then clip to [0,1].

  ---
  STAGE 2 — Trail Fade (lines 195-211)

  Exponential decay applied to the entire 160-pixel trail buffer. The rate is amplitude-dependent:

  decayRate = 0.8 + 3.5 * |m_wfPeakScaled|
  fade = exp(-decayRate * dt)

  ┌────────────────┬────────────┬────────────┬───────────────────────────┐
  │   Amplitude    │ Decay rate │ Time to 5% │          Visual           │
  ├────────────────┼────────────┼────────────┼───────────────────────────┤
  │ 0.0 (silence)  │ 0.8/s      │ ~3.7s      │ Long, ghostly persistence │
  ├────────────────┼────────────┼────────────┼───────────────────────────┤
  │ 0.5 (moderate) │ 2.55/s     │ ~1.2s      │ Visible trail ~1s         │
  ├────────────────┼────────────┼────────────┼───────────────────────────┤
  │ 1.0 (full)     │ 4.3/s      │ ~0.7s      │ Tight, punchy             │
  └────────────────┴────────────┴────────────┴───────────────────────────┘

  Every pixel in trailBuffer[0..159] is multiplied by fade each frame. This is dt-corrected — same visual result at 60 or 240 FPS.

  ---
  STAGE 3 — Trail Scroll (lines 213-229)

  The trail shifts outward from centre at a dt-corrected rate:

  scrollRate = 150 px/s * (ctx.speed / 10.0)
  scrollAccum += scrollRate * dt
  pixelsToScroll = floor(scrollAccum)   // integer pixels this frame
  scrollAccum -= pixelsToScroll         // keep sub-pixel residual

  At default speed (10), this produces ~1.25 pixels/frame at 120 FPS. The sub-pixel accumulator prevents frame-rate aliasing.

  The scroll operation shifts trailBuffer indices rightward (toward 159), zeroing position 0. This means content radiates outward from
   the left edge of the buffer — but the mirror in Stage 6 will fold it symmetrically.

  ---
  STAGE 4 — Dot Position (lines 231-243)

  The waveform peak drives the dot's position on the strip:

  amp = m_wfPeakLast                    // 0-1, EMA-smoothed (tau=23ms)
  amp *= 0.7 / sensitivity              // sensitivity scales the range
  amp = clamp(amp, -1.0, 1.0)
  posF = 80 + amp * 80                  // maps to pixel 80-160 (right half)

  m_wfPeakLast comes from the two-stage envelope follower in the base class:
  - Stage 1 (tau=16ms): fast symmetric follower on raw normalised peak — smooths transients
  - Stage 2 (tau=23ms): slower EMA — produces the final dot position value

  At sensitivity=1.0, the dot spans roughly pixels 80–136 (centre to ~70% of the right half). Cranking sensitivity to 0.1 would push
  it to the edge.

  ---
  STAGE 5 — Sub-Pixel Dot Rendering (lines 245-259)

  The dot is blended across two adjacent pixels using the fractional position:

  posI = floor(posF)          // integer pixel index
  frac = posF - posI          // 0.0-1.0 fractional part

  trailBuffer[posI]   += dotColor * (1.0 - frac)
  trailBuffer[posI+1] += dotColor * frac

  This is additive — the dot adds to whatever trail content already exists at that position. Combined with the fade, this creates the
  characteristic bright-dot-with-fading-trail pattern. Sub-pixel blending eliminates the 1-pixel stepping artifact that would
  otherwise be visible at slow dot movement.

  ---
  STAGE 6 — Centre Mirror (lines 261-264)

  for i in 0..79:
      trailBuffer[79 - i] = trailBuffer[80 + i]

  The right half (pixels 80-159) is copied to the left half (pixels 79-0), creating perfect bilateral symmetry around the centre pair
  (LEDs 79/80). This is the centre-origin constraint in action — all visual content originates from the centre and radiates outward.

  ---
  STAGE 7 — Output (lines 266-276)

  for i in 0..159:
      ctx.leds[i] = trailBuffer[i].toCRGB()      // float→uint8 with clip

  for i in 0..159:
      ctx.leds[160 + i] = ctx.leds[i]            // copy strip 1 → strip 2

  The float trail buffer is quantised to 8-bit RGB for FastLED. Strip 2 gets an identical copy (the EdgeMixer in the RendererActor
  then applies hue splitting to strip 2 for LGP depth differentiation).

  ---
  Audio Data Flow (end-to-end)

  Microphone → I2S DMA → AudioActor (Core 0)
    → EsV11Backend (Goertzel 64-bin)
    → EsV11Adapter (normalise, chroma fold, waveform peak)
    → ControlBusFrame (published via SnapshotBuffer)
    → RendererActor (Core 1, reads latest frame)
    → SbK1BaseEffect::baseProcessAudio()
      → m_chromaSmooth[12] from controlBus.chroma[12]
      → m_wfPeakScaled/m_wfPeakLast from updateWaveformPeak()
      → m_huePosition from processColorShift(flux)
    → SbK1WaveformEffect::renderEffect()
      → dot colour from chromagram + palette
      → dot position from waveform peak
      → trail fade + scroll + mirror → LEDs

  What Makes It "K1 Waveform" vs Other Waveform Effects

  The key differentiators from WaveformParityEffect (SB 3.1.0 port) and AudioWaveformEffect:

  1. Chromagram colour synthesis — the dot colour is an additive sum of 12 pitch-class colours, not a single hue. Musical harmony
  directly drives visual colour mixing.
  2. K1 hue shift — a 6-step novelty-driven algorithm slowly walks the hue position, giving organic colour evolution over time.
  3. Amplitude-dependent trail fade — loud = short punchy trails, quiet = long ghostly persistence. Other waveform effects use fixed
  decay.
  4. Two-stage peak follower — smooths the dot movement without losing transient punch.
  5. Palette-driven — uses the user-selected palette, not hardcoded HSV. Same algorithm, different aesthetic per palette.

  Parity Audit: SB 3.0.0 Waveform vs K1 Port

  The Bombshell

  light_mode_waveform() is dead code in SB 3.0.0. It exists at lightshow_modes.h:128-228 but is never called
  from render_leds(). The .ino dispatches to GDFT, CHROMAGRAM, BLOOM, BLOOM_FAST, VU, and VU_DOT — no WAVEFORM
  branch. The mode name array has "WAVEFORM" at index 4, but the enum has LIGHT_MODE_VU at index 4. The
  function was planned but never wired in.

  The K1 port is based on a function that never shipped as an active mode.

  The Fundamental Divergence

  Even setting aside the dead-code issue, the algorithms are architecturally different:

  ┌──────────────┬────────────────────────────────────────────────────────┬───────────────────────────────┐
  │              │                  SB 3.0.0 (dead code)                  │            K1 Port            │
  ├──────────────┼────────────────────────────────────────────────────────┼───────────────────────────────┤
  │ Visual       │ Full-strip oscilloscope — all 128 LEDs show the        │ Bouncing dot with fading      │
  │ concept      │ waveform shape simultaneously                          │ trails                        │
  ├──────────────┼────────────────────────────────────────────────────────┼───────────────────────────────┤
  │ LED mapping  │ Each LED = one waveform sample (4-frame averaged,      │ Single dot position from peak │
  │              │ temporally smoothed)                                   │  amplitude                    │
  ├──────────────┼────────────────────────────────────────────────────────┼───────────────────────────────┤
  │ Trail system │ None — full rewrite every frame                        │ Exponential fade +            │
  │              │                                                        │ dt-corrected scroll           │
  ├──────────────┼────────────────────────────────────────────────────────┼───────────────────────────────┤
  │ Brightness   │ Pedestal: 0.5 + sample*0.5 (all LEDs at least 50% of   │ Dot only — rest fades to      │
  │              │ peak brightness)                                       │ black                         │
  └──────────────┴────────────────────────────────────────────────────────┴───────────────────────────────┘

  This is not a port with drift. It's a completely different mode that shares the chromagram colour synthesis.

  Colour Synthesis Divergences (the shared part)

  ┌─────┬─────────────────┬───────────────────────────────┬─────────────────────────┬─────────────────────┐
  │  #  │      Area       │           SB 3.0.0            │         K1 Port         │      Severity       │
  ├─────┼─────────────────┼───────────────────────────────┼─────────────────────────┼─────────────────────┤
  │ 1   │ 1.5x brightness │ bright *= 1.5; clamp(1.0)     │ Missing                 │ HIGH — K1 bins are  │
  │     │  boost          │ after squaring                │                         │ dimmer              │
  ├─────┼─────────────────┼───────────────────────────────┼─────────────────────────┼─────────────────────┤
  │ 2   │ led_share       │ Multiplies bin brightness by  │ Not present             │ HIGH — magnitude    │
  │     │ scaling         │ 255/12 = 21.25                │                         │ difference          │
  ├─────┼─────────────────┼───────────────────────────────┼─────────────────────────┼─────────────────────┤
  │     │ Temporal RGB    │ Heavy 0.05/0.95 EMA on output │ None (relies on         │ HIGH — K1 colour    │
  │ 3   │ smoothing       │  RGB (~20-frame inertia)      │ pre-smoothed            │ changes ~20x faster │
  │     │                 │                               │ chromagram)             │                     │
  ├─────┼─────────────────┼───────────────────────────────┼─────────────────────────┼─────────────────────┤
  │ 4   │ 0.05f threshold │ Processes ALL bins including  │ Skips bins below 0.05   │ LOW                 │
  │     │                 │ near-zero                     │                         │                     │
  ├─────┼─────────────────┼───────────────────────────────┼─────────────────────────┼─────────────────────┤
  │ 5   │ HSV function    │ hsv2rgb_spectrum()            │ Palette lookup via      │ INTENTIONAL         │
  │     │                 │                               │ paletteColorF()         │                     │
  ├─────┼─────────────────┼───────────────────────────────┼─────────────────────────┼─────────────────────┤
  │ 6   │ Non-chromatic   │ Fixed from CHROMA knob        │ Drifts via              │ MEDIUM              │
  │     │ hue             │ (chroma_val)                  │ m_huePosition           │                     │
  ├─────┼─────────────────┼───────────────────────────────┼─────────────────────────┼─────────────────────┤
  │ 7   │ Chromatic       │ Top 5% of knob range (>=      │ Top 50% of saturation   │ LOW                 │
  │     │ threshold       │ 0.95)                         │ (>= 128)                │                     │
  └─────┴─────────────────┴───────────────────────────────┴─────────────────────────┴─────────────────────┘

  K1 Features Not In SB

  - Dynamic amplitude-responsive trail fade
  - dt-corrected sub-pixel scroll with speed lever
  - Sub-pixel dot rendering (fractional blending)
  - Invisible dot failsafe
  - Palette-driven colour (user-selectable)
  - Sensitivity / contrast / hue offset parameters
  - Soft-knee brightness cap
  - Dual-strip output + mandatory centre-origin mirror

  SB Features Missing From K1

  - Full-waveform oscilloscope rendering (the entire visual concept)
  - 4-frame waveform history averaging per LED position
  - Per-sample temporal smoothing controlled by MOOD knob
  - Brightness pedestal (50% minimum fill)
  - RGB temporal smoothing (0.05/0.95 EMA)

  File Header Accuracy

  The K1 file header claims:
  ▎ "Exact algorithm from lightshow_modes.h:1393-1507"
  ▎ "K1 Waveform — exact parity with light_mode_waveform()"

  Neither is accurate. Lines 1393-1507 don't exist in the 228-line function (may reference a different SB
  version). The algorithm is fundamentally different.

  ---
  What Do You Want To Do?

  The question is: what's the intended target?

  Option A: Accept the K1 Waveform as its own mode (bouncing dot with chromagram colour) and fix the header
  comments to accurately describe what it does. Apply the 1.5x brightness boost to bring colour synthesis
  closer to SB's intent.

  Option B: Build a true SB waveform parity mode — full-strip oscilloscope showing the actual waveform shape,
  with the pedestal brightness, temporal smoothing, and MOOD-controlled responsiveness. This would be a new
  effect alongside the current one.

  Option C: Hybrid — keep the current K1 dot mode but fix the colour synthesis divergences (brightness boost,
  temporal smoothing, threshold removal) so the colour character matches SB's even if the visual layout
  doesn't.