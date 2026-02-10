# Agent Routing Matrix

**Version:** 1.0  
**Last Updated:** 2026-01-18  
**Status:** Operational Reference

This document defines the routing matrix that maps files, tasks, and domains to specialised Claude-Flow agents, ensuring work reaches domain experts with appropriate guardrails and acceptance criteria.

---

## Routing Rules

### Domain-Based Routing

| Domain | Path Pattern | Agent Type | Primary Constraints |
|--------|--------------|------------|-------------------|
| **Firmware Effects** | `firmware/v2/src/effects/**` | `embedded` / `firmware` | Centre-origin (79/80), no heap in render loops, 120+ FPS |
| **Network/API** | `firmware/v2/src/network/**` | `api` / `network` | Protected file protocols, additive JSON only, thread safety |
| **Dashboard UI** | `lightwave-dashboard/**` | `frontend` / `ui` | Accessibility standards, E2E tests, UI standards compliance |
| **Documentation** | `docs/**` | `documentation` / `docs` | British English, spelling (centre/colour/initialise) |

### Protected File Routing

**CRITICAL**: Protected files require extra review gates before agent modification.

| File Pattern | Protection Level | Mandatory Checks |
|--------------|------------------|------------------|
| `firmware/v2/src/network/WiFiManager.*` | **CRITICAL** | FreeRTOS EventGroup bit clearing, mutex-protected state transitions |
| `firmware/v2/src/network/WebServer.*` | **HIGH** | Rate limiting thread safety, WebSocket handler synchronization |
| `firmware/v2/src/audio/AudioActor.*` | **HIGH** | Cross-core atomic buffer correctness, lock-free double-buffering |

**Routing Behaviour**:
- Protected files trigger `hooks_route` with review gate
- Agent must read entire file + `.claude/harness/PROTECTED_FILES.md`
- Pre-commit: Run related unit tests before AND after changes

---

## Acceptance Criteria by Domain

### Firmware Effects Domain

**Files**: `firmware/v2/src/effects/**`, `firmware/v2/src/plugins/**`

**Mandatory Checks**:
1. **Centre-origin compliance**: Effects originate from LEDs 79/80, propagate outward
2. **No heap allocation**: No `new`, `malloc`, `std::vector` growth, `String` concatenation in `render()`
3. **Performance**: Target 120+ FPS (validate with `s` serial command)
4. **No rainbows**: No full hue-wheel sweeps or rainbow colour cycling
5. **Effect ID stability**: New effects maintain stable IDs matching PatternRegistry indices

**Validation Commands**:
```bash
# Serial: s (FPS/CPU check)
# Serial: validate <effectId> (centre-origin, hue-span, FPS, heap-delta)
# Build: cd firmware/v2 && pio run -e esp32dev_audio_esv11
```

**Pattern Memory Keys**:
- `effect_migration`: IEffect class + registration + PatternRegistry + validation
- `centre_origin_math`: Signed position helpers (CENTER_LEFT/CENTER_RIGHT usage)
- `render_loop_safety`: Stack allocation patterns, pre-allocated buffers

---

### Network/API Domain

**Files**: `firmware/v2/src/network/**`, `firmware/v2/src/network/webserver/**`

**Mandatory Checks**:
1. **Protected file protocols**: WiFiManager, WebServer require review gate
2. **Additive JSON only**: API responses extend fields only (backward compatibility)
3. **Thread safety**: WebSocket handlers on different task than HTTP (mutex protection)
4. **Rate limiting**: Non-blocking behaviour, proper timeout handling
5. **OpenAPI sync**: Handler changes update OpenAPI spec in WebServer.cpp

**Protected File Specifics**:

**WiFiManager**:
- `setState()` MUST clear EventGroup bits when entering `STATE_WIFI_CONNECTING`
- Clear: `EVENT_CONNECTED | EVENT_GOT_IP | EVENT_CONNECTION_FAILED`
- All state transitions mutex-protected via `m_stateMutex`

**WebServer**:
- Rate limiting state thread-safe (mutex or atomic)
- WebSocket handlers run on different task (use mutex for shared state)
- AsyncWebServer callbacks NOT synchronized (add explicit synchronization)

**Validation Commands**:
```bash
# Build: cd firmware/v2 && pio run -e esp32dev_audio_esv11 -t upload
# Serial: Verify WiFi connects without "IP: 0.0.0.0" appearing first
# API: GET /api/v1/effects (verify backward compatibility)
```

**Pattern Memory Keys**:
- `rest_endpoint_add`: Handler + OpenAPI update + dashboard client + docs
- `websocket_command`: WsCommandRouter registration + client update
- `protected_file_edit`: Review gate checklist, EventGroup bit clearing

---

### Dashboard UI Domain

**Files**: `lightwave-dashboard/**`

