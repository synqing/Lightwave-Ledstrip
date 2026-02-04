# Motion Notes: connectionIndicator

- What animates: status dot glow and any connection-state indicator elements.
- Trigger: input booleans `isConnected`, `isConnecting`, `isDiscovering`, `hasError`.
- Expected behaviour:
- Connected: steady glow.
- Connecting/Discovering: pulsing or scanning motion.
- Error: distinct colour or flash to indicate failure.
- Resting state: steady dot with minimal motion when connected.
