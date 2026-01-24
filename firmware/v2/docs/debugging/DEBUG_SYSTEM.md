# LightwaveOS Debug System

**Version:** 1.0.0
**Last Updated:** 2026-01-22
**Status:** Active

## Overview

LightwaveOS v2 provides a unified debug system with:
- **5 debug levels** with clear semantics
- **Per-domain control** for audio, render, network, actor subsystems
- **On-demand diagnostics** via serial commands
- **Opt-in periodic output** (off by default)
- **REST API** for remote configuration

This system replaces the previous fragmented approach (6-tier audio verbosity, scattered Serial.print statements) with a consistent, discoverable interface.

---

## Quick Reference

### Debug Levels

| Level | Name | When to Use |
|-------|------|-------------|
| 0 | OFF | Production, minimal logging |
| 1 | ERROR | Show only actual failures |
| 2 | WARN | (DEFAULT) Errors + actionable warnings |
| 3 | INFO | + Significant events |
| 4 | DEBUG | + Diagnostic values |
| 5 | TRACE | + Everything (raw data, per-frame) |

### Serial Commands

```
dbg                  - Show current debug config
dbg <0-5>            - Set global level
dbg audio <0-5>      - Set audio domain level
dbg render <0-5>     - Set render domain level
dbg network <0-5>    - Set network domain level
dbg actor <0-5>      - Set actor domain level

dbg status           - Print audio health NOW
dbg spectrum         - Print spectrum NOW
dbg beat             - Print beat tracking NOW
dbg memory           - Print heap/stack NOW

dbg interval status <N>    - Auto-print every N seconds (0=off)
dbg interval spectrum <N>  - Auto-print spectrum every N seconds
```

### REST API

```
GET  /api/v1/debug/config   - Get debug configuration
POST /api/v1/debug/config   - Update configuration
POST /api/v1/debug/status   - Get status as JSON
```

---

## Default Behaviour

With default settings (level 2 = WARN):
- **No periodic output** - Serial is quiet unless something is wrong
- **Errors always print** - Failures are always visible
- **Warnings print** - Actionable issues like low stack, high latency

This is a significant change from the previous system where enabling audio debugging automatically produced periodic status output.

---

## Domain-Specific Control

Each subsystem can have its own debug level, allowing targeted debugging without noise from unrelated components.

### Audio Domain (`dbg audio`)
Controls output from:
- AudioNode (I2S/DMA, AGC, FFT)
- Beat tracking and tempo detection
- Musical saliency analysis
- Style detection

### Render Domain (`dbg render`)
Controls output from:
- RendererNode (effect execution, modifiers)
- Zone composer
- Transition engine
- FastLED driver timing

### Network Domain (`dbg network`)
Controls output from:
- WiFiManager (connection state, events)
- WebServer (request handling, rate limiting)
- WebSocket (message processing)
- mDNS

### Actor Domain (`dbg actor`)
Controls output from:
- ActorSystem (message routing, task scheduling)
- Inter-node communication
- Snapshot buffer operations

---

## On-Demand Diagnostics

These commands print diagnostic information **immediately** without enabling continuous output.

### `dbg status` - Audio Health Check

Prints a one-shot summary of audio system health:

```
=== Audio Health Check ===
DMA: OK (overflow=0, underflow=0)
AGC: enabled, gain=1.23
Beat: 120.5 BPM, confidence=0.87
Saliency: harmonic=0.45, rhythmic=0.82
Style: RHYTHMIC_DRIVEN
========================
```

### `dbg spectrum` - Current Spectrum

Prints the current 64-bin FFT spectrum:

```
=== Spectrum (64 bins, 55Hz-2093Hz) ===
Bins 0-7   (sub-bass):  [####    |##      |###     |##      |#       |##      |###     |####    ]
Bins 8-15  (bass):      [######  |#####   |####    |###     |##      |###     |####    |#####   ]
...
==========================================
```

### `dbg beat` - Beat Tracking State

Prints beat tracking and tempo information:

