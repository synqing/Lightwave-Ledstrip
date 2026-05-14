# Findings: Git Provenance Audit

## Local Repo State
- `origin` points to `https://github.com/synqing/Lightwave-Ledstrip.git`.
- A second remote, `synqing`, points to `https://github.com/synqing/claude-mem.git`.
- `git merge-base --all HEAD synqing/main` returned no merge-base.
- `git branch -a --contains 598369e8942eb731651e391b01f60bb7329cc57c` lists only `remotes/synqing/*`.
- `git log --branches --remotes=origin --author='Alex Newman|thedotmack|Uchida|Masayuki|Ben Younes|Ousama|Ethan'` returned zero commits.

## Author Clusters
- Lightwave local branches plus `origin/*`: 1,229 commits; top author identities are SpectraSynq, K1 Research Agent, qaxzy, and synqing. None of the named suspect identities are present.
- `synqing/*` remote: 1,825 commits, 120 author identities.
- `synqing/*` single-commit author identities: 73.
- `synqing/*` named identities:
  - `Alex Newman <thedotmack@gmail.com>`: 1,531 author commits.
  - `thedotmack <thedotmack@gmail.com>`: 3 author commits.
  - `Ousama Ben Younes <benyounes.ousama@gmail.com>`: 20 author commits.
  - `Ben Younes <benyounes.ousama@gmail.com>`: 7 author commits.
  - `UCHIDA Masayuki <antilogic@hey.com>`: 1 author commit.
  - `Ethan <ethan@hursty.co.nz>`: 1 author commit.

## Specific Commits
- Alex Newman / thedotmack examples:
  - `28b40c05f2e1316948453b10e4feecce01817b6c`, `2026-04-29`, `docs: update CHANGELOG.md for v12.4.9`, reachable as `synqing/main`.
  - `598369e8942eb731651e391b01f60bb7329cc57c`, `2025-09-06`, `Initial release v3.3.8`, reachable only from `synqing/*`.
- UCHIDA Masayuki:
  - `544e9d39f581741a5f4b28f19a6684f3eb91881e`, `fix: replace hardcoded nvm/homebrew PATH with universal login shell resolution (#1833)`, changed `plugin/hooks/hooks.json`, reachable only from `synqing/*`.
- Ethan:
  - `16a0737dfcc294215cd655cd0cd2a6b4586eb2b1`, `fix: use parent project name for worktree observation writes (#1820)`, changed claude-mem `src/...` files, reachable only from `synqing/*`.
- Ben Younes:
  - `05232ff09150be17f1dcbc07bc32abd7751a78aa`, `fix: reap stuck generators in reapStaleSessions (fixes #1652) (#1698)`, reachable only from `synqing/*`.
  - Multiple related bugfix commits are claude-mem worker/session/smart-explore changes, not K1 firmware.

## Why It Appears Here
- Reflog shows `refs/remotes/synqing/*` fetched in this checkout on `2026-05-02 19:20:09 +0800` and `2026-05-03 12:46:53 +0800`.
- Shell history contains:
  - `git remote add synqing https://github.com/synqing/claude-mem.git`
  - `git fetch synqing codex/remediation-hardening-pr-clean-20260502`
  - `git cherry-pick b4e4eebceca123cf6c93132dd9eea8892e90e2d9`
- Claude session context from 2026-05-02 says the fork `synqing/claude-mem` was created for claude-mem remediation work and a PR attempt against `thedotmack/claude-mem` failed due permission limits.
- Therefore the cause is local remote/ref contamination from claude-mem remediation work performed while operating in or around this Lightwave checkout. It is not evidence that these authors contributed to Lightwave firmware history.

## Bad Prior Claim Found
- `docs/tooling/notebooklm-bundles/lightwave_ledstrip/_FORENSIC_WIFI_GENESIS.md` claims `598369e8` was a claude-mem subtree later merged into this repo.
- Current git evidence does not support that claim for `HEAD`/`origin`: `598369e8` is only under `refs/remotes/synqing/*`, and `HEAD` has no merge-base with `synqing/main`.
