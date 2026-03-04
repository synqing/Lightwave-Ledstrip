# SpectraSynq Toolchain Implementation Guide

**Purpose:** Agent-executable runbooks for installing and integrating 8 high-leverage tools across all SpectraSynq projects.

**Who this is for:** A Claude Code agent operating with full filesystem access. Each section is self-contained — an agent can execute any single tool independently.

**Projects in scope:**

| Project | Path | Stack |
|---------|------|-------|
| firmware-v3 | `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3` | C++17, ESP32-S3, FreeRTOS, PlatformIO, 800+ files |
| tab5-encoder | `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/tab5-encoder` | C++17, ESP32-P4 RISC-V, LVGL 9.3, PlatformIO |
| lightwave-ios-v2 | `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/lightwave-ios-v2` | Swift 6, SwiftUI, iOS 17+, SPM |
| Landing Page | `/Users/spectrasynq/SpectraSynq.LandingPage` | Next.js 14, React 18, Three.js/R3F, GLSL, Tailwind, Stripe/Square |
| Blender pipeline | `/Users/spectrasynq/SpectraSynq.LandingPage/Blender` | Python, Blender MCP, batch rendering |
| War Room | `/Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom` | Obsidian vault, Python harvesters, governed knowledge pipeline |

**Development environments in active use:** Claude Code (primary), Codex, Cursor, TRAE

---

## ENVIRONMENT STATUS (Verified 2026-03-04)

| Dependency | Status | Path / Version | Action Required |
|-----------|--------|----------------|-----------------|
| Node.js | v20.19.5 via nvm | `~/.nvm/versions/node/v20.19.5` | **Upgrade to v22+ for QMD** (`nvm install 22`) |
| Python 3.14 | Default `python3` | `/opt/homebrew/bin/python3` | Do NOT use for Zvec (unsupported) |
| Python 3.12 | Available | `python3.12` (3.12.12) | **Use this for Zvec and AgentKeeper** |
| Python 3.11 | Available | `python3.11` (3.11.14) | Backup option for Zvec |
| Bun | **NOT installed** | — | `curl -fsSL https://bun.sh/install \| bash` |
| pnpm | **NOT installed** | — | `npm install -g pnpm` |
| Codex CLI | Installed | `/opt/homebrew/bin/codex` | None |
| Homebrew | Installed | `/opt/homebrew/bin/brew` | None |
| nvm | Installed | `~/.nvm/` (only v20 present) | Install v22 |
| Lightwave `.mcp.json` | **Does NOT exist** | — | Create during Tool 4 or 7 |
| Landing Page `.mcp.json` | Exists (empty) | `{"mcpServers": {}}` | Add QMD + code-graph servers |
| War Room `.mcp.json` | Exists | Has youtube-transcript only | Add QMD server |
| `.claude/skills/` dirs | Exist in both projects | Lightwave + Landing Page | Clone x-research here |

---

## CRITICAL CONTEXT — READ BEFORE ANY IMPLEMENTATION

### Hard constraints (applies to ALL work)
- **K1 is AP-ONLY.** Never enable STA mode on ESP32 firmware.
- **Centre origin.** All LED effects originate from LED 79/80 outward. All K1 web visualisations inject light at centre, propagating symmetrically.
- **No heap alloc in render().** No `new`/`malloc`/`String` in render paths. Static buffers only.
- **120 FPS target.** Per-frame effect code under 2ms.
- **British English** in comments and docs (centre, colour, initialise).
- **Spec-driven development** on Landing Page — write spec first in `docs/specs/`.
- **Lint-zero** on Landing Page — `npm run lint` uses `max-warnings=0`.

### Existing MCP/Tool infrastructure
- **Lightwave-Ledstrip** has 590+ permission rules in `.claude/settings.local.json`
- **Landing Page** has 133 permission rules in `.claude/settings.local.json`
- **War Room** has taskmaster-ai and figma MCP servers enabled, plus youtube-transcript MCP
- **Episodic memory** is active across projects (`mcp__plugin_episodic-memory`)
- **claude-mem** is active across projects (`mcp__plugin_claude-mem_mcp-search`)
- **Blender MCP** is active on Landing Page (get_scene_info, execute_blender_code, etc.)
- **auggie** codebase retrieval is active on Landing Page
- **Nogic** code analysis is active on Lightwave-Ledstrip
- **Context7** library docs is active globally

