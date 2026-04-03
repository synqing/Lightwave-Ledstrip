# STM 128-Band Spectral Upgrade — Claude Code CLI Prompt

**Status:** READY FOR EXECUTION
**Date:** 2026-04-02
**Supersedes:** `STM-SPECTRAL-SHOOTOUT-CODEX-PROMPT.md` (shootout cancelled — resource constraints that justified either/or do not exist)

---

## Context for the Agent

You are upgrading the spectral modulation path of the STMExtractor in the LightwaveOS firmware. The temporal path (Goertzel-based) is correct and untouched. The spectral path currently uses a 16-point FFT on 16 mel bands, which resolves only 1.1 cycles/octave — 18% of the 6 cycles/octave established as perceptually meaningful by Chang et al. (2025). You are replacing this with a 128-mel-band → 128-point FFT that resolves 9.1 cycles/octave.

This is NOT a rewrite. The existing STMExtractor architecture is sound. You are replacing one private method (`computeSpectralModulation`) and adding a second mel filterbank for the spectral path. Everything else — temporal Goertzel, ControlBus wiring, AudioActor integration, EdgeMixer STM_DUAL mode — stays as-is.

---

## MANDATORY Pre-Reading (in this order)

Read these files BEFORE writing any code. They contain hard constraints, existing interfaces, and architectural context that will prevent you from making mistakes.

1. `firmware-v3/CONSTRAINTS.md` — Timing and memory budgets (2ms effect ceiling, 320KB SRAM, no heap in render)
2. `firmware-v3/src/audio/pipeline/STMExtractor.h` — Current class interface (90 lines)
3. `firmware-v3/src/audio/pipeline/STMExtractor.cpp` — Current implementation (199 lines)
4. `firmware-v3/src/audio/contracts/ControlBus.h` — ControlBusRawInput and ControlBusFrame structs (STM fields at lines 87-93 and 177-183)
5. `firmware-v3/src/audio/contracts/ControlBus.cpp` — How STM fields are published and smoothed (search for "stmTemporal")
6. `firmware-v3/src/audio/AudioActor.cpp` — Where STMExtractor::process() is called (line ~1649, search for "stmReady")
7. `firmware-v3/src/audio/pipeline/FFT.h` — Custom FFT: `fft::rfft(float*, size_t)`, supports powers of 2 up to 512. Output format: `buf[0]=DC, buf[1]=Nyquist, buf[2k]=Re[k], buf[2k+1]=Im[k]`
8. `firmware-v3/src/effects/enhancement/EdgeMixer.h` — STM_DUAL mode (line 380-399): reads `stmTemporalEnergy` and `stmSpectralEnergy` scalars only
9. `firmware-v3/docs/specs/STM-DUAL-EDGE-SPEC.md` — Parent engineering spec (§3.4 is superseded — that's what you're fixing)
10. `firmware-v3/docs/specs/STM-SPECTRAL-SHOOTOUT-SPEC.md` — Candidate A description and implementation notes (§2 and §4)

---

## Deliverables

### Phase 1: Upgrade STMExtractor Spectral Path

**File: `firmware-v3/src/audio/pipeline/STMExtractor.h`**

Changes:
- Add `static constexpr uint8_t SPECTRAL_MEL_BANDS = 128;` — the spectral-path mel resolution
- Add `static constexpr uint8_t SPECTRAL_FFT_BINS = 42;` — bins 0–41 of the 128-point FFT (≤6 cycles/octave across 7 octaves)
- Change `SPECTRAL_BINS` from `7` to `42` (this is the output dimension)
- Add a separate `SpectralMelBand` struct for the 128-band filterbank — or reuse `MelBand` if the weight structure fits
- Add spectral mel filterbank storage. **CRITICAL:** The 128-band mel filterbank weights MUST be stored as `static const` in flash (PROGMEM), NOT as class members. They are precomputed constants. The class stores only the FFT buffer and magnitude output.
- Add: `float m_spectralFftBuffer[128]` and `float m_spectralMagnitudes[64]` as class members
- Add private method: `void initialiseSpectralMelBands()` (generates the flash-resident weights)
- Update the `static_assert` — the class will grow slightly but should stay well under 16 KB given 181 KB SRAM headroom. Set a reasonable new limit (e.g., 8192 bytes)
- Keep the existing `MelBand m_melBands[MEL_BANDS]` for the temporal path (16 bands, unchanged)

