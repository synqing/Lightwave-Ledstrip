---
abstract: "End-user guide for the Zone Mixer physical controller (AtomS3 + PaHub). Covers hardware layout, all 20 input controls (4 scroll wheels + 16 encoders), LED feedback system, connection workflow, and troubleshooting."
---

# Zone Mixer User Guide

The Zone Mixer is a dedicated physical controller for the K1 LightwaveOS LED system's Zone Composer. It gives you hands-on, tactile control of up to three independent LED zones, plus master parameters and the EdgeMixer blending engine.

Think of it as a DJ mixer for light: scroll wheels act as channel faders controlling zone brightness, while encoder banks provide rotary knobs for effect selection, speed, palette, and mixing parameters. Every control has an illuminated LED that reflects the current state at a glance.

## Hardware

| Component | Role |
|-----------|------|
| **AtomS3** | ESP32-S3 brain with 0.85" colour display |
| **PaHub v2.1** | I2C multiplexer (PCA9548A) connecting 6 devices |
| **4x Unit-Scroll** | Scroll wheels with integrated button and LED |
| **2x M5Rotate8** | 8-encoder modules, each with 8 knob LEDs + 1 status LED |

Total: 4 scroll wheels + 16 rotary encoders = **20 input controls** and **22 LEDs**.

## Hardware Layout

The PaHub multiplexer connects each device to a dedicated I2C channel. The physical channel map (confirmed from firmware):

| PaHub Channel | Device | Role |
|---------------|--------|------|
| CH0 | 8Encoder #1 (addr 0x42) | Zone Selection Bank (EncA) |
| CH1 | 8Encoder #2 (addr 0x41) | Mix and Config (EncB) |
| CH2 | Unit-Scroll | Zone 1 Brightness |
| CH3 | Unit-Scroll | Zone 2 Brightness |
| CH4 | Unit-Scroll | Zone 3 Brightness |
| CH5 | Unit-Scroll | Master Brightness |

## Zone Colour Code

Each zone has a consistent colour identity used across all LEDs, the display, and the K1 itself:

| Zone | Colour | Hex |
|------|--------|-----|
| Zone 1 (innermost) | Cyan | `#00FFFF` |
| Zone 2 (middle) | Green | `#00FF99` |
| Zone 3 (outermost) | Orange | `#FF6600` |
| Master | White | `#FFFFFF` |

## Scroll Wheels -- Zone Faders

The four scroll wheels function as dedicated channel faders. Each wheel always controls its assigned brightness -- there are no modes to switch between.

| Scroll | Wheel Rotation | Button Press | LED Colour |
|--------|---------------|--------------|------------|
| **Zone 1** (CH2) | Zone 1 brightness (0--255) | Toggle Zone 1 enable/disable | Cyan, proportional to brightness |
| **Zone 2** (CH3) | Zone 2 brightness (0--255) | Toggle Zone 2 enable/disable | Green, proportional to brightness |
| **Zone 3** (CH4) | Zone 3 brightness (0--255) | Toggle Zone 3 enable/disable | Orange, proportional to brightness |
| **Master** (CH5) | Master brightness (0--255) | Toggle zone system on/off | White, proportional to brightness |

**Notes:**
- When a zone is disabled (via button press), its LED dims to a low indicator level rather than going fully dark.
- The Master scroll button controls the entire zone system. When the zone system is off, all zone-specific controls are inactive on the K1 -- the system reverts to single-effect mode.
- Brightness changes are continuous -- rotate the wheel smoothly for fine adjustment.

## 8Encoder #1 -- Zone Selection Bank (EncA)

This encoder module handles per-zone effect selection, speed, and palette control.

