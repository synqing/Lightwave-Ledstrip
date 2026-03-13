# gstack Install — CC CLI Agent Prompt

Paste the following into your Claude Code CLI session:

---

## One-shot install prompt

```
Install gstack (Claude Code workflow skills by Garry Tan). Execute these steps exactly:

### Step 1: Prerequisites
Check that bun is installed: `bun --version`. If not found, install it: `curl -fsSL https://bun.sh/install | bash` then restart your shell or source the profile.

### Step 2: Clone and build
git clone https://github.com/garrytan/gstack.git ~/.claude/skills/gstack
cd ~/.claude/skills/gstack && ./setup

This will:
- Install node dependencies via bun
- Compile the /browse Playwright binary (~58MB) to browse/dist/browse
- Create symlinks in ~/.claude/skills/ for each skill: browse, plan-ceo-review, plan-eng-review, review, ship, retro

### Step 3: Verify installation
Confirm these exist:
- ~/.claude/skills/gstack/browse/dist/browse (executable binary)
- ~/.claude/skills/browse (symlink → gstack/browse)
- ~/.claude/skills/review (symlink → gstack/review)
- ~/.claude/skills/plan-eng-review (symlink → gstack/plan-eng-review)
- ~/.claude/skills/plan-ceo-review (symlink → gstack/plan-ceo-review)
- ~/.claude/skills/ship (symlink → gstack/ship)
- ~/.claude/skills/retro (symlink → gstack/retro)

### Step 4: Add gstack section to CLAUDE.md
Open the project CLAUDE.md (or global ~/.claude/CLAUDE.md) and add:

## gstack

This project uses [gstack](https://github.com/garrytan/gstack) workflow skills.

Available skills:
- `/plan-ceo-review` — Founder/product thinking mode
- `/plan-eng-review` — Engineering architecture review mode
- `/review` — Paranoid pre-landing code review
- `/ship` — Automated release workflow (sync, test, push, PR)
- `/browse` — Browser-based QA via Playwright
- `/retro` — Weekly engineering retrospective

For web browsing tasks, use the /browse skill from gstack.
If skills aren't working, run: `cd ~/.claude/skills/gstack && ./setup`

### Step 5: Confirm
Run `ls -la ~/.claude/skills/` and show the output to confirm all symlinks are in place.
Then say "gstack installed — 6 skills available" and list them.
```

---

## Troubleshooting

If `./setup` fails:
- **bun not found**: `curl -fsSL https://bun.sh/install | bash && source ~/.bashrc`
- **Binary build fails**: `cd ~/.claude/skills/gstack && bun install && bun run build`
- **Symlinks missing**: `cd ~/.claude/skills/gstack && ./setup` (re-run setup)

## Upgrading later

```
cd ~/.claude/skills/gstack && git fetch origin && git reset --hard origin/main && ./setup
```

## Uninstalling

```
for s in browse plan-ceo-review plan-eng-review review ship retro; do rm -f ~/.claude/skills/$s; done
rm -rf ~/.claude/skills/gstack
```
Then remove the gstack section from CLAUDE.md.
