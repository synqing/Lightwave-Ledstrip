# Comprehensive Git Staging and Commit Strategy

This directory contains documentation and scripts for the comprehensive commit strategy covering unified logging system, audio debug infrastructure, and K1 beat tracker debugging.

## Documentation

- **commit-1-unified-logging.md**: Unified logging system foundation
- **commit-2-3-audio-logging.md**: Audio component logging migration
- **commit-4-audio-debug-verbosity.md**: Runtime audio debug verbosity control
- **commit-5-6-k1-debug.md**: K1 beat tracker debug infrastructure and CLI
- **commit-7-10-logging-migration.md**: Remaining component logging migrations
- **STAGING_NOTES.md**: Special considerations for multi-commit files
- **staging-commands.sh**: Automated staging script (requires manual intervention for main.cpp)

## Quick Start

### Automated Staging (with manual intervention)

```bash
# Review the script first
cat docs/commits/staging-commands.sh

# Execute (will pause for manual staging of main.cpp at commits 4, 6, 10)
bash docs/commits/staging-commands.sh
```

### Manual Staging (recommended for cleanest commits)

1. **Commit 1**: Unified logging system
   ```bash
   git add v2/src/utils/Log.h v2/platformio.ini v2/src/config/features.h
   git commit -m "feat(logging): Add unified logging system with colored output
   ...
   ```

2. **Commit 2**: AudioActor logging migration
   ```bash
   git add v2/src/audio/AudioActor.cpp v2/src/audio/AudioActor.h
   git commit -m "refactor(logging): Migrate AudioActor to unified logging system
   ...
   ```

3. **Commit 3**: AudioCapture logging migration
   ```bash
   git add v2/src/audio/AudioCapture.cpp
   git commit -m "refactor(logging): Migrate AudioCapture to unified logging system
   ...
   ```

4. **Commit 4**: Audio debug verbosity (use `git add -p` for main.cpp)
   ```bash
   git add v2/src/audio/AudioDebugConfig.h v2/src/audio/AudioDebugConfig.cpp
   git add v2/src/audio/AudioActor.cpp v2/src/audio/AudioCapture.cpp
   git add -p v2/src/main.cpp  # Select only adbg-related changes
   git commit -m "feat(audio): Add runtime-configurable audio debug verbosity
   ...
   ```

5. **Commit 5**: K1 debug infrastructure
   ```bash
   git add v2/src/audio/k1/K1DebugMetrics.h v2/src/audio/k1/K1DebugRing.h
   git add v2/src/audio/k1/K1DebugMacros.h v2/src/config/features.h
   git add v2/src/audio/k1/K1BeatClock.h v2/src/audio/k1/K1Pipeline.h
   git add v2/src/audio/k1/K1Pipeline.cpp v2/src/audio/AudioActor.h
   git add v2/src/audio/AudioActor.cpp
   git commit -m "feat(k1): Add K1 beat tracker debug infrastructure
   ...
   ```

6. **Commit 6**: K1 debug CLI (use `git add -p` for main.cpp)
   ```bash
   git add v2/src/audio/k1/K1DebugCli.h v2/src/audio/k1/K1DebugCli.cpp
   git add -p v2/src/main.cpp  # Select only k1-related changes
   git commit -m "feat(k1): Add K1 beat tracker CLI debug commands
   ...
   ```

7. **Commit 7**: RendererActor logging migration
   ```bash
   git add v2/src/core/actors/RendererActor.cpp
   git commit -m "refactor(logging): Migrate RendererActor to unified logging system
   ...
   ```

8. **Commit 8**: Hardware components logging migration
   ```bash
   git add v2/src/hardware/EncoderManager.cpp v2/src/hardware/PerformanceMonitor.cpp
   git commit -m "refactor(logging): Migrate hardware components to unified logging system
   ...
   ```

9. **Commit 9**: Network components logging migration
   ```bash
   git add v2/src/network/WebServer.cpp v2/src/network/WiFiManager.cpp
   git commit -m "refactor(logging): Migrate network components to unified logging system
   ...
   ```

10. **Commit 10**: Main application logging migration
    ```bash
    git add v2/src/main.cpp  # Remaining logging migration changes
    git commit -m "refactor(logging): Migrate main.cpp to unified logging system
    ...
    ```

## Commit Summary

1. **feat(logging)**: Unified logging system foundation
2. **refactor(logging)**: AudioActor migration
3. **refactor(logging)**: AudioCapture migration
4. **feat(audio)**: Runtime audio debug verbosity
5. **feat(k1)**: K1 debug infrastructure
6. **feat(k1)**: K1 debug CLI
7. **refactor(logging)**: RendererActor migration
8. **refactor(logging)**: Hardware components migration
9. **refactor(logging)**: Network components migration
10. **refactor(logging)**: Main application migration

## Validation

All commits:
- ✅ Follow conventional commit format
- ✅ Are atomic (single logical change)
- ✅ Have proper dependencies
- ✅ Include comprehensive documentation
- ✅ Maintain build compatibility

## Notes

- See `STAGING_NOTES.md` for details on handling `main.cpp` multi-commit changes
- Full commit messages are in `staging-commands.sh`
- Each commit includes references to detailed documentation

