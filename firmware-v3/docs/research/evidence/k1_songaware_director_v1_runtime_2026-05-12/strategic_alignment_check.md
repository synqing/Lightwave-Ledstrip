# Song-Aware Director V1 Strategic Alignment Check

RBDO label: GROUNDED

## Product Question

Can K1 autonomously change visual language in response to song structure, liveliness, section energy, and perceived musical intent, so it feels like it understands the song?

## Alignment Result

Aligned.

This implementation is not the rejected fixed-effect parameter-only lane. It adds Director mode with:

- Coarse song state classification from existing ControlBus fields.
- A fixed allowlist mapping song states to existing effect IDs and visual-language labels.
- Dwell, cooldown, rate, confidence, health, manual-owner, and show-owner gates.
- Runtime effect switching through the RendererActor control path before single-effect rendering, not inside pixel loops.
- Parameter overlays retained only as a support layer.

## Source Anchors

- State/effect policy allowlist: `firmware-v3/src/core/songaware/SongAwareDirector.cpp:36`.
- Director switch gate implementation: `firmware-v3/src/core/songaware/SongAwareDirector.cpp:198`.
- Coarse song-state classifier: `firmware-v3/src/core/songaware/SongAwareDirector.cpp:491`.
- Renderer integration before single-effect render: `firmware-v3/src/core/actors/RendererActor.cpp:1961`.
- Serial status/control surface: `firmware-v3/src/serial/SerialCLI.cpp:101`, `firmware-v3/src/serial/SerialCLI.cpp:524`.

## Runtime Proof

Controlled audio run produced an automatic visual-language switch with the corrected medium-candidate allowlist:

- `0x1302 K1 Waveform -> 0x0407 LGP Photonic Crystal`
- Reason: `drop_impact`
- State: `drop`
- Confidence: `1.000`
- `automaticEffectSwitches=1` after the corrected medium-candidate decision.

## Non-Claims

This evidence proves runtime Director support and bounded visual-language switching. It does not claim final product quality or Captain visual preference.
