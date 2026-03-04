# Stage 1 Cherry-Pick Plan — Controlled Reintroduction

**Created:** 2026-03-05
**Baseline:** `stage0/k1-prephase0-baseline` branch at `e39b1152` (= `f4c579ed` + GPIO fix)
**Worktree:** `/tmp/stage0-baseline`
**Tag:** `stage0-k1-bootclean-2026-03-04`
**Stage 0 result:** PASS (30-min soak, 0 panics, 0 RMT errors, 60 effects cycled)

---

## Dependency Chain (MUST cherry-pick in this order)

```
Pick 1: a5e7d247  (tests)        — standalone, no runtime deps
Pick 2: 54765b16  (build config) — standalone, no runtime deps
Pick 3: dfc05298  (AudioContext)  — adds API surface, no callers yet
Pick 4: f8fac95b  (core actors)  — deletes EffectPostProcessor, renames CENTER_POINT
Pick 5: 4a8338d6  (55+ effects)  — DEPENDS on Pick 3 (AudioContext API) + Pick 4 (deletions/renames)
Pick 6: 9a59e899  (3 new effects)— DEPENDS on Pick 5 (registration in CoreEffects.cpp)
```

Picks 1-4 are independent of each other at the code level but MUST precede Pick 5. Pick 5 is the critical-path bottleneck. Pick 6 is additive.

---

## Universal Gates (apply to EVERY pick)

### G1: Build Gate
```bash
cd /tmp/stage0-baseline/firmware-v3
~/.platformio/penv/bin/pio run -e esp32dev_audio_esv11_k1v2_32khz
```
**Pass:** exit code 0, no warnings in effect/audio/core code (linker warnings acceptable).
**Fail:** DO NOT flash. Fix or revert.

### G2: MAC Verification Gate
```bash
~/.platformio/penv/bin/python -m esptool --port /dev/cu.usbmodem212401 read_mac
```
**Pass:** MAC = `b4:3a:45:a5:87:f8` (K1).
**Fail:** STOP. Wrong device. Verify port.

### G3: Flash Gate
```bash
~/.platformio/penv/bin/pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem212401
```
**Pass:** `Hard resetting via RTS pin...` message.

### G4: Boot Gate (via serial)
Open serial within 5s of flash, wait 15s for boot. Parse output for:
- **PASS criteria:** See `WiFi` or `AP` or `WebServer` in boot output. No panic patterns.
- **FAIL patterns:** `Guru Meditation`, `abort()`, `Stack canary`, `assert failed`, `RMT CHANNEL ERR`, `TG1WDT_SYS_RST`.

### G5: Audio Gate
Send `adbg spectrum\n` via serial after 10s of music playing.
- **PASS:** At least 3 of 8 bands > 0.05 (not all-zero, not noise floor).
- **FAIL:** All bands < 0.01 — GPIO or I2S misconfiguration.

### G6: Visual Gate
Human observation during soak (or reported by user):
- **PASS:** Both strips lit, effects visible, no single-strip blackout, no frozen frames.
- **FAIL:** Any strip dark, visible stutter/choke, frozen output.

### G7: Soak Gate
Run serial soak test for specified duration per pick.
- **PASS:** 0 panics, 0 RMT errors, effect cycling completes, audio remains live.
- **FAIL:** Any panic, RMT error, or sustained audio dropout.

### G8: Tag Gate
After soak PASS, tag the commit:
```bash
git -C /tmp/stage0-baseline tag "stage1-pick<N>-<date>"
```
This is the instant-rollback anchor.

---

## Pick 1: `a5e7d247` — Test Cleanup

### What It Contains
- Deletes entire stale `test/unit/` suite (6 files, ~2,675 lines)
- Updates `test/test_native/` mocks for 16-bit EffectId
- Simplifies `test/test_ws_color_codec/` structure
- **23 files changed**, +204 / -3,200

### Runtime Impact: NONE
Zero changes to `src/`. Entirely test infrastructure. Cannot affect ESP32 behaviour.

### Conflict Risk: LOW
`test/unit/` deletion is clean (files exist at baseline). Minor risk on `test/test_native/` mock updates if file content diverged.

### Conflict Resolution Strategy
If conflicts occur in test files, accept the incoming (cherry-picked) version — the old tests are stale.

