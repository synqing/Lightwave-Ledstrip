RBDO label: GROUNDED

Selection resolver uses:
- current stable state
- confidence
- policy min confidence
- active effect
- allowlist target
- current/previous effect pair

Runtime scoring evidence:
- Drop decision reported `selectionScore=1.000`.
- Same-effect suppression after 0x0407 was active reported selectedEffect=0x0407 and `suppressed=same_effect`.
- Low-confidence/silence path reported selectedEffect=0x0202 and `suppressed=low_confidence`, with no switch.

Native tests cover:
- same-effect
- low-confidence
- dwell
- cooldown
- rate-limit
- anti-thrash
- manual/show owner
- health gate
