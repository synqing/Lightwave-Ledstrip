---
name: ssa-management
description: Fans out and manages specialist subagent swarms safely. Use before spawning subagents, when deciding what to delegate, when classifying evidence as load-bearing/optional, when subagents disagree, or before committing/deploying/publishing based on a subagent claim. Requires consumption contracts, on-disk evidence paths, contradiction resolution, validator audit, and an orchestrator re-run of decision-critical claims.
---

# SSA-Management — consuming subagent evidence without laundering prose into fact

## 1 · The one law (and the flagship move)
A subagent (SSA) multiplies your **context** and your **coverage** — **never your judgment.** Its
output is **prose, not proof**: fluent and confident even when wrong. Treat every returned claim as a
*hypothesis with a fingerprint*, not a finding.

**The flagship move — the re-run.** Before any *decision-critical* claim becomes a decision, a
commit, or a doc, the **orchestrator personally re-runs the decisive artifact** (its command, with
your own eyes). That single act breaks the "agents checking agents forever" regress. Everything below
exists to serve it.

## 2 · Specialist swarm fan-out (before launch)
1. **Classify the task domain** — map problem → specialist (see `discover-specialists` or project `.claude/agents/`).
2. **Minimum viable swarm** — start with one probe; add specialists only when they cover a named blind spot (never fan out for a one-line lookup).
3. **Parallel only when independent** — launch concurrent SSAs in a single message when evidence questions do not depend on each other; serialize when B needs A's artifact path.
4. **One brief per SSA** — paste the launch contract (§5) into every Task/subagent prompt; reject returns that ignore the return contract (§6).
5. **Orchestrator stays editor** — SSAs read/measure/draft; orchestrator edits, commits, flashes, and re-runs decision-critical commands.

## 3 · When to invoke
- Before spawning / fanning out subagents.
- Deciding what to **delegate vs keep** (§4).
- **Classifying** a returned claim — optional / load-bearing / decision-critical (§7).
- Two subagents **disagree**, or a result is catastrophic / counter-intuitive.
- Before you **commit / flash / deploy / publish / canonise** on a subagent claim.

## 4 · Delegate vs keep
| Delegate to SSAs (their context) | Keep on the orchestrator |
|---|---|
| heavy reading, recon, archaeology | the code / firmware **edit** |
| exploratory measurement, sweeps, breadth search | **build / test verification** in the real env |
| drafting in-repo docs | the **commit**, the **device action** |
| **adversarial audit of each other** | the **final synthesis** + **re-run of any decision-critical claim** |

Why: the SSA reads in *its* window and returns a verdict into *yours* — you buy coverage and spare
your context. A fan-out that returns raw logs into your window defeats its own purpose.

## 5 · Launch contract (paste into every SSA task)
```
SSA BRIEF
  Task ID:
  Evidence question:        <the ONE thing it must answer>
  Default verdict:          NOT_VERIFIED   (grade up only on reproducible evidence; try to REFUTE it)
  Classification:           load-bearing | optional
  Blast radius:             <what acting on a wrong answer would cost>
  Scope / Out of scope:
  Allowed tools:            read-only | write-scratch-only | execute-only | write-repo-allowed
                            (validators DEFAULT to read-only or scratch — NO commit/flash/deploy
                             tools inside an SSA unless the task explicitly justifies it)
  Write evidence to:        <on-disk path — full evidence lands HERE, not in the return>
  Required artifact:        <script / log / dataset it must leave behind>
  Required re-run command:  <exact command the orchestrator can run, or NONE + why>
  Return format:            SSA RETURN CONTRACT only (§6), <= 10 lines
  Failure mode to watch:
  Checkpoint / Fallback:    <wall-clock limit; owner + action if it hangs or contradicts>
```