### War Room pipeline pattern
All knowledge tools MUST be compatible with: **Input → Filter → Process → Validate → Integrate → Propagate**

Source notes go to `01-Sources/Incoming/`. Validated knowledge to `03-Knowledge/Domains/{domain}/`. DNA updates require Captain's explicit approval.

### Existing War Room harvesters
- `arxiv-harvester.py` — ArXiv paper discovery
- `youtube-harvester.py` — YouTube transcript extraction
- `web-harvester.py` — Web content harvesting
- NEW tools should follow the same pattern and output format

### Zero marketing budget
Social media intelligence is the primary GTM channel. Tools serving this function are mission-critical, not optional.

---

## TOOL 1: x-research-skill (Twitter/X Intelligence)

**What:** Claude Code skill for searching X/Twitter, following threads, tracking profiles, and producing sourced briefings.
**Why:** Zero marketing budget means social media IS the acquisition channel. This slots into the War Room harvester pipeline as the X/Twitter source.
**Repo:** https://github.com/rohunvora/x-research-skill

### Prerequisites
```bash
# 1. Install Bun runtime (NOT currently installed on this machine)
curl -fsSL https://bun.sh/install | bash
source ~/.zshrc  # Reload shell to pick up bun

# Verify:
bun --version  # Should return version number

# 2. Obtain X API v2 Bearer Token
# Go to: https://developer.x.com → Create app → Generate Bearer Token
# X API requires prepaid credits — budget ~$5-10/month for research use
```

### Installation

**Install as Claude Code skill (Lightwave-Ledstrip project):**
```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip
mkdir -p .claude/skills
cd .claude/skills
git clone https://github.com/rohunvora/x-research-skill.git x-research
cd x-research
bun install
```

**Install as Claude Code skill (Landing Page project):**
```bash
cd /Users/spectrasynq/SpectraSynq.LandingPage
mkdir -p .claude/skills
cd .claude/skills
git clone https://github.com/rohunvora/x-research-skill.git x-research
cd x-research
bun install
```

### Configuration
```bash
# Set bearer token globally
echo 'X_BEARER_TOKEN=your-token-here' >> ~/.config/env/global.env

# Or per-project in .env
echo 'X_BEARER_TOKEN=your-token-here' >> /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.env
```

### War Room Integration

Create a new harvester script at:
`/Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom/war-room/04-Operations/Utilities/x-harvester.py`

This harvester should:
1. Use x-research-skill to search for SpectraSynq-relevant topics (LED art, audio-reactive, ambient lighting, smart home aesthetics)
2. Output source notes in the War Room format to `01-Sources/Incoming/`
3. Follow the naming convention: `SRC · GTM · X Harvest {date}.md`
4. Include frontmatter matching existing harvest notes (see `OPS · Multi · Web Harvest 2026-03-04.md` for format)

### Permission Updates Required

Add to `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.claude/settings.local.json`:
```json
"Bash(bun run .claude/skills/x-research/x-search.ts:*)"
```

Add to `/Users/spectrasynq/SpectraSynq.LandingPage/.claude/settings.local.json`:
```json
"Bash(bun run .claude/skills/x-research/x-search.ts:*)"
```

### Verification
```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.claude/skills/x-research
bun run x-search.ts search "audio reactive LED" --sort likes --limit 5
# Should return formatted tweet results with engagement metrics
```

### Usage in Claude Code
```
# Search for competitors and market signals
bun run x-search.ts search "LED strip ambient" --sort likes --since 7d --save --markdown

# Track specific accounts (influencers, competitors)
bun run x-search.ts watchlist add "nanoleaf" "Competitor: smart lighting"
bun run x-search.ts watchlist add "govee_official" "Competitor: LED strips"
bun run x-search.ts watchlist check

# Follow a thread for deeper analysis
bun run x-search.ts thread TWEET_ID
```

### Cost awareness
- Post read: $0.005 each
- Quick search: ~$0.50/page (100 tweets)
- 24-hour API deduplication prevents duplicate charges
- Cached repeats: free

---

## TOOL 2: Zvec (In-Process Vector Database)

