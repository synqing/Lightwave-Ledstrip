---
abstract: "Visual frequency mapping for WLED Sound Reactive. Shows exact Hz ranges for all 16 GEQ channels at 22 kHz mode. Includes drum frequency zones and musical reference notes."
---

# WLED Frequency Mapping — Visual Reference

## 16-Channel GEQ Frequency Distribution (22 kHz Mode)

```
Frequency Range (Hz)    Channel    Bin Range    Musical Zone            Example Instruments
─────────────────────────────────────────────────────────────────────────────────────────
43 – 86 Hz              0          1–2          SUB-BASS / KICK         Kick drum (fundamental)
                                                Foundation              Bass guitar (low notes)
                                                                        Organ (lowest notes)

86 – 129 Hz             1          2–3          BASS FUNDAMENTAL        Kick drum (attack)
                                                Boom                    Bass guitar (standard)
                                                                        Cello (low range)

129 – 172 Hz            2          3–4          MID-BASS               Kick drum (sustain)
                                                                        Bass guitar (mid notes)

172 – 215 Hz            3          4–5          BASS WARMTH            Kick drum (release)
                                                Lower male vocals       Bass warmth zone

215 – 260 Hz            4          5–6          LOWER-MID              Toms (low)
                                                Body                    Male vocals (center)
                                                                        Cello

260 – 303 Hz            5          6–7          LOWER-MID              Toms (mid-low)
                                                Presence warmth         Guitar (low E string)

303 – 346 Hz            6          8–11         MID-RANGE              Snare body
                                                Presence                Tenor vocals
                                                                        Viola

346 – 389 Hz            7          15–20        MID-RANGE              Snare attack
                                                Presence+               Piano (low–mid)
                                                                        Accordion

389 – 432 Hz            8          20–27        UPPER-MID              Snare crack
                                                Presence++              Alto/Tenor vocals
                                                                        Clarinet

432 – 475 Hz            9          27–36        UPPER-MID              Snare (high attack)
                                                Power zone              Trumpet

475 – 518 Hz            10         36–48        UPPER-MID              Piccolo (low notes)
                                                                        Cymbals (body)

518 – 561 Hz            11         48–64        UPPER-MID / TREBLE     Cymbals (initial)
                                                Sibilance               Hi-hat
                                                                        Ride cymbal

561 – 604 Hz            12         64–85        TREBLE                 Hi-hat (attack)
                                                Bright presence         Cymbals (sizzle)

604 – 647 Hz            13         85–114       TREBLE                 Hi-hat (sustain)
                                                High presence           Cymbal crash

647 – 690 Hz            14         114–151      TREBLE (HIGH)          Hi-hat (decay)
                                                Brilliance              Crash cymbal

7,106 – 9,259 Hz        15         165–215×0.7 HIGH TREBLE            Cymbals (air)
                                                Air / Shimmer           Sizzle
                                                                        Breath noise
```

---

## Drum Frequency Zones

**Kick Drum**
```
┌─────────────────────────────┐
│ Fundamental: 40–60 Hz       │ GEQ Channel 0 (sub-bass)
│ Body: 100–150 Hz            │ GEQ Channel 1–2 (bass)
│ Beater click: 300–500 Hz    │ GEQ Channels 5–8 (presence)
└─────────────────────────────┘
```

**Snare Drum**
```
┌─────────────────────────────┐
│ Body: 100–500 Hz            │ GEQ Channels 4–8 (lower to upper-mid)
│ Crack: 2–5 kHz              │ Above GEQ range (use full FFT for exact)
│ Sizzle: 5–10 kHz            │ GEQ Channel 15 (high treble)
└─────────────────────────────┘
```

**Hi-Hat (Closed)**
```
┌─────────────────────────────┐
│ Attack: 500–2,000 Hz        │ GEQ Channels 8–12 (upper-mid to treble)
│ Sustain: 3–8 kHz            │ GEQ Channel 15 (high treble)
│ Decay: Broadband 500–10kHz  │ GEQ Channels 8–15 all active
└─────────────────────────────┘
```

