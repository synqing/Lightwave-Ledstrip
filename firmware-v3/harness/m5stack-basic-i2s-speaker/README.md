---
abstract: "WeActStudio I2S Speaker Module V1 (PCM5102A) integration test on M5Stack Basic v2.7. PlatformIO harness with M5Unified; three vertically adjacent GPIOs on the left M5-Bus column (G2/G12/G15 = BCK/DIN/WS) match the module's top-to-bottom pad order, so a 1x3 ribbon plugs straight on with no jumper crossings. On-screen buttons trigger tone, sweep and chord fixtures. Read when wiring or characterising the external I2S DAC + amp on M5Stack Basic."
---

# M5Stack Basic v2.7 + WeActStudio I2S Speaker Module V1

Integration harness for the WeActStudio I2S Speaker Module V1 ([GitHub](https://github.com/WeActStudio/WeActStudio.I2SSpeakerModuleV1)).

The module is a **PCM5102A** stereo I2S DAC + small on-board amp. Seven pads: `VIN`, `GND`, `BCK` (bit clock), `WS` (word select / LRCK), `DIN` (data), `MC` (master clock / SCK), `SD` (XSMT soft-mute).

## Pin map — left M5-Bus column, three in a row

The WeAct module's three I2S pads run top-to-bottom in the order **BCK, DIN, WS**. The wiring below matches that order so a 1x3 ribbon plugs straight onto the left-column header.

| M5-Bus row | M5Stack pin | WeAct pin | Direction | Notes |
|----:|---|---|---|---|
|  1–3 | GND | `GND` | power | any of the three GND pins at the top of the left column |
| 10 | **GPIO 2** | **BCK** | output | I2S bit clock |
| 11 | **GPIO 12** | **DIN** | output | I2S serial data (M5-Bus `I2S_SK` lane; also internal NS4168 BCLK — internal amp disabled below) |
| 12 | **GPIO 15** | **WS** | output | I2S word select / LRCK (M5-Bus `I2S_OUT` lane) |
| 14 (right column) | 5V | `VIN` | power | 5 V supply; PCM5102A LDO drops to 3.3 V on-board |
| — | float | `MC` | — | PCM5102A internal PLL derives SCK from BCK; leave floating |
| — | float | `SD` | — | On-board pull-up keeps XSMT high (unmuted); see Troubleshooting if silent |

Pin selection rationale and rejected alternatives live in [`docs/pin-assignment.md`](docs/pin-assignment.md).

## Wiring at a glance

```
              Left M5-Bus column                Right M5-Bus column
              +------------------+              +-------------------+
   row  1     |       GND        |--+ blk ---+  |        G35        |  1
   row  2     |       GND        |  |        |  |        G36        |  2
   row  3     |       GND        |  |        |  |        RST        |  3
   row  4     |       G23        |  |        |  |        G25        |  4
   row  5     |       G19        |  |        |  |        G26        |  5
   row  6     |       G18        |  |        |  |        3V3        |  6
   row  7     |       G3         |  |        |  |        G1         |  7
   row  8     |       G16        |  |        |  |        G17        |  8
   row  9     |       G21        |  |        |  |        G22        |  9
   row 10     |  G2  -[BCK]------|  |        |  |        G5         | 10
   row 11     |  G12 -[DIN]------|--|--------|  |        G13        | 11
   row 12     |  G15 -[WS] ------|--|--------|  |        G0         | 12
   row 13     |       HPWR       |  |        |  |        G34        | 13
   row 14     |       HPWR       |  |        +--|---- 5V -[VIN]     | 14
   row 15     |       HPWR       |  |           |        BAT        | 15
              +------------------+  +------ to module GND            +-------------------+
```

A single 1x3 female-to-female ribbon onto rows 10/11/12 carries all three I2S signals. Power is two more wires (5V row 14 right, GND any of rows 1–3 left).

## Build, flash and observe

```bash
cd firmware-v3/harness/m5stack-basic-i2s-speaker

# Build
pio run

# Verify USB serial device, then flash + open monitor.
# Newer M5Stack Basic v2.7 units enumerate as wchusbserial (WCH CH9102);
# older units use usbserial (Silicon Labs CP2104).
ls /dev/tty.wchusbserial* /dev/tty.usbserial-* 2>/dev/null

pio run -t upload --upload-port /dev/tty.wchusbserial57140442941
pio device monitor -b 115200 --port /dev/tty.wchusbserial57140442941
```

After boot the screen shows three button labels:

| Button | Test fixture |
|--------|--------------|
| A (tap) | 1 kHz / 250 ms tone — verifies sample clock and amp wake-up |
| A (hold ≥ 700 ms) | Volume down by 16 |
| B | 200 Hz → 2000 Hz sweep at 25 Hz / 20 ms steps — verifies frequency response and absence of glitches |
| C (tap) | A4 + C#5 + E5 chord on three virtual channels — verifies mixer and stereo summing |
| C (hold ≥ 700 ms) | Volume up by 16 |

Volume is displayed at the bottom of the screen; serial monitor prints the active pin map at boot.

## Acceptance criteria

| Check | Pass condition |
|-------|---------------|
| Power-on | Board boots, screen draws header, no continuous hiss from speaker |
| 1 kHz tone | Clean single pitch, no buzz, no DC click before/after |
| Sweep | Monotonically rising pitch, no dropouts, no octave doubling |
| Chord | Three discernible pitches sounding together, no clipping at default volume |
| Volume control | Audible level change between min/mid/max with no scratching |
| Serial | At least one `[WeAct I2S]` boot banner; no `Guru Meditation` panics in the next 60 s |

## Troubleshooting

| Symptom | Likely cause | Action |
|---|---|---|
| Silence + internal speaker buzzes | Internal NS4168 still active and fighting our drive on G12 | Confirm `cfg.internal_spk = false` is set before `M5.begin(cfg)` |
| Total silence, no buzz | PCM5102A muted (XSMT low) | Tie `SD` to 3V3 (directly, or through 10 kΩ) |
| Total silence, even with SD high | SCK left floating with internal PLL disabled | Tie `MC` to GND (forces PLL-active mode on most WeAct boards) |
| Boot fails / repeating reset | G12 pulled high at boot fakes 1.8 V flash voltage | Module must present a high-Z input on its `DIN` pad at boot; check no external pull-up on the WeAct module's `DIN` pin. If the board has one, move that wire to a different GPIO. |
| Distortion at low volume | `magnification` too high in `M5.Speaker.config()` | Reduce `spk_cfg.magnification` from 16 toward 8 |
| Cracking on every tone | Power rail sag | Move VIN from 3V3 to 5V (already default); add 100 µF bulk cap across VIN/GND at the module |
| Stereo doubling | `spk_cfg.stereo` was left true | PCM5102A summed to one speaker is mono in this harness — keep `stereo = false` |

## Files

```
firmware-v3/harness/m5stack-basic-i2s-speaker/
├── platformio.ini
├── README.md                <- this file
├── src/
│   └── main.cpp             <- test programme
└── docs/
    ├── pin-assignment.md    <- pin selection rationale
    └── wiring-diagram.md    <- ASCII wiring + bring-up checklist
```

---
**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-11 | agent:claude-opus-4-7 | Created harness for WeActStudio I2S Speaker Module V1 + M5Stack Basic v2.7. |
| 2026-05-11 | agent:claude-opus-4-7 | Re-pinned to three vertically adjacent left-M5-Bus pins (rows 10/11/12 = G2/G12/G15 → BCK/WS/DIN) so the module plugs onto a 1x3 header. Updated chip identification to PCM5102A based on confirmed pin names (BCK/WS/MC/SD/DIN). |
| 2026-05-11 | agent:claude-opus-4-7 | Reordered to match WeAct module's physical pad order top-to-bottom (BCK, DIN, WS). Final pin map: G2=BCK, G12=DIN, G15=WS. |
