# STM Feasibility Harness

Standalone ESP32-S3 benchmark for the STM feature extractor.

It measures:

- `mel_filterbank`: 256-bin FFT magnitude to 16 mel bands
- `buffer_update`: 16x16 sliding-history update
- `temporal_mod`: 16-band temporal modulation via Goertzel
- `spectral_mod`: 16-point spectral modulation FFT
- `full_pipeline`: end-to-end STM hop processing

Build:

```bash
cd harness/stm-feasibility
pio run -e stm_benchmark
```

Flash and monitor on hardware:

```bash
pio run -e stm_benchmark -t upload
pio device monitor -b 115200
```

Expected serial format:

```text
[STM-BENCH] mel_filterbank: min=Xus med=Xus p99=Xus
[STM-BENCH] buffer_update: min=Xus med=Xus p99=Xus
[STM-BENCH] temporal_mod: min=Xus med=Xus p99=Xus
[STM-BENCH] spectral_mod: min=Xus med=Xus p99=Xus
[STM-BENCH] full_pipeline: min=Xus med=Xus p99=Xus
[STM-BENCH] RESULT: PASS|FAIL (p99=Xus vs threshold=100us)
```
