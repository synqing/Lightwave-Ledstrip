---
abstract: "ASCII wiring diagram and bring-up checklist for connecting the WeActStudio I2S Speaker Module V1 (PCM5102A) to the M5Stack Basic v2.7 left M5-Bus column (rows 10/11/12 = G2/G12/G15 → BCK/DIN/WS), matching the module's top-to-bottom pad order. Includes signal direction, recommended jumper length, and decoupling guidance."
---

# Wiring diagram — WeAct I2S Speaker V1 on M5Stack Basic v2.7

## Bus pin map

```
              Left M5-Bus column                 Right M5-Bus column
              +------------------+              +-------------------+
   row  1     |       GND        |              |        G35        |
   row  2     |       GND        |              |        G36        |
   row  3     |       GND        |              |        RST        |
   row  4     |       G23        |              |        G25        |
   row  5     |       G19        |              |        G26        |
   row  6     |       G18        |              |        3V3        |
   row  7     |       G3         |              |        G1         |
   row  8     |       G16        |              |        G17        |
   row  9     |       G21        |              |        G22        |
   row 10     |     [ G2 ]------ BCK            |        G5         |
   row 11     |     [ G12 ]----- DIN            |        G13        |
   row 12     |     [ G15 ]----- WS             |        G0         |
   row 13     |       HPWR       |              |        G34        |
   row 14     |       HPWR       |              |     [ 5V ]------- VIN
   row 15     |       HPWR       |              |        BAT        |
              +------------------+              +-------------------+
```

Three I2S signals run straight down the left column. Power comes from the right column row 14 (5 V); ground comes from any of rows 1-3 on the left column.

## Signal map

```
M5Stack Basic v2.7                     WeAct I2S Speaker V1 (PCM5102A)
+------------------------+             +--------------------+
| Left bus row 10  G2  --|---- yellow -|  BCK  (top pad)    |
| Left bus row 11  G12 --|---- green --|  DIN  (middle pad) |
| Left bus row 12  G15 --|---- white --|  WS   (bottom pad) |
| Left bus row 1-3 GND --|---- black --|  GND               |
| Right bus row 14  5V --|---- red ----|  VIN               |
|                        |             |  MC   (no connect) |
|                        |             |  SD   (no connect) |
+------------------------+             +--------------------+
                                                  | +SPK | -SPK |
                                                  +-----+-------+
                                                       |    |
                                                     4-8 Ω speaker
                                                       (≤3 W)
```

Five logical wires total. The three signal wires run as a 1x3 ribbon onto adjacent header pins.

## Cable choices

- The cleanest mechanical option is a **1x3 female-to-female ribbon** soldered directly to the module's `BCK`, `WS`, `DIN` pads, plugged onto bus rows 10/11/12.
- Keep all jumpers under **150 mm**. BCK at 44.1 kHz × 32 bits ≈ 1.41 MHz square wave — long unshielded runs can radiate.
- Twist the GND return alongside the BCK lead if you see jitter on a scope.

## Power notes

- The PCM5102A core runs at 3.3 V via an on-board LDO. The module accepts 3.3 V – 5 V on `VIN`; use 5 V from the M5-Bus right-column row 14 for best supply margin.
- Add a 100 µF electrolytic + 0.1 µF ceramic across VIN/GND **at the module** if you hear motorboating on transients.

## Speaker output

- The WeAct module includes a small Class-D amp after the PCM5102A. Both speaker terminals are differential — neither is ground.
- Use a 4-8 Ω speaker rated for at least 3 W.
- Do not tie either speaker terminal to GND or to a scope-probe ground. Use a differential probe or a 10 kΩ / 10 kΩ attenuator if you want to look at the output.

## First power-up checklist

1. **Module unplugged.** Power the M5Stack Basic via USB only. Confirm the harness boots, the LCD draws the header text and the serial monitor prints the `[WeAct I2S] BCK=GPIO2 DIN=GPIO12 WS=GPIO15` banner.
2. **Power off.** Wire `VIN` and `GND` only (no clock / data). Power on — module should be silent, no smoke, no heat after 30 s. If the M5Stack fails to boot here, the module is pulling `DIN` (G12) high — see Troubleshooting in the README.
3. **Power off.** Add BCK, DIN, WS. Power on — module may emit a brief click as XSMT settles; that is normal.
4. **Tap button A.** Expect a clean 1 kHz tone for 250 ms. If silent, jump `SD` to 3V3 (10 kΩ resistor or direct).
5. **Press button B.** Sweep 200 → 2000 Hz. The pitch should rise smoothly without breaks. If it sounds an octave high, the PCM5102A is in 16-bit packed mode while M5.Speaker is sending 32-bit frames — verify `magnification = 16` and `stereo = false`.
6. **Press button C.** Chord. Three pitches should sound simultaneously without clipping at the default 50 % volume.

If step 4 fails, see the Troubleshooting table in [`../README.md`](../README.md).

---
**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-11 | agent:claude-opus-4-7 | Created (Port B + Port C wiring). |
| 2026-05-11 | agent:claude-opus-4-7 | Rewritten for 3-in-a-row left M5-Bus column wiring. Pin map G2/G12/G15 → BCK/WS/DIN. Boot-strap risk note added for G12. |
| 2026-05-11 | agent:claude-opus-4-7 | Reordered to match module's top-to-bottom pad order BCK/DIN/WS. Final map: G2=BCK, G12=DIN, G15=WS. |