### Acceptance Criteria
| Gate | Requirement | Duration |
|------|-------------|----------|
| G1 Build | Compiles clean | — |
| G4 Boot | No regression from Stage 0 boot | — |
| G5 Audio | Non-zero bands (same as Stage 0) | — |
| G7 Soak | **5 minutes** (minimal — no runtime change) | 5 min |
| G8 Tag | `stage1-pick1-YYYY-MM-DD` | — |

### Test Harness
```bash
# After cherry-pick, verify native tests still pass
cd /tmp/stage0-baseline/firmware-v3
~/.platformio/penv/bin/pio test -e native --verbose
```
If native test env does not exist or tests fail due to API drift from baseline, document but do not block (tests are being updated, not firmware).

### Rollback
```bash
git -C /tmp/stage0-baseline reset --hard stage0-k1-bootclean-2026-03-04
```

---

## Pick 2: `54765b16` — Build/AP-Only Config

### What It Contains
- Adds `-D WIFI_AP_ONLY=1` to K1v2 build flags in `platformio.ini`
- Removes hardcoded `upload_port` from 7 environments
- Adds `platformio_override.ini.example` (new file)
- Updates `CONSTRAINTS.md` with timing/memory budgets
- Updates `.gitignore` entries
- **5 files changed**, +118 / -19

### Runtime Impact: LOW-MODERATE
The `WIFI_AP_ONLY` flag defines a compile-time constant. If WiFiManager.h has `#ifdef WIFI_AP_ONLY` guards at baseline, this enforces AP-only at build level. If the guard does NOT exist at `f4c579ed`, the flag is defined but has no effect (harmless dead define).

### Conflict Risk: HIGH (platformio.ini)
`platformio.ini` has been modified in 8+ intervening commits. The baseline version at `f4c579ed` will differ significantly. **Manual conflict resolution is required.**

### Conflict Resolution Strategy
1. Cherry-pick with `--no-commit` to stage changes
2. For `platformio.ini`: manually apply ONLY these changes from the pick:
   - Add `-D WIFI_AP_ONLY=1` to `[env:esp32dev_audio_esv11_k1v2]` build_flags
   - Remove `upload_port` lines (already handled by override template)
   - Do NOT carry forward new envs (`pipelinecore_sta`, `pipelinecore_perf`) or TempoTracker src_filter changes — they reference code not at baseline
3. For `.gitignore`, `CONSTRAINTS.md`, `platformio_override.ini.example`: accept as-is (additive, low risk)
4. Verify the WIFI_AP_ONLY flag is consumed. Check:
   ```bash
   grep -r "WIFI_AP_ONLY" /tmp/stage0-baseline/firmware-v3/src/
   ```
   If zero matches, the flag is a no-op at this baseline (acceptable — it becomes active when WiFiManager changes are later cherry-picked).

### Acceptance Criteria
| Gate | Requirement | Duration |
|------|-------------|----------|
| G1 Build | Compiles clean with new flags | — |
| G2 MAC | Verify K1 before flash | — |
| G3 Flash | Successful upload | — |
| G4 Boot | AP starts, `LightwaveOS-AP` visible in WiFi scan | — |
| G5 Audio | Non-zero bands | — |
| G6 Visual | Both strips rendering | — |
| G7 Soak | **10 minutes** (build config change, light validation) | 10 min |
| G8 Tag | `stage1-pick2-YYYY-MM-DD` | — |

### Test Harness
```bash
# Verify AP still broadcasts after WIFI_AP_ONLY flag
# (from macOS, after boot)
swift -e 'import CoreWLAN; if let n = try? CWWiFiClient.shared().interface()?.scanForNetworks(withName:nil) { print(n.filter{($0.ssid ?? "").contains("Lightwave")}.map{$0.ssid!}) }'
```

### Rollback
```bash
git -C /tmp/stage0-baseline reset --hard stage1-pick1-YYYY-MM-DD
```

---

## Pick 3: `dfc05298` — AudioContext Accessor Layer

### What It Contains
- Adds `GetFrameRef()` method to `ControlBus.h` (+1 line)
- Adds `AudioContext` class to `EffectContext.h` (+67 lines)
- **2 files changed**, +68 / -2