```
=== Beat Tracking ===
Tempo: 128.0 BPM (confidence: 0.92)
Phase: 0.73 (next beat in 84ms)
Last onset: 234ms ago
History: [0.47, 0.52, 0.48, 0.51, 0.47]
=====================
```

### `dbg memory` - Heap/Stack Stats

Prints memory usage information:

```
=== Memory Stats ===
Heap: 124,567 / 327,680 free (38%)
Min free: 98,234 (since boot)
Fragmentation: 12%
Stack (AudioNode): 2,048 / 4,096 used
Stack (RendererNode): 1,536 / 4,096 used
====================
```

---

## Periodic Output (Opt-In)

By default, no periodic output is generated. Use `dbg interval` to enable automatic printing.

### Interval Commands

```bash
dbg interval status 10    # Print status every 10 seconds
dbg interval spectrum 2   # Print spectrum every 2 seconds
dbg interval status 0     # Disable status auto-print
```

### Available Intervals

| Interval | Description | Recommended Value |
|----------|-------------|-------------------|
| status | Audio health summary | 10-30s |
| spectrum | 64-bin FFT display | 1-5s |
| beat | Beat tracking state | 5-10s |
| memory | Heap/stack stats | 30-60s |

---

## REST API Reference

### GET /api/v1/debug/config

Returns current debug configuration:

```json
{
  "success": true,
  "data": {
    "globalLevel": 2,
    "domains": {
      "audio": 2,
      "render": 2,
      "network": 2,
      "actor": 2
    },
    "intervals": {
      "status": 0,
      "spectrum": 0,
      "beat": 0,
      "memory": 0
    }
  },
  "timestamp": 123456789,
  "version": "1.0"
}
```

### POST /api/v1/debug/config

Update debug configuration:

```bash
# Set audio domain to DEBUG level
curl -X POST http://lightwaveos.local/api/v1/debug/config \
  -H "Content-Type: application/json" \
  -d '{"audio": 4}'

# Set multiple domains
curl -X POST http://lightwaveos.local/api/v1/debug/config \
  -H "Content-Type: application/json" \
  -d '{"audio": 4, "render": 3, "intervals": {"status": 10}}'

# Set global level (affects all domains)
curl -X POST http://lightwaveos.local/api/v1/debug/config \
  -H "Content-Type: application/json" \
  -d '{"globalLevel": 3}'
```

### POST /api/v1/debug/status

Get one-shot status as JSON (same data as `dbg status` but structured):

```bash
curl -X POST http://lightwaveos.local/api/v1/debug/status
```

Response:
```json
{
  "success": true,
  "data": {
    "audio": {
      "dma": {"overflow": 0, "underflow": 0},
      "agc": {"enabled": true, "gain": 1.23},
      "beat": {"bpm": 120.5, "confidence": 0.87, "phase": 0.73},
      "saliency": {"harmonic": 0.45, "rhythmic": 0.82}
    },
    "memory": {
      "heapFree": 124567,
      "heapTotal": 327680,
      "minFree": 98234,
      "fragmentation": 12
    }
  },
  "timestamp": 123456789,
  "version": "1.0"
}
```

---

## Examples

### Silence All Debug Output

```bash
dbg 0
```

### Enable Verbose Audio Debugging

```bash
dbg audio 5                  # Maximum audio verbosity
dbg interval status 10       # Auto status every 10s
dbg interval spectrum 2      # Auto spectrum every 2s
```

### One-Shot Diagnostics (No Continuous Output)

```bash
dbg status    # Print health check once
dbg spectrum  # Print current spectrum once
dbg memory    # Print heap stats once
```

### Debug Network Issues

```bash
dbg network 4                # Enable network DEBUG level
dbg audio 1                  # Reduce audio noise to errors only
```

### Remote Configuration via REST

```bash
# Get current config
curl http://lightwaveos.local/api/v1/debug/config

# Enable debug level for audio
curl -X POST http://lightwaveos.local/api/v1/debug/config \
  -H "Content-Type: application/json" \
  -d '{"audio": 4}'

# Get one-shot status
curl -X POST http://lightwaveos.local/api/v1/debug/status
```

### Production Mode (Minimal Logging)

