---
abstract: "Full AI toolchain inventory: Claude Code + 13 MCP servers + 29 skills. Gap analysis vs community consensus (March 2026). Cost is ~$20/mo. Includes Dicklesworthstone and snarktank repo assessments."
---

# The SpectraSynq Multiplier Stack

> Canonical reference — what we run, why we chose it, what the world runs, and where we diverge on purpose.
>
> Last updated: 5 March 2026

---

## Our Stack at a Glance

| Layer | Tool | Status | Role |
|-------|------|--------|------|
| **Primary Agent** | Claude Code CLI (Opus 4.6) | Active | Terminal-native agentic coding, 200K context |
| **Secondary Agent** | Codex CLI | Active | Cross-provider backup, OpenAI model access |
| **IDE** | VS Code + PlatformIO | Active | Firmware builds, hardware debug |
| **Multi-Provider Router** | Get Shit Done (GSD) | Installed | Claude/Codex/Gemini/OpenCode dispatch |
| **Cross-Session Memory** | Claude-mem (7 tools) | Active | Search, observations, timeline across all CC sessions. Persistent memory layer. |
| **Codebase Retrieval** | Auggie (Augment Code) | Active | Semantic codebase retrieval — understands code structure, not just text search. |
| **C++ Code Intelligence** | clangd-mcp-server (9 tools) | Active | Call hierarchy, definitions, references, type hierarchy, diagnostics |
| **JS/TS Code Intelligence** | ~~MCP Code Graph~~ | **REMOVED** | Redundant: Auggie + clangd + QMD + Context7 cover all needs. SaaS dependency removed. |
| **Doc Retrieval / RAG** | QMD (330 docs, 2,421 chunks) | Active | Vector search across War Room + firmware docs + landing page docs |
| **Live Documentation** | Context7 | Active | Version-specific library docs, anti-hallucination |
| **Worktree Isolation** | Worktrunk | Active | Git worktree management, parallel agent sandboxing |
| **Shared Embeddings** | Zvec | Installed | Cross-project vector index (shared-vector-index/) |
| **Agent Memory (Facts)** | ~~AgentKeeper~~ | **REMOVED** | Cancelled 5 Mar 2026. $37/mo, no documentation, no API key mechanism, unclear units. CLAUDE.md hard constraints + Claude-mem cover this need. |
| **Review Loop** | Claude Review Loop | Pending install | Iterative PR review with configurable criteria |
| **X/Twitter Research** | x-research-skill | Deferred ($100/mo) | Real-time trend monitoring (manual alternative: browser-use MCP) |
| **Project Knowledge** | Obsidian War Room | Active | Strategic intelligence, knowledge base, operations |
| **Build System** | PlatformIO 6.1.19 | Active | ESP32 firmware compilation, compiledb generation |
| **Runtimes** | Node 20 (default) / 22 (QMD), Python 3.12, Bun 1.3.10 | Active | Tool dependencies |

---

## The Community "God Tier" Stack (March 2026 Consensus)

Based on synthesis of 25+ developer community sources, surveys, and benchmarks from January–March 2026.

### The Big Three IDEs/Agents

The community has converged on three tier-1 choices. Nobody agrees on which is #1; everyone agrees these are the only three that matter:

**Cursor** (1M+ users, 360K paying) — Best codebase understanding, tightest feedback loop, 8 parallel agents. $20/mo. The safe pick for large codebases. Enterprise teams default here.

**Claude Code** (71K GitHub stars, 135K commits/day) — Largest context window (200K tokens), terminal-native, best for complex decomposable tasks. $20/mo. The power-user pick for engineers who live in the terminal.

**Windsurf** (by Codeium, acquired by Cognition for $250M) — Best autonomy, cheapest tier-1 at $15/mo, Cascade agentic system. The value pick for maximum AI autonomy.

Honourable mentions: Aider (39K stars, CLI-first, git-native), OpenCode (112K stars, model-agnostic, privacy-first), GitHub Copilot (20M users but losing mindshare to the above three among serious engineers).

### The Consensus Essential MCP Servers

The community says: pick 3–5 servers max. Each adds parse overhead.

