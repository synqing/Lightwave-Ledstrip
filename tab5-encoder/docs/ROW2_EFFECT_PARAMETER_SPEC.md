# Tab5 Row-2 Effect Parameter Spec

## 1) Scope
This spec defines a dynamic Row-2 UI/encoder contract for effect-specific parameters exposed by LightwaveOS via `effects.parameters.get/set`.

This is additive to fixed global controls (`effect`, `palette`, `speed`, `mood`, `fadeAmount`, `complexity`, `variation`, `brightness`).

## 2) Source-of-truth APIs

### 2.1 Global parameters (fixed, all effects)
- REST/WS logical surface: `parameters.get`, `parameters.set`
- Keys: `brightness`, `speed`, `paletteId`, `hue`, `intensity`, `saturation`, `complexity`, `variation`, `mood`, `fadeAmount`
- Sources:
  - `firmware-v3/src/network/webserver/handlers/ParameterHandlers.cpp`
  - `firmware-v3/src/codec/HttpParameterCodec.cpp`

### 2.2 Effect-specific parameters (dynamic, effect-dependent)
- WS commands: `effects.parameters.get`, `effects.parameters.set`
- WS responses: `effects.parameters`, `effects.parameters.changed`
- Sources:
  - `firmware-v3/src/network/webserver/ws/WsEffectsCommands.cpp`
  - `firmware-v3/src/network/webserver/handlers/EffectHandlers.cpp`

## 3) Input-mode contract (Tab5)

### 3.1 Unit A (ENC-A)
- `SW0`: fixed global controls (8 encoders)
- `SW1`: same controls with coarse acceleration only
- Never remap ENC-A to effect-specific parameter keys

### 3.2 Unit B (ENC-B)
- `SW0` (Preset Mode):
  - Rotaries: no-op
  - Buttons: preset actions only
    - single click: load slot
    - double click: save slot
    - long hold: delete slot
- `SW1` (FX Param Mode):
  - Rotaries: drive Row-2 effect-specific parameters (paged)
  - Buttons: Row-2 navigation/reset (defined below)

## 4) Row-2 data model

```c
struct FxParamDescriptor {
  char key[32];          // server key (authoritative)
  char displayName[48];  // UI label
  float minValue;
  float maxValue;
  float defaultValue;
  float value;
  uint8_t type;          // inferred from step/type when available
  float step;
  char group[16];        // timing/wave/blend/colour/... when provided
  char unit[8];          // s, Hz, led/s, x, ...
};
```

- Ordering: preserve firmware order from `effects.parameters.get`.
- No client-side sorting.
- Page size: 8 slots (mapped to ENC-B 0..7).

## 5) Paging model

- `paramCount = N`
- `pageSize = 8`
- `totalPages = max(1, ceil(N / 8))`
- `pageIndex in [0, totalPages-1]`
- Slot mapping:
  - `slot i` -> `paramIndex = pageIndex * 8 + i`
  - if `paramIndex >= N`, slot is disabled/blank

### 5.1 Page indicator behaviour
- Show a compact indicator line above Row-2:
  - `FX Params  p/total  (N)`
  - Example: `FX Params 2/3 (16)`
- Show mode badge:
  - `ENC-B SW1`
- Optional: 8 slot occupancy dots per page (filled=active key, hollow=empty slot)

### 5.2 Page state transitions
- On effect change:
  - force `pageIndex = 0`
  - request fresh metadata via `effects.parameters.get`
  - redraw indicator immediately as `FX Params 1/1 (loading...)`
- On metadata response:
  - compute `totalPages`
  - clamp `pageIndex` in range
  - render slots from firmware order
- On `totalPages == 1`:
  - keep nav affordance visible but disabled (no-op on press)

### 5.3 Demo mode (no paging, two fixed rows of 8)

For demo delivery speed, paging can be disabled entirely because current firmware max is 16 keys/effect.

