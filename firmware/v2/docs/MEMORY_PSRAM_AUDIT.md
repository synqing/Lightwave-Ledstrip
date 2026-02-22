# PSRAM Migration Audit — Effect Buffers Still in DRAM

> **Policy** (MEMORY_ALLOCATION.md §3.5): All effect instance buffers **>64 bytes** MUST be allocated from PSRAM. Internal DRAM use for effect data is **FORBIDDEN** unless operationally required (DMA, ISR).

Firmware uploaded. This document lists effects that still hold **>64 bytes** of buffer data in DRAM and should be migrated to PSRAM (alloc in `init()`, free in `cleanup()`, access via `m_ps->` in `render()`).

---

## Already in PSRAM (no action)

- LGPReactionDiffusionEffect, LGPReactionDiffusionTriangleEffect, LGPReactionDiffusionTestRigEffect  
- LGPChimeraCrownEffect, LGPCatastropheCausticsEffect, LGPLorenzRibbonEffect, LGPIFSBioRelicEffect (Holy Shit Bangers pack)  
- BloomParityEffect, KuramotoTransportEffect, KuramotoOscillatorField, KuramotoTransportBuffer  
- BeatPulseTransportCore, WaveformParityEffect  
- LGPLangtonHighwayEffect (`m_grid` in PSRAM)
- **LGPSpectrumDetailEnhancedEffect** (SpectrumDetailEnhancedPsram)
- **LGPFluidDynamicsEffect** (FluidPsram)
- **LGPMycelialNetworkEffect** (MycelialPsram)
- **LGPPerlinShocklinesAmbientEffect** (ShocklinesPsram)
- **AudioBloomEffect** (BloomPsram)
- **RippleEffect**, **RippleEnhancedEffect**, **RippleEsTunedEffect** (RipplePsram)
- **EsWaveformRefEffect** (WaveformRefPsram), **SbWaveform310RefEffect** (SbWaveform310Psram)
- **LGPPerlinBackendEmotiscopeFullEffect** (PerlinBackendPsram)
- **LGPSpectrumDetailEffect** (SpectrumDetailPsram)
- **LGPRule30CathedralEffect** (Rule30Psram, in LGPShapeBangersPack)
- **FireEffect** (FirePsram)
- **SnapwaveLinearEffect** (SnapwavePsram)

---

## To Migrate (by estimated DRAM saved)

*All 15 effects from the original audit have been migrated. No remaining items.*

---

## Exceptions (≤64 bytes or non-effect)

- Small per-effect state (e.g. `m_chromaSmoothed[12]`, `m_ringRadius[8]`, `m_tipPositions[16]` when total effect state ≤64 B) can remain in DRAM per policy.
- **DMA buffers** (e.g. LED RMT/FastLED) must stay in internal SRAM.
- **TransitionEngine** `m_ringPhases[5]` = 20 B — OK in DRAM.
- **Network/WebServer** buffers are constrained by lwIP/AsyncTCP and cannot be moved to PSRAM.

---

## Pattern to Apply

For each effect in the table:

1. Add a `struct PsramData { ... };` (or named `XxxPsram`) in the header with the large arrays.
2. Replace member arrays with `PsramData* m_ps = nullptr;`.
3. In `init()`: `m_ps = static_cast<PsramData*>(heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));` then `memset(m_ps, 0, sizeof(PsramData));` (and init fields as needed). Return `false` if alloc fails.
4. In `cleanup()`: `if (m_ps) { heap_caps_free(m_ps); m_ps = nullptr; }`.
5. In `render()`: guard with `if (!m_ps) return;` and use `m_ps->field[i]` everywhere.
6. Wrap alloc/free and guard in `#ifndef NATIVE_BUILD` if the effect is built for native tests.

See **LGPReactionDiffusionEffect** or **LGPHolyShitBangersPack** for reference.

---

*Generated after firmware upload and codebase audit. Update this file as migrations are completed.*
