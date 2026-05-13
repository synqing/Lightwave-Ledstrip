# NVS No-save Check

RBDO label: GROUNDED

## Source Check

Command:

```text
rg -n "saveToNVS|SAVE_.*NVS|Preferences|put" firmware-v3/src/core/songaware firmware-v3/src/serial/SerialCLI.cpp firmware-v3/src/core/actors/RendererActor.cpp firmware-v3/src/core/actors/ShowDirectorActor.cpp
```

Result summary:

- No `saveToNVS`, `Preferences`, or NVS write route exists in `firmware-v3/src/core/songaware`.
- Existing unrelated NVS routes remain in SerialCLI/RendererActor for EdgeMixer, colour correction, and zone config.
- This run did not invoke any save command such as `saveEdgeMixer`, `Csave`, `}` or zone save.

## Runtime Control Check

The run used serial runtime controls only:

- Serial text commands: `songaware status`, `songaware reset`, `songaware mode director`, `songaware switching on`, `songaware off`.
- Serial JSON runtime commands: `setEffect`, `setBrightness`, `setSpeed`, `setIntensity`, `setSaturation`, `setComplexity`, `setVariation`, `setPalette`, `setEdgeMixer`.

No NVS save command was used.

## Decision

PASS: no Song-Aware NVS save route was added, and no NVS save route was called during validation.