### Runtime Impact: MODERATE (but currently dead code)
Adds new API surface that is NOT called by any existing effects at this baseline. `AudioContext` wraps `ControlBusFrame` with typed accessors (`getBand()`, `getChroma()`, `rms()`, `flux()`, etc.). `GetFrameRef()` provides zero-copy const-reference access. Neither is invoked until Pick 5 migrates effects.

### Conflict Risk: LOW
First commit to touch `ControlBus.h` and `EffectContext.h` in the ancestry from baseline. Should apply cleanly.

### Conflict Resolution Strategy
Should not need conflict resolution. If conflicts occur, accept incoming version.

### Acceptance Criteria
| Gate | Requirement | Duration |
|------|-------------|----------|
| G1 Build | Compiles clean — new API compiles but is unreferenced | — |
| G4 Boot | No regression | — |
| G5 Audio | Non-zero bands | — |
| G7 Soak | **10 minutes** (additive API, dead code at this point) | 10 min |
| G8 Tag | `stage1-pick3-YYYY-MM-DD` | — |

### Additional Verification
```bash
# Verify the new API compiles without warnings
grep -c "AudioContext" /tmp/stage0-baseline/firmware-v3/src/plugins/api/EffectContext.h
# Expected: > 0 (class defined)

# Verify no effects use it yet (should be zero callers)
grep -r "ctx\.audio\.\(getBand\|getChroma\|rms()\|flux()\|bass()\)" /tmp/stage0-baseline/firmware-v3/src/effects/ | wc -l
# Expected: 0
```

### Rollback
```bash
git -C /tmp/stage0-baseline reset --hard stage1-pick2-YYYY-MM-DD
```

---

## Pick 4: `f8fac95b` — Core Actor Refactor

### What It Contains
- **ActorSystem.cpp/h**: Adds stimulus override system (thread-safe pointer swap for external audio injection) (+91 lines)
- **RendererActor.cpp/h**: Adds `AudioInputMode` enum (Live/Trinity/StimulusOverride), single-pointer swap in `renderFrame()`, tightens silence gate threshold (+143/-49)
- **features.h**: Removes 10 always-on feature guards, adds mutual exclusion compile check (+80)
- **Actor.h**: Minor changes (+4)
- **EffectPostProcessor.h**: DELETED (-315 lines)
- **Sync.h**: DELETED (-99 lines)
- **8 files changed**, +208 / -564

### Runtime Impact: HIGH
This touches RendererActor (Core 1 render loop) and ActorSystem (task orchestration). Key risks:
1. **EffectPostProcessor.h deletion**: Any effect that `#include`s this at baseline will fail to compile. Must verify no remaining references.
2. **CENTER_POINT rename to CENTER_LED_INDEX**: Any effect referencing `CENTER_POINT` will fail. Must verify all references updated.
3. **Silence gate threshold change**: `gateScale <= 1u` (tighter) may cause visual differences during quiet passages.
4. **Feature guard removal**: If any code at baseline is gated behind removed `#if FEATURE_X` guards, it will change behaviour.

### Conflict Risk: HIGH
RendererActor.cpp has 143 lines changed and is a high-churn file. `features.h` restructuring will conflict if baseline has different guard structure.

### Conflict Resolution Strategy
1. Cherry-pick with `--no-commit`
2. **Before applying**: verify no effects reference deleted files:
   ```bash
   grep -r "EffectPostProcessor" /tmp/stage0-baseline/firmware-v3/src/ --include="*.cpp" --include="*.h" | grep -v "^Binary"
   grep -r "CENTER_POINT" /tmp/stage0-baseline/firmware-v3/src/ --include="*.cpp" --include="*.h"
   grep -r "Sync\.h" /tmp/stage0-baseline/firmware-v3/src/ --include="*.cpp" --include="*.h" | grep -v "^Binary"
   ```
3. If references exist, they must be updated as part of conflict resolution (remove includes, rename constants)
4. For `features.h`: carefully merge — keep mutual exclusion check, match guard state to baseline reality
5. For `RendererActor.cpp`: apply stimulus override and silence gate changes, resolve conflicts conservatively

