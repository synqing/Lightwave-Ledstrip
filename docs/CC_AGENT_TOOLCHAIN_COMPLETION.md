# CC Agent — Toolchain Completion Prompt

> **One-shot execution prompt.** Paste this into a fresh Claude Code CLI session at the Lightwave-Ledstrip project root.
> **Working directory:** `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip`

---

## CONTEXT

A Cowork session completed Phases 0–2 of the SpectraSynq toolchain installation. 8 tools were researched, selected, and partially installed. A 9th tool (clangd-mcp-server) was added to replace the failed Nogic experiment. This prompt completes everything that remains.

### What IS already done
- Node 20.19.5 (default) + Node 22.22.0 (via nvm)
- pnpm 10.30.3 (global)
- Bun 1.3.10 (at `~/.bun/bin/bun` — NOT on default shell PATH)
- PlatformIO CLI 6.1.19 (at `/opt/homebrew/bin/pio`)
- Python 3.12.12 (at `python3.12`)
- Worktrunk 0.28.2 (via Homebrew)
- Zvec installed under python3.12
- QMD 1.0.7 (installed globally under Node 22)
- x-research-skill cloned to `.claude/skills/x-research` in both Lightwave and LandingPage
- ~~AgentKeeper~~ **CANCELLED 5 Mar 2026** — $37/mo, no docs, no API key. Delete repo at `/Users/spectrasynq/Workspace_Management/Software/agentkeeper/`
- ~~MCP Code Graph~~ **REMOVED 5 Mar 2026** — redundant (Auggie + clangd + QMD + Context7 cover all needs). Repo still at `/Users/spectrasynq/Workspace_Management/Software/mcp-code-graph/` — can be deleted.
- Claude Review Loop cloned at `/Users/spectrasynq/Workspace_Management/Software/claude-review-loop/`
- clangd-mcp-server cloned + built at `/Users/spectrasynq/Workspace_Management/Software/clangd-mcp-server/`
- `.mcp.json` created for Lightwave (qmd, clangd), updated for LandingPage and War Room. Code Graph entry removed.
- `settings.local.json` updated with permissions for MCP tools (qmd ×6, clangd ×9). Code Graph permissions removed.
- `.worktreeinclude`, `review-config.md` created
- `shared-vector-index/` directory created

### What is DEFERRED (manual / Captain's discretion)
- **x-research-skill (Tool 1):** Requires X Developer API Basic tier ($100/month). Captain will decide whether to subscribe. The skill files are already cloned — only the Bearer Token is missing. If skipped, consider a browser-use MCP as a free alternative for X/Twitter research.
- **War Room x-harvester:** Depends on x-research API access. Deferred until/unless Tool 1 is activated.

### What IS NOT done (your job)
All items below, executed in the order given.

---

## PHASE 1: REMOVE NOGIC (completely)

Nogic was audited and scored F for this project — it indexed 1 file out of 11,919+, supports only Python/JS/TS, and is useless for C++ firmware. Remove all traces.

### 1a. Remove Nogic permission from Lightwave settings.local.json

File: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.claude/settings.local.json`

Find and DELETE this exact line (currently at line 592, but confirm by searching):
```
      "mcp__nogic__find_issues",
```

**Important:** Remove the line entirely. Ensure the surrounding JSON remains valid (no trailing comma issues).

### 1b. Uninstall Nogic uv tool

```bash
uv tool uninstall nogic 2>/dev/null || echo "nogic not installed via uv tool"
pip3.12 uninstall nogic -y 2>/dev/null || echo "nogic not installed via pip"
```

### 1c. Clean Nogic cache

```bash
# Remove any cached Nogic data
rm -rf ~/.nogic 2>/dev/null
# Remove Nogic from uv cache if present
find ~/.cache/uv/archive-v0/ -maxdepth 2 -name "nogic" -type d -exec rm -rf {} + 2>/dev/null
```

### 1d. Verify removal

```bash
grep -rn "nogic" /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.claude/settings.local.json
# Should return NOTHING

uv tool list 2>/dev/null | grep nogic
# Should return NOTHING

python3.12 -c "import nogic" 2>&1
# Should say ModuleNotFoundError
```

---

## PHASE 2: INSTALL CLANGD

clangd is the language server that powers the clangd-mcp-server. It is NOT currently installed.

```bash
brew install llvm
```

After installation, find where clangd landed:
```bash
# Check standard Homebrew location
ls /opt/homebrew/opt/llvm/bin/clangd && echo "Found at Homebrew LLVM"

