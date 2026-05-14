# K1 Medium Phase 0/1B Pre-capture State
Date: 2026-05-12
Captured: 2026-05-12T04:52:37
Port: /dev/cu.usbmodem1101
K1v2 confirmed: MAC b4:3a:45:a5:87:f8 and Sync UUID LW-B43A45A587F8

## Control Surface Capture
Effect: 0x1302 K1 Waveform
Controls: brightness=149 speed=25 intensity=128 saturation=128 complexity=128 variation=0
Palette: 3 / RGI 15
EdgeMixer: mode=tetradic spread=30 strength=255 spatial=uniform temporal=rms_gate

## Command sequence and timestamps
- 2026-05-12T04:52:37 `vp stack`
- 2026-05-12T04:52:37 `s`
- 2026-05-12T04:52:37 `dbg memory`
- 2026-05-12T04:52:37 `#`
- 2026-05-12T04:52:37 `adbg status`
- 2026-05-12T04:52:37 `dbg status`

## vp stack
```text
{"event":"telemetry.boot","ts_mono_ms":0,"version":"2.0"}
[1483][INFO][SystemInit] Phase 1: Serial initialised complete
[1483][INFO][Main] Loop scratch buffers: effectIds=PSRAM validation=PSRAM
[1483][INFO][Capture] Capture buffers: dumpFrame=PSRAM frameBuf=PSRAM taskFrame=PSRAM
[1483][INFO][SystemInit] Phase 2: PSRAM scratch + capture streamer initialised complete
{"event":"ota.boot.check","ts_mono_ms":1483,"rollbackPossible":false,"partition":"app0"}
[1483][INFO][OtaBoot] Boot partition 'app0' -- already validated (no rollback pending)
[1484][INFO][SystemInit] Phase 3: OTA boot verifier + WiFi deinit complete
[1484][INFO][SystemInit] Phase 4: WDT safe-mode check complete
[1484][INFO][SystemInit] ==========================================
[1484][INFO][SystemInit] LightwaveOS v2 - Actor System + Zones
[1484][INFO][SystemInit] ==========================================
[1484][INFO][SystemInit] Phase 5: System monitoring initialised complete
[1484][INFO][Main] Die temp sensor armed (range -10..80 C, L2 DAC offset)
[1484][INFO][SystemInit] Initializing Actor System...
[  1484][I][ActorSystem.cpp:81] init(): [ActorSystem] Initializing Actor System...
[  1492][I][ActorSystem.cpp:82] init(): [ActorSystem] Free heap: 8625439 bytes
[  1502][I][ActorSystem.cpp:139] init(): [ActorSystem] AudioActo[1502][INFO][Audio] ControlBusFrame snapshot storage: actor=0x3d80a214(PSRAM) payload=0x3fcf50c8(DRAM) frame=3728 payload=7456 object=7472
r created (Phase 2 audio sync enabled)
[  1511][I][ActorSystem.cpp:160] init(): [ActorSystem] Actors created successfully
[  1518][I][ActorSystem.cpp:161] init(): [ActorSystem] Free heap after init: 8550671 bytes
[1526][INFO][SystemInit] Actor System: INITIALIZED
[1527][INFO][SystemInit] Registering effects...
[WARNING] Effect count mismatch: registered 212, expected 203
[WARNING] This may indicate missing effect registrations or metadata drift
[1535][INFO][SystemInit] Effects registered: 212
[1535][INFO][SystemInit] Initialising Audio Mapping Registry...
[1558][INFO][SystemInit] Audio Mapping Registry: READY
[1558][INFO][SystemInit] Phase 6: Actor system + effects initialised complete
[1558][INFO][SystemInit] Initializing NVS...
[NVS] Initializing NVS flash...
[NVS] NVS flash initialized successfully
[NVS] Partition stats: 113 entries used, 517 entries free
[1559][INFO][SystemInit] NVS: INITIALIZED
[1559][INFO][OtaToken] OTA token loaded from NVS (32 chars)
{"event":"ota.token.init","ts_mono_ms":1560,"source":"nvs","tokenLen":32}
[1560][INFO][SystemInit] OTA Token Manager: INITIALIZED
[1560][INFO][SystemInit] Initializing Zone Composer...
[ZoneComposer] Initialised
[ZoneComposer] Layout set to 2 zones
[ZoneConfig] Zone configuration loaded from NVS
[1561][INFO][SystemInit] Zone Composer: INITIALIZED (restored from NVS)
[1561][INFO][SystemInit] Phase 7: NVS + zones initialised complete
[1561][INFO][SystemInit] Phase 8: Status strip + TTP223 button initialised complete
[1561][INFO][SystemInit] Starting Actor System...
[  1561][I][ActorSystem.cpp:193] start(): [ActorSystem] Starting actors...
[1568][INFO][Renderer] Initializing LEDs on Core 1
[1568][INFO][Renderer] Renderer task added to watchdog
[1569][INFO][LedDriver_S3] FastLED init: 2x160 LEDs on GPIO 6/7
[1577][INFO][Renderer] LED driver initialized: 2x160 LEDs on pins 6/7
[  1578][I][MessageBus.cpp:60] MessageBus(): [MessageBus] MessageBus initialized (max 32 types, 8 subs/type)
[1588][INFO][Renderer] Ready - 212 effects, brightness=96, target=120 FPS
[  1588][I][Actor.cpp:288] run(): [Actor] [Renderer] onStart() complete, entering main loop (tickInterval=0)
[  1600][I][ColorCorrectionEngine.cpp:71] initLUTs(): [ColorCorrection] LUTs initialized (gamma=2.2)
[  1609][E][Preferences.cpp:50] begin(): nvs_open failed: NOT_FOUND
[  1615][W][ColorCorrectionEngine.cpp:466] loadFromNVS(): [ColorCorrection] NVS not found, using defaults
[  1631][I][Actor.cpp:114] start(): [Actor] [Renderer] Started on core 1 (priority=5, stack=16384)
[  1640][I][[S h o1w6D4i0r]e[cIt]o[rAAccttoorr..ccpppp::121741]]  sotnaSrtta(r)t:( )[:A c[tSohro]w D[iSrheocwtDoirr]e cSthoorw]D iSrteactrorActor starting on Core 0
ted on core 0 (priority=2, stack=12288)
[  1658][I][Actor.cpp:114] start(): [Actor] [Audio] Started on core 0 (priority=4, stack=16384)
[1658][INFO][AudioESV11] ESV11 init: heap=185408, PSRAM=8225235/8386071
[1662][INFO][AudioESV11] ESV11 init: heap 185408 -> 185524, PSRAM 8225235 -> 8138675
[  1667][I][ActorSystem.cpp:254] start(): [ActorSystem] Audio int[1673][INFO][Audio] OnsetDetector: 1024-point FFT (esp-dsp), 31.25 Hz/bin, 512 bins
[1673][INFO][Audio] ES v1.1 audio backend: INITIALISED (125 Hz hop rate)
[  1673][I][Actor.cpp:288] run(): [Actor] [Audio] onStart() complete, entering main loop (tickInterval=0)
egration enabled - ControlBus (ES v1.1 backend)
[  1693][I][ActorSystem.cpp:274] start(): [ActorSystem] All actors started successfully
[  1658][I][ShowDirectorActor.cpp:291] onStart(): [ShowDirector] ShowDirectorActor ready
[  1712][I][ActorSystem.cpp:275] start(): [ActorSystem] Free heap: 8297051 bytes
[1721][INFO][SystemInit] Actor System: RUNNING
[1721][INFO][SystemInit] Boot memory: heap 166768 bytes free
[1721][INFO][SystemInit] Initializing Plugin Manager...
[  1727][I][Actor.cpp:288] run(): [Actor] [ShowDirector] onStart() complete, entering main loop (tickInterval=50)
[1730][INFO][SystemInit] Plugin Manager: Target registry set to RendererActor
[1730][INFO][PluginMgr] PluginManagerActor starting...
[1730][INFO][PluginMgr] Loading plugins from LittleFS...
[1749][INFO][PluginMgr] No plugin manifests found
[1757][INFO][SystemInit] Plugin Manager: INITIALIZED
[1757][INFO][SystemInit] Phase 9a: Actor system + plugin manager started complete
[1757][INFO][Main] Loading system state...
[ZoneConfig] System state loaded from NVS
[1776][INFO][Main] Restored: Effect=0x1302, Bri=149, Spd=25, Pal=3, Preset=0
[1777][INFO][Main] Expression: H=214 S=253 M=255 T=128 I=128 C=128 V=0
[1777][INFO][SystemInit] Initializing WiFiManager (AP-only boot)...
[1778][INFO][WiFi] Credentials set for 'CONFIGURE_ME'
[1778][INFO][WiFi] Soft-AP enabled: 'LightwaveOS-AP'
[1778][INFO][WiFi] Starting non-blocking WiFi management
[  1778][E][Preferences.cpp:483] ge[1779][INFO][Renderer] Effect changed: 0xFFFF (Unknown) -> 0x1302 (K1 Waveform)
tString(): nvs_get_str len fail: net_0 NOT_FOUND
[  1802][E][Preferences.cpp:483] getString(): nvs_get_str len fail: net_1 NOT_FOUND
[  1829][E][Preferences.cpp:483] getString(): nvs_get_str len fail: net_2 NOT_FOUND
[  1847][E][Preferences.cpp:483] getString(): nvs_get_str len fail: net_3 NOT_FOUND
[  1874][E][Preferences.cpp:483] getString(): nvs_get_str len fail: net_4 NOT_FOUND
[  1918][E][Preferences.cpp:483] getString(): nvs_get_str len fail: net_5 NOT_FOUND
[  1937][E][Preferences.cpp:483] getString(): nvs_get_str len fail: net_6 NOT_FOUND
[  1956][E][Preferences.cpp:483] getString(): nvs_get_str len fail: net_7 NOT_FOUND
[  1985][E][Preferences.cpp:483] getString(): nvs_get_str len fail: net_8 NOT_FOUND
[  1995][E][Preferences.cpp:483] getString(): nvs_get_str len fail: net_9 NOT_FOUND
[2005][INFO][WiFiCreds] WiFiCredentialsStorage initialized - 0 networks saved
[2005][WARN][WiFi] WIFI_AP_ONLY enabled - starting in AP mode only
[2151][INFO][WiFi] AP started on attempt 1/3: 'LightwaveOS-AP' at 192.168.4.1
[2151][WARN][WiFi] AP DIAG: ssid='LightwaveOS-AP' ssid_len=14 ch=1 auth=0 hidden=0 max_conn=4
[2151][WARN][WiFi] AP DIAG: wifi_mode=2 (1=STA 2=AP 3=APSTA)
[2178][WARN][WiFi] AP DIAG: tx_power=80 (max=84 ~21dBm)
[2179][INFO][WiFi] Task created on Core 0 (stack: 4096 bytes)
[2179][INFO][SystemInit] WiFiManager: STARTED (AP: LightwaveOS-AP, IP: 192.168.4.1)
[2179][INFO][SystemInit] Use serial 'wifi connect SSID PASS' to enable STA mode
[WiFiCred] No saved networks found, starting fresh
[2179][INFO][SystemInit] Phase 10: WiFi AP initialised complete
[2185][INFO][WiFi] Task started
[2189][INFO][SystemInit] Starting Web Server... (free heap: 120484)
[2189][INFO][SystemInit] Plugin Manager: Created (WebServer integration pending)
[2189][INFO][WebServer] Starting v2 WebServer...
[  2189][W][LittleFS.cpp:77] begin(): LittleFS Already Mounted!
[2199][INFO][WebServer] LittleFS mounted
[2228][INFO][UdpStream] UDP streamer started on port 0
[2228][INFO][WebServer] UDP streamer initialised
[2228][INFO][WebServer] WebSocket log streaming not yet implemented - requires logging callback system
[2229][INFO][WebServer] WiFi in AP mode via WiFiManager
[2229][INFO][V1Api] V1ApiRoutes::registerRoutes() called
```
```
## s
```text
[2640][INFO][WebServer] WebSocket handler registered at /ws (max clients: 8)
[2650][INFO][WsModifier] Modifier WebSocket commands registered (stub)
[2650][INFO][WsShow] Registered 11 show/prim8 WS commands
[2650][INFO][WebServer] WebSocket commands registered: 142/192 handlers
[2650][INFO][WebServer] Starting mDNS service...
[2650][INFO][WebServer]   Hostname: lightwaveos
[2650][INFO][WebServer]   IP Address: 192.168.4.1
[2659][INFO][WebServer]   WiFi Mode: AP
[2703][INFO][WebServer]   mDNS.begin() succeeded
[2749][INFO][WebServer]   Service registration: http=OK ws=OK
[2759][INFO][WebServer]   Sync UUID: LW-B43A45A587F8
[2759][INFO][WebServer] mDNS started successfully: http://lightwaveos.local
[2759][INFO][WebServer]   WebSocket: ws://lightwaveos.local:80/ws
[2759][INFO][WebServer] Starting AsyncWebServer on port 80 (AP IP: 192.168.4.1)...
[2769][INFO][WebServer] Zone state callback registered
[2769][INFO][WebServer] Server running on port 80
[2769][INFO][WebServer] AP mode - IP: 192.168.4.1
[2769][INFO][SystemInit] Web Server: RUNNING
[2769][INFO][SystemInit] REST API: http://lightwaveos.local/api/v1/
[2769][INFO][SystemInit] WebSocket: ws://lightwaveos.local/ws
[2769][INFO][SystemInit] Phase 11: WebServer initialised complete
{"event":"ota.boot.validated","ts_mono_ms":2778,"heap":27820,"wifiState":"connected_or_ap","webServer":true,"note":"already_validated"}
[2778][INFO][SystemInit] Phase 12: Post-boot validation complete
[2778][INFO][SystemInit] ==========================================
[2778][INFO][SystemInit] ACTOR SYSTEM: OPERATIONAL
[2778][INFO][SystemInit] ==========================================