**File: `firmware-v3/src/audio/pipeline/STMExtractor.cpp`**

Changes:
- Add `initialiseSpectralMelBands()` — generates 128 triangular mel filter weights from 50 Hz to `selectMaxMelHz()` across `bins256[256]`. Same maths as the existing `initialiseMelBands()` but with 128 bands. Store results in flash-resident arrays.
- Add `applySpectralMelFilterbank(const float* bins256, float* melOut128)` — applies the 128-band filterbank to produce a 128-element mel frame
- Rewrite `computeSpectralModulation()`:
  1. Apply 128-band mel filterbank to bins256 → 128-element mel frame
  2. Run `fft::rfft(buffer, 128)` on the mel frame (the existing FFT.h supports 128-point)
  3. Extract magnitudes for bins 0–41 via `fft::magnitudes()`
  4. Peak-normalise to [0, 1]
  5. Output to `spectralOut[42]`
- Call `initialiseSpectralMelBands()` from the constructor
- The 128-band mel filterbank operates on the CURRENT frame only (no history buffer needed for spectral path). This is a single-frame spatial analysis, not a temporal analysis. That is why memory cost is low.

**Approach for flash-resident weights:**

Since `constexpr` generation of triangular mel weights is complex, use a simpler pattern: declare `static` arrays inside the .cpp file at namespace scope, populated once by `initialiseSpectralMelBands()` at construction time. These won't be `const` in the strictest sense, but they'll be initialised once and never modified — functionally equivalent. The alternative (constexpr tables) is more correct but significantly harder to implement. Either approach is acceptable.

```cpp
// At namespace scope in STMExtractor.cpp
namespace {
    // Populated once by initialiseSpectralMelBands(), never modified after
    static constexpr uint8_t kSpectralMelBands = 128;
    static constexpr uint8_t kMaxSpectralMelWeights = 8; // 128 bands = narrower filters = fewer weights each

    struct SpectralMelBand {
        uint16_t startBin = 0;
        uint8_t weightCount = 0;
        float weights[kMaxSpectralMelWeights] = {0.0f};
    };

    static SpectralMelBand s_spectralMelBands[kSpectralMelBands];
    static bool s_spectralMelInitialised = false;
}
```

This keeps the 128 × (2 + 1 + 8×4) = ~4.5 KB in BSS (static storage), not on the class instance. If you can make it `constexpr`, even better — it moves to flash.

### Phase 2: Update ControlBus Fields

**File: `firmware-v3/src/audio/contracts/ControlBus.h`**

Changes to `ControlBusRawInput`:
- Change `STM_SPECTRAL_BINS` from `7` to `42`
- `stmSpectral[STM_SPECTRAL_BINS]` automatically resizes (+140 bytes = 35 extra floats)
- The `static_assert(sizeof(ControlBusRawInput) <= 4096)` WILL likely fail. Increase to `5120`. We have 181 KB SRAM headroom — 5 KB is nothing.

Changes to `ControlBusFrame`:
- `STM_SPECTRAL_BINS` mirrors from `ControlBusRawInput` (already does via `= ControlBusRawInput::STM_SPECTRAL_BINS`) — no change needed
- `stmSpectral[STM_SPECTRAL_BINS]` automatically resizes (+140 bytes)
- The smoothing buffer `m_stm_spectral_s[STM_SPECTRAL_BINS]` also grows (+140 bytes)
- The `static_assert(sizeof(ControlBusFrame) <= 4096)` WILL likely fail. Increase to `5120`.
- **All downstream code uses the `STM_SPECTRAL_BINS` constant for loop bounds and memcpy sizes.** No hardcoded `7` exists anywhere in the codebase. Confirmed by grep.

**File: `firmware-v3/src/audio/contracts/ControlBus.cpp`**

