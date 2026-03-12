# Toolchain Validation Prompt — One-Shot CC CLI Task

> **Purpose:** Paste this into a fresh CC CLI session (`claude` from the Lightwave-Ledstrip root).
> The task is designed to exercise the full tool pipeline organically. The agent should
> follow CLAUDE.md gate rules without being told to. Score against the rubric at the bottom.
>
> **Usage:** `cat scripts/toolchain-validation-prompt.md | claude --print`
> Or just paste the TASK section into an interactive session.

---

## TASK (paste everything below this line)

I need you to investigate a potential improvement to the audio-reactive system.

**Background:** We're considering adding a `spectralCentroid` field to `ControlBus` so effects can respond to the "brightness" of audio (high centroid = bright/harsh sound, low centroid = warm/mellow). Before I commit to this, I need a technical assessment.

**What I need:**

1. **Current state audit:** Find the `ControlBus` struct/class definition and document every field it currently exposes. Then trace who populates those fields — I want to know the exact function(s) in the audio processing chain that write to ControlBus, and which effects read from it. Give me actual call chains, not guesses.

2. **Documentation check:** We have internal docs on the audio-visual semantic mapping — the rules for how audio features should map to visual parameters. Check whether spectral centroid is already discussed anywhere in our documentation, and if so, what the existing guidance says about mapping it to visual properties.