| MCP Server | Category | Why Essential |
|------------|----------|---------------|
| **Context7** | Documentation | Live version-specific docs. Solves AI hallucination on APIs. |
| **Sequential Thinking** | Reasoning | Measurably improves AI reasoning quality on complex tasks. |
| **GitHub** (Anthropic official) | Version Control | Free, well-maintained, essential for repo workflows. |
| **Figma Dev Mode** | Design | Live design hierarchy, auto-layout, variants → code. |
| **Sentry** | Error Monitoring | Connect AI directly to error pipeline with breadcrumbs. |

### The Consensus Code Intelligence Layer

**CodeGrok MCP** — Semantic code search + AST + vector embeddings. Replaces grep with 10x better context efficiency. The community pick for AI agent code navigation.

**Code Pathfinder MCP** — Call graph generation via AST analysis. Python-only currently; JS/TS/Go on roadmap.

**grepai** — Local-first semantic search via Ollama. Privacy-first alternative to cloud-based tools. No API calls.

**Enterprise: Sourcegraph Cody** — Multi-repo semantic search. Queries like "Where is the payment token validated?" across an entire org.

### The Consensus Memory Layer

The community agrees agent amnesia is the #1 unsolved problem. Three-tier model is standard: working memory (context window) → short-term (session history) → long-term (persistent cross-session).

**Letta (formerly MemGPT)** — Memory as first-class agent state. Agents persist and evolve across sessions.

**OneContext** — Persistent context layer. Agents retain project knowledge across sessions, devices, collaborators.

**Zep** — Temporal knowledge graph. Tracks how facts change over time. Enterprise-focused.

Research from Mem0 shows persistent memory achieves 26% higher response accuracy. This is the category with the most churn and least consensus.

### The Consensus Review / Quality Gate Layer

Key stat: **41% of commits in Feb 2026 are AI-assisted. AI code has 1.7x more defects than human code. 46% of developers distrust AI output.** Quality gates are now mandatory, not optional.

**CodeRabbit** — Most-installed AI review app on GitHub/GitLab. Detects 46% of runtime bugs.

**Qodo** — Blocking rules prevent merge until critical issues resolved. CI/CD-integrated.

**Codacy** — Quality gates block merges if standards aren't met.

Standard pattern: AI Generation → CI/CD → Automated AI Review → Quality Gates → Human Review (architecture/logic only).

### The Consensus Orchestration Layer

**CrewAI** — Python-based, role-playing autonomous agents. Best for simple multi-agent workflows.

**LangGraph** — Complex DAG-based workflows with loops and state.

**OpenAI Agents SDK** — Official multi-agent orchestration from OpenAI.

### Dark Horse / Sleeper Tools

**oh-my-opencode** (36.8K stars) — Hash-anchored edits improve edit success from 6.7% → 68.3%. Multi-model routing. LSP + AST integration. Requires OpenCode runtime. Arguably the most technically innovative tool in the space right now.

**Zed Editor** — Rust, GPU-accelerated, 120fps, native multiplayer. Growing fast among speed-obsessed teams.

**Pinecone** — Vector database becoming standard infrastructure for agent memory.

**UiPath Maestro** — Multi-vendor agent orchestration platform. Enterprise dark horse.

---

## Gap Analysis: Our Stack vs Community Consensus

### Where We Match or Exceed

| Category | Community Pick | Our Pick | Assessment |
|----------|---------------|----------|------------|
| **Primary Agent** | Claude Code | Claude Code (Opus 4.6) | **Match.** We're on the consensus #1 for terminal-native work. |
| **Context Window** | 200K (Claude) | 200K (Claude) | **Match.** Strategic advantage for firmware (800+ files). |
| **Worktree Isolation** | (no consensus tool) | Worktrunk | **Exceed.** Most teams use raw `git worktree`. We have managed isolation. |
| **Doc Retrieval** | Context7 + RAG | QMD (2,421 chunks) + Context7 | **Exceed.** QMD gives us domain-specific RAG the community doesn't have. |
| **C++ Intelligence** | (no consensus — C++ is niche) | clangd-mcp-server (9 tools) | **Exceed.** Most teams don't have MCP-integrated C++ intelligence at all. |
| **Multi-Provider** | Cursor (built-in) or GSD | GSD + Codex CLI | **Match.** Multi-provider access covered. |
| **Cross-Session Memory** | Letta / OneContext / Zep | Claude-mem (7 tools) + CLAUDE.md hard constraints | **Exceed.** Claude-mem provides search + observations + timeline natively within CC sessions. CLAUDE.md provides deterministic constraint loading every session. Most stacks are still assembling this from third-party tools. |
| **Codebase Retrieval** | Sourcegraph Cody / CodeGrok | Auggie (Augment Code) | **Match.** Semantic codebase retrieval already active. Auggie understands code structure, not just text matching. |
| **Build System** | (varies) | PlatformIO + compile_commands.json | **Appropriate.** Embedded-specific, no alternative. |