- The existing `std::memcpy(m_frame.stmSpectral, raw.stmSpectral, sizeof(m_frame.stmSpectral))` will automatically copy the right size because `sizeof` tracks the array.
- Verify `applyStmSmoothing()` — it likely iterates over `STM_SPECTRAL_BINS`. If it does, it will automatically handle 42 bins. If it uses a hardcoded loop bound, fix it to use the constant.

### Phase 3: Verify No Downstream Breakage

**EdgeMixer (DO NOT MODIFY):**
- `processStmDual()` reads `stmTemporalEnergy` and `stmSpectralEnergy` — both are scalars computed from the vector. As long as the scalar energy computation in AudioActor/ControlBus still works (mean of the vector), EdgeMixer is unaffected.
- Confirm: the scalar energy computation in AudioActor.cpp (`stmSpectralEnergy = mean(stmSpectral)`) iterates using the constant or `sizeof`. If hardcoded to 7, fix it to use `STM_SPECTRAL_BINS`.

**AudioActor (MINIMAL CHANGE):**
- `process()` call: `raw.stmReady = m_stmExtractor.process(raw.bins256, raw.stmTemporal, raw.stmSpectral);` — the output arrays in ControlBusRawInput have been resized, so this call is correct as-is.
- Scalar energy computation: search for where `stmSpectralEnergy` is computed. Fix any hardcoded `7` to use `ControlBusRawInput::STM_SPECTRAL_BINS`.

**WsEdgeMixerCommands.cpp (DO NOT MODIFY):**
- Only deals with mode switching (`"stm_dual"` string parsing). Unaffected by spectral bin count.

### Phase 4: Build and Verify

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz 2>&1
```

**Pass criteria:**
- Zero errors
- Zero warnings (`-Wall -Werror` is enforced)
- `static_assert` constraints pass
- Flash usage increase < 10 KB (mel filterbank weights)
- SRAM usage increase < 1 KB (FFT buffers + magnitude array)

### Phase 5: Add Benchmark Instrumentation (Optional but Recommended)

Add timing instrumentation to `STMExtractor::process()` so we can measure actual on-device performance:

```cpp
#if defined(ESP_PLATFORM)
#include <esp_timer.h>
#endif

// In process(), wrap computeSpectralModulation():
#if defined(ESP_PLATFORM)
    const int64_t t0 = esp_timer_get_time();
#endif
    computeSpectralModulation(m_melFrame, spectralOut);
#if defined(ESP_PLATFORM)
    const int64_t t1 = esp_timer_get_time();
    // Log every 1000th call to avoid serial flood
    static uint32_t callCount = 0;
    if (++callCount % 1000 == 0) {
        ESP_LOGI("STM", "spectral128: %lld us", (long long)(t1 - t0));
    }