**Hi-Hat (Open)**
```
┌─────────────────────────────┐
│ Attack: 100–500 Hz          │ GEQ Channels 4–8 (for body)
│ Ring: 1–5 kHz               │ GEQ Channels 12–15 (treble)
│ Sustain: Broadband 100–10kHz│ Most channels active
└─────────────────────────────┘
```

**Cymbals (Crash/Ride)**
```
┌─────────────────────────────┐
│ Body: 400–1,000 Hz          │ GEQ Channels 7–11 (presence)
│ Sizzle: 5–15 kHz            │ GEQ Channel 15 (high treble, extended)
│ Air: 10–20 kHz              │ Beyond Nyquist at 22 kHz (loss)
└─────────────────────────────┘
```

---

## Musical Reference (Piano Middle C = 261.6 Hz)

```
Note    Frequency (Hz)    GEQ Channel(s)    Comment
─────────────────────────────────────────────────
C1      32.7             Below range (mic roll-off)
C2      65.4             0–1               Lower limit for WLED
C3      130.8            1–2               Bass notes (guitar open D, E strings)
C4      261.6            4–6               Middle C (piano reference)
C5      523.3            9–11              Soprano voice
C6      1046.5           14–15             Treble (high notes)
C7      2093.0           15 (partially)    Very high
C8      4186.0           At Nyquist edge   Extreme high (5 kHz limit at 22 kHz)
```

---

## Peak Detection Bin Selection (WLED `binNum` Parameter)

When setting `binNum` for beat detection, you're selecting ONE of the 256 FFT bins to monitor:

```
binNum  Frequency (Hz)   Use Case
─────────────────────────────────────
0       0 (DC)           ✗ Avoid (noise)
1–4     43–172 Hz        ✗ Too low (sub-bass chatter)
5–7     215–346 Hz       ✓ Bass/kick (good for hip-hop, electronic)
8–10    346–475 Hz       ✓ Mid-bass/snare (general music)
11–13   518–604 Hz       ✓ Upper-mid/presence (dance, pop)
14      647 Hz           ✓ Bright treble (more sensitive, noisier)
15+     >7 kHz           ✗ Too high (cymbal noise, chatter)

DEFAULT: binNum = 8 (~350 Hz, catches both kick and snare)
```

---

## AGC Response Visualization

### "Normal" Preset (Balanced)

```
Input Signal Envelope        After AGC Gain
(Random dynamics)            (Smoothed to target)

┌─────────┐                  ┌──────┐
│ LOUD    │──────┐           │ 220  │ agcTarget1 (mid)
│         │      │           │      │
│ NORMAL  │──────┼──────────→├──────┤  (PI controller adjusts gain
│         │      │           │      │   to keep output here)
│ QUIET   │──────┘           │ 112  │ agcTarget0 (low)
│         │                  │      │
└─────────┘                  └──────┘
```

**Attack:** 192 ms (slow rise, protects from overload)
**Decay:** 6144 ms (9 seconds, natural-sounding release)
**Max gain:** 32× (+30 dB), absolute limit

### "Vivid" Preset (More Responsive)

```
Attack: 128 ms (faster transient capture)
Decay: 4096 ms (quicker volume recovery)
Max gain: Higher (448 setpoint vs 336 in Normal)
→ More "pop" on hits, more chop in quiet passages
```

### "Lazy" Preset (Smooth & Stable)

```
Attack: 256 ms (gentle onset)
Decay: 8192 ms (11+ seconds, very smooth)
Max gain: Conservative (304 setpoint, pulls back)
→ Smooth, but misses quick dynamics
```

---

## Squelch (Noise Gate) Behavior

```
Input Signal Level     Gate State    FFT Output
─────────────────────────────────────────────
> 10 (default          OPEN          Full FFT output
  soundSquelch)
                      ┌─────────┐    Updated
├─ 10 ─┤              │ OPEN    │
                      │         │
< 10                  │         │    Decay 85% per cycle
                      │ CLOSED  │    (smooth fade to silence)
                      └─────────┘

Default: soundSquelch = 10 (0–255 range)
Effect: Removes noise from silence, prevents beat
        detection false positives in quiet gaps
```

---

## Pink Noise Correction Curve

