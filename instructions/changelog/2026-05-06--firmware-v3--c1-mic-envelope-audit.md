## Summary
- Audited whether the 2026-04-27 AFS v2 baseline resolves C-1 microphone-domain operating envelope debt.
- Added a focused C-1 audit note with source-grounded raw-hop RMS, `frame.rms`, and `silentScale` findings.
- Updated `BACKLOG.md` so C-1 no longer points future agents back to a repeat audit; the remaining gate is a Captain hardware envelope characterisation pass.

## Validation
- `git diff --check`
- `rg -n "C-1 stays open|rawHopRms|silentScale|calibrated peak" firmware-v3/docs/research/c1_mic_domain_envelope_audit_2026-05-06.md BACKLOG.md`
