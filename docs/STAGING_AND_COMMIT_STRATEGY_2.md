# Staging and Commit Strategy (Second Wave) — feature/tab5-ios-effectid-alignment

This document provides a comprehensive staging and commit strategy for the **current uncommitted changes** (second wave of IEffect parameter API adoption). It mirrors the structure and detail level of `docs/STAGING_AND_COMMIT_STRATEGY.md`.

---

## 1. Change Analysis

### 1.1 Summary

| Category | File count | Description |
|----------|------------|-------------|
| **Refactors (effects)** | 199 | IEffect parameter API adoption: getParameterCount, getParameter(index/name), setParameter (bool), descriptor arrays; all previously unparameterised effects |
| **Config / manifest** | 1 | tunable_manifest.json: coverage_mode full, all families enforced, exposed_parameters populated |
| **Documentation** | 3 | docs/api/api-v1.md, api-v2.md; tools/k1-composer-dashboard/README.md |
| **Untracked (tooling)** | 1 | firmware/v2/tools/effect_tunables/bulk_expose_remaining.py |

**Totals (from `git diff --stat`)**: 202 modified files, ~21,557 insertions, ~979 deletions. One untracked file.

### 1.2 Categorisation by Type

#### Refactors — IEffect parameter API (second wave)

All changes follow the same pattern already applied in the first wave (see original strategy):

- **Header**: Add overrides `getParameterCount()`, `getParameter(uint8_t index)`, `setParameter(const char* name, float value)`, `getParameter(const char* name)`.
- **Implementation**: Static descriptor array (type, step, group, unit, advanced); getParameterCount returns array size; getParameter(index) returns descriptor; setParameter(name, value) returns bool and updates member; getParameter(name) returns current value.
- **No behavioural change** to render logic; only parameter surface for dashboard/tunable store.

**Effect groups in this wave:**

1. **Legacy / core effects (non-LGP)**  
   AudioBloom, AudioWaveform, BPM, BPMEnhanced, BeatPulse* (Bloom, Breathe, LGPInterference, Resonant, Ripple, ShockwaveCascade, Shockwave, Spectral, SpectralPulse, Stack, Void), BloomParity, Breathing, BreathingEnhanced, ChevronWaves, ChevronWavesEnhanced, ChromaticInterference, Confetti, Fire, Heartbeat, HeartbeatEsTuned, Interference, Juggle, Ocean, Plasma, Pulse, Ripple, RippleEnhanced, RippleEsTuned, Sinelon, SnapwaveLinear, TrinityTest, WaveAmbient, WaveReactive, WaveformParity.

2. **LGP single-effect implementations**  
   LGPAudioTest, LGPAuroraBorealis, LGPBassBreath, LGPBeatPulse, LGPBioluminescentWaves, LGPCausticShards, LGPChordGlow, LGPChromaticAberration, LGPChromaticLens, LGPChromaticPulse, LGPColorAccelerator, LGPColorTemperature, LGPComplementaryMixing, LGPCrystallineGrowth, LGPDNAHelix, LGPDopplerShift, LGPFluidDynamics, LGPFresnelCausticSweep, LGPGoldCodeSpeckle, LGPGratingScan, LGPGratingScanBreakup, LGPGravitationalWaveChirp, LGPHolographicAutoCycle, LGPHolographicEsTuned, LGPInterferenceScannerEffectEnhanced, LGPMoireSilk, LGPMycelialNetwork, LGPNeuralNetwork, LGPParallaxDepth, LGPPerceptualBlend, LGPPerlin* (BackendEmotiscopeFull/Quarter, BackendFastLED, Caustics, CausticsAmbient, InterferenceWeave, InterferenceWeaveAmbient, Shocklines, ShocklinesAmbient, Veil, VeilAmbient), LGPPhaseTransition, LGPPhotonicCrystalEffectEnhanced, LGPPlasmaMembrane, LGPQuantumColors, LGPQuantumEntanglement, LGPQuasicrystalLattice, LGPRGBPrism, LGPReactionDiffusion, LGPSchlierenFlow, LGPShapeBangersPack, LPSpectrumBars, LPSpectrumDetail, LPSpectrumDetailEnhanced, LGPStarBurst, LGPStarBurstEffectEnhanced, LGPStarBurstNarrative, LGPStressGlass, LGPStressGlassMelt, LGPWaterCaustics, LGPWaveCollisionEffectEnhanced.

3. **Pack files (multi-effect headers)**  
   LGPExperimentalAudioPack (new pack with parameter overrides per inner class), LGPHolyShitBangersPack (parameter API added; implementation changes for descriptors/setParameter).

4. **esv11 reference effects**  
   EsAnalogRefEffect, EsOctaveRefEffect, EsWaveformRefEffect (parameter API; naming distinct from earlier EsBloomRef/EsSpectrumRef).