```
Frequency (Hz)    GEQ Channel    Correction Gain    Effect
─────────────────────────────────────────────────────────
43–86             0              1.70×              Boost low bass
86–129            1              1.71×
129–172           2              1.73×
172–215           3              1.78×
215–260           4              1.82×
260–303           5              2.10× ◄─ Max at low-mid
303–346           6              2.35×
346–389           7              3.00×              Heavy presence
389–432           8              3.93×
432–475           9              5.12×
475–518           10             6.70×              Extreme treble
518–561           11             8.37×              boost
561–604           12             10.00×◄─ Peak (max)
604–647           13             11.22×
647–690           14             11.90×
7106–9259         15             9.55× ◄─ Rollback (prevent noise)
```

**Why this shape?**
- Microphone has natural low-frequency roll-off → boost low end (1.7–2.1×)
- Microphone peaks at presence zone → boost middle heavily (3–11×)
- Microphone susceptible to noise at very high freq → slight rollback (9.55× vs potential 15×)
- Result: Perceptually flat frequency response despite physical losses

---

## Choosing the Right AGC Preset

```
Music Genre          Best Preset    Reason
──────────────────────────────────────────────
Electronic/EDM       VIVID          Fast, snappy transients
Dance/House          VIVID          Kick definition
Hip-Hop/Trap         NORMAL         Balanced kick+snare+hihat
Pop/Vocal            NORMAL         Natural dynamics
Jazz/Acoustic        LAZY           Smooth response to live playing
Classical/Ambient    LAZY           Avoid "pumping" on dynamics
```

---

## Common Mistakes in WLED Setup

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Squelch too high | Beat detector never triggers in quiet music | Lower `soundSquelch` (try 5–8) |
| Squelch too low | Constant beat pulses during silence | Raise `soundSquelch` (try 12–15) |
| binNum too low (< 5) | Constant beat pulses from sub-bass | Use binNum 8–10 for stable beat |
| binNum too high (> 14) | No beat triggers, cymbal noise sensitivity | Use binNum 8–10 for stable beat |
| maxVol too high | Beat detector never triggers | Lower `maxVol` (try 20–25) |
| maxVol too low | Beat pulses on background noise | Raise `maxVol` (try 40–50) |
| AGC off | Quiet songs inaudible, loud songs overdriven | Enable NORMAL preset |
| Decay time too long | Visual lag, "stale" reactivity | Reduce `decayTime` (try 800–1000 ms) |

---

## Real-World Examples

### Pop/Rock Song (120 BPM, Drums + Bass + Vocals)
```
Kick drum: 50 Hz (GEQ 0)
Snare: 250 Hz (GEQ 5–6)
Hi-hat: 8 kHz (GEQ 15)
Bass guitar: 100 Hz (GEQ 1)
Vocals: 200–2000 Hz (GEQ 5–12)

Recommended setup:
  AGC: NORMAL
  binNum: 8 (catches both kick + snare attack)
  maxVol: 35
  soundSquelch: 10
  Decay: 1000 ms (slightly snappier than default)
```

### Electronic/EDM (128 BPM, Synth Bass + Kick + Sidechain)
```
Kick: 40 Hz (GEQ 0, very sub)
Bass synth: 150–300 Hz (GEQ 3–5)
Snare: 400 Hz (GEQ 7–8)
Cymbals: 5–10 kHz (GEQ 15)
Hi-hat: Sidechain pump → broad spectrum

Recommended setup:
  AGC: VIVID (faster response to sidechain)
  binNum: 6 (catches kick + low-mid synth)
  maxVol: 30 (more sensitive, synths are loud)
  soundSquelch: 8 (lower gate for smooth response)
  Decay: 600 ms (snappier for dance music)
```

### Jazz/Acoustic (95 BPM, Live Band)
```
Double bass: 40–100 Hz (GEQ 0–1)
Piano: 40–3000 Hz (GEQ 0–12)
Drums (brushes): Broadband, lower energy
Cymbals: 5–15 kHz (GEQ 15)

Recommended setup:
  AGC: LAZY (preserve natural dynamics)
  binNum: 10 (higher-mid, avoids rumble)
  maxVol: 40 (live band quieter, more selective)
  soundSquelch: 12 (higher gate to avoid noise)
  Decay: 1400+ ms (preserve sustain, avoid pumping)
```

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:research | Created visual frequency mapping reference for WLED 16-channel GEQ |