## 6 · SSA return contract (mandatory shape — reject narrative)
```
STATUS:       VERIFIED | NOT_VERIFIED | CONTRADICTORY | BLOCKED
CLAIM:        <one sentence>
EVIDENCE:     <path(s) written to disk>
COMMAND:      <exact re-run command, or NONE + why>
METHOD_RISK:  <the main way this result could be wrong>
NEXT:         <what the orchestrator should do>
```
A return that is polished prose instead of this shape has not done its job — re-issue it. Put this
schema in every brief (§5) so the prose-as-proof failure cannot recur at the boundary.

## 7 · Consumption algorithm (how the orchestrator ingests a return)
```
1. CLASSIFY the claim:        optional | load-bearing | decision-critical
2. CHECK the artifact:        evidence path exists · command exists · method is inspectable
3. optional          -> consume as PROVISIONAL context only
4. load-bearing      -> audit the METHOD · check for contradictions · require artifact + command
5. decision-critical -> the orchestrator PERSONALLY RE-RUNS the decisive command,
                        records the result in the ledger, and ONLY THEN acts / reports / canonises
6. contradictory     -> do NOT average; spawn a decisive resolver, or re-run it yourself
```

## 8 · Calibrate effort to blast radius
| Stakes | Swarm | Verification |
|---|---|---|
| orientation only | 1 probe | provisional; **must not silently promote into a load-bearing claim** |
| input to a reversible decision | 1–2 probes | sanity-check the one number |
| a commit / a doc you'll canonise | adversarial probe(s) | **orchestrator re-runs the decisive metric** |
| a flash / irreversible / strategic fork | adversarial panel + contradiction-resolution | **re-run + cross-confirm; default NOT_VERIFIED** |

Add agents only when they demonstrably improve the outcome — never because fan-out feels powerful.

## 9 · Stop gates (hard)
- About to **relay an SSA claim as fact** without your own re-run of a decision-critical claim → STOP; re-run.
- Two load-bearing probes **contradict** → STOP; resolve, never average.
- A result is **catastrophic / counter-intuitive** → STOP; **audit the validator** before the meat (a bent scale can't weigh it — see `load-bearing-edges`).
- A load-bearing SSA **hangs** past its checkpoint → STOP polling; partial → one bounded retry → fallback.
- About to **fan out many agents for a one-line lookup** → STOP; do it yourself.

## 10 · Delegation ledger (carry it in any answer built on load-bearing SSAs)
```
ID · SSA/task · Claim · Classification ·
Status: received | replaced | missing | blocked | contradicted | verified ·
Evidence path · Re-run command · Orchestrator re-run result · Contradictions ·
CONSUMED AS:  provisional context | verified evidence | rejected      <- the load-bearing field ·
Decision impact
```
**`CONSUMED AS`** is where a subagent's prose either stays provisional or becomes evidence. Nothing
is "verified evidence" without an **orchestrator re-run result** beside it.

## 11 · References & companions
- **`references/v-octave-near-disaster.md`** — the costly scar this skill is built on: the fan-out
  bought coverage, but *consumption* failed — multiple fluent SSA claims were treated as findings
  before their methods and decisive artifacts were re-run, nearly driving the revert of a committed,
  hardware-validated gain. Read it once.
- Companions: **`discover-specialists`** (pick the right specialist before fan-out) ·
  **`load-bearing-edges`** (audit the validator; don't canonise edge-incomplete prose) ·
  **`codex-offload`** (route bounded reading to Codex's tokens; re-verify in the real env).
- The consumption-contract + stuck-agent-recovery *structure* lives in `.claude/CLAUDE.md` /
  `AGENTS.md` (parallel-agent orchestration discipline). This skill is the **evidence-consumption
  discipline** layer over that structure — its sharper category is not "how to fan out" (fan-out is
  commodity) but "how an orchestrator consumes subagent evidence without laundering prose into fact."

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-06-04 | agent:CTO | Created — SSA deployment + consumption doctrine. |
| 2026-06-04 | agent:CTO | v0.2 rework per review: trigger-surface frontmatter; mandatory return contract; consumption algorithm; ledger schema. |
| 2026-06-26 | agent:cursor | v0.3 — specialist swarm fan-out section; `discover-specialists` companion; frontmatter names swarm management. |
