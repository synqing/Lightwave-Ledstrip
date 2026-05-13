# VP Render Path Layer Audit - 2026-05-05

Status: GROUNDED source audit. Documentation and visualisation only; no product visual default changed.

Update 2026-05-05: gamma LUT lifecycle/status work shipped in `adacee3d`. Subjective colour A/B was explicitly cancelled/skipped; do not use this audit to justify changing product visual defaults without a new validation protocol.

Companion visualiser: `firmware-v3/docs/audit/VP_RENDER_PATH_VISUALISER_2026-05-05.html`

Purpose: explain how the current visual pipeline changes the final LED output, identify which layers are creative/corrective/protective/transport/legacy/inactive, and turn the findings into a decision queue Captain can act on.

## 1. Operator Summary

The LED output is not the raw effect. Each frame has one lifecycle: the renderer starts frame N, an effect authors pixels into one of two buffer surfaces, the shared output stages prepare those pixels for the physical strips, the RMT driver emits the frame, the wire-time fence waits for physical output to finish, and only then can frame N+1 safely begin.

`frame tick -> effect authors pixels -> buffer ownership fork -> optional ColorCorrectionEngine on m_leds -> tone map on actual authored buffer -> split/copy decision -> m_strip1/m_strip2 actual output -> silence scaling -> EdgeMixer -> HAL transmit buffers -> FastLED correction/brightness/dither/power -> WS2812 RMT wire output -> wire-time fence -> next frame tick`

The recent "pure saturated colour" improvement is most plausibly explained by the S3 LED-driver wire-time fence, not dithering. `LedDriver_S3::show()` now waits after `FastLED.show()` because the patched RMT path can return before the WS2812 frame is physically complete (`firmware-v3/src/hal/esp32s3/LedDriver_S3.cpp:171-177`). That prevents the renderer from advancing while the hardware is still emitting the previous frame.

The biggest current visual-risk surface is `ColorCorrectionEngine`: it can alter saturation, white content, brightness, gamma, and warm colour temperature after an effect has rendered (`firmware-v3/src/effects/enhancement/ColorCorrectionEngine.cpp:181-217`). It may be helping the product look cleaner, but it is also the main candidate for unwanted haze, pastelisation, hue drift, and loss of authorial colour.

The biggest correctness bug was gamma LUT regeneration. The constructor built LUTs before NVS load and WebSocket config writes previously mutated gamma fields without routing through `setConfig()`. This was fixed in `adacee3d`; runtime status now exposes `gammaEnabled`, `gammaValue`, `lutGenerationId`, and LUT samples through REST, WebSocket, SerialCLI, and SerialJSON.

The biggest ordering concern is that colour correction targets the single 320-buffer authored frame (`m_leds`), while some effects can author the actual output directly into strip buffers (`m_strip1/m_strip2`). That is not a second end-to-end pipeline; it is a buffer-ownership fork. The specific risk is a colour-correction miss on strip-authored buffers (`firmware-v3/src/core/actors/RendererActor.cpp:931-947`, `:2017-2021`).

## 2. Layer Taxonomy

| Class | Meaning | Current examples | Captain-facing decision |
|---|---|---|---|
| Creative | Deliberately creates visual character. | Effect render, ZoneComposer blend modes, EdgeMixer non-MIRROR modes. | Keep only when the creative effect is wanted and visible. |
| Corrective | Alters output to solve an aesthetic/technical defect. | Colour correction, white guardrail, tone map, gamma. | Needs A/B proof; can improve or degrade. |
| Protective | Prevents corruption, crashes, or hardware invalid output. | TX-buffer copy, wire-time fence, show mutex, Core 1 assertion. | Do not remove without hardware evidence. |
| Transport | Moves pixels from CPU buffers to hardware. | Strip split, HAL `show()`, FastLED GRB/correction/brightness/dither. | Should be boring, deterministic, and measurable. |
| Control | Changes effect input parameters before pixels exist. | Audio mappings, auto-speed, `silentScale`. | Scope carefully so ambient effects do not pretend to be reactive. |
| Legacy / inactive | Implemented but not active in the live global path. | LayerStack, FramebufferLPF, FrameBlend, ColorEngine global use. | Do not blame for current visuals unless a call site is proven. |

