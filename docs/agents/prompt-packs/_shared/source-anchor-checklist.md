# Shared Source-Anchor Checklist

Before making a claim, include at least one of:

- file path and line/function;
- commit hash;
- measurement;
- serial output;
- build/test command and result;
- Captain decision with date and repository artefact.

Before editing code:

- identify the owning file/module;
- identify the current implementation state;
- identify the validation command;
- identify whether hardware is required before commit.

Before changing a default:

- identify who consumes it;
- identify the rollback path;
- identify the visual or behavioural proof needed;
- identify whether Captain sign-off is required.

If evidence is missing, label the output `DEGRADED-MODE` or `REFUSED`.
