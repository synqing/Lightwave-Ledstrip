# Song-Aware Parameter Mode V0 Visual Review Protocol

RBDO label: GROUNDED

Date: 2026-05-12  
Mode: CAPTAIN_VISUAL_REVIEW  
Purpose: close the missing optical/human visible-gain gate after serial/runtime A/B validation passed.

## Boundary honoured

- No firmware/source edits.
- No tuning.
- No implementation.
- No family morphing.
- No constrained switching.
- No automatic effect-ID switching.
- No NVS saves.
- No REST/WS dependency.
- USB serial control only.

## Fixed runtime controls

| Control | Value |
|---|---:|
| effect | `0x1302 K1 Waveform` |
| brightness | 160 |
| speed | 27 |
| intensity | 128 |
| saturation | 128 |
| complexity | 128 |
| variation | 0 |
| palette | `10 Vintage 01` |
| EdgeMixer | `MIRROR` |

## Actual review sequence

The written protocol initially listed A/B visual comparison runs. Captain interrupted the first baseline replay and correctly challenged why another `songaware off` run was being repeated after serial/runtime A already proved fixed/inert behaviour.

After that interruption, the visual gate was pivoted to the live product question: does Condition B, `songaware on` + `mode balanced`, create visible musical intent versus the already-known fixed baseline?

| Run | Track | Condition | Status |
|---|---|---|---|
| T1-A | ambient / low-energy intro-heavy | A, `songaware off` | Stopped early after about 62.7 s; serial remained fixed/inert and healthy. |
| T1-B | ambient / low-energy intro-heavy | B, `songaware on`, `mode balanced` | Full available track run; Captain rejected the visible result. |

## Track used

| Track role | File | Duration | Limitation |
|---|---|---:|---|
| ambient / low-energy intro-heavy | `/System/Library/PrivateFrameworks/Slideshows.framework/Versions/A/Resources/Content/Audio/Reflections.m4a` | 243.9 s | Local system slideshow audio. Trigger track was not identifiable from local recall/repo search in this session. |

## Video capture

No video file exists. AVFoundation listed cameras, including an iPhone Continuity camera, but bounded frame probes timed out before the review. The visual decision therefore depends on Captain's live human observation, not recorded video.

