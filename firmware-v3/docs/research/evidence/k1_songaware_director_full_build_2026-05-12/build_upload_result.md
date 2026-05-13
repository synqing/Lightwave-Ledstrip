RBDO label: GROUNDED

Build command:
- `pio run -e esp32dev_audio_esv11_k1v2_32khz`

Build result:
- SUCCESS.
- RAM: 126620 / 327680 bytes, 38.6%.
- Flash: 2506757 / 7340032 bytes, 34.2%.

Upload preflight:
- Port: `/dev/cu.usbmodem1101`.
- Chip: ESP32-S3 rev v0.2.
- MAC: `b4:3a:45:a5:87:f8`.

Upload command:
- `pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem1101`

Upload result:
- SUCCESS.
- Firmware data hash verified.
- Hard reset via RTS pin.