**What:** Lightweight, in-process vector database from Alibaba. Searches billions of vectors in milliseconds. Python and Node.js bindings.
**Why:** War Room's file-based knowledge is high-friction, low-value compared to semantic search. Zvec gives every project — Landing Page (Node.js), War Room harvesters (Python), firmware docs — a shared semantic layer without external infrastructure.
**Repo:** https://github.com/alibaba/zvec

### Prerequisites
```bash
# IMPORTANT: Zvec requires Python 3.10-3.12. System default is 3.14.3 (unsupported).
# Python 3.12.12 IS available on this machine as python3.12:
python3.12 --version  # Should return Python 3.12.12

# Node.js for Landing Page integration
node --version  # v20.19.5 — sufficient for Zvec npm package
```

### Installation

**Python (for War Room harvesters and firmware doc indexing):**
```bash
# MUST use python3.12, NOT python3 (which is 3.14 and unsupported by Zvec)
python3.12 -m pip install zvec --break-system-packages

# Or within War Room venv (create with python3.12):
cd /Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom
python3.12 -m venv .venv
source .venv/bin/activate
pip install zvec
```

**Node.js (for Landing Page integration):**
```bash
cd /Users/spectrasynq/SpectraSynq.LandingPage
npm install @zvec/zvec
```

### Architecture Design

Create a shared vector index at:
`/Users/spectrasynq/Workspace_Management/shared-vector-index/`

This index should contain collections for:
1. **war-room-knowledge** — All validated knowledge from `03-Knowledge/Domains/`
2. **firmware-docs** — Documentation from `firmware-v3/docs/` (API reference, effect development standard, CQRS architecture, etc.)
3. **landing-page-docs** — Specs and architecture docs from `SpectraSynq.LandingPage/docs/`
4. **social-intel** — Indexed outputs from x-research-skill harvests
5. **source-notes** — Processed source notes from `01-Sources/Active/`

### Python Usage (War Room Indexing Script)

Create at: `/Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom/war-room/04-Operations/Utilities/zvec-indexer.py`

```python
from zvec import Collection, CollectionSchema, Doc, VectorQuery
import os
import glob

# Schema: 768-dim vectors (matches common embedding models)
schema = CollectionSchema(vector_size=768)

# Create/open collection
INDEX_PATH = "/Users/spectrasynq/Workspace_Management/shared-vector-index/war-room"
collection = Collection(path=INDEX_PATH, schema=schema)

# Index all knowledge files
knowledge_dir = "/Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom/war-room/03-Knowledge"
for md_file in glob.glob(f"{knowledge_dir}/**/*.md", recursive=True):
    with open(md_file, 'r') as f:
        content = f.read()
    # Generate embedding (requires an embedding model — see note below)
    vector = generate_embedding(content)  # You'll need an embedding function
    doc = Doc(id=md_file, vector=vector, data={"text": content, "path": md_file})
    collection.insert([doc])
```

**IMPORTANT NOTE:** Zvec is the storage/search layer. You still need an embedding model to generate vectors. Options:
- `sentence-transformers` (Python, local, free): `pip install sentence-transformers`
- OpenAI embeddings API (paid, high quality)
- Local GGUF model via `node-llama-cpp` (pairs with QMD)

### Verification
```bash
python3.12 -c "from zvec import Collection; print('zvec installed successfully')"
```

---

## TOOL 3: Worktrunk (Git Worktree Manager)

**What:** CLI for managing git worktrees to run multiple AI agents in parallel without filesystem conflicts.
**Why:** Replaces the manual `cp -r firmware-v3 /tmp/agent_<name>_<timestamp>/` pattern documented in CLAUDE.md. The BeatTracker.cpp incident (6 corrupted parameters, regression from 17/17 to 15/17 tests) happened because parallel agents modified source directly. Worktrunk prevents this architecturally.
**Repo:** https://github.com/max-sixty/worktrunk

### Installation
```bash
# macOS via Homebrew
brew install worktrunk

# Shell integration (REQUIRED — add to shell profile)
wt config shell install

# Restart shell or source profile
source ~/.zshrc
```

### Configuration for Lightwave-Ledstrip

Create `.worktreeinclude` at repo root:
```bash
cat > /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.worktreeinclude << 'EOF'
# Share PlatformIO build cache between worktrees (saves ~15MB copy per tree)
.pio/build/
# Share node_modules for landing page
node_modules/
EOF
```

### Usage — Replacing the Manual Sandbox Pattern