## 3. Active Ordered Path

| Order | Layer | Class | Source | What it does to final output |
|---:|---|---|---|---|
| 1 | Frame tick / dispatch | Transport | `RendererActor::onTick()` calls `renderFrame()`, colour correction, then `showLeds()` (`RendererActor.cpp:897-963`). | Defines the fixed order. No direct colour change. |
| 2 | Transition override | Creative / control | Active transitions call `m_transitionEngine->update()` and skip normal render (`RendererActor.cpp:1438-1447`, `TransitionEngine.cpp:177-240`). | Blends source and target frames. Can change hue, brightness, contrast, timing, and geometry. |
| 3 | Audio context | Control | Snapshot copied into `m_sharedAudioCtx` (`RendererActor.cpp:1454-1663`). | Provides RMS, bands, chroma, silence, onset, saliency, and tempo to effects. |
| 4 | Audio mappings | Control | Mutates brightness/speed/intensity/saturation/complexity/variation/hue before render (`RendererActor.cpp:1813-1853`). | Can make the same effect brighter, faster, more saturated, or different in hue before pixels are written. |
| 5 | Auto-speed | Control | Optionally overwrites `ctx.speed` from liveliness and user trim (`RendererActor.cpp:1856-1877`). | Changes motion speed and temporal density, not colour directly. |
| 6 | Effect render | Creative | Single-effect context fields set at `RendererActor.cpp:1757-1781`; `effect->render(ctx)` at `RendererActor.cpp:1905`. | Primary authored colour, geometry, hue, brightness, contrast, and saturation. |
| 7 | ZoneComposer | Creative / corrective | Zone mode renders and composites zones (`ZoneComposer.cpp:218-270`, `:378-418`). | Per-zone brightness and blend modes can brighten, darken, flatten, muddy, or create composite colours. |
| 8 | Colour correction | Corrective | Runs unless bench-disabled or skipped by metadata (`RendererActor.cpp:931-947`, `PatternRegistry.cpp:437-460`). | Global post-effect colour/tone stack. Main aesthetic-risk layer. |
| 9 | Tone map | Corrective | Runs only for explicit additive effect IDs (`RendererActor.cpp:2013-2043`, `effect_ids.h:619-653`). | Compresses bright additive output to reduce white clipping. Can dim high-energy effects. |
| 10 | Buffer merge / split decision | Transport | `m_leds` is copied into strip buffers unless dual-channel mode has already authored them (`RendererActor.cpp:2045-2051`). | No intended colour change, but determines whether the actual output inherited `m_leds` correction or came from strip-authored buffers. |
| 11 | Silence gates | Control / corrective | Global `silentScale` and hard reactive gate combine in one pass (`RendererActor.cpp:2053-2119`, `effect_ids.h:608-617`). | Brightness-only scaling. Can make ambient output appear audio-reactive. |
| 12 | EdgeMixer | Creative / corrective | Runs after silence, before Tap C/show (`RendererActor.cpp:2121-2122`, `EdgeMixer.h:92-144`). | In non-MIRROR modes, changes strip 2 hue/saturation/depth; STM modes can affect both strips. |
| 13 | HAL transmit copy | Protective / transport | Waits before copying to TX buffers (`LedDriver_S3.cpp:131-144`). | Prevents buffer reuse while prior RMT output may still be shifting. |
| 14 | FastLED output state | Transport / corrective | Brightness, max power, dither, LED correction (`LedDriver_S3.cpp:89-97`, `:193-207`, `:242-265`). | Final global scale, colour correction, temporal quantisation, and power limiting. |
| 15 | Wire-time fence | Protective | `FastLED.show()` then `esp_rom_delay_us(kWireTimeUs)` (`LedDriver_S3.cpp:171-177`). | Prevents physical output corruption. Must keep until better RMT completion proof exists. |

## 4. Visual Impact Matrix

Scale: `0` no direct effect, `1` weak/indirect, `2` moderate, `3` strong/direct.

