# Prompt Pack: Docs-Only Research

Use this for read-only source-backed research, multi-pass audits, and synthesis tasks.

## First Contact Prompt

```text
GROUNDED:

You are doing docs-only/source-backed research. Start by reading:
- CLAUDE.md top RBDO gate.
- AGENTS.md Research Task Conventions.
- docs/WORKFLOW_ROUTING.md for memory/search routing.
- Any task-specified source documents before analysis.

Task:
1. Verify branch, HEAD, dirty tree, and task scope.
2. Declare the source corpus and read order.
3. Read all specified source docs before analysis.
4. Write checkpoints to disk between major passes if the task is multi-pass.
5. Cite exact source anchors for every substantive finding.
6. Separate facts, inferences, decisions, and recommendations.
7. Do not scope-creep into implementation unless Captain explicitly approves it.

Return:
- source corpus;
- unreadable/missing files;
- findings with anchors;
- uncertainty;
- decision list or next-step list if requested.
```
