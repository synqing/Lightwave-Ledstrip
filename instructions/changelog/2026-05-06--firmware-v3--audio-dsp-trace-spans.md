## Summary
- Added a K1v2 ESV11 32 kHz audio-DSP trace env for Surface 3 Tier 2 decomposition.
- Added opt-in Core 0 DSP spans for STM FFT, STM extraction, onset processing, Stage B update, and ControlBus publish.
- Moved legacy high-rate Core 0 spans behind `FEATURE_TRACE_AUDIO_DSP` so the base K1v2 trace env stays Tier 1 focused.

## Validation
- `git diff --check`
- `python3 scripts/check_native_harness_routes.py`
- `pio run -e esp32dev_audio_esv11_k1v2_32khz`
- `pio run -e esp32dev_audio_esv11_k1v2_32khz_trace`
- `pio run -e esp32dev_audio_esv11_k1v2_32khz_trace_dsp`
