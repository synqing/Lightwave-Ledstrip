# Phase 11: Workflow Supply-Chain Hardening - Implementation Status

## ✅ Completed

### 1. Dependabot Configuration
- **File**: `.github/dependabot.yml`
- **Status**: ✅ Created and configured
- **Features**:
  - Weekly checks (Mondays 09:00 UTC)
  - Groups related actions together
  - Labels and commit message prefixes configured

### 2. OpenSSF Scorecards Workflow
- **File**: `.github/workflows/scorecards.yml`
- **Status**: ✅ Created (SHAs need verification)
- **Features**:
  - Runs weekly + on push/PR
  - Minimal permissions: `contents: read`, `security-events: write`, `id-token: write`
  - Publishes results to GitHub Security

### 3. Least-Privilege Permissions
- **Status**: ✅ Already verified in Phase 10.2
- **Contextpack workflow**: `contents: read`, `pull-requests: read`, `actions: read`

### 4. SHA Pinning Helper Script
- **File**: `tools/command_centre/pin_action_shas.sh`
- **Status**: ✅ Created

## ⚠️ Remaining Work: SHA Pinning

All workflows need actions pinned to full commit SHAs. Currently using version tags which are mutable.

### Action Inventory

**Common Actions Needing SHA Pins:**

1. `actions/checkout@v3` / `@v4` - Used in 7 workflows
2. `actions/setup-python@v4` / `@v5` - Used in 5 workflows  
3. `actions/upload-artifact@v3` / `@v4` - Used in 2 workflows
4. `actions/cache@v4` - Used in 1 workflow
5. `actions/setup-node@v3` - Used in 1 workflow
6. `actions/download-artifact@v4` - Used in 1 workflow
7. `actions/github-script@v7` - Used in 1 workflow
8. `peter-evans/create-pull-request@v6` - Used in 1 workflow
9. `platformio/platformio-action@v1` - Used in 1 workflow
10. `rhymedcode/actionlint-action@v1` - Used in 1 workflow
11. `ossf/scorecard-action@v2` - Used in scorecards workflow

### How to Pin (Manual Steps)

For each action, replace `@v3` with `@<40-char-SHA> # v3`:

```bash
# Example: Get SHA for actions/checkout@v3
curl -s https://api.github.com/repos/actions/checkout/git/refs/tags/v3 | \
  grep -o '"sha":"[^"]*"' | head -1 | cut -d'"' -f4

# Then update workflow:
# - uses: actions/checkout@<SHA_HERE> # v3
```

### Workflows Needing Updates

- [ ] `.github/workflows/contextpack_test.yml` - 4 actions
- [ ] `.github/workflows/audio_benchmark.yml` - 7 actions
- [ ] `.github/workflows/codec_boundary_check.yml` - 2 actions
- [ ] `.github/workflows/itf_validation.yml` - 2 actions
- [ ] `.github/workflows/contextpack_lockdown.yml` - 3 actions
- [ ] `.github/workflows/pattern_validation.yml` - 2 actions
- [ ] `.github/workflows/scorecards.yml` - 3 actions (placeholders need real SHAs)

## Next Steps

1. **Run SHA fetching script** for each action:
   ```bash
   ./tools/command_centre/pin_action_shas.sh actions/checkout v3
   ```

2. **Update each workflow** replacing `@v3` with `@<SHA> # v3`

3. **Verify workflows still run** after pinning

4. **Dependabot will maintain** SHA pins going forward via PRs

## Verification

After pinning all actions:

```bash
# Check no unpinned actions remain
grep -r "uses:.*@v[0-9]" .github/workflows/ || echo "All actions pinned!"

# Verify YAML syntax
yamllint .github/workflows/*.yml
```
