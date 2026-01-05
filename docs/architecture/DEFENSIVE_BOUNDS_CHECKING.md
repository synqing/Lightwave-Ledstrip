# Defensive Bounds Checking

LightwaveOS accepts effect IDs, palette IDs, zone indices, and parameter values from multiple input surfaces (serial, REST, WebSocket, encoder UI). Any of these inputs can be invalid due to user error, client bugs, or stale state.

## Rule

Never index arrays or fixed-size buffers directly from external input. Always validate first.

## Pattern

- Use `validateXxx()` helpers (or equivalent) that:
  - Clamp or reject out-of-range values
  - Return a safe default (commonly `0`) when invalid
  - Avoid heap allocation and avoid blocking operations
- Validate as close to the input boundary as possible (API handler/command router), and re-validate at the execution point if the value may have gone stale.

## Why this matters

Out-of-bounds access is a common root cause of crashes (LoadProhibited), corrupted state, and difficult-to-debug intermittent failures.

