# Effects

> **When to read this:** Before creating or modifying visual effects, working with zones, or adding audio-reactive features.

## Adding a New Effect

1. Create an `IEffect` subclass in `firmware/v2/src/effects/ieffect/`
2. Register in `registerAllEffects()` in `firmware/v2/src/effects/CoreEffects.cpp`
3. Update `EXPECTED_EFFECT_COUNT` in CoreEffects.cpp
4. Bump `MAX_EFFECTS` if needed in: RendererActor.h, SystemState.h, ZoneConfigManager.h, AudioEffectMapping.h, RequestValidator.h
5. Test with serial menu (`e` command) or web UI

### Registration Pattern

```cpp
// In firmware/v2/src/effects/CoreEffects.cpp -- registerAllEffects()
static ieffect::MyNewEffect myNewEffectInstance;
if (renderer->registerEffect(total, &myNewEffectInstance)) {
    total++;
}
```

Each effect receives an `EffectContext` with access to LED buffers, timing, palette, audio data, and parameters. See existing effects in `firmware/v2/src/effects/ieffect/` for examples.

## Centre Origin Rule (MANDATORY)

All effects MUST originate from LED 79/80 (centre point).

**Valid patterns:**
- Outward: centre (79/80) toward edges (0 and 159)
- Inward: edges (0/159) toward centre (79/80)

**Invalid patterns:**
- Linear left-to-right (0 -> 159)
- Linear right-to-left (159 -> 0)

```cpp
// CORRECT: Centre-out iteration
for (int i = 0; i < 80; i++) {
    leds[79 - i] = color;  // Left from centre
    leds[80 + i] = color;  // Right from centre
}

// WRONG: Linear sweep -- will be rejected
for (int i = 0; i < 160; i++) {
    leds[i] = color;
}
```

**Why:** The Light Guide Plate creates interference patterns. Edge-originating effects look wrong on this hardware.

## Zone Effects

The Zone Composer (`firmware/v2/src/effects/zones/`) provides up to 4 independent zones (40 LEDs each, symmetric around centre).

- Effect must work with buffer proxy (renders to temp buffer, then composites)
- Do not assume LED indices -- use provided buffer size
- NVS persistence for zone configurations
- 5 built-in presets: Single, Dual, Triple, Quad, Alternating

## Audio-Reactive Effects

**MANDATORY:** Before adding ANY audio-reactive features, read:
[docs/audio-visual/audio-visual-semantic-mapping.md](../audio-visual/audio-visual-semantic-mapping.md)

Key rules:
- No rigid frequency-to-visual bindings ("bass -> expansion" is a trap)
- Respond to musical saliency, not raw spectrum
- Style-adaptive response (EDM/vocal/ambient need different strategies)
- Use temporal context (history), not just instantaneous values

### Musical Intelligence Feature Flags

| Flag | CPU Cost | Purpose |
|------|----------|---------|
| `FEATURE_MUSICAL_SALIENCY` | ~80 us/hop | Harmonic/rhythmic/timbral/dynamic novelty |
| `FEATURE_STYLE_DETECTION` | ~60 us/hop | Classifies music style |

Currently used by 3 of 101+ effects. Disable in `firmware/v2/src/config/features.h` if unused.

## Common Mistakes

- Caching palette pointers across frames (palettes can change)
- Integer rounding at centre boundary (79 vs 80)
- Assuming global `ledCount` inside zone buffers
- Rainbow colour cycling (explicitly forbidden)

## Deeper References

- [docs/effects/ENHANCED_EFFECTS_GUIDE.md](../effects/ENHANCED_EFFECTS_GUIDE.md) -- Detailed effect development guide
- [docs/audio-visual/audio-visual-semantic-mapping.md](../audio-visual/audio-visual-semantic-mapping.md) -- Full audio-reactive protocol
