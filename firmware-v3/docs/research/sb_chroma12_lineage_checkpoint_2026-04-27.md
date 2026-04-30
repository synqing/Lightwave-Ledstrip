# Sensory Bridge Chroma12 Lineage Checkpoint - 2026-04-27

## Scope

Read-only research checkpoint for the question: why Sensory Bridge and K1 expose `chroma[12]`, whether that was inherited, and whether K1 effects should use higher-resolution sources such as `bins64` or `bins256`.

SSAs deployed in parallel:

- `019dcd13-8110-7953-b25b-a28925b95807`: Sensory Bridge 4.0.0
- `019dcd13-d0d6-7e33-a7ff-ce3dce3ad57b`: Sensory Bridge 4.1.0
- `019dcd13-d2e7-7d20-ab2a-e2d65a49fc9e`: Sensory Bridge cross-version audit
- `019dcd13-d523-72e0-b614-014fb5f0cfd8`: current K1/LightwaveOS lineage audit

## Finding

`chroma[12]` is not a low-resolution replacement for the spectral analyser. In Sensory Bridge it is a pitch-class projection derived from a separate 64-bin semitone-resolved Goertzel spectrogram.

The important distinction is:

- `spectrogram_smooth[64]` / K1 `bins64[64]`: note or spectrum detail.
- `chroma[12]` / `chromagram_smooth[12]`: octave-folded pitch-class energy, intended for harmony and colour semantics.

## Sensory Bridge Evidence

Sensory Bridge 4.1.0 states that the analyser runs 64 Goertzel instances, one per selected musical frequency bin, rather than an FFT with linear bin spacing:

- `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.0/SENSORY_BRIDGE_FIRMWARE/GDFT.h:8-12`
- `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.0/SENSORY_BRIDGE_FIRMWARE/GDFT.h:27-34`
- `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.0/SENSORY_BRIDGE_FIRMWARE/GDFT.h:79-81`
- `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.0/SENSORY_BRIDGE_FIRMWARE/GDFT.h:197-199`

The 12-bin chromagram is then built by folding the smoothed 64-bin spectrogram modulo 12:

- `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.0/SENSORY_BRIDGE_FIRMWARE/led_utilities.h:1199-1213`
- `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.0/SENSORY_BRIDGE_FIRMWARE/led_utilities.h:1222-1233`

Sensory Bridge uses both layers in effects:

- 64-bin GDFT display: `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.0/SENSORY_BRIDGE_FIRMWARE/lightshow_modes.h:64-96`
- 12-bin chromagram gradient/dots/bloom: `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.0/SENSORY_BRIDGE_FIRMWARE/lightshow_modes.h:343-395`, `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.0/SENSORY_BRIDGE_FIRMWARE/lightshow_modes.h:451-461`

The cross-version SSA found the same shape back to Sensory Bridge 3.0.0: 64 frequency bins, 12 chroma bins, octave folding into 12 pitch classes. No inspected version used more than 12 chroma bins for lightshows. `CHROMAGRAM_RANGE` controls how many of the 64 spectrum bins are folded; it does not increase chroma width.

## K1 Evidence

K1 carries the inherited chroma model and also keeps the higher-resolution spectral surfaces.

Current contract fields:

- `CONTROLBUS_NUM_CHROMA = 12`: `firmware-v3/src/audio/contracts/ControlBus.h:11-12`
- `chroma[12]`, `heavy_chroma[12]`, `sb_note_chromagram[12]`, `sb_chromagram_smooth[12]`: `firmware-v3/src/audio/contracts/ControlBus.h:124-142`
- `bins64[64]`, `bins64Adaptive[64]`, `bins256[256]`, STM spectral data: `firmware-v3/src/audio/contracts/ControlBus.h:169-182`

Current ESv11 mapping:

- ES spectrogram to `bins64` and `bins64Adaptive`: `firmware-v3/src/audio/backends/esv11/EsV11Adapter.cpp:83-115`
- ES chromagram to `chroma[12]` and `heavy_chroma[12]`: `firmware-v3/src/audio/backends/esv11/EsV11Adapter.cpp:129-166`
- SB note chromagram derived by octave-folding `bins64Adaptive`: `firmware-v3/src/audio/backends/esv11/EsV11Adapter.cpp:233-257`

The vendored ESv11 Goertzel code mirrors the same chroma fold:

- `firmware-v3/src/audio/backends/esv11/vendor/goertzel.h:282-294`

Effect-facing accessors expose both chroma and `bins64`:

- `firmware-v3/src/plugins/api/EffectContext.h:245-256`
- `firmware-v3/src/plugins/api/EffectContext.h:312-335`

The reference documentation states the intended practical split: `bands[8]`/`chroma[12]` are common paths; `bins64[64]` is specialist; docs currently advise bands for backend-agnostic frequency access rather than direct `bins64`/`bins256`.

- `firmware-v3/docs/reference/audio-pipeline-parameters.md:116-139`
- `firmware-v3/docs/reference/audio-pipeline-parameters.md:159-164`
- `firmware-v3/docs/reference/audio-pipeline-parameters.md:232-253`
- `firmware-v3/docs/reference/audio-pipeline-parameters.md:347-352`

## Interpretation

The original Sensory Bridge reason for 12 chroma bins is musically coherent: 12 pitch classes per octave. It answers "which note family is energised, independent of octave?" It is useful for hue, harmonic identity, chord/root inference, and stable colour behaviour.

It is not enough as the only audio image for K1 effects. It loses octave/register, timbre, spectral envelope, and percussion detail. If an effect only uses chroma, it can become harmonically aware but spectrally flat.

## Recommendation

Do not replace `chroma[12]` with `bins64[64]` globally. Use a layered contract:

- `chroma[12]`: colour/harmony anchor, circular hue, pitch-class identity.
- `bands[8]`: backend-agnostic macro frequency energy and movement.
- `bins64[64]` / `bins64Adaptive[64]`: specialist note/spectrum texture, fine-grain spatial modulation, peak selection, octave/register-aware behaviour.
- `bins256[256]`: specialist FFT detail only when production backend support, latency, and effect semantics justify it.
- STM/saliency fields: movement, modulation, and perceptual feature control.

The product issue is not that `chroma[12]` exists. The issue is when effects collapse visual response to chroma alone instead of combining chroma with spectral or perceptual detail.

