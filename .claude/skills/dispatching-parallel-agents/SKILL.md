---
name: dispatching-parallel-agents
version: 2.0.0
description: MANDATORY protocol for parallel agent execution when facing 3+ independent failures or tasks that can be investigated without shared state or dependencies - dispatches multiple Claude agents to investigate and fix independent problems concurrently
---

# Dispatching Parallel Agents

**Protocol Version:** 2.0.0  
**Status:** MANDATORY  
**Last Updated:** 2025-01-XX

## Overview

When you have multiple unrelated failures (different test files, different subsystems, different bugs), investigating them sequentially wastes time. Each investigation is independent and can happen in parallel.

**Core principle:** Dispatch one agent per independent problem domain. Let them work concurrently.

**MANDATORY REQUIREMENT:** You MUST use parallel execution when:
- 3+ independent failures exist
- Tasks can be divided into independent components
- No shared state conflicts
- Different subsystems or file sets are involved

## When to Use (MANDATORY)

```dot
digraph when_to_use {
    "Multiple failures?" [shape=diamond];
    "Are they independent?" [shape=diamond];
    "Single agent investigates all" [shape=box];
    "One agent per problem domain" [shape=box];
    "Can they work in parallel?" [shape=diamond];
    "Sequential agents" [shape=box];
    "Parallel dispatch [MANDATORY]" [shape=box, style="bold"];

    "Multiple failures?" -> "Are they independent?" [label="yes"];
    "Are they independent?" -> "Single agent investigates all" [label="no - related"];
    "Are they independent?" -> "Can they work in parallel?" [label="yes"];
    "Can they work in parallel?" -> "Parallel dispatch [MANDATORY]" [label="yes"];
    "Can they work in parallel?" -> "Sequential agents" [label="no - shared state"];
}
```

**MANDATORY - You MUST use parallel execution when:**
- 3+ test files failing with different root causes
- Multiple subsystems broken independently
- Each problem can be understood without context from others
- No shared state between investigations
- Complex tasks can be divided into independent components
- Different file sets or subsystems are involved

**Exception - Sequential execution required when:**
- Failures are related (fix one might fix others)
- Need to understand full system state
- Agents would interfere with each other
- Shared state dependencies exist

## The Pattern (MANDATORY)

### 1. Identify Independent Domains (MANDATORY)

**MANDATORY ACTION:** Group failures or tasks by what's broken or what needs to be done:
- File A tests: Tool approval flow
- File B tests: Batch completion behavior
- File C tests: Abort functionality
- Subsystem X: Network configuration
- Subsystem Y: Visual effects rendering

Each domain is independent - fixing tool approval doesn't affect abort tests.

**MANDATORY:** You MUST identify all independent domains before proceeding.

### 2. Divide Complex Tasks into Parallelizable Components (MANDATORY)

**MANDATORY ACTION:** For complex tasks, you MUST decompose them into parallelizable components:

- **Component 1:** Independent subsystem A
- **Component 2:** Independent subsystem B
- **Component 3:** Independent test file C

**Each component must be:**
- Independent (no shared state)
- Self-contained (all context provided)
- Clearly scoped (specific goal)

### 3. Create Focused Agent Tasks (MANDATORY)

**MANDATORY ACTION:** Each agent gets:
- **Specific scope:** One test file, subsystem, or component
- **Clear goal:** Make these tests pass / implement this component
- **Constraints:** Don't change other code
- **Expected output:** Summary of what you found and fixed

### 4. Dispatch in Parallel (MANDATORY)

```typescript
// In Claude Code / AI environment
Task("Fix agent-tool-abort.test.ts failures")
Task("Fix batch-completion-behavior.test.ts failures")
Task("Fix tool-approval-race-conditions.test.ts failures")
// All three run concurrently
```

### 4. Review and Integrate

When agents return:
- Read each summary
- Verify fixes don't conflict
- Run full test suite
- Integrate all changes

## Agent Prompt Structure

Good agent prompts are:
1. **Focused** - One clear problem domain
2. **Self-contained** - All context needed to understand the problem
3. **Specific about output** - What should the agent return?

```markdown
Fix the 3 failing tests in src/agents/agent-tool-abort.test.ts:

1. "should abort tool with partial output capture" - expects 'interrupted at' in message
2. "should handle mixed completed and aborted tools" - fast tool aborted instead of completed
3. "should properly track pendingToolCount" - expects 3 results but gets 0

These are timing/race condition issues. Your task:

1. Read the test file and understand what each test verifies
2. Identify root cause - timing issues or actual bugs?
3. Fix by:
   - Replacing arbitrary timeouts with event-based waiting
   - Fixing bugs in abort implementation if found
   - Adjusting test expectations if testing changed behavior

Do NOT just increase timeouts - find the real issue.

Return: Summary of what you found and what you fixed.
```

## Common Mistakes

**❌ Too broad:** "Fix all the tests" - agent gets lost
**✅ Specific:** "Fix agent-tool-abort.test.ts" - focused scope

**❌ No context:** "Fix the race condition" - agent doesn't know where
**✅ Context:** Paste the error messages and test names

**❌ No constraints:** Agent might refactor everything
**✅ Constraints:** "Do NOT change production code" or "Fix tests only"

**❌ Vague output:** "Fix it" - you don't know what changed
**✅ Specific:** "Return summary of root cause and changes"

## When NOT to Use

**Related failures:** Fixing one might fix others - investigate together first
**Need full context:** Understanding requires seeing entire system
**Exploratory debugging:** You don't know what's broken yet
**Shared state:** Agents would interfere (editing same files, using same resources)

## Real Example from Session

**Scenario:** 6 test failures across 3 files after major refactoring

**Failures:**
- agent-tool-abort.test.ts: 3 failures (timing issues)
- batch-completion-behavior.test.ts: 2 failures (tools not executing)
- tool-approval-race-conditions.test.ts: 1 failure (execution count = 0)

**Decision:** Independent domains - abort logic separate from batch completion separate from race conditions

**Dispatch:**
```
Agent 1 → Fix agent-tool-abort.test.ts
Agent 2 → Fix batch-completion-behavior.test.ts
Agent 3 → Fix tool-approval-race-conditions.test.ts
```

**Results:**
- Agent 1: Replaced timeouts with event-based waiting
- Agent 2: Fixed event structure bug (threadId in wrong place)
- Agent 3: Added wait for async tool execution to complete

**Integration:** All fixes independent, no conflicts, full suite green

**Time saved:** 3 problems solved in parallel vs sequentially

## Key Benefits

1. **Parallelization** - Multiple investigations happen simultaneously
2. **Focus** - Each agent has narrow scope, less context to track
3. **Independence** - Agents don't interfere with each other
4. **Speed** - 3 problems solved in time of 1

## Verification

After agents return:
1. **Review each summary** - Understand what changed
2. **Check for conflicts** - Did agents edit same code?
3. **Run full suite** - Verify all fixes work together
4. **Spot check** - Agents can make systematic errors

## Real-World Impact

From debugging session (2025-10-03):
- 6 failures across 3 files
- 3 agents dispatched in parallel
- All investigations completed concurrently
- All fixes integrated successfully
- Zero conflicts between agent changes