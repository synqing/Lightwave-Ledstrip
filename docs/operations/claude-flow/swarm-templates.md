# Swarm Templates

**Version:** 1.0  
**Last Updated:** 2026-01-18  
**Status:** Operational Reference

This document provides reusable swarm templates for common Lightwave-Ledstrip workflows, with CLI snippets for `swarm_init`, `agent_spawn`, and coordination patterns.

---

## Template Usage

### Effect Migration Swarm

**Use Case**: Migrate a legacy effect function to native IEffect class

**Pattern**: Hierarchical (queen coordinates sequential steps)

**Steps**:
1. Create IEffect class files (`*.h` and `*.cpp`)
2. Register effect in CoreEffects.cpp (stable ID)
3. Update PatternRegistry.cpp (taxonomy metadata)
4. Update documentation (effect migration notes)

**CLI Commands**:

```bash
# Initialize hierarchical swarm
npx claude-flow@v3alpha swarm_init \
  --pattern hierarchical \
  --queen-agent embedded \
  --task "Migrate legacy effect to IEffect: <EffectName>"

# Spawn specialised workers
npx claude-flow@v3alpha agent_spawn \
  --agent embedded \
  --task "Create IEffect class: firmware/v2/src/effects/ieffect/<EffectName>Effect.{h,cpp}"

npx claude-flow@v3alpha agent_spawn \
  --agent embedded \
  --task "Register effect in CoreEffects.cpp (stable ID: <effectId>)"

npx claude-flow@v3alpha agent_spawn \
  --agent embedded \
  --task "Update PatternRegistry.cpp: family/tags/story metadata for <effectId>"

npx claude-flow@v3alpha agent_spawn \
  --agent documentation \
  --task "Document migration in docs/effects/<EffectName>_MIGRATION.md"
```

**Memory Search**:

```bash
# Find previous migration patterns
npx claude-flow@v3alpha memory_search "effect_migration IEffect PatternRegistry stable_id"
```

**Acceptance Criteria**:
- Centre-origin compliance (LEDs 79/80)
- No heap allocation in `render()` method
- Effect ID matches PatternRegistry index
- `validate <effectId>` passes (centre-origin, hue-span, FPS, heap-delta)
- Documentation uses British English

---

### API Change Swarm

**Use Case**: Add new REST endpoint or modify existing handler

**Pattern**: Hierarchical (queen ensures handler → OpenAPI → client → docs ordering)

**Steps**:
1. Implement handler in `firmware/v2/src/network/webserver/handlers/`
2. Register route in `WebServer.cpp` (OpenAPI spec update)
3. Update dashboard client in `lightwave-dashboard/src/services/v2/api.ts`
4. Update API documentation in `docs/api/`

**CLI Commands**:

```bash
# Initialize hierarchical swarm for API changes
npx claude-flow@v3alpha swarm_init \
  --pattern hierarchical \
  --queen-agent api \
  --task "Add REST endpoint: <Method> /api/v1/<resource>/<action>"

# Sequential worker spawn (order matters)
npx claude-flow@v3alpha agent_spawn \
  --agent api \
  --task "Implement handler: firmware/v2/src/network/webserver/handlers/<Resource>Handlers.{cpp,h}"

npx claude-flow@v3alpha agent_spawn \
  --agent api \
  --task "Register route in WebServer.cpp (OpenAPI spec: <Method> /api/v1/<resource>/<action>)"

npx claude-flow@v3alpha agent_spawn \
  --agent frontend \
  --task "Update dashboard client: lightwave-dashboard/src/services/v2/api.ts (add <method>Request function)"

npx claude-flow@v3alpha agent_spawn \
  --agent documentation \
  --task "Update API docs: docs/api/api-v2.md (add endpoint documentation)"
```

**Memory Search**:

```bash
# Find previous endpoint patterns
npx claude-flow@v3alpha memory_search "rest_endpoint_add handler OpenAPI dashboard_client"
```

**Acceptance Criteria**:
- Handler thread-safe (mutex protection if shared state)
- OpenAPI spec matches actual handler behaviour
- Dashboard client matches firmware API contract
- Response uses additive JSON only (backward compatible)
- Documentation uses British English

---

### Dashboard UI Feature Swarm

**Use Case**: Add new dashboard component or feature

**Pattern**: Mesh (parallel subtasks: component, test, accessibility, docs)

**Steps**:
1. Create component in `lightwave-dashboard/src/components/`
2. Add E2E test in `lightwave-dashboard/e2e/`
3. Verify accessibility (`accessibility.spec.ts`)
4. Update UI documentation in `lightwave-dashboard/docs/`

**CLI Commands**:

```bash
# Initialize mesh swarm for parallel UI work
npx claude-flow@v3alpha swarm_init \
  --pattern mesh \
  --task "Add dashboard component: <ComponentName>"

# Parallel worker spawn (independent subtasks)
npx claude-flow@v3alpha agent_spawn \
  --agent frontend \
  --task "Create component: lightwave-dashboard/src/components/<ComponentName>.tsx"

npx claude-flow@v3alpha agent_spawn \
  --agent frontend \
  --task "Add E2E test: lightwave-dashboard/e2e/<component-name>.spec.ts"

npx claude-flow@v3alpha agent_spawn \
  --agent frontend \
  --task "Verify accessibility: lightwave-dashboard/e2e/accessibility.spec.ts (add <ComponentName> checks)"

npx claude-flow@v3alpha agent_spawn \
  --agent documentation \
  --task "Update UI docs: lightwave-dashboard/docs/<ComponentName>_GUIDE.md"
```

**Memory Search**:

