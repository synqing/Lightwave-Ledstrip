You are a Senior Engineer. Implement the following Intent Spec using a sequential workflow.

CRITICAL RULES:
- The 'constraints' section contains constitutional rules you MUST NOT VIOLATE.
- The **Space Context** (product vision, audience, principles) explains WHY we are building this. Read it.
- Your goal is to satisfy ALL 'outcomes'. Each one is a required deliverable.
- Follow the phases below in order. Do not skip phases.

## Phase 1: Orientation
Read the full spec below. Then identify affected files (likely: src/effects/IEffect.h, RendererActor.{h,cpp}, EffectDescriptor / PatternRegistry, the ~12 currently-correct effects, the ~140 audio-reactive effects). Read existing patterns. Note conflicts. Use Glob/Grep/Read.

## Phase 2: Plan
List every file. For each: specific changes. Order: shared types first (SilenceBehaviour enum, EffectDescriptor field), then RendererActor hook, then per-effect tagging audit. Map each change to the outcome it satisfies. Use TodoWrite per outcome.

## Phase 3: Implement
Dependency order. After each file change, verify no breakage. Respect ALL constraints (constitutional rules are non-negotiable).

## Phase 4: Validate

**E2E**: LED output during recorded silence — FrameworkFade extinguishes ≤ ε within τ; IntentionallyPersistent retains brightness; InternalFade honours declared curve.
**Unit**: Descriptor field present on every effect; lint passes on the catalogue.
**Manual**: Backend sampler verifies silentScale → 0 within documented window during a known-silent recording.

# Intent Spec: silentScale framework enforcement: post-render multiplier + SilenceBehaviour descriptor opt-out

**ID**: `bf67678d-700a-4d9d-b4d7-c2c9da3893c5` | **Status**: validated

## Objective

Move the silence contract from convention to framework-level guarantee, AND reconcile the Pathmode wording to match. Current `needsSilenceGate()` requires every author to do three things correctly (opt in, fetch silentScale, apply consistently); only ~12 of ~140+ audio-reactive effects comply. Captain decision 2026-04-26: make RendererActor enforce silence by default; relegate per-effect customisation to a constexpr descriptor field that's compile-time decidable and grep-auditable. Removes both 'forgot to opt in' and 'forgot to multiply' failure modes at once.

## Success Outcomes

- [ ] Pathmode REACTIVE Pattern Contract principle rewritten to: 'Audio-reactive patterns honour a silence contract enforced by the rendering framework: after each effect's render() returns, RendererActor multiplies the output buffer by controlBus.silentScale unless the effect's static EffectDescriptor declares SilenceBehaviour::InternalFade or SilenceBehaviour::IntentionallyPersistent. Default is FrameworkFade. Lint and a backend-side runtime sampler are layered insurance, not the primary mechanism.'
- [ ] EffectDescriptor gains: `constexpr SilenceBehaviour silence{SilenceBehaviour::FrameworkFade};` with variants FrameworkFade (default — framework multiplies output by controlBus.silentScale), InternalFade (effect owns its non-linear curve), IntentionallyPersistent (status/OOBE/idle pulse — must remain visible).
- [ ] RendererActor post-render hook: `if (descriptor.silence == SilenceBehaviour::FrameworkFade) { const float s = ctx.controlBus.silentScale; for (int i = 0; i < kLedCount; ++i) ctx.leds[i].nscale8(uint8_t(s * 255.0f)); }`
- [ ] needsSilenceGate() deprecated; the ~12 currently-correct effects choose between FrameworkFade (strip internal fade) or InternalFade (keep non-linear curve).
- [ ] Lint check (~30 lines Python, runs in CI) flags any descriptor missing the silence field, OR any effect whose render() multiplies by silentScale while declaring FrameworkFade (double-fade smell).
- [ ] Runtime sampler reframed as backend correctness check: 'is controlBus.silentScale actually reaching 0 within the documented window during real silence?' — instrumented once on the audio output, not 140 times.

## Constraints & Constitution

- [!] Framework multiplier overhead ≤ 5 µs at 240 MHz on 320 LEDs (640 floats × 1 mul).
- [!] silenceBehaviour must be constexpr — no runtime branch, no virtual call.
- [!] Both opt-out variants (InternalFade, IntentionallyPersistent) must be greppable so the auditable opt-out list stays current.
- [!] Algorithm change at framework layer (e.g. linear → gamma-corrected) requires Captain re-approval and bumps kSilenceFadeFrameworkVersion.
- [!] Migration is one CL plus per-effect tagging only for the handful claiming non-default behaviour.
- [!] British English.
- [!] Precedence: Constraint > Standard > Pattern.
- [!] Standards: identify source of truth + appropriate verification tier.
- [!] Patterns: prefer existing helpers and conventions before creating new behaviour.

## Edge Cases
- **Effect declares InternalFade but doesn't actually fade** → Goes bright in silence; visible failure during 5 s of hardware testing; lint warns if render() doesn't reference silentScale.
- **Effect declares FrameworkFade AND multiplies by silentScale internally** → Double-fade smell; lint flags.
- **Audio backend bug holds silentScale = 1.0 during actual silence** → Every effect glares regardless of behaviour tag; runtime sampler catches at the *backend* contract, not the effect contract.

## Sketch

```cpp
enum class SilenceBehaviour : uint8_t {
  FrameworkFade,           // default — framework multiplies by controlBus.silentScale
  InternalFade,            // effect owns its own (non-linear) curve
  IntentionallyPersistent, // status / OOBE / idle pulse — must stay visible
};

// In EffectDescriptor:
constexpr SilenceBehaviour silence{SilenceBehaviour::FrameworkFade};

// In RendererActor, between effect->render(ctx) and FastLED.show():
if (descriptor.silence == SilenceBehaviour::FrameworkFade) {
  const float s = ctx.controlBus.silentScale;
  for (int i = 0; i < kLedCount; ++i) {
    ctx.leds[i].nscale8(uint8_t(s * 255.0f));
  }
}
```
