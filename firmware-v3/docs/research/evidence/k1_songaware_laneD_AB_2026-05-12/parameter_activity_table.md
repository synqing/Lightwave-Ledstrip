# K1 Song-Aware Lane D A/B Parameter Activity Table - 2026-05-12

## Summary

Parameter activity result: not observable.

The runtime exposes audio debug values and a manual merge layer, but it does not expose an autonomous song-aware parameter-only mode that can be enabled for Condition B.

## Activity Table

| Condition | Source of Parameter Change | Activation Path | Observed Changes/min | Evidence | Result |
| --- | --- | --- | ---: | --- | --- |
| A Baseline | Fixed controls only | Not run after B blocker | 0 | No playback; no polling run | Not measured |
| B Song-aware parameter mode | Required autonomous song-aware parameter adaptation | No available runtime activation path found | Not observable | `songAware` scan found no runtime surface; serial `merge` is manual; serial `A` is narrative auto-play | Blocked |

## Adjacent Runtime Controls Rejected As Condition B

| Control | Evidence | Why It Was Not Used As B |
| --- | --- | --- |
| Serial `merge <param> <value> [source]` | `firmware-v3/src/serial/SerialCLI.cpp:526-650` submits explicit parameter values to the merge layer | It is manual/external parameter injection, not autonomous song-aware parameter mode |
| Serial `A` auto-play | `firmware-v3/src/serial/SerialCLI.cpp:2437-2446` toggles narrative auto-play | It is narrative mode, not proven song-aware parameter-only adaptation and may affect behaviour beyond the requested B condition |
| REST audio mappings | `docs/protocol/k1-rest-contract.yaml:291-329` lists mapping endpoints | Host could not reach `192.168.4.1`; also no `songAware.mode` contract was found |
| WS EdgeMixer controls | `docs/protocol/k1-ws-contract.yaml:1038-1099` lists EdgeMixer set/get/save | EdgeMixer is not song-aware parameter mode; save path persists to NVS and was not used |

## Audio Feature Exposure

Precheck serial showed audio debug data was available:

| Field | Precheck Example |
| --- | --- |
| RMS | `0.000` |
| Flux | `0.098-0.114` |
| BPM | `48.0` |
| Confidence | `0.564-0.615` |

Audio feature exposure alone is insufficient for Condition B. The missing part is an existing runtime director/parameter-adaptation path that consumes those features under the requested mode.