- Row-2A shows parameter indices `0..7`
- Row-2B shows parameter indices `8..15`
- If an effect has `<16` keys, trailing slots are blank/disabled
- No page indicator needed in demo mode
- On effect change:
  - fetch `effects.parameters.get`
  - remap both rows immediately from firmware order
  - clear any slots above `paramCount`

## 6) Button semantics in ENC-B SW1 (Row-2 mode)

- `Single click B0`: previous page
- `Single click B7`: next page
- `Long hold B0`: first page
- `Long hold B7`: last page
- `Double click Bn (n=0..7)`: reset that slot's currently mapped parameter to effect default

Notes:
- In SW1, preset actions are disabled by contract.
- If `totalPages == 1`, page actions are accepted but no page transition occurs.
- Click precedence on B0/B7:
  - single/long click = navigation
  - double click = slot reset (for that slot), never page navigation

### 6.1 Demo mode button/rotary semantics (recommended for immediate demo)

- ENC-B `SW0`:
  - Rotaries `0..7` control Row-2A (params `0..7`)
  - Buttons keep preset semantics:
    - single click: load slot
    - double click: save slot
    - long hold: delete slot
- ENC-B `SW1`:
  - Rotaries `0..7` control Row-2B (params `8..15`)
  - Buttons:
    - double click `Bn`: reset mapped Row-2B parameter to default
    - single/long clicks: no-op (demo-safe default)

## 7) Reset semantics

### 7.1 Slot reset (MVP)
- Trigger: `Double click Bn` in SW1
- Behaviour:
  1. Resolve mapped descriptor for slot `n`
  2. Send `effects.parameters.set` with `defaultValue`
  3. Optimistically update UI
  4. Reconcile on `effects.parameters.changed` or next `effects.parameters`

### 7.2 Out-of-scope reset (explicitly not in MVP)
- Reset-all-params for effect
- Reset-all-params for page

Reason: high accidental-destruction risk without explicit confirmation UX.

## 8) WS payload contract (Tab5 <-> LightwaveOS)

### 8.1 Metadata fetch on effect change
Request:
```json
{
  "type": "effects.parameters.get",
  "effectId": 6912,
  "requestId": "tab5.fxparams.meta.6912.42"
}
```

Response (success):
```json
{
  "type": "effects.parameters",
  "success": true,
  "requestId": "tab5.fxparams.meta.6912.42",
  "data": {
    "effectId": 6912,
    "name": "Time-Reversal Mirror",
    "hasParameters": true,
    "parameters": [
      {"name":"csq","displayName":"Wave Propagation","min":0.01,"max":0.4,"default":0.15,"value":0.15}
    ]
  }
}
```

### 8.2 Parameter update (single key per message)
Request:
```json
{
  "type": "effects.parameters.set",
  "effectId": 6912,
  "parameters": {
    "csq": 0.19
  },
  "requestId": "tab5.fxparams.set.6912.csq.981"
}
```

Ack:
```json
{
  "type": "effects.parameters.changed",
  "success": true,
  "data": {
    "effectId": 6912,
    "queued": ["csq"],
    "failed": []
  }
}
```

### 8.3 Error/recovery
- If key appears in `failed`, revert slot UI to last confirmed value.
- If WS `error` is returned, keep local value stale-marked and request full refresh with `effects.parameters.get`.

### 8.4 Firmware limits and validation contract
- `effects.parameters.get` is capped at `MAX_EFFECT_PARAMS = 16` in firmware WS handler.
  - Current largest effect (`Time-Reversal Mirror Mod3`) uses all 16 slots.
  - If a future effect exceeds 16, extra keys will not appear until firmware limit is raised.
- `effects.parameters.set` validates each key name against effect descriptors.
  - unknown keys are returned in `failed[]`
  - accepted keys are returned in `queued[]`
- `effectId` is optional in both get/set.
  - if omitted, firmware applies to current active effect

## 9) Rate and mapping rules

- Per-slot send throttle: >=50 ms
- Coalesce repeated turns on same slot to last value
- Mapping (encoder 0..255 to effect float range):
  - `value = min + (enc/255) * (max-min)`
