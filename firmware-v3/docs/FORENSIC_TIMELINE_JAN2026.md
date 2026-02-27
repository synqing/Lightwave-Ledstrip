# Forensic Investigation: LightwaveOS Regression & Development Timeline

**Investigation Date:** 2026-01-17
**Period Analyzed:** December 30, 2025 - January 16, 2026
**Investigator:** Claude Opus 4.5 (parallel agent deployment)
**Status:** COMPREHENSIVE FORENSIC ANALYSIS COMPLETE

---

## Executive Summary

This forensic investigation reveals a **turbulent 18-day development arc** characterized by:

| Phase | Dates | Description |
|-------|-------|-------------|
| 1 | Dec 30-31 | K1 beat tracker deemed unreliable, Emotiscope investigation begins |
| 2 | Jan 1 | New Goertzel-based TempoTracker developed (~60% accuracy) |
| 3 | Jan 2-6 | Feature explosion: Tab5, Zone Composer, 13+ effects |
| 4 | Jan 7-8 | **Crisis**: Rewrite to onset-timing, 11 P0/P0.5 bugs fixed |
| 5 | Jan 9 | "Confidence stuck at 0.00" bug, FFT redesign begins |
| 6 | Jan 15 | Developer frustration peak: "MORE FUCKING DOG SHIT MESS" |
| 7 | Jan 16 | WIP pre-reset snapshot, LEDs non-functional |

**Root Cause:** Attempted to replace working Goertzel system with onset-timing architecture that has **fundamental design flaw** (cannot resolve harmonic ambiguity without spectral resonance).

---

## Detailed Timeline

### Phase 1: The Beginning - K1 Deemed Unreliable (Dec 30-31, 2025)

#### December 30, 2025

| Commit | Description |
|--------|-------------|
| `ee101e5` | feat(audio): Expose beat strength and enable 64-bin spectrum for effects |
| `1f8667e` | fix(serial): Add line buffering to prevent command interception |

#### December 31, 2025

**Key Discovery from Memory Record #9411:**
> "K1 beat tracker's tempo output reported as **completely unreliable** despite perceived technical superiority and complexity"

Investigation into Emotiscope source code began as potential replacement algorithm.

| Commit | Description |
|--------|-------------|
| `9c6b08b` | fix(audio): resolve K1 tempo mis-lock with confidence semantics |
| `b068173` | feat(effects): add frame-rate independent SmoothingEngine primitives |
| `5e07ebd` | feat(audio): add narrative-driven behavior selection for effects |
| `e6616c9` | feat(audio): enhance ControlBus with heavy chromagram and silence gating |
| `f6575a8` | feat(api): extend EffectContext with new audio signals and helpers |
| `ca731e3` - `f1fd250` | 10+ effects refactored with Sensory Bridge patterns |

**Architecture Decision:** Sensory Bridge patterns from Emotiscope adopted as reference architecture.

---

### Phase 2: New TempoTracker Birth (Jan 1, 2026)

#### January 1, 2026 - The Goertzel Solution

| Commit | Description |
|--------|-------------|
| `5bb96f9` | **feat(audio): add TempoTracker Goertzel-based beat detection** |
| `2ff7520` | refactor(audio): integrate TempoTracker into AudioActor pipeline |
| `f5f2990` | refactor(audio): remove obsolete K1 beat tracker |
| `5c62df9` | feat(core): complete TempoTracker integration in RendererActor |
| `b4c4779` | feat(effects): update effects for TempoTracker and fix zone buffers |
| `9d0ccd4` | feat(network): complete handler extraction and fix WiFi EventGroup bug |