Commands:
  SPACE   - Next effect (quick tap)
  0-9/a-k - Select effect by key
  n/N     - Next/Prev effect
  +/-     - Adjust brightness
  [/]     - Adjust speed
  ,/.     - Prev/Next palette (75 total)
  p/P     - Bloom prism opacity +/-
  o/O     - Bloom bulb opacity +/-
  i/I     - Mood +/- (transport speed)
  f/F     - Bloom alpha +/- (persistence)
  h/H     - Bloom square iter +/- (contrast)
  j/J     - Bloom prism iter +/- (ghost layers)
  k/K     - Bloom gHue speed +/- (palette sweep)
  u/U     - Bloom spatial spread +/- (palette gradient)
  v/V     - Bloom intensity coupling +/- (spatial↔heatmap)
  l       - List effects
  s       - Print status

Effect Registers:
  r       - Reactive effects only (audio-responsive)
  m       - Ambient effects only (time-based)
  *       - All effects (default)

Zone Commands:
  z       - Toggle zone mode
  Z       - Print zone status
  zs      - Set zone speed: zs <zoneId> <speed> OR zs <speed0> <speed1> <speed2>
  1-5     - Load zone preset (in zone mode)
  S       - Save all settings to NVS

Transition Commands:
  t/T     - RD Triangle F +/- (auto-selects RD Triangle)
  !       - List transition types

