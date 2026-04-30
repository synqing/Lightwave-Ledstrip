---
abstract: "Read-only trace of the canonical-path chord-detection chain to resolve the highest-leverage open question from AUDIO_MUSICAL_LOGIC_SOURCE_AUDIT.md (§10 Q1): does chordState.rootNote inherit the verified D#-origin chroma defect, or compensate? Verdict: rootNote INHERITS the D#-origin offset and contradicts the C-origin contract. The chord-detection algorithm itself (pure argmax + relative semitone interval templates at ControlBus.cpp:797–857) is internally consistent — chord QUALITY (major/minor/dim/aug) detection is correct because the templates use relative semitone offsets — but the rootNote integer carries an unlabelled +3-semitone offset relative to the documented contract (ControlBus.h:42, EffectContext.h:248,276). New cross-system defect: WebServerBroadcast.cpp:69–95 emits chord/key data to Tab5/iOS/dashboard clients via a hardcoded NOTE_NAMES[]={\"C\", \"C#\", ...} table indexed by the unshifted rootNote, so the K1 currently sends \"C\" when D# is detected — a systematic +3 semitone mislabel reaching every external surface. No tests exercise chord correctness. Read this when assessing chord/root/key-dependent UI, palette, or geometry behaviour. No code modified, no architecture proposed."
---

# Chord Root Origin Trace

**Mode:** READ-ONLY investigation. No source modified, renamed, or patched. No architecture proposed.
**Author:** orchestrator-claude (synthesised from 2 read-only SSAs)
**Date:** 2026-04-28
**Companion to:** [AUDIO_MUSICAL_LOGIC_SOURCE_AUDIT.md](AUDIO_MUSICAL_LOGIC_SOURCE_AUDIT.md) — resolves §10 Q1.
**Scope:** firmware-v3 chord-detection chain on canonical env `esp32dev_audio_esv11_k1v2_32khz`.
**RBDO label:** GROUNDED — every premise traced to file:line evidence; arithmetic independently verified.

---

## 0. Executive Summary

- **rootNote origin:** **D#-origin (inherits chroma defect).** rootNote is an unmodified argmax index over `frame.chroma[]`. Because chroma[0] carries D# energy (per audit §4.2), rootNote=0 means "D# detected", not "C detected" as the contract documents. No compensation, no offset constant, no rotation utility, no calibration fixture was found in any chord-detector code path.

- **Chord quality detection (major/minor/dim/aug):** **internally correct.** The algorithm uses RELATIVE semitone offsets (+3, +4, +6, +7, +8) over the same chroma array. Because chroma indices ARE semitone-spaced (the only defect is the origin rotation, not the spacing), the templates correctly identify the third and fifth relative to whatever pitch chroma[rootIdx] represents. A C-major input produces `type = MAJOR` correctly; only the rootNote integer is misnamed.

- **Highest-risk consumer:** `WebServerBroadcast.cpp:69–95`. Hardcoded `NOTE_NAMES[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}` is indexed by unshifted rootNote and broadcast to Tab5/iOS/dashboard via WebSocket `"key"` field at three call-sites (lines 191, 214, 237). **The K1 currently sends `"C"` to all external clients when the music is actually in D#.** This crosses the system boundary; severity HIGH.

- **Other affected consumers (per audit §5.2):** SbK1WaveformHarmonicEffect (HIGH, geometry-defining), LGPExperimentalAudioPack (MEDIUM, palette), BeatPulseBloomEffect (MEDIUM, palette shift), RippleEffect (MEDIUM, palette shift). The 11+ effects using `circularChromaHueSmoothed()` are unaffected (the rotation-invariant atan2 pattern produces correctly-rotated hues).

- **Tests:** **zero test fixtures** validate chord-root semantics. No "C major → rootNote==0", no chromatic-walk, no chord-template regression. `detectChord()` is defined but never invoked from any test.