**Before (CLAUDE.md current pattern):**
```bash
cp -r firmware-v3 /tmp/agent_<name>_<timestamp>/
# Agent works in /tmp/...
# Agent returns ONLY data
# Orchestrator applies winning change
```

**After (Worktrunk pattern):**
```bash
# Create isolated worktree for agent task
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip
wt switch --create agent-beattracker-fix

# Agent works in worktree (full git isolation)
# Make changes, build, test — all isolated

# When done, merge back
wt merge main

# Or discard if the approach didn't work
wt remove
```

**Launching Claude Code in a worktree:**
```bash
wt switch -x claude -c agent-effect-audit -- 'Audit all 349 effect files for centre-origin compliance'
```

### CLAUDE.md Update Required

The parallel agent sandboxing section in `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/CLAUDE.md` should be updated to reference Worktrunk instead of manual `/tmp/` copies. Proposed replacement:

```markdown
## Parallel Agent Sandboxing (MANDATORY)

When running parallel subagents that modify or build firmware code, **each agent MUST work in an isolated git worktree via Worktrunk**.

**The rule:**
1. Create worktree: `wt switch --create agent-<name>`
2. Agent works EXCLUSIVELY in its worktree
3. Agent returns ONLY data (numerical results, findings) — never merges without orchestrator approval
4. The orchestrator compares results across agents, then merges the winning worktree: `wt merge main`
5. Failed approaches: `wt remove`

**Why:** Git worktrees share the object store (instant creation, no 15MB copy), provide full git isolation, and integrate with build cache sharing via `.worktreeinclude`.
```

### Permission Updates Required

Add to Lightwave-Ledstrip `.claude/settings.local.json`:
```json
"Bash(wt:*)"
```

### Verification
```bash
wt --version
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip
wt list  # Should show main worktree
```

---

## TOOL 4: QMD (Local Semantic Search + MCP)

**What:** On-device search engine for markdown notes with BM25 + vector semantic search + LLM re-ranking. MCP server built in.
**Why:** Makes the entire War Room knowledge base, all firmware docs, and landing page specs agent-searchable via MCP. Any Claude Code session in any project can query: "What did we learn about gamut mapping?" and get ranked results from validated knowledge.
**Repo:** https://github.com/tobi/qmd

### Prerequisites
```bash
# CRITICAL: QMD requires Node.js >= 22. Current system Node is v20.19.5 (via nvm).
# You MUST install Node 22+ before QMD will work.
nvm install 22
nvm use 22
node --version  # Must show v22.x.x

# macOS SQLite (already installed via Homebrew on this machine)
# If missing: brew install sqlite
```

### Installation
```bash
# Ensure Node 22+ is active first:
nvm use 22

npm install -g @tobilu/qmd
```

**First-run model download:** QMD auto-downloads ~2GB of GGUF models to `~/.cache/qmd/models/` on first use:
- embedding-gemma-300M-Q8_0 (~300MB)
- qwen3-reranker-0.6b-q8_0 (~640MB)
- qmd-query-expansion-1.7B-q4_k_m (~1.1GB)

### Collection Setup

**Index all SpectraSynq knowledge sources:**
```bash
# War Room validated knowledge
qmd collection add "/Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom/war-room/03-Knowledge" --name war-room-knowledge

# War Room operations & specs
qmd collection add "/Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom/war-room/04-Operations" --name war-room-ops

# War Room active sources
qmd collection add "/Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom/war-room/01-Sources/Active" --name war-room-sources

# Firmware documentation
qmd collection add "/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/docs" --name firmware-docs

# Landing page docs and specs
qmd collection add "/Users/spectrasynq/SpectraSynq.LandingPage/docs" --name landing-page-docs

# Add context descriptions for better search
qmd context add qmd://war-room-knowledge "Validated knowledge across domains: AI-Infrastructure, Business-Strategy, Design, Firmware, GTM, Hardware, IP-Legal, Manufacturing, Software"
qmd context add qmd://war-room-ops "Operational specs, research arenas, harvest digests, agent specs, DNA updates"
qmd context add qmd://war-room-sources "Processed external intelligence: ArXiv papers, YouTube transcripts, web research"
qmd context add qmd://firmware-docs "LightwaveOS v2 firmware: API reference, audio-visual mapping, CQRS architecture, effect development standard, constraints"
qmd context add qmd://landing-page-docs "Landing page: architecture, specs, center origin guide, context guide, skills catalog"

# Build index
qmd embed
```

