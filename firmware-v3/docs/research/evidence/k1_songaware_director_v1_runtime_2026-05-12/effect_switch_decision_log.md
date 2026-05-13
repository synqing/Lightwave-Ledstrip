# Effect Switch Decision Log

RBDO label: GROUNDED

## Corrected Medium-candidate Decision

```text
time_ms=166233
state=drop
confidence=1.000
previousEffect=0x1302 K1 Waveform
targetEffect=0x0407 LGP Photonic Crystal
selectedFamily=advanced_optical
selectedVisualLanguage=photonic_drop_texture
reason=drop_impact
automaticEffectSwitches=1
health: show_skips=0 failures=0 rmt_errors=0 underruns=0
```

Serial log:

```text
[166232][INFO][Renderer] Effect changed: 0x1302 (K1 Waveform) -> 0x0407 (LGP Photonic Crystal)
[166233][INFO][Renderer] SongAware Director switch state=drop confidence=1.000 prev=0x1302 target=0x0407 family=advanced_optical language=photonic_drop_texture reason=drop_impact
```

## Safety Notes

- The corrected target was in the Director V1 allowlist.
- No automatic switch targeted an unregistered effect.
- No automatic switch occurred while `songaware` was off.
- Manual control suppression was observed as `owner=manual suppressed=manual_owner`.
- No hard health counters changed from zero.
