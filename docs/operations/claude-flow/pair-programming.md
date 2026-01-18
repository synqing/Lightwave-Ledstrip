# Pair Programming Modes

**Version:** 1.0  
**Last Updated:** 2026-01-18  
**Status:** Operational Guide

This document provides guidance on when and how to use Claude-Flow pair programming modes for Lightwave-Ledstrip development, with mode selection criteria and domain-specific recommendations.

---

## Pair Programming Overview

Claude-Flow pair programming enables collaborative coding between human developers and AI agents, with role switching, TDD support, and real-time verification. Different modes optimise for different workflows and risk profiles.

### Available Modes

| Mode | Human Role | AI Role | Best For |
|------|------------|---------|----------|
| **Navigator** | Directing, reviewing | Writing code | High-risk changes, protected files, effect registration |
| **TDD** | Writing tests | Implementing | New IEffect classes, REST endpoints, dashboard components |
| **Switch** | Alternating | Alternating | General feature development, documentation updates |
| **Driver** | Writing code | Reviewing, suggesting | Learning, exploration, experimental work |

---

## Mode Selection by Domain

### Firmware Effects Domain

**High-Risk Scenarios** (Use **Navigator Mode**):

1. **Render Loop Modifications**
   - **Why**: No heap allocation verification requires real-time review
   - **Example**: Optimising effect performance, adding new render logic
   - **AI Role**: Implement render loop changes, verify no `new`/`malloc`/`std::vector` growth
   - **Human Role**: Review code for heap allocation, verify 120+ FPS target

2. **Effect Registration Changes**
   - **Why**: Effect ID stability is critical (must match PatternRegistry indices)
   - **Example**: Adding new IEffect, migrating legacy effect
   - **AI Role**: Register effect in CoreEffects.cpp, update PatternRegistry.cpp
   - **Human Role**: Verify ID stability, check PatternRegistry index alignment

3. **Centre-Origin Math Changes**
   - **Why**: Centre point logic (LEDs 79/80) is load-bearing for all effects
   - **Example**: Modifying CENTER_LEFT/CENTER_RIGHT helpers, signed position functions
   - **AI Role**: Implement centre-origin math changes
   - **Human Role**: Verify centre compliance, test with `validate <effectId>`

**Test-First Scenarios** (Use **TDD Mode**):

1. **New IEffect Implementation**
   - **Why**: Test-first ensures centre-origin compliance before render logic
   - **Example**: Creating new native IEffect class
   - **Human Role**: Write test for centre-origin, hue-span, FPS, heap-delta checks
   - **AI Role**: Implement IEffect to pass tests

2. **Effect Migration (Legacy → IEffect)**
   - **Why**: Preserve existing behaviour while migrating to new interface
   - **Example**: Migrating `effectFire()` to `FireEffect` class
   - **Human Role**: Write migration test (centre-origin, visual match)
   - **AI Role**: Implement IEffect class to match legacy behaviour

**CLI Commands**:

```bash
# Navigator mode for render loop changes
npx claude-flow@v3alpha pair start \
  --mode navigator \
  --task "Optimise effect render loop: <EffectName> (verify no heap allocation)"

# TDD mode for new IEffect
npx claude-flow@v3alpha pair start \
  --mode tdd \
  --task "Implement new IEffect: <EffectName> (test-first centre-origin compliance)"
```

---

### Network/API Domain

**High-Risk Scenarios** (Use **Navigator Mode**):

1. **Protected File Edits**
   - **Why**: WiFiManager EventGroup bugs, WebServer thread safety are recurring issues
   - **Example**: Fixing WiFiManager state machine, adding WebSocket handler synchronization
   - **AI Role**: Implement protected file changes following review gate protocols
   - **Human Role**: Review EventGroup bit clearing, mutex usage, thread safety

2. **Rate Limiting Changes**
   - **Why**: Non-blocking behaviour and timeout handling are critical
   - **Example**: Modifying RateLimiter logic, adding new rate limit rules
   - **AI Role**: Implement rate limiting changes
   - **Human Role**: Verify non-blocking behaviour, timeout correctness

**Test-First Scenarios** (Use **TDD Mode**):

1. **New REST Endpoint**
   - **Why**: Test defines API contract before handler implementation
   - **Example**: Adding `GET /api/v1/effects/metadata?id=N`
   - **Human Role**: Write integration test (request/response format, error cases)
   - **AI Role**: Implement handler to match test contract

2. **WebSocket Command Handler**
   - **Why**: Real-time behaviour requires test-first validation
   - **Example**: Adding new WebSocket command type
   - **Human Role**: Write WebSocket test (command format, response, error handling)
   - **AI Role**: Implement WsCommandRouter handler

**CLI Commands**:

```bash
# Navigator mode for protected files
npx claude-flow@v3alpha pair start \
  --mode navigator \
  --task "Edit WiFiManager: fix EventGroup bit clearing in setState()"

# TDD mode for REST endpoint
npx claude-flow@v3alpha pair start \
  --mode tdd \
  --task "Add REST endpoint: GET /api/v1/effects/metadata?id=N (test-first API contract)"
```

---

### Dashboard UI Domain

