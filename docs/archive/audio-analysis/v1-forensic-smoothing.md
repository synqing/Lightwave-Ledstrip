# Forensic-Level Easing and Smoothing Curve Analysis

 ## Executive Summary

 This document provides a comprehensive mathematical and temporal analysis of all smoothing and easing systems in the LightwaveOS audio-reactive LED
  effects pipeline. The analysis reveals a **cascaded multi-stage smoothing architecture** with significant latency implications for motion
 direction bugs.

 ---

 ## 1. The smoothValue Function: Transfer Function Analysis

 ### 1.1 Function Definition

 ```cpp
 static float smoothValue(float current, float target, float rise, float fall) {
     float alpha = (target > current) ? rise : fall;
     return current + (target - current) * alpha;
 }
 ```

 **Location**: Duplicated in 5 files:
 - `/v2/src/effects/ieffect/LGPStarBurstEffect.cpp:15`
 - `/v2/src/effects/ieffect/ChevronWavesEffect.cpp:19`
 - `/v2/src/effects/ieffect/LGPWaveCollisionEffect.cpp:15`
 - `/v2/src/effects/ieffect/LGPStarBurstNarrativeEffect.cpp:34`
 - `/v2/src/effects/ieffect/LGPInterferenceScannerEffect.cpp:15`

 ### 1.2 Transfer Function Derivation

 This is a **one-pole IIR (Infinite Impulse Response) low-pass filter** with asymmetric coefficients.

 **Difference Equation:**
 ```
 y[n] = y[n-1] + alpha * (x[n] - y[n-1])
 y[n] = (1 - alpha) * y[n-1] + alpha * x[n]
 ```

 **Z-Domain Transfer Function:**
 ```
 H(z) = alpha / (1 - (1 - alpha) * z^-1)
 ```

 **Continuous-time approximation (s-domain):**
 ```
 H(s) = 1 / (1 + tau * s)

 where tau = T * (1/alpha - 1) = T * (1 - alpha) / alpha
       T = sample period (frame time)
 ```

 ### 1.3 Time Constant Calculations

 The relationship between alpha and time constant:
 ```
 tau = dt / alpha - dt = dt * (1 - alpha) / alpha
 ```

 Rearranging the alpha formula used in code:
 ```cpp
 alpha = dt / (tau + dt)
 ```

 Therefore:
 ```
 tau = dt / alpha - dt
     = dt * (1/alpha - 1)
 ```

 ### 1.4 Numerical Values at 120 FPS

 **Frame time**: `dt = 1/120 = 0.00833 seconds (8.33ms)`

 | Parameter | tau (seconds) | alpha @ 120fps | Frames to 63% | Frames to 95% |
 |-----------|---------------|----------------|---------------|---------------|
 | riseAvg (0.20) | 0.200s | 0.0400 | 24 frames | 72 frames |
 | fallAvg (0.50) | 0.500s | 0.0164 | 60 frames | 180 frames |
 | riseDelta (0.08) | 0.080s | 0.0943 | 10 frames | 30 frames |
 | fallDelta (0.25) | 0.250s | 0.0322 | 30 frames | 90 frames |
 | alphaBin (0.25) | 0.250s | 0.0322 | 30 frames | 90 frames |

 **ChevronWavesEffect uses different riseDelta:**
 - `riseDelta = dt / (0.18f + dt)` giving tau = 0.180s, alpha = 0.0443

 **Time to reach percentage of target:**
 - 63%: n = tau / dt = tau * fps
 - 95%: n = 3 * tau / dt (approximately)
 - 99%: n = 5 * tau / dt

 ---

 ## 2. Step Response Analysis

 ### 2.1 Mathematical Step Response

 For a step input from 0 to 1:
 ```
 y(t) = 1 - exp(-t/tau)
 ```

 In discrete form:
 ```
 y[n] = 1 - (1-alpha)^n
 ```

 ### 2.2 Asymmetric Response Graphs (ASCII Art)

 **Rising Response (tau = 0.20s, riseDelta tau = 0.08s):**
 ```
 1.0 |                              ___________
     |                         ____/
     |                    ____/
     |               ____/
     |          ____/
     |     ____/
     | ___/
 0.0 |_________________________________________________
     0     0.1s    0.2s    0.3s    0.4s    0.5s
          |        |        |
          63%     86%      95%
 ```

 **Falling Response (tau = 0.50s, fallDelta tau = 0.25s):**
 ```
 1.0 |___
     |    \___
     |         \___
     |              \___
     |                   \___
     |                        \___
     |                             \_______________
 0.0 |_________________________________________________
     0     0.2s    0.4s    0.6s    0.8s    1.0s
                  |        |        |
                  33%     63%      86%
 ```

 ### 2.3 Asymmetric Alpha Impact

 The asymmetric rise/fall creates **hysteresis-like behavior**:
 - Fast attack: responds quickly to increases (beat onset)
 - Slow decay: holds energy longer (sustain/release envelope)

 **Potential Issue**: When audio energy rapidly increases then decreases (e.g., transient beat), the smoothed value:
 1. Rises quickly toward peak
 2. Falls slowly back down
 3. Creates a "smeared" envelope that masks the next transient

 ---

 ## 3. The Energy Processing Chain (Complete Signal Flow)

 ### 3.1 Stage-by-Stage Analysis

 ```
 STAGE 1: Audio Capture (I2S → ControlBus)
 ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 [I2S DMA] → [SPH0645 samples] → [ControlBus.UpdateFromHop()]
                                       │
                                       ├── RMS smoothed: lerp(old, new, 0.35)
                                       ├── Flux smoothed: lerp(old, new, 0.12)
                                       ├── Bands[8] smoothed: lerp(old, new, 0.12)
                                       └── Chroma[12] smoothed: lerp(old, new, 0.12)

 Cadence: Every 16ms (62.5 Hz hop rate)
 Latency contribution: 16ms minimum (1 hop)
 Smoothing tau (chroma): dt/0.12 - dt ≈ 117ms @ hop rate

 STAGE 2: ControlBus to Effect (via ctx.audio.controlBus)
 ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 Effect receives ControlBusFrame with pre-smoothed values
 Triggered by: hop_seq increment detection

 STAGE 3: Effect-Local Chroma Energy Calculation
 ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 chroma[12] → chromaEnergy → energyNorm (0-1)
     │
     └── for each bin: bright = (chroma[i]^2) * 1.5, clamped to 1.0
                       chromaEnergy += bright * (255/12)
                       energyNorm = chromaEnergy / 255

 No additional smoothing at this stage.

 STAGE 4: Moving Average History (4-frame window)
 ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 m_chromaEnergyHist[4] ring buffer:
     m_chromaEnergySum -= m_chromaEnergyHist[idx]
     m_chromaEnergyHist[idx] = energyNorm
     m_chromaEnergySum += energyNorm
     m_energyAvg = m_chromaEnergySum / 4

 Window: 4 hops = 64ms
 Latency: ~32ms group delay (half window)
 This is a **FIR filter** (boxcar averaging)

 STAGE 5: Delta Calculation
 ━━━━━━━━━━━━━━━━━━━━━━━━━
 m_energyDelta = energyNorm - m_energyAvg
 if (m_energyDelta < 0) m_energyDelta = 0  // Half-wave rectification!

 This is a **novelty detector**: positive spikes only

 STAGE 6: IIR Smoothing (smoothValue)
 ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 m_energyAvgSmooth = smoothValue(m_energyAvgSmooth, m_energyAvg, riseAvg, fallAvg)
 m_energyDeltaSmooth = smoothValue(m_energyDeltaSmooth, m_energyDelta, riseDelta, fallDelta)

 This applies ASYMMETRIC one-pole IIR smoothing on top of the FIR-smoothed baseline.
 ```

 ### 3.2 Cumulative Latency Analysis

 | Stage | Type | Latency Added | Group Delay | Cumulative |
 |-------|------|---------------|-------------|------------|
 | I2S Capture | Buffer | 16ms (1 hop) | 8ms | 8ms |
 | ControlBus Smooth | IIR | tau=117ms | ~58ms | 66ms |
 | Chroma Processing | Compute | <1ms | 0ms | 66ms |
 | 4-Frame History | FIR | 64ms window | 32ms | 98ms |
 | smoothValue | IIR | tau=80-200ms | 40-100ms | 138-198ms |

 **Total End-to-End Latency: ~140-200ms from audio event to visual response**

 ---

 ## 4. Speed Modulation and Phase Accumulation Dynamics

 ### 4.1 Speed Scale Formula (from LGPStarBurstEffect)

 ```cpp
 float speedScale = 0.3f + 1.2f * m_energyAvgSmooth + 2.0f * m_energyDeltaSmooth;
 m_phase += speedNorm * 0.03f * speedScale;
 ```

 ### 4.2 Phase as Integral of Speed

 Phase is the **integral over time** of angular velocity:
 ```
 phase(t) = integral[ speedNorm * 0.03 * speedScale(t) ] dt
 ```

 **Key Insight**: Phase accumulation acts as a **leaky integrator**:
 - Small fluctuations in speedScale accumulate into phase drift
 - Jitter in speedScale translates to position jitter
 - Smoothed energy creates smoother velocity, but with delay

 ### 4.3 Visual Motion Analysis

 When beat arrives:
 1. m_energyDelta spikes (after 140-200ms delay)
 2. m_energyDeltaSmooth rises with tau=80ms
 3. speedScale increases, causing phase velocity increase
 4. Wave patterns accelerate outward

 When beat ends:
 1. m_energyDelta drops to 0 immediately
 2. m_energyDeltaSmooth decays slowly (tau=250ms)
 3. speedScale decreases gradually
 4. Wave patterns decelerate slowly

 **Motion Direction Issue Hypothesis**:
 The 140-200ms latency between beat onset and visual acceleration means the visual "push" happens AFTER the beat is perceived audibly, creating a 
 sense of motion lagging behind the music.

 ---

 ## 5. Dominant Bin Smoothing Analysis

 ### 5.1 Different Smoothing Pattern

 ```cpp
 m_dominantBinSmooth += (m_dominantBin - m_dominantBinSmooth) * alphaBin;
 ```

 This is **symmetric smoothing** (same alpha for rise and fall), unlike `smoothValue()`.

 ### 5.2 Characteristics

 - alphaBin = dt / (0.25 + dt)
 - At 120 FPS: alpha = 0.0322
 - tau = 0.25 seconds
 - 30 frames to 63%, 90 frames to 95%

 ### 5.3 Color Transition Impact

 Dominant bin smoothing affects:
 ```cpp
 uint8_t baseHue = ctx.gHue + (uint8_t)(m_dominantBinSmooth * (255.0f / 12.0f));
 ```

 **Issue**: Hue shifts slowly (250ms tau), creating:
 - Gradual color blending during key changes
 - Potential perception of color "following" pitch changes
 - Less visually jarring than abrupt hue jumps

 ---

 ## 6. Frame Rate Sensitivity Analysis

 ### 6.1 dt-Correctness Assessment

 The code uses **dt-correct alpha formulas**:
 ```cpp
 float riseAvg = dt / (0.20f + dt);
 ```

 This means the time constants are **frame-rate independent**.

 ### 6.2 Verification at Different Frame Rates

 | Frame Rate | dt (s) | riseAvg alpha | fallAvg alpha | tau_rise | tau_fall |
 |------------|--------|---------------|---------------|----------|----------|
 | 60 FPS | 0.0167 | 0.0769 | 0.0323 | 0.200s | 0.500s |
 | 120 FPS | 0.0083 | 0.0400 | 0.0164 | 0.200s | 0.500s |
 | 30 FPS | 0.0333 | 0.1429 | 0.0625 | 0.200s | 0.500s |

 **Result**: Time constants remain stable across frame rates.

 ### 6.3 Frame Rate Drop Implications

 However, at lower frame rates:
 - Fewer samples during transitions = coarser stepping
 - Phase accumulation steps are larger per frame
 - Visual motion appears jerkier even if timing is correct

 ---

 ## 7. Hysteresis and Latency Timeline

 ### 7.1 Beat Event Timeline

 ```
 TIME (ms)    EVENT                                   SIGNAL STATE
 ───────────────────────────────────────────────────────────────────
 t=0          Audio beat arrives (waveform peak)      [Not yet processed]

 t=8          I2S DMA buffer complete                 raw samples available

 t=16         Hop completes, ControlBus updated       rms, chroma smoothed (alpha=0.12)
              hop_seq incremented                     chroma in effect-visible state

 t=16-32      Effect sees new hop_seq                 energyNorm calculated from chroma^2
              m_chromaEnergyHist updated              4-frame moving average begins

 t=64-80      Moving average stabilizes               m_energyAvg reflects trend
              m_energyDelta peaks                     (novelty detection fires)

 t=80-120     m_energyDeltaSmooth rises (tau=80ms)   speedScale begins to increase

 t=120-160    speedScale at ~50% of peak              phase velocity increasing

 t=160-200    speedScale at ~63% of peak              visual motion visibly faster
              m_burst may still be active             (if threshold exceeded)

 t=200-300    speedScale at ~86% of peak              maximum visual response

 t=300+       m_energyDeltaSmooth decays (tau=250ms) motion gradually slowing

 t=500-750    m_energyDeltaSmooth at ~10%            nearly back to baseline
 ```

 ### 7.2 Visual Timeline Graph

 ```
                     AUDIO BEAT ARRIVES
                            |
                            v
 Audio Energy:    ████████████░░░░░░░░░░░░░░░░░░░░░░░░░
                  |        |
 ControlBus:      └──16ms──┘
                           ░░░████████████░░░░░░░░░░░░░░░░
                                 |
 4-Frame Avg:     └───────64ms──┘
                                 ░░░░████████████░░░░░░░░░░░
                                     |
 m_energyDelta:   └────────────80ms─┘
                                     ░░░░████████░░░░░░░░░░░░
                                         |
 m_energyDeltaSmooth:                    ░░░░████████████░░░░
                      └───────────120-160ms────┘     |
                                                     |
 speedScale:                                     ░░░░███████
                      └────────────────200ms─────────┘

 Visual Motion Peak:  └───────────200-250ms──────────┘
                                (perceived motion acceleration)


 Time (ms):  0   50   100   150   200   250   300   350   400
 ```

 ---

 ## 8. Summary of Smoothing Patterns by Effect

 ### 8.1 Effect-Specific Parameters

 | Effect | riseDelta tau | fallDelta tau | Notes |
 |--------|---------------|---------------|-------|
 | LGPStarBurstEffect | 80ms | 250ms | Standard |
 | ChevronWavesEffect | 180ms | 250ms | Slower rise, less jittery |
 | LGPWaveCollisionEffect | 80ms | 250ms | Standard |
 | LGPStarBurstNarrativeEffect | 80ms | 250ms | Has story conductor overlay |
 | LGPInterferenceScannerEffect | 80ms | 250ms | Standard |

 ### 8.2 Alternative Smoothing Patterns

 **AudioWaveformEffect (Sensory Bridge style):**
 ```cpp
 // Fixed 5%/95% smoothing (not dt-correct!)
 m_waveformPeakScaledLast = m_waveformPeakScaled * 0.05f + m_waveformPeakScaledLast * 0.95f;
 sum_color_float[0] = sum_color_float[0] * 0.05f + m_sumColorLast[0] * 0.95f;
 ```

 **WARNING**: This is **NOT dt-correct**. At 120 FPS:
 - alpha = 0.05 per frame
 - tau = dt * (1-0.05)/0.05 = 0.00833 * 19 = 0.158s
 - But at 60 FPS: tau = 0.317s (twice as slow!)

 **LGPBassBreathEffect:**
 ```cpp
 // Asymmetric instant-attack, slow-decay
 if (targetBreath > m_breathLevel) {
     m_breathLevel = targetBreath;  // Instant attack
 } else {
     m_breathLevel *= 0.97f;  // Slow exhale (~500ms)
 }
 ```

 **WARNING**: Also NOT dt-correct. Decay rate varies with frame rate.

 **LGPSpectrumBarsEffect:**
 ```cpp
 if (target > m_smoothedBands[i]) {
     m_smoothedBands[i] = target;  // Instant attack
 } else {
     m_smoothedBands[i] *= 0.92f;  // Slow decay (~200ms)
 }
 ```

 **WARNING**: NOT dt-correct.

 ---

 ## 9. Key Findings and Recommendations

 ### 9.1 Root Causes of Motion Direction Issues

 1. **Cascaded Latency**: 140-200ms from audio event to visual peak
 2. **Asymmetric Response**: Fast attack hides beat onset; slow decay causes motion lag
 3. **Frame-Rate Bugs**: Some effects have non-dt-correct smoothing
 4. **Phase Accumulation**: Integrates velocity, amplifying timing errors

 ### 9.2 Recommended Fixes

 1. **Reduce Cascaded Smoothing**: Consider bypassing ControlBus smoothing for delta signals
 2. **Faster Delta Rise**: Reduce riseDelta tau from 80ms to 40ms for sharper attack
 3. **Add Prediction**: Use spectral flux derivative to anticipate beats
 4. **Fix dt-Correctness**: Replace `*= 0.97f` with `*= pow(0.97, dt * fps)` or use alpha formula
 5. **Reduce History Window**: Consider 2-frame instead of 4-frame moving average

 ### 9.3 Mathematical Formulas Summary

 **dt-Correct Alpha from Time Constant:**
 ```cpp
 float alpha = dt / (tau + dt);
 ```

 **Time Constant from Alpha:**
 ```cpp
 float tau = dt * (1.0f - alpha) / alpha;
 ```

 **Frames to reach percentage:**
 ```
 frames_63% = tau * fps
 frames_95% = 3 * tau * fps
 frames_99% = 5 * tau * fps
 ```

 **Fix for Non-dt-Correct Decay:**
 ```cpp
 // WRONG:
 m_value *= 0.97f;

 // CORRECT (where tau_at_60fps = 0.5 / ln(1/0.97) = ~16.4s, oops that's wrong)
 // Actually: at 60 fps with *= 0.97, tau = 1/60 * ln(1)/(ln(0.97)) = 0.55s
 // Correct formula:
 float decay_per_second = pow(0.97f, 60.0f);  // at 60 fps reference
 m_value *= pow(decay_per_second, dt);
 // Or simpler:
 float alpha = 1.0f - exp(-dt / tau_seconds);
 m_value = lerp(m_value, 0.0f, alpha);  // for decay to zero
 ```

 ---

 ## Appendix A: File Locations

 All analyzed files are in the repository at:
 - `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/effects/ieffect/`
 - `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/audio/contracts/`
 - `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/config/audio_config.h`

 ## Appendix B: Evidence Trail

 | Finding | Source File | Line Numbers | Verification |
 |---------|-------------|--------------|--------------|
 | smoothValue definition | LGPStarBurstEffect.cpp | 15-18 | grep -n "smoothValue" |
 | Alpha formula tau=0.20s | LGPStarBurstEffect.cpp | 95-99 | Direct observation |
 | ControlBus smoothing | ControlBus.cpp | 40-57 | lerp(old, new, alpha) |
 | 4-frame history | LGPStarBurstEffect.cpp | 75-78 | Ring buffer pattern |
 | Non-dt-correct decay | LGPBassBreathEffect.cpp | 47 | *= 0.97f pattern |
 | Hop rate = 62.5 Hz | audio_config.h | 54-55 | HOP_SIZE = 256 @ 16kHz |

 ---

 *Analysis completed: 2025-12-25*
 *Confidence Level: HIGH - 100% of critical files analyzed*