### Acceptance Criteria
| Gate | Requirement | Duration |
|------|-------------|----------|
| G1 Build | Compiles clean — no dangling includes or missing symbols | — |
| G2 MAC | Verify K1 | — |
| G3 Flash | Successful upload | — |
| G4 Boot | No panic, no RMT error, stimulus system initialises | — |
| G5 Audio | Non-zero bands, audio path unaffected by stimulus plumbing | — |
| G6 Visual | Both strips, effects visible, silence gate behaviour acceptable | — |
| G7 Soak | **15 minutes** — core actor changes need longer validation | 15 min |
| G8 Tag | `stage1-pick4-YYYY-MM-DD` | — |

### Effect-Specific Tests
After boot, cycle through these effects (known to use trail persistence and centre-origin):
```
e 0    # Default/solid
e 1    # Breathing
e 3    # Ripple (centre-origin)
e 5    # Fire
e 99   # Legacy
e 100  # Legacy
```
Verify each renders visually correct for 30s minimum.

### Regression Signals
- **New silence gate threshold may cause dark periods**: If effects go dark during quiet music passages that Stage 0 handled, this is a regression. Document but do not revert unless sustained blackouts occur.
- **EffectPostProcessor removal may change trail behaviour**: Effects that relied on post-processing for trail fade will now show different trail characteristics. This is EXPECTED — document the visual delta.

### Rollback
```bash
git -C /tmp/stage0-baseline reset --hard stage1-pick3-YYYY-MM-DD
```

---

## Pick 5: `4a8338d6` — The Monster (55+ Effects Migration)

### What It Contains
- Migrates 55+ effects from `ctx.audio.controlBus.X` to `ctx.audio.X()` AudioContext API
- Rewrites trail persistence in 18 effects (memset/fill_solid -> fadeToBlackBy+nblend)
- Adopts AsymmetricFollower in 8 effects (changes envelope smoothing)
- Centre-origin migration in 10 effects (linear -> radial addressing)
- dt-decay corrections with `powf(rate, dt * 120.0f)`
- Perlin noise `>>8` coordinate fix in 4 ambient effects
- Bin index migration to `bands[]` API
- 7 dead/broken effects commented out in CoreEffects.cpp
- 3 new radial effects registered (but files arrive in Pick 6)
- **87 files changed**, +1,236 / -1,000

### Runtime Impact: VERY HIGH
This modifies the render() path of 55+ effects running at 120 FPS on Core 1. Every change is a potential visual regression, performance regression, or crash.

### Conflict Risk: VERY HIGH
87 files. Many effects at baseline still use old `ctx.audio.controlBus` access. The AudioContext API must already be present (Pick 3) and EffectPostProcessor deleted (Pick 4) before this pick can work.

### HARD PREREQUISITES
- **Pick 3 MUST be applied first** (AudioContext class in EffectContext.h)
- **Pick 4 MUST be applied first** (EffectPostProcessor.h deleted, CENTER_LED_INDEX rename)
- Without these, Pick 5 will not compile.

### Conflict Resolution Strategy
1. Cherry-pick with `--no-commit`
2. **Expect heavy conflicts** in:
   - `CoreEffects.cpp` (registration changes)
   - `PatternRegistry.cpp` (family registration)
   - Effect files that were modified between baseline and this commit
3. For each effect file conflict:
   - Accept the incoming (cherry-picked) version if the only change is API migration
   - If the conflict involves both API migration AND other logic changes, resolve manually ensuring centre-origin compliance
4. For `CoreEffects.cpp`: carefully merge registrations — do NOT register effects whose .cpp files don't exist yet (Pick 6's radial effects). Comment them out until Pick 6.
5. For `display_order.h` / `effect_ids.h`: accept incoming IDs, verify no collisions

### Acceptance Criteria
| Gate | Requirement | Duration |
|------|-------------|----------|
| G1 Build | Compiles clean — all 55+ migrated effects resolve AudioContext API | — |
| G2 MAC | Verify K1 | — |
| G3 Flash | Successful upload | — |
| G4 Boot | No panic, no RMT error | — |
| G5 Audio | Non-zero bands, audio path intact | — |
| G6 Visual | CRITICAL — see effect test matrix below | — |
| G7 Soak | **30 minutes** — full effect cycling with expanded test list | 30 min |
| G8 Tag | `stage1-pick5-YYYY-MM-DD` | — |

### Effect Test Matrix (MANDATORY)

These effects MUST be individually verified after Pick 5. They represent the highest-risk changes.