- If descriptor `step > 0`, quantise before sending:
  - `value = round((value - min)/step)*step + min`
- If `max <= min`, treat key as byte-like fallback (`0..255`)

## 10) Parameterised effects inventory (current firmware)

These are the effects that currently expose custom parameter keys through `effects.parameters.get`.

Important distinction:
- Counts below are **effect-specific keys only**.
- They do **not** include global controls (`brightness`, `speed`, `paletteId`, etc.).
- Example: `LGP KdV Soliton Pair = 13` means 13 extra keys on top of globals, not "13 minus globals".

### 10.0 Summary table (effect -> key count -> pages at 8/pg)

| Effect | Keys | Pages |
|---|---:|---:|
| Beat Pulse (LGP Interference) | 2 | 1 |
| Kuramoto Transport | 2 | 1 |
| LGP RD Triangle | 3 | 1 |
| Fresnel Caustic Sweep | 3 | 1 |
| Gold Code Speckle | 3 | 1 |
| Quasicrystal Lattice | 3 | 1 |
| KdV Soliton Pair | 13 | 2 |
| Time-Reversal Mirror (Base) | 5 | 1 |
| Time-Reversal Mirror AR | 5 | 1 |
| Time-Reversal Mirror Mod1 | 10 | 2 |
| Time-Reversal Mirror Mod2 | 11 | 2 |
| Time-Reversal Mirror Mod3 | 16 | 2 |

## 10.1 Beat Pulse (LGP Interference)
Behaviour summary: dual-strip standing-wave interference with phase relationships.

- `phaseMode` (`0..2`, default `2`)
  - Meaning (explicit): strip phase relationship (`0=in-phase`, `1=quadrature`, `2=anti-phase`)
  - Visual characteristic: interference node/antinode structure and symmetry
- `spatialFreq` (`2..12`, default `4`)
  - Meaning (explicit): standing-wave box/node count
  - Visual characteristic: spatial density of interference bands

## 10.2 Kuramoto Transport
Behaviour summary: oscillator coherence field injects events into a transport buffer.

- `sync_ratio` (`0..1`, default `0.55`)
  - Meaning (explicit): coherence regime control (chaotic -> synced)
  - Visual characteristic: order/coherence of transported structures
- `radius` (`0..1`, default `0.50`)
  - Meaning (explicit): coupling radius mapped to neighbourhood size
  - Visual characteristic: local vs broad coupling influence, structure scale

## 10.3 LGP RD Triangle
Behaviour summary: reaction-diffusion front wedge extraction.

- `F` (`0.0300..0.0500`, default `0.0380`)
  - Meaning (inferred from Gray-Scott naming): feed term
  - Visual characteristic: growth/replenishment of pattern domains
- `K` (`0.0550..0.0750`, default `0.0630`)
  - Meaning (inferred): kill term
  - Visual characteristic: decay/erosion balance of patterns
- `melt` (`0.0010..0.0035`, default `0.0018`)
  - Meaning (inferred from label "Melt Glue"): diffusion/softening glue factor
  - Visual characteristic: edge softness and front coherence

## 10.4 Fresnel Caustic Sweep
Behaviour summary: Fresnel/Cornu caustic sweep.

- `lgpfresnel_caustic_sweep_effect_speed_scale` (`0.25..2.0`, default `1.0`)
  - Meaning (explicit label + group): timing scale
  - Visual characteristic: sweep velocity/temporal evolution
- `lgpfresnel_caustic_sweep_effect_output_gain` (`0.25..2.0`, default `1.0`)
  - Meaning (explicit): output gain
  - Visual characteristic: brightness/contrast intensity
- `lgpfresnel_caustic_sweep_effect_centre_bias` (`0.50..1.50`, default `1.0`)
  - Meaning (explicit): centre weighting
  - Visual characteristic: centre-vs-edge energy distribution

## 10.5 Gold Code Speckle
Behaviour summary: pseudo-random spread-spectrum speckle field.

