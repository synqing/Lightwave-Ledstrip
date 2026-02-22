# Kuramoto → Events → Transport (LightwaveOS)

**Purpose:** Replace "audio→LED" mapping with an *invisible* dynamical engine (Kuramoto field) that emits *events* which inject light-substance into a Bloom-style transport buffer.

## Why this architecture (and why the old one looks like every other audio-reactive pattern)

Most audio-reactive patterns do this:

**Audio features → brightness/palette per LED per frame**

That often looks "computed" because:
- there’s little/no **state** (history) beyond a few envelopes
- motion has no **inertia** (every frame is a fresh solve)
- the system has no **derived structure** (events) that can surprise you

Instead we want:

**Oscillators → derived events → transported light substance → LEDs**

The Kuramoto field should be *invisible*. What you *see* is transported light.

## Nonlocal coupling is not optional if you want chimera-like regimes

Chimera states (coexistence of coherent and incoherent regions) are strongly associated with **nonlocal coupling** on a ring of oscillators; they are not generally seen with purely local nearest-neighbour coupling or fully global coupling in the classic setting. citeturn1search0turn1search3

So:
- **Coupling radius** must be a first-class parameter.
- A **cosine / Gaussian kernel** is a sane default.

## The invisible engine

We evolve a 1D ring of phases:

\[ \dot\theta_i = \omega_i + \frac{K}{\sum w}\sum_{j} w_{ij}\,\sin(\theta_j - \theta_i) + \eta_i(t) \]

Where:
- \(\theta_i\) phase, \(\omega_i\) natural frequency
- \(K\) coupling strength
- \(w_{ij}\) kernel weights for nonlocal coupling
- \(\eta\) noise / kicks (steering, not rendering)

Order parameter background:
- Global coherence is commonly summarized by \(re^{i\psi}=\frac{1}{N}\sum e^{i\theta_j}\). citeturn1search1turn1search4

### Replace “Kc talk” with a practical regime control

In our use-case, we don’t need analytic \(K_c\) precision; we need a robust steering knob.

Use a dimensionless regime control:

**sync_ratio = K / (freq_spread + ε)**

That behaves like a practical “how synchronized should the field be?” control, without pretending we’re matching idealized assumptions.

## Numerics: don’t use forward Euler here

Forward Euler is cheap but frame-rate dependent and can destabilize oscillatory systems.

Use **Heun / RK2** as the default integrator. citeturn1search11

## Feature extraction (events, not rendering)

From the phase field (per index i):

1) **phase_gradient[i] = ∂θ/∂x**
   - Becomes a direction / “wind” term for transport.

2) **local coherence r_local[i]**
   - Compute over the same nonlocal neighbourhood as coupling (complex average of neighbour phases).

3) **coherence_edge[i] = |r_local[i] − r_local[i±1]|**
   - Boundary detector between coherent/incoherent regions.

4) **phase_slip[i]**
   - Detect big wrapped jumps in Δθ over time (~π scale).
   - These are excellent injection events.

5) **curvature[i] = ∂²θ/∂x²**
   - Compression/expansion / wavefront folding detector.

## Visible output: Transport buffer (“light substance”)

We maintain a HDR-ish history buffer and do **subpixel advection** + persistence:

- **Advect** (move the buffer by fractional offsets) using 2-tap interpolation.
- **Decay** with dt-correct persistence.
- **Diffuse** slightly (1D bloom / viscous smear).

Conceptually, this mirrors the semi-Lagrangian “advect then interpolate” trick used in stable real-time fluid solvers: trace/move quantities, interpolate, and remain stable at real-time timesteps. citeturn1search2turn1search12

### Crucial point

The Kuramoto engine never directly sets LED brightness.

It only:
- steers the oscillator regime (via audio)
- emits events

The transport buffer renders those events into “stuff that moves.”

## Audio steering policy (allowed)

Audio should modulate **parameters**, not pixels:
- sync_ratio (coupling vs spread)
- noiseSigma (stochasticity)
- kickRate (rare perturbations that cause phase slips)
- palette drift (slow colour wandering)

Any mapping that looks like “rms → LED brightness” is explicitly out.

## Acceptance criteria (what success looks like)

A1. Interesting motion with **no audio** (engine self-organizes; transport keeps it alive)

A2. Non-repeating behaviour over minutes (not a 2-second loop)

A3. Frame-rate independence (60 vs 120 fps yields similar trail length and event timing)

A4. Regime diversity using just 2–3 knobs (sync_ratio, radius, diffusion)
