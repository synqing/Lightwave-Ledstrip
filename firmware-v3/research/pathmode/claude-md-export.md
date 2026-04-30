<!-- PATHMODE:START - Do not edit this section manually -->

# Pathmode Intent Context

_Generated 2026-04-26 from Pathmode workspace `spectrasynq/k1` via mcp__pathmode__export_context. Use as context for any agent session that touches firmware-v3 code._

## Constitution Rules

_These are mandatory constraints. Do NOT violate them._

- **[constraint]** Precedence: Constraint > Standard > Pattern. When they conflict, the higher-tier rule wins; the lower-tier rule must be amended to fit, not waived. Constraints are hard gates, not preferences. Before proposing or implementing work, every agent must identify the relevant product constraints, obey the designated authoritative source for the product, and flag stale or conflicting documentation instead of averaging it into the answer. Any implementation that expands hardware capability, network topology, control surfaces, data flow, safety behaviour, or user promises beyond the declared constraints requires explicit user approval and an updated intent/spec. Actions on shared, irreversible, or externally visible state, including commits, pushes, deploys, network/topology changes, external messages, and real-tree writes from sandboxed agents, require explicit human authorisation regardless of whether they appear within declared constraints.

- **[standard]** Standards define the evidence required before an agent may claim correctness. Every implementation or recommendation must identify its source of truth, distinguish product surface from implementation detail, name the affected client/device/surface explicitly, and include verification appropriate to the risk. Verification escalates with risk: static analysis for contracts, unit and integration tests for logic, runtime or capture evidence for timing, visual, or audio behaviour, and soak or hardware testing for irreversible commits to firmware or shared infrastructure. Each implementation must declare which verification tier was applied and why.

- **[pattern]** Patterns are the product's implementation grammar. Agents must first look for existing named patterns, helper APIs, metadata conventions, and examples before creating new behaviour. New variants must declare how they relate to the existing pattern: conforming, extending, or intentionally violating it. Intentional violations must be human-readable and programmatically detectable.

## Space Product Manifest

**Space**: K1

**Product Vision**: K1 is a dedicated hi-fi instrument for music visualization. We bet that by stripping away 'smart' distractions—apps, cloud, and latency—we can create a physical medium where light is as immediate and high-fidelity as the audio itself. It is a permanent fixture for critical listening, not a disposable party accessory.

**North Star**: End-to-end latency < 8ms (Audio-to-Photon).

**Target Audience**: High-fidelity audiophiles and dedicated music room owners who prioritize tactile hardware and zero-latency visual feedback over app-controlled smart home gimmicks.

**Constraints**:
- Microphone input only: Do not design for or reference physical Line-In/Aux features.
- Bilateral Centre-Origin: Rendering logic must assume a dual-strip (2x160 LED) layout mirrored around a central origin (LED 79/80).
- Canonical Spectrum Geography: K1 uses bass-at-centre, treble-at-edges. Origin represents low-frequency/pressure; outer edges represent treble/air. (Band 0 = centre, Band 7 = edge).
- Inversion Bypass Protocol: To bypass the Bass-at-Center constraint, the source code MUST include the comment tag '@spatial-mapping: inverted' AND the registry metadata MUST begin with '[INVERTED]'.
- Fixed 8/8 Encoder Split: Physical Unit A (Encoders 0-7) is HARD-CODED for Global Performance. Physical Unit B (8-15) is CONTEXTUAL based on touchscreen tab.
- No Hidden Shift Layers: Primary performance controls must never require a shift-key or 'hold' gesture.

**Principles**:
- Spatial Consistency: Users must be able to operate Unit A by feel. Never break the 1-to-1 mapping of global parameters.
- Aesthetic: Geometric/Abstract priority. Use premium organic smoothness. Avoid 'party-light' chaos.
- REACTIVE Pattern Contract: Patterns must respect the silence contract (fade to black via silentScale).
- Frequency-Spatial Logic: Bass/Kick = Centre (dist 0-20); Midrange = Inner-middle; Treble/Hats = Outer edge (dist 60-79).