Auto-Play (Narrative) Commands:
  A       - Toggle auto-play mode
  @       - Print narrative status

EdgeMixer Commands:
  e       - Cycle mode (mirror/analog/comp/split_comp/sat_veil)
  w/W     - Spread +/- (hue width, 0-60)
  </>     - Strength -/+ (0-255)
  y       - Toggle spatial (uniform/centre_gradient)
  Y       - Toggle temporal (static/rms_gate)
  }       - Save EdgeMixer to NVS
  #       - Print EdgeMixer status

Color Correction Commands:
  c       - Cycle correction mode (OFF→HSV→RGB→BOTH→OFF)
  C       - Show color correction status
  E       - Toggle auto-exposure
  g       - Toggle/cycle gamma (off→2.2→2.5→2.8→off)
  b/B     - RD Triangle K +/- (auto-selects RD Triangle)
  cc      - Show correction mode (0=OFF,1=HSV,2=RGB,3=BOTH)
  cc N    - Set correction mode (0-3, accepts 'cc1' or 'cc 1')
  ae      - Show auto-exposure status
  ae 0/1  - Disable/enable auto-exposure (accepts 'ae0' or 'ae 0')
  gamma   - Show gamma status
  gamma N - Set gamma (0=off, 1.0-3.0, accepts 'gamma1.5' or 'gamma 1.5')
  brown   - Show brown guardrail status
  brown 0/1 - Disable/enable brown guardrail (accepts 'brown0' or 'brown 0')
  Csave   - Save color settings to NVS
  dither  - Show LED dithering status
  dither 0/1 - Disable/enable LED dithering

