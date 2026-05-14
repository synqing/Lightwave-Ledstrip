---
abstract: "Pin selection rationale for WeActStudio I2S Speaker Module V1 (PCM5102A) on M5Stack Basic v2.7. Constraint: three GPIOs in a vertical line on one M5-Bus column, matching the module's top-to-bottom pad order BCK/DIN/WS. Records why left-column rows 10/11/12 (G2/G12/G15) was chosen over right-column rows 10/11/12 (G5/G13/G0) and other 3-in-a-row candidates."
---

# Pin assignment — M5Stack Basic v2.7 ↔ WeAct I2S Speaker V1

The brief: pick **three GPIOs that run vertically top-to-bottom on a single M5-Bus column** so the module's BCK/WS/DIN pins plug straight onto a 1x3 header with no jumper crossings.

## Bus layout (from physical M5-Bus, top → bottom)

```
Left column                            Right column
 1: GND                                  1: G35      (ADC input-only)
 2: GND                                  2: G36      (ADC input-only)
 3: GND                                  3: RST
 4: G23   (SPI MOSI, LCD/SD)             4: G25      (NS4168 DOUT internal speaker)
 5: G19   (SPI MISO, SD)                 5: G26      (DAC, free output)
 6: G18   (SPI SCK, LCD/SD)              6: 3V3
 7: G3    (USB UART RX)                  7: G1       (USB UART TX)
 8: G16   (UART2 RX, free)               8: G17      (UART2 TX, free)
 9: G21   (I2C internal SDA — IMU/PMU)   9: G22      (I2C internal SCL — IMU/PMU)
10: G2    (free output)                 10: G5       (free output, boot strap pulled high)
11: G12   (M5-Bus I2S_SK, also NS4168)  11: G13      (M5-Bus I2S_WS, free)
12: G15   (M5-Bus I2S_OUT, free)        12: G0       (M5-Bus I2S_MK, also BOOT button)
13: HPWR                                13: G34      (ADC input-only)
14: HPWR                                14: 5V
15: HPWR                                15: BAT
```

## Candidate 3-in-a-row windows

| Window | Pins | Status | Verdict |
|---|---|---|---|
| Left 1-2-3   | GND, GND, GND | Three grounds, not GPIOs | ✗ |
| Left 4-5-6   | G23, G19, G18 | All SPI bus (LCD + SD) | ✗ Would corrupt display and SD |
| Left 5-6-7   | G19, G18, G3 | SPI + USB UART | ✗ |
| Left 6-7-8   | G18, G3, G16 | SPI + USB UART | ✗ |
| Left 7-8-9   | G3, G16, G21 | USB UART + I2C | ✗ G21 is internal I2C SDA |
| Left 8-9-10  | G16, G21, G2  | I2C in the middle | ✗ G21 is internal I2C SDA |
| Left 9-10-11 | G21, G2, G12  | I2C at the top | ✗ G21 is internal I2C SDA |
| **Left 10-11-12** | **G2, G12, G15** | **All output-capable; G12 + G15 are M5-Bus-designated I2S; only collision is internal NS4168 speaker (which we disable)** | **✓ Chosen** |
| Left 11-12-13 | G12, G15, HPWR | HPWR is power | ✗ |
| Right 1-2-3  | G35, G36, RST | Two input-only ADC pins + RST | ✗ Cannot source clock |
| Right 2-3-4  | G36, RST, G25 | Input-only + reset | ✗ |
| Right 3-4-5  | RST, G25, G26 | Reset in the middle | ✗ |
| Right 4-5-6  | G25, G26, 3V3 | Hits a power rail | ✗ |
| Right 5-6-7  | G26, 3V3, G1  | Power rail + USB UART TX | ✗ |
| Right 6-7-8  | 3V3, G1, G17  | Power rail + USB UART TX | ✗ |
| Right 7-8-9  | G1, G17, G22  | USB UART + I2C internal | ✗ G22 is internal I2C SCL |
| Right 8-9-10 | G17, G22, G5  | I2C in the middle | ✗ |
| Right 9-10-11 | G22, G5, G13 | I2C at the top | ✗ |
| Right 10-11-12 | G5, G13, G0 | All output-capable | △ Workable, but G0 is the BOOT button (strap pin + tactile switch on board) — pressing BOOT during runtime yanks WS low |
| Right 11-12-13 | G13, G0, G34 | G34 input-only | ✗ |

