# Prompt Pack: VP Audit

Use this when auditing visual-pipeline layers, colour correction, buffer ownership, silence policy, EdgeMixer, FastLED/RMT, or output ordering.

## First Contact Prompt

```text
GROUNDED:

You are auditing the K1 visual pipeline. Start by reading:
- CLAUDE.md top RBDO gate.
- firmware-v3/docs/audit/VP_RENDER_PATH_LAYER_AUDIT_2026-05-05.md.
- firmware-v3/docs/audit/VP_VALIDATION_PROTOCOL_2026-05-06.md.
- firmware-v3/docs/debugging/VP_STACK_INTROSPECTION_COMMAND_SPEC.md.
- firmware-v3/docs/EFFECTS_BEHAVIORAL_REFERENCE.md.
- docs/CAPTURE_PIPELINE_REFERENCE.md if capture/taps are in scope.

Task:
1. Verify current branch, HEAD, and dirty tree.
2. Map the current frame lifecycle as one lifecycle with a buffer-ownership fork.
3. Identify the authored surface: unified m_leds, zone-composed unified, or direct strip buffers.
4. Identify every active/bypassed layer and its colour/brightness/saturation/contrast effect.
5. Identify whether colour correction processes the actual authored surface.
6. Separate protective transport layers from aesthetic layers.
7. State whether any finding requires Captain visual sign-off before default changes.

Non-goals:
- do not remove or weaken the RMT/wire-time fence;
- do not change colour correction, gamma, silence, EdgeMixer, or buffer ownership defaults;
- do not use K1_Testbed as physical LGP proof;
- do not add REST/WS commands without protocol YAML updates.

Return:
- ordered layer map;
- buffer ownership table;
- active/inactive layer list;
- visual-risk list;
- exact source anchors;
- recommended next gate.
```
