# CC CLI Handoff — Remaining Tasks

> **Context:** Phases 0–2 of the toolchain installation are complete. The items below are what could NOT be done from Cowork and require a CC CLI session or Captain's direct input.

> **Status update 7 Mar 2026:**
> - ~~AgentKeeper~~ **CANCELLED 5 Mar** ($37/mo, no docs). claude-mem + CLAUDE.md covers this.
> - ~~MCP Code Graph / CodeGPT~~ **REMOVED 5 Mar**. Redundant — clangd + Auggie cover C++ and code intelligence.
> - **clangd** — DONE. llvm 22.1.0 installed, compile_commands.json exists (510 entries), MCP configured.
> - **Worktrunk** — DONE. Functional. Shell integration cosmetic. 12 prunable worktrees to clean.
> - **QMD** — DONE. Fully indexed: 330 files, 2,421 vectors, 5 collections (war-room-knowledge, war-room-ops, war-room-sources, firmware-docs, landing-page-docs). Verified 7 Mar with 93% relevance on test queries.
> - **X Bearer token** — PARKED. Not blocking. $100/mo X API. Manual browser research is viable alternative.
> - **CLAUDE.md enforcement** — UPDATED 7 Mar. Hard gate rules added for clangd, QMD, Context7. Anti-pattern table inline.

---

## 1. ~~API Keys~~ (RESOLVED — mostly parked/cancelled)

### X API Bearer Token (Tool 1: x-research-skill)
```bash
# Get token from: https://developer.x.com → Create app → Generate Bearer Token
# Then set it:
echo 'X_BEARER_TOKEN=<your-token>' >> ~/.config/env/global.env

# Test:
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.claude/skills/x-research
bun run x-search.ts search "LED art" --limit 3
```

### CodeGPT API Key (Tool 7: MCP Code Graph)
```bash
# Get key from: https://app.codegpt.co → Create account → Generate API key
# Then update BOTH .mcp.json files — replace YOUR_CODEGPT_API_KEY_HERE:
#   /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.mcp.json
#   /Users/spectrasynq/SpectraSynq.LandingPage/.mcp.json
#
# ALSO: Upload repos to CodeGPT Code Graph:
#   firmware-v3, tab5-encoder, lightwave-ios-v2, apps/web-main
```

### AgentKeeper API Keys (Tool 5)
```bash
# Edit: /Users/spectrasynq/Workspace_Management/Software/agentkeeper/.env
# Replace the placeholder values:
#   ANTHROPIC_API_KEY=sk-ant-...  (from existing Claude setup)
#   OPENAI_API_KEY=sk-...         (for Codex/Cursor cross-provider memory)
```

---

## 2. QMD Collection Setup + Indexing

QMD is installed but has no collections yet. Run these in a terminal with Node 22 active:

```bash
# Activate Node 22
export NVM_DIR="$HOME/.nvm" && . "$NVM_DIR/nvm.sh" && nvm use 22

# Create collections
qmd collection add "/Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom/war-room/03-Knowledge" --name war-room-knowledge
qmd collection add "/Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom/war-room/04-Operations" --name war-room-ops
qmd collection add "/Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom/war-room/01-Sources/Active" --name war-room-sources
qmd collection add "/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/docs" --name firmware-docs
qmd collection add "/Users/spectrasynq/SpectraSynq.LandingPage/docs" --name landing-page-docs

# Add context descriptions
qmd context add qmd://war-room-knowledge "Validated knowledge across domains: AI-Infrastructure, Business-Strategy, Design, Firmware, GTM, Hardware, IP-Legal, Manufacturing, Software"
qmd context add qmd://war-room-ops "Operational specs, research arenas, harvest digests, agent specs, DNA updates"
qmd context add qmd://war-room-sources "Processed external intelligence: ArXiv papers, YouTube transcripts, web research"
qmd context add qmd://firmware-docs "LightwaveOS v2 firmware: API reference, audio-visual mapping, CQRS architecture, effect development standard, constraints"
qmd context add qmd://landing-page-docs "Landing page: architecture, specs, center origin guide, context guide, skills catalog"

# Build the index (downloads ~2GB of models on first run, then indexes all collections)
qmd embed

# Verify
qmd status
qmd query "gamut mapping for LED hardware"
```

---

## 3. Worktrunk Shell Integration

Worktrunk is installed but the shell integration needs a reload:
```bash
wt config shell install
source ~/.zshrc

# Verify:
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip
wt list
```

---

## 4. AgentKeeper Critical Facts Seeding

After API keys are set in `.env`, seed the critical project facts:
```bash
cd /Users/spectrasynq/Workspace_Management/Software/agentkeeper
python3.12 -c "
import agentkeeper

# Firmware agent
fw = agentkeeper.create(agent_id='spectrasynq-firmware', provider='anthropic')
fw.remember('K1 is AP-ONLY. Never enable STA mode.', critical=True)
fw.remember('BeatTracker.cpp was corrupted by parallel agents. Always use worktree isolation.', critical=True)
fw.remember('Centre origin: all effects from LED 79/80 outward.', critical=True)
fw.remember('120 FPS target: per-frame render under 2ms.', critical=True)
fw.save()

# Landing page agent
lp = agentkeeper.create(agent_id='spectrasynq-landing', provider='anthropic')
lp.remember('Center origin: all K1 visualisations inject light at CENTER, propagating symmetrically.', critical=True)
lp.remember('Design tokens: brand gold #FFB84D, background #000000.', critical=True)
lp.remember('Lint-zero: npm run lint uses max-warnings=0.', critical=True)
lp.save()

# War Room agent
wr = agentkeeper.create(agent_id='spectrasynq-warroom', provider='anthropic')
wr.remember('Pipeline: Input > Filter > Process > Validate > Integrate > Propagate', critical=True)
wr.remember('DNA updates require Captain explicit approval.', critical=True)
wr.save()

print('All agents seeded.')
"
```

