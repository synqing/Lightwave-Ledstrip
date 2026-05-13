# Chunk 2A Hardware Smoke Evidence

RBDO: DEGRADED-MODE for network and baseline-diff gates.

## Grounded Passes

- MAC verified during PlatformIO upload: `b4:3a:45:a5:87:f8`.
- Flash path: `pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/tty.usbmodem2101`.
- Upload result: success, PlatformIO only, direct `esptool.py` not used.
- Serial ESV11 audio liveness: `dbg status` returned `RMS=0.973`, `Flux=0.188`, `BPM=122.0`, `Conf=0.957`.
- Serial CLI canonical command accepted: `synqmatrix status`.
- Serial CLI legacy alias accepted: `sa status`.
- Serial JSON canonical status accepted: `synqMatrix.status`.
- Serial JSON legacy status accepted: `songAware.status`.
- Serial JSON status payloads expose `rawState`, `previousState`, `currentState`, and `candidateState`; no legacy `*SongState` fields were observed.
- Serial JSON invalid-mode parity passed using `mode:"bogus"`: both aliases returned `success:false` with `error:"mode must be off, assist, or director"`.

## Degraded Or Blocked Gates

- Serial `N1 > 0` and `N2 > N1` Captures inequality was not collectable on this HEAD. The live ESV11 `AudioActor::printStatus()` path prints RMS/flux/BPM/onset telemetry, not the `Captures:` line used by the older smoke brief.
- Temporary STA validation failed. `wifi connect VX220-013F 3232AA90E0F24` saved credentials and initiated AP+STA, but repeated `wifi status` showed `Connected: NO` and no DHCP IP. Logs included `WIFI_AP_ONLY: forcing AP mode`.
- REST and WS smoke did not run. After Captain correction, Mac AirPort interaction is forbidden, and AP association fallback is not allowed.
- Baseline diff is DEGRADED. No pre-migration WS baseline exists, and post-2A WS capture could not run without network reachability. No statistical equivalence claim is made.
- AP return is DEGRADED. `wifi ap` was accepted and STA was not connected, but immediate `wifi status` still printed `Mode: AP+STA`, `Connected: NO`, `AP IP: 192.168.4.1`.

## Evidence Files

- `firmware-v3/.smoke/post-2A-synqMatrix-status.jsonl`

## PR Disclosure Required

The PR must explicitly disclose PATCH #6: Chunk 2A preserves legacy command and route names, but response payload fields are canonical-only. Clients expecting `rawSongState`, `previousSongState`, `currentSongState`, or `candidateSongState` must update to `rawState`, `previousState`, `currentState`, and `candidateState`.
