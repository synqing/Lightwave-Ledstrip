# Workflow Routing — Tool & Skill Dispatch Guide

> **This document is loaded via CLAUDE.md. Every CC session must follow these routing rules.**
> Last updated: 5 Mar 2026

---

## WHY THIS EXISTS

This project has 29 skills, 13 MCP server groups (47 individual tools), and multiple orchestration protocols. Despite this, CC sessions routinely ignore available tools and reinvent from scratch. This document fixes that by providing explicit routing: **when you encounter X, use Y, in this way.**

**The rule is simple: check this routing table BEFORE writing code or executing tasks.** If a skill or tool exists for the job, USE IT. Do not freestyle.

---

## PHASE 0: SESSION START (every session)

Before doing anything:

1. **Read this file** (you're doing it now — good)
2. **Check Claude-mem for recent context:**
   ```
   mcp__plugin_claude-mem_mem-search__search("recent activity on [topic]")
   mcp__plugin_claude-mem_mem-search__timeline()
   ```
3. **If the task touches code, query Auggie first:**
   ```
   mcp__auggie__codebase-retrieval("[what you're looking for]")
   ```
   Auggie understands code structure. Use it before grep/glob fishing expeditions.

---

## ROUTING TABLE: Task → Tool

### Code Intelligence (C++ Firmware)

| I need to... | Use this | How |
|---|---|---|
| Find where a function is defined | `mcp__clangd__find_definition` | Pass file path + position |
| Find all callers of a function | `mcp__clangd__get_call_hierarchy` | Returns full call tree |
| Find all references to a symbol | `mcp__clangd__find_references` | Includes headers |
| See type relationships | `mcp__clangd__get_type_hierarchy` | Class inheritance chains |
| Find all symbols in a file | `mcp__clangd__get_document_symbols` | Faster than reading the whole file |
| Search symbols by name | `mcp__clangd__workspace_symbol_search` | Fuzzy match across codebase |
| Get compiler diagnostics | `mcp__clangd__get_diagnostics` | Errors/warnings without building |
| Get hover info (type, docs) | `mcp__clangd__get_hover` | Quick type lookup |
| Find interface implementations | `mcp__clangd__find_implementations` | Virtual method overrides |

**Prerequisite:** compile_commands.json must exist in firmware-v3/. If missing: `pio run -e esp32dev_audio_pipelinecore --target compiledb`

### Code Intelligence (General / Cross-Language)

| I need to... | Use this | How |
|---|---|---|
| Understand code structure semantically | `mcp__auggie__codebase-retrieval` | Natural language query, understands code not just text |
| Search project documentation | `mcp__qmd__qmd_search` or `mcp__qmd__qmd_vector_search` | Keyword or semantic search across indexed docs |
| Deep search with context | `mcp__qmd__qmd_deep_search` | Multi-step retrieval with reranking |
| Get specific doc by path | `mcp__qmd__qmd_get` | Exact retrieval when you know the doc |
| Check QMD index health | `mcp__qmd__qmd_status` | Verify collections are populated |
| Look up library API docs | Context7 (via CC built-in) | Version-specific, prevents hallucination |

### Memory & Context

| I need to... | Use this | How |
|---|---|---|
| Recall what happened in past sessions | `mcp__plugin_claude-mem_mem-search__search` | Natural language query across session history |
| Get observations from memory | `mcp__plugin_claude-mem_mem-search__get_observations` | Structured facts stored by past sessions |
| View session timeline | `mcp__plugin_claude-mem_mem-search__timeline` | Chronological session activity |
| Search MCP-specific memory | `mcp__plugin_claude-mem_mcp-search__search` | Tool usage history |
| Get memory system help | `mcp__plugin_claude-mem_mem-search__help` | When unsure how to query |
| Read episodic memory | `mcp__plugin_episodic-memory_episodic-memory__read` | Detailed episode recall |
| Get current task list | `mcp__taskmaster-ai__get_tasks` | Taskmaster project state |

### Browser & Testing

| I need to... | Use this | How |
|---|---|---|
| Test web UI / take screenshots | Playwright MCP (`mcp__playwright__*`) | 8 tools: navigate, click, snapshot, screenshot, evaluate, console, resize, wait |
| Automated browser testing | `/playwright-skill` | Full test scripts with server detection |
| Test web app interactively | `/webapp-testing` | Playwright-based interactive testing |
| Browse the web as a user | `mcp__plugin_superpowers-chrome_chrome__use_browser` | Chrome automation for research |
| Test iOS simulator | `/ios-simulator-skill-main` | 21 scripts for iOS testing |

### Design & Creative

| I need to... | Use this | How |
|---|---|---|
| Work with Figma designs | `mcp__figma__add_figma_file` | Import Figma files for reference |
| 3D work / Blender scenes | `mcp__blender__*` | 7 tools: scene info, viewport, code execution, PolyHaven assets |
| Build frontend UI | `/front-end-design` skill | Production-grade, avoids generic AI aesthetics |
| Build React artifacts | `/web-artifacts-builder` skill | Multi-component with shadcn/ui |
| D3.js visualisations | `/claude-d3js-skill-main` skill | Interactive data viz |
| Apply themes to outputs | `/theme-factory` skill | 10 presets + custom generation |

### Feature Planning

| I need to... | Use this | How |
|---|---|---|
| Develop a concept / plan a feature | `mcp__Context-Engineer__concept_development` or `mcp__Context-Engineer__plan_feature` | Structured feature planning |
| Brainstorm before coding | `/brainstorming` skill | **MANDATORY before implementation plans** — refines ideas through questioning |
| Display results to user | `mcp__nimbalyst-mcp__display_to_user` | Formatted output display |

---

## ROUTING TABLE: Workflow Phase → Skill

### The Development Lifecycle

Every feature should flow through these phases. **Do not skip phases. Do not combine phases.**

```
IDEA → BRAINSTORM → DESIGN → PLAN → TEST-FIRST → IMPLEMENT → REVIEW → FINISH
```

| Phase | Skill | Trigger | Notes |
|---|---|---|---|
| **1. BRAINSTORM** | `/brainstorming` | "I want to build..." / new feature idea | Refines idea into design via questions. Outputs to docs/plans/ |
| **2. ARCHITECTURE** | `/software-architecture` | Design decisions, code structure | Clean Architecture + DDD. Early returns, library-first. |
| **3. TEST FIRST** | `/test-driven-development` | Before ANY implementation code | **IRON LAW: No production code without a failing test first.** |
| **4. IMPLEMENT** | `/subagent-driven-development` | Executing an implementation plan | Fresh subagent per task + code review between tasks |
| **4a. PARALLEL** | `/dispatching-parallel-agents` | 3+ independent tasks | **MANDATORY** when tasks don't share state |
| **5. REVIEW** | `/review-implementing` | PR feedback received | Systematic processing of review comments |
| **6. FINISH** | `/finishing-a-development-branch` | All tests pass, ready to merge | Guides merge/PR/cleanup decision |
| **7. CHANGELOG** | `/Changelog-generator` | After merge | Auto-generates user-facing changelog from commits |

### iOS-Specific

| Phase | Skill | Notes |
|---|---|---|
| Any iOS work | `/iOS Expert` | Swift, SwiftUI, UIKit, Xcode. Invoke on ANY iOS mention. |
| iOS testing | `/ios-simulator-skill-main` | 21 scripts for build, test, automation |

### Research & Content

| I need to... | Skill | Notes |
|---|---|---|
| Research X/Twitter | `/x-research` | Last 7 days of discourse. Needs API key (currently deferred). |
| Write content with citations | `/Content Research Writer` | Research + hooks + citations + iteration |
| Find sales leads | `/lead-research-assistant` | Company analysis + contact strategies |
| Write internal comms | `/internal-comms` | Status reports, updates, newsletters in company format |

### Infrastructure & Tooling

| I need to... | Skill | Notes |
|---|---|---|
| Create a new CC skill | `/skill-creator` | Guide for building skills with evals |
| Build an MCP server | `/mcp-builder` | Python (FastMCP) or Node/TypeScript |
| Build a CC slash command | `/claude-command-builder` | Interactive command creator |
| Organise files | `/file-organizer` | Finds duplicates, suggests structure |
| Use Git worktrees | `/using-git-worktrees` | Smart worktree creation with safety checks |
| Work with PDFs | `/pdf` | Extract, create, merge, split, fill forms |
| Supabase anything | `/supabase-expert` | 2,616 official docs. Auth, realtime, storage, edge functions, pgvector. |

### Formatting & Output

| I need to... | Skill | Notes |
|---|---|---|
| Format structured data efficiently | `/toon-formatter` | TOON v2.0 for arrays/tables ≥5 items. **Use by default when tokens matter.** |
| Apply brand styling | `/brand-guidelines` | Anthropic brand colours/typography |
| Theme any artifact | `/theme-factory` | 10 presets + custom |
| Apply front-end design quality | `/front-end-design` | Avoids generic AI aesthetics |

---

## ORCHESTRATION: Ralph

**Ralph** (snarktank) is already active on this project. It's an autonomous iteration loop that:
- Breaks features into discrete user stories
- Spawns fresh CC instances per iteration with clean context
- Uses git history + AGENTS.md for cross-iteration persistence
- Already delivered: `ralph/api-authentication` branch (8 user stories)

**When to use Ralph:** Features that exceed a single context window. Multi-story features. Anything where "do it all in one session" would result in context exhaustion.

**How to invoke:** Ralph is branch-based. Create a `ralph/<feature-name>` branch and follow the AGENTS.md protocol.

---

## ANTI-PATTERNS (things you must NOT do)

1. **Do NOT grep for code when clangd can find it.** `find_definition`, `find_references`, and `get_call_hierarchy` are faster and more accurate than text search for C++ symbols.

2. **Do NOT search docs by reading files when QMD can search them.** `qmd_search` and `qmd_vector_search` cover 330 docs / 2,421 chunks. Use them.

3. **Do NOT start coding without checking brainstorming + TDD skills.** The brainstorming skill refines ideas BEFORE you commit to an approach. The TDD skill ensures tests exist BEFORE implementation.

4. **Do NOT run 3+ independent tasks sequentially.** The parallel agents skill is marked MANDATORY. Use it.

5. **Do NOT ignore Auggie.** Before writing "let me search the codebase..." — query Auggie first. It understands code structure, not just text patterns.

6. **Do NOT forget Claude-mem.** Start sessions by checking recent memory. End sessions knowing the memory system captures what happened.

7. **Do NOT write frontend without checking `/front-end-design`.** It exists specifically to prevent generic AI-looking output.

8. **Do NOT build Supabase features without `/supabase-expert`.** 2,616 official docs are indexed. Use them.

---

## DECISION TREE: "What tool do I use?"

```
Is this about C++ firmware code?
├── YES → clangd (9 tools) for navigation, Auggie for semantic queries
│         QMD for documentation, Context7 for library APIs
│
Is this about iOS / Swift?
├── YES → /iOS Expert skill + /ios-simulator-skill-main
│
Is this a new feature idea?
├── YES → /brainstorming FIRST, then /software-architecture
│         Then /test-driven-development BEFORE implementation
│         Then /subagent-driven-development to execute
│
Is this 3+ independent tasks?
├── YES → /dispatching-parallel-agents (MANDATORY)
│
Is this about testing a web UI?
├── YES → Playwright MCP tools + /webapp-testing or /playwright-skill
│
Is this a research task?
├── YES → Auggie (code), QMD (docs), Claude-mem (history),
│         /x-research (Twitter), /Content Research Writer (general)
│
Am I about to write code?
├── YES → Did you brainstorm? Did you write the test first?
│         If no to either: STOP. Go back.
│
Am I finishing a branch?
├── YES → /finishing-a-development-branch
│         Then /Changelog-generator after merge
```

---

## Document History

| Date | Change |
|------|--------|
| 5 Mar 2026 | Initial creation. Full inventory: 29 skills, 13 MCP groups, 47 tools. Routing table, lifecycle phases, anti-patterns, decision tree. |