### Where We Have Gaps

| Category | Community Consensus | Our Current State | Gap Severity | Recommendation |
|----------|-------------------|-------------------|-------------|----------------|
| **IDE** | Cursor ($20/mo) | VS Code (free) | **MEDIUM** | Cursor's 8 parallel agents and AI-native codebase understanding are genuinely better than VS Code + extensions. However, for firmware, PlatformIO integration is critical and works best in VS Code. **Verdict: Evaluate Cursor for Landing Page work; keep VS Code for firmware.** |
| **Automated Code Review** | CodeRabbit + Qodo | Claude Review Loop (not yet installed) | **HIGH** | Review Loop is good but CodeRabbit catches 46% of runtime bugs automatically. For firmware with timing/memory constraints, this matters. **Verdict: Install Review Loop now. Evaluate CodeRabbit as a CI/CD addition.** |
| **Agent Memory** | Letta / OneContext / Zep | Claude-mem (active, 7 tools) + AgentKeeper (pending API key) | **LOW** | Claude-mem already provides cross-session search, observations, and timeline — the core memory loop the community is chasing with Letta/OneContext. AgentKeeper adds persistent critical facts. Together they cover 2 of the 3 tiers (short-term via Claude-mem, long-term via AgentKeeper). **Verdict: Already ahead of most stacks. Revisit only if temporal knowledge graphs become necessary.** |
| **Sequential Thinking MCP** | Essential (consensus) | Not installed | **LOW-MEDIUM** | Measurably improves reasoning on complex tasks. Free to add. **Verdict: Add it. Zero cost, measurable benefit.** |
| **Automated Quality Gates** | Qodo / Codacy (CI/CD blocking) | None | **MEDIUM** | We rely on human review + hard constraints in CLAUDE.md. No automated blocking. **Verdict: Not urgent at current team size (1). Add when velocity increases.** |
| **Orchestration Framework** | CrewAI / LangGraph | Manual (CLAUDE.md rules + subagent dispatch) | **LOW** | Our actor model firmware doesn't need agent orchestration frameworks. The CLAUDE.md parallel sandboxing rules are sufficient. **Verdict: Skip. Our domain is too specialised for generic orchestration.** |
| **Error Monitoring MCP** | Sentry MCP | None (serial monitor + manual) | **N/A** | Sentry is for web services. ESP32 firmware uses serial output. **Verdict: Not applicable to our primary codebase.** |
| **Browser-Use MCP** | (emerging) | Not installed | **LOW** | Alternative to x-research $100/mo API. Could enable free X/Twitter research via browser automation. **Verdict: Evaluate when x-research decision is made.** |

### Where We Deliberately Diverge

| Category | Community Does | We Do | Why |
|----------|---------------|-------|-----|
| **IDE choice** | Cursor | VS Code | PlatformIO integration is mandatory for ESP32. Cursor doesn't support PlatformIO natively. |
| **Orchestration** | CrewAI / LangGraph | Manual rules in CLAUDE.md | Our parallel agent sandboxing rules exist because of a specific incident (BeatTracker.cpp corruption). Generic orchestration can't enforce "K1 is AP-only" or "no heap alloc in render()". |
| **Memory tier** | 3-tier (working + short + long) | Claude-mem (short-term session memory) + CLAUDE.md (long-term critical facts) | Two-tier coverage. Claude-mem handles "what happened recently?" while CLAUDE.md deterministically loads "never forget K1 is AP-only" every session. We skip temporal knowledge graphs (Zep) because our constraints are static, not evolving. AgentKeeper was trialled and cancelled ($37/mo, no docs, no API key mechanism). |
| **Review approach** | AI review + CI gates | CLAUDE.md hard constraints + human review | Domain-specific constraints (timing budgets, centre-origin, AP-only) can't be expressed as generic code review rules. Human review catches what CodeRabbit can't. |
| **Cost model** | $20-50/mo per tool | Minimal recurring costs | Solo founder, zero marketing budget. Every dollar justified. x-research deferred because $100/mo isn't justified pre-launch. |