**Key Decisions**:
- Hardware: Dual 160-LED linear strips (320 total).
- Controller: 2x M5ROTATE8 units (Unit A = Global, Unit B = Contextual).
- Unit A Map: 0:Effect, 1:Palette, 2:Brightness, 3:Sensitivity, 4:Speed, 5:Intensity, 6:Complexity, 7:Variation.
- Unit B Modes: FX Params, Edge/Color, Zones, Presets (Switched via touchscreen tabs).
- Silence Gate Logic: Single RMS test (clamp01(rmsUngated) < threshold) + sustain timer + EMA fade-to-black.

> ⚠️ **Manifest still reflects pre-2026-04-26 wording.** Several validated intents propose specific replacement text (vision, North Star, REACTIVE Pattern Contract principle, key decisions). See `firmware-v3/research/pathmode/k1-manifest-paste-pack.md` for the paste-ready replacement block. Captain to apply via Pathmode UI; this export will reflect those changes once applied.

## Active Intents (10 validated)

For full-detail per-intent spec including outcomes / constraints / edge cases / verification / health metrics, see:
- Pathmode UI: <https://pathmode.io/spectrasynq/k1>
- Per-intent agent prompts: `firmware-v3/research/pathmode/agent-prompts/`
- Synthesis report: `firmware-v3/research/k1-spec-recommendations-2026-04.md`

| Slot | ID | Title | Severity |
|------|----|----|------|
| 1 | `c3c356bc` | K1 Product Grammar Drift Control (constitution & lint rules) | high |
| 2 | `cb928ddd` | Local-only control surface clarity (device-vs-peripheral + encoder routing) | high |
| 3 | `e3102ffc` | Audio-to-photon latency programme (parallel RMT + per-stage instrumentation) | critical |
| 4 | `bf67678d` | silentScale framework enforcement (post-render multiplier + descriptor opt-out) | high |
| 5 | `fbc867b6` | Audio backend consolidation (ESV11 sole; PipelineCore deprecated) | high |
| 6 | `5a3abcab` | Render contract enforcement (per-effect budget + skip-list + Inversion Bypass + no-heap + rainbow + FrequencyMap) | high |
| 7 | `4df0963f` | BeatTracker correctness lock (regression gate + version constant + Captain re-approval) | critical |
| 8 | `05df15cc` | ControlBus field-population contract (manifest + versioning) | high |
| 9 | `a6a991ec` | Reliability core (brownout + panic log + mic SNR + Tab5 diagnostics) | high |
| 10 | `2ca2f44a` | Manufacturing & OTA (factory test + PSU/inrush + OTA validation extension) | medium |

## Captain decisions resolved 2026-04-26

1. **North Star (Slot 3)**: Pursue parallel RMT to halve FastLED wire-time floor; keep `<8 ms` defensible.
2. **BeatTracker (Slot 7)**: Comb-tooth (already landed, commit fab1802d). Locked behind version constant + regression gate + Captain re-approval requirement.
3. **PipelineCore (Slot 5)**: Deprecate from production builds now; ESV11 sole production backend.
4. **Inversion Bypass (Slot 6)**: Enforce via paired-CI gate (both markers required).
5. **silentScale (Slot 4)**: Framework post-render multiplier + `SilenceBehaviour` descriptor opt-out (`FrameworkFade` default; `InternalFade` / `IntentionallyPersistent` opt-outs); lint and runtime sampler are layered insurance, not primary.

<!-- PATHMODE:END -->

<!-- For the full Pathmode export with every intent's outcomes / constraints / edge cases / health metrics inline, regenerate via:
     mcp__pathmode__export_context(format: "claude-md", productId: "efc1a976-9527-4578-8401-634a7f86096d")
     This file is intentionally a short-form pointer; the long-form export is ~1000 lines and lives in Pathmode itself. -->