| Layer | Colour | Hue | Brightness | Temperature | Contrast | Saturation | Sharpness | Temporal stability | Latency |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Effect render | 3 | 3 | 3 | 3 | 3 | 3 | 3 | 2 | 0 |
| Audio mappings | 2 | 2 | 2 | 1 | 2 | 2 | 1 | 2 | 0 |
| Zone blend modes | 3 | 2 | 3 | 2 | 3 | 2 | 1 | 1 | 0 |
| Palette correction | 2 | 1 | 1 | 1 | 1 | 3 | 0 | 0 | 0 |
| Auto-exposure | 1 | 0 | 3 | 0 | 2 | 1 | 0 | 1 | 0 |
| V-clamp | 1 | 0 | 3 | 0 | 2 | 1 | 0 | 0 | 0 |
| Saturation boost | 3 | 1 | 1 | 1 | 1 | 3 | 0 | 0 | 0 |
| White guardrail | 3 | 2 | 2 | 1 | 2 | 3 | 0 | 0 | 0 |
| Brown guardrail | 3 | 2 | 2 | 3 | 2 | 3 | 0 | 0 | 0 |
| Gamma | 1 | 0 | 3 | 0 | 3 | 1 | 0 | 0 | 0 |
| Tone map | 1 | 0 | 3 | 0 | 2 | 1 | 0 | 0 | 0 |
| Silence gates | 0 | 0 | 3 | 0 | 1 | 0 | 0 | 2 | 0 |
| EdgeMixer | 3 | 3 | 2 | 2 | 2 | 3 | 1 | 2 | 0 |
| FastLED brightness/correction/dither | 2 | 1 | 3 | 2 | 1 | 1 | 0 | 2 | 0 |
| RMT wire-time fence | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 | 3 |

## 5. Colour-Correction Stack Deep Dive

| Stage | Source | Visible intent | Possible degradation | Current assessment |
|---|---|---|---|---|
| Palette correction | `correctPalette()` only for `PAL_WHITE_HEAVY`, called on palette set (`ColorCorrectionEngine.cpp:118-131`, `RendererActor.cpp:2332-2346`). | Reduce white-heavy palette entries before effects use them. | Double intervention if buffer white guardrail also runs; palette may lose intentional pale colours. | A/B only on palette-driven effects. |
| Auto-exposure | Samples every fourth LED and only darkens frames above target (`ColorCorrectionEngine.cpp:223-244`). Default disabled (`ColorCorrectionEngine.h:64-67`). | Prevent blown-out frames. | Can hide effect dynamics and make loud sections feel compressed. | Keep disabled unless explicitly testing. |
| V-clamp | Caps max channel if `maxBrightness < 255` (`ColorCorrectionEngine.cpp:322-340`). Default max is 255 (`ColorCorrectionEngine.h:77-80`). | Prevent channel clipping while preserving hue ratio. | With a low cap, output can feel flat or underpowered. | Mostly no-op by default. |
| Saturation boost | Adds fixed HSV saturation when `vClampEnabled` and amount > 0 (`ColorCorrectionEngine.cpp:199-204`, `:349-363`). Default amount 25. | Restore colour after correction and reduce washed-out pixels. | Can overcook already-saturated effects, quantise hue, and make subtle gradients look synthetic. | High-priority isolated A/B. |
| White guardrail | Detects high-min/low-spread whitish pixels and boosts saturation or subtracts white (`ColorCorrectionEngine.cpp:251-279`, `:370-375`). | Remove white haze and restore chroma. | Can destroy intentional white, silver, pastel, or bloom highlights. | Potentially useful, but should be skipped for precise colour-ramp effects. |
| Brown guardrail | Clamps G/B relative to R for `R > G >= B` (`ColorCorrectionEngine.cpp:286-301`, `:378-380`). Default disabled. | Stop muddy warm tones. | Can turn amber/gold/orange design into red/brown, reducing warmth. | Keep off unless a specific warm-tone defect is proven. |
| Gamma | Applies LUT to every channel (`ColorCorrectionEngine.cpp:308-315`). Default enabled. | Perceptual tone curve and mid-level contrast. | Stale LUT bug; can crush mid-level brightness or make visual judgement unreliable. | Fix correctness before judging visually. |