---

## The Three Repos We Evaluated

### your-source-to-prompt.html
747 stars, MIT, single HTML file. Opens in Chrome, lets you pick files from a codebase, concatenates them into an LLM-ready prompt with token counting and optional minification.

**Verdict: Skip.** Claude Code CLI + MCP tools already do intelligent context assembly. This is a manual file picker — a step backwards from semantic search. Useful as a fallback for non-technical collaborators, not for daily engineering work.

### compound-product
487 stars, MIT, 2 weeks old. Reads daily performance reports → LLM identifies #1 priority → auto-implements → creates PR for review. Autonomous improvement loop.

**Verdict: Park for later.** Good concept for the Landing Page (standard web stack, clear metrics). Dangerous for firmware (will violate timing constraints, AP-only rule, heap restrictions). Too young (2 weeks old, 487 stars). Revisit in 6 months when it matures and when landing page velocity justifies automation.

### oh-my-opencode
36,800 stars, daily commits, extremely active. Agent harness for OpenCode with discipline agents (Sisyphus, Hephaestus, Prometheus), multi-model routing, hash-anchored edits (6.7% → 68.3% success rate), LSP + AST integration.

**Verdict: The strongest candidate we've seen. Not for now, but for Phase 2.** Hash-anchored edits solve a real problem (stale-line errors in large files). Multi-model routing is genuine leverage. However: requires adopting OpenCode as a runtime, means paying multiple model subscriptions, and has a non-trivial learning curve. **Recommendation: Trial in Q2 2026 for Landing Page work. If the edit success improvement holds, evaluate for firmware.**

---

## Dicklesworthstone Repos — Assessed