- **No auxiliary helper compensates.** No KeyFinder, HarmonyAnalyzer, ChordDetector class, KeyProfile, TonalCentroid, ChromaTemplate, Krumhansl, ChordTemplate, or RootDetector class exists in src/audio/. MusicalSaliencyFrame at MusicalSaliency.h stores `prevChordRoot` as a transparent passthrough of `chord.rootNote`.

---

## 1. Evidence Table

| Claim | Status | Evidence | Source path:line | Notes |
|---|---|---|---|---|
| ChordState struct documents C-origin | VERIFIED | `uint8_t rootNote; ///< 0-11 (C=0, C#=1, D=2, ..., B=11)` | ControlBus.h:41–48 | Contract claim |
| chordState writer call-site | VERIFIED | `if (m_chord_detection_enabled) detectChord(frame.chroma, frame.chordState);` | ControlBus.cpp:670–672 | Sole call-site |
| detectChord is a member of ControlBus, no helper class | VERIFIED | Function defined inline at lines 797–857 | ControlBus.cpp:797–857 | No delegation |
| detectChord input is `frame.chroma[]` directly | VERIFIED | `void detectChord(const float* chroma, ChordState& cs)` parameter | ControlBus.cpp:797 | Not bins64, not raw Goertzel |
| Algorithm = pure argmax over chroma | VERIFIED | argmax loop at lines 801–807 | ControlBus.cpp:801–807 | `cs.rootNote = rootIdx` at line 810 |
| Type detection uses relative semitone offsets +3, +4, +6, +7, +8 | VERIFIED | `(rootIdx + 3) % 12` etc. at lines 814–818 | ControlBus.cpp:814–818 | Relative-interval template matching |
| No pitch-class offset constant in chord-detector scope | VERIFIED (absence) | grep for CHROMA_ORIGIN / PITCH_OFFSET / KEY_OFFSET in src/audio/ returned no matches | (grep result) | No compensation |
| No chroma-rotation utility (`rotate_chroma`, `transpose_chroma`, `normalise_to_c`) | VERIFIED (absence) | grep returned no matches | (grep result) | No compensation |
| No KeyFinder / HarmonyAnalyzer / ChordTemplate / Krumhansl class | VERIFIED (absence) | grep for those class names returned no matches in src/audio/ | (grep result) | Chord detection is monolithic |
| No FEATURE_AUDIO_CHORD_DETECTION compile flag | VERIFIED (absence) | Runtime flag `m_chord_detection_enabled` defaults true | ControlBus.h:528 | Always compiled, default-on |
| Confidence = triad/total energy ratio normalised to 0.4 | VERIFIED | `cs.confidence = clamp01((triadEnergy / totalEnergy) / 0.4f);` | ControlBus.cpp:842–847 | Origin-agnostic |
| MusicalSaliencyFrame.prevChordRoot is passthrough | VERIFIED | `sal.prevChordRoot = chord.rootNote;` | ControlBus.cpp:886 | No compensation |
| MusicalSaliency.h has prevChordRoot/prevChordType fields | VERIFIED | struct fields at lines 110, 115 | src/audio/contracts/MusicalSaliency.h:110, 115 | History only |
| Chord-detector docstring claims "C, C#, D, ..., B" | VERIFIED (stale) | `* @param chroma Pointer to 12-element chromagram array (C, C#, D, ..., B)` | ControlBus.cpp:795 | Contradicts implementation |
| WebServerBroadcast hardcoded NOTE_NAMES[] table | VERIFIED | `NOTE_NAMES[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}` | src/network/WebServerBroadcast.cpp:70–72 | Indexed by unshifted rootNote |
| WebSocket "key" field broadcast call-sites | VERIFIED | 3 broadcast points emit `formatKeyName(chord.rootNote, chord.type)` | src/network/WebServerBroadcast.cpp:191, 214, 237 | Cross-system mislabel surface |
| REST API does not expose chord/root | VERIFIED (absence) | grep for rootNote in REST handlers returned no matches | (grep result) | WebSocket only |
| Serial debug logging of chord/root | VERIFIED (absence) | grep for "Root:" / "Detected chord" in src/ (excluding WebSocket) returned no matches | (grep result) | None |
| No test asserts rootNote value from a known chroma input | VERIFIED (absence) | grep across test/ for chordState/rootNote in assertion context returned no matches | (grep result) | Zero test coverage |
| `detectChord()` never invoked from test code | VERIFIED (absence) | grep for `detectChord(` in test/ returned no matches | (grep result) | Defined-but-untested |