**Trail persistence rewrites (18 effects):**
| Effect | ID | What Changed | Verify |
|--------|----|-------------|--------|
| BreathingEffect | 1 | fadeToBlackBy+nblend | Smooth breathing, no flicker |
| RippleEsTuned | varies | Trail persistence | Ripples fade smoothly |
| LGPNeuralNetwork | varies | Centre-origin + trails | Renders from centre outward |
| LGPSolitonWaves | varies | Centre-origin + trails | Wave propagation from centre |
| LGPBirefringentShear | varies | Centre-origin | Shear pattern from centre |

**AsymmetricFollower adoption (8 effects):**
Verify smooth envelope response — no jittery or snappy transitions compared to Stage 0.

**Centre-origin migration (10 effects):**
Verify NO linear sweeps. All motion originates from LED 79/80 outward (or inward to 79/80).

**Perlin noise fixes (4 ambient effects):**
| Effect | Verify |
|--------|--------|
| LGPPerlinInterferenceWeave | Smooth ambient motion, no blockiness |
| LGPPerlinVeil | Smooth veil effect |
| LGPPerlinCaustics | Smooth caustic patterns |
| LGPPerlinShocklines | Smooth shockline motion |

**dt-decay corrections:**
Verify effects run at visually similar speed regardless of actual FPS. Effects should not appear "faster" or "slower" than Stage 0 baseline.

### Soak Test Configuration
The serial soak script must cycle through ALL modified effects, not just a subset:
```python
EFFECT_IDS = [
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,   # Core effects
    99, 100, 101, 102,                      # Legacy IDs
    # Add every effect ID modified in this pick
]
```

### Regression Detection
Run `adbg spectrum` every 60s during soak and log. Compare band energy distribution against Stage 0 log. Flag if:
- Mean band energy drops below 50% of Stage 0 baseline
- Any band is consistently zero that was non-zero in Stage 0

### Rollback
```bash
git -C /tmp/stage0-baseline reset --hard stage1-pick4-YYYY-MM-DD
```

---

## Pick 6: `9a59e899` — New Radial Effect Variants

### What It Contains
- 3 new centre-origin radial effects (6 new files, +535 lines):
  - `LGPBirefringentShearRadialEffect` (.cpp + .h)
  - `LGPNeuralNetworkRadialEffect` (.cpp + .h)
  - `LGPSolitonWavesRadialEffect` (.cpp + .h)
- **All new files, no modifications to existing code**

### Runtime Impact: LOW-MEDIUM
New effects only. Cannot break existing effects. Risk limited to:
- PSRAM allocation issues in init()
- Render time exceeding 2ms budget
- OOB access in new code

### Conflict Risk: LOW (new files)
Files do not exist at baseline — will apply cleanly. However, these effects need REGISTRATION in CoreEffects.cpp (which was handled in Pick 5). If Pick 5's CoreEffects.cpp merge already registered them, they'll be wired up. If not, manual registration is needed.

### HARD PREREQUISITE
- **Pick 5 MUST be applied first** (effect registration, AudioContext API usage in new effects)

### Conflict Resolution Strategy
Should apply cleanly. If `CoreEffects.cpp` registration was deferred in Pick 5, add registration now:
```cpp
#include "effects/ieffect/LGPBirefringentShearRadialEffect.h"
#include "effects/ieffect/LGPNeuralNetworkRadialEffect.h"
#include "effects/ieffect/LGPSolitonWavesRadialEffect.h"
// ... in registerEffects():
registry.add<LGPBirefringentShearRadialEffect>(EID_BIREFRINGENT_SHEAR_RADIAL);
registry.add<LGPNeuralNetworkRadialEffect>(EID_NEURAL_NETWORK_RADIAL);
registry.add<LGPSolitonWavesRadialEffect>(EID_SOLITON_WAVES_RADIAL);
```

### Acceptance Criteria
| Gate | Requirement | Duration |
|------|-------------|----------|
| G1 Build | Compiles clean with new effects | — |
| G3 Flash | Successful upload | — |
| G4 Boot | No panic | — |
| G5 Audio | Non-zero bands | — |
| G6 Visual | New effects render from centre outward, smooth, no crashes | — |
| G7 Soak | **15 minutes** — cycle new effects + existing effects | 15 min |
| G8 Tag | `stage1-pick6-YYYY-MM-DD` (FINAL Stage 1 tag) | — |