**High-Risk Scenarios** (Use **Navigator Mode**):

1. **Focus Management Changes**
   - **Why**: Keyboard navigation and accessibility are WCAG 2.1 AA requirements
   - **Example**: Adding skip links, modifying focus trap logic
   - **AI Role**: Implement focus management changes
   - **Human Role**: Verify keyboard navigation, screen reader compatibility

2. **State Management Refactoring**
   - **Why**: React state updates affect multiple components
   - **Example**: Refactoring `state/v2.tsx`, modifying WebSocket state sync
   - **AI Role**: Implement state management changes
   - **Human Role**: Verify component updates, WebSocket sync correctness

**Test-First Scenarios** (Use **TDD Mode**):

1. **New Dashboard Component**
   - **Why**: Accessibility and E2E tests define component contract
   - **Example**: Adding new tab component, creating ZoneEditor enhancement
   - **Human Role**: Write E2E test, accessibility test (WCAG 2.1 AA checks)
   - **AI Role**: Implement component to pass tests

2. **UI Interaction Features**
   - **Why**: User interactions require E2E test coverage
   - **Example**: Adding drag-and-drop, implementing slider controls
   - **Human Role**: Write Playwright E2E test (interaction flow, visual regression)
   - **AI Role**: Implement interaction feature

**CLI Commands**:

```bash
# Navigator mode for focus management
npx claude-flow@v3alpha pair start \
  --mode navigator \
  --task "Update focus management: add skip links (verify keyboard navigation)"

# TDD mode for new component
npx claude-flow@v3alpha pair start \
  --mode tdd \
  --task "Add dashboard component: <ComponentName> (test-first accessibility compliance)"
```

---

### Documentation Domain

**General Scenarios** (Use **Switch Mode**):

1. **Documentation Updates with Code Changes**
   - **Why**: Balanced collaboration for prose and code examples
   - **Example**: Updating API docs with new endpoint, adding code examples
   - **Human Role**: Write/revise prose, verify British English
   - **AI Role**: Generate code examples, verify technical accuracy

2. **Cross-Reference Maintenance**
   - **Why**: Coordinated updates across multiple docs
   - **Example**: Updating doc links after restructuring, fixing broken references
   - **Human Role**: Identify broken links, verify link targets
   - **AI Role**: Update cross-references, verify link resolution

**Exploratory Scenarios** (Use **Driver Mode**):

1. **New Documentation Structure**
   - **Why**: Human exploration before AI codification
   - **Example**: Designing new doc format, creating documentation templates
   - **Human Role**: Design structure, write initial draft
   - **AI Role**: Review for British English, suggest improvements, verify consistency

**CLI Commands**:

```bash
# Switch mode for docs with code
npx claude-flow@v3alpha pair start \
  --mode switch \
  --task "Update API docs: add <Endpoint> documentation with code examples"

# Driver mode for exploratory docs
npx claude-flow@v3alpha pair start \
  --mode driver \
  --task "Design new documentation template: <TemplateName>"
```

---

## When to Use Swarms vs Pair Programming

**Use Pair Programming When:**
- Task is sequential (one file/component at a time)
- High-risk change requires real-time review (protected files, render loops)
- Test-first approach is preferred (new IEffect, REST endpoint, component)
- Exploratory/experimental work (no clear pattern yet)

**Use Swarms When:**
- Task spans 3+ domains (firmware + API + dashboard + docs)
- Multiple independent subtasks can run in parallel (mesh pattern)
- Quality gates require coordination (hierarchical pattern)
- Established pattern exists (effect migration, API change)

**Combination Approach:**
- Use **pair programming within swarm workers**: Individual swarm subtasks can use Navigator/TDD modes for high-risk changes
- Example: Effect migration swarm with TDD mode for IEffect class creation

---

## Mode Selection Checklist

**Use Navigator Mode if:**
- [ ] Protected file edit (WiFiManager, WebServer, AudioActor)
- [ ] Render loop modification (heap allocation verification)
- [ ] Effect registration change (ID stability critical)
- [ ] Focus management change (accessibility critical)
- [ ] Thread safety concern (mutex protection required)

**Use TDD Mode if:**
- [ ] New IEffect implementation (centre-origin test-first)
- [ ] New REST endpoint (API contract test-first)
- [ ] New dashboard component (accessibility test-first)
- [ ] WebSocket command handler (real-time test-first)

**Use Switch Mode if:**
- [ ] General feature development (balanced collaboration)
- [ ] Documentation with code changes (prose + examples)
- [ ] Cross-reference maintenance (coordinated updates)

**Use Driver Mode if:**
- [ ] Learning/exploration (experimental work)
- [ ] New documentation structure (human design, AI review)
- [ ] Initial prototype (rapid iteration)

---

## Related Documentation

- **[overview.md](./overview.md)** - Strategic integration overview
- **[agent-routing.md](./agent-routing.md)** - Domain-specific routing rules
- **[swarm-templates.md](./swarm-templates.md)** - Swarm configurations (can combine with pair programming)

---

*Pair programming modes optimise human-AI collaboration for different risk profiles and workflows, ensuring high-risk changes receive real-time review while test-first development accelerates new feature delivery.*
