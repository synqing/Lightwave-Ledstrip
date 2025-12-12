# DEPRECATED Files

This directory contains files that are being phased out as part of the migration from the legacy monolithic audio pipeline to the new pluggable node architecture.

## Directory Structure

```
DEPRECATED/
├── backup/              # Old backup files (.bak)
├── examples/            # Example code (moved to avoid confusion)
├── legacy/              # Legacy implementations after migration
│   ├── src/            # Legacy source files
│   └── include/        # Legacy headers
└── README.md           # This file
```

## Migration Timeline

- **Phase 1** (Current): Files marked with deprecation notices
- **Phase 2**: Parallel operation of both systems
- **Phase 3**: Legacy files moved here
- **Phase 4**: Directory removed after successful migration

## Do Not Use

Files in this directory should NOT be used for new development. Refer to the pluggable node architecture in `include/audio/nodes/` for the modern implementation.

See `DEPRECATION_TRACKER.md` in the parent directory for detailed migration status.