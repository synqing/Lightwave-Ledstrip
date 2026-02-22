# Agent Brief: Kuramoto-Driven Event Transport (LightwaveOS)

## Non-negotiables

1) **The Kuramoto field is invisible.**
   - Do **not** render phase directly.
   - Do **not** map audio features to per-LED brightness.

2) **You render transported light substance.**
   - Derived events inject energy into a persistent buffer.
   - That buffer advects (subpixel) + decays + diffuses.

3) **Nonlocal coupling is first-class.**
   - Include coupling radius R and kernel weights.
   - Nearest-neighbour coupling alone is not the goal.

4) **Frame-rate independence.**
   - Use RK2/Heun for oscillator integration.
   - Use dt-correct persistence + dt-correct advection.

## Pipeline

Invisible Engine:
- `KuramotoOscillatorField` (N=80 ring)
- audio steers: K, spread, noise, kick_rate, palette drift

Feature extraction:
- `phase_gradient` → velocity v[i]
- `coherence_edge`, `phase_slip`, `curvature` → event[i]

Visible output:
- `KuramotoTransportBuffer`
  - advectWithVelocity(v[i])
  - injectAtPos(i)
  - diffuse + tone-map
  - readout to centre-origin LEDs

## Acceptance checks

A1) Looks alive with NO audio
A2) Surprises after 60+ seconds (not a loop)
A3) Not a “ring on beat” pattern
A4) Sync/chaos regimes shift smoothly with one knob (sync_ratio)
