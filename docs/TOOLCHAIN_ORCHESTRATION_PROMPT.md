# SpectraSynq Toolchain Installation — Master Orchestration Prompt

> **Copy this entire file into a fresh Claude Code session to execute all 8 tool installations.**
> The session MUST have filesystem access to `/Users/spectrasynq/`.

---

## YOUR ROLE

You are an infrastructure agent. Your job is to install and configure 8 development tools across the SpectraSynq project ecosystem. You must execute every step methodically, verify each installation before proceeding, and STOP for human input where indicated.

## GOVERNING DOCUMENT

Read this file FIRST — it contains all runbooks, exact paths, configs, and verification steps:
```
/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/docs/TOOLCHAIN_IMPLEMENTATION_GUIDE.md
```

Read it in full before executing anything. The Environment Status table at the top tells you exactly what is and isn't already installed.

## HARD RULES

1. **Do NOT modify any source code.** You are installing tools and updating configs only.
2. **Do NOT modify CLAUDE.md files** without Captain's explicit approval (the guide proposes a Worktrunk update — flag it, don't apply it).
3. **British English** in all comments and documentation you create.
4. **Verify each tool** before moving to the next. If verification fails, STOP and report — do not continue.
5. **Back up config files** before modifying them: `cp file file.bak.$(date +%s)`
6. **API keys** — you will encounter 3 places that need API keys (X Bearer Token, CodeGPT API key, OpenAI API key for AgentKeeper). STOP and ask the user for each. Do NOT proceed without them.

## EXECUTION SEQUENCE

### Phase 0 — Runtime Prerequisites

Execute these in order. Each must succeed before Phase 1.

```bash
# 0.1 — Install Bun
curl -fsSL https://bun.sh/install | bash
source ~/.zshrc
bun --version  # MUST return a version number

# 0.2 — Install pnpm
npm install -g pnpm
pnpm --version  # MUST return a version number

# 0.3 — Install Node 22 (for QMD — keep v20 as nvm default)
nvm install 22
nvm alias default 20  # Keep v20 as default for existing projects
node --version  # Should still show v20

# 0.4 — Verify Python 3.12
python3.12 --version  # MUST show 3.12.x
```

**Checkpoint:** All 4 commands must return valid versions. If any fail, stop and troubleshoot.

---

### Phase 1 — Foundation Tools (independent, can be parallel)

**Tool 3: Worktrunk**
```bash
brew install worktrunk
wt config shell install
source ~/.zshrc
wt --version  # Verify
```
Then create `.worktreeinclude` per the guide (do NOT modify CLAUDE.md).

**Tool 6: Get Shit Done**
```bash
npx get-shit-done-cc@latest --all --global
```
Verify by starting a Claude Code session and running `/gsd:help`.

**Tool 2: Zvec**
```bash
# Python binding (use 3.12, NOT 3.14)
python3.12 -m pip install zvec --break-system-packages

# Node.js binding (for Landing Page)
cd /Users/spectrasynq/SpectraSynq.LandingPage
npm install @zvec/zvec

# Verify
python3.12 -c "from zvec import Collection; print('zvec OK')"
```

Create the shared vector index directory:
```bash
mkdir -p /Users/spectrasynq/Workspace_Management/shared-vector-index
```

---

### Phase 2 — Knowledge Layer

**Tool 4: QMD**
```bash
# MUST use Node 22
nvm use 22
npm install -g @tobilu/qmd
nvm use default  # Switch back to v20

# First run will download ~2GB of models — allow time
qmd --help  # Verify install
```

Set up collections per the guide (all 5 collections + context descriptions).

Then run `qmd embed` — this will take several minutes.

**Create/Update MCP configs** — follow the guide exactly:
- CREATE `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.mcp.json` (does not exist yet)
- UPDATE `/Users/spectrasynq/SpectraSynq.LandingPage/.mcp.json` (currently empty)
- UPDATE `/Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom/.mcp.json` (has youtube-transcript)

Verify: `qmd status` should show all collections healthy.

**Tool 1: x-research-skill**

⚠️ **STOP HERE — Ask Captain for X API Bearer Token before proceeding.**

```bash
# Clone into both project skill directories
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.claude/skills
git clone https://github.com/rohunvora/x-research-skill.git x-research
cd x-research && bun install

cd /Users/spectrasynq/SpectraSynq.LandingPage/.claude/skills
git clone https://github.com/rohunvora/x-research-skill.git x-research
cd x-research && bun install
```