#endif
```

This lets us verify the ~50 µs estimate on real hardware. If p99 exceeds 200 µs, something is wrong — escalate.

---

## Hard Constraints (NON-NEGOTIABLE)

1. **No heap allocation.** No `new`, `malloc`, `String`, or any STL container that heap-allocates. All buffers are static or stack.
2. **British English** in all comments, docs, logs, and UI strings: centre, colour, initialise, serialise, behaviour, normalise.
3. **Zero warnings.** The build uses `-Wall -Werror`. Zero means zero.
4. **Do not touch the temporal path.** `updateTemporalModulation()`, the 16-band mel filterbank for temporal, and the Goertzel accumulator are correct and unchanged.
5. **Do not touch EdgeMixer.** The STM_DUAL mode reads scalar energy values. It is unaffected by spectral bin count.
6. **Do not touch WsEdgeMixerCommands.** The WebSocket command wiring is unaffected.
7. **128-band mel filterbank weights go in static storage, not on the class instance.** Flash/BSS, not stack or heap.
8. **Use the existing `fft::rfft()` from FFT.h.** It already supports 128-point. Do NOT add esp-dsp as a dependency — it is not currently in the project.
9. **Centre origin rule:** This change does not affect LED rendering, but be aware — all K1 visualisations inject light at the CENTRE (LED 79/80) and propagate symmetrically outward. Do not introduce any linear sweep behaviour.
10. **The `process()` signature stays the same.** Input: `bins256[256]`. Outputs: `temporalOut[MEL_BANDS]`, `spectralOut[SPECTRAL_BINS]`. The SPECTRAL_BINS constant changes from 7 to 42, but callers use the constant.

---

## Escalation Triggers

Stop and report (do not attempt to fix) if:

1. The 128-point FFT via `fft::rfft()` produces incorrect output (verify with a known input: 128-sample cosine at bin 10 → magnitude peak at bin 10)
2. `sizeof(ControlBusRawInput)` exceeds 5120 bytes after the change
3. `sizeof(ControlBusFrame)` exceeds 5120 bytes after the change
4. The build produces warnings that cannot be resolved without modifying files outside the scope
5. The 128-band mel filterbank has bands with zero weights (degenerate filters) — this means the frequency range parameters need adjustment

---

## File Manifest

### CREATE (0 new files)

No new files. This is a modification of existing files.

### MODIFY (3 files)

| File | What changes |
|------|-------------|
| `firmware-v3/src/audio/pipeline/STMExtractor.h` | Add SPECTRAL_MEL_BANDS=128, SPECTRAL_FFT_BINS=42, update SPECTRAL_BINS=42, add spectral FFT buffers, update static_assert |
| `firmware-v3/src/audio/pipeline/STMExtractor.cpp` | Add 128-band mel filterbank init + apply, rewrite computeSpectralModulation to use 128-point FFT, add optional timing instrumentation |
| `firmware-v3/src/audio/contracts/ControlBus.h` | Update STM_SPECTRAL_BINS from 7 to 42, adjust static_asserts if needed |

### VERIFY (4 files — read and confirm no hardcoded `7` for spectral bins)

| File | What to check |
|------|--------------|
| `firmware-v3/src/audio/contracts/ControlBus.cpp` | memcpy uses sizeof (OK) or constant (OK), smoothing loop uses constant |
| `firmware-v3/src/audio/AudioActor.cpp` | stmSpectralEnergy computation uses constant, not hardcoded 7 |
| `firmware-v3/src/effects/enhancement/EdgeMixer.h` | Only reads scalar energy — no spectral bin iteration |
| `firmware-v3/src/network/webserver/ws/WsEdgeMixerCommands.cpp` | Only mode string parsing — no spectral data |

### DO NOT TOUCH

| File | Why |
|------|-----|
| `firmware-v3/src/audio/pipeline/FFT.h` | Working correctly, supports 128-point already |
| `firmware-v3/src/audio/pipeline/ChromaAnalyzer.h` | Unrelated subsystem |
| `firmware-v3/src/audio/AudioActor.h` | STMExtractor member declaration is already correct |
| `firmware-v3/src/effects/enhancement/EdgeMixer.h` | Reads scalars only, unaffected |
| Any file in `firmware-v3/src/effects/` | Effects consume ControlBusFrame, not STMExtractor directly |

---

## Validation Checklist

After implementation, verify each item:

- [ ] `pio run -e esp32dev_audio_esv11_k1v2_32khz` — zero errors, zero warnings
- [ ] `STMExtractor::SPECTRAL_BINS == 42`
- [ ] `ControlBusRawInput::STM_SPECTRAL_BINS == 42`
- [ ] `ControlBusFrame::STM_SPECTRAL_BINS == 42` (mirrors from RawInput)
- [ ] 128-band mel filterbank weights are in static storage (namespace scope or `static` in .cpp), not class members
- [ ] `computeSpectralModulation()` calls `fft::rfft(buffer, 128)`, not `fft::rfft(buffer, 16)`
- [ ] `computeSpectralModulation()` extracts bins 0–41 from the FFT output
- [ ] No `new`, `malloc`, or `String` anywhere in modified code
- [ ] All comments use British English (normalise, initialise, centre, colour)
- [ ] `stmSpectralEnergy` computation in AudioActor.cpp uses `STM_SPECTRAL_BINS` constant, not hardcoded 7
- [ ] `applyStmSmoothing()` in ControlBus.cpp handles 42 spectral bins correctly
- [ ] No changes to EdgeMixer, WsEdgeMixerCommands, or AudioActor.h
- [ ] Flash increase < 10 KB, SRAM increase < 1 KB (check build output size report)

---

## Changelog
- 2026-04-02: Initial draft — direct 128-band implementation replacing shootout approach