#### Config / manifest

- **firmware/v2/docs/effects/tunable_manifest.json**
  - `coverage_mode`: `"phased"` → `"full"`.
  - `enforced_families`: extended to include CORE, GEOMETRIC, ORGANIC, COLOUR_MIXING, NOVEL_PHYSICS, CHROMATIC, AUDIO_REACTIVE, PERLIN_*, ENHANCED_AUDIO, DIAGNOSTIC, AUTO_CYCLE, ES_REFERENCE, ES_TUNED, SB_REFERENCE, BEAT_PULSE, TRANSPORT, HOLOGRAPHIC_VAR, REACTION_DIFFUSION, SHAPE_BANGERS, HOLY_SHIT_BANGERS, EXPERIMENTAL_AUDIO, SHOWPIECE_PACK3.
  - Per-effect `exposed_parameters` and `discovered_named_tunables` populated for all effects in this wave (aligns with generate_manifest.py --validate --enforce-all-families).

#### Documentation

- **docs/api/api-v1.md**, **docs/api/api-v2.md**: Updates reflecting full effect list, parameter surface, and any endpoint/response changes from parameter API.
- **tools/k1-composer-dashboard/README.md**: Minor updates (e.g. usage or catalog instructions).

#### Untracked tooling

- **firmware/v2/tools/effect_tunables/bulk_expose_remaining.py**: Script to bulk-expose remaining effects in the tunable manifest (optional to include in this branch).

### 1.3 File Dependencies and Affected Components

```
tunable_manifest.json
    → generate_manifest.py --validate --enforce-all-families (pre-commit / CI)
    → EffectTunableStore, HTTP/WS parameter encoding (no code change in this wave)

IEffect.h (unchanged in this wave)
    → All effect .cpp/.h in this wave implement the existing contract

CoreEffects.cpp
    → Not modified in this wave; effect registration already includes these effects

docs/api/*.md
    → Human/API consumer reference only
```

No new dependencies introduced; this wave is purely additive parameter surface and manifest/docs alignment.

---

## 2. Staging Strategy

Commits are ordered so that **manifest and tooling** come first, then **effects in logical groups**, then **docs**. No commit mixes unrelated concerns.

### Commit 1 — chore(manifest): full coverage and all families in tunable manifest

**Stage**: `firmware/v2/docs/effects/tunable_manifest.json`.

**Rationale**: Declares full coverage and enforced families; pre-commit/CI validation will pass after effect implementations are committed. Single file, no code dependency.

### Commit 2 — chore(tools): add bulk_expose_remaining script for tunable manifest

**Stage**: `firmware/v2/tools/effect_tunables/bulk_expose_remaining.py` (add as new file).

**Rationale**: Optional tool used to generate or maintain manifest entries; keeps tooling with effect_tunables. Skip this commit if you prefer not to track this script.

### Commit 3 — refactor(effects): IEffect parameter API — legacy and core effects

**Stage**: All modified files under `firmware/v2/src/effects/ieffect/` that are **not** under an `LGP*` prefix and **not** pack files or esv11_reference:

- AudioBloomEffect, AudioWaveformEffect, BPMEffect, BPMEnhancedEffect  
- BeatPulseBloomEffect, BeatPulseBreatheEffect, BeatPulseLGPInterferenceEffect, BeatPulseResonantEffect, BeatPulseRippleEffect, BeatPulseShockwaveCascadeEffect, BeatPulseShockwaveEffect, BeatPulseSpectralEffect, BeatPulseSpectralPulseEffect, BeatPulseStackEffect, BeatPulseVoidEffect  
- BloomParityEffect, BreathingEffect, BreathingEnhancedEffect  
- ChevronWavesEffect, ChevronWavesEffectEnhanced  
- ChromaticInterferenceEffect, ConfettiEffect, FireEffect  
- HeartbeatEffect, HeartbeatEsTunedEffect, InterferenceEffect, JuggleEffect  
- OceanEffect, PlasmaEffect, PulseEffect  
- RippleEffect, RippleEnhancedEffect, RippleEsTunedEffect  
- SinelonEffect, SnapwaveLinearEffect, TrinityTestEffect  
- WaveAmbientEffect, WaveReactiveEffect, WaveformParityEffect  

**Rationale**: One coherent group (non-LGP, non-pack, non-reference); easier review and revert by category.

### Commit 4 — refactor(effects): IEffect parameter API — LGP single-effect implementations

**Stage**: All modified `firmware/v2/src/effects/ieffect/LGP*.cpp` and `LGP*.h` **except**:

- LGPExperimentalAudioPack.cpp / .h  
- LGPHolyShitBangersPack.cpp / .h  
- LGPShapeBangersPack.cpp / .h  

