#!/bin/bash
# QMD Collection Setup — Run on Mac terminal (NOT Cowork VM)
# Requires: NVM with Node 22 installed
# Time: ~2 min (+ ~2GB model download on first `qmd embed`)
set -euo pipefail

echo "=== Activating Node 22 ==="
export NVM_DIR="$HOME/.nvm" && . "$NVM_DIR/nvm.sh" && nvm use 22

echo ""
echo "=== Verifying QMD ==="
qmd --version || { echo "FATAL: qmd not found. Install: npm install -g @nicepkg/qmd"; exit 1; }

echo ""
echo "=== Creating collections ==="

# War Room (Obsidian vault — strategic intelligence)
qmd collection add "$HOME/Workspace_Management/Software/Obsidian.warroom/war-room/03-Knowledge" --name war-room-knowledge 2>/dev/null || echo "war-room-knowledge already exists"
qmd collection add "$HOME/Workspace_Management/Software/Obsidian.warroom/war-room/04-Operations" --name war-room-ops 2>/dev/null || echo "war-room-ops already exists"
qmd collection add "$HOME/Workspace_Management/Software/Obsidian.warroom/war-room/01-Sources/Active" --name war-room-sources 2>/dev/null || echo "war-room-sources already exists"

# Firmware docs (50 markdown files — architecture, effects, API, audio-visual)
qmd collection add "$HOME/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/docs" --name firmware-docs 2>/dev/null || echo "firmware-docs already exists"

# Landing page docs (if exists)
if [ -d "$HOME/SpectraSynq.LandingPage/docs" ]; then
    qmd collection add "$HOME/SpectraSynq.LandingPage/docs" --name landing-page-docs 2>/dev/null || echo "landing-page-docs already exists"
else
    echo "SKIP: landing-page-docs (directory not found)"
fi

echo ""
echo "=== Adding context descriptions ==="
qmd context add qmd://war-room-knowledge "Validated knowledge across domains: AI-Infrastructure, Business-Strategy, Design, Firmware, GTM, Hardware, IP-Legal, Manufacturing, Software" 2>/dev/null || true
qmd context add qmd://war-room-ops "Operational specs, research arenas, harvest digests, agent specs, DNA updates" 2>/dev/null || true
qmd context add qmd://war-room-sources "Processed external intelligence: ArXiv papers, YouTube transcripts, web research" 2>/dev/null || true
qmd context add qmd://firmware-docs "LightwaveOS v3 firmware: API reference, audio-visual mapping, CQRS architecture, effect development standard, constraints, incident postmortems, technical debt audits" 2>/dev/null || true
qmd context add qmd://landing-page-docs "Landing page: architecture, specs, center origin guide, context guide, skills catalog" 2>/dev/null || true

echo ""
echo "=== Building vector index (first run downloads ~2GB of embedding models) ==="
qmd embed

echo ""
echo "=== Verification ==="
qmd status
echo ""
echo "=== Test query ==="
qmd query "beat tracking algorithm and onset detection"
echo ""
echo "=== Test query 2 ==="
qmd query "centre origin constraint for LED effects"
echo ""
echo "DONE. QMD is now indexed and ready for MCP use in CC CLI sessions."
echo "Verify in CC: mcp__qmd__qmd_status should return collection counts."