### MCP Integration — All Projects

**Option A: Claude Desktop (global):**

Add to `~/Library/Application Support/Claude/claude_desktop_config.json`:
```json
{
  "mcpServers": {
    "qmd": {
      "command": "qmd",
      "args": ["mcp"]
    }
  }
}
```

**Option B: Per-project MCP (recommended for granularity):**

**NOTE:** Lightwave-Ledstrip `.mcp.json` does not exist yet. It will be created when installing Tool 7 (MCP Code Graph). Add QMD to it at that time, or create it now:

Create `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.mcp.json`:
```json
{
  "mcpServers": {
    "qmd": {
      "command": "qmd",
      "args": ["mcp"]
    }
  }
}
```

Update `/Users/spectrasynq/SpectraSynq.LandingPage/.mcp.json` (currently empty `{"mcpServers": {}}`):
```json
{
  "mcpServers": {
    "qmd": {
      "command": "qmd",
      "args": ["mcp"]
    }
  }
}
```

Add to `/Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom/.mcp.json`:
```json
{
  "mcpServers": {
    "qmd": {
      "command": "qmd",
      "args": ["mcp"]
    },
    "youtube-transcript": {
      "command": "npx",
      "args": ["-y", "@kimtaeyoon83/mcp-server-youtube-transcript"]
    }
  }
}
```

### Permission Updates Required

Add to Lightwave-Ledstrip `.claude/settings.local.json`:
```json
"mcp__qmd__qmd_search",
"mcp__qmd__qmd_vector_search",
"mcp__qmd__qmd_deep_search",
"mcp__qmd__qmd_get",
"mcp__qmd__qmd_multi_get",
"mcp__qmd__qmd_status"
```

Add equivalent to Landing Page and War Room settings.

### HTTP Daemon Mode (Recommended)

For long sessions, run QMD as a persistent server to avoid repeated model loading:
```bash
qmd mcp --http --daemon
# Runs on localhost:8181 by default
```

### Verification
```bash
qmd status                                    # Check index health
qmd query "gamut mapping for LED hardware"    # Test semantic search
qmd search "audio-visual" -c firmware-docs    # Test collection-scoped search
```

---

## TOOL 5: AgentKeeper (Cross-Provider Memory)

**What:** Cognitive persistence layer for AI agents. Maintains memory across provider switches (Claude ↔ Codex ↔ Cursor ↔ TRAE) and session restarts.
**Why:** Captain uses Claude Code (primary), Codex, Cursor, and TRAE. Critical decisions and findings made in one environment are invisible to another. AgentKeeper bridges that gap with 95% memory recovery across provider switches.
**Repo:** https://github.com/Thinklanceai/agentkeeper

### Installation
```bash
cd /Users/spectrasynq/Workspace_Management/Software
git clone https://github.com/Thinklanceai/agentkeeper.git
cd agentkeeper
# Use python3.12 if requirements include packages incompatible with 3.14
python3.12 -m pip install -r requirements.txt --break-system-packages

# Create configuration
cp env.example .env
```

### Configuration

Edit `/Users/spectrasynq/Workspace_Management/Software/agentkeeper/.env`:
```bash
ANTHROPIC_API_KEY=sk-ant-...   # From existing Claude setup
OPENAI_API_KEY=sk-...          # For Codex/Cursor
# GEMINI_API_KEY=...           # If using Gemini
# OLLAMA_HOST=http://localhost:11434  # If using local models
```

### Architecture

