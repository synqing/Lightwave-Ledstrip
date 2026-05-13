---
id: 2026-05-06--firmware-v3--k1v2-sram-psram-reclaim-handoff
date_utc: 2026-05-06
agent: codex
scope: firmware-v3
type: docs
summary: Add K1v2 SRAM/PSRAM reclaim execution handoff and BACKLOG authority anchor
files_changed:
  - firmware-v3/docs/research/k1v2_sram_psram_reclaim_handoff_2026-05-06.md
  - BACKLOG.md
validation: Documentation-only change; validate with git diff --check
breaking_change: false
follow_ups:
  - Execute the SRAM/PSRAM reclaim pass on K1v2 before resuming Phase 5 visual-quality testing.
---

## Details

Records the current K1v2 low-heap failure evidence, the post-`63a4b392` headroom baseline, safe candidate batches, hard refusals, hardware verification commands, and the return path back to Phase 5 visual tuning.