Full profile scan of [github.com/Dicklesworthstone](https://github.com/Dicklesworthstone). This developer builds **agentic coding infrastructure** — tools that make AI agents work safely together at scale. Repos are Rust-heavy, production-grade, and designed to interoperate.

### Top Picks

**MCP Agent Mail** (1,764 stars) — Asynchronous multi-agent coordination with shared infrastructure. Message exchange, file reservation leases, persistent Git-backed storage + SQLite indexing, web UI for human oversight. Solves the exact problem we hit with BeatTracker.cpp (parallel agents corrupting shared state). **Verdict: Strong candidate if we ever scale beyond single-agent workflows. Park alongside Worktrunk — complementary, not competitive.**

**Destructive Command Guard (DCG)** (612 stars) — Security hook intercepting destructive commands (rm -rf, git reset --hard, dangerous DB ops). 49+ modular security packs, SIMD-accelerated <1ms filtering. **Verdict: Worth installing as a safety net. Zero friction, prevents catastrophic accidents from agentic code execution.**

**Coding Agent Session Search (CASS)** (538 stars) — Unified search across 11+ AI agent conversation histories (Claude Code, Codex, Gemini, Cursor, Aider, etc.). Sub-60ms local search with full-text + semantic via MiniLM. **Verdict: Solves "I solved this 3 sessions ago but can't find it." Evaluate after Claude-mem covers most needs — CASS adds cross-tool search Claude-mem can't do.**

**Agentic Coding Flywheel Setup (ACFS)** (1,207 stars) — One-command bootstrapping of a complete AI dev environment on a fresh Ubuntu VPS. 30+ tools, three AI agents, security verification, idempotent. **Verdict: Not relevant to our macOS workflow, but useful reference architecture.**

**Ultimate Bug Scanner (UBS)** — Static analysis meta-runner: 1000+ bug patterns across 8 languages including C/C++. SARIF export, agent-optimised JSON output. **Verdict: Relevant for firmware. Evaluate as a CI/CD quality gate alongside CodeRabbit.**

### Noted but Not Prioritised

Pi Agent Rust (488 stars, Rust CLI for AI coding), Beads Rust (669 stars, git-native issue tracker), NTM (166 stars, tmux agent orchestrator), FrankenTUI/FrankenSQLite/Frankensearch (infrastructure-level Rust tools). All well-built but solve problems we don't currently have.

---

## Snarktank Repos — Assessed

Full profile scan of [github.com/snarktank](https://github.com/snarktank). This developer builds **autonomous product development loops** — tools that break work into iterations and execute them with quality gates.

### Top Picks

**Ralph** (11,986 stars) — Autonomous AI agent loop that breaks product requirements into discrete user stories and iteratively implements them until completion, spawning fresh AI instances with clean context between iterations. Uses git history + progress logs + AGENTS.md for persistence. **Verdict: Directly applicable to firmware feature development. The "spawn fresh context per iteration" pattern is exactly how we should handle features that exceed a single context window. Evaluate for Landing Page first, firmware second.**

**ai-dev-tasks** (7,578 stars) — Markdown templates structuring feature development into PRD → tasks → staged implementation with review checkpoints. Works with Claude Code, Codex, and others. **Verdict: Lightweight methodology companion to Ralph. The templates themselves are valuable regardless of whether we adopt Ralph. Worth importing immediately.**

**Antfarm** (1,862 stars) — TypeScript CLI for orchestrating teams of specialised AI agents through coordinated workflows. Ships with 3 production workflows: Feature Development (7 agents), Security Auditing (7 agents), Bug Fixing (6 agents). YAML configs, SQLite state, deterministic execution. **Verdict: More structured than our CLAUDE.md parallel sandboxing rules. Evaluate when we need repeatable multi-agent workflows beyond ad-hoc subagent dispatch.**

**ai-pr-review** (57 stars) — GitHub Actions providing AI-powered PR code reviews as comments. Claude Code or Amp backend. **Verdict: Lightweight complement to Claude Review Loop. Automates what we currently do manually. Low stars but well-tested pattern.**

**Context7** (snarktank fork, 3 stars) — This appears to be a fork/contribution to the Context7 MCP server we already have listed. Not a separate tool.

---

## Upgrade Path (Prioritised)

These are the changes that would move us closer to the community "god tier" without breaking what works.

### Now (zero cost, immediate value)
1. **Finish toolchain activation** — Review Loop plugin, GSD verification (10 min in terminal)
2. **Add Sequential Thinking MCP** — Community consensus essential, free, improves reasoning quality
3. **Import ai-dev-tasks templates** — Markdown PRD → task templates for structured feature development (snarktank, 7.5K stars)

### Soon (low cost, clear ROI)
4. **Evaluate Cursor for Landing Page** — $20/mo, 8 parallel agents, superior codebase understanding vs VS Code. Keep VS Code for firmware (PlatformIO).
5. **Evaluate CodeRabbit** — Free tier available, automated PR review catches 46% of runtime bugs. Complements Review Loop.
6. **Browser-use MCP** — Free alternative to x-research $100/mo API for X/Twitter research
7. **Install DCG (Destructive Command Guard)** — Safety net against catastrophic agentic commands. Zero friction, <1ms overhead.
8. **Evaluate Ralph** — Autonomous iteration loop for features exceeding single context window (snarktank, 12K stars)

### Later (higher cost, velocity-dependent)
9. **oh-my-opencode trial** — When Landing Page reaches weekly deploy cadence and stale-line edit errors become a friction point
10. **compound-product trial** — When daily structured reports exist and Landing Page improvements can be safely automated
11. **CASS (Coding Agent Session Search)** — When cross-tool session search becomes needed beyond what Claude-mem provides
12. **Antfarm** — When we need repeatable multi-agent workflows beyond ad-hoc subagent dispatch

### Probably Never (for our stack)
- **Sentry MCP** — Web service monitoring, not applicable to embedded firmware
- **CrewAI / LangGraph** — Generic orchestration doesn't understand our domain constraints
- **Copilot** — We already have Claude Code + Codex. Adding a third provider adds cost without capability.

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                        CAPTAIN (Human)                               │
│                    Strategic decisions, API keys                      │
└──────────────┬──────────────────────────────────┬───────────────────┘
               │                                  │
    ┌──────────▼──────────┐            ┌──────────▼──────────┐
    │   Claude Code CLI   │            │     Codex CLI        │
    │   (Opus 4.6, 200K)  │            │   (cross-provider)   │
    │   Primary Agent      │            │   Secondary Agent    │
    └──────────┬──────────┘            └──────────┬──────────┘
               │                                  │
               ├──── GSD (multi-provider router) ─┘
               │
    ┌──────────▼──────────────────────────────────────────────┐
    │                    MCP Server Layer                       │
    │                                                          │
    │  ┌─────────────┐  ┌──────────┐  ┌───────────────────┐  │
    │  │   clangd     │  │   QMD    │  │   Taskmaster-AI   │  │
    │  │  (9 tools)   │  │ (RAG)    │  │  (task tracking)  │  │
    │  │  C++ intel   │  │ 2421     │  │                   │  │
    │  │  firmware-v3 │  │ chunks   │  │                   │  │
    │  └──────┬──────┘  └────┬─────┘  └───────────────────┘  │
    │         │              │                                 │
    │  ┌──────┴──────┐  ┌───┴──────┐  ┌───────────────────┐  │
    │  │  Context7    │  │ Claude-  │  │   Auggie           │  │
    │  │  (live docs) │  │ mem (7)  │  │  (codebase retr.) │  │
    │  └─────────────┘  │ memory   │  └───────────────────┘  │
    │                    └──────────┘                          │
    │  ┌─────────────┐  ┌──────────┐                          │
    │  │  Figma Dev   │  │ Seq.     │                          │
    │  │  Mode        │  │ Think.   │                          │
    │  └─────────────┘  │ (TODO)   │                          │
    │                    └──────────┘                          │
    └─────────────────────────────────────────────────────────┘
               │
    ┌──────────▼──────────────────────────────────────────────┐
    │                  Workspace Layer                          │
    │                                                          │
    │  ┌─────────────┐  ┌──────────────┐  ┌────────────────┐ │
    │  │  Worktrunk   │  │ Episodic Mem │  │ Review Loop    │ │
    │  │  (worktrees) │  │ (episodes)   │  │ (PR review)    │ │
    │  │  isolation   │  │ active       │  │ pending install│ │
    │  └─────────────┘  └──────────────┘  └────────────────┘ │
    │                                                          │
    │  ┌─────────────┐  ┌──────────────┐  ┌────────────────┐ │
    │  │    Zvec      │  │ x-research   │  │ War Room       │ │
    │  │  (vectors)   │  │ (deferred)   │  │ (Obsidian)     │ │
    │  └─────────────┘  └──────────────┘  └────────────────┘ │
    └─────────────────────────────────────────────────────────┘
               │
    ┌──────────▼──────────────────────────────────────────────┐
    │                   Build Layer                             │
    │                                                          │
    │  PlatformIO 6.1.19 → compile_commands.json (13MB)       │
    │  Node 20/22 · Python 3.12 · Bun 1.3.10 · pnpm          │
    └─────────────────────────────────────────────────────────┘
               │
    ┌──────────▼──────────────────────────────────────────────┐
    │                   Target Projects                        │
    │                                                          │
    │  firmware-v3     tab5-encoder    lightwave-ios-v2       │
    │  (C++/ESP32-S3)  (C++/ESP32-P4)  (Swift 6/SwiftUI)     │
    │                                                          │
    │  LandingPage     War Room        Blender Pipeline       │
    │  (Next.js 14)    (Obsidian)      (Python)               │
    └─────────────────────────────────────────────────────────┘
```

---

## Cost Summary

| Item | Monthly Cost | Status | Justification |
|------|-------------|--------|--------------|
| Claude Pro | $20 | Active | Primary agent, 200K context |
| Codex | (API usage) | Active | Secondary agent |
| Cursor | $0 (not adopted) | Evaluate | Would add $20/mo. Justified only for Landing Page. |
| x-research API | $0 (deferred) | Deferred | $100/mo not justified pre-launch |
| ~~CodeGPT API~~ | — | **REMOVED** | Redundant. Code Graph axed 5 Mar 2026. |
| CodeRabbit | $0 (not adopted) | Evaluate | Free tier available |
| All other tools | $0 | Active | Open-source, self-hosted, or free |

**Current total: ~$20/mo.** The entire multiplier stack runs on one Claude Pro subscription plus open-source tools. This is deliberate — every dollar must be justified at the solo founder stage.

---

## Document History

| Date | Change |
|------|--------|
| 5 Mar 2026 | Initial creation. 9-tool stack documented. Community consensus researched. Gap analysis complete. |
| 5 Mar 2026 | **CORRECTION**: Added Claude-mem (7 tools, cross-session memory) and Auggie (codebase retrieval) — both were already active but omitted from initial version. Added Dicklesworthstone repos (MCP Agent Mail, DCG, CASS, UBS) and snarktank repos (Ralph, ai-dev-tasks, Antfarm, ai-pr-review). Updated gap analysis, architecture diagram, and upgrade path. |
