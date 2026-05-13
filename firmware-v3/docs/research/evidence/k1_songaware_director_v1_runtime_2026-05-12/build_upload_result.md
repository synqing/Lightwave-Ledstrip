# Build And Upload Result

RBDO label: GROUNDED

## Build

Command:

```text
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz
```

Result:

```text
SUCCESS
Environment: esp32dev_audio_esv11_k1v2_32khz
Duration: 00:00:43.883
RAM: 38.4% (used 125860 bytes from 327680 bytes)
Flash: 33.8% (used 2480801 bytes from 7340032 bytes)
```

Compiler warnings observed:

```text
"WS_MAX_QUEUED_MESSAGES" redefined
```

The warnings are existing build-surface warnings and did not block firmware generation.

## Upload

Command:

```text
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem1101
```

Result:

```text
SUCCESS
Environment: esp32dev_audio_esv11_k1v2_32khz
Duration: 00:00:59.150
Chip: ESP32-S3 revision v0.2
MAC: b4:3a:45:a5:87:f8
Firmware bytes: 2481168
Hash of data verified.
Hard resetting via RTS pin.
```

## Decision

PASS: build and upload both completed successfully.