---

## 2. Root Derivation Chain

The full canonical-path chain from raw audio data to `chordState.rootNote`:

```
Microphone → I2S DMA → AudioActor (Core 0)
  → ESV11 vendor DSP (32 kHz / 125 Hz hop)
  → vendor get_chromagram() at vendor/goertzel.h:282–297
       chromagram[i % 12] += spectrogram_smooth[i] / 5.0f   (i < 60, NO offset)
  → EsV11Adapter::buildFrame() at EsV11Adapter.cpp:141–167
       frame.chroma[i] = AGC-clamped vendor chromagram[i]
  → ControlBus::applyDerivedFeatures() at ControlBus.cpp:670–672
       if (m_chord_detection_enabled)
           detectChord(frame.chroma, frame.chordState);
  → ControlBus::detectChord() at ControlBus.cpp:797–857
       (full algorithm below)
  → frame.chordState.rootNote populated
  → publish via SnapshotBuffer
```

### detectChord() algorithm (ControlBus.cpp:797–857)

The function does three things, in order:

**Step 1 — argmax over chroma (lines 801–807):**

```cpp
uint8_t rootIdx = 0;
float rootVal = chroma[0];
float totalEnergy = chroma[0];
for (uint8_t i = 1; i < CONTROLBUS_NUM_CHROMA; ++i) {
    totalEnergy += chroma[i];
    if (chroma[i] > rootVal) {
        rootVal = chroma[i];
        rootIdx = i;
    }
}
```

`rootIdx` is the index of the maximum chroma bin. `cs.rootNote = rootIdx;` is assigned at line 810.

**Step 2 — interval template indices (lines 814–818):**

```cpp
uint8_t minorThirdIdx     = (rootIdx + 3) % 12;
uint8_t majorThirdIdx     = (rootIdx + 4) % 12;
uint8_t perfectFifthIdx   = (rootIdx + 7) % 12;
uint8_t dimFifthIdx       = (rootIdx + 6) % 12;
uint8_t augFifthIdx       = (rootIdx + 8) % 12;
```

These are RELATIVE semitone offsets from `rootIdx`. The chroma index lattice IS semitone-spaced (only the origin is rotated; spacing is preserved), so `(rootIdx + N) % 12` correctly yields the index N semitones above `rootIdx` regardless of where the origin sits.

**Step 3 — chord type classification (lines ≈820–840):**

The energies at the five interval indices are read from `chroma[]`, compared, and a `ChordType` is selected:
- If `perfectFifth >= dimFifth && perfectFifth >= augFifth`: type is `MAJOR` if `majorThird > minorThird`, else `MINOR`.
- Else if `dimFifth > perfectFifth`: type is `DIMINISHED`.
- Else: type is `AUGMENTED`.

**Step 4 — confidence (lines 842–847):**

```cpp
float triadEnergy = cs.rootStrength + cs.thirdStrength + cs.fifthStrength;
if (totalEnergy > 0.01f) {
    cs.confidence = clamp01((triadEnergy / totalEnergy) / 0.4f);
} else {
    cs.confidence = 0.0f;
}
if (cs.confidence < 0.3f) cs.type = ChordType::NONE;     // lines 854–856
```

Confidence is a triad-energy ratio; it is origin-agnostic (independent of the chroma rotation).

### What this means for the C-major case

Suppose the music plays a C-major triad (C + E + G). Pitch classes 0, 4, 7 are excited. Under the canonical D#-origin fold (audit §4.2):

| Note played | Pitch class | Lands in chroma index |
|---|---|---|
| C | 0 | chroma[9] (because chroma[(0 - 3) mod 12] = chroma[9]) |
| E | 4 | chroma[1] |
| G | 7 | chroma[4] |

