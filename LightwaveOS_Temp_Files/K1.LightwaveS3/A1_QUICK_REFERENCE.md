# A1 Novelty Decay Tuning - Quick Reference

**Status**: ✓ Implementation Complete, Ready for Testing
**Date**: 2026-01-07

---

## What Changed?

4 parameters tuned across 3 files for improved beat tracking stability:

| Parameter | Old Value | New Value | Impact |
|-----------|-----------|-----------|--------|
| NOVELTY_DECAY | 0.999f | 0.996f | 4× faster decay of stale beats |
| hysteresisThreshold | 1.1f (10%) | 1.08f (8%) | Faster tempo lock-on |
| hysteresisFrames | 5 frames | 4 frames | 40ms faster acquisition |
| confidenceTau | 1.0s | 0.75s | 25% faster confidence response |
| phaseCorrectionGain | 0.35f | 0.40f | 14% stronger beat alignment |
| m_band_attack | 0.15f | 0.18f | Smoother visual onsets |
| m_band_release | 0.03f | 0.04f | Faster decay |
| m_heavy_band_attack | 0.08f | 0.10f | More responsive ambient |
| m_heavy_band_release | 0.015f | 0.020f | Cleaner fade |

---

## Flash Firmware

```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware/v2
pio run -e esp32dev_audio -t upload
pio device monitor -b 115200
```

---

## Quick Test (5 minutes)

1. **EDM Track** (128 BPM steady):
   - Watch serial: `tempo_confidence` should hit >0.7 within 8 seconds
   - Watch serial: `bpm_smoothed` should stabilize at 128 ± 2 BPM
   - Verify: beat_tick aligns with kicks

2. **Silence** (mute audio):
   - Verify: `tempo_confidence` decays to 0
   - Verify: `beat_tick` = false (no ghost beats)
   - Verify: `silence_detected` = true

3. **Vocal Track** (variable dynamics):
   - Verify: confidence recovers quickly after vocal breaks
   - Verify: no false beats during quiet sections

---

## Success Criteria (from implementation plan)

- [ ] Confidence >0.7 within 4 bars on steady-tempo tracks
- [ ] BPM variance <2% after lock
- [ ] Zero false positives on silence/ambient
- [ ] CPU overhead <5% increase

---

## Rollback (if needed)

If testing reveals issues, revert with:

```bash
# Edit these files back to original values:
# src/audio/tempo/TempoTracker.h (NOVELTY_DECAY = 0.999f)
# src/audio/contracts/MusicalGrid.h (confidenceTau = 1.00f)
# src/audio/contracts/ControlBus.h (attack/release values)

pio run -e esp32dev_audio -t upload
```

---

## Full Documentation

- **Test Plan**: `docs/audio/A1_NOVELTY_DECAY_TUNING_TEST_PLAN.md`
- **Implementation Summary**: `docs/audio/A1_IMPLEMENTATION_SUMMARY.md`
