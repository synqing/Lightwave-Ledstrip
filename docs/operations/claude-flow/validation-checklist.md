# Validation Checklist

**Version:** 1.0  
**Last Updated:** 2026-01-18  
**Status:** Operational Reference

This document provides a comprehensive validation checklist for Claude-Flow integration setup, runtime verification, and ongoing operational checks.

---

## Initial Setup Validation

### MCP Configuration

**Location**: `.cursor/mcp.json` (or Cursor global MCP config)

**Required Configuration**:

```json
{
  "mcpServers": {
    "claude-flow": {
      "command": "npx",
      "args": ["claude-flow@v3alpha", "mcp", "start"],
      "env": {
        "ANTHROPIC_API_KEY": "${ANTHROPIC_API_KEY}",
        "OPENAI_API_KEY": "${OPENAI_API_KEY}",
        "GEMINI_API_KEY": "${GEMINI_API_KEY}",
        "CLAUDE_FLOW_PROJECT_ROOT": "${PROJECT_ROOT}"
      }
    }
  }
}
```

**Validation Steps**:

1. **Check MCP Config Exists**
   ```bash
   # Check if .cursor/mcp.json exists (or global config)
   test -f .cursor/mcp.json && echo "MCP config found" || echo "MCP config missing"
   
   # Or check Cursor global config location
   # macOS: ~/Library/Application Support/Cursor/User/globalStorage/...
   # Windows: %APPDATA%\Cursor\User\globalStorage\...
   ```

2. **Verify Environment Variables**
   ```bash
   # Check required API keys are set (at least one LLM provider)
   test -n "$ANTHROPIC_API_KEY" && echo "ANTHROPIC_API_KEY set" || echo "ANTHROPIC_API_KEY missing"
   test -n "$OPENAI_API_KEY" && echo "OPENAI_API_KEY set" || echo "OPENAI_API_KEY missing"
   test -n "$GEMINI_API_KEY" && echo "GEMINI_API_KEY set" || echo "GEMINI_API_KEY missing"
   ```

3. **Test MCP Server Connection**
   ```bash
   # List available MCP servers (if claude command available)
   claude mcp list | grep claude-flow
   
   # Or verify via Cursor: Settings → MCP → Verify claude-flow server status
   ```

**Acceptance Criteria**:
- [ ] MCP config file exists and is valid JSON
- [ ] At least one LLM provider API key is set
- [ ] MCP server connection verified (claude-flow appears in server list)

---

### Claude-Flow Installation

**Required Installation**:

```bash
# Install Claude-Flow globally (or verify npx availability)
npm install -g claude-flow@v3alpha

# Or use npx (no global install needed)
npx claude-flow@v3alpha --version
```

**Validation Steps**:

1. **Check Claude-Flow Version**
   ```bash
   npx claude-flow@v3alpha --version
   # Expected: v3alpha or higher
   ```

2. **Verify Core Tools Available**
   ```bash
   # Test swarm_init
   npx claude-flow@v3alpha swarm_init --help
   
   # Test agent_spawn
   npx claude-flow@v3alpha agent_spawn --help
   
   # Test memory_search
   npx claude-flow@v3alpha memory_search --help
   
   # Test hooks_route
   npx claude-flow@v3alpha hooks_route --help
   ```

**Acceptance Criteria**:
- [ ] Claude-Flow v3alpha or higher installed (or npx accessible)
- [ ] `swarm_init`, `agent_spawn`, `memory_search`, `hooks_route` commands available
- [ ] All commands show help output (not "command not found")

---

### Tool Integration Verification

**MCP Tool Access**:

Once MCP server is connected, verify tool access via Cursor or Claude Desktop:

```bash
# If using Cursor with MCP integration:
# Tools should appear in tool picker:
# - swarm_init
# - agent_spawn
# - memory_search
# - hooks_route
# - memory_store
# - (170+ more tools)
```

**Validation Steps**:

1. **List Available Tools** (if MCP client supports)
   ```bash
   # Via MCP client (Cursor/Claude Desktop)
   # Tools should include claude-flow tools with "claude-flow" namespace
   ```

2. **Test Tool Execution** (if possible)
   ```bash
   # Test memory_search (read-only, safe)
   npx claude-flow@v3alpha memory_search "effect_migration"
   
   # Test hooks_route (read-only status check)
   npx claude-flow@v3alpha hooks_route --status
   ```

**Acceptance Criteria**:
- [ ] MCP tools visible in Cursor/Claude Desktop tool picker
- [ ] `memory_search` returns results (or empty, not error)
- [ ] `hooks_route --status` shows routing configuration (or empty config)

---

## Runtime Verification

### Agent Routing Verification

**Check Routing Rules**:

```bash
# List configured routing rules
npx claude-flow@v3alpha hooks_route --status

# Expected output should include routes for:
# - firmware/v2/src/effects/** → embedded/firmware agent
# - firmware/v2/src/network/** → api/network agent
# - lightwave-dashboard/** → frontend/ui agent
# - docs/** → documentation/docs agent
```