argmax → `rootIdx = 9` (the C is loudest). Templates:
- `(9 + 3) % 12 = 0` → minor-3rd-above-C should be at chroma[0]. chroma[0] holds D# energy. C-major has no D#, so chroma[0] is weak. ✓
- `(9 + 4) % 12 = 1` → major-3rd-above-C should be at chroma[1]. chroma[1] holds E energy. Strong (E is in the chord). ✓
- `(9 + 7) % 12 = 4` → 5th-above-C should be at chroma[4]. chroma[4] holds G energy. Strong (G is in the chord). ✓

Type detection: majorThird strong, minorThird weak, perfectFifth strong → `MAJOR`. **Correct.**

`rootNote = 9` is stored. Per the C-origin contract (ControlBus.h:42), 9 means "A". **The contract reports A-major. The music is C-major.** Mislabel by +3 semitones in the integer.

### What this means for chord QUALITY robustness

Because the templates use relative semitone offsets and the chroma lattice is semitone-spaced, **chord quality detection is invariant to the origin rotation**. Major chords are detected as MAJOR, minor as MINOR, etc. The pitch-class-origin defect is **purely a labelling defect on the rootNote integer**, not an algorithmic-correctness defect on the chord type.

> **Note on a cross-SSA refinement:** SSA-A characterised the situation as "interval template corruption" — claiming the +3/+4/+7 offsets seek wrong pitch classes. That is incorrect. The offsets are relative; they correctly identify the third and fifth above whatever pitch chroma[rootIdx] represents. The orchestrator's arithmetic above was independently verified for C-major and D-minor cases. The actual defect is narrower: rootNote integer ↔ note name mapping is shifted by +3 semitones; chord quality is intact.

---

## 3. Offset / Compensation Check

The audit's central question for this trace is: **does any code in the chord-detection chain compensate for the D#-origin chroma input?**

**Answer: No. Verified absent across every search axis.**

| Compensation mechanism searched | Result | Evidence |
|---|---|---|
| Constant offset expression `(idx + N) % 12` for N ∈ {3, 9} acting as origin shift | NOT FOUND | The +3/+4/+6/+7/+8 offsets in detectChord are interval-relative, not origin-relative. No `(idx + 3) % 12` or `(idx + 9) % 12` applied to rootNote. (ControlBus.cpp:797–857) |
| Lookup table mapping chroma indices → pitch-class names | NOT FOUND | grep for table-style declarations in src/audio/ |
| Bin-frequency-to-pitch-class recomputation (Hz → 12·log2(f/440)+9) | NOT FOUND | grep for `log2(`, `log(`, `note_for_freq` in detectChord scope |
| Constants: BOTTOM_NOTE / CHROMA_ORIGIN_OFFSET / PITCH_CLASS_OFFSET / KEY_OFFSET | NOT FOUND in src/audio/ for chord scope | BOTTOM_NOTE exists at vendor/goertzel.h:28 but is used only by Goertzel detector init, never referenced in detectChord |
| Runtime-set offset from a calibration fixture | NOT FOUND | No calibration-fixture invocation; no setter for an offset variable |
| Auxiliary chroma rotation utility (`rotate_chroma`, `transpose_chroma`, `normalise_to_c`) | NOT FOUND | grep across src/audio/ |
| KeyFinder / Krumhansl-style key-profile correlation that could imply C-relative semantics | NOT FOUND | No such class compiled |
| MusicalSaliency rotation of chord.rootNote before storage | NOT FOUND | passthrough at ControlBus.cpp:886 |
| WebServerBroadcast chord-name compensation before NOTE_NAMES[] lookup | NOT FOUND | NOTE_NAMES indexed directly by chord.rootNote at WebServerBroadcast.cpp:75 |

**Therefore:** rootNote is the unmodified argmax index over a D#-origin chroma. It contains the +3-semitone offset relative to the documented contract. No code path corrects it.

---

## 4. Consumer Impact

Re-uses prior audit §5 evidence and adds the new SSA-B finding for the WebSocket broadcast surface.

