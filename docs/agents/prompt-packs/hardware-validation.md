# Prompt Pack: Hardware Validation

Use this when flashing, serial-monitoring, capturing traces, or preparing Captain visual sign-off.

## First Contact Prompt

```text
GROUNDED:

You are running K1 hardware validation. Start by reading:
- CLAUDE.md top RBDO gate.
- AGENTS.md Visual Pipeline Guardrails.
- firmware-v3/docs/audit/VP_VALIDATION_PROTOCOL_2026-05-06.md.
- firmware-v3/docs/testing/K1_TESTBED_INTEGRATION_BOUNDARY.md if simulation was used.
- docs/CAPTURE_TEST_SUITES.md if capture suites are in scope.

Task:
1. Verify branch, HEAD, dirty tree, and exact firmware env.
2. Verify device identity by MAC before upload. Do not rely on serial port name alone.
3. Confirm K1 WiFi mode expectations. Do not introduce STA/REST dependency unless explicitly authorised.
4. Build the exact env before upload.
5. Upload only to the confirmed target.
6. Capture serial `s` and relevant debug/status commands.
7. Confirm no panics, no RMT errors, sane LED show time, `showSkips=0`, stable heap and stack.
8. Only then ask Captain for visual judgement if the task requires human perception.

Non-goals:
- no two-unit A/B unless Captain explicitly asks for it;
- no private media paths in public docs;
- no hidden REST assumption on AP-only devices;
- no commit after a visible firmware behaviour change until hardware evidence exists.

Return:
- env, port, MAC, build result, upload result;
- serial evidence;
- visual prompt given to Captain, if any;
- pass/fail/degraded classification.
```
