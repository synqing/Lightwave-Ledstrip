# firmware-v3: restore K1v2 production heap headroom

- Disabled K1v2 production diagnostic monitor flags for heap monitoring, memory leak detection, stack profiling, and validation profiling.
- Recovered 6920 bytes of static RAM in the `esp32dev_audio_esv11_k1v2_32khz` build.
- Hardware-verified on K1v2 `/dev/cu.usbmodem2101`: boot internal heap rose above the 12 KB effect-load floor, and previously rejected `0x2103` / `0x0100` effect switches succeeded.