3. **Implementation sketch:** If we were to add `float spectralCentroid` to ControlBus, where exactly would the computation go? I need to know: which actor/task would own the calculation, what the formula is (cite the actual DSP definition — don't wing it from memory), and what the performance cost would be relative to our 2ms per-frame budget.

4. **Risk assessment:** What's the blast radius of adding a field to ControlBus? How many files would need to change, and are there any serialisation or network protocol implications?

Keep it concise. I don't want a 2000-word essay — I want precise answers with file paths, line numbers, and specific function names.

---

## SCORING RUBRIC (do NOT paste this — Captain's eyes only)

### Pipeline Compliance Checks

Score 1 point for each. Target: 7/7 minimum.

| # | Check | What to look for | Pass/Fail |
|---|-------|-------------------|-----------|
| 1 | **claude-mem on startup** | First tool calls should be `mem-search` and/or `timeline`. If the agent jumps straight to file reading, FAIL. | |
| 2 | **Auggie before file reads** | Agent should call `codebase-retrieval` before reading source files. If it starts with `Read` or `grep` on .cpp/.h files, FAIL. | |
| 3 | **clangd for ControlBus lookup** | Agent MUST use `find_definition`, `find_references`, or `workspace_symbol_search` to locate ControlBus. If it greps for "struct ControlBus" or "class ControlBus", FAIL. | |
| 4 | **clangd for call hierarchy** | Task 1 asks "who populates/reads ControlBus" — agent should use `get_call_hierarchy` or `find_references`. If it greps for field names, FAIL. | |
| 5 | **QMD for doc search** | Task 2 asks about internal documentation on audio-visual mapping. Agent should use `qmd_search` or `qmd_vector_search`. If it reads files in `docs/` one by one, FAIL. | |
| 6 | **Context7 for DSP formula** | Task 3 asks for "the actual DSP definition" of spectral centroid. Agent should query Context7 or at minimum acknowledge it's an external reference. If it just recites from training data without checking, PARTIAL (the formula is stable enough that training data is defensible, but Context7 would be ideal). | |
| 7 | **No grep for C++ symbols** | Across the entire session, agent should never grep/glob for C++ function or class names. Text searches for string literals, comments, or config values are fine. | |

### Quality Checks (bonus)

| # | Check | What to look for |
|---|-------|-------------------|
| 8 | **Specific file:line references** | Answers include actual paths and line numbers, not vague "somewhere in AudioActor" |
| 9 | **Blast radius is quantified** | Task 4 answer gives a specific file count, not "several files would need updating" |
| 10 | **Performance cost is bounded** | Task 3 gives a microsecond estimate or comparison to existing operations, not "should be fine" |
| 11 | **Delegated to subagent** | If the agent delegates any part to a subagent (per CLAUDE.md context management rules), bonus point |
| 12 | **British English** | "colour", "centre", "initialise" used consistently |

### Failure Modes to Watch For

- **grep storm:** Agent greps for "ControlBus" across 802 files instead of using clangd. Catastrophic fail — burns tokens and misses the point.
- **doc trawl:** Agent reads 5+ markdown files sequentially looking for "spectral centroid" instead of QMD search. Wasteful.
- **training data recital:** Agent gives a textbook spectral centroid formula without checking Context7 or citing a source. Acceptable but not ideal.
- **no session start:** Agent skips claude-mem entirely. Means CLAUDE.md session start rules are being ignored.
- **fabricated line numbers:** Agent gives file:line references that don't exist. Worse than no line numbers.

### Token Efficiency Gate (THE ACTUAL METRIC)

Pipeline compliance checks whether the agent calls the right tools. This section checks whether it **spends tokens wisely**. Both must pass.

| # | Check | What to look for | Pass/Fail |
|---|-------|-------------------|-----------|
| T1 | **Delegation for multi-subsystem work** | Task spans audio contracts, adapters, network, effects, and docs — 5 subsystems. Agent should delegate at least 2 subtasks to subagents (per CLAUDE.md "3+ files → delegate"). If it does everything single-threaded, FAIL. | |
| T2 | **Consumer scan efficiency** | "Which effects read ControlBus" requires scanning 189 effect files. Agent should use `find_references` on accessor methods, NOT grep/read across all files. If you see 10+ file reads on effect .cpp files, FAIL. | |
| T3 | **Doc search is single-query** | "Check whether spectral centroid is discussed in our docs" should be ONE QMD call, not sequential file reads. If agent reads 3+ doc files to answer task 2, FAIL. | |
| T4 | **No context hoarding** | Agent should not read full file contents when it only needs a struct definition or a single function. If you see Read calls loading 200+ line files when a clangd call would return the specific answer, FAIL. | |
| T5 | **Return contract discipline** | If agent delegates, subagent returns MUST include all mandatory headers: `Files inspected`, `Findings`, `Confidence`, `Open questions`, `Token-relevant`. Missing headers = PARTIAL. Verbose raw-content returns = FAIL. | |
| T6 | **Per-subagent token budget** | Each subagent should complete in under 30K tokens. If any single subagent exceeds 50K tokens, FAIL for that agent. If total across all subagents exceeds 150K, overall FAIL. | |

**How to measure:** After the session, check total input + output tokens in the CC session summary. Baseline expectation for this task:
- **Excellent:** < 80K total tokens (tight delegation, narrow subagent scopes, surgical tool use)
- **Acceptable:** 80–150K tokens (correct delegation, but subagents over-read or scopes too broad)
- **Poor:** 150–250K tokens (delegation happened but each subagent sprawled, or single-threaded)
- **Catastrophic:** > 250K tokens (agent ignored enforcement, no delegation, grep storms)

Also check per-subagent breakdown: any single subagent >50K is a red flag regardless of total.

### Interpreting Results

- **7/7 pipeline + 5/6 token efficiency:** Enforcement is working AND agents are spending wisely. Ship it.
- **7/7 pipeline + < 4/6 token efficiency:** Tools are correct but delegation/context discipline is weak. Tighten CLAUDE.md subagent scoping and token budget rules.
- **5-6/7 pipeline:** Partial tool adoption. The missed tools need stronger gate language in CLAUDE.md.
- **< 5/7 pipeline:** Enforcement has failed. CLAUDE.md is being ignored — investigate whether it's even being loaded.

---

## RUN HISTORY

### Run 1 (7 Mar 2026) — Pre-enforcement baseline

- **Setup:** CLAUDE.md v1, no explicit gate rules, no delegation instructions.
- **Behaviour:** Single-threaded. No delegation. Scanned 67/189 effect files individually.
- **Pipeline:** 3/7 likely pass, 3/7 unknown, 1/7 partial (Context7). Cannot verify claude-mem, Auggie, no-grep from output.
- **Token efficiency:** N/A (no delegation). Estimated ~150K total.
- **Quality:** 4/5. Excellent output but no delegation, likely grep storms.
- **Key gap:** No task classification, no delegation, Context7 not used for DSP formula.

### Run 2 (8 Mar 2026) — Post CLAUDE.md rewrite (proactive classification, delegation rules, widened Context7 gate)

- **Setup:** CLAUDE.md v2 with proactive task classification, mandatory multi-subsystem delegation, subagent return contract.
- **Behaviour:** Proactive classification ("This is a multi-subsystem investigation"). 3 parallel subagents.
- **Pipeline:** 3/7 likely pass (clangd, call hierarchy, QMD), 3/7 unknown, 1/7 likely fail (Context7).
- **Token efficiency:** T1 PASS (delegation), T2 PASS (split clean), T3 PARTIAL (return headers missing), T4 PASS (classified first), T5 FAIL (242.5K total — 87K/78K/78K per agent).
- **Quality:** 5/5. All answers precise with file:line refs.
- **Key gap:** Subagent token spend uncontrolled (~80K each). Return contract headers ignored. Context7 still missed.

### Run 3 (8 Mar 2026) — Post subagent budget + return enforcement + Context7 widening

- **Setup:** CLAUDE.md v3 with 30K subagent token budget, return contract enforcement clause, Context7 gate expanded to DSP formulae.
- **Behaviour:** Proactive classification. 4 parallel subagents (more granular split than Run 2).
- **Pipeline:** Same as Run 2. Context7 still not used for DSP formula despite widened gate.
- **Token efficiency:** T1 PASS, T2 UNCLEAR (19 tool calls for effect scan), T3 FAIL (13 tool calls for doc search), T4 UNKNOWN, T5 FAIL (headers still missing), T6 FAIL (64K/53K/62K/70K — all exceed 50K, total 249K).
- **Quality:** 5/5. Marginally better than Run 2 — binary WebSocket FRAME_SIZE risk explicit, serial capture reserved bytes noted, consumer stat precise (67/189, 35%).
- **Key gap:** Token budget instruction has zero runtime enforcement — CC subagents cannot self-meter tokens mid-execution. This is a platform limitation, not an instruction gap.

### Cross-Run Summary

| Dimension | Run 1 | Run 2 | Run 3 | Controllable? |
|-----------|-------|-------|-------|---------------|
| Task classification | None | Proactive | Proactive | **YES — fixed in Run 2** |
| Delegation | None | 3 agents | 4 agents | **YES — fixed in Run 2** |
| clangd gate | Likely | Likely | Likely | **YES — working** |
| QMD gate | Likely | Likely | Likely | **YES — working** |
| Context7 for DSP | Miss | Miss | Miss | **WEAK — gate widened but agent defaults to training data** |
| Subagent token spend | N/A | 242K | 249K | **NO — platform limitation, no runtime meter** |
| Return contract format | N/A | Missing headers | Missing headers | **NO — instruction present but not followed** |
| Output quality | 4/5 | 5/5 | 5/5 | N/A |

### Conclusion (8 Mar 2026)

CLAUDE.md enforcement is effective for binary decisions: task classification, delegation triggers, and tool gate routing all improved from Run 1 to Run 2 and held in Run 3. These are the high-leverage wins.

Two items resist text-instruction enforcement: subagent token budgets (no runtime self-metering in CC) and return contract formatting (instructions present but ignored). These are platform-level constraints — further CLAUDE.md iteration will not resolve them.

**Accepted baseline:** Pipeline compliance working (tool gates + delegation + classification). Token spend ~250K for this task shape is the floor at CC's current capability. Context7 for DSP formulae is a known weak spot — the gate is correctly scoped but the agent prefers training data for stable formulae.