```bash
dbg 1                        # Global ERROR level only
dbg interval status 0        # Disable all periodic output
dbg interval spectrum 0
```

---

## Backwards Compatibility

The legacy `adbg` command still works for audio debugging:

| Command | Equivalent | Notes |
|---------|------------|-------|
| `adbg` | `dbg audio` | Shows audio domain level |
| `adbg 0` | `dbg audio 0` | Disable audio debug |
| `adbg 3` | `dbg audio 3` | Set audio to INFO |
| `adbg status` | `dbg status` | One-shot status |

---

## Migration from Old System

The previous audio debug system used a 6-tier verbosity with automatic periodic output. Here's how to achieve equivalent behaviour:

| Old Command | Old Behaviour | New Equivalent |
|-------------|---------------|----------------|
| `adbg 0` | No output | `dbg audio 0` |
| `adbg 1` | Errors only | `dbg audio 1` |
| `adbg 2` | 10s health status | `dbg audio 2` + `dbg interval status 10` |
| `adbg 3` | 5s DMA stats | `dbg audio 3` + `dbg interval status 5` |
| `adbg 4` | 2s spectrum | `dbg audio 4` + `dbg interval spectrum 2` |
| `adbg 5` | 1s detailed | `dbg audio 5` + `dbg interval status 1` |

**Key difference:** The new system separates verbosity level from periodic output. You can have high verbosity (level 5) without any periodic output, or low verbosity (level 2) with frequent status updates.

---

## Architecture

### Source Files

| File | Purpose |
|------|---------|
| `src/config/DebugConfig.h` | Configuration struct, default values |
| `src/utils/Log.h` | Domain-aware logging macros |
| `src/main.cpp` | Serial command handlers |
| `src/network/webserver/handlers/DebugHandlers.cpp` | REST API handlers |

### Configuration Struct

```cpp
struct DebugConfig {
    uint8_t globalLevel = 2;     // WARN
    uint8_t audioLevel = 2;
    uint8_t renderLevel = 2;
    uint8_t networkLevel = 2;
    uint8_t actorLevel = 2;

    uint16_t statusIntervalSec = 0;    // 0 = disabled
    uint16_t spectrumIntervalSec = 0;
    uint16_t beatIntervalSec = 0;
    uint16_t memoryIntervalSec = 0;
};
```

### Log Macros

```cpp
// Domain-aware macros
DBG_AUDIO(level, fmt, ...)    // Audio domain
DBG_RENDER(level, fmt, ...)   // Render domain
DBG_NETWORK(level, fmt, ...)  // Network domain
DBG_ACTOR(level, fmt, ...)    // Actor domain

// Level shortcuts
DBG_AUDIO_ERROR(fmt, ...)     // Level 1
DBG_AUDIO_WARN(fmt, ...)      // Level 2
DBG_AUDIO_INFO(fmt, ...)      // Level 3
DBG_AUDIO_DEBUG(fmt, ...)     // Level 4
DBG_AUDIO_TRACE(fmt, ...)     // Level 5
```

---

## Troubleshooting

### No Output Even at Level 5

1. Check global level: `dbg` (should show levels)
2. Verify domain level: `dbg audio`
3. Check serial connection: 115200 baud
4. Verify FEATURE_SERIAL_MENU is enabled

### Too Much Output

1. Reduce global level: `dbg 2` (WARN only)
2. Disable periodic output: `dbg interval status 0`
3. Target specific domain: `dbg audio 4` but `dbg render 1`

### REST API Not Responding

1. Check WiFi connection: `net status`
2. Verify WebServer is running: `http://lightwaveos.local/api/v1/`
3. Check FEATURE_WEB_SERVER is enabled

---

## See Also

- [ARCHITECTURE_REVIEW.md](ARCHITECTURE_REVIEW.md) - Debug system architecture analysis
- [SERIAL_DEBUG_REDESIGN_PROPOSAL.md](SERIAL_DEBUG_REDESIGN_PROPOSAL.md) - Design rationale
- [API_V1.md](../api/API_V1.md) - Full REST API documentation