**Validation Steps**:

1. **Verify Domain Routing**
   ```bash
   # Check firmware effects routing
   npx claude-flow@v3alpha hooks_route --pattern "firmware/v2/src/effects/**" --query
   
   # Check network/API routing
   npx claude-flow@v3alpha hooks_route --pattern "firmware/v2/src/network/**" --query
   
   # Check dashboard routing
   npx claude-flow@v3alpha hooks_route --pattern "lightwave-dashboard/**" --query
   
   # Check documentation routing
   npx claude-flow@v3alpha hooks_route --pattern "docs/**" --query
   ```

2. **Verify Protected File Routing**
   ```bash
   # Check WiFiManager protection
   npx claude-flow@v3alpha hooks_route --pattern "firmware/v2/src/network/WiFiManager.*" --query
   # Expected: protection_level=CRITICAL, review_gate=true
   
   # Check WebServer protection
   npx claude-flow@v3alpha hooks_route --pattern "firmware/v2/src/network/WebServer.*" --query
   # Expected: protection_level=HIGH, review_gate=true
   ```

**Acceptance Criteria**:
- [ ] Domain routing rules configured (effects → embedded, network → api, dashboard → frontend, docs → documentation)
- [ ] Protected files have protection_level and review_gate set
- [ ] Routing queries return expected agent assignments

---

### Memory Search Verification

**Test Pattern Memory**:

```bash
# Search for common patterns
npx claude-flow@v3alpha memory_search "effect_migration IEffect PatternRegistry"

npx claude-flow@v3alpha memory_search "centre_origin CENTER_LEFT CENTER_RIGHT"

npx claude-flow@v3alpha memory_search "british_english centre colour initialise"
```

**Validation Steps**:

1. **Verify Memory Search Works**
   ```bash
   # Test memory search (should return results or empty, not error)
   npx claude-flow@v3alpha memory_search "test_pattern" 2>&1 | head -5
   # Expected: Results list or "No results found" (not "command not found" or error)
   ```

2. **Verify Pattern Storage** (after first successful task)
   ```bash
   # After completing an effect migration, search for pattern
   npx claude-flow@v3alpha memory_search "effect_migration"
   # Expected: Should find pattern from previous migration (or empty if first time)
   ```

**Acceptance Criteria**:
- [ ] Memory search returns results (or empty, not error)
- [ ] Pattern storage works (after first successful task, patterns are retrievable)

---

### Swarm Execution Verification

**Test Swarm Initialization**:

```bash
# Test swarm_init (dry-run or minimal task)
npx claude-flow@v3alpha swarm_init \
  --pattern hierarchical \
  --queen-agent embedded \
  --task "Test swarm: validate routing" \
  --dry-run
```

**Validation Steps**:

1. **Verify Swarm Init Works**
   ```bash
   # Test swarm_init with dry-run (should not execute, just validate)
   npx claude-flow@v3alpha swarm_init \
     --pattern hierarchical \
     --queen-agent embedded \
     --task "Test: validate routing" \
     --dry-run 2>&1
   # Expected: Validation output (pattern accepted, agents available) or dry-run confirmation
   ```

2. **Verify Agent Spawn Works**
   ```bash
   # Test agent_spawn (dry-run or minimal task)
   npx claude-flow@v3alpha agent_spawn \
     --agent embedded \
     --task "Test: validate agent" \
     --dry-run 2>&1
   # Expected: Agent spawn validation (agent found, task queued) or dry-run confirmation
   ```

**Acceptance Criteria**:
- [ ] `swarm_init` validates configuration (pattern, agents available)
- [ ] `agent_spawn` validates agent availability (agent found, not "unknown agent")
- [ ] Dry-run mode works (no actual execution, just validation)

---

## Domain-Specific Validation

### Firmware Effects Domain

**Validation Commands**:

```bash
# Serial: s (FPS/CPU check)
# Serial: validate <effectId> (centre-origin, hue-span, FPS, heap-delta)
# Build: cd firmware/v2 && pio run -e esp32dev_audio
```

**Acceptance Criteria**:
- [ ] Effect migration: `validate <effectId>` passes (centre-origin, hue-span, FPS, heap-delta)
- [ ] Render loop changes: `s` shows acceptable FPS (≥120)
- [ ] Centre-origin compliance: Effects originate from LEDs 79/80, propagate outward
- [ ] No heap allocation: No `new`/`malloc`/`std::vector` growth in `render()`
- [ ] No rainbows: No full hue-wheel sweeps

---

### Network/API Domain

**Validation Commands**:

```bash
# Build: cd firmware/v2 && pio run -e esp32dev_audio -t upload
# Serial: Verify WiFi connects without "IP: 0.0.0.0" appearing first
# API: GET /api/v1/effects (verify backward compatibility)
```

