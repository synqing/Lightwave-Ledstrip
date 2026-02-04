# CLAUDE.md - Developer Guide

This file provides guidance to Claude Code and developers working with this repository.

## Project Overview

iOS Simulator Skill is a production-ready Agent Skill providing 21 scripts for iOS app building, testing, and automation. It wraps Apple's `xcrun simctl` and Facebook's `idb` tools with semantic interfaces designed for AI agents and developers.

**Key Statistics:**
- 21 production scripts (~8,500 lines)
- 5 script categories (Build, Navigation, Testing, Permissions, Lifecycle)
- 6 shared utility modules (~1,400 lines)
- 100% token-optimized default output
- Full test coverage on all new features

## Project Structure

```
ios-simulator-skill/
├── skill/                          # Distributable package
│   ├── SKILL.md                   # Entry point (table of contents)
│   ├── CLAUDE.md                  # Developer guide
│   ├── README.md                  # User-facing overview
│   ├── scripts/                   # 21 production scripts
│   │   ├── build_and_test.py
│   │   ├── xcode/                 # Xcode integration module
│   │   ├── log_monitor.py
│   │   ├── screen_mapper.py
│   │   ├── navigator.py
│   │   ├── gesture.py
│   │   ├── keyboard.py
│   │   ├── app_launcher.py
│   │   ├── accessibility_audit.py
│   │   ├── visual_diff.py
│   │   ├── test_recorder.py
│   │   ├── app_state_capture.py
│   │   ├── clipboard.py
│   │   ├── status_bar.py
│   │   ├── push_notification.py
│   │   ├── privacy_manager.py
│   │   ├── simctl_boot.py
│   │   ├── simctl_shutdown.py
│   │   ├── simctl_create.py
│   │   ├── simctl_delete.py
│   │   ├── simctl_erase.py
│   │   ├── sim_health_check.sh
│   │   └── common/                # Shared utilities
│   ├── examples/
│   └── references/
├── .github/workflows/
├── pyproject.toml
└── README.md
```

## Architecture Patterns

### Pattern 1: Class-Based Script Design

All scripts use class-based architecture for testability:

```python
class DeviceManager:
    def __init__(self, udid: str | None = None):
        self.udid = udid

    def execute(self, **kwargs) -> tuple[bool, str]:
        # Return (success, message) tuple
        pass

def main():
    parser = argparse.ArgumentParser()
    manager = DeviceManager(args.udid)
    success, message = manager.execute()
    print(message)
    sys.exit(0 if success else 1)
```

### Pattern 2: Auto-UDID Detection

All scripts support optional `--udid` with auto-detection:

```python
try:
    udid = resolve_device_identifier(args.udid)  # May be None
except RuntimeError as e:
    print(f"Error: {e}", file=sys.stderr)
    sys.exit(1)
```

### Pattern 3: Output Formatting

Consistent format across all scripts:

```
# Default (3-5 lines)
Device booted: iPhone 16 Pro (ABC123) [2.1s]

# --verbose (50+ lines)
Device booted successfully.
Device UDID: ABC123DEF456...
Boot time: 2.1 seconds

# --json (20-30 lines)
{"action": "boot", "udid": "ABC123", "success": true}
```

### Pattern 4: Batch Operations

Most scripts support batch operations:

```bash
python scripts/simctl_boot.py --all
python scripts/simctl_boot.py --type iPhone
```

## Script Categories

### Build & Development (2)
- **build_and_test.py**: Build with progressive disclosure
- **log_monitor.py**: Real-time log monitoring

### Navigation & Interaction (5)
- **screen_mapper.py**: Analyze screen
- **navigator.py**: Semantic element finding
- **gesture.py**: Touch simulation
- **keyboard.py**: Text input and keys
- **app_launcher.py**: App lifecycle

### Testing & Analysis (5)
- **accessibility_audit.py**: WCAG compliance
- **visual_diff.py**: Screenshot comparison
- **test_recorder.py**: Test documentation
- **app_state_capture.py**: Debugging snapshots
- **sim_health_check.sh**: Environment verification

### Advanced Testing & Permissions (4)
- **clipboard.py**: Clipboard management
- **status_bar.py**: Status bar control
- **push_notification.py**: Push notifications
- **privacy_manager.py**: Permission management

### Device Lifecycle Management (5)
- **simctl_boot.py**: Boot device
- **simctl_shutdown.py**: Shutdown device
- **simctl_create.py**: Create device
- **simctl_delete.py**: Delete device
- **simctl_erase.py**: Reset device

## Shared Utilities

### device_utils.py (~450 lines)
- `resolve_device_identifier()`: UDID/name/booted resolution
- `list_simulators()`: List with state filtering
- `_extract_device_type()`: Parse device type from name

### screenshot_utils.py (~346 lines)
- `capture_screenshot()`: File or inline mode
- `generate_screenshot_name()`: Semantic naming
- `resize_screenshot()`: Token optimization

### cache_utils.py (~258 lines)
- `ProgressiveCache`: Large output caching with TTL

## Quality Standards

1. Type hints (modern Python syntax)
2. Docstrings on all functions
3. Specific exception handling
4. --help support on all scripts
5. Black formatter compliance
6. Ruff linter (0 errors)
7. Never use shell=True

## Token Efficiency

96% reduction in typical output:

- Default: 3-5 lines (5-10 tokens)
- Verbose: 50+ lines (400+ tokens)
- JSON: 20-30 lines (20-30 tokens)

This keeps AI agent conversations focused and cost-effective.

## Contributing

New scripts should:
- Use class-based design for > 50 lines of logic
- Support --udid and auto-detection
- Support --json output
- Provide --help documentation
- Follow Black and Ruff standards
- Update SKILL.md table of contents
- Work with real simulators before submission

## Release Process

1. Update version in SKILL.md frontmatter, pyproject.toml
2. Verify CI passes (Black, Ruff)
3. Create GitHub release with vX.X.X tag
4. Attach zipped skill/ directory

## Design Philosophy

**Semantic**: Find elements by meaning, not pixels.

**Progressive**: Minimal output by default, details on demand.

**Accessible**: Built on standard iOS accessibility APIs.

**Zero-Config**: Works immediately with no setup.

**Structured**: JSON and formatted text, not raw logs.

**Reusable**: Common patterns across all scripts.

This design works for both developers and AI agents.


<claude-mem-context>
# Recent Activity

<!-- This section is auto-generated by claude-mem. Edit content outside the tags. -->

### Feb 1, 2026

| ID | Time | T | Title | Read |
|----|------|---|-------|------|
| #24716 | 9:17 AM | 🔵 | iOS Simulator Skill Production-Ready Script Inventory | ~718 |
| #24695 | 9:10 AM | 🔵 | iOS Simulator Skill Specification Structure | ~595 |
</claude-mem-context>