### 4.1 Cross-system surface (NEW finding from this trace)

**WebServerBroadcast — HIGH severity, system-boundary defect.**

```
src/network/WebServerBroadcast.cpp:69–95
  static constexpr const char* NOTE_NAMES[] =
      {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  ...
  const char* note = NOTE_NAMES[rootNote];     // line 75 (approx)
```

Three broadcast call-sites:
- `WebServerBroadcast.cpp:191` — `doc["key"] = formatKeyName(chord.rootNote, chord.type);`
- `WebServerBroadcast.cpp:214` — same
- `WebServerBroadcast.cpp:237` — same

**Behavioural consequence:** All WebSocket clients (Tab5 encoder, iOS app, web dashboard) receive a `"key"` field that names the wrong note. When the music is in C, the K1 broadcasts `"key": "A …"`; when the music is in D#, the K1 broadcasts `"key": "C …"`. The mislabel is systematic (+3 semitones consistent across all detections) and crosses the firmware-tab5/firmware-ios system boundary.

This is the highest-leverage finding in this trace because the defect is observable to the user without effect-rendering — anything that displays the key name (Tab5 OLED, iOS UI label, dashboard chord readout) is wrong now.

### 4.2 Effect-layer consumers (from audit §5.2, severity unchanged)

| Effect | File | Mechanism | Severity if rootNote D#-shifted |
|---|---|---|---|
| SbK1WaveformHarmonicEffect | src/effects/sensorybridge_reference/SbK1WaveformHarmonicEffect.cpp:10–14, 177–182 | Fixed pitch-class → strip-position map ("Bin 0 (C) → pixel 80") | **HIGH** — geometry is the entire effect |
| LGPExperimentalAudioPack | src/effects/ieffect/LGPExperimentalAudioPack.cpp (NOTE_HUES[12]) | Hue lookup by chroma index | MEDIUM — colour authenticity |
| BeatPulseBloomEffect | src/effects/ieffect/BeatPulseBloomEffect.cpp | `paletteShift = rootNote * 21` | MEDIUM — palette shift |
| RippleEffect | src/effects/ieffect/RippleEffect.cpp | `hue = (rootNote * 21) + chordHueShift` | MEDIUM — palette shift |
| 11+ effects using `circularChromaHueSmoothed()` | ChromaUtils.h:94–110 + 11 effects | atan2 weighted mean over chroma[] | LOW — rotation-invariant |

### 4.3 Saliency consumer

`MusicalSaliencyFrame.prevChordRoot` (MusicalSaliency.h:110) and `prevChordType` (MusicalSaliency.h:115) are written by `ControlBus::computeSaliency()` at ControlBus.cpp:872–985. Specifically `sal.prevChordRoot = chord.rootNote;` at line 886. **Transparent passthrough** — saliency carries the same +3 offset.

### 4.4 Effects calling chroma() / heavyChroma() directly

Per audit §5: 44 chroma() call-sites across 10 files; 9 heavyChroma() call-sites. The 4 hardcoded-C-origin effects above are the defect carriers; the rotation-invariant pattern (atan2 over `cos(i·30°)`/`sin(i·30°)`) shields the rest.

### 4.5 Unaffected paths

- REST API: no rootNote / chord exposure (grep returned no matches).
- Serial debug: no chord/root print statements.
- Effects using `circularChromaHueSmoothed()`: hue rotates by the same +3 offset as the data, so visually nothing is wrong (just a different anchor point on the colour wheel).

---

## 5. Test Coverage

| Test category | Status | Evidence |
|---|---|---|
| C major chord input → rootNote == 0 | NOT EXIST | grep across test/ |
| A minor chord input → rootNote == 9 | NOT EXIST | grep |
| Chromatic scale C→B → rootNote walks 0→11 | NOT EXIST | grep |
| C major triad chroma [strong @ 0,4,7] → rootNote == 0 | NOT EXIST | grep |
| A minor triad chroma [strong @ 9,0,4] → rootNote == 9 | NOT EXIST | grep |
| Any test asserting a specific rootNote value | NOT EXIST | grep for rootNote in assertion context returned no matches |
| Real-WAV chord recognition test | NOT EXIST | test_esv11_real_music.cpp validates tempo, not chord root |
| chordConfidence value test | NOT EXIST | grep |
| Chroma → chord round-trip | NOT EXIST | `detectChord(` never invoked from test code |

The only test reference to chroma values is `test/test_native/test_attack_only_pitch_velocity.cpp:230–231`, which sets `bus.chroma[0] = 0.9f; bus.chroma[7] = 0.7f;` for an unrelated velocity-rendering test — not a chord-detection assertion.

**Verdict:** zero test fixtures detect, prevent, or even surface the rootNote labelling defect.

---

## 6. Risk Register

| Risk | Evidence | Affected files/effects | Severity | Confidence | Captain decision required? |
|---|---|---|---|---|---|
| **rootNote integer is +3 semitones offset from its documented label** | §2 algorithm trace; ControlBus.cpp:801–810; audit §4.2 chroma origin | All consumers trusting "0=C, 1=C#, …" | HIGH | HIGH | YES |
| **WebSocket "key" broadcast emits hardcoded note names indexed by unshifted rootNote** | §4.1; WebServerBroadcast.cpp:69–95, 191, 214, 237 | Tab5 OLED key display, iOS UI key label, dashboard chord readout | HIGH (system boundary) | HIGH | YES |
| **Chord QUALITY (major/minor/dim/aug) detection is internally correct despite origin shift** | §2 arithmetic; ControlBus.cpp:814–818 | Effects reading chord.type | INFORMATIONAL — nothing visibly broken about quality itself | HIGH | NO (informational) |
| **MusicalSaliencyFrame.prevChordRoot inherits the offset transparently** | §4.3; ControlBus.cpp:886 | Any saliency consumer that compares prevChordRoot to a known pitch class | MEDIUM (no current consumers found that interpret prevChordRoot as a label) | HIGH | YES — clarify intent |
| **No FEATURE flag gates chord detection; runtime flag defaults true** | ControlBus.h:528 | All canonical builds | (governance) | HIGH | YES — decide whether to gate while a fix is pending |
| **Stale comment at ControlBus.cpp:795 documents C-origin parameter** | §1; ControlBus.cpp:795 | Future maintainers | LOW (comment) | HIGH | NO |
| **Zero test coverage for chord-root semantics** | §5 | Regression risk on any future fix | HIGH (governance) | HIGH | YES |
| **No KeyFinder / harmony helper exists to host a future compensation** | §1; SSA-B grep result | Architectural surface for any fix | (architectural) | HIGH | YES — Captain decides where compensation should live IF chosen |
| **Hardcoded `NOTE_NAMES[]` in WebServerBroadcast embeds C-origin assumption at the network boundary** | §4.1; WebServerBroadcast.cpp:70–72 | Cross-system contract with Tab5/iOS/dashboard | HIGH | HIGH | YES — fix at one of (chroma fold / detectChord output / NOTE_NAMES lookup) is a system-design decision |

---

## 7. Investigator Conclusion

**Conclusion 2 (verbatim from the prescribed options): "Source verifies `rootNote` inherits D#-origin chroma and contradicts the C-origin contract."**

Supporting evidence:

1. **Input is D#-origin chroma.** `frame.chroma[]` is populated by the canonical fold at `vendor/goertzel.h:288` (`chromagram[i % 12] += spectrogram_smooth[i] / 5.0f`, `i < 60`, no offset), with bin 0 = D#2 (audit §3 + §4.2). Therefore chroma[0] = D#, chroma[9] = C, etc.

2. **detectChord performs argmax with no offset.** ControlBus.cpp:797–857. The argmax loop (lines 801–807) reports the chroma index with maximum energy. `cs.rootNote = rootIdx;` (line 810) stores that index unmodified. No constant offset is applied; no lookup table; no rotation utility; no calibration step.

