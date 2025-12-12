# I2C Mutex Fix for Wire Library Contention

## Problem
The crash occurs because both the encoder task (Core 0) and the main loop via EncoderLEDFeedback (Core 1) are trying to access the I2C bus simultaneously without proper synchronization. The Wire library is not thread-safe by default.

## Error Details
```
[ 23653][E][Wire.cpp:422] beginTransmission(): could not acquire lock
Guru Meditation Error: Core  1 panic'ed (LoadProhibited). Exception was unhandled.
```

## Solution
Add a global I2C mutex that protects all Wire library calls across both cores.

## Implementation Steps

1. Create a global I2C mutex in hardware_config.h
2. Protect all encoder.writeRGB() calls in EncoderManager
3. Protect all encoder operations in EncoderLEDFeedback
4. Ensure mutex is taken before any Wire operations

## Files to Modify
- src/config/hardware_config.h - Add global I2C mutex
- src/hardware/EncoderManager.cpp - Protect all I2C operations
- src/hardware/EncoderLEDFeedback.h - Add mutex protection
- src/main.cpp - Initialize the mutex