```bash
# Find component patterns
npx claude-flow@v3alpha memory_search "component_add E2E test accessibility UI_standards"
```

**Acceptance Criteria**:
- Component uses Tailwind classes (no inline responsive styles)
- E2E test passes (`npm run test:e2e`)
- Accessibility checks pass (WCAG 2.1 AA)
- Focus management implemented (keyboard navigation)
- No CDN dependencies (all assets bundled)

---

### Audio Documentation Swarm

**Use Case**: Add or update audio-reactive pipeline documentation

**Pattern**: Hierarchical (queen coordinates pipeline update → diagrams → validation references)

**Steps**:
1. Update pipeline documentation in `firmware/v2/docs/audio-visual/`
2. Create/update data flow diagrams
3. Update validation references in related docs

**CLI Commands**:

```bash
# Initialize hierarchical swarm for audio docs
npx claude-flow@v3alpha swarm_init \
  --pattern hierarchical \
  --queen-agent documentation \
  --task "Update audio pipeline documentation: <DocumentName>"

# Sequential worker spawn
npx claude-flow@v3alpha agent_spawn \
  --agent documentation \
  --task "Update pipeline doc: firmware/v2/docs/audio-visual/<DocumentName>.md (follow AUDIO_PIPELINE_DATA_FLOW.md format)"

npx claude-flow@v3alpha agent_spawn \
  --agent documentation \
  --task "Create/update data flow diagrams (Mermaid format, follow AUDIO_PIPELINE_DATA_FLOW.md structure)"

npx claude-flow@v3alpha agent_spawn \
  --agent documentation \
  --task "Update cross-references: docs/audio-visual/AUDIO_VISUAL_SEMANTIC_MAPPING.md, docs/architecture/**"
```

**Memory Search**:

```bash
# Find audio doc patterns
npx claude-flow@v3alpha memory_search "audio_doc_structure pipeline_data_flow layer_audit"
```

**Acceptance Criteria**:
- Follows `AUDIO_PIPELINE_DATA_FLOW.md` structure (signal path, timing, resource metrics)
- Documents complete layer audit (ALL layers of effect)
- Avoids rigid frequency→visual bindings (musical saliency emphasis)
- Uses British English (centre/colour/initialise)
- Cross-references resolve (internal doc links valid)

---

## Generic Templates

### Parallel Documentation Update

**Use Case**: Update documentation across multiple domains simultaneously

**Pattern**: Mesh (independent doc files)

**CLI Commands**:

```bash
# Initialize mesh swarm for parallel docs
npx claude-flow@v3alpha swarm_init \
  --pattern mesh \
  --task "Update documentation across domains: <Topic>"

# Parallel agent spawn
npx claude-flow@v3alpha agent_spawn \
  --agent documentation \
  --task "Update firmware docs: docs/effects/<Topic>.md"

npx claude-flow@v3alpha agent_spawn \
  --agent documentation \
  --task "Update API docs: docs/api/<Topic>.md"

npx claude-flow@v3alpha agent_spawn \
  --agent documentation \
  --task "Update dashboard docs: lightwave-dashboard/docs/<Topic>.md"
```

**Acceptance Criteria**:
- British English verified across all docs
- Cross-references updated consistently
- Spelling consistency (centre/colour/initialise)

---

### Protected File Edit Protocol

**Use Case**: Modify protected files (WiFiManager, WebServer, AudioActor)

**Pattern**: Single agent with review gate (no swarm - sequential only)

**CLI Commands**:

```bash
# Protected file edit requires review gate
npx claude-flow@v3alpha hooks_route \
  --pattern "firmware/v2/src/network/WiFiManager.*" \
  --agent network \
  --protection-level CRITICAL \
  --review-gate true

# Single agent with mandatory reads
npx claude-flow@v3alpha agent_spawn \
  --agent network \
  --task "Edit WiFiManager: <specific_change>" \
  --mandatory-reads ".claude/harness/PROTECTED_FILES.md,WifiManager.cpp" \
  --pre-commit-tests "wifi_manager_tests"
```

**Acceptance Criteria**:
- Entire file read and understood
- `.claude/harness/PROTECTED_FILES.md` reviewed
- Pre-commit tests pass (before AND after changes)
- Protected file protocols followed (EventGroup bit clearing, mutex protection)

---

## Template Customization

### Adding New Templates

When adding new swarm templates:

1. **Identify Pattern**: Hierarchical (ordering matters) or Mesh (parallel independent tasks)
2. **Define Steps**: List sequential or parallel subtasks
3. **Specify Agents**: Map each step to domain specialist (embedded/api/frontend/documentation)
4. **Memory Search**: Define pattern memory keys for future reuse
5. **Acceptance Criteria**: List domain-specific validation requirements

### Template Variables

Templates use placeholders that should be replaced:
- `<EffectName>`: Name of effect being migrated
- `<effectId>`: Stable numeric effect ID
- `<Method>`: HTTP method (GET/POST/PUT/DELETE)
- `<resource>`: API resource name
- `<action>`: API action name
- `<ComponentName>`: React component name
- `<DocumentName>`: Documentation filename
- `<Topic>`: Documentation topic/subject

---

## Related Documentation

- **[agent-routing.md](./agent-routing.md)** - Routing rules and acceptance criteria per domain
- **[pair-programming.md](./pair-programming.md)** - Pair programming modes for swarm execution
- **[validation-checklist.md](./validation-checklist.md)** - Pre-commit and post-commit gates

---

*These templates accelerate common workflows by encoding successful patterns in swarm configurations, reducing setup time and ensuring consistency across similar tasks.*