(I.e. all LGP single-effect files and LGP* that are not multi-effect packs.)

**Rationale**: Large but single-purpose refactor; keeps pack files separate because they contain multiple effect classes per file.

### Commit 5 — refactor(effects): IEffect parameter API — pack files and esv11 reference

**Stage**:

- `firmware/v2/src/effects/ieffect/LGPExperimentalAudioPack.cpp`, `.h`  
- `firmware/v2/src/effects/ieffect/LGPHolyShitBangersPack.cpp`, `.h`  
- `firmware/v2/src/effects/ieffect/LGPShapeBangersPack.cpp`, `.h`  
- `firmware/v2/src/effects/ieffect/esv11_reference/EsAnalogRefEffect.cpp`, `.h`  
- `firmware/v2/src/effects/ieffect/esv11_reference/EsOctaveRefEffect.cpp`, `.h`  
- `firmware/v2/src/effects/ieffect/esv11_reference/EsWaveformRefEffect.cpp`, `.h`  

**Rationale**: Pack files have different structure (multiple classes, larger diffs); esv11 reference is a distinct family. One commit for “packs + reference” keeps the rest of the LGP refactor clean.

### Commit 6 — docs(api): API v1/v2 and K1 README updates

**Stage**: `docs/api/api-v1.md`, `docs/api/api-v2.md`, `tools/k1-composer-dashboard/README.md`.

**Rationale**: Documentation only; reflects full effect list and parameter surface. K1 README stays with API docs in a single docs commit for simplicity.

---

## 3. Commit Message Format

Each commit MUST follow conventional commits and include a body and footer where applicable.

### Template

```
<type>(<scope>): <subject (≤50 chars)

[optional body]
- Implementation approach
- Technical considerations

Refs: feature/tab5-ios-effectid-alignment
```

### Full commit message examples

**Commit 1 — chore(manifest)**

```
chore(manifest): tunable manifest full coverage and all families enforced

- coverage_mode: phased → full.
- enforced_families: extended to all effect families (CORE, GEOMETRIC, ORGANIC, COLOUR_MIXING, NOVEL_PHYSICS, CHROMATIC, AUDIO_REACTIVE, PERLIN_*, ENHANCED_AUDIO, DIAGNOSTIC, AUTO_CYCLE, ES_REFERENCE, ES_TUNED, SB_REFERENCE, BEAT_PULSE, TRANSPORT, HOLOGRAPHIC_VAR, REACTION_DIFFUSION, SHAPE_BANGERS, HOLY_SHIT_BANGERS, EXPERIMENTAL_AUDIO, SHOWPIECE_PACK3).
- Per-effect exposed_parameters and discovered_named_tunables populated for second-wave effects.

Refs: feature/tab5-ios-effectid-alignment
```

**Commit 2 — chore(tools)**

```
chore(tools): add bulk_expose_remaining.py for tunable manifest

Script to bulk-expose remaining effects in tunable_manifest.json.
Refs: feature/tab5-ios-effectid-alignment
```

**Commit 3 — refactor(effects) legacy**

```
refactor(effects): IEffect parameter API for legacy and core effects

Legacy and non-LGP effects: getParameterCount, getParameter(index/name), setParameter(bool), descriptor arrays. Effects: AudioBloom, AudioWaveform, BPM, BPMEnhanced, BeatPulse*, BloomParity, Breathing*, ChevronWaves*, ChromaticInterference, Confetti, Fire, Heartbeat*, Interference, Juggle, Ocean, Plasma, Pulse, Ripple*, Sinelon, SnapwaveLinear, TrinityTest, Wave*, WaveformParity.

Refs: feature/tab5-ios-effectid-alignment
```

**Commit 4 — refactor(effects) LGP**

```
refactor(effects): IEffect parameter API for LGP single-effect implementations

LGP* single-effect files (excl. pack files): parameter descriptors and setParameter/getParameter. Aligns with tunable manifest and dashboard parameter surface.

Refs: feature/tab5-ios-effectid-alignment
```

**Commit 5 — refactor(effects) packs and reference**

```
refactor(effects): IEffect parameter API for pack files and esv11 reference

LGPExperimentalAudioPack, LGPHolyShitBangersPack, LGPShapeBangersPack; EsAnalogRef, EsOctaveRef, EsWaveformRef. Parameter overrides and descriptor arrays per effect class.

Refs: feature/tab5-ios-effectid-alignment
```

**Commit 6 — docs**

```
docs(api): API v1/v2 and K1 composer README updates

Effect list and parameter surface; K1 dashboard README.
Refs: feature/tab5-ios-effectid-alignment
```

---

## 4. Context Documentation

