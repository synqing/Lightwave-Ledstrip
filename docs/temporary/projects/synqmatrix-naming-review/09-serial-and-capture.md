---
abstract: "Naming inventory for the serial subsystem: SerialCLI user-typed command strings (single-char and multi-char), SerialJsonGateway JSON request `type:` values + request/response field names, CaptureStreamer `capture *` subcommands + binary protocol header/metrics trailer fields, and ValidationMode `VAL:` stimulus protocol. Flags every leftover `songAware` user-facing token in the CLI prose and JSON wire layer that the rename to synqmatrix has not yet swept through."
---

# 09 — Serial CLI, JSON Gateway, Capture & Validation

Read-only naming extraction for `firmware-v3/src/serial/`. British English prose; names + signatures + wire-strings only; bodies elided.

## Files inspected

| File | Lines | Role |
|------|-------|------|
| `firmware-v3/src/serial/SerialCLI.h` | 109 | Public interface — `SerialCLI` class, `SerialCLIDeps` struct |
| `firmware-v3/src/serial/SerialCLI.cpp` | 3305 | Command dispatch, single-char hotkeys, multi-char text commands |
| `firmware-v3/src/serial/SerialJsonGateway.h` | 56 | `processSerialJsonCommand` entry, `SerialJsonGatewayDeps` |
| `firmware-v3/src/serial/SerialJsonGateway.cpp` | 1555 | JSON `type:` dispatcher (54 type strings) |
| `firmware-v3/src/serial/CaptureStreamer.h` | 126 | `CaptureStreamer` class — async producer, frame assembly |
| `firmware-v3/src/serial/CaptureStreamer.cpp` | 689 | `capture *` subcommands, binary frame protocol v1/v2/v3/v4 |
| `firmware-v3/src/serial/ValidationMode.h` | 549 | `VAL:` 2AFC stimulus protocol (header-only) |
| `firmware-v3/src/serial/CLAUDE.md` | n/a | Folder context note |

## 1 — SerialCLI user-typed command strings

### 1.1 Single-character hotkeys (immediate, no Enter)

Grouped by subsystem.

#### Effect selection / navigation (`!inZoneMode`)
| Key | Action |
|-----|--------|
| `0`..`9` | Select display-order effect 0–9 (`6` cycles audio effects: `EID_AUDIO_WAVEFORM` → `EID_AUDIO_BLOOM`) |
| `a`..`k` (non-reserved) | Select display-order effect 10–20 |
| `Space` / `n` | Next effect in active register (rate-limited ≥20 ms) |
| `N` | Previous effect in active register |
| `L` | Jump to last effect in current register |
| `l` | List all effects |
| `r` | Switch to **Reactive** register |
| `m` | Switch to **aMbient** register |
| `*` | Switch back to **All** register |
| `?` | Cycle renderer mode (Unified ↔ Independent) |
| `|` | Toggle active strip for effect-cycle keys (Independent only) |

#### Parameters
| Key | Action |
|-----|--------|
| `+` / `=` | Brightness +16 |
| `-` / `_` | Brightness -16 |
| `[` | Speed −1 |
| `]` | Speed +1 |
| `,` | Previous palette |
| `.` | Next palette |
| `i` | Mood +16 |
| `I` | Mood −16 |
| `s` | Print actors status |

#### SynqMatrix (Director) — single-char
| Key | Action |
|-----|--------|
| `D` | `cycleSynqMatrixMode()` |
| `G` | `cycleSynqMatrixProfile()` |
| `Q` | `printSynqMatrixCompactStatus()` |

#### EdgeMixer
| Key | Action |
|-----|--------|
| `e` | Cycle EdgeMixer mode |
| `w` / `W` | Spread +5 / −5 |
| `<` / `>` | Strength −15 / +15 |
| `y` | Toggle spatial (uniform ↔ centre_gradient) |
| `Y` | Toggle temporal (static ↔ rms_gate) |
| `}` | Save EdgeMixer to NVS |
| `#` | Print EdgeMixer status |

#### Colour correction
| Key | Action |
|-----|--------|
| `c` | Cycle correction mode (OFF → HSV → RGB → BOTH) |
| `C` | Show colour-correction status |
| `E` | Toggle auto-exposure |
| `g` | Toggle gamma / cycle 2.2 → 2.5 → 2.8 → OFF |

#### Bloom Parity tuning (BP::*)
| Key | Action |
|-----|--------|
| `p` / `P` | Prism opacity +/− 0.05 |
| `o` / `O` | Bulb opacity +/− 0.05 |
| `M` | Cycle PrismMode (Bloom-Parity test mode) |
| `f` / `F` | Alpha +/− 0.01 |
| `h` / `H` | Square iterations +/− 1 |
| `j` / `J` | Prism iterations +/− 1 |
| `k` / `K` | gHue speed +/− 0.25 |
| `u` / `U` | Spatial spread +/− 16 |
| `v` / `V` | Intensity coupling +/− 0.1 |

