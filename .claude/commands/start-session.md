---
description: Gather project state and prepare for a development session
---

Gather the current project state and prepare for this session.

## Actions to perform:

1. **Check git status** - Run `git status` and `git branch --show-current` to see current branch and any uncommitted changes

2. **Show recent commits** - Run `git log --oneline -5` to see what was done recently

3. **Check for build errors** - Run `pio run` (dry run) to verify the project compiles

4. **Read CLAUDE.md** - Load the project guidance file for context

5. **Search episodic memory** - Search for recent conversations about this project to restore context from previous sessions

6. **Report findings** - Summarize:
   - Current branch and status
   - Recent work (from commits)
   - Any build issues
   - Previous session context (from memory)

7. **Ask user** - After gathering context, ask: "What would you like to work on this session?"
