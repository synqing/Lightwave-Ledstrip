---
id: 2026-05-05--firmware-v3--vp-render-path-audit
date_utc: 2026-05-05
agent: codex
scope: firmware-v3
type: docs
summary: Documented the current VP frame lifecycle, layer risks, and buffer-ownership fork
files_changed:
  - firmware-v3/docs/audit/VP_RENDER_PATH_LAYER_AUDIT_2026-05-05.md
  - firmware-v3/docs/audit/VP_RENDER_PATH_VISUALISER_2026-05-05.html
validation: python3 UTF-8 read check; git diff --check
breaking_change: false
follow_ups:
  - Buffer-ownership correction is gated because changing strip-authored colour correction affects visible output.
  - Subjective colour A/B was skipped; do not use this audit as visual sign-off.
---

## Details

The audit records the live renderer frame lifecycle as one shared output path with a buffer-ownership fork: effects may author either the unified `m_leds` surface or physical strip buffers before converging into strip output stages.

The visualiser is a standalone HTML explainer for the same lifecycle. It now carries the explicit post-test boundary: gamma/status correctness shipped separately, while subjective colour A/B was skipped and is not evidence for changing defaults.
