## Summary
- Audited the `CONTROLBUS_NUM_ZONES = 4` debt without changing firmware behaviour.
- Added a source-grounded F-6 audit covering current 8-band/chroma partitions, live consumers, ESV11 production bypasses, and refactor hazards.
- Updated `BACKLOG.md` to mark F-6 Phase 1 audit complete while keeping implementation gated on Captain's restructuring decision and hardware validation.

## Validation
- `git diff --check`
- `rg -n "F-6|CONTROLBUS_NUM_ZONES|Phase 1 audit complete|Production ESV11|applyDerivedFeatures|UpdateFromHop" firmware-v3/docs/research/f6_controlbus_num_zones_audit_2026-05-06.md BACKLOG.md`