**Acceptance Criteria**:
- [ ] Protected file edits: All related unit tests pass (before AND after)
- [ ] API changes: Backward compatibility verified (GET /api/v1/effects unchanged)
- [ ] WiFiManager: WiFi connects without "IP: 0.0.0.0" appearing first
- [ ] Thread safety: No race conditions (WebSocket handlers, rate limiting)
- [ ] Additive JSON only: API responses extend fields only

---

### Dashboard UI Domain

**Validation Commands**:

```bash
# E2E: npm run test:e2e (Playwright)
# Accessibility: npm run test:e2e -- accessibility.spec.ts
# Lint: npm run lint (ESLint, no-inline-responsive-styles)
# Build: npm run build (Vite)
```

**Acceptance Criteria**:
- [ ] E2E tests pass (`npm run test:e2e`)
- [ ] Accessibility checks pass (`accessibility.spec.ts`)
- [ ] No inline responsive styles (`npm run lint`)
- [ ] Focus management implemented (keyboard navigation)
- [ ] No CDN dependencies (all assets bundled)

---

### Documentation Domain

**Validation Commands**:

```bash
# Spell check: grep -ri "center\|color\|initialize" docs/ (should be empty except for code)
# Link check: Verify internal doc links resolve
# Structure check: Audio docs follow AUDIO_PIPELINE_DATA_FLOW.md format
```

**Acceptance Criteria**:
- [ ] British English verified (no `center`/`color`/`initialize` in prose)
- [ ] Cross-references resolve (internal links valid)
- [ ] Audio docs follow `AUDIO_PIPELINE_DATA_FLOW.md` structure
- [ ] Spelling consistency (centre/colour/initialise throughout)

---

## Ongoing Operational Checks

### Weekly Checks

1. **Memory Search Hit Rate**
   - Review `memory_search` queries vs new pattern creation
   - Target: >50% hit rate (patterns reused, not recreated)

2. **Agent Routing Accuracy**
   - Review agent assignments vs task domains
   - Target: >90% correct routing (tasks reach correct specialists)

3. **Protected File Safety**
   - Review protected file edits (WiFiManager, WebServer, AudioActor)
   - Target: Zero EventGroup bugs, zero race conditions

4. **Invariant Violation Rate**
   - Review centre-origin, heap allocation, British English violations
   - Target: Zero violations (caught by routing/gates)

### Monthly Reviews

1. **Swarm Template Effectiveness**
   - Review swarm completion time (parallel vs sequential)
   - Target: 30-50% time reduction vs sequential execution

2. **Pattern Memory Growth**
   - Review pattern storage (new patterns added vs reused)
   - Target: Diminishing pattern creation (learning curve)

3. **Documentation Consistency**
   - Review British English, spelling, cross-references
   - Target: Zero American spellings in prose, all links valid

---

## Troubleshooting

### MCP Server Not Connecting

**Symptoms**: Tools not visible in Cursor/Claude Desktop

**Checks**:
1. Verify MCP config file exists and is valid JSON
2. Check API keys are set (at least one LLM provider)
3. Verify `npx claude-flow@v3alpha mcp start` runs without errors
4. Check Cursor/Claude Desktop MCP server logs

**Resolution**:
- Fix JSON syntax errors in MCP config
- Set missing API keys (ANTHROPIC_API_KEY, OPENAI_API_KEY, or GEMINI_API_KEY)
- Restart Cursor/Claude Desktop after MCP config changes

---

### Agent Routing Not Working

**Symptoms**: Tasks not reaching correct specialists

**Checks**:
1. Verify `hooks_route --status` shows routing rules
2. Check routing pattern matches file paths (wildcards correct)
3. Verify agent types are valid (embedded/api/frontend/documentation)

**Resolution**:
- Add missing routing rules via `hooks_route --pattern <pattern> --agent <agent>`
- Fix pattern wildcards (use `**` for recursive, `*` for single-level)
- Verify agent types match Claude-Flow agent registry

---

### Memory Search Returns Empty

**Symptoms**: `memory_search` returns no results

**Checks**:
1. Verify pattern memory is enabled (not disabled)
2. Check if patterns have been stored (first successful task completed)
3. Verify search query matches stored pattern keys

**Resolution**:
- Wait for first successful task (patterns stored after completion)
- Use broader search terms (patterns indexed by keywords)
- Check pattern memory storage location (disk space, permissions)

---

## Related Documentation

- **[overview.md](./overview.md)** - Strategic integration overview
- **[agent-routing.md](./agent-routing.md)** - Routing rules and acceptance criteria
- **[swarm-templates.md](./swarm-templates.md)** - Reusable swarm configurations
- **[pair-programming.md](./pair-programming.md)** - Pair programming mode selection

---

*This validation checklist ensures Claude-Flow integration is correctly configured and operational, with ongoing checks to maintain routing accuracy, pattern memory effectiveness, and invariant enforcement.*