### Effect-Specific Tests
Switch to each new effect individually, play music for 30s:
```
e <BIREFRINGENT_SHEAR_RADIAL_ID>   # Verify centre-origin shear
e <NEURAL_NETWORK_RADIAL_ID>        # Verify centre-origin neural pattern
e <SOLITON_WAVES_RADIAL_ID>         # Verify centre-origin wave propagation
```
Confirm: centre-origin, no linear sweep, smooth audio response, no crash.

### Rollback
```bash
git -C /tmp/stage0-baseline reset --hard stage1-pick5-YYYY-MM-DD
```

---

## Test Harness: Enhanced Serial Soak Script

The Stage 0 soak script at `/tmp/stage0-soak/serial_soak_test.py` needs enhancement for Stage 1. Create `/tmp/stage1-soak/serial_soak_test.py` with these additions:

### Requirements
1. **Configurable effect list** per pick (pass via `--effects` flag or config file)
2. **Audio verification**: Parse `adbg spectrum` response, assert `bands[i] > threshold` for at least 3 of 8 bands
3. **Memory tracking**: Parse `heap_free=` and `largest=` from health responses, track min/max/trend
4. **Stack monitoring**: Parse `loopTask` stack watermark if reported, flag if < 200 bytes free
5. **Regression comparison**: Load Stage 0 baseline metrics from `/tmp/stage0-soak/soak_log.txt`, flag deviations > 50%
6. **Per-effect crash detection**: If a panic occurs within 5s of an effect change, log `EFFECT_CRASH: <effect_id>`
7. **Summary report**: JSON output with pass/fail per gate, metrics, and effect-level results

### Acceptance Thresholds
```python
THRESHOLDS = {
    "min_bands_nonzero": 3,        # At least 3 of 8 bands > 0.01
    "min_band_energy": 0.01,       # Floor for "non-zero"
    "min_heap_free_bytes": 20000,  # Below this = danger zone
    "max_stack_usage_pct": 96,     # Above this = canary risk
    "max_panics": 0,               # ZERO tolerance
    "max_rmt_errors": 0,           # ZERO tolerance
    "min_effects_cycled": 10,      # Must cycle at least this many
}
```

### Output Format
```json
{
    "pick": 5,
    "duration_min": 30,
    "verdict": "PASS",
    "panics": 0,
    "rmt_errors": 0,
    "effects_cycled": 45,
    "effects_crashed": [],
    "audio": {
        "samples": 30,
        "all_zero_count": 0,
        "mean_band_energy": [0.42, 0.38, ...]
    },
    "memory": {
        "heap_free_min": 34000,
        "heap_free_max": 78000,
        "stack_usage_max_pct": 94
    },
    "baseline_comparison": {
        "audio_drift_pct": -2.3,
        "heap_drift_pct": +1.1
    }
}
```

---

## Commits NOT in Stage 1 (Post-Stage 1 Assessment)

20 commits between `f4c579ed` and current HEAD are NOT in the 6-pick list. After Stage 1 completes, these need triage:

### Priority 1 — Critical Tech Debt Fixes (should cherry-pick next)
| SHA | Subject | Why Critical |
|-----|---------|-------------|
| `d7a84c63` | mutex-protected snapshot accessors for ControlBus | **R3 data race fix** (CRITICAL in audit) |
| `10947ab7` | thread-safe AudioActor accessors in WS/REST handlers | **R3 companion** — completes the data race fix |
| `d14e2696` | K1 runtime tuning — heap shedding, stack hysteresis, AP fix | **Addresses H-5 stack pressure and AP heap contention** |
| `032d1a5c` | clean up main.cpp, harden renderer, add HW test strip | **Includes renderer hardening from incident mitigations** |

### Priority 2 — Build/Config Improvements
| SHA | Subject |
|-----|---------|
| `c972e2a2` | K1v1 production runtime environment |
| `f82cb50a` | move K1 heap thresholds to dedicated runtime env |
| `12604cad` | fix SpectrumDetailEffect for AudioContext API |