Create project-specific agents:
```python
import sys
sys.path.insert(0, '/Users/spectrasynq/Workspace_Management/Software/agentkeeper')
import agentkeeper

# Create agents per project
firmware_agent = agentkeeper.create(agent_id="spectrasynq-firmware", provider="anthropic")
landing_agent = agentkeeper.create(agent_id="spectrasynq-landing", provider="anthropic")
warroom_agent = agentkeeper.create(agent_id="spectrasynq-warroom", provider="anthropic")

# Store critical facts that must persist across providers
firmware_agent.remember("K1 is AP-ONLY. Never enable STA mode.", critical=True)
firmware_agent.remember("BeatTracker.cpp was corrupted by parallel agents on 2026-02-XX. Always use worktree isolation.", critical=True)
firmware_agent.remember("Centre origin: all effects from LED 79/80 outward.", critical=True)
firmware_agent.remember("120 FPS target: per-frame render under 2ms.", critical=True)

landing_agent.remember("Center origin: all K1 visualisations inject light at CENTER, propagating symmetrically.", critical=True)
landing_agent.remember("Design tokens: brand gold #FFB84D, background #000000.", critical=True)
landing_agent.remember("Lint-zero: npm run lint uses max-warnings=0.", critical=True)

warroom_agent.remember("Pipeline: Input → Filter → Process → Validate → Integrate → Propagate", critical=True)
warroom_agent.remember("DNA updates require Captain's explicit approval.", critical=True)
warroom_agent.remember("Captain is NOT an engineer. Translate strategic language to execution.", critical=True)

# Save all
firmware_agent.save()
landing_agent.save()
warroom_agent.save()
```

### Integration Pattern

When switching from Claude Code to Cursor/Codex, the receiving agent loads context:
```python
agent = agentkeeper.load("spectrasynq-firmware")
context = agent.ask("What are the critical constraints for this project?", provider="openai", token_budget=4000)
# Returns prioritised critical facts within token budget
```

### Verification
```bash
python3.12 -c "
import sys; sys.path.insert(0, '/Users/spectrasynq/Workspace_Management/Software/agentkeeper')
import agentkeeper
agent = agentkeeper.create(agent_id='test-verify', provider='anthropic')
agent.remember('test fact', critical=True)
agent.save()
loaded = agentkeeper.load('test-verify')
print(loaded.stats())
"
```

---

## TOOL 6: Get Shit Done (Context Engineering)

**What:** Meta-prompting and context engineering system that prevents quality degradation across long AI coding sessions. Spec-driven workflow with parallel wave execution.
**Why:** firmware-v3 has 800+ files. Landing Page has a 1,239-line physics engine. Both CLAUDE.md files explicitly warn about context destruction from unmanaged exploration. GSD maintains fresh 200k context per executor — no accumulated degradation.
**Repo:** https://github.com/gsd-build/get-shit-done

### Installation

**For Claude Code (primary — install globally):**
```bash
npx get-shit-done-cc@latest --claude --global
```

**For Codex (secondary):**
```bash
npx get-shit-done-cc@latest --codex --global
```

**For all runtimes at once:**
```bash
npx get-shit-done-cc@latest --all --global
```

### Verification
In Claude Code session:
```
/gsd:help
```
Should display all available GSD commands.

### Usage — Firmware Development

```bash
# Map existing codebase first (IMPORTANT for brownfield projects)
/gsd:map-codebase

# Start new feature with full context engineering
/gsd:new-project

# Plan a specific phase
/gsd:plan-phase 1

# Execute with parallel wave execution (fresh context per executor)
/gsd:execute-phase 1

# Verify results
/gsd:verify-work 1

# Complete and archive
/gsd:complete-milestone
```

### Integration with Existing Spec-Driven Workflow

GSD's spec-driven approach complements the Landing Page's existing pattern:
- Landing Page uses `docs/specs/FEATURE_SPEC.md` template → GSD uses `PROJECT.md` + `REQUIREMENTS.md` + `ROADMAP.md`
- Both enforce "spec before code"
- GSD adds: parallel wave execution, fresh context per executor, atomic commits

**Recommended approach:** Use GSD for firmware-v3 and cross-project work. Continue using existing spec template for Landing Page single-feature work.

### Key Operational Notes
- GSD creates a `.planning/` directory in the project root for research, plans, and summaries
- Each phase gets atomic commits: `feat(phase): description`
- Wave execution uses fresh 200k context per executor — this is the key anti-degradation mechanism
- Quick mode (`/gsd:quick`) available for bug fixes and small features

---

## TOOL 7: MCP Code Graph (Code Graph Analysis)

**What:** MCP server for code graph analysis — semantic search, dependency mapping, and impact analysis across repositories.
**Why:** firmware-v3's actor model spans 5 FreeRTOS tasks across 800+ files. Landing Page has a complex engine with FBO management, shader pipeline, transitions, and physics. Code graph makes your 13 specialist agents smarter about dependency chains before they make changes.
**Repo:** https://github.com/davila7/mcp-code-graph