| Encoder | Rotation | Button Press | LED Colour |
|---------|----------|-------------|------------|
| **E0** | Cycle Zone 1 effect (scroll through effect list) | -- | Cyan (Zone 1 colour) |
| **E1** | Cycle Zone 2 effect | -- | Green (Zone 2 colour) |
| **E2** | Cycle Zone 3 effect | -- | Orange (Zone 3 colour) |
| **E3** | Master speed (1--100) | -- | Blue |
| **E4** | Zone 1 speed (1--50) OR palette (0--74) | Toggle between speed and palette mode | Blue = speed mode, Green = palette mode |
| **E5** | Zone 2 speed OR palette | Toggle between speed and palette mode | Blue = speed mode, Green = palette mode |
| **E6** | Zone 3 speed OR palette | Toggle between speed and palette mode | Blue = speed mode, Green = palette mode |
| **E7** | Unassigned | -- | Dark |

**Status LED (LED 8):** Indicates zone system state. **Green** = zone system enabled, **dark** = zone system disabled.

**Effect selection (E0--E2):** The effect list is fetched from the K1 on connection. Rotating the encoder steps through the full list, wrapping at boundaries. If the effect list has not loaded yet (no WebSocket connection), rotation is ignored.

**Speed/palette toggle (E4--E6):** Each zone encoder starts in speed mode (blue LED). Press the button to switch to palette mode (green LED). Press again to switch back. The rotation always controls whichever mode is active.

## 8Encoder #2 -- Mix and Config (EncB)

This encoder module handles the EdgeMixer blending engine, zone layout, transitions, camera mode, and presets.

| Encoder | Rotation | Button Press | LED Colour |
|---------|----------|-------------|------------|
| **E0** | EdgeMixer mode (7 modes, 0--6) | -- | Mode colour (see table below) |
| **E1** | EdgeMixer spread (0--60 degrees) | -- | Warm amber, proportional |
| **E2** | EdgeMixer strength (0--255) | -- | Cool blue, proportional |
| **E3** | -- | Cycle spatial/temporal (4 states) | State colour (see table below) |
| **E4** | Zone count (1--3) | -- | Green |
| **E5** | Transition type (0--11, 12 types) | Trigger next effect transition | Dim purple |
| **E6** | -- | Toggle camera mode on/off | Orange = on, Dark = off |
| **E7** | Preset slot (0--7) | Load preset from current slot | White dim |

**Status LED (LED 8):** Indicates WebSocket connection state:
- **Green (solid)** = connected to K1
- **Blue (breathing)** = connecting (sine wave pulse, 1.5s cycle)
- **Red (solid)** = disconnected

### EdgeMixer Mode Colours (E0)

| Mode | LED Colour |
|------|-----------|
| 0 | Grey |
| 1 | Orange |
| 2 | Blue |
| 3 | Teal |
| 4 | Grey |
| 5 | Purple |
| 6 | Yellow |

### Spatial/Temporal States (E3)

The E3 button cycles through four states that control how the EdgeMixer blends zones:

| State | Spatial | Temporal | LED Colour |
|-------|---------|----------|-----------|
| 0 | Off | Off | Dark (off) |
| 1 | On | Off | Blue |
| 2 | Off | On | Green |
| 3 | On | On | White |

## LED Feedback System

All 22 LEDs provide real-time visual feedback. The system is designed so you can read the controller state at a glance without looking at a screen.

### Proportional Brightness

Scroll wheel LEDs and several encoder LEDs track the current parameter value:
- **Zone scrolls:** LED brightness is proportional to zone brightness (brighter LED = higher brightness value).
- **Master scroll:** White LED intensity tracks master brightness.
- **EdgeMixer spread (EncB E1):** Warm amber intensity tracks spread value.
- **EdgeMixer strength (EncB E2):** Cool blue intensity tracks strength value.

### Button Flash

Every button press triggers a 100ms white flash on the corresponding LED, providing immediate tactile-visual confirmation regardless of the control's normal colour.

### Refresh Rate

LEDs are flushed to hardware at 10Hz maximum (100ms intervals) using a dirty-flag system. Only LEDs whose colour has changed since the last flush are updated, minimising I2C bus traffic.

## Display

The AtomS3's 0.85" screen shows four lines of status information, refreshed at 2Hz:

| Line | Content | Colour |
|------|---------|--------|
| 1 | WiFi status: IP address or "---" | Green = connected, Red = disconnected |
| 2 | WebSocket status | Green = "connected", Yellow = "connecting...", Grey = "waiting" |
| 3 | Master brightness, speed, and zone count | White |
| 4 | Version string ("Zone Mixer v0.4") | Grey |

## Connection and Startup

1. **Power on** the K1 first. It creates the "LightwaveOS-AP" WiFi network (open, no password).
2. **Connect the Zone Mixer** via USB-C. The AtomS3 display shows "Zone Mixer v0.4" immediately.
3. **WiFi connects automatically** to the K1's access point at 192.168.4.1. The display updates to show the assigned IP address in green.
4. **WebSocket connects** to `ws://192.168.4.1:80/ws`. During connection, the EncB status LED breathes blue.
5. **Hello sequence** runs automatically on connection:
   - Enables the zone system on the K1
   - Requests current status (brightness, speed, effects)
   - Requests zone state (per-zone brightness, speed, effect, palette, enabled state)
   - Requests EdgeMixer state
   - Requests camera mode state
   - Requests the full effect list (for E0--E2 effect cycling)
6. **Ready.** The EncB status LED turns solid green. All controls are now live.

**Reconnection:** If the K1 is power-cycled or goes out of range, the Zone Mixer detects the disconnection and automatically reconnects with exponential backoff (starting at 1 second, up to 30 seconds). No manual intervention is required.

## Multi-Client Behaviour

The Zone Mixer works alongside the Tab5 controller and the iOS app. All three can be connected to the K1 simultaneously.

- **Outbound changes** from the Zone Mixer are sent to the K1 immediately. The K1 broadcasts the new state to all connected clients.
- **Inbound changes** from other clients (Tab5, iOS app) are received via WebSocket broadcasts and applied to the Zone Mixer's internal state. Encoder positions and scroll values update to reflect the new values.
- **Echo guard** prevents snapback: after a local change, the Zone Mixer ignores incoming updates for the same parameter for 1 second. This prevents the following scenario: you rotate an encoder, the K1 echoes the old value back before processing your change, and the control snaps back momentarily.
- **Zone system state** is shared. If another client disables the zone system, the EncA status LED goes dark and a serial log message confirms: "System DISABLED by another client."
- **Initial sync** on first connection forces all parameters to accept the K1's values regardless of echo guard state.

## Troubleshooting

| Problem | Cause | Solution |
|---------|-------|----------|
| Zone controls have no effect | Zone system is disabled | Check EncA status LED (LED 8). If dark, press the Master scroll button to re-enable the zone system. |
| No WiFi connection | K1 not powered or out of range | Power on the K1 first. The Zone Mixer auto-connects to "LightwaveOS-AP". Check display line 1 for IP address. |
| Encoder rotation does nothing | Effect list not loaded | Wait for WebSocket connection (EncB status LED turns green). Effect selection requires the effect list from K1. |
| EncB status LED stays red | WebSocket cannot connect | Verify the K1 is running and within WiFi range. The Zone Mixer will retry automatically with increasing backoff. |
| Wrong zone is affected | Physical arrangement mismatch | Scrolls are mapped by PaHub channel: CH2 = Zone 1, CH3 = Zone 2, CH4 = Zone 3, CH5 = Master. Check the physical wiring order. |
| LED not updating | Dirty-flag flush rate | LEDs update at 10Hz maximum. A brief delay (up to 100ms) between input and LED change is normal. |
| Display shows "NO INPUT DEVICES" | I2C bus or wiring fault | Check the Grove cable between AtomS3 and PaHub, and all PaHub-to-device cables. Power cycle the system. |
| Speed/palette encoder seems wrong | Wrong mode active | Check the LED colour on EncA E4/E5/E6: blue = speed mode, green = palette mode. Press the button to toggle. |
| Values snap back after adjustment | Echo guard timing | This can occur in rare cases when network latency exceeds the 1-second echo holdoff. Allow the change to settle before making further adjustments. |

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-01 | captain + agent:claude-opus-4.6 | Created -- comprehensive user guide covering all 20 controls, LED feedback, connection workflow, and troubleshooting |