**Technical Specifications (Memory Record #10024):**
```
- 96 Goertzel bins (60-156 BPM, 1 BPM resolution)
- Dual-rate novelty: spectral @ 25Hz, VU @ 50Hz
- Memory footprint: ~22KB (down from K1's ~35KB)
- Hysteresis winner selection (10% advantage, 5 frames)
- Window-free Goertzel (novelty pre-smoothed)
```

---

### Phase 3: Feature Explosion (Jan 2-6, 2026)

#### January 2, 2026 - Tab5 Encoder Controller

| Commit | Description |
|--------|-------------|
| `c4c74be` | feat(Tab5.encoder): M5Stack Tab5 ESP32-P4 encoder controller |
| `4a40ee4` | feat(K1.8encoderS3): AtomS3 ESP32-S3 encoder controller |
| `59ef000` | feat(Tab5.8encoder): Tab5 ESP32-P4 encoder controller (Arduino IDE variant) |
| `dcf5834` | refactor(project): relocate v2/ firmware to firmware/v2/ |
| `6f44409` | Fix WebSocket connection stability and handshake timeout |
| `aa3f153` | fix(network): resolve route registration order causing 404s |

#### January 3, 2026 - Zone Composer & Architecture Refactor

| Commit | Description |
|--------|-------------|
| `937c882` | refactor(core): rename Actor base class to Node |
| `ec21561` | refactor(core): rename ActorSystem to NodeOrchestrator |
| `163700b` | refactor(core): rename specialized actors to nodes |
| `4fe6f1d` | refactor(core): update all references to use Node terminology |
| `161a687` | feat(zones): Zone Composer Phase 2 - Advanced Features |
| `3f9973e` | feat(v2): Preset Manager + enhanced web UI zone controls |
| `dcaa89b` | feat(Tab5.encoder): WiFi + WebSocket integration |

**Note:** Actor → Node renaming occurred, later partially reverted.

#### January 4, 2026 - EmotiscopeEngine Integration Attempt

| Commit | Description |
|--------|-------------|
| `8162e53` | **feat(audio): Replace TempoTracker with EmotiscopeEngine v2.0** |
| `4e0962e` | feat(audio): Replace TempoTracker with EmotiscopeEngine v2.0 |
| `76ba73d` | feat(audio): migrate from EmotiscopeEngine to TempoEngine |
| `a80f75a` | fix(docs): Correct phase unwrapping bug in Emotiscope reference |
| `65abab0` | docs: add beat tracker migration debrief |
| `3c94ff7` | feat(shows): implement custom show persistence |

**Warning Sign:** Multiple commits on same day for "Replace TempoTracker" indicates instability.

#### January 5, 2026 - Brief Pause

| Commit | Description |
|--------|-------------|
| `a78982b` | Save work before controller-only WiFi implementation |

#### January 6, 2026 - Effects & Audio Enhancement

| Commit | Description |
|--------|-------------|
| `1249680` | **feat(v2/effects): add three new audio-reactive effects (IDs 88-90)** |
| `5099abb` | feat(v2/effects): enhance audio effects with percussion burst |
| `0ae7b96` | feat(v2/zones): add zone reordering by centre distance |
| `62b841c` | feat(effects): add SnapwaveEffect and WaveformEffect |
| `f5e0c30` | refactor(effects): major SaliencyAwareEffect smoothing overhaul |
| `47325c8` | refactor(effects): standardize fadeAmount, palette, smoothing |
| `357c7f6` | fix(effects): add missing targetRms in LGPStarBurstNarrativeEffect |

**New Effects Added:**
- SpectrumAnalyzerEffect (ID 88): 64-bin Goertzel visualization
- SaliencyAwareEffect (ID 89): Musical Intelligence adaptation
- StyleAdaptiveEffect (ID 90): 5 music style detection

---

### Phase 4: Crisis Day (Jan 7-8, 2026)

#### January 7, 2026 - Harness Integration

| Commit | Description |
|--------|-------------|
| `7732688` | feat(harness): Integrate Ralph Loop & PRD System |
| `7c183a4` | feat(audio): Expose existing 64-bin FFT data |
| `135acfd` | feat: audio metrics broadcasting, Tab5 OTA |
| `4bcd3d6` | fix(network): update AP SSID to LightwaveOS-AP |

#### January 8, 2026 - THE CRITICAL DAY

| Commit | Description | Severity |
|--------|-------------|----------|
| `a4dfbf9` | **feat(audio/tempo): rewrite tempo tracker from Goertzel to onset-timing** | MAJOR PIVOT |
| `5a24fc8` | feat(tempo): Phase 0 - Create test infrastructure | Phase 0 |
| `90a9054` | **fix(tempo): Phase 1 - Fix 11 P0/P0.5 critical bugs** | P0 |
| `99dd8f7` | feat(tempo): Phase 2 - Dual-bank architecture + 4 onset fixes | P0.5 |
| `ff89aee` | feat(tempo): Phase 3 - Configuration extraction (60 parameters) | Refactor |
| `a92a969` | feat(tempo): Phase 4 - Optimization + Adaptive Logic | Enhancement |
| `6b5c812` | feat(tempo): Phase 5 - State Machine + Time-Weighted Voting | Enhancement |
| `76eb636` | **fix(audio): fix critical boot crash in RhythmBank/HarmonyBank** | P0 CRASH |
| `101c4b3` | feat(audio): Add FFT WebSocket streaming | Feature |
| `d5825a2` | feat(api): Complete 3 missing API features | Feature |

##### Phase 1 Bug Details (commit `90a9054`)

| Bug ID | Description | Fix |
|--------|-------------|-----|
| P0-A | beat_tick generation - prev_phase stored too late | Store at START of advancePhase() |
| P0-B | beat_tick gating - confidence gating in wrong place | Move to getOutput() |
| **P0-C** | **Onset poisoning - 99.7% interval rejection rate** | Only update lastOnsetUs for beat-range intervals |
| P0-D | Native determinism - timer dependencies | Verified sample_counter timebase |
| P0-E | K1 baseline initialization - wrong defaults | Set to 1.0f for K1 mode |
| P0-F | Refractory period too short (100ms) | Changed to 200ms (300 BPM max) |
| P0-G | Compilation errors | Fixed syntax |

##### Boot Crash Root Cause (commit `76eb636`)

```cpp
// CRASH: LoadProhibited (EXCVADDR: 0x00000006)
// Both banks initialized GoertzelBank with nullptr, then attempted
// unsafe destructor + placement-new reconstruction.
// GoertzelBank tried to access config[i] through nullptr.

// FIX: Created static lazy-initialized getHarmonyConfigs() and
// getRhythmConfigs() functions. Removed placement-new pattern.
```

---

### Phase 5: Merge & Integration Chaos (Jan 9, 2026)

#### January 9, 2026 - FFT Redesign & Confidence Bug

| Commit | Description |
|--------|-------------|
| `9447903` | chore: Stage complete tempo tracking phase 0-6 work |
| `95e24bb` | merge: Integrate complete tempo tracking from fix/tempo-onset-timing |
| `2b7cc1e` | **feat(audio): Implement FFT redesign Phases 1-6** |
| `11c168c` | Implement real KissFFT integration for K1FFTAnalyzer |
| `2abd432` | **Fix confidence stuck at 0.00: Resolve density buffer voting failure** |

##### THE CONFIDENCE BUG - Root Cause Analysis (commit `2abd432`)

```
ROOT CAUSE: lastOnsetUs timer stalled when hi-hats triggered fast onsets:

1. Fast onsets (250+ BPM hi-hats) detected
2. Rejected as "too fast" by tempo tracker
3. BUT: lastOnsetUs was NOT updated
4. Timer never advanced
5. Next onset computed interval from same stale time
6. All intervals remained 'too fast'
7. No votes added to density buffer
8. votes_in_winner_bin = 0
9. confidence forced to 0.0 (line 856: conf = 0 / 0.0000001)

FIXES:
1. Update lastOnsetUs for ALL onsets (fast and slow)
2. Raise maxBpm from 180 to 300 BPM
3. Fix phase coherence circular dependency
```

---

### Phase 6: Tab5 Hardening (Jan 10-11, 2026)

| Commit | Description |
|--------|-------------|
| `925a74e` | feat(tab5): Add ConnectivityTab network management UI |
| `aec7c83` | chore(tab5): Add ESP32-P4 JTAG debug environment |
| `4d2fb5e` | fix(tab5): Add comprehensive watchdog protection |
| `72e00a5` | fix(tab5): Add WDT protection to ZoneComposerUI |
| `3f6a1b0` | feat(effects): Add 4 new effect modifiers (Blur, Saturation, Strobe, Trail) |
| `d5471b9` | feat(effects): Add 10 enhanced audio-reactive LGP effects |
| `c4e8b86` | feat(network): Comprehensive API and WebSocket improvements |
| `ba1c88d` | feat(v2): Core system, audio pipeline, effect improvements |
| `c8b0692` | refactor(effects): Remove deprecated audio visualization effects |
| `8735dbe` | feat: Add dithering benchmark suite |
| `78fbeab` | feat(tools): Complete dither_bench implementation |

---

### Phase 7: The Breaking Point (Jan 15-16, 2026)

#### January 15, 2026 - Developer Frustration Peak

| Commit | Description |
|--------|-------------|
| `cc2e4ce` | untracked files on feat/tempo-fft-redesign |
| `05650f9` | index on feat/tempo-fft-redesign |
| `f90d238` | **"On feat/tempo-fft-redesign: MORE FUCKING DOG SHIT MESS"** |

#### January 16, 2026 - Pre-Reset Snapshot

| Commit | Description |
|--------|-------------|
| `21cab03` | **WIP: Pre-reset snapshot - tempo/FFT redesign work** |

**DEBUGGING_FAILURE_REPORT.md created documenting:**
- LEDs never turn on despite firmware booting successfully
- Multiple fix attempts failed
- Stack overflow fix applied but didn't solve LED issue
- Two separate bugs conflated
- No systematic runtime debugging performed

---

## Beat Tracking Architecture Evolution

```
Generation 1 (Pre-Dec 30): K1-Lightwave Integration
├── 121-resonator Goertzel bank (60-180 BPM)
├── Family scoring, LOST/COAST/LOCKED state machine
├── Memory: ~40KB
└── Status: ABANDONED - "completely unreliable"

Generation 2 (Jan 1): Goertzel TempoTracker
├── 96-bin Goertzel (60-156 BPM, 1 BPM resolution)
├── Dual-rate novelty (spectral 25Hz + VU 50Hz)
├── Hysteresis winner selection
├── Memory: ~22KB
└── Status: WORKING (~60% accuracy)

Generation 3 (Jan 4): EmotiscopeEngine v2.0 Hybrid
├── 64-bin Goertzel + Emotiscope parity attempt
├── Complex hybrid novelty extraction
├── Memory: ~25KB
└── Status: FAILED (compilation errors, bugs)

Generation 4A (Jan 8): Onset-Timing Architecture
├── Inter-Onset Interval (IOI) based tracking
├── NO Goertzel resonators
├── FUNDAMENTAL FLAW: Cannot distinguish 60 vs 120 BPM
└── Status: CATASTROPHIC FAILURE → "confidence stuck at 0.00"

Generation 4B (Jan 9+): FFT Redesign
├── KissFFT integration attempted
├── Branch: feat/tempo-fft-redesign
└── Status: WIP/ABANDONED
```

---

## Critical Bug Registry

| Bug | Date Introduced | Date Fixed | Root Cause | Impact |
|-----|-----------------|------------|------------|--------|
| K1 unreliability | Pre-Dec 30 | Never | Spectral flux noise, phase independence | Unusable tempo |
| 99.7% interval rejection | Jan 8 | Jan 8 | Onset poisoning from hi-hats | No beat detection |
| Boot crash (RhythmBank) | Jan 8 | Jan 8 | nullptr + placement-new | Device won't boot |
| Confidence stuck at 0.00 | Jan 9 | Jan 9 | Timer stall in IOI tracker | No confidence |
| LED non-functionality | Unknown | **UNRESOLVED** | Unknown | LEDs never light |
| WiFi EventGroup stale | Pre-Jan 1 | Jan 1 | FreeRTOS bits persist | "IP: 0.0.0.0" bug |

---

## Why Onset-Timing Architecture Failed

### The Fundamental Flaw

**Onset-Timing Approach:**
```
1. Detect onsets from novelty peaks
2. Measure time between onsets (IOIs)
3. Vote IOIs into density buffer at BPM bins
```

**FATAL PROBLEM:** Cannot distinguish harmonic ambiguity

| Scenario | 60 BPM Kick | 120 BPM Hi-hat | IOI Pattern |
|----------|-------------|----------------|-------------|
| Beat 1 | KICK | hat | 500ms |
| Beat 2 | KICK | hat-hat | 500ms |
| Beat 3 | KICK | hat | 500ms |

Both 60 BPM kick and 120 BPM hi-hat produce 500ms intervals!

**Goertzel Approach (works):**
```
- Each tempo bin resonates at specific BPM frequency
- Energy naturally accumulates at correct tempo
- Harmonic relationships handled by family scoring
- Spectral information enables frequency separation
```

---

## Emotional Arc (from Memory Records)

| Date | Source | Sentiment | Quote |
|------|--------|-----------|-------|
| Dec 31 | Memory #9411 | Frustrated | "completely unreliable despite perceived technical superiority" |
| Jan 1 | Memory #10024 | Optimistic | "Memory footprint reduced...hysteresis winner selection" |
| Jan 8 | Memory #12600 | Determined | "Phase 1 Complete: 11 Critical Tempo Tracking Bugs Fixed" |
| Jan 8 | Memory #12391 | Exhausted | "48-Hour Debugging Forensic Analysis" |
| Jan 15 | Git commit | Furious | "MORE FUCKING DOG SHIT MESS" |

---

## Current State Assessment

### What's Broken

| Component | Status | Notes |
|-----------|--------|-------|
| LED output | BROKEN | Never lights up, root cause unknown |
| Tempo tracking | UNSTABLE | Between Goertzel/onset/FFT approaches |
| feat/tempo-fft-redesign | WIP | Pre-reset snapshot state |

### What's Working

| Component | Status | Notes |
|-----------|--------|-------|
| Firmware boot | OK | No crashes after RhythmBank fix |
| WiFi | OK | EventGroup fix applied |
| Zone system | OK | Phase 2 features complete |
| Tab5 encoder | OK | WebSocket integration working |
| API v1 | OK | Full REST/WebSocket implementation |
| 90+ effects | OK | Including 13 new audio-reactive |

### Modified Files (Uncommitted)

```
firmware/v2/src/audio/tempo/TempoTracker.cpp
firmware/v2/src/audio/tempo/TempoTracker.h
firmware/v2/src/audio/AudioActor.cpp
firmware/v2/src/audio/AudioCapture.cpp
firmware/v2/src/core/actors/Actor.cpp
firmware/v2/src/core/actors/ActorSystem.cpp
firmware/v2/src/config/audio_config.h
+ others
```

---

## Recommendations

### Immediate Actions (Today)

1. **STOP** further tempo architecture changes
2. **REVERT** to known-working Goertzel TempoTracker (Generation 2)
3. **ADD LOGGING** to trace LED render path
4. **VERIFY** reference firmware actually works on hardware

### Short-Term (This Week)

1. Fix LED non-functionality with systematic debugging:
   - Add debug output to verify render loop runs
   - Verify FastLED.show() is called
   - Verify LED buffer contains non-zero values
   - Test with simple "all LEDs red" override
2. Stabilize on Goertzel-based tempo tracking
3. Tune existing parameters rather than rewrite architecture

### Medium-Term (Next 2 Weeks)

1. Implement perceptually-weighted spectral flux
2. Add multi-band onset detection
3. Improve confidence formula with multiple factors

### Long-Term (Future)

1. Hybrid Goertzel + onset-timing (onset for phase correction only)
2. Machine learning tempo confidence weighting
3. Genre-adaptive parameters

---

## Lessons Learned

### 1. Working > Perfect
Generation 2 Goertzel was 60% accurate but reliable. Chasing 90% broke everything.

### 2. Onset-Timing is NOT a Replacement for Resonance
It's a phase correction technique, not a primary tempo tracker. IOI cannot distinguish harmonic ambiguity.

### 3. Fix One Thing at a Time
Phase 1 fixed 11 bugs simultaneously. Some fixes introduced new problems. Should have been incremental.

### 4. Systematic Debugging Matters
LED issue remains because no runtime debugging was added. Hours spent reading code instead of measuring.

### 5. Preserve Working Baselines
The "MORE FUCKING DOG SHIT MESS" commit shows what happens when you iterate too far without a stable baseline.

### 6. Conflating Bugs is Dangerous
Stack overflow ≠ LED non-functionality. Fixing one doesn't fix the other.

---

## Investigation Methodology

### Sources Analyzed

- **Git History:** 150+ commits (Dec 30, 2025 - Jan 16, 2026)
- **Episodic Memory:** 20+ conversation records
- **Claude-Mem:** 36+ observations across sessions
- **Documentation:** 10+ debug/audit reports
- **Branches:** main, feat/tempo-fft-redesign, fix/tempo-onset-timing, feature/emotiscope-tempo-replacement

### Tools Used

- Git log analysis with date filtering
- Episodic memory semantic search
- Claude-mem observation retrieval
- Parallel Explore agents (3 deployed)
- File content analysis

---

## Appendix: Branch Inventory

| Branch | Purpose | Status |
|--------|---------|--------|
| `main` | Production baseline | Stable but outdated |
| `feat/tempo-fft-redesign` | FFT-based tempo redesign | WIP/Abandoned |
| `fix/tempo-onset-timing` | Onset-timing experiment | Merged, caused bugs |
| `feature/emotiscope-tempo-replacement` | EmotiscopeEngine port | Abandoned |
| `backup-before-reset` | Safety backup | Archive |
| `main-backup-jan4` | Jan 4 snapshot | Archive |

---

*Investigation completed 2026-01-17 by Claude Opus 4.5*
*Parallel agent deployment: 3 Explore agents + 11 MCP tool invocations*