Configure bearer token per the guide.

Create the War Room x-harvester at:
`/Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom/war-room/04-Operations/Utilities/x-harvester.py`

Verify: `bun run x-search.ts search "LED art" --limit 3`

---

### Phase 3 — Agent Intelligence

**Tool 5: AgentKeeper**

⚠️ **STOP HERE — Confirm Captain has Anthropic + OpenAI API keys available for .env config.**

```bash
cd /Users/spectrasynq/Workspace_Management/Software
git clone https://github.com/Thinklanceai/agentkeeper.git
cd agentkeeper
python3.12 -m pip install -r requirements.txt --break-system-packages
cp env.example .env
```

Edit `.env` with API keys. Then seed critical facts per the guide (firmware, landing, warroom agents).

**Tool 7: MCP Code Graph**

⚠️ **STOP HERE — Ask Captain for CodeGPT API key and confirm repos are uploaded to CodeGPT.**

```bash
cd /Users/spectrasynq/Workspace_Management/Software
git clone https://github.com/davila7/mcp-code-graph.git
cd mcp-code-graph
pnpm install
pnpm build
```

Add code-graph to Lightwave `.mcp.json` and Landing Page `.mcp.json` per the guide.

**NOTE:** Nogic is already configured on Lightwave-Ledstrip and provides similar capabilities locally. MCP Code Graph adds value only if Captain wants cross-provider code analysis via CodeGPT. Flag this decision point to Captain.

**Tool 8: Claude Review Loop**
```bash
# Try marketplace first
claude plugin marketplace add hamelsmu/claude-review-loop
claude plugin install review-loop@hamel-review

# If marketplace fails, manual:
cd /Users/spectrasynq/Workspace_Management/Software
git clone https://github.com/hamelsmu/claude-review-loop.git
```

Create the review config at `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.claude/review-config.md` per the guide.

Verify: `/review-loop Add a comment to README.md`

---

### Phase 4 — Permission Updates

**Back up ALL settings files FIRST:**
```bash
cp /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.claude/settings.local.json \
   /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.claude/settings.local.json.bak.$(date +%s)

cp /Users/spectrasynq/SpectraSynq.LandingPage/.claude/settings.local.json \
   /Users/spectrasynq/SpectraSynq.LandingPage/.claude/settings.local.json.bak.$(date +%s)
```

Then add all permission entries listed in the guide for each tool to the appropriate `settings.local.json` files. The guide specifies exact entries for:
- Bun run permissions (x-research)
- Worktrunk (`wt:*`)
- QMD MCP tools (6 entries)
- Code Graph MCP tools (6 entries)

---

### Phase 5 — Post-Installation Verification

Run through the complete post-installation checklist at the bottom of the guide. Every checkbox must pass. Report results to Captain.

---

## DECISION POINTS REQUIRING CAPTAIN'S INPUT

1. **X API Bearer Token** — Required for Tool 1. Cannot proceed without it.
2. **CodeGPT API Key + Repo Upload** — Required for Tool 7. Captain may decide Nogic is sufficient and skip this tool.
3. **OpenAI API Key for AgentKeeper** — Required for Tool 5 cross-provider memory.
4. **CLAUDE.md Worktrunk Update** — The guide proposes replacing the manual sandbox section. Do NOT apply without explicit approval — just report the proposed change.
5. **QMD nvm default** — Currently v20 is default. QMD needs v22. The guide keeps v20 as default and requires `nvm use 22` before QMD operations. Captain may prefer to make v22 the default instead.
6. **MCP Code Graph vs Nogic** — Flag potential redundancy. Nogic already provides local code graph analysis on Lightwave-Ledstrip.

## WHAT SUCCESS LOOKS LIKE

After all phases complete:
- `bun --version`, `pnpm --version`, `wt --version` all return versions
- `nvm use 22 && qmd status` shows all 5 collections indexed
- `python3.12 -c "from zvec import Collection"` succeeds
- `/gsd:help` responds in any CC session
- x-research-skill can search X/Twitter
- AgentKeeper can save/load facts across providers
- Code graph MCP tools appear in CC tool list (if Tool 7 installed)
- Review loop triggers implementation + review cycle (if Tool 8 installed)
- All 3 `.mcp.json` files updated with QMD (+ code-graph where applicable)
- All `settings.local.json` files updated with new tool permissions
- War Room has `x-harvester.py` in Utilities directory
- Shared vector index directory exists at `/Users/spectrasynq/Workspace_Management/shared-vector-index/`
