RBDO label: GROUNDED

Thrash controls:
- cooldown: 20000 ms.
- max switches: 2 per 60000 ms.
- anti-thrash: 45000 ms.
- same-effect switches blocked.
- transition-active switches blocked.

Runtime:
- 0x0407 -> 0x1B01 at 328252 ms.
- 0x1B01 -> 0x0204 at 350270 ms.
- Debug after second window showed `switchesInWindow=2 maxSwitchesPerWindow=2`.
- No unbounded cycling or random preset roulette observed.

Native tests:
- rate-limit third switch block passed.
- A->B->A anti-thrash block passed.
