RBDO label: GROUNDED

Implemented gates:
- Director mode required.
- Constrained switching required.
- Allowlisted target required.
- Confidence floor required.
- Candidate stability required.
- Boot/enable grace required.
- Dwell required.
- Cooldown required.
- Max two switches per 60000 ms window.
- Anti-thrash window.
- Manual/show owner suppression.
- Health clean gate.
- Transition-active suppression.
- Same-effect suppression.

Serial gate evidence:
- Parameter mode switching command rejected.
- Same-effect suppression captured after first 0x0407 switch.
- Low-confidence/silence suppression captured after playback stopped.
- Hard health counters stayed zero.