# Or check if it's on PATH
which clangd
clangd --version
```

**If clangd is NOT on the default PATH** (likely — Homebrew LLVM doesn't auto-link), update the clangd MCP server config:

Edit `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.mcp.json` and add `CLANGD_PATH` to the `clangd` server env:

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

Replace the `CLANGD_PATH` value with wherever `clangd` actually is on this system.

---

## PHASE 3: GENERATE compile_commands.json

clangd-mcp-server requires a compilation database. PlatformIO generates this.

```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3
pio run -e esp32dev_audio_pipelinecore --target compiledb
```

This may take several minutes (full build + compiledb generation). When complete:

```bash
ls -la compile_commands.json
# Should exist and be >100KB
head -20 compile_commands.json
# Should show JSON array of compilation commands
```

**If the build fails:** The `esp32dev_audio_pipelinecore` environment may need libraries installed first. Run `pio pkg install -e esp32dev_audio_pipelinecore` then retry.

---

## PHASE 4: FIX BUN PATH

Bun 1.3.10 is installed at `~/.bun/bin/bun` but is NOT on the default shell PATH. Fix this so x-research-skill and any future bun-based tools work.

```bash
# Add to .zshrc if not already present
grep -q 'BUN_INSTALL' ~/.zshrc || echo '
# Bun
export BUN_INSTALL="$HOME/.bun"
export PATH="$BUN_INSTALL/bin:$PATH"' >> ~/.zshrc

source ~/.zshrc

# Verify
which bun
bun --version
# Should output: 1.3.10
```

---

## PHASE 5: WORKTRUNK SHELL INTEGRATION

Worktrunk is installed but the shell hook isn't active.

```bash
wt config shell install
source ~/.zshrc

# Verify
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip
wt list
```

---

## PHASE 6: QMD COLLECTION SETUP + INDEXING

QMD is installed but has zero collections. This step downloads ~2GB of embedding models on first run.

```bash
# QMD requires Node 22
export NVM_DIR="$HOME/.nvm" && . "$NVM_DIR/nvm.sh" && nvm use 22

# Create 5 collections
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

# Build the index (downloads models on first run — expect ~2GB + several minutes)
qmd embed

# Verify
qmd status
qmd query "gamut mapping for LED hardware"
```

---

## ~~PHASE 7: AGENTKEEPER~~ — CANCELLED

AgentKeeper has been cancelled (5 Mar 2026). $37/mo, no documentation, no API key mechanism, unclear pricing units. Critical facts are already handled by CLAUDE.md hard constraints (loaded deterministically every session) + Claude-mem (7 tools for cross-session search/observations/timeline).

**Cleanup:** Delete the AgentKeeper repo if it exists:
```bash
rm -rf /Users/spectrasynq/Workspace_Management/Software/agentkeeper/
```

---

## PHASE 8: CLAUDE REVIEW LOOP

```bash
# Try marketplace first:
claude plugin marketplace add hamelsmu/claude-review-loop
claude plugin install review-loop@hamel-review

# If marketplace fails, install manually from cloned repo:
# Repo is at /Users/spectrasynq/Workspace_Management/Software/claude-review-loop
# Follow the README there for manual plugin registration.

# Verify:
/review-loop --help
```

---

## PHASE 9: GSD VERIFICATION

Get Shit Done was installed previously. Verify it works:

```bash
/gsd:help
```

If it fails or `gsd` is not found, reinstall:
```bash
claude plugin install get-shit-done
/gsd:help
```

---

## PHASE 10: SMOKE TEST EVERYTHING

Run this comprehensive verification sequence. Every check must pass.

```bash
echo "========================================="
echo "  TOOLCHAIN VERIFICATION — $(date)"
echo "========================================="

echo ""
echo "--- Tool 1: x-research-skill (DEFERRED — manual) ---"
ls /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.claude/skills/x-research/x-search.ts && echo "PASS: skill file exists (API key deferred)" || echo "FAIL"
# x-research requires X API Basic tier ($100/mo). Captain decides. Files are cloned; only Bearer Token missing.

echo ""
echo "--- Tool 2: Zvec ---"
python3.12 -c "import zvec; print(f'PASS: zvec {zvec.__version__}')" 2>&1 || echo "FAIL"

echo ""
echo "--- Tool 3: Worktrunk ---"
wt --version && echo "PASS" || echo "FAIL"

echo ""
echo "--- Tool 4: QMD ---"
export NVM_DIR="$HOME/.nvm" && . "$NVM_DIR/nvm.sh" && nvm use 22 > /dev/null 2>&1
qmd status && echo "PASS" || echo "FAIL"
nvm use 20 > /dev/null 2>&1

echo ""
echo "--- Tool 5: AgentKeeper ---"
echo "CANCELLED — deleted. $37/mo with no docs, no API key mechanism."

echo ""
echo "--- Tool 6: GSD ---"
echo "(verify via /gsd:help in CC session)"

echo ""
echo "--- Tool 7: MCP Code Graph ---"
echo "REMOVED — redundant (5 Mar 2026). Repo can be deleted."

echo ""
echo "--- Tool 8: Claude Review Loop ---"
ls /Users/spectrasynq/Workspace_Management/Software/claude-review-loop/README.md && echo "PASS: cloned" || echo "FAIL"
echo "(verify via /review-loop in CC session)"

echo ""
echo "--- Tool 9: clangd-mcp-server ---"
ls /Users/spectrasynq/Workspace_Management/Software/clangd-mcp-server/dist/index.js && echo "PASS: built" || echo "FAIL"
clangd --version 2>/dev/null && echo "PASS: clangd binary" || echo "FAIL: clangd not installed or not on PATH"
ls /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/compile_commands.json 2>/dev/null && echo "PASS: compile_commands.json" || echo "FAIL: compile_commands.json missing"