TempoTracker Debug:
  tempo   - Show BPM, confidence, phase, lock state

Audio Debug Verbosity:
  x              - Print 8-band + bass/mid/treble/rms/flux (one-shot; alias: bands)
  adbg           - Show current level
  adbg <0-5>     - Set level (0=off, 2=warnings, 5=trace)
  adbg status    - Print health summary (one-shot)
  adbg spectrum  - Print 8-band + 64-bin spectrum (one-shot)
  adbg beat      - Print BPM, phase, confidence (one-shot)
  adbg interval <N> - Set base interval in frames

Debug Commands (unified):
  d             - Toggle Bloom effect debug (spd/vel/mood/beat)
  dbg           - Show debug config
  dbg <0-5>     - Set global level (0=off,2=warn,3=info,4=debug,5=trace)
  dbg audio <0-5>   - Set audio domain level
  dbg render <0-5>  - Set render domain level
  dbg network <0-5> - Set network domain level
  dbg actor <0-5>   - Set actor domain level
  dbg status    - Print audio health NOW
  dbg spectrum  - Print 64-bin spectrum NOW
  dbg beat      - Print beat tracking NOW
  dbg memory    - Print heap/stack NOW
  vp stack      - Print VP stack topology, surfaces, and colour layer state
  dbg interval status <N>  - Auto status every N sec (0=off)
  dbg interval spectrum <N>- Auto spectrum every N sec (0=off)

