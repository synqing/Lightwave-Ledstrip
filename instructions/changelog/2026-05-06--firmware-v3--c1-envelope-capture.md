---
scope: firmware-v3
change_type: docs
summary: Record C-1 K1v2 microphone-domain envelope capture evidence
---

- Added C-1 serial envelope capture tooling with guarded process-owned playback timing and post-hard-stop recovery calculation.
- Added a C-1 K1v2 microphone-domain capture report with sanitised numeric evidence and no committed private clip paths.
- Updated `BACKLOG.md` to mark C-1 as measured-degraded for current K1v2 firmware-domain work while leaving SPL/LUFS, cross-device, and production-acoustic claims degraded.

## Validation
- `python3 -m py_compile firmware-v3/tools/capture_c1_envelope.py firmware-v3/test/test_native/test_capture_c1_envelope.py`
- `python3 firmware-v3/test/test_native/test_capture_c1_envelope.py -v`
- `pio run -e esp32dev_audio_esv11_k1v2_32khz_c1_envelope`
- `pio run -e esp32dev_audio_esv11_k1v2_32khz_c1_envelope -t upload --upload-port /dev/cu.usbmodem2101`
- `~/.platformio/penv/bin/python3 firmware-v3/tools/capture_c1_envelope.py --audio-manifest <private-audio-manifest.json> --armed --serial-c1-lines`