3. **No compensation exists upstream OR downstream within the firmware.** Verified via grep across src/audio/ for offset constants, rotation utilities, key-finders, transposition helpers, and Krumhansl-style profiles — all returned no matches.

4. **Therefore rootNote is structurally D#-origin.** rootNote=0 ↔ chroma[0] is loudest ↔ D# detected. The contract at ControlBus.h:42 and EffectContext.h:248,276 states rootNote=0 ↔ "C". Source contradicts contract.

5. **Chord QUALITY detection is correct despite the offset** (a refinement of SSA-A's framing): the +3/+4/+6/+7/+8 offsets in lines 814–818 are relative semitone steps over a still-semitone-spaced chroma array; they correctly identify the third and fifth above whatever pitch chroma[rootIdx] represents. C-major is correctly detected as MAJOR; only the rootNote integer carries the +3-semitone label shift.

6. **The defect is surfaced to external clients via WebServerBroadcast.** `NOTE_NAMES[12]` at WebServerBroadcast.cpp:70–72 is hardcoded C-origin and indexed directly by the unshifted rootNote, broadcast at three call-sites (lines 191, 214, 237). This is a system-boundary mislabel reaching Tab5, iOS, and the dashboard.

7. **No tests detect the defect.** Zero fixtures invoke `detectChord()` or assert any rootNote value; `detectChord()` is defined-but-untested.

The prior audit's §10 Q1 ("does chordState.rootNote inherit the verified D#-origin chroma defect, compensate, or remain ambiguous?") is now resolved: **inherits, no compensation, defect carries through to the WebSocket protocol surface**.

---

### Update — 2026-04-28: Resolved by lattice fix

The +3-semitone label shift documented in §0, §2, §4.1, and §6 row 1–3 has been resolved upstream of detectChord by re-anchoring the Goertzel lattice to C-origin. Source change: `firmware-v3/src/audio/backends/esv11/vendor/goertzel.h:28` `BOTTOM_NOTE 12 → 6`.

**Why detectChord itself was not patched:** §2 of this trace established that the chord algorithm (argmax + relative semitone interval templates +3/+4/+6/+7/+8) is internally correct because the offsets are relative — chord QUALITY (major/minor/dim/aug) was already accurate. Only the rootNote integer carried the +3-semitone label shift. With chroma[] now C-origin (chroma[0]=C, chroma[9]=A, etc.), `cs.rootNote = argmax(chroma)` now produces a C-origin index that matches the documented contract. WebServerBroadcast NOTE_NAMES[rootNote] (§4.1) becomes correct by consequence.

**Verification:** Captain hardware-verified on K1 V2 (MAC `b4:3a:45:a5:87:f8`) per the §0 recommended hardware-test golden path (WebSocket `key` field check, palette colour check, negative test).

**Status of trace sections:**
- §0 — *"rootNote origin: D#-origin (inherits chroma defect)"* now reads: rootNote is C-origin via lattice anchor; defect resolved.
- §6 Risk Register — rows 1, 2 (rootNote +3 offset, WebSocket NOTE_NAMES mislabel) close as a consequence of the lattice flip.
- §7 Investigator Conclusion — option 2 was correct at the time of trace; the defect is now closed at the source.

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-28 | orchestrator-claude (synthesised from 2 read-only SSAs) | Created. Resolves AUDIO_MUSICAL_LOGIC_SOURCE_AUDIT.md §10 Q1. SSA-A traced detectChord internals at ControlBus.cpp:797–857; SSA-B mapped tests, helpers, WebSocket broadcast surface, and absence of KeyFinder/HarmonyAnalyzer. Orchestrator independently verified the C-major and D-minor argmax+template arithmetic to refine SSA-A's "interval template corruption" claim — chord QUALITY detection is internally correct; the defect is purely a rootNote labelling shift, plus the new cross-system finding at WebServerBroadcast.cpp:69–95. No source modified. |
| 2026-04-28 | orchestrator-claude (engineering pass) | Defect resolved at the lattice anchor. See `### Update — 2026-04-28` above. detectChord itself remains unchanged (intentionally). |