**Mandatory Checks**:
1. **Accessibility**: WCAG 2.1 AA compliance (use `e2e/accessibility.spec.ts`)
2. **E2E tests**: New components include Playwright tests
3. **UI standards**: No inline responsive styles (use Tailwind classes)
4. **Focus management**: Keyboard navigation, skip links (see `FOCUS_MANAGEMENT.md`)
5. **No CDNs**: All assets bundled (verify with `scripts/verify-no-cdns.js`)

**Validation Commands**:
```bash
# E2E: npm run test:e2e (Playwright)
# Accessibility: npm run test:e2e -- accessibility.spec.ts
# Lint: npm run lint (ESLint, no-inline-responsive-styles)
# Build: npm run build (Vite)
```

**Pattern Memory Keys**:
- `component_add`: Component + test + accessibility + docs
- `ui_standards_check`: Tailwind classes, no inline styles, focus management
- `dashboard_api_sync`: Dashboard client matches firmware API contract

---

### Documentation Domain

**Files**: `docs/**`, `firmware/v2/docs/**`, `lightwave-dashboard/docs/**`

**Mandatory Checks**:
1. **British English**: Use `centre` (not `center`), `colour` (not `color`), `initialise` (not `initialize`)
2. **Spelling consistency**: Check all docs for American spellings (automated where possible)
3. **Cross-references**: Update related doc links when adding new docs
4. **Audio-reactive docs**: Follow `AUDIO_PIPELINE_DATA_FLOW.md` structure for audio docs

**Audio-Reactive Documentation Requirements**:
- Read `docs/audio-visual/AUDIO_VISUAL_SEMANTIC_MAPPING.md` before adding audio features
- Document complete layer audit (ALL layers of effect)
- Avoid rigid frequency→visual bindings (bass→expansion is a trap)
- Emphasise musical saliency analysis (what's important, not all signals)

**Validation Commands**:
```bash
# Spell check: grep -ri "center\|color\|initialize" docs/ (should be empty except for code)
# Link check: Verify internal doc links resolve
# Structure check: Audio docs follow AUDIO_PIPELINE_DATA_FLOW.md format
```

**Pattern Memory Keys**:
- `british_english_template`: Centre/colour/initialise spelling patterns
- `audio_doc_structure`: Pipeline data flow format, layer audit protocol
- `cross_reference_update`: Related doc link maintenance

---

## Routing Implementation

### hooks_route Configuration

Configure `hooks_route` to enforce domain boundaries and protected file gates:

```javascript
// Example routing hook (conceptual)
hooks_route({
  pattern: "firmware/v2/src/effects/**",
  agent: "embedded",
  constraints: ["centre-origin", "no-heap-allocation", "120-fps"],
  memory_keys: ["effect_migration", "centre_origin_math"]
});

hooks_route({
  pattern: "firmware/v2/src/network/WiFiManager.*",
  agent: "network",
  protection_level: "CRITICAL",
  review_gate: true,
  mandatory_reads: [".claude/harness/PROTECTED_FILES.md", "WiFiManager.cpp"],
  pre_commit_tests: ["wifi_manager_tests"]
});
```

### Memory Search Patterns

Common memory search queries:

```bash
# Effect migration pattern
memory_search "effect_migration IEffect PatternRegistry"

# Centre-origin math helpers
memory_search "centre_origin CENTER_LEFT CENTER_RIGHT signed position"

# Render loop safety patterns
memory_search "render_loop no_heap_allocation stack_buffer pre_allocated"

# Protected file protocols
memory_search "WiFiManager EventGroup setState STATE_WIFI_CONNECTING"

# British English spelling
memory_search "british_english centre colour initialise spelling"
```

---

## Quality Gates

### Pre-Commit Gates

**Firmware Effects**:
- `validate <effectId>` passes (centre-origin, hue-span, FPS, heap-delta)
- `s` serial command shows acceptable FPS (≥120)

**Network/API**:
- Protected file edits: All related unit tests pass
- API changes: Backward compatibility verified (GET /api/v1/effects unchanged)

**Dashboard UI**:
- E2E tests pass (`npm run test:e2e`)
- Accessibility checks pass (`accessibility.spec.ts`)
- No inline responsive styles (`npm run lint`)

**Documentation**:
- British English verified (no `center`/`color`/`initialize` in prose)
- Cross-references resolve (internal links valid)

### Post-Commit Gates

**Pattern Memory Updates**:
- Successful patterns stored via `memory_search` indexing
- Failed patterns flagged for review (invariant violations, test failures)

**Routing Feedback**:
- Agent routing accuracy tracked (correct domain specialist reached)
- Protected file safety tracked (no EventGroup bugs, no race conditions)

---

## Related Documentation

- **[overview.md](./overview.md)** - Strategic integration overview
- **[swarm-templates.md](./swarm-templates.md)** - Swarm configurations using this routing
- **[pair-programming.md](./pair-programming.md)** - Pair programming modes per domain
- **[validation-checklist.md](./validation-checklist.md)** - Setup and runtime verification

---

*This routing matrix ensures work reaches domain experts with appropriate guardrails, enforcing invariants through specialised agents and protected file protocols.*
