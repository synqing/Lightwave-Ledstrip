# K1 Song-Aware Lane D A/B Precheck Serial Evidence - 2026-05-12

## Verdict

Precheck result: `BLOCKED_RUNTIME_SUPPORT`

The K1 device was reachable over USB serial, but no existing runtime surface exposed `songAware.mode=on`, a named song-aware parameter-only mode, or an equivalent autonomous parameter-adaptation mode that could be activated and observed without inventing a controller path. The A/B run was therefore stopped before playback.

## Boundary Confirmation

- Firmware/source edits: none.
- Production defaults: unchanged.
- NVS saves: none.
- Family morphing: not run.
- Constrained switching: not run.
- VP substrate, medium layer, timing, cadence, DSP expansion: untouched.
- Automatic effect-ID switching: not enabled.

## Device Reachability

USB serial device present:

```text
/dev/cu.usbmodem1101
```

Serial precheck opened the device and produced a fresh boot/pre-state log. Important observed lines:

```text
[LW][INFO][WiFiMgr] Started AP: LightwaveOS-AP
[LW][INFO][WiFiMgr] AP IP: 192.168.4.1
[LW][INFO][UnifiedAudio] Using ESV11AudioBackend
[LW][INFO][AudioActor] AudioActor using backend: ESV11AudioBackend
Initialized 212 effects
Initial effect from settings: 4866 (0x1302) - K1 Waveform
```

HTTP/REST was not reachable from the host network during precheck:

```text
curl -m 2 -sS http://192.168.4.1/api/v1/ping
curl: (28) Connection timed out after 2006 milliseconds
```

That means REST/WS contract surfaces could not be used for live control from the Mac during this run unless the host joined the K1 AP. Serial remained available.

## Serial Health Snapshot

Initial serial status after boot reported:

```text
Effect: 4866 (0x1302, K1 Waveform)
Brightness: 149
Speed: 25
FPS: 119 target 120
Frames: 184
Drops: 50
Frame time: avg=8380 us, min=8251 us, max=32947 us
LED show: avg=6205 us, max=7459 us, skips=0
Free heap: 8089399
Min free heap: 8089267
```

Initial `vp stack` snapshot reported:

```text
current effect=0x1302 K1 Waveform
controls brightness=149 speed=25 intensity=128 saturation=253 complexity=128 variation=0
edge_mixer mode=tetradic spatial=uniform temporal=rms_gate spread=30 strength=255
led_show show_skips=0 failures=0 rmt_errors=0 underruns=0
frame fps=119 avg_us=8391 min=8251 max=32947
```

Audio debug was exposed:

```text
adbg status: RMS 0.000 Flux 0.114 BPM 48.0 Conf 0.564
dbg status:  RMS 0.000 Flux 0.098 BPM 48.0 Conf 0.615
```

`edbg status` was attempted and produced no usable output in the captured precheck.

## Runtime Surface Scan

Command:

```text
rg -n "songAware|song-aware|familyMorphing|constrainedSwitching|parameter-only|parameter mode" firmware-v3/src docs/protocol tab5-encoder/src lightwave-ios-v2 k1-composer tools scripts -g '!firmware-v3/docs/research/**'
```

Result:

```text
tab5-encoder/src/ui/ZoneComposerUI.h.bak2:38: * Zone parameter modes (Effect, Palette, Speed, Brightness)
tab5-encoder/src/ui/ZoneComposerUI.h.bak2:195:     * Set active parameter mode (Effect, Palette, Speed, Brightness)
tab5-encoder/src/ui/ZoneComposerUI.h.bak2:196:     * @param mode New parameter mode
tab5-encoder/src/ui/ZoneComposerUI.h.bak2:212:     * Get active parameter mode
```

No source or protocol match exposed a song-aware runtime mode, family morphing flag, constrained switching flag, or parameter-only director equivalent.

Existing adjacent controls found:

- `firmware-v3/src/serial/SerialCLI.cpp:526` exposes `merge <param> <value> [source]`, which submits manual parameter values to the merge layer.
- `firmware-v3/src/serial/SerialCLI.cpp:2437` exposes serial `A`, which toggles narrative auto-play, not song-aware parameter-only adaptation.
- `docs/protocol/k1-rest-contract.yaml:291` exposes audio mapping endpoints, but the host could not reach `192.168.4.1` during this run.
- `docs/protocol/k1-ws-contract.yaml:1038` exposes EdgeMixer controls; `edge_mixer.save` persists to NVS and was not used.

Manual `merge` was not treated as Condition B because it would require an external controller to generate parameter changes. Serial `A` was not treated as Condition B because it toggles narrative auto-play and is not proven parameter-only or song-aware.

## Candidate Track Metadata

No track playback was run because Condition B failed the activation/observability gate before the A/B sequence. Local audio files were present, but the immediately verified corpus files were 25-second clips, not the requested 2-4 minute tracks:

```text
firmware-v3/test/music_corpus/harmonixset/esv11_benchmark/audio_32k/gte3BoXKwP0_32k.wav: 25.000000 sec, PCM 16-bit mono 32000 Hz
firmware-v3/test/music_corpus/harmonixset/esv11_benchmark/audio_32k/XtE1TmikgIQ_32k.wav: 25.000000 sec, PCM 16-bit mono 32000 Hz
firmware-v3/test/music_corpus/harmonixset/esv11_benchmark/audio_32k/9VZhI5iafH0_32k.wav: 25.000000 sec, PCM 16-bit mono 32000 Hz
```

Because runtime support blocked the run, no substitute short-track comparison was performed.