### Priority 3 — Feature/Platform Work (defer)
| SHA | Subject |
|-----|---------|
| `a39a03b8` | stimulus override REST/WebSocket API |
| `c3cad71a` | Tab5 16-bit effect IDs |
| `569d3e4b` | iOS minor fixes |
| `58d6addb` | Tab5 Control Surface UI |
| `2c6d3cec` | camera mode and effect preset APIs |
| `d6b6b334` | iOS Rive runtime, palette resources |

### Priority 4 — Docs/Tests (low risk, cherry-pick opportunistically)
| SHA | Subject |
|-----|---------|
| `39d6dcee` | effect development standard, stimulus contract, CI check |
| `3e4f515c` | tech debt audit, Tab5 specs, pipeline harness |
| `957a07af` | Harmonix beat tracking corpus metadata |
| `6a040b36` | context-management guidance docs |

### Not Needed at Baseline
| SHA | Subject | Reason |
|-----|---------|--------|
| `f4048610` | fuse PipelineCore hot loops, cache FFT twiddles | PipelineCore not active in ESV11 K1 build |
| `c1e2925f` | harden tempo selection (ESV11) | Audio backend internals, test separately |
| `fab1802d` | bass-onset beat tracking harness | Test tooling, not runtime |

---

## Execution Sequence Summary

```
[ ] Pick 1: a5e7d247 (tests)        -> build -> 5-min soak  -> tag
[ ] Pick 2: 54765b16 (build config) -> build -> flash -> 10-min soak -> tag
[ ] Pick 3: dfc05298 (AudioContext)  -> build -> flash -> 10-min soak -> tag
[ ] Pick 4: f8fac95b (core actors)  -> build -> flash -> 15-min soak -> tag
[ ] Pick 5: 4a8338d6 (55+ effects)  -> build -> flash -> 30-min soak + full effect matrix -> tag
[ ] Pick 6: 9a59e899 (3 new effects)-> build -> flash -> 15-min soak -> tag

Total estimated soak time: ~85 minutes
Total estimated execution time (including build/flash/conflict resolution): 3-4 hours
```

---

## Operational Discipline (from Postmortem Stage 2)

These rules are in effect for the ENTIRE Stage 1 execution:

1. **Verify target MAC before every upload.** No exceptions.
2. **One variable change per test cycle.** Each pick is one variable. Do NOT combine picks.
3. **Keep AP-only mode constant.** No STA experiments during Stage 1.
4. **Record logs per commit checkpoint.** Every soak log saved to `/tmp/stage1-soak/pick<N>/`.
5. **No merging "performance", "render", and "network" changes in the same batch.** Picks 4+5 are the exception (they are the documented risk cluster), and they get the longest soak.
6. **If ANY pick fails soak, STOP.** Do not proceed to the next pick. Investigate, fix, or revert. The tag system enables instant rollback.

---

## Decision Points

### If Pick 5 conflicts are unresolvable
**Option A:** Apply Pick 5 changes manually, file by file, in sub-batches:
- Batch 5a: AudioContext API migration only (find-replace `ctx.audio.controlBus.X` -> `ctx.audio.X()`)
- Batch 5b: Trail persistence rewrites (18 effects)
- Batch 5c: Centre-origin migrations (10 effects)
- Batch 5d: dt-decay corrections and Perlin fixes
- Batch 5e: CoreEffects.cpp registration changes

Soak after each sub-batch. This turns Pick 5 into 5 mini-picks with individual rollback points.

**Option B:** Abandon cherry-pick approach for Pick 5. Instead, manually port the AudioContext API migration using a script:
```bash
# Example: automated API migration
find /tmp/stage0-baseline/firmware-v3/src/effects -name "*.cpp" -exec sed -i '' \
  's/ctx\.audio\.controlBus\.bands\[/ctx.audio.getBand(/g' {} \;
```
This is riskier (no review of individual effect logic changes) but faster.

### If Pick 4 causes visual regression
The silence gate threshold tightening and EffectPostProcessor removal will change visual behaviour. If the user deems the visual delta unacceptable:
- Revert silence gate change (keep original threshold)
- Re-introduce a minimal trail fade in RendererActor post-render (replacing EffectPostProcessor function)
- Re-soak with the adjusted version

### If new radial effects (Pick 6) crash
These are new code with no baseline. If they crash:
1. Comment out their registration in CoreEffects.cpp
2. Re-build and flash without them
3. Debug the crashing effect offline
4. The rest of Stage 1 is unaffected
