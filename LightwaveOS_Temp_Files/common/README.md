/**
 * K1.LightwaveOS Common Protocol Package
 * 
 * Shared library for Hub (Tab5) and Node (K1/S3) firmware.
 * 
 * ## Contents
 * 
 * ### Protocol Layer (`proto/`)
 * - `proto_constants.h` - Frozen protocol constants (MUST NOT change without version bump)
 * - `ws_messages.h` - WebSocket control plane messages (HELLO, WELCOME, KEEPALIVE, etc.)
 * - `udp_packets.h` - UDP stream plane binary packets (100Hz tick)
 * 
 * ### Clock Layer (`clock/`)
 * - `monotonic.h` - Platform-agnostic monotonic time access
 * - `timesync.h` - Node time synchronization algorithm
 * - `schedule_queue.h` - Bounded applyAt scheduler queue
 * 
 * ### Utilities (`util/`)
 * - `ringbuf.h` - Lock-free ring buffer for inter-task communication
 * - `logging.h` - Lightweight logging macros
 * 
 * ## Design Principles
 * 
 * 1. **Header-only where practical** - Minimal linking dependencies
 * 2. **No heap allocation in hot paths** - Bounded, predictable memory usage
 * 3. **Platform portability** - ESP-IDF, Arduino, host compilation
 * 4. **Frozen constants** - Single source of truth in `proto_constants.h`
 * 5. **Explicit endianness** - Network byte order for UDP packets
 * 
 * ## Usage
 * 
 * ### In PlatformIO projects:
 * 
 * ```ini
 * [env]
 * build_flags =
 *     -I../common
 * ```
 * 
 * ### In source files:
 * 
 * ```c
 * #include "proto/proto_constants.h"
 * #include "proto/ws_messages.h"
 * #include "proto/udp_packets.h"
 * #include "clock/monotonic.h"
 * #include "clock/timesync.h"
 * #include "clock/schedule_queue.h"
 * #include "util/ringbuf.h"
 * #include "util/logging.h"
 * ```
 * 
 * ## Implementation Notes
 * 
 * Some functions require platform-specific implementation (provided in your firmware projects):
 * 
 * - `lw_token_hash32()` - Token hashing (see `udp_packets.h`)
 * - `lw_msg_*_to_json()` / `lw_msg_*_from_json()` - JSON serialization (see `ws_messages.h`)
 * - `lw_timesync_*()` - Time sync implementation (see `timesync.c`)
 * - `lw_schedule_*()` - Scheduler implementation (see `schedule_queue.c`)
 * 
 * ## Verification
 * 
 * This protocol package is designed to match the Quint specifications:
 * - `specs/quint/control_plane.qnt`
 * - `specs/quint/udp_stream.qnt`
 * - `specs/quint/timesync.qnt`
 * - `specs/quint/session_auth.qnt`
 * - `specs/quint/time_scheduling.qnt`
 * 
 * Trace replay tools in `tools/trace_replay/` can validate firmware conformance.
 */

#pragma once

// Version
#define LW_COMMON_VERSION_MAJOR 1
#define LW_COMMON_VERSION_MINOR 0
#define LW_COMMON_VERSION_PATCH 0