WiFi Commands:
  wifi              - Show status (mode, SSID, IP, RSSI, saved networks)
  wifi ap           - Switch to AP-only mode (no STA)
  wifi connect SSID PASS - Connect to network + save to NVS
  wifi connect SSID - Connect using saved NVS creds
  wifi connect      - Reconnect to last/first saved
  wifi scan         - Trigger network scan

Web API:
  GET  /api/v1/effects - List effects
  POST /api/v1/effects/set - Set effect
  WS   /ws - Real-time control

[2868][INFO][SystemInit] Phase 13: Help banner printed complete
[2878][INFO][LW] Effect registers: 86 reactive, 104 ambient, 212 total

=== VP Stack Introspection ===
effect: 0x1302 K1 Waveform
palette: 3 RGI 15
controls: brightness=149 speed=25 intensity=128 saturation=253 complexity=128 variation=0 hue=214 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=m_leds output=physical_strips mismatch=false
layers:
  1 effect_render: active surface=m_leds
  2 colour_correction: active toggle=on skipped_by_effect=false apply_count=132 skip_count=0
  3 tone_map: active
  4 split/converge: m_leds -> physical_strips
  5 silence_policy: global_active=false bypassed=false hard_gate_effect=false silent_scale=1.000 audio=true
  6 edge_mixer: mode=tetradic spatial=uniform temporal=rms_gate spread=30 strength=255
  7 led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
colour:
  mode=both hsv_min_sat=120 rgb_white_threshold=150 rgb_target_min=100 saturation_boost=25
  auto_exposure=off target=110 brown_guardrail=off v_clamp=on max_brightness=255
  gamma=on value=2.200 lut_gen=1 samples=[0,3,12,56,137,255]
frame:
  target_fps=120 frames=132 drops=53 fps=118 avg_us=8409 min_us=8283 max_us=32947 cpu=100%
  timing: effect_render last_us=540 avg_us=467 colour_correction last_us=514 avg_us=473
  timing: show_leds last_us=6444 avg_us=6365 pre_pacing_work last_us=8470 avg_us=8148
  timing: output_prep last_us=161 avg_us=149 led_driver_show avg_us=6179
  led_show: frames=133 last_us=6248 avg_us=6179 max_us=7485 brightness=149
capture:
  enabled=false tap_mask=0x00 last_effect=0xFFFF last_palette=0 frame=0 timestamp_us=0
==============================

=== LightwaveOS v2 Actor System ===
State: 2
Uptime: 1403 ms
Active actors: 3
Total messages: 17
Heap: 8089027 / min 8088163 bytes
SPIRAM free: 8061707 bytes

--- Renderer ---
Effect: 4866 (K1 Waveform)
Brightness: 149
Speed: 25
FPS: 118 (target: 120)
CPU: 100%
Frames: 157, Drops: 59
Frame time: avg=8370, min=8248, max=32947 us
LED show: avg=6184, max=7485 us, skips=0
Stack watermark: 10432 words

--- ShowDirector ---
Has show: NO

--- MessageBus ---

=== MessageBus Subscriptions ===
Active entries: 2/32
Published: 17, Delivered: 1, Failed: 0

Type 0x83: 1 subscribers [ Renderer ]
Type 0xD3: 1 subscribers [ ShowDirector ]
================================

===================================

  Current effect type: IEffect (native)
  Metadata: K1 Waveform - K1 Lightwave waveform dot mode (parity port)
```
```
## dbg memory
```text

=== Memory Status ===
  Free heap: 27320 bytes
  Min free heap: 26496 bytes
  Max alloc heap: 17396 bytes

```
```
## EdgeMixer
```text

=== EdgeMixer Status ===
  Mode:     tetradic
  Spread:   30
  Strength: 255
  Spatial:  uniform
  Temporal: rms_gate

```
```
## adbg status
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.000  Flux: 0.009
  BPM: 48.0  Conf: 0.502  BeatTick: 0
  Onset: in=0.00406 floor=0.00200 act=0.352 gate[abs=0 act=0 prev=0 warm=1] flux=31.792 env=0.000 evt=0.000 k/s/h=0/0/0 us=1776
```
```
## dbg status
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.000  Flux: 0.018
  BPM: 48.0  Conf: 0.525  BeatTick: 0
  Onset: in=0.00664 floor=0.00200 act=1.000 gate[abs=0 act=0 prev=0 warm=1] flux=36.965 env=0.000 evt=0.000 k/s/h=0/0/0 us=1577
```
```