- `lgpgold_code_speckle_effect_speed_scale` (`0.25..2.0`, default `1.0`)
  - Visual characteristic: temporal update rate
- `lgpgold_code_speckle_effect_output_gain` (`0.25..2.0`, default `1.0`)
  - Visual characteristic: speckle intensity/contrast
- `lgpgold_code_speckle_effect_centre_bias` (`0.50..1.50`, default `1.0`)
  - Visual characteristic: centre concentration bias

## 10.6 Quasicrystal Lattice
Behaviour summary: aperiodic lattice shimmer with rotational symmetry.

- `lgpquasicrystal_lattice_effect_speed_scale` (`0.25..2.0`, default `1.0`)
  - Visual characteristic: lattice drift/evolution speed
- `lgpquasicrystal_lattice_effect_output_gain` (`0.25..2.0`, default `1.0`)
  - Visual characteristic: lattice brightness/contrast
- `lgpquasicrystal_lattice_effect_centre_bias` (`0.50..1.50`, default `1.0`)
  - Visual characteristic: centre emphasis vs outer field

## 10.7 KdV Soliton Pair (13 params)
Behaviour summary: two soliton pulses with staged interaction and spark accents.

Wave-shape controls:
- `a1` (`0.10..3.0`, default `1.00`) -> amplitude of soliton 1, perceived pulse strength
- `a2` (`0.10..3.0`, default `0.55`) -> amplitude of soliton 2, secondary pulse strength
- `width1` (`0.05..1.0`, default `0.18`) -> soliton 1 width, sharp vs broad crest
- `width2` (`0.05..1.0`, default `0.23`) -> soliton 2 width, sharp vs broad crest

Transport/timing controls:
- `base_vel1` (`0.1..60.0`, default `14.0`) -> propagation speed of soliton 1
- `base_vel2` (`0.1..60.0`, default `8.0`) -> propagation speed of soliton 2
- `stage0_dur` (`0.2..30.0`, default `6.0`) -> stage-0 duration (inferred: early phase)
- `stage1_dur` (`0.2..30.0`, default `3.0`) -> stage-1 duration (inferred: interaction phase)
- `stage2_dur` (`0.2..30.0`, default `3.0`) -> stage-2 duration (inferred: late phase)

Spark/colour controls:
- `spark_gain` (`0.0..20.0`, default `6.0`) -> spark intensity around interaction events
- `spark_hue_shift` (`0..255`, default `20`) -> spark hue offset
- `strip2_hue_shift` (`0..255`, default `30`) -> strip-2 hue offset vs strip-1
- `strip2_bright` (`0..255`, default `217`) -> strip-2 brightness scaling

## 10.8 Time-Reversal Mirror (Base, 5 params)
Behaviour summary: forward wave recording then reverse playback.

- `csq` (`0.01..0.40`, default `0.15`) -> wave propagation term, spatial advance rate
- `damping` (`0.005..0.20`, default `0.04`) -> attenuation/loss
- `impulse_every` (`12..240`, default `90`) -> source injection period (frames)
- `forward_sec` (`1.0..20.0`, default `4.0`) -> forward phase length
- `reverse_sec` (`0.5..20.0`, default `2.5`) -> reverse phase length

## 10.9 Time-Reversal Mirror AR (5 params)
Behaviour summary: audio-reactive TRM variant.

- `csq` (`0.01..0.40`, default `0.15`) -> wave propagation term
- `base_damping` (`0.005..0.20`, default `0.04`) -> baseline attenuation
- `base_impulse_every` (`12..240`, default `90`) -> baseline impulse period
- `forward_sec` (`1.0..20.0`, default `4.0`) -> forward phase duration
- `reverse_sec` (`0.5..20.0`, default `2.5`) -> reverse phase duration

## 10.10 Time-Reversal Mirror Mod1 (10 params)
Behaviour summary: dispersive medium variant.

- `csq` (`0.01..0.40`, default `0.14`)
  - Meaning: wave propagation coefficient (`c^2` term)
  - Visual characteristic: outward travel speed / wavefront spacing