## 6. Degradation Pattern Catalogue

| Symptom Captain sees | Likely layer | Why it happens | First diagnostic |
|---|---|---|---|
| White haze over all effects | Colour correction, RMT timing, additive clipping | White guardrail/saturation/gamma can change whites; RMT corruption can inject invalid bright pixels; additive output can clip. | Confirm RMT fence present, then A/B `render.color_correction`. |
| Pure saturated colour suddenly appears | RMT fence, white guardrail, saturation boost | RMT fence removes corrupt white flashes/haze; correction layers may subtract white and boost saturation. | Compare current firmware against pre-fence commit visually if needed. |
| Ambient effect fades with music state | `silentScale` global gate | Silence scale is post-output brightness scaling, not an effect-local behaviour (`RendererActor.cpp:2053-2119`). | Test same effect with global silence bypass or forced active `silentScale`. |
| Top strip has different hue/colour | EdgeMixer or direct dual-channel effect | EdgeMixer modifies strip 2 in non-MIRROR modes; dual-channel effects can write strips separately. | Force EdgeMixer MIRROR and test again. |
| Effect looks washed/pastel | Additive blend, screen/lighten, white guardrail, gamma | Per-channel blends converge channels; correction can reduce white or alter saturation; gamma changes mids. | Use Tap A/B/C captures or colour-correction off/on. |
| Effect looks too dim | Gamma, tone map, silence gate, FastLED brightness/power | Multiple brightness scalars can stack after render. | Inspect brightness, `silentScale`, tone-map effect list, FastLED current brightness. |
| Warm colours become muddy or red | Brown guardrail, RGB white reduction, gamma | Warm channels are clamped or transformed after render. | Verify brown guardrail disabled and A/B correction mode. |
| Smooth gradients step or band | Saturation boost, gamma LUT, dither state, colour correction skip mismatch | HSV round-trip and LUTs can quantise; dithering affects low-level transitions. | Compare correction off and dither on/off with a gradient effect. |
| Zone mode feels unrelated to global controls | ZoneComposer defaults | ZoneComposer uses fixed intensity/saturation/complexity/variation defaults (`ZoneComposer.cpp:228-238`, `:320-330`). | Compare same effect in single-effect mode and zone mode. |

## 7. Ordering Rules

These are proposed rules for future VP work, derived from current source behaviour and visual-processing principles.

1. Author first: effect render must create the intended colour/geometry before any global correction.
2. Compose before correct: zone/layer composition should happen before global colour correction, otherwise each layer may be corrected differently and then blend into unexpected colours.
3. Correct the actual output buffer: if a frame is strip-buffer authored, post-processing only `m_leds` is not sufficient.
4. Protect before transport reuse: never copy into FastLED TX buffers while a previous WS2812 frame may still be shifting.
5. Split before edge-specific transforms: EdgeMixer belongs after strip split because it is explicitly a dual-edge transform.
6. Silence should be policy-scoped: global silence scaling should not automatically apply to every ambient/non-reactive effect unless product behaviour says so.
7. Hardware correction last: FastLED brightness/correction/dither/power are final output-stage decisions and should not be mixed into effect logic.
8. Inactive helpers stay inactive until a deliberate insertion point is chosen and validated.

## 8. Active vs Inactive Infrastructure

| Component | Active globally? | Evidence | Decision |
|---|---|---|---|
| `ColorCorrectionEngine` | Yes, for non-skipped effects | Called in `RendererActor::onTick()` (`RendererActor.cpp:931-947`). | Audit and A/B. |
| `EdgeMixer` | Yes, after silence gate | Called in `showLeds()` (`RendererActor.cpp:2121-2122`). | Keep MIRROR as known-clean baseline; test non-MIRROR separately. |
| `ZoneComposer` | Conditional | Renderer returns through zone path if enabled (`RendererActor.cpp:1727-1741`). | Align controls or document divergence. |
| `LayerStack` | No global call site found | Header describes sibling composer (`LayerStack.h:1-35`, `:131-177`). | Future candidate only. |
| `FramebufferLPF` | No global call site found | Header says renderer mandatory pass is later (`FramebufferLPF.h:65-66`). | Insert only after a dedicated plan. |
| `FrameBlend` | No production call site found in audit search | Described as whole-frame post-process by sub-audit. | Do not blame for current output. |
| `ColorEngine` | No global renderer call found | Palette blending/rotation/diffusion helpers exist (`ColorEngine.cpp:41-60`, `:93-117`, `:131-164`). | WS controls may be misleading if no effect uses it. |