echo ""
echo "--- Nogic removal ---"
grep -c "nogic" /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.claude/settings.local.json 2>/dev/null
# Should output: 0

echo ""
echo "--- Bun on PATH ---"
which bun && bun --version && echo "PASS" || echo "FAIL: bun not on PATH"

echo ""
echo "========================================="
echo "  VERIFICATION COMPLETE"
echo "========================================="
```

---

## PHASE 11: DOCUMENT UPDATES

### 11a. Update CC_HANDOFF_REMAINING_TASKS.md

File: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/docs/CC_HANDOFF_REMAINING_TASKS.md`

Add a completion summary at the top of the file, above the existing `# CC CLI Handoff` header:

```markdown
# ✅ TOOLCHAIN INSTALLATION COMPLETE

**Completed by CC agent on [TODAY'S DATE]**

All 9 tools installed and verified. Nogic removed. See verification output below.

Remaining manual items:
- [ ] X API Bearer Token (Section 1) — Captain must provide (deferred, $100/mo)
- [ ] Delete AgentKeeper repo at `/Users/spectrasynq/Workspace_Management/Software/agentkeeper/` (cancelled)
- [ ] War Room x-harvester (Section 7) — optional, create after X API key is set
- [ ] Delete `/Users/spectrasynq/Workspace_Management/Software/mcp-code-graph/` (Code Graph removed 5 Mar 2026)

---
```

### 11b. Update Section 11 (Nogic) in the same file

Replace the entire Section 11 content with:

```markdown
## 11. Nogic — REMOVED

Removed on [TODAY'S DATE]. All traces cleaned:
- Permission entry removed from settings.local.json
- uv tool uninstalled
- Cache cleared from ~/.cache/uv/ and ~/.nogic
- Replaced by clangd-mcp-server (Tool 9) for C++ code intelligence
```

### 11c. Update CLAUDE.md Parallel Agent Sandboxing section (Decision Point 8)

File: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/CLAUDE.md`

Find the section `## Parallel Agent Sandboxing (MANDATORY)` and append after the existing content (after `**When to use:** Any time 2+ agents will modify source code, build, or run tests on the same codebase concurrently.`):

```markdown

**Worktrunk alternative:** For CC CLI sessions, `wt` (Worktrunk) can manage worktrees instead of manual `cp -r` sandboxing. Run `wt create <name>` to create an isolated worktree, work in it, then `wt delete <name>` when done. The `.worktreeinclude` file at the project root shares `.pio/build/` and `node_modules/` between worktrees to avoid redundant builds.
```

---

## PHASE 12: FINAL REPORT

After completing all phases, output a summary in this format:

```
TOOLCHAIN COMPLETION REPORT
============================
Phase 1  — Nogic removal:        [PASS/FAIL]
Phase 2  — clangd installed:     [PASS/FAIL] (version: ...)
Phase 3  — compile_commands.json: [PASS/FAIL] (size: ... KB)
Phase 4  — Bun on PATH:          [PASS/FAIL]
Phase 5  — Worktrunk shell:      [PASS/FAIL]
Phase 6  — QMD collections:      [PASS/FAIL] (N collections, indexed)
Phase 7  — AgentKeeper:          CANCELLED (repo deleted)
Phase 8  — Review Loop:          [PASS/FAIL]
Phase 9  — GSD:                  [PASS/FAIL]
Phase 10 — Smoke tests:          [N/N passed]
Phase 11 — Docs updated:         [PASS/FAIL]

BLOCKED ITEMS (require Captain):
- ...
```

---

## DECISION POINTS (ask Captain if encountered)

1. ~~**AgentKeeper API keys**~~ — CANCELLED. Delete the repo instead.
2. **compile_commands.json build failure** — If `pio run --target compiledb` fails, report the exact error. Captain may need to resolve PlatformIO library dependencies first.
3. **x-research-skill** — DEFERRED. Requires X Developer API Basic tier ($100/month). Do NOT attempt to configure this. Captain will handle separately. Files are already in place.
4. ~~**CodeGPT API key**~~ — MCP Code Graph has been REMOVED (5 Mar 2026). No action needed.

---

## FILE REFERENCE

| File | Purpose |
|------|---------|
| `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.mcp.json` | MCP server configs (qmd, clangd) |
| `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.claude/settings.local.json` | CC permissions (~620 lines) |
| `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/docs/CC_HANDOFF_REMAINING_TASKS.md` | Handoff doc to update |
| `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/CLAUDE.md` | Project instructions — update sandboxing section |
| `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/docs/TOOLCHAIN_IMPLEMENTATION_GUIDE.md` | Master reference (read-only, don't modify) |
| ~~`/Users/spectrasynq/Workspace_Management/Software/agentkeeper/`~~ | **DELETE** — AgentKeeper cancelled |
| `/Users/spectrasynq/SpectraSynq.LandingPage/.mcp.json` | Landing Page MCP configs |
| `/Users/spectrasynq/Workspace_Management/Software/Obsidian.warroom/.mcp.json` | War Room MCP configs |
