# CC Audit Skill Inventory

**Label:** GROUNDED for file counts and marker counts. DEGRADED-MODE for relevance/waste conclusions because transcript markers do not provide a complete invocation contract.

**Scope:** Claude, project, Codex, agent, and plugin-cache skill surfaces visible from the Lightwave-Ledstrip workspace.

---

## Physical Inventory

| Root | `SKILL.md` Total | Disabled | Active |
|---|---:|---:|---:|
| `~/.claude/skills` | 122 | 19 | 103 |
| `.claude/skills` | 31 | 4 | 27 |
| `~/.codex/skills` | 49 | 0 | 49 |
| `~/.agents/skills` | 120 | 19 | 101 |
| `~/.claude/plugins/cache` | 108 | 0 | 108 |
| `~/.codex/plugins/cache` | 78 | 0 | 78 |

Total physical `SKILL.md` files across searched roots: `508`.

Active physical files: `466`.

Unique active skill directory names: `245`.

Active names appearing in multiple roots: `139`.

---

## Domain Classification

| Domain | Unique Active Names | Classification |
|---|---:|---|
| Firmware / embedded / DSP / K1 | 23 | `KEEP` |
| Memory / retrieval / session tooling | 6 | `KEEP` |
| iOS | 6 | `KEEP` |
| Planning / orchestration / review | 86 | `PER-TASK` |
| Web / frontend / browser / Vercel | 38 | `PER-TASK` |
| Infra / backend / Git / auth / plugin | 17 | `PER-TASK` |
| Content / research / ops | 8 | `PER-TASK` |
| Creative / 3D / image | 2 | `PER-TASK` |
| Unclassified mixed plugin/tool names | 59 | `UNKNOWN` |

---

## Invocation Evidence

Searched live Lightwave Claude JSONL under:

`~/.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip`

The search avoided printing raw prompt/tool bodies.

| Evidence Class | Count |
|---|---:|
| JSONL files scanned in SSA sample | 157 |
| Files with skill marker hits | 33 |
| Broad marker hits across user/assistant/attachments | 366 |
| Assistant-role marker hits only | 36 |
| Assistant slash-skill references | 21 |
| Assistant `SKILL.md` references | 14 |
| Assistant explicit "use skill" phrase | 1 |
| XML-ish skill tags | 2 total: 1 assistant `<skill-name>`, 1 user `<skill>` |

Top assistant-side extracted names:

| Name | Count |
|---|---:|
| `brainstorming` | 10 |
| `subagent-driven-development` | 5 |
| `test-driven-development` | 3 |
| `software-architecture` | 2 |
| `dispatching-parallel-agents` | 1 |
| `k1-blender-production` | 1 |

Several non-command-like names also appear through broad path/reference matching. Treat the table above as marker evidence, not a complete invocation ledger.

---

## Audit Conclusion

The skill surface is materially larger than the workflow-routing snapshot. The correct remediation path is not bulk deletion; it is domain classification and per-task activation. Firmware, memory, iOS, review, debugging, and governance skills remain useful. Web, creative, deploy, browser, and external-service skills should be per-task unless current work requires them.
