# Agent Workflow Architecture

**Version:** 1.0.0  
**Last Updated:** 2025-01-XX  
**Purpose:** Comprehensive workflow architecture specification for multi-agent parallel execution

---

## Overview

This document defines the multi-threaded workflow architecture for agent coordination in the Lightwave-Ledstrip project. It specifies how agents are selected, dispatched, and coordinated to achieve optimal resource utilization through parallel execution.

---

## Architecture Principles

### 1. Pre-Task Agent Selection (MANDATORY)

**Before ANY task initiation:**

1. **Complete Inventory Review** - Review `.claude/agents/README.md`
2. **Domain Expertise Analysis** - Analyze each agent's capabilities
3. **Strategic Selection** - Select optimal agent combination
4. **Parallel Execution Evaluation** - Determine if tasks can be parallelized

**Reference:** `CLAUDE.md` - Pre-Task Agent Selection Protocol

### 2. Parallel Execution (MANDATORY When Applicable)

**MANDATORY when:**
- 3+ independent tasks exist
- Tasks can be divided into independent components
- No shared state conflicts
- Different subsystems or file sets involved

**Reference:** `.claude/skills/dispatching-parallel-agents/SKILL.md`

### 3. Task Decomposition (MANDATORY for Complex Tasks)

**MANDATORY action:**
- Divide complex tasks into parallelizable components
- Identify independent subsystems
- Create focused agent tasks
- Plan parallel dispatch

---

## Workflow Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    PRE-TASK PROTOCOL                            │
│  (MANDATORY - Before ANY task initiation)                       │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│  Step 1: Review Complete Agent Inventory                        │
│  - Read .claude/agents/README.md                                │
│  - Identify all 10 specialist agents                            │
│  - Understand agent capabilities                                │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│  Step 2: Analyze Domain Expertise                               │
│  - Map task requirements to agent domains                       │
│  - Identify primary and secondary agents                        │
│  - Determine complexity levels                                  │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│  Step 3: Strategic Agent Selection                               │
│  - Select optimal agent combination                             │
│  - Determine if multiple agents needed                          │
│  - Document selection rationale                                 │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│  Step 4: Evaluate Parallel Execution Potential                   │
│  - Can tasks be parallelized?                                  │
│  - Are components independent?                                 │
│  - No shared state conflicts?                                  │
└─────────────────────────────────────────────────────────────────┘
                            ↓
        ┌───────────────────┴───────────────────┐
        │                                       │
        ↓                                       ↓
┌───────────────────────┐         ┌───────────────────────┐
│  PARALLEL EXECUTION   │         │ SEQUENTIAL EXECUTION  │
│  (MANDATORY if        │         │ (Required if          │
│   applicable)         │         │  dependencies exist)  │
└───────────────────────┘         └───────────────────────┘
        │                                       │
        ↓                                       ↓
┌─────────────────────────────────────────────────────────────────┐
│  Multi-Agent Coordination Patterns                             │
│  - Pattern 1: Sequential Coordination                          │
│  - Pattern 2: Parallel Coordination                            │
│  - Pattern 3: Hybrid Coordination                              │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│  Task Execution & Review                                        │
│  - Execute tasks (parallel or sequential)                       │
│  - Code review between/after tasks                             │
│  - Integration and verification                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Multi-Threaded Workflow Patterns

### Pattern 1: Sequential Coordination

**Use when:** Tasks are dependent or must execute in order

```
┌─────────────────┐
│  Agent A        │ → Hardware changes
│  (embedded)     │
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│  Agent B        │ → Effect updates using new hardware
│  (visual-fx)    │
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│  Agent C        │ → API exposure of new features
│  (network-api)  │
└─────────────────┘
```

**Characteristics:**
- Tasks execute one after another
- Output of Agent A feeds into Agent B
- Shared state or dependencies exist
- Ordered execution required

### Pattern 2: Parallel Coordination

**Use when:** Tasks are independent and can execute concurrently

```
┌─────────────────┐
│  Agent A        │ → New effect implementation
│  (visual-fx)    │
└─────────────────┘
         │
         │ (concurrent)
         │
┌─────────────────┐
│  Agent B        │ → New palette creation
│  (palette)      │
└─────────────────┘
         │
         │ (concurrent)
         │
┌─────────────────┐
│  Agent C        │ → API endpoint updates
│  (network-api)  │
└─────────────────┘
         │
         ↓
┌─────────────────┐
│  Integration    │ → Review & merge all changes
│  & Review       │
└─────────────────┘
```

**Characteristics:**
- Tasks execute simultaneously
- No dependencies between tasks
- Independent file sets or subsystems
- Optimal resource utilization

### Pattern 3: Hybrid Coordination

**Use when:** Some tasks are independent, others are dependent

