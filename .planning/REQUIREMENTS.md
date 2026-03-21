# Requirements: Critical Bug Fixes C-2, C-3, C-4

**Defined:** 2026-03-21
**Core Value:** Eliminate undefined behaviour and silent data corruption in the network API layer.

## v1 Requirements

### C-2: API Type Truncation

- [ ] **C2-01**: All 5 audio mapping endpoints use EffectId (uint16_t) instead of uint8_t for effect ID parsing
- [ ] **C2-02**: Input validation added — reject IDs outside valid range with HTTP 400
- [ ] **C2-03**: Effects with IDs >= 256 can be queried and modified via REST API

### C-3: Dangling Pointer

- [ ] **C3-01**: handleEffectsGetCategories() uses persistent buffers for family names (not stack-local loop variable)
- [ ] **C3-02**: WebSocket /effects/categories response contains correct family names for all families

### C-4: Cross-Core Data Race

- [ ] **C4-01**: getControlBusMut() removed from AudioActor public API
- [ ] **C4-02**: All audio config mutations routed through actor message queue (SET_ZONE_AGC_CONFIG, etc.)
- [ ] **C4-03**: Read-only audio diagnostics accessible via snapshot buffer (no mutable cross-task access)
- [ ] **C4-04**: All callers of getControlBusMut() in WebServer/WsCommands migrated to message-based API

### Verification

- [ ] **VER-01**: Build succeeds (pio run -e esp32dev_audio_esv11_k1v2_32khz)
- [ ] **VER-02**: Flash to K1 and soak 5+ minutes — zero panics, zero crashes
- [ ] **VER-03**: iOS app can connect and control effects (WebSocket functional)

## Out of Scope

| Feature | Reason |
|---------|--------|
| C-1 PipelineCore | Already mitigated by not using it |
| New API endpoints | Fix only, no additions |
| API authentication (S-2) | Separate security concern |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| C2-01 | Phase 1: Quick Fixes | Pending |
| C2-02 | Phase 1: Quick Fixes | Pending |
| C2-03 | Phase 1: Quick Fixes | Pending |
| C3-01 | Phase 1: Quick Fixes | Pending |
| C3-02 | Phase 1: Quick Fixes | Pending |
| C4-01 | Phase 2: Data Race Fix | Pending |
| C4-02 | Phase 2: Data Race Fix | Pending |
| C4-03 | Phase 2: Data Race Fix | Pending |
| C4-04 | Phase 2: Data Race Fix | Pending |
| VER-01 | All phases | Pending |
| VER-02 | All phases | Pending |
| VER-03 | Phase 2 | Pending |

**Coverage:**
- v1 requirements: 12 total
- Mapped to phases: 12
- Unmapped: 0

---
*Requirements defined: 2026-03-21*
