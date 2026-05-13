RBDO label: GROUNDED

Suppression evidence:

1. Parameter mode switching blocked:
- Command: `songaware switching on`
- Output: `songAware switching rejected: Director mode is required`.

2. Same-effect suppression:
- After 0x0407 became active, status reported:
- `suppressed=same_effect`
- `selectedEffect=0x0407`
- `automaticEffectSwitches=1`
- No duplicate same-effect switch counted.

3. Low-confidence/silence suppression:
- After playback stopped, status reported:
- `suppressed=low_confidence`
- No automatic switch occurred from the silence/low-confidence path.

4. Health not suppressing:
- Hard counters remained zero.