```
Phase 1 (Parallel):
┌─────────────────┐
│  Agent A        │ → Hardware optimization
│  (embedded)     │
└─────────────────┘
         │
         │ (concurrent)
         │
┌─────────────────┐
│  Agent B        │ → Effect improvements
│  (visual-fx)    │
└─────────────────┘
         │
         ↓
Phase 2 (Sequential):
┌─────────────────┐
│  Agent C        │ → API integration
│  (network-api)  │   (depends on A & B)
└─────────────────┘
```

**Characteristics:**
- Initial parallel phase
- Followed by sequential phase
- Dependencies exist but can be delayed
- Balanced resource utilization

---

## Task Decomposition Framework

### Step 1: Identify Task Components

**For complex tasks, decompose into:**

1. **Independent Components:**
   - Subsystem A (e.g., network layer)
   - Subsystem B (e.g., visual effects)
   - Subsystem C (e.g., serial interface)

2. **Component Characteristics:**
   - Self-contained (all context provided)
   - Independent (no shared state)
   - Clearly scoped (specific goal)

### Step 2: Map Components to Agents

**Use agent capability matrix:**

| Component | Domain | Primary Agent | Secondary Agent |
|-----------|--------|---------------|-----------------|
| Network API | Network | `network-api-engineer` | - |
| Visual Effect | Effects | `visual-fx-architect` | `palette-specialist` |
| Hardware Config | Hardware | `embedded-system-engineer` | - |

### Step 3: Plan Parallel Dispatch

**Create dispatch plan:**

```typescript
// Independent components → Parallel dispatch
Task("Component A: Network API changes", network-api-engineer)
Task("Component B: Visual effect updates", visual-fx-architect)
Task("Component C: Hardware optimization", embedded-system-engineer)
// All three execute concurrently
```

---

## Resource Utilization Optimization

### Multi-Threaded Execution Benefits

1. **Time Savings:**
   - 3 independent tasks: Sequential = 3× time, Parallel = 1× time
   - Optimal resource utilization across agent capabilities

2. **Focus Enhancement:**
   - Each agent has narrow scope
   - Less context to track
   - Reduced cognitive load

3. **Quality Improvement:**
   - Specialized agents for specialized tasks
   - Domain expertise applied correctly
   - Better outcomes per task

### Optimal Resource Utilization

**Parallel Execution Metrics:**

| Scenario | Sequential Time | Parallel Time | Efficiency Gain |
|----------|----------------|---------------|-----------------|
| 3 independent tasks | 3× | 1× | 3× faster |
| 5 independent tasks | 5× | 1× | 5× faster |
| Mixed (3 parallel + 2 sequential) | 5× | 3× | 1.67× faster |

---

## Workflow Implementation Checklist

### Pre-Task Protocol (MANDATORY)

- [ ] Reviewed complete agent inventory (`.claude/agents/README.md`)
- [ ] Analyzed domain expertise of each agent
- [ ] Strategically selected optimal agent combination
- [ ] Evaluated parallel execution potential
- [ ] Reviewed available skills (`.claude/skills/`)

### Task Execution

- [ ] Identified independent task components
- [ ] Decomposed complex tasks into parallelizable components
- [ ] Created focused agent tasks with clear goals
- [ ] Dispatched agents (parallel or sequential as appropriate)
- [ ] Monitored execution progress

### Post-Task Review

- [ ] Reviewed each agent's summary
- [ ] Verified fixes/changes don't conflict
- [ ] Ran full test suite
- [ ] Integrated all changes
- [ ] Documented outcomes

---

## Exception Handling

### When Sequential Execution is Required

**Valid exceptions to parallel execution:**

1. **Tightly Coupled Tasks:**
   - Tasks share critical state
   - Output of one feeds directly into another
   - Ordered execution is necessary

2. **Shared State Dependencies:**
   - Agents would interfere with each other
   - Same files or resources needed
   - Race conditions possible

3. **Exploratory Debugging:**
   - Root cause unknown
   - Need full system context
   - Sequential investigation required

**Documentation Required:** If sequential execution is chosen, document why parallel execution was not possible.

---

## Version History

- **v1.0.0** (2025-01-XX): Initial workflow architecture specification
  - Defined multi-threaded workflow patterns
  - Created task decomposition framework
  - Specified resource utilization optimization
  - Added implementation checklist

---

## Related Documentation

- `CLAUDE.md` - Main agent guidance and pre-task protocol
- `.claude/agents/README.md` - Agent capability matrix
- `.claude/skills/dispatching-parallel-agents/SKILL.md` - Parallel execution protocol
- `.claude/skills/subagent-driven-development/SKILL.md` - Subagent workflow protocol
- `docs/AGENT_OPERATIONAL_PROTOCOL_AUDIT.md` - Complete protocol audit

---

**Remember:** This workflow architecture is MANDATORY. Always follow the pre-task protocol, evaluate parallel execution potential, and decompose complex tasks into parallelizable components for optimal resource utilization.