## Chosen window — Left rows 10/11/12

The WeAct module's three I2S pads run top-to-bottom in the order **BCK, DIN, WS**. The mapping below matches that order so the module plugs directly onto a 1x3 header inline with bus rows 10/11/12 with no crossed jumpers.

| Row | GPIO | Native function | I2S assignment | Direction |
|---:|---|---|---|---|
| 10 | **G2**  | Free GPIO | **BCK** | output |
| 11 | **G12** | M5-Bus `I2S_SK` (also internal NS4168 BCLK) | **DIN** | output |
| 12 | **G15** | M5-Bus `I2S_OUT` | **WS** | output |

Note: the M5-Bus silkscreen labels for G12 and G15 (`I2S_SK` and `I2S_OUT`) describe their intended role when the bus is driven by the on-board NS4168 — they don't constrain how we use them here. `M5.Speaker.config()` lets us assign any I2S role to any GPIO; the silkscreen is for documentation, not function.

### Why this wins

1. **All three are output-capable** — no ADC-only pins, no reset/power rails inside the run.
2. **Two of the three are purpose-built M5-Bus I2S lanes** — G12 = `I2S_SK`, G15 = `I2S_OUT`. M5Stack designed these specifically for stacked HATs carrying I2S audio.
3. **The third (G2) is a free GPIO with no internal-bus ownership** on M5Stack Basic v2.7.
4. **Single conflict, easily resolved.** G12 is also wired to the on-board NS4168 internal speaker's BCLK. We set `M5.config().internal_spk = false` before `M5.begin()` so the NS4168 driver never claims I2S0 / G12 / G0 / G25.
5. **No BOOT-button collision** — the right-column equivalent (rows 10/11/12 = G5/G13/G0) puts the BOOT button on a clock pin, which would briefly silence audio every time the button is pressed.

### Boot-strap awareness

G2, G12 and G15 are all ESP32 boot-strap pins.

| Pin | Strap requirement | Risk at boot | Mitigation |
|---|---|---|---|
| G2 | Pulled low at boot (internal pulldown) selects programming mode under certain conditions | The WeAct module's BCK input is high-Z → no external drive at boot → ESP32 internal pulldown wins | None needed |
| G12 | **Must be LOW at boot.** A high reading tells the ESP32 the flash is 1.8 V and boot fails | The WeAct module's DIN input is high-Z → same as above; ESP32 internal pulldown wins | If the specific module has an external pull-up on its DIN pad, swap that line to a different GPIO (or remove the pull-up) |
| G15 | Pull low at boot to suppress the UART boot message — purely cosmetic | None | None needed |

All three accept normal use after the second-stage bootloader runs, which is what M5Unified's `Speaker.begin()` relies on.

## Why not the M5-Bus dedicated full I2S set?

The full M5-Bus I2S lane is five pins: `I2S_SK` (G12), `I2S_WS` (G13), `I2S_OUT` (G15), `I2S_MK` (G0), `I2S_IN` (G34). They span both columns and are not vertically contiguous on one side. The brief required three pins in a single column, so we took the three left-column pins from that set (G12 + G15) plus the free G2 directly above them. The PCM5102A doesn't need `I2S_MK` (internal PLL) and we're not reading audio (so `I2S_IN` is unused), so three pins is sufficient.

---
**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-11 | agent:claude-opus-4-7 | Created (Port B + Port C wiring). |
| 2026-05-11 | agent:claude-opus-4-7 | Rewritten for 3-in-a-row constraint. Records every candidate window on both columns and the rationale for left rows 10/11/12 = G2/G12/G15. |
| 2026-05-11 | agent:claude-opus-4-7 | Reordered I2S role assignment to match module's top-to-bottom pad order (BCK, DIN, WS). Final mapping: G2=BCK, G12=DIN, G15=WS. |