---

## 5. Claude Review Loop Installation

This needs a CC CLI session:
```bash
# Try marketplace first:
claude plugin marketplace add hamelsmu/claude-review-loop
claude plugin install review-loop@hamel-review

# If marketplace fails, the repo is already cloned at:
# /Users/spectrasynq/Workspace_Management/Software/claude-review-loop
# Follow the README there for manual registration.

# Verify in a CC session:
/review-loop Add a comment to README.md
```

---

## 6. GSD Verification

In a CC session, run:
```
/gsd:help
```
Should display all available GSD commands.

---

## 7. War Room x-harvester (Optional — create when x-research API key is configured)

Create `/Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom/war-room/04-Operations/Utilities/x-harvester.py` following the pattern of existing harvesters (arxiv-harvester.py, web-harvester.py). It should:
1. Use x-research-skill to search for SpectraSynq-relevant topics
2. Output source notes to `01-Sources/Incoming/`
3. Follow naming convention: `SRC · GTM · X Harvest {date}.md`

---

## 8. Decision Point: CLAUDE.md Worktrunk Update

The implementation guide proposes replacing the manual sandbox section in Lightwave CLAUDE.md with a Worktrunk-based pattern. This has NOT been applied. Review the proposed text in TOOLCHAIN_IMPLEMENTATION_GUIDE.md (Tool 3 section) and approve/modify before applying.

---

## 9. Decision Point: MCP Code Graph vs Nogic — RESOLVED

**Audit result:** Both Nogic and MCP Code Graph are **useless for firmware C++ work.**

- **Nogic** indexed 1 file out of 11,919+ (0.008%). It supports only Python/JS/TS via tree-sitter. 22 of its 36 tools require OpenTelemetry (not viable for embedded). Grade: F for this project.
- **MCP Code Graph** (CodeGPT) is a SaaS product for JS/TS/Python codebases. No C++ support.

**Replacement: clangd-mcp-server** has been cloned, built, and configured as a new MCP server entry in Lightwave's `.mcp.json`. It provides 9 tools: `find_definition`, `find_references`, `get_hover`, `workspace_symbol_search`, `find_implementations`, `get_document_symbols`, `get_diagnostics`, `get_call_hierarchy`, `get_type_hierarchy`.

Permissions for all 9 tools have been added to `settings.local.json`.

**What remains for clangd-mcp-server to work:**

---

## 10. clangd-mcp-server Prerequisites (REQUIRED)

### 10a. Install clangd
```bash
# Option A: via Homebrew (recommended)
brew install llvm
# clangd will be at /opt/homebrew/opt/llvm/bin/clangd
# Add to PATH or set CLANGD_PATH in .mcp.json env

# Option B: via Xcode Command Line Tools (may already be installed)
xcode-select --install
# clangd may appear at /Library/Developer/CommandLineTools/usr/bin/clangd

# Verify:
clangd --version
```

### 10b. Install PlatformIO CLI (if not already on PATH)
```bash
# If PlatformIO is only in VS Code extension, install the CLI:
pip3 install platformio --break-system-packages

# Or if already in ~/.platformio:
export PATH="$HOME/.platformio/penv/bin:$PATH"

# Verify:
pio --version
```

### 10c. Generate compile_commands.json
```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3
pio run -e esp32dev_audio_pipelinecore --target compiledb

# This creates compile_commands.json in the firmware-v3 directory
# Verify:
ls -la compile_commands.json
cat compile_commands.json | head -20
```

### 10d. Verify clangd-mcp-server
```bash
# Quick smoke test — should start and respond to MCP init:
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"capabilities":{}}}' | \
  PROJECT_ROOT=/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3 \
  node /Users/spectrasynq/Workspace_Management/Software/clangd-mcp-server/dist/index.js

# In a CC session, test:
# "Use the clangd tools to find the definition of ControlBus"
# "Show me the call hierarchy of render() in EffectBase"
```

### 10e. (Optional) Update CLANGD_PATH if not on system PATH
If clangd is at a non-standard location, update the `.mcp.json` entry:
```json
"clangd": {
  "command": "node",
  "args": ["/Users/spectrasynq/Workspace_Management/Software/clangd-mcp-server/dist/index.js"],
  "env": {
    "PROJECT_ROOT": "/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3",
    "COMPILE_COMMANDS_DIR": "/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3",
    "CLANGD_PATH": "/opt/homebrew/opt/llvm/bin/clangd",
    "LOG_LEVEL": "WARN"
  }
}
```

---

## 11. Nogic — Retain or Remove?

Nogic remains useful for **Landing Page** work (Next.js/React/TypeScript) and potentially for the **Blender pipeline** (Python). It is configured only on Lightwave-Ledstrip currently. Options:

- **Keep for Landing Page**: Move or duplicate the Nogic config to SpectraSynq.LandingPage where it would actually index the codebase properly
- **Remove from Lightwave**: It serves no purpose for firmware C++ code
- **Do nothing**: It sits idle but doesn't hurt anything