### Prerequisites
```bash
# 1. CodeGPT account required
# Go to: https://app.codegpt.co → Create account → Generate API key

# 2. Upload repositories to CodeGPT Code Graph
# Upload: firmware-v3, tab5-encoder, lightwave-ios-v2, apps/web-main

# 3. pnpm required (NOT currently installed on this machine)
npm install -g pnpm
```

### Installation
```bash
cd /Users/spectrasynq/Workspace_Management/Software
git clone https://github.com/davila7/mcp-code-graph.git
cd mcp-code-graph
pnpm install
pnpm build
```

### Configuration

**Add to Lightwave-Ledstrip MCP config:**

**NOTE:** `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.mcp.json` does NOT currently exist. Create it:
```json
{
  "mcpServers": {
    "code-graph": {
      "command": "node",
      "args": ["/Users/spectrasynq/Workspace_Management/Software/mcp-code-graph/build/index.js"],
      "env": {
        "CODEGPT_API_KEY": "your-api-key-here"
      }
    }
  }
}
```

**Add to Landing Page MCP config:**

Update `/Users/spectrasynq/SpectraSynq.LandingPage/.mcp.json`:
```json
{
  "mcpServers": {
    "code-graph": {
      "command": "node",
      "args": ["/Users/spectrasynq/Workspace_Management/Software/mcp-code-graph/build/index.js"],
      "env": {
        "CODEGPT_API_KEY": "your-api-key-here"
      }
    }
  }
}
```

### Permission Updates Required

Add to both project `.claude/settings.local.json` files:
```json
"mcp__code-graph__list_graphs",
"mcp__code-graph__get_code",
"mcp__code-graph__find_direct_connections",
"mcp__code-graph__nodes_semantic_search",
"mcp__code-graph__docs_semantic_search",
"mcp__code-graph__get_usage_dependency_links"
```

### Available MCP Tools
- `list_graphs` — Discover accessible repository graphs
- `get_code` — Retrieve complete code implementations by symbol
- `find_direct_connections` — Explore immediate code relationships (callers, callees)
- `nodes_semantic_search` — "Find functions related to beat detection" → ranked results
- `docs_semantic_search` — Query documentation semantically
- `get_usage_dependency_links` — Impact analysis: what breaks if I change this function?

### IMPORTANT NOTE
This tool requires a CodeGPT account and uploaded repositories. It is NOT local-only. If this dependency is unacceptable, **Nogic** (`mcp__nogic`) is already configured on Lightwave-Ledstrip and provides similar code graph capabilities locally. Consider whether MCP Code Graph adds value beyond what Nogic already provides, or whether it's redundant.

### Verification
```bash
# After uploading repos to CodeGPT:
node /Users/spectrasynq/Workspace_Management/Software/mcp-code-graph/build/index.js
# Should start MCP server without errors
```

---

## TOOL 8: Claude Review Loop (Multi-Agent Code Review)

**What:** Automated two-phase code review: Claude Code implements tasks, then Codex independently reviews via up to 4 parallel review agents.
**Why:** The BeatTracker.cpp incident proved single-agent development is insufficient for safety-critical firmware. Multi-agent review catches constraint violations (heap in render, centre-origin breaks, timing regressions) that a single agent may miss.
**Repo:** https://github.com/hamelsmu/claude-review-loop

### Prerequisites
```bash
# 1. Codex CLI required for review agents
npm install -g @openai/codex

# 2. OpenAI API key (for Codex review agents)
export OPENAI_API_KEY=sk-...

# 3. jq (JSON processor)
brew install jq  # If not already installed

# 4. Enable multi-agent mode in Codex
mkdir -p ~/.codex
cat > ~/.codex/config.toml << 'EOF'
[features]
multi_agent = true
EOF
```

### Installation
```bash
# Via Claude Code marketplace
claude plugin marketplace add hamelsmu/claude-review-loop
claude plugin install review-loop@hamel-review
```

**If marketplace install fails, manual install:**
```bash
cd /Users/spectrasynq/Workspace_Management/Software
git clone https://github.com/hamelsmu/claude-review-loop.git
# Follow repo README for manual plugin registration
```

### Configuration for SpectraSynq

The review loop spawns up to 4 parallel Codex sub-agents. For firmware work, customise the review criteria:

Create `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.claude/review-config.md`:
```markdown
# SpectraSynq Firmware Review Criteria

## CRITICAL CHECKS (review MUST flag violations)
- Any `new`, `malloc`, or `String` allocation in render() paths
- Any effect that doesn't use centre-origin (LED 79/80)
- Any per-frame code exceeding 2ms budget estimate
- Any STA mode WiFi configuration on K1
- Any rainbow cycling or full hue-wheel sweep

## IMPORTANT CHECKS
- British English in comments and docs
- Zone bounds checking: `(ctx.zoneId < kMaxZones) ? ctx.zoneId : 0`
- Static buffers used instead of heap allocation
- FreeRTOS task affinity correct (Audio=Core 0, Renderer=Core 1)

## STRUCTURAL CHECKS
- Actor model boundaries respected
- ControlBus used for audio state sharing
- RenderContext used for per-frame data
```

### Usage
```bash
# In Claude Code session:
/review-loop Implement new audio-reactive pulse effect with centre-origin compliance

# Cancel if needed:
/cancel-review
```

### Review Agents Spawned
| Agent | Always/Conditional | Checks |
|-------|-------------------|--------|
| Diff Review | Always | Code quality, test coverage, OWASP security |
| Holistic Review | Always | Project structure, documentation, architecture |
| Next.js Review | Conditional (if Next.js detected) | App Router, Server Components, caching |
| UX Review | Conditional (if browser tests exist) | E2E testing, accessibility, responsiveness |

### State Files
- Review state: `.claude/review-loop.local.md` (gitignored)
- Review output: `reviews/review-<id>.md`
- Execution logs: `.claude/review-loop.log`
- Stop hook timeout: 900 seconds (adjustable in `hooks/hooks.json`)

### Verification
```bash
# Check Codex is available
codex --version

# Check multi-agent mode
cat ~/.codex/config.toml
# Should show: multi_agent = true

# Test in Claude Code:
/review-loop Add a comment to README.md
# Should trigger implementation + review cycle
```

---

## INSTALLATION SEQUENCE (Recommended Order)

Execute in this order to minimise dependency conflicts:

```
Phase 0 — Runtime Prerequisites (MUST complete first)
├── Install Bun:        curl -fsSL https://bun.sh/install | bash && source ~/.zshrc
├── Install pnpm:       npm install -g pnpm
├── Install Node 22:    nvm install 22 (keep v20 as default; use v22 for QMD only)
└── Verify python3.12:  python3.12 --version  (should show 3.12.12)

Phase 1 — Foundation (no dependencies between these)
├── Tool 3: Worktrunk (brew install worktrunk && wt config shell install)
├── Tool 6: Get Shit Done (npx get-shit-done-cc@latest --all --global)
└── Tool 2: Zvec (python3.12 -m pip install zvec + npm install @zvec/zvec)

Phase 2 — Knowledge Layer (Zvec should be installed first)
├── Tool 4: QMD (nvm use 22 && npm install -g @tobilu/qmd, collection setup, MCP config)
├── Tool 1: x-research-skill (git clone into .claude/skills/, bun install, env config)
└── Create Lightwave .mcp.json (does not exist yet — create with QMD + code-graph entries)

Phase 3 — Agent Intelligence (API keys required)
├── Tool 5: AgentKeeper (git clone, python3.12 -m pip install, .env config)
├── Tool 7: MCP Code Graph (git clone, pnpm build, CodeGPT account)
└── Tool 8: Claude Review Loop (marketplace install, Codex CLI already present, config)
```

### Post-Installation Checklist

- [ ] All permission updates applied to `.claude/settings.local.json` for each project
- [ ] All MCP configs updated in `.mcp.json` for each project
- [ ] QMD collections created and indexed (`qmd embed`)
- [ ] Worktrunk `.worktreeinclude` configured
- [ ] CLAUDE.md sandbox section updated to reference Worktrunk
- [ ] x-research-skill bearer token configured
- [ ] AgentKeeper critical facts seeded for all 3 project agents
- [ ] Codex multi-agent mode enabled for review loop
- [ ] War Room x-harvester created and tested
- [ ] `qmd status` returns healthy index
- [ ] `wt list` works in Lightwave-Ledstrip repo
- [ ] `/gsd:help` responds in Claude Code