### 4.1 Second-wave parameter API adoption

- **Problem**: First wave (see STAGING_AND_COMMIT_STRATEGY.md) parameterised a subset of effects (e.g. LGP AnisotropicCloak, BoxWave, …; ModalResonance; EsBloomRef, EsSpectrumRef; SbWaveform310Ref). Remaining effects had no getParameterCount/getParameter/setParameter/getParameter(name), so dashboard and EffectTunableStore could not list or persist their parameters.
- **Solution**: Add the same IEffect parameter overrides and static descriptor arrays to all remaining effects: legacy (Audio*, BPM, BeatPulse*, …), LGP single-effect, pack files (ExperimentalAudio, HolyShitBangers, ShapeBangers), and esv11 reference (Analog, Octave, Waveform).
- **Alternatives**: Leave some effects without parameters (rejected: inconsistent UX); codegen from manifest (future improvement).
- **Challenges**: Large file count (199); ensuring no render-path allocation; keeping descriptor names within kEffectParameterNameMaxLen.
- **Performance**: No change to render hot path; parameter methods are call-on-demand.
- **Testing**: Pre-commit tunable manifest validation; manual effect.list / effect.parameters.get; dashboard parameter sliders.
- **Limitations**: Same as first wave: name length 40; no i18n for displayName yet.

### 4.2 Tunable manifest full coverage

- **Problem**: Manifest was phased (subset of families enforced); not all effects had exposed_parameters populated.
- **Solution**: coverage_mode full; all families in enforced_families; every effect in this wave has exposed_parameters and discovered_named_tunables filled so generate_manifest.py --validate --enforce-all-families passes.
- **Challenges**: Keeping manifest in sync with code when adding parameters; bulk script (bulk_expose_remaining.py) helps.
- **Limitations**: Manifest is hand/maintained or script-generated; no automatic extraction from source yet.

---

## 5. Lessons Learned

- **Batching effects**: Splitting by “legacy / LGP single / packs+reference” keeps commits reviewable and makes revert by category easy.
- **Manifest first**: Committing tunable_manifest.json (full coverage + families) before the large effect refactor allows CI to validate incrementally once effects are committed.
- **Pack files**: Multi-effect headers (ExperimentalAudio, HolyShitBangers, ShapeBangers) have multiple classes per file; separate commit avoids mixing with single-effect LGP diffs.
- **esv11 reference naming**: EsAnalogRef, EsOctaveRef, EsWaveformRef are distinct from EsBloomRef/EsSpectrumRef; document in commit message to avoid confusion.

---

## 6. Future Contributor Guidance

### Maintenance

- **Adding parameters to an effect**: Extend the static descriptor array; implement setParameter/getParameter; run generate_manifest.py and update tunable_manifest.json if needed; keep name length ≤ kEffectParameterNameMaxLen.
- **New effect**: Implement full IEffect including getParameterCount/getParameter/setParameter/getParameter(name); add to tunable manifest and enforced_families if new family; run --validate --enforce-all-families.

### Gotchas

- **Do not** add heap allocation in getParameter/setParameter or in descriptor construction; use static const arrays.
- **Do not** change parameter names after release without a migration story for EffectTunableStore (NVS keys are name-based).
- **Do not** commit tunable_manifest.json with empty exposed_parameters for an effect that now has parameters; CI will fail.

### References

- First-wave strategy and commit order: `docs/STAGING_AND_COMMIT_STRATEGY.md`
- IEffect API: `firmware/v2/src/plugins/api/IEffect.h`
- Tunable manifest and tooling: `firmware/v2/docs/effects/tunable_manifest.json`, `firmware/v2/tools/effect_tunables/generate_manifest.py`
- Centre origin and constraints: `.cursor/rules/000-core-constraints.mdc`, `CLAUDE.md`, `AGENTS.md`

---

## 7. Execution Checklist

Use the following to stage and commit in order. Adjust paths if the working tree has changed.

