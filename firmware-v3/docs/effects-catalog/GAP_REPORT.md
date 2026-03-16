# Effects Documentation Pipeline -- Gap Report

**Version:** 1.0.0
**Generated:** 2026-02-21 08:54:38

---

## Summary

| Severity | Count |
|----------|-------|
| CRITICAL | 1 |
| WARNING | 7 |
| INFO | 20 |
| **Total** | **28** |

**Sources:**

- cross-reference-validation: 3 gap(s)
- inventory: 3 gap(s)
- manifest: 6 gap(s)
- math-appendix: 8 gap(s)
- taxonomy: 3 gap(s)

## Contradictions Between Outputs

The following inconsistencies were found during cross-reference validation of the inventory, taxonomy, and math appendix outputs.

### Audio Reactive Mismatch

- **[WARNING]** Effect 9 (Heartbeat) is audio_reactive in inventory but has no audio coupling mode in taxonomy.
- **[WARNING]** Effect 112 (Beat Pulse (Shockwave In)) is audio_reactive in inventory but has no audio coupling mode in taxonomy.

### Removed Effect Not In Taxonomy

- **[INFO]** Removed effect 112 (Beat Pulse (Shockwave In)) not classified in any rendering pattern. Expected for removed slots.


## Missing Items

### Effects Not Found

No missing effects detected.

### Functions Not Found

No missing functions detected.

### Files Not Found

- **[INFO]** `audio/AudioMath.h` -- File 'audio/AudioMath.h' referenced in analysis but not in manifest file_index.
- **[INFO]** `effects/CoreEffects.h` -- File 'effects/CoreEffects.h' referenced in analysis but not in manifest file_index.
- **[INFO]** `effects/enhancement/SmoothingEngine.h` -- File 'effects/enhancement/SmoothingEngine.h' referenced in analysis but not in manifest file_index.
- **[INFO]** `effects/utils/FastLEDOptim.h` -- File 'effects/utils/FastLEDOptim.h' referenced in analysis but not in manifest file_index.
- **[INFO]** `plugins/api/EffectContext.h` -- File 'plugins/api/EffectContext.h' referenced in analysis but not in manifest file_index.

## Unclassified Effects

All effects are classified in at least one rendering pattern.

## Upstream Agent Gaps

These gaps were reported by the upstream analysis agents.

### From manifest

- **[INFO]** Header AudioReactivePolicy.h has no class definition (may be utility/types header)
- **[INFO]** Header BeatPulseCore.h has no class definition (may be utility/types header)
- **[INFO]** Header BeatPulseRenderUtils.h has no class definition (may be utility/types header)
- **[INFO]** Header ChromaUtils.h has no class definition (may be utility/types header)
- **[INFO]** Header LGPFilmPost.h has no class definition (may be utility/types header)
- **[INFO]** Header PerlinNoiseTypes.h has no class definition (may be utility/types header)

### From inventory

- **[WARNING]** Beat Pulse (Shockwave In) removed. Metadata position preserved but no class registered. Code comment says slot left empty for ID stability, but the code does not actually skip the total counter, causing a -1 offset in registered IDs for effects 112+ vs their PATTERN_METADATA positions.
- **[INFO]** CHROMA_ARGMAX_FIX_SPEC Section 5.3 lists Chevron Waves as ID 18, but PATTERN_METADATA position 18 is LGP Diamond Lattice. Chevron Waves is at metadata position 22.
  - Note: Spec document has incorrect ID for Chevron Waves. Actual ID from PatternRegistry is 22.
- **[CRITICAL]** PATTERN_METADATA position 98 = 'Audio Bloom Parity' but CoreEffects registers SnapwaveLinearEffect at code total 98. Position 99 = 'Audio Waveform Parity' but CoreEffects registers TrinityTestEffect at code total 99.
  - Note: Display names in PatternRegistry at positions 98-99 do not match the actual class implementations registered at those code positions. SnapwaveLinearEffect (linear, not centre-origin) is registered where 'Audio Bloom Parity' metadata sits.

### From math-appendix

- **[LOW]** LGPFilmPost.cpp static functions (hash32, buildGammaLUT) are file-static and not fully documented as they are internal implementation details of the cinema post chain.
- **[WARNING]** MusicalSaliency.h and StyleDetector.h (included by ControlBus.h) were not read. MusicalSaliencyFrame and MusicStyle types are documented via ControlBusFrame fields only.
- **[WARNING]** AudioTime.h and MusicalGrid.h were not read. MusicalGridSnapshot and AudioTime types are documented via AudioContext accessors only.
- **[LOW]** BehaviorSelection.h was not read. BehaviorContext and VisualBehavior types are documented via AudioContext accessor signatures only.
- **[LOW]** used_by_effects_count values for audio_features are estimates based on grep patterns rather than exact per-field counts.
- **[LOW]** The manifest's full symbol_index and class_index were not readable due to file size. Function discovery was done via grep of source files.
- **[WARNING]** Perlin backend effects (LGPPerlinBackendEmotiscopeFullEffect, LGPPerlinBackendEmotiscopeQuarterEffect, LGPPerlinBackendFastLEDEffect) contain inline Perlin noise implementations that were not fully parsed.
- **[WARNING]** LGPExperimentalAudioPack.cpp (41KB) and LGPHolyShitBangersPack.cpp (20KB) contain multiple inline helper functions that were not individually catalogued due to being pack-internal.

### From taxonomy

- **[INFO]** Audio coupling mode could not be determined from code markers
- **[INFO]** Effect slot removed from registry, no source file
- **[INFO]** Effect is marked audio_reactive but uses internal timer-based lub-dub pattern, not ControlBus audio data. Audio coupling is self-generated timing.

## Unresolved Cross-References

No unresolved cross-references detected.

## Recommendations

### [CRITICAL] Investigate: PATTERN_METADATA position 98 = 'Audio Bloom Parity' but CoreEffects registers SnapwaveLinearEffect at code total 98. Position 99 = 'Audio Waveform Parity' but CoreEffects registers TrinityTestEffect at code total 99.

Reported by inventory agent.

### [WARNING] Review 2 audio-reactive classification mismatches between inventory and taxonomy.

May indicate incomplete audio analysis or incorrect audio_reactive flags.

### [INFO] Re-run the pipeline after addressing CRITICAL gaps to verify consistency.

Ensures all corrections propagate through all documents.
