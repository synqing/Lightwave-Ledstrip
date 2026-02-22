# Kuramoto Transport Implementation Guide (LightwaveOS)

This guide implements a *separated* pipeline:

1. **KuramotoOscillatorField** (invisible engine)
2. **KuramotoFeatureExtractor** (derived events + velocity field)
3. **KuramotoTransportBuffer** (visible “light substance”)
4. **KuramotoTransportEffect** (IEffect wrapper + audio steering)

The rule: **nothing in audio directly sets pixel brightness.** Audio only steers *engine parameters*.

---

## Geometry assumptions

LightwaveOS uses centre-origin symmetry within each 160-LED strip.

- `radialLen = ctx.centerPoint + 1` (expected 80)
- `dist = 0` maps to the centre pair `(centrePoint, centrePoint+1)`
- `dist` increases outward
- If `ctx.ledCount >= 320`, treat output as two concatenated 160-LED strips.

---

## 1) KuramotoOscillatorField

### Per-zone state
A single effect instance is rendered multiple times in zone mode. Store separate engine state per zone:
- `theta[N]` (phase)
- `omega[N]` (natural frequency)
- `prevTheta[N]` (for phase-slip detection)

Suggested: `N = 80` to match `radialLen`.

### Nonlocal coupling (first-class)
Chimera-like regimes require intermediate/nonlocal coupling on a ring. citeturn1search0turn1search3

Use a kernel over an integer radius `R`:
- cosine kernel (Abrams/Strogatz) or
- Gaussian-ish weights

### Practical regime control
Skip analytic \(K_c\) arguments; use a robust dimensionless knob:

`sync_ratio = K / (freqSpread + eps)`

### Integration
Use **Heun / RK2** (reduces frame-rate dependence vs Euler). citeturn1search11

Clamp dt (e.g., 1–30ms) to avoid rare long frames blowing up the engine.

---

## 2) KuramotoFeatureExtractor

The extractor turns phases into *events* and a *velocity field*.

Outputs (length N):
- `velocity[i]` in [-1, +1] (from phase gradient)
- `coherence[i]` in [0, 1] (local order parameter)
- `event[i]` in [0, 1] (injection strength)

Compute:
- `phase_gradient[i] = wrap(theta[i+1]-theta[i-1]) / 2`
- `curvature[i] = wrap(theta[i+1]-2*theta[i]+theta[i-1])`
- `r_local[i] = |mean(exp(j*theta[j]))|` over the same nonlocal neighbourhood
- `coherence_edge[i] = |r_local[i]-r_local[i±1]|`
- `phase_slip[i] = step(|wrap(theta[i]-prevTheta[i])| - slipThresh)`

Event recipe (example):

`event = wSlip*slip + wEdge*edge + wCurv*abs(curvature)`

Apply a soft threshold and limiter so you don’t inject everywhere.

---

## 3) KuramotoTransportBuffer

This is the thing you see.

Each frame per zone:
1. **Advect** the history buffer by a *fractional* offset using 2-tap interpolation.
2. **Apply persistence** with dt-correction.
3. **Diffuse** slightly (1D bloom / viscous smear).
4. **Inject** energy at positions where `event[i]` is strong.

Conceptually: this is the semi-Lagrangian “advect then interpolate” move used in stable real-time fluids. citeturn1search2turn1search12

Advection should use a *velocity field*:
- local sign/magnitude from `phase_gradient` (not a single global speed)

Readout:
- mirror the radial buffer to centre-origin LEDs
- optional tone-map to avoid hard clipping/whitewash

---

## 4) KuramotoTransportEffect (IEffect)

The wrapper is responsible for:
- selecting `zoneIndex`
- computing dt
- audio steering (parameters only)
- stepping engine
- extracting features
- transport advect + inject
- final readout to `ctx.leds`

Audio steering (allowed):
- `K`, `freqSpread`, `noiseSigma`, `kickRateHz`, palette drift

Audio steering (forbidden):
- `rms → LED brightness`, `flux → LED brightness`, etc.

---

## Acceptance criteria

A1) Looks alive with *no audio*.

A2) Doesn’t settle into a 2–4 second loop.

A3) 60 vs 120fps looks similar (trail length + event timing).

A4) Turning one knob (sync_ratio) shifts regimes smoothly (incoherent → chimera-ish → coherent).