- `damping` (`0.005..0.20`, default `0.035`)
  - Meaning: per-step energy loss
  - Visual characteristic: tail length and persistence
- `edge_absorb` (`0.00..0.30`, default `0.09`)
  - Meaning: additional boundary absorption
  - Visual characteristic: edge reflection suppression and reduced edge flashing
- `impulse_every` (`16..240`, default `96`)
  - Meaning: centre impulse cadence
  - Visual characteristic: pulse/event density in forward phase
- `forward_sec` (`1.0..30.0`, default `6.0`)
  - Meaning: forward record phase duration
  - Visual characteristic: how long buildup runs before rewind
- `reverse_sec` (`0.5..30.0`, default `3.75`)
  - Meaning: reverse playback duration
  - Visual characteristic: rewind tempo (fast snapback vs slow unwind)
- `intro_sec` (`0.1..8.0`, default `1.6`)
  - Meaning: intro envelope duration
  - Visual characteristic: length of initial lead-in texture
- `intro_drive` (`0.0..0.30`, default `0.07`)
  - Meaning: intro drive amplitude
  - Visual characteristic: intro brightness/energy injection
- `normalise_follow_hz` (`0.5..20.0`, default `6.0`)
  - Meaning: output normaliser follower rate
  - Visual characteristic: responsiveness of brightness equalisation
- `peak_gamma` (`0.5..3.0`, default `1.35`)
  - Meaning: nonlinear peak sculpting
  - Visual characteristic: contrast of ridges/highlights vs mids

## 10.11 Time-Reversal Mirror Mod2 (11 params)
Behaviour summary: nonlinear medium variant.

- `csq` (`0.01..0.40`, default `0.14`) -> wave propagation coefficient; sets wavefront travel character
- `damping` (`0.005..0.20`, default `0.035`) -> attenuation; sets decay length
- `edge_absorb` (`0.00..0.30`, default `0.09`) -> boundary absorption; reduces edge ringing
- `impulse_every` (`16..240`, default `96`) -> impulse cadence; sets pulse frequency
- `forward_sec` (`1.0..30.0`, default `6.0`) -> forward duration; sets buildup length
- `reverse_sec` (`0.5..30.0`, default `3.75`) -> reverse duration; sets rewind speed
- `intro_sec` (`0.1..8.0`, default `1.6`) -> intro duration; sets opening phase length
- `intro_drive` (`0.0..0.30`, default `0.07`) -> intro drive; sets opening intensity
- `beat_release_sec` (`0.05..2.0`, default `0.42`)
  - Meaning: beat envelope release constant
  - Visual characteristic: how long beat energy lingers after impact
- `normalise_follow_hz` (`0.5..20.0`, default `6.0`) -> normaliser follow rate; sets gain adaptation speed
- `peak_gamma` (`0.5..3.0`, default `1.35`) -> peak curve shaping; sets highlight aggressiveness

## 10.12 Time-Reversal Mirror Mod3 (16 params)
Behaviour summary: lossy medium + ridge analyser controls.

- `csq` (`0.01..0.40`, default `0.14`) -> wave propagation coefficient; controls core travel dynamics
- `damping` (`0.005..0.20`, default `0.035`) -> attenuation; controls decay/persistence
- `edge_absorb` (`0.00..0.30`, default `0.09`) -> boundary absorption; controls edge calmness
- `impulse_every` (`16..240`, default `96`) -> impulse cadence; controls trigger density
- `forward_sec` (`1.0..30.0`, default `6.0`) -> forward duration; controls record window
- `reverse_sec` (`0.5..30.0`, default `3.75`) -> reverse duration; controls rewind pacing
- `intro_sec` (`0.1..8.0`, default `1.6`) -> intro duration; controls opening time
- `intro_drive` (`0.0..0.30`, default `0.07`) -> intro drive; controls opening energy
- `beat_release_sec` (`0.05..2.0`, default `0.42`) -> beat release time; controls beat-tail decay
- `normalise_follow_hz` (`0.5..20.0`, default `6.0`) -> normaliser speed; controls gain adaptation quickness
- `peak_gamma` (`0.5..3.0`, default `1.35`) -> peak shaping; controls highlight sharpness
- `ridge_attack_hz` (`0.5..60.0`, default `18.0`)
  - Meaning: ridge envelope attack speed
  - Visual characteristic: how quickly fang/ridge features lock in
