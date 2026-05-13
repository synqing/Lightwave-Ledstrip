RBDO label: GROUNDED

Precedence:
- show/cue owner
- manual owner
- director owner
- none

Implemented:
- Renderer manual control messages mark manual hold.
- ShowDirector playback marks show hold.
- Director acts only when enabled and unsuppressed.

Test evidence:
- `test_song_aware_manual_and_show_owners_suppress_director_switches` passed.

Runtime suppression proof:
- `suppressed=same_effect` captured.
- `suppressed=low_confidence` captured.

No show playback was active in runtime smoke:
- `ShowDirector: Has show: NO`.
