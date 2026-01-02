# PlatformIO Board Discovery Hints

If the Tab5 board is not found in PlatformIO, use these commands to discover available board IDs.

## List All ESP32 Boards

```bash
pio boards | grep -i esp32
```

## Search for Tab5 Specifically

```bash
pio boards | grep -i tab5
```

## Search for ESP32-P4

```bash
pio boards | grep -i "esp32-p4"
pio boards | grep -i "p4"
```

## List All M5Stack Boards

```bash
pio boards | grep -i m5stack
```

## Update platformio.ini

Once you find the correct board ID, update `platformio.ini`:

```ini
[env:tab5]
board = <found-board-id>
```

## Common Board IDs to Try

- `m5stack-tab5`
- `esp32-p4-devkitm-1`
- `esp32-p4`
- `m5stack_tab5`

## Fallback

If no suitable board is found, use **Arduino IDE** instead (see README.md).