- `ridge_release_hz` (`0.5..30.0`, default `4.5`)
  - Meaning: ridge envelope release speed
  - Visual characteristic: ridge persistence after local slope/curvature drops
- `ridge_sensitivity` (`0.2..10.0`, default `3.6`)
  - Meaning: ridge detector gain on slope/curvature terms
  - Visual characteristic: amount of ridge extraction/detail
- `ridge_blend` (`0.0..1.0`, default `0.56`)
  - Meaning: blend ratio between base sculpt and ridge sculpt
  - Visual characteristic: smooth flow vs fang-like ridge emphasis
- `ridge_floor` (`0.0..0.5`, default `0.05`)
  - Meaning: minimum ridge contribution bias
  - Visual characteristic: persistent baseline ridge visibility

## 10.13 Concrete slot maps for paged effects (`>8` keys)

### KdV Soliton Pair (13 keys)
- Page 1 (slots B0..B7):
  - `a1`, `a2`, `width1`, `width2`, `spark_gain`, `stage0_dur`, `stage1_dur`, `stage2_dur`
- Page 2 (slots B0..B7):
  - `base_vel1`, `base_vel2`, `spark_hue_shift`, `strip2_hue_shift`, `strip2_bright`, `-`, `-`, `-`

### Time-Reversal Mirror Mod1 (10 keys)
- Page 1 (slots B0..B7):
  - `csq`, `damping`, `edge_absorb`, `impulse_every`, `forward_sec`, `reverse_sec`, `intro_sec`, `intro_drive`
- Page 2 (slots B0..B7):
  - `normalise_follow_hz`, `peak_gamma`, `-`, `-`, `-`, `-`, `-`, `-`

### Time-Reversal Mirror Mod2 (11 keys)
- Page 1 (slots B0..B7):
  - `csq`, `damping`, `edge_absorb`, `impulse_every`, `forward_sec`, `reverse_sec`, `intro_sec`, `intro_drive`
- Page 2 (slots B0..B7):
  - `beat_release_sec`, `normalise_follow_hz`, `peak_gamma`, `-`, `-`, `-`, `-`, `-`

### Time-Reversal Mirror Mod3 (16 keys)
- Page 1 (slots B0..B7):
  - `csq`, `damping`, `edge_absorb`, `impulse_every`, `forward_sec`, `reverse_sec`, `intro_sec`, `intro_drive`
- Page 2 (slots B0..B7):
  - `beat_release_sec`, `normalise_follow_hz`, `peak_gamma`, `ridge_attack_hz`, `ridge_release_hz`, `ridge_sensitivity`, `ridge_blend`, `ridge_floor`

## 11) Capacity impact summary

- Unique effect-specific keys across current parameterised effects: **47**
- Global shared keys (separate API surface): **10**
  - `brightness`, `speed`, `paletteId`, `hue`, `intensity`, `saturation`, `complexity`, `variation`, `mood`, `fadeAmount`

- Effects with `>8` custom keys: 4
  - `LGP KdV Soliton Pair` (13)
  - `Time-Reversal Mirror Mod1` (10)
  - `Time-Reversal Mirror Mod2` (11)
  - `Time-Reversal Mirror Mod3` (16)

Implication: Row-2 paging is mandatory.

## 12) Notes on certainty

- Explicit: parameter names/display/ranges/defaults, and comments present in source.
- Inferred: some "visual characteristic" descriptions for keys without direct inline explanation (e.g. `F`, `K`, stage durations, ridge terms), derived from naming, group labels, and effect descriptions.
