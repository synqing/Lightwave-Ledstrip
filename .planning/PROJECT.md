# K1↔Tab5 Protocol Contract + LVGL Component Reference

## What This Is

Building two agent harness documents that prevent protocol drift between K1 and Tab5, and prevent LVGL UI breakage when agents modify Tab5 screens. Plus fixing 3 real bugs found during protocol research (naming mismatches, dead commands).

## Core Value

Agents must be able to modify K1 network code or Tab5 UI code without breaking the other side or producing visual garbage.

## Requirements

### Active

- [ ] WebSocket protocol contract (YAML) covering all 141 commands + broadcasts
- [ ] REST protocol contract (YAML) covering all 93 endpoints
- [ ] LVGL component reference (~300 lines) with widget trees, fonts, colours, anti-patterns, templates
- [ ] Fix Tab5 WsMessageRouter broadcast naming mismatches (effectChanged, zones.stateChanged)
- [ ] Remove 3 dead commands from Tab5 WebSocketClient + document as deprecated
- [ ] CLAUDE.md updates making both harnesses mandatory reading
- [ ] Build + flash Tab5 to verify bug fixes

### Out of Scope

- Protocol lint script (Phase 2, after contract stabilises)
- C++ enum code generation from YAML (Phase 3)
- Tab5 test infrastructure (separate milestone)
- Consolidating duplicated TAB5_COLOR_* / make_card() (document, don't fix)

---
*Last updated: 2026-03-21 after eng review*