```bash
# Commit 1 — tunable manifest
git add firmware/v2/docs/effects/tunable_manifest.json
git commit -m "chore(manifest): tunable manifest full coverage and all families enforced

- coverage_mode: phased → full.
- enforced_families: extended to all effect families.
- Per-effect exposed_parameters and discovered_named_tunables populated for second-wave effects.

Refs: feature/tab5-ios-effectid-alignment"

# Commit 2 — bulk script (optional; omit if not tracking)
git add firmware/v2/tools/effect_tunables/bulk_expose_remaining.py
git commit -m "chore(tools): add bulk_expose_remaining.py for tunable manifest

Refs: feature/tab5-ios-effectid-alignment"

# Commit 3 — legacy and core effects (non-LGP, non-pack, non-esv11_reference)
git add firmware/v2/src/effects/ieffect/AudioBloomEffect.cpp firmware/v2/src/effects/ieffect/AudioBloomEffect.h
git add firmware/v2/src/effects/ieffect/AudioWaveformEffect.cpp firmware/v2/src/effects/ieffect/AudioWaveformEffect.h
git add firmware/v2/src/effects/ieffect/BPMEffect.cpp firmware/v2/src/effects/ieffect/BPMEffect.h
git add firmware/v2/src/effects/ieffect/BPMEnhancedEffect.cpp firmware/v2/src/effects/ieffect/BPMEnhancedEffect.h
git add firmware/v2/src/effects/ieffect/BeatPulseBloomEffect.cpp firmware/v2/src/effects/ieffect/BeatPulseBreatheEffect.cpp
git add firmware/v2/src/effects/ieffect/BeatPulseLGPInterferenceEffect.cpp firmware/v2/src/effects/ieffect/BeatPulseResonantEffect.cpp
git add firmware/v2/src/effects/ieffect/BeatPulseRippleEffect.cpp firmware/v2/src/effects/ieffect/BeatPulseShockwaveCascadeEffect.cpp
git add firmware/v2/src/effects/ieffect/BeatPulseShockwaveEffect.cpp firmware/v2/src/effects/ieffect/BeatPulseSpectralEffect.cpp
git add firmware/v2/src/effects/ieffect/BeatPulseSpectralPulseEffect.cpp firmware/v2/src/effects/ieffect/BeatPulseStackEffect.cpp
git add firmware/v2/src/effects/ieffect/BeatPulseVoidEffect.cpp
git add firmware/v2/src/effects/ieffect/BloomParityEffect.cpp firmware/v2/src/effects/ieffect/BloomParityEffect.h
git add firmware/v2/src/effects/ieffect/BreathingEffect.cpp firmware/v2/src/effects/ieffect/BreathingEffect.h
git add firmware/v2/src/effects/ieffect/BreathingEnhancedEffect.cpp firmware/v2/src/effects/ieffect/BreathingEnhancedEffect.h
git add firmware/v2/src/effects/ieffect/ChevronWavesEffect.cpp firmware/v2/src/effects/ieffect/ChevronWavesEffectEnhanced.cpp firmware/v2/src/effects/ieffect/ChevronWavesEffectEnhanced.h
git add firmware/v2/src/effects/ieffect/ChromaticInterferenceEffect.cpp firmware/v2/src/effects/ieffect/ChromaticInterferenceEffect.h
git add firmware/v2/src/effects/ieffect/ConfettiEffect.cpp firmware/v2/src/effects/ieffect/ConfettiEffect.h
git add firmware/v2/src/effects/ieffect/FireEffect.cpp firmware/v2/src/effects/ieffect/FireEffect.h
git add firmware/v2/src/effects/ieffect/HeartbeatEffect.cpp firmware/v2/src/effects/ieffect/HeartbeatEffect.h
git add firmware/v2/src/effects/ieffect/HeartbeatEsTunedEffect.cpp firmware/v2/src/effects/ieffect/HeartbeatEsTunedEffect.h
git add firmware/v2/src/effects/ieffect/InterferenceEffect.cpp firmware/v2/src/effects/ieffect/InterferenceEffect.h
git add firmware/v2/src/effects/ieffect/JuggleEffect.cpp firmware/v2/src/effects/ieffect/JuggleEffect.h
git add firmware/v2/src/effects/ieffect/OceanEffect.cpp firmware/v2/src/effects/ieffect/OceanEffect.h
git add firmware/v2/src/effects/ieffect/PlasmaEffect.cpp firmware/v2/src/effects/ieffect/PlasmaEffect.h
git add firmware/v2/src/effects/ieffect/PulseEffect.cpp firmware/v2/src/effects/ieffect/PulseEffect.h
git add firmware/v2/src/effects/ieffect/RippleEffect.cpp firmware/v2/src/effects/ieffect/RippleEffect.h
git add firmware/v2/src/effects/ieffect/RippleEnhancedEffect.cpp firmware/v2/src/effects/ieffect/RippleEnhancedEffect.h
git add firmware/v2/src/effects/ieffect/RippleEsTunedEffect.cpp firmware/v2/src/effects/ieffect/RippleEsTunedEffect.h
git add firmware/v2/src/effects/ieffect/SinelonEffect.cpp firmware/v2/src/effects/ieffect/SinelonEffect.h
git add firmware/v2/src/effects/ieffect/SnapwaveLinearEffect.cpp firmware/v2/src/effects/ieffect/SnapwaveLinearEffect.h
git add firmware/v2/src/effects/ieffect/TrinityTestEffect.cpp firmware/v2/src/effects/ieffect/TrinityTestEffect.h
git add firmware/v2/src/effects/ieffect/WaveAmbientEffect.cpp firmware/v2/src/effects/ieffect/WaveAmbientEffect.h
git add firmware/v2/src/effects/ieffect/WaveReactiveEffect.cpp firmware/v2/src/effects/ieffect/WaveReactiveEffect.h
git add firmware/v2/src/effects/ieffect/WaveformParityEffect.cpp firmware/v2/src/effects/ieffect/WaveformParityEffect.h
git commit -m "refactor(effects): IEffect parameter API for legacy and core effects

Refs: feature/tab5-ios-effectid-alignment"

# Commit 4 — LGP single-effect (exclude LGPExperimentalAudioPack, LGPHolyShitBangersPack, LGPShapeBangersPack)
git add firmware/v2/src/effects/ieffect/LGPAudioTestEffect.cpp firmware/v2/src/effects/ieffect/LGPAudioTestEffect.h
git add firmware/v2/src/effects/ieffect/LGPAuroraBorealisEffect.cpp firmware/v2/src/effects/ieffect/LGPAuroraBorealisEffect.h
git add firmware/v2/src/effects/ieffect/LGPBassBreathEffect.cpp firmware/v2/src/effects/ieffect/LGPBassBreathEffect.h
git add firmware/v2/src/effects/ieffect/LGPBeatPulseEffect.cpp firmware/v2/src/effects/ieffect/LGPBeatPulseEffect.h
git add firmware/v2/src/effects/ieffect/LGPBioluminescentWavesEffect.cpp firmware/v2/src/effects/ieffect/LGPBioluminescentWavesEffect.h
git add firmware/v2/src/effects/ieffect/LGPCausticShardsEffect.cpp firmware/v2/src/effects/ieffect/LGPCausticShardsEffect.h
git add firmware/v2/src/effects/ieffect/LGPChordGlowEffect.cpp firmware/v2/src/effects/ieffect/LGPChordGlowEffect.h
git add firmware/v2/src/effects/ieffect/LGPChromaticAberrationEffect.cpp firmware/v2/src/effects/ieffect/LGPChromaticAberrationEffect.h
git add firmware/v2/src/effects/ieffect/LGPChromaticLensEffect.cpp firmware/v2/src/effects/ieffect/LGPChromaticLensEffect.h
git add firmware/v2/src/effects/ieffect/LGPChromaticPulseEffect.cpp firmware/v2/src/effects/ieffect/LGPChromaticPulseEffect.h
git add firmware/v2/src/effects/ieffect/LGPColorAcceleratorEffect.cpp firmware/v2/src/effects/ieffect/LGPColorAcceleratorEffect.h
git add firmware/v2/src/effects/ieffect/LGPColorTemperatureEffect.cpp firmware/v2/src/effects/ieffect/LGPColorTemperatureEffect.h
git add firmware/v2/src/effects/ieffect/LGPComplementaryMixingEffect.cpp firmware/v2/src/effects/ieffect/LGPComplementaryMixingEffect.h
git add firmware/v2/src/effects/ieffect/LGPCrystallineGrowthEffect.cpp firmware/v2/src/effects/ieffect/LGPCrystallineGrowthEffect.h
git add firmware/v2/src/effects/ieffect/LGPDNAHelixEffect.cpp firmware/v2/src/effects/ieffect/LGPDNAHelixEffect.h
git add firmware/v2/src/effects/ieffect/LGPDopplerShiftEffect.cpp firmware/v2/src/effects/ieffect/LGPDopplerShiftEffect.h
git add firmware/v2/src/effects/ieffect/LGPFluidDynamicsEffect.cpp firmware/v2/src/effects/ieffect/LGPFluidDynamicsEffect.h
git add firmware/v2/src/effects/ieffect/LGPFresnelCausticSweepEffect.cpp firmware/v2/src/effects/ieffect/LGPFresnelCausticSweepEffect.h
git add firmware/v2/src/effects/ieffect/LGPGoldCodeSpeckleEffect.cpp firmware/v2/src/effects/ieffect/LGPGoldCodeSpeckleEffect.h
git add firmware/v2/src/effects/ieffect/LGPGratingScanBreakupEffect.cpp firmware/v2/src/effects/ieffect/LGPGratingScanBreakupEffect.h
git add firmware/v2/src/effects/ieffect/LGPGratingScanEffect.cpp firmware/v2/src/effects/ieffect/LGPGratingScanEffect.h
git add firmware/v2/src/effects/ieffect/LGPGravitationalWaveChirpEffect.cpp firmware/v2/src/effects/ieffect/LGPGravitationalWaveChirpEffect.h
git add firmware/v2/src/effects/ieffect/LGPHolographicAutoCycleEffect.cpp firmware/v2/src/effects/ieffect/LGPHolographicAutoCycleEffect.h
git add firmware/v2/src/effects/ieffect/LGPHolographicEsTunedEffect.cpp firmware/v2/src/effects/ieffect/LGPHolographicEsTunedEffect.h
git add firmware/v2/src/effects/ieffect/LGPInterferenceScannerEffectEnhanced.cpp firmware/v2/src/effects/ieffect/LGPInterferenceScannerEffectEnhanced.h
git add firmware/v2/src/effects/ieffect/LGPMoireSilkEffect.cpp firmware/v2/src/effects/ieffect/LGPMoireSilkEffect.h
git add firmware/v2/src/effects/ieffect/LGPMycelialNetworkEffect.cpp firmware/v2/src/effects/ieffect/LGPMycelialNetworkEffect.h
git add firmware/v2/src/effects/ieffect/LGPNeuralNetworkEffect.cpp firmware/v2/src/effects/ieffect/LGPNeuralNetworkEffect.h
git add firmware/v2/src/effects/ieffect/LGPParallaxDepthEffect.cpp firmware/v2/src/effects/ieffect/LGPParallaxDepthEffect.h
git add firmware/v2/src/effects/ieffect/LGPPerceptualBlendEffect.cpp firmware/v2/src/effects/ieffect/LGPPerceptualBlendEffect.h
git add firmware/v2/src/effects/ieffect/LGPPerlinBackendEmotiscopeFullEffect.cpp firmware/v2/src/effects/ieffect/LGPPerlinBackendEmotiscopeFullEffect.h
git add firmware/v2/src/effects/ieffect/LGPPerlinBackendEmotiscopeQuarterEffect.cpp firmware/v2/src/effects/ieffect/LGPPerlinBackendEmotiscopeQuarterEffect.h
git add firmware/v2/src/effects/ieffect/LGPPerlinBackendFastLEDEffect.cpp firmware/v2/src/effects/ieffect/LGPPerlinBackendFastLEDEffect.h
git add firmware/v2/src/effects/ieffect/LGPPerlinCausticsAmbientEffect.cpp firmware/v2/src/effects/ieffect/LGPPerlinCausticsAmbientEffect.h
git add firmware/v2/src/effects/ieffect/LGPPerlinCausticsEffect.cpp firmware/v2/src/effects/ieffect/LGPPerlinCausticsEffect.h
git add firmware/v2/src/effects/ieffect/LGPPerlinInterferenceWeaveAmbientEffect.cpp firmware/v2/src/effects/ieffect/LGPPerlinInterferenceWeaveAmbientEffect.h
git add firmware/v2/src/effects/ieffect/LGPPerlinInterferenceWeaveEffect.cpp firmware/v2/src/effects/ieffect/LGPPerlinInterferenceWeaveEffect.h
git add firmware/v2/src/effects/ieffect/LGPPerlinShocklinesAmbientEffect.cpp firmware/v2/src/effects/ieffect/LGPPerlinShocklinesAmbientEffect.h
git add firmware/v2/src/effects/ieffect/LGPPerlinShocklinesEffect.cpp firmware/v2/src/effects/ieffect/LGPPerlinShocklinesEffect.h
git add firmware/v2/src/effects/ieffect/LGPPerlinVeilAmbientEffect.cpp firmware/v2/src/effects/ieffect/LGPPerlinVeilAmbientEffect.h
git add firmware/v2/src/effects/ieffect/LGPPerlinVeilEffect.cpp firmware/v2/src/effects/ieffect/LGPPerlinVeilEffect.h
git add firmware/v2/src/effects/ieffect/LGPPhaseTransitionEffect.cpp firmware/v2/src/effects/ieffect/LGPPhaseTransitionEffect.h
git add firmware/v2/src/effects/ieffect/LGPPhotonicCrystalEffectEnhanced.cpp firmware/v2/src/effects/ieffect/LGPPhotonicCrystalEffectEnhanced.h
git add firmware/v2/src/effects/ieffect/LGPPlasmaMembraneEffect.cpp firmware/v2/src/effects/ieffect/LGPPlasmaMembraneEffect.h
git add firmware/v2/src/effects/ieffect/LGPQuantumColorsEffect.cpp firmware/v2/src/effects/ieffect/LGPQuantumColorsEffect.h
git add firmware/v2/src/effects/ieffect/LGPQuantumEntanglementEffect.cpp firmware/v2/src/effects/ieffect/LGPQuantumEntanglementEffect.h
git add firmware/v2/src/effects/ieffect/LGPQuasicrystalLatticeEffect.cpp firmware/v2/src/effects/ieffect/LGPQuasicrystalLatticeEffect.h
git add firmware/v2/src/effects/ieffect/LGPRGBPrismEffect.cpp firmware/v2/src/effects/ieffect/LGPRGBPrismEffect.h
git add firmware/v2/src/effects/ieffect/LGPReactionDiffusionEffect.cpp firmware/v2/src/effects/ieffect/LGPReactionDiffusionEffect.h
git add firmware/v2/src/effects/ieffect/LGPSchlierenFlowEffect.cpp firmware/v2/src/effects/ieffect/LGPSchlierenFlowEffect.h
git add firmware/v2/src/effects/ieffect/LGPSpectrumBarsEffect.cpp firmware/v2/src/effects/ieffect/LGPSpectrumBarsEffect.h
git add firmware/v2/src/effects/ieffect/LGPSpectrumDetailEffect.cpp firmware/v2/src/effects/ieffect/LGPSpectrumDetailEffect.h
git add firmware/v2/src/effects/ieffect/LGPSpectrumDetailEnhancedEffect.cpp firmware/v2/src/effects/ieffect/LGPSpectrumDetailEnhancedEffect.h
git add firmware/v2/src/effects/ieffect/LGPStarBurstEffect.cpp firmware/v2/src/effects/ieffect/LGPStarBurstEffect.h
git add firmware/v2/src/effects/ieffect/LGPStarBurstEffectEnhanced.cpp firmware/v2/src/effects/ieffect/LGPStarBurstEffectEnhanced.h
git add firmware/v2/src/effects/ieffect/LGPStarBurstNarrativeEffect.cpp firmware/v2/src/effects/ieffect/LGPStarBurstNarrativeEffect.h
git add firmware/v2/src/effects/ieffect/LGPStressGlassEffect.cpp firmware/v2/src/effects/ieffect/LGPStressGlassEffect.h
git add firmware/v2/src/effects/ieffect/LGPStressGlassMeltEffect.cpp firmware/v2/src/effects/ieffect/LGPStressGlassMeltEffect.h
git add firmware/v2/src/effects/ieffect/LGPWaterCausticsEffect.cpp firmware/v2/src/effects/ieffect/LGPWaterCausticsEffect.h
git add firmware/v2/src/effects/ieffect/LGPWaveCollisionEffectEnhanced.cpp firmware/v2/src/effects/ieffect/LGPWaveCollisionEffectEnhanced.h
git commit -m "refactor(effects): IEffect parameter API for LGP single-effect implementations

Refs: feature/tab5-ios-effectid-alignment"

# Commit 5 — pack files and esv11 reference
git add firmware/v2/src/effects/ieffect/LGPExperimentalAudioPack.cpp firmware/v2/src/effects/ieffect/LGPExperimentalAudioPack.h
git add firmware/v2/src/effects/ieffect/LGPHolyShitBangersPack.cpp firmware/v2/src/effects/ieffect/LGPHolyShitBangersPack.h
git add firmware/v2/src/effects/ieffect/LGPShapeBangersPack.cpp firmware/v2/src/effects/ieffect/LGPShapeBangersPack.h
git add firmware/v2/src/effects/ieffect/esv11_reference/EsAnalogRefEffect.cpp firmware/v2/src/effects/ieffect/esv11_reference/EsAnalogRefEffect.h
git add firmware/v2/src/effects/ieffect/esv11_reference/EsOctaveRefEffect.cpp firmware/v2/src/effects/ieffect/esv11_reference/EsOctaveRefEffect.h
git add firmware/v2/src/effects/ieffect/esv11_reference/EsWaveformRefEffect.cpp firmware/v2/src/effects/ieffect/esv11_reference/EsWaveformRefEffect.h
git commit -m "refactor(effects): IEffect parameter API for pack files and esv11 reference

Refs: feature/tab5-ios-effectid-alignment"

# Commit 6 — docs
git add docs/api/api-v1.md docs/api/api-v2.md tools/k1-composer-dashboard/README.md
git commit -m "docs(api): API v1/v2 and K1 composer README updates

Refs: feature/tab5-ios-effectid-alignment"
```

**Note**: Commit 3 and 4 list files explicitly; if your shell supports it, you can use globs for Commit 4 while excluding the three pack file pairs (e.g. `git add firmware/v2/src/effects/ieffect/LGP*.cpp firmware/v2/src/effects/ieffect/LGP*.h` then `git reset firmware/v2/src/effects/ieffect/LGPExperimentalAudioPack.* firmware/v2/src/effects/ieffect/LGPHolyShitBangersPack.* firmware/v2/src/effects/ieffect/LGPShapeBangersPack.*`). Verify with `git status` after each commit.

---

*This strategy was generated from a full `git diff` and `git status` review of the second wave of uncommitted changes. Execute staging in the order above and adjust commit boundaries if the working tree has diverged.*
