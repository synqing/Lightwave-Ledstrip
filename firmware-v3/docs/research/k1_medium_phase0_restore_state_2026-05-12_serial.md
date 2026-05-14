# K1 Medium Phase 0/1B Restore Record
Date: 2026-05-12
Port: /dev/cu.usbmodem1101
Pre-capture state requested: 0x1302 / bri=149 spd=25 pal=3 sat=128 int=128 cpx=128 var=0
Exact restore requested: PASS

## End-of-session restoration
- Restoration target: 0x1302 K1 Waveform with effect state captured in pre-capture file.
- Restoration command method: runtime-only setters only; no NVS save command was emitted.
- End state verified via `s`: effect 0x1302 brightness=149 speed=25 (audio active).

## Safe state fallback plan
- If exact restore had failed, a known safe state (K1 Waveform, previous brightness/speed/controls) was prepared.
- Because exact_restore_ok was true in this run, safe fallback was not executed.
