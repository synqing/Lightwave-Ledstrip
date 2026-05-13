RBDO label: GROUNDED

Inputs used from existing runtime audio/control surface:
- RMS / fast RMS
- flux / fast flux
- silence / silentScale
- BPM / confidence
- beat strength / onset event
- saliency / liveliness
- ControlBusFrame audio confidence

Runtime telemetry examples:
- Drop run: `RMS=0.807 Flux=0.034 BPM=122.0 Conf=0.844`.
- Broadband run: `RMS=0.900 flux=0.569 BPM=93.0 confidence=1.000`.
- Restore run: `RMS=0.317 Flux=0.076 BPM=132.0 Conf=0.715`.

Missing external metadata:
- None used.
- No Spotify/cloud/app dependency used.