## 9. Redundancy And Risk Register

| Candidate | Classification | Evidence | Recommendation |
|---|---|---|---|
| RMT wire-time fence | Protective, must keep | White-flash fix tied to `FastLED.show()` wait (`LedDriver_S3.cpp:171-177`). | Do not remove without hardware-completion proof. |
| TX-buffer copy | Protective, must keep for now | Copy waits until prior show start plus wire time (`LedDriver_S3.cpp:131-144`). | Keep until RMT ownership/lifetime is proven another way. |
| Dithering | Transport/corrective, keep default | Captain hardware check showed flashes persisted with dither off and disappeared with fence. | Do not blame for white flashes; leave controllable. |
| Colour correction stack | Corrective, high-risk | Six-stage post-render stack (`ColorCorrectionEngine.cpp:181-217`). | A/B off/on, then isolate saturation/gamma/white guardrail. |
| Saturation boost | Corrective, likely over-broad | Runs when `vClampEnabled`, even when max brightness means clamp is no-op (`ColorCorrectionEngine.cpp:199-204`, `:322-340`). | Test as independent knob. |
| Gamma | Corrective, correctness bug fixed | `adacee3d` routes NVS/runtime config writes through `setConfig()` and exposes LUT proof samples. | Keep status proof; do not make subjective gamma/default decisions from the cancelled A/B. |
| Tone map | Corrective, scoped | Explicit additive list (`effect_ids.h:619-653`). | Keep scoped; tune list if wrong. |
| Global silence scale | Control/corrective, policy risk | Applied after strip split to both strips (`RendererActor.cpp:2053-2119`). | Decide ambient/non-reactive policy. |
| EdgeMixer non-MIRROR | Creative/corrective | Can hue-shift/desaturate strip 2 (`EdgeMixer.h:92-144`). | Use MIRROR as baseline for all visual debugging. |
| ZoneComposer fixed controls | Legacy/control risk | Fixed defaults for intensity/saturation/etc. | Align with renderer controls or mark as separate surface. |

## 10. Decision Queue

Engineering correctness, no Captain visual judgement needed:

1. DONE in `adacee3d`: fix gamma LUT regeneration after NVS load and runtime config updates.
2. DONE in `adacee3d`: expose colour-correction/gamma status proof through REST, WS, SerialCLI, and SerialJSON.
3. OPEN/GATED: make the buffer-ownership correction behaviour explicit. Current source still corrects `m_leds` in `RendererActor::onTick()` while direct dual-channel effects can author `m_strip1/m_strip2`; changing this to correct strip buffers is a visible output behaviour change and needs explicit approval.

Captain visual judgement needed:

1. Should default output be "effect-authored colour first" with minimal post-processing, or "global product correction" with skip rules?
2. Should non-reactive/ambient effects ignore global `silentScale`?
3. Is saturation boost part of the desired K1 look, or was it compensating for pre-fence haze?
4. Which EdgeMixer modes are product-grade defaults versus experimental creative tools?
5. Should zone mode visually match global controls, or remain a separate curated composition mode?

Recommended order after the skipped visual A/B:

1. Treat `adacee3d` as the gamma/status correctness baseline.
2. Do not run the cancelled subjective colour A/B workflow.
3. If output defaults must change, create a new validation protocol first; do not revive the failed timed two-unit A/B script.
4. For code-side work, add diagnostics or tests first; only then consider visible behaviour changes.

## 11. Immediate Next Work

1. Keep `adacee3d` gamma/status fix as committed baseline.
2. Plan the colour-correction placement decision for strip-buffer authored output, but do not patch output behaviour without explicit approval.
3. Plan silence-policy metadata for ambient/non-reactive effects; this is also visible behaviour and should be implemented behind metadata/tests before default changes.
