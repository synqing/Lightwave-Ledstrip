 Current 64-bin layout in the 16 kHz Nyquist LUT
 
  Spacing — uniform logarithmic (one semitone per bin)

  NOTE_STEP = 2 in a quarter-tone LUT = exactly 1 semitone between adjacent bins. So:

  - f(bin i) = 65.40639 × 2^(i/12) Hz
  - Each adjacent bin is 2^(1/12) ≈ 1.05946× the previous bin's frequency
  - In log-frequency space: uniform (constant Δlog₂ = 1/12 octave per bin)
  - In linear Hz space: highly non-uniform — Δf grows exponentially with bin index

  ┌────────────────────────────────┬──────────────────────────┐
  │             Region             │ Δf between adjacent bins │
  ├────────────────────────────────┼──────────────────────────┤
  │ Low bass (bins 0–11, C2..B2)   │ 3.9–7.3 Hz               │
  ├────────────────────────────────┼──────────────────────────┤
  │ Mid (bins 24–35, C4..B4)       │ 15.6–29.4 Hz             │
  ├────────────────────────────────┼──────────────────────────┤
  │ Upper mid (bins 48–59, C6..B6) │ 62.3–117.5 Hz            │
  ├────────────────────────────────┼──────────────────────────┤
  │ Top (bin 62→63)                │ ~152 Hz                  │
  └────────────────────────────────┴──────────────────────────┘

  Position within 0–16 kHz Nyquist range

      0 Hz                                                              16 kHz
      │                                                                    │
      │     [used: 65.41 Hz — 2489.02 Hz, 5.25 octaves, 64 bins]           │
      │     ████████████████████                                           │
      │     ▲                  ▲                                           │
      │     bin 0 (C2)         bin 63 (D#7)                                │
      │                                                                    │
      │  ◄─── 65 Hz unused ──► ◄──── 13,511 Hz unused (2.68 octaves) ────► │

  ┌────────────────────────────────┬─────────────────┬──────────────────────────┬────────────────────────────────┐
  │              Axis              │      Used       │       Unused below       │          Unused above          │
  ├────────────────────────────────┼─────────────────┼──────────────────────────┼────────────────────────────────┤
  │ Linear Hz (out of 16,000 Hz)   │ 2,423.6 Hz      │ 65.4 Hz (0.4%)           │ 13,511 Hz (84.4%)              │
  │                                │ (15.1%)         │                          │                                │
  ├────────────────────────────────┼─────────────────┼──────────────────────────┼────────────────────────────────┤
  │ Log octaves (out of 8.21 below │ 5.25 oct        │ 0.25 oct (3.0%)          │ 2.68 oct (32.6%)               │
  │  Nyquist)                      │ (64.0%)         │                          │                                │
  ├────────────────────────────────┼─────────────────┼──────────────────────────┼────────────────────────────────┤
  │ LUT entries (out of 198)       │ 64 entries      │ 6 entries (indices 0–5:  │ 65 entries (indices 133–197:   │
  │                                │ (32.3%)         │ A1..B1)                  │ E7..F#10)                      │
  └────────────────────────────────┴─────────────────┴──────────────────────────┴────────────────────────────────┘

  Octave layout (bin → pitch)

  ┌───────┬───────────────────┬───────────────┬────────────┬─────────────────────────────────────────────────────┐
  │ Bins  │       Range       │ Pitch class   │   Δ Hz     │                        Notes                        │
  │       │                   │     span      │  spanned   │                                                     │
  ├───────┼───────────────────┼───────────────┼────────────┼─────────────────────────────────────────────────────┤
  │ 0–11  │ 65.41 – 123.47 Hz │ C2 – B2       │ 58.1 Hz    │ Sub-bass / kick fundamental                         │
  ├───────┼───────────────────┼───────────────┼────────────┼─────────────────────────────────────────────────────┤
  │ 12–23 │ 130.81 – 246.94   │ C3 – B3       │ 116.1 Hz   │ Bass / low vocals                                   │
  │       │ Hz                │               │            │                                                     │
  ├───────┼───────────────────┼───────────────┼────────────┼─────────────────────────────────────────────────────┤
  │ 24–35 │ 261.63 – 493.88   │ C4 – B4       │ 232.3 Hz   │ Middle C / vocal core                               │
  │       │ Hz                │               │            │                                                     │
  ├───────┼───────────────────┼───────────────┼────────────┼─────────────────────────────────────────────────────┤
  │ 36–47 │ 523.25 – 987.77   │ C5 – B5       │ 464.5 Hz   │ Upper vocals / lead                                 │
  │       │ Hz                │               │            │                                                     │
  ├───────┼───────────────────┼───────────────┼────────────┼─────────────────────────────────────────────────────┤
  │ 48–59 │ 1046.50 – 1975.53 │ C6 – B6       │ 929.0 Hz   │ Presence / brilliance                               │
  │       │  Hz               │               │            │                                                     │
  ├───────┼───────────────────┼───────────────┼────────────┼─────────────────────────────────────────────────────┤
  │ 60–63 │ 2093.00 – 2489.02 │ C7 – D#7      │ 396.0 Hz   │ Spectrum-only (excluded from chroma fold; partial   │
  │       │  Hz               │               │            │ 5th octave)                                         │
  └───────┴───────────────────┴───────────────┴────────────┴─────────────────────────────────────────────────────┘

  Total covered: A 12-TET semitone grid spanning C2 → D#7, with the chroma fold consuming the clean 5-octave block
  C2..B6 (60 bins) and the residual 4 bins (C7, C#7, D7, D#7) feeding only the spectrum/HF semantic writers.

  Headroom relative to Nyquist

  What sits in the LUT but is currently unused:

  - Below bin 0 (LUT indices 0–5): A1 (55 Hz), A#1, B1, plus 3 quarter-tone fractional steps. ~10 Hz of Hz space, ¼
  octave. Sub-bass fundamental territory; current floor at C2 already excludes A1/A#1/B1.
  - Above bin 63 (LUT indices 133–197): 65 unused quarter-tones = 32 unused semitones = 2.68 octaves of usable lattice
   headroom up to ~16 kHz Nyquist if we ever wanted true HF coverage without re-spacing. The LUT's last entry below
  Nyquist is notes[196] = 15804.26 Hz; notes[197] = 16274 Hz is above Nyquist and not safely usable at 32 kHz sample
  rate.

  - Below bin 0 (LUT indices 0–5): A1 (55 Hz), A#1, B1, plus 3 quarter-tone fractional steps. ~10 Hz of Hz space, ¼ octave. Sub-bass fundamental territory; current floor at C2 already excludes A1/A#1/B1.
  - Above bin 63 (LUT indices 133–197): 65 unused quarter-tones = 32 unused semitones = 2.68 octaves of usable lattice headroom up to ~16 kHz Nyquist if we ever wanted true HF coverage without re-spacing. The LUT's
  last entry below Nyquist is notes[196] = 15804.26 Hz; notes[197] = 16274 Hz is above Nyquist and not safely usable at 32 kHz sample rate.
  - Concrete extension envelope (informational, no proposal):
    - Bin 64 would target notes[134] = 2637.02 Hz = E7
    - Bin 95 would target notes[196] = 15804.26 Hz (≈ E10 region, far above any musical instrument fundamental)
    - So at the current semitone spacing we could add up to ~32 more bins before hitting the LUT/Nyquist ceiling

  Where the audit's HF fields actually sit on this layout

  Per the audit §6.2 (with post-fix bin frequencies recomputed):

  ┌────────────────────┬──────────────────────────┬──────────────────────┬──────────────┐
  │       Field        │        Bin range         │   Frequency range    │ Octave label │
  ├────────────────────┼──────────────────────────┼──────────────────────┼──────────────┤
  │ hihatEnergy        │ 50..60                   │ 1174.66 – 2093.00 Hz │ D6 – C7      │
  ├────────────────────┼──────────────────────────┼──────────────────────┼──────────────┤
  │ hfEnergy / hfFlux  │ 50..63                   │ 1174.66 – 2489.02 Hz │ D6 – D#7     │
  ├────────────────────┼──────────────────────────┼──────────────────────┼──────────────┤
  │ airEnergy          │ 58..63                   │ 1864.66 – 2489.02 Hz │ A#6 – D#7    │
  ├────────────────────┼──────────────────────────┼──────────────────────┼──────────────┤
  │ cymbalSustain      │ weighted 50..63 + 58..63 │ ~1.2–2.5 kHz         │ D6 – D#7     │
  ├────────────────────┼──────────────────────────┼──────────────────────┼──────────────┤
  │ spectralBrightness │ 0..63 (centroid)         │ 65.41 – 2489.02 Hz   │ full lattice │
  └────────────────────┴──────────────────────────┴──────────────────────┴──────────────┘

  (Numbers shifted slightly post-fix because the lattice moved down by 3 semitones — the F6–F#7 range pre-fix is now D6–D#7 post-fix. Mismatch direction unchanged: comments still claim 6–12 kHz, actual tops out at
  ~2.5 kHz.)

  The 6–12 kHz region the comments and field names imply lives in the unused upper 2.68 octaves of the LUT, not in any currently-instantiated detector. To reach it at the current semitone spacing would need ~32
  additional bins targeting notes[156..188] (≈5.6–11.8 kHz). Per audit §6.3, this is what bins256[] was supposed to capture (FFT, 0–16 kHz at 62.5 Hz spacing) but no writer populates it on the canonical path.