#### RD Triangle
| Key | Action |
|-----|--------|
| `t` / `T` | F −/+ 0.001 (range 0.030–0.050) |
| `b` / `B` | K −/+ 0.001 (range 0.055–0.075) |

#### Other
| Key | Action |
|-----|--------|
| `R` | Rose Bloom — cycle test mode |
| `a` | Toggle audio debug |
| `d` | Toggle Bloom debug |
| `q` | Toggle cinema post-processing |
| `A` | Toggle narrative auto-play |
| `@` | Print narrative status |
| `!` | List transition types |
| `z` | Toggle zone mode |
| `Z` | Print zone status |
| `S` | Save all settings to NVS |
| `1`..`5` (zone mode) | Load zone preset |
| `` ` `` | Cycle status-strip idle mode |
| `x` / `X` | Bands observability one-shot dump |

### 1.2 Multi-character text commands (parsed by `handleMultiCharCommand`)

Listed in the order the dispatcher attempts them.

#### VAL — validation stimulus gateway
- `VAL:*` — forwarded to `RendererActor::enqueueValidationCommand()` (see §5)

#### Bands / observability
- `x`, `bands` — print live `BandsDebugSnapshot`
- `vp stack`, `vpstack`, `vp-stack` — print VP stack snapshot

#### SynqMatrix (with `sa` short alias)
All commands accept both `synqmatrix <verb>` and `sa <verb>`:

| Long form | Short | Effect |
|-----------|-------|--------|
| `synqmatrix` / `synqmatrix status` | `sa` / `sa status` | Compact status |
| `synqmatrix on` | `sa on` | Enable (mode=Assist, switching=false) |
| `synqmatrix off` | `sa off` | Disable |
| `synqmatrix mode <off\|assist\|director>` | `sa mode <…>` | Set mode (legacy `subtle/balanced/high/high_energy/on/parameter` mapped) |
| `synqmatrix profile <subtle\|balanced\|high>` | `sa profile <…>` | Set profile |
| `synqmatrix switch on` / `synqmatrix switching on` | `sa switch on` / `sa switching on` | Constrained switching ON (Director only) |
| `synqmatrix switch off` / `synqmatrix switching off` | `sa switch off` / `sa switching off` | Constrained switching OFF |
| `synqmatrix wipe` / `synqmatrix reset` | `sa wipe` / `sa reset` | `reset()` runtime state |
| `synqmatrix restore` | `sa restore` | Restore safe baseline |
| `synqmatrix dbg [N]` / `synqmatrix debug` | `sa dbg [N]` / `sa debug` | Debug level 0–N |
| `synqmatrix policy` | `sa policy` | (deprecated → `sa dbg 2`) |
| `synqmatrix allowlist` | `sa allowlist` | Print policy allowlist |
| `synqmatrix allow <state> on\|off` | `sa allow <state> on\|off` | Edit allowlist |
| `synqmatrix allow reset` | `sa allow reset` | Reset allowlist |
| `synqmatrix health` | `sa health` | (deprecated → `sa dbg 3`) |
| `synqmatrix counters reset` | `sa counters reset` | Reset health counters |

#### Render / metrics
- `dither` / `dither status` / `dither 0|1|off|on|true|false`
- `vrms` — print VRMS metrics
- `merge`, `merge status`, `merge dump`, `merge clear [src]`, `merge <param> <value> [source]` (param ∈ `brightness|speed|intensity|saturation|complexity|variation|hue|mood|fadeAmount|paletteIdx`; source 2=ai_agent, 3=gesture)

#### Colour correction multi-char
- `c` (also handled here), `cc`, `cc <0-3>`, `ccN`
- `ae`, `ae 0|1`, `aeN`, `ae <0-255>`
- `gamma`, `gamma <0|1.0-3.0>`, `gammaN`
- `brown`, `brown 0|1`, `brownN`
- `Csave` — save colour correction settings to NVS

#### Capture (delegated to `CaptureStreamer::handleCommand`)
- `capture *` — see §3.

#### Effect selection
- `effect <id>` — accepts decimal display index or `0x…` EffectId

#### Strip / renderer mode (Phase 1B/C)
- `mode` / `mode 0` / `mode u` / `mode unified` / `mode 1` / `mode i` / `mode independent`
- `s0 [id]` / `s1 [id]` — per-strip effect assignment (read-back if no id)

#### Tempo (ESV11 backend)
- `tempo` — dump current params
- `tempo <key> <value>` where key ∈ `gate_base, gate_scale, gate_tau, conf_floor, valid_thr, stab_tau, hold_us, oct_runs, decay_floor, oct_ratio_lo, oct_ratio_hi, ws_sep_floor, conf_decay, generic_persist_us`

#### Audio debug
- `validation_stats`, `val_stats`
- `stack_usage`, `stack_profile`
- `adbg`, `adbg <0-5>`, `adbgN`, `adbg status`, `adbg spectrum`, `adbg beat`, `adbg interval <N>`

#### Unified debug
- `dbg`, `dbg <0-5>`
- `dbg audio <0-5>`, `dbg render <0-5>`, `dbg network <0-5>`, `dbg actor <0-5>`, `dbg motion <0-5>`
- `dbg motion` (one-shot field print), `dbg status`, `dbg spectrum`, `dbg beat`, `dbg memory`
- `dbg interval status <N>`, `dbg interval spectrum <N>`

#### Trace / bench
- `trace` — flush MabuTrace buffer (gated `FEATURE_MABUTRACE`)
- `bench`, `bench list`, `bench begin <name>`, `bench split <variant>`, `bench end`, `bench toggle <name> <on|off|true|false|1|0>`, `bench reset`

#### Zones / validation
- `zs <zoneId> <speed>` or `zs <s0> <s1> <s2>` — zone speed
- `validate <effect_id>` — automated centre-origin / hue / FPS / heap audit

#### WiFi
- `wifi` / `wifi status`
- `wifi connect [SSID] [PASSWORD]`
- `wifi ap`
- `wifi scan`

### 1.3 CLI naming inconsistencies

| Concern | Examples |
|---------|----------|
| **Verb casing inside multi-word commands** is space-separated, lowercase | `sa allow reset`, `merge clear`, `dbg interval status` |
| **Snake_case vs space form** coexist | `validation_stats` + `val_stats` (alias); `stack_usage` + `stack_profile`; `gate_base/conf_floor/oct_runs` (tempo params, snake_case); `dither status` (space) |
| **Hyphen variant accepted only for `vp-stack`** | `vp stack`, `vpstack`, `vp-stack` — sole hyphenated form in the CLI |
| **No-separator vs separated argument** | `cc1` and `cc 1` both work; `ae0`/`ae 0`; `gamma1.5`/`gamma 1.5`; `brown0`/`brown 0`; `adbg2`/`adbg 2`. Tolerated but undocumented as a single convention. |
| **Deprecation pathway uses prose, not codes** | `synqmatrix on` prints `songAware: 'on' is deprecated; …`; `sa policy` prints `songAware: 'policy' is deprecated; use 'sa dbg 2'`. The deprecated-noun is `songAware` (legacy alias for the subsystem) — see §6 below. |
| **VAL protocol uppercases sub-tokens** | `VAL:PULSE`, `VAL:CHASE`, `VAL:GRADIENT`, `VAL:SPARKLE`, `VAL:WAVE`, `VAL:FADE`, `VAL:NOISE`, `VAL:CLEAR`, `VAL:ACK`, `VAL:DIM` — diverges from all other lowercase CLI commands. |
| **Two distinct "sa" abbreviations** | `sa` is short for `synqmatrix`. No collision with the single-char `s` (status) — but a stray user typing `sa` without intent gets a status print, not a parse error. |
| **`reset` vs `wipe`** | `sa reset` is deprecated in favour of `sa wipe`; the prose flags this. |

## 2 — SerialJsonGateway JSON `type:` request strings

Single dispatcher (`processSerialJsonCommand`) on lines beginning with `{`. Top-level request envelope: `{"type": "…", "requestId": "…", …payload}`. Response envelope: `{"type": "…", "requestId": "…", "success": true|false, "data": {…}}` or `{"type":"error", "requestId":"…", "success":false, "error":"…"}`.

### 2.1 Request types (54)

Grouped by namespace prefix.

| Group | Types |
|-------|-------|
| Device | `device.getStatus` |
| Effects | `effects.getCurrent`, `effects.list`, `effects.getCategories`, `effects.parameters.set` |
| Parameters | `parameters.get` |
| Single-param setters | `setEffect`, `setBrightness`, `setSpeed`, `setPalette`, `setHue`, `setIntensity`, `setSaturation`, `setComplexity`, `setVariation`, `setMood`, `setFadeAmount` |
| Zones (list / aliases) | `zones.list`, `zones.update`, `zones.enabled`, `zones.setPreset` |
| Zone (singular) | `zone.enable`, `zone.setEffect`, `zone.setBrightness`, `zone.setSpeed`, `zone.setPalette`, `zone.setBlend`, `zone.loadPreset` |
| Transitions | `transition.getTypes`, `transition.trigger`, `transition.config` |
| EdgeMixer | `getEdgeMixer`, `setEdgeMixer`, `saveEdgeMixer` |
| Render | `render.dithering.get`, `render.dithering.set` |
| SynqMatrix (named `songAware.*` on the wire) | `songAware.config.get`, `songAware.config.set`, `songAware.status`, `songAware.reset`, `songAware.restore`, `songAware.debug`, `songAware.policy`, `songAware.allowlist`, `songAware.allowlist.set`, `songAware.allowlist.reset`, `songAware.health`, `songAware.counters.reset`, `songAware.countersReset` (alias) |
| Colour correction | `colorCorrection.getConfig`, `colorCorrection.setConfig` |
| Prim8 (8-dim primitive bundle) | `prim8.set` |
| Shows | `show.list`, `show.upload`, `show.play`, `show.pause`, `show.resume`, `show.stop`, `show.delete`, `show.status` |
| Narrative | `narrative.setPhase`, `narrative.config` (alias of `setPhase`) |
| Trinity (audio sync) | `trinity.sync`, `trinity.macro`, `trinity.beat` |
| Error response | `error` (response-only type) |

### 2.2 Request-payload field names (by group)

| Type(s) | Fields read |
|---------|-------------|
| All | `type`, `requestId` |
| `effects.list` | `offset`, `limit` |
| `effects.parameters.set` | `effectId`, `parameters` (JsonObject) |
| `setBrightness/setSpeed/setHue/setIntensity/setSaturation/setComplexity/setVariation/setMood/setFadeAmount` | `value` |
| `setEffect` | `effectId` |
| `setPalette` | `paletteId` |
| `transition.trigger` | `toEffect`, `transitionType` |
| `setEdgeMixer` | `mode`, `spread`, `strength`, `spatial`, `temporal` |
| `render.dithering.set` | `enabled` |
| `songAware.config.set` | `enabled`, `mode`, `profile`, `familyMorphing`, `constrainedSwitching`, `switchingEnabled`, `sensitivity`, `intensityScalar`, `motionScalar`, `confidenceFloor` |
| `songAware.allowlist.set` | `state`, `enabled` |
| `colorCorrection.setConfig` | `mode`, `hsvMinSaturation`, `rgbWhiteThreshold`, `rgbTargetMin`, `autoExposureEnabled`, `autoExposureTarget`, `gammaEnabled`, `gammaValue`, `brownGuardrailEnabled`, `maxGreenPercentOfRed`, `maxBluePercentOfRed`, `vClampEnabled`, `maxBrightness`, `saturationBoostAmount` |
| `prim8.set` | `zone`, `pressure`, `impact`, `mass`, `momentum`, `heat`, `space`, `texture`, `gravity`, `paletteId` |
| `zone.enable`/`zones.enabled` | `enable`, `enabled` (either accepted) |
| `zone.setEffect` | `zoneId`, `effectId` |
| `zone.setBrightness` | `zoneId`, `brightness` |
| `zone.setSpeed` | `zoneId`, `speed` |
| `zone.setPalette` | `zoneId`, `paletteId` |
| `zone.setBlend` | `zoneId`, `blendMode` |
| `zones.update` | `zoneId`, optional `effectId`, `brightness`, `speed`, `paletteId`, `blendMode` |
| `zone.loadPreset`/`zones.setPreset` | `presetId` |
| `transition.config` | `enabled`, `defaultDuration`, `defaultType` |
| `show.play`/`show.delete` | `showId` |
| `narrative.setPhase`/`narrative.config` | `enabled`, `phase`, `phaseDuration`, `tension`, `buildDuration`, `holdDuration`, `releaseDuration`, `restDuration` |
| `trinity.sync` | `action` ∈ `start\|stop\|pause\|resume\|seek`, `position_sec`, `bpm` |
| `trinity.macro` | `energy`, `vocal_presence`, `bass_weight`, `percussiveness`, `brightness` |
| `trinity.beat` | `bpm`, `beat_phase`, `tick`, `downbeat`, `beat_in_bar` |

### 2.3 Response-data field names (union across all responders)

Echoed envelope fields: `type`, `requestId`, `success`, `data`, `error`.

Domain-specific fields (alphabetised within group; sourced from `data["…"] = …` and `obj["…"] = …` assignments):

| Group | Fields |
|-------|--------|
| `device.getStatus` | `freeHeap`, `uptimeMs`, `wifi`, `fps`, `effectCount` |
| `effects.getCurrent` | `effectId`, `name` |
| `effects.list` | `total`, `offset`, `limit`, `effects[]` (each: `id`, `name`) |
| `effects.getCategories` | `categories[]` (each: `name`, `count`) |
| `parameters.get` | `brightness`, `speed`, `paletteId`, `paletteName`, `effectId`, `intensity`, `saturation`, `complexity`, `variation`, `mood`, `hue`, `fadeAmount` |
| `set*` single-param | echoes the parameter name: `effectId`, `brightness`, `speed`, `paletteId`, `paletteName`, `intensity`, `saturation`, `complexity`, `variation`, `mood`, `hue`, `fadeAmount` |
| `zones.list` | `enabled`, `zoneCount`, `zones[]` (each: `id`, `zoneId`, `effectId`, `effectName`, `brightness`, `speed`, `palette`, `paletteId`, `enabled`) |
| `transition.getTypes` | `types[]` (each: `id`, `name`, `durationMs`) |
| `getEdgeMixer`/`setEdgeMixer` | `mode`, `modeName`, `spread`, `strength`, `spatial`, `spatialName`, `temporal`, `temporalName` |
| `render.dithering.*` | `enabled` |
| `songAware.config.*` | `enabled`, `mode`, `profile`, `switchingEnabled`, `familyMorphing`, `constrainedSwitching`, `sensitivity`, `intensityScalar`, `motionScalar`, `confidenceFloor` |
| `songAware.status` | `enabled`, `mode`, `effectiveMode`, `profile`, `owner`, `suppressedReason`, `previousSuppressedReason`, `classificationReason`, `rawSongState`, `previousSongState`, `currentSongState`, `candidateSongState`, `intent`, `actionPlan`, `boundaryGate`, `boundaryReady`, `waitingForBoundary`, `boundaryConfidence`, `confidence`, `selectionScore`, `lastAction`, `activeEffectId`, `previousEffectId`, `selectedEffectId`, `selectedFamily`, `selectedVisualLanguage`, `lastSwitchReason`, `parameterUpdates`, `automaticEffectSwitches`, `lastDecisionAtMs`, `lastSwitchAtMs`, `stateAgeMs`, `candidateAgeMs`, `candidateHoldRemainingMs`, `dwellRemainingMs`, `cooldownRemainingMs`, `bootGraceRemainingMs`, `enableGraceRemainingMs`, `switchWindowRemainingMs`, `switchesInWindow`, `maxSwitchesPerWindow`, `antiThrashRemainingMs`, `lastSwitchFromEffectId`, `lastSwitchToEffectId`, `transitionActive`, `transitionPreviousEffectId`, `transitionTargetEffectId`, `transitionStartedAtMs`, `transitionDurationMs`, `transitionRemainingMs`, `transitionProgress`, `rms`, `flux`, `bpm`, `audioConfidence`, nested `health{healthDegraded, showSkips, failures, rmtErrors, underruns, healthCleanForMs, healthCleanWindowRemainingMs}` |
| `songAware.reset`/`songAware.restore` | `reset` / `restored`, nested `status{…}` |
| `songAware.debug`/`songAware.policy` | `bootGraceMs`, `postEnableGraceMs`, `stableStateHoldMs`, `dropStateHoldMs`, `minimumDwellMs`, `switchCooldownMs`, `switchWindowMs`, `maxSwitchesPerWindow`, `antiThrashWindowMs`, `healthCleanWindowMs`, `allowlistCount`, plus nested `config{…}`, `status{…}`, `policy{…}`, `allowlist{count, policies[]}` |
| `songAware.allowlist` policy element | `state`, `effectId`, `family`, `visualLanguage`, `reason`, `minConfidence`, `enabled` |
| `colorCorrection.getConfig`/`setConfig` | `mode`, `hsvMinSaturation`, `rgbWhiteThreshold`, `rgbTargetMin`, `autoExposureEnabled`, `autoExposureTarget`, `gammaEnabled`, `gammaValue`, `lutGenerationId`, `gammaLut{0,32,64,128,192,255}`, `brownGuardrailEnabled`, `maxGreenPercentOfRed`, `maxBluePercentOfRed`, `vClampEnabled`, `maxBrightness`, `saturationBoostAmount`, `updated` |
| `show.list` | `shows[]` (each built-in: `id`, `name`, `durationMs`, `builtin`, `looping`; each dynamic: same + `cueCount`, `slot`) |
| `show.upload` | `showId`, `cueCount`, `chapterCount`, `ramUsageBytes`, `slot` |
| `show.play` | `showId`, `source` (`dynamic`/`builtin`), `slot` or `builtinIndex` |
| `show.delete` | `deleted` |
| `show.status` | `playing`, `showId` (nullable), `paused`, `dynamicShowCount`, `dynamicShowRamBytes` |
| `narrative.setPhase` | `enabled`, `phase` (string `BUILD`/`HOLD`/`RELEASE`/`REST`), `tension`, `totalDuration` |
| `trinity.sync` | `action`, `dispatched` |

## 3 — CaptureStreamer subcommands

Entry point: `bool CaptureStreamer::handleCommand(const String& input)`.
Dispatch is `lower.startsWith("capture")` followed by `subcmd` matching.

| Subcommand | Args | Effect |
|------------|------|--------|
| `capture off` | — | `setCaptureMode(false, 0)` |
| `capture on [abc]` | letters a/b/c set tapMask bits 0x01/0x02/0x04 (default 0x07) | `setCaptureMode(true, mask)` |
| `capture dump <a\|b\|c>` | tap letter | One-shot frame dump using legacy v1 frame format (17-byte header + 960 RGB) |
| `capture stream <a\|b\|c> [fps]` | tap + optional FPS (1–60, default 15) | Start async producer task `captureTx` on Core 0 (pri 2, 4096 B stack) + `esp_timer` `capTimer` |
| `capture stop` | — | Stop timer, signal task, print stats |
| `capture fps <1-60>` | FPS | Adjust interval (no restart) |
| `capture tap <a\|b\|c>` | tap | Switch tap during active stream |
| `capture format <v1\|v2\|meta\|v3\|slim\|v4\|1\|2\|3\|4>` | format token | Select frame version |
| `capture status` | — | Print streaming, write/assemble timings, drop counters |

### 3.1 Tap enum names (consumed from `RendererActor::CaptureTap`)

`TAP_A_PRE_CORRECTION` (0x01), `TAP_B_POST_CORRECTION` (0x02), `TAP_C_PRE_WS2812` (0x04). Default stream tap is **B** (post-correction).

### 3.2 Binary frame protocol fields

Header (17 bytes, common to all versions):

| Offset | Field | Type | Notes |
|-------:|-------|------|-------|
| 0 | magic | u8 | `0xFD` |
| 1 | version | u8 | 1/2/3/4 |
| 2 | tap | u8 | `streamTapRaw` (enum value) |
| 3 | effectId | u8 | from `RendererActor::getCaptureMetadata()` |
| 4 | paletteId | u8 | — |
| 5 | brightness | u8 | — |
| 6 | speed | u8 | — |
| 7..10 | frameIndex | u32 LE | — |
| 11..14 | timestampUs | u32 LE | — |
| 15..16 | rgbLen | u16 LE | 0 / 480 / 960 |

RGB payload sizes by version:

| Version | rgbLen | Total frame | Notes |
|---------|--------|-------------|-------|
| v1 | 960 | 977 | 320 LEDs × RGB |
| v2 | 960 | 1009 | v1 + 32-byte metrics trailer |
| v3 (`meta`) | 0 | 49 | metrics only |
| v4 (`slim`) | 480 | 529 | 160 LEDs × RGB (every other LED) + metrics |

Metrics trailer (32 bytes, v2/v3/v4):

| Offset | Field | Type | Source / encoding |
|-------:|-------|------|-------------------|
| 0..1 | showTimeUs | u16 LE | `ledStats.lastShowUs`, clamped 65535 |
| 2..3 | rms | u16 LE | `BandsDebugSnapshot::rms × 65535` |
| 4..11 | bands[0..7] | 8 × u8 | `BandsDebugSnapshot::bands[i] × 255` |
| 12 | beatTick | u8 | `cbf.tempoBeatTick \|\| cbf.es_beat_tick` |
| 13 | onsetTick | u8 | `cbf.onsetEvent > 0` |
| 14..15 | flux | u16 LE | `BandsDebugSnapshot::flux × 65535` |
| 16..19 | heapFree | u32 LE | `esp_get_free_heap_size()` |
| 20..21 | showSkips | u16 LE | `ledStats.showSkips`, clamped 65535 |
| 22..23 | bpm | u16 LE | `es_bpm` else `tempoBpm`, × 100 |
| 24..25 | beatConfidence | u16 LE | `es_tempo_confidence` else `tempoConfidence`, × 65535 |
| 26..27 | onsetEnv | u16 LE | `cbf.onsetEnv × 65535` (clamped 0..1) |
| 28..29 | onsetEvent | u16 LE | `cbf.onsetEvent × 65535` |
| 30 | onsetBits | u8 | bit0 kick, bit1 snare, bit2 hihat |
| 31 | onsetProcessUs | u8 | quantised in 16-µs steps (0–4080 µs) |

`capture dump` uses its own inline header (no metrics trailer): magic `0xFD`, version `0x01`, tap byte, then effectId/paletteId/brightness/speed/frameIndex(u32 LE)/timestampUs(u32 LE)/frameLen(u16 LE) followed by 960 RGB bytes.

## 4 — SerialCLI types and helpers (named)

Public symbols:
- `struct lightwaveos::serial::SerialCLIDeps` — fields: `actors`, `renderer`, `zoneComposer`, `zoneConfigMgr`, `captureStreamer`, `showStore`, `effectIdScratch`, `validationScratch`, `effectIdScratchCap`.
- `class lightwaveos::serial::SerialCLI` — methods: `init(deps)`, `tick()`; private `initRegisters()`, `processCommand(input, firstChar)`, `handleMultiCharCommand(input, inputLower, &handled)`, `handleSingleCharCommand(cmd)`, `dispatchEffect(eid)`.
- Internal `EffectRegister` enum: `ALL`, `REACTIVE`, `AMBIENT`.

Helper functions referenced (defined elsewhere in the TU): `cycleSynqMatrixMode()`, `cycleSynqMatrixProfile()`, `printSynqMatrixCompactStatus()`, `printSynqMatrixStatus()`, `printSynqMatrixDebugLevel(level)`, `printSynqMatrixAllowlist()`, `printSynqMatrixHealth()`, `captureSynqMatrixRestorePoint()`, `restoreSynqMatrixSafeBaseline(actors)`, `printVpStackSnapshot(snap)`.

Gateway-side helpers (file-static in `SerialJsonGateway.cpp`):
- `serialJsonResponse(type, reqId, dataJson)`
- `serialJsonError(reqId, error)`
- `serialJsonDocResponse(type, reqId, JsonDocument)`
- `appendGammaLutStatus(data, status)`
- `appendColorCorrectionConfig(data, cfg, gammaStatus)`
- `appendSynqMatrixConfig(data, config)`
- `appendSynqMatrixPolicy(data, policy)`
- `appendSynqMatrixAllowlist(data, allowlist)`
- `appendSynqMatrixHealth(data, status)`
- `appendSynqMatrixStatus(data, status)`
- `appendSynqMatrixDebug(data, debug)`
- `applySynqMatrixConfigJson(root, config, &error)`
- File-scope state: `g_songAwareRestorePoint`, `g_songAwareRestorePointValid`.

## 5 — ValidationMode `VAL:` protocol

Header-only class `lightwaveos::serial::ValidationMode` parses newline-terminated ASCII commands starting with `VAL:`. Used by `tools/stimulus_generator.py` for 2AFC perceptual validation.

| Command | Key=value parameters | Default |
|---------|----------------------|---------|
| `VAL:ACK` | — | Prints `VAL:READY` |
| `VAL:CLEAR` | — | fill_solid black + active=false |
| `VAL:PULSE` | `attack`, `decay`, `intensity`, `origin` (`all\|center\|left\|right`) | 10, 300, 255, all |
| `VAL:CHASE` | `speed`, `width`, `trail`, `count` | 100, 3, 200, 1 |
| `VAL:GRADIENT` | `palette`, `scroll`, `scale` | 0, 0.0, 1.0 |
| `VAL:SPARKLE` | `density`, `fade` | 40, 200 |
| `VAL:WAVE` | `waveform` (`sine\|tri\|saw\|square`), `freq`, `speed`, `amp` | sine, 1.0, 0.5, 200 |
| `VAL:FADE` | `start_h`, `start_s`, `start_v`, `end_h`, `end_s`, `end_v`, `duration` | 0,0,0,0,0,0,1000 |
| `VAL:NOISE` | `speed`, `scale`, `palette` | 0.5, 0.3, 0 |
| `VAL:DIM` | `weight`, `time`, `space`, `flow`, `fluidity`, `impulse` | 0,0,0,0,0,0 |

Enums: `ValCommand` (`NONE`, `PULSE`, `CHASE`, `GRADIENT`, `SPARKLE`, `WAVE`, `FADE`, `NOISE`, `CLEAR`, `ACK`, `DIM`); `WaveShape` (`SINE`, `TRI`, `SAW`, `SQUARE`); `PulseOrigin` (`ALL`, `CENTER`, `LEFT`, `RIGHT`).

Parameter structs: `PulseParams`, `ChaseParams`, `GradientParams`, `SparkleParams`, `WaveParams`, `FadeParams`, `NoiseParams`, `DimParams`.

## 6 — Anomalies: leftover `songAware` / `song_aware` tokens

The CLI verbs were renamed to `synqmatrix` / `sa`, but the deprecated noun **`songAware`** still leaks through the user-facing layer in three places.

### 6.1 SerialCLI.cpp (60 occurrences)

| Surface | Examples |
|---------|----------|
| Prose `Serial.println` / `printf` literals | `songAware: …`, `songAware mode invalid …`, `songAware profile invalid …`, `songAware switching rejected …`, `songAware allow invalid …`, `songAware counters: RESET`, `songAware: 'on' is deprecated; …`, `songAware: 'reset' is deprecated; …`, `songAware: 'debug' is deprecated; use 'sa dbg 4'`, `songAware: 'policy' is deprecated; …`, `songAware: 'allowlist' is mutable; …`, `songAware: 'health' is deprecated; …`, `songAware: WIPE`, `songAware: RESTORED safe baseline (0x1302, fixed controls, off)`, `songAware: OFF` |
| Structured status lines | `songAware_status: …`, `songAware_director: …`, `songAware_health: …`, `songAware_audio: …`, `songAware_allowlist: count=…`, `songAware_policy:`, `songAware_debug: …`, `songAware_debug_gates: …`, `songAware_debug_switching: …`, `songAware_debug_transition: …` (these are the prefix tokens machine-parsers may grep for) |
| Helper function calls into `lightwaveos::synqmatrix::songAwareModeName(...)`, `songAwareProfileName`, `songAwareOwnerName`, `songAwareSuppressedReasonName`, `songAwareStateName`, `songAwareSwitchReasonName`, `songAwareIntentName`, `songAwareActionPlanName`, `songAwareBoundaryGateName`, `songAwareLastActionName`, `songAwareClassificationReasonName` | These are **library-side** translator functions — not strictly serial-layer strings, but they shape the wire output. |

### 6.2 SerialJsonGateway.cpp (45 occurrences)

| Surface | Examples |
|---------|----------|
| JSON `type:` wire strings | `songAware.config.get`, `songAware.config.set`, `songAware.status`, `songAware.reset`, `songAware.restore`, `songAware.debug`, `songAware.policy`, `songAware.allowlist`, `songAware.allowlist.set`, `songAware.allowlist.reset`, `songAware.health`, `songAware.counters.reset`, `songAware.countersReset` |
| Response type echoes | `serialJsonDocResponse("songAware.config", reqId, …)` |
| File-scope state names | `g_songAwareRestorePoint`, `g_songAwareRestorePointValid`, `captureSynqMatrixRestorePoint()` (function name uses `SynqMatrix` but mutates a `songAware`-named global) |
| Error messages | `"invalid songAware config"`, `"no restore point captured in this serial JSON session"` (latter is neutral) |
| Calls to library translators | `songAwareModeName`, `songAwareProfileName`, `songAwareOwnerName`, `songAwareSuppressedReasonName`, `songAwareStateName`, `songAwareSwitchReasonName`, `songAwareIntentName`, `songAwareActionPlanName`, `songAwareBoundaryGateName`, `songAwareLastActionName`, `songAwareClassificationReasonName` |

### 6.3 Clean — no `songAware` / `song_aware` leaks

| File | Result |
|------|--------|
| `CaptureStreamer.cpp` | 0 occurrences |
| `CaptureStreamer.h` | 0 occurrences |
| `ValidationMode.h` | 0 occurrences |
| `SerialCLI.h` / `SerialJsonGateway.h` | 0 occurrences |

### 6.4 Verdict for rename pass

- **CLI verbs (commands the user types)**: already renamed — `synqmatrix` / `sa` + aliases. No `song_aware` or `songaware` command form is accepted as input.
- **CLI human-readable output (prose, structured status lines)**: still says `songAware*`. Any external dashboard or log scraper keying off `songAware_status:` etc. is depending on the old name.
- **JSON wire types (`type` strings)**: still `songAware.*`. Tab5/iOS/PRISM Studio consumers reading or writing this gateway must continue to send `songAware.*` — there are no `synqmatrix.*` JSON types yet.
- **JSON response fields**: many use `songAware*` translator outputs (state-name strings, mode-name strings) — those are determined by the library, not the gateway, so renaming the gateway alone will not change wire-string values.
- The single hyphenated token (`vp-stack`) is the lone hyphenated CLI command; everything else uses spaces or no separator.

## 7 — Output report

- **Files inspected**: 8 (4 cpp, 4 hpp/h/md)
- **Output path**: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/docs/temporary/projects/synqmatrix-naming-review/09-serial-and-capture.md`
- **CLI commands enumerated**: 79 single-char hotkeys (including dual-key variants); 90+ multi-char text commands
- **JSON `type:` values**: 54 distinct request types, plus `error` response type
- **Capture subcommands**: 9 (`off`, `on`, `dump`, `stream`, `stop`, `fps`, `tap`, `format`, `status`)
- **VAL stimulus commands**: 10 (`ACK`, `CLEAR`, `PULSE`, `CHASE`, `GRADIENT`, `SPARKLE`, `WAVE`, `FADE`, `NOISE`, `DIM`)
- **Anomalies**: 105 `songAware`/`song_aware` tokens remain across SerialCLI.cpp (60) and SerialJsonGateway.cpp (45). CaptureStreamer, ValidationMode, and all headers are clean.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-13 | agent:subagent | Initial extraction of serial subsystem naming inventory (CLI commands, JSON types, capture protocol fields, VAL stimulus protocol). Identified 105 remaining `songAware` references across SerialCLI.cpp and SerialJsonGateway.cpp. |
