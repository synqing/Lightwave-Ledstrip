# firmware-v3 FSM Reference

10 state machines governing system behavior. Sources: FSM diagram JSONs + source enum definitions.

---

### WiFi Manager
**File:** `network/WiFiManager.h:72`
**States:** INIT, SCANNING, CONNECTING, CONNECTED, FAILED, AP_MODE, DISCONNECTED

| From | To | Trigger |
|------|-----|---------|
| INIT | SCANNING | no cached channel |
| INIT | CONNECTING | cached credentials |
| INIT | AP_MODE | no credentials |
| SCANNING | CONNECTING | network found |
| SCANNING | FAILED | no networks, retry < 2 |
| SCANNING | AP_MODE | no networks after 2 scans |
| CONNECTING | CONNECTED | GOT_IP |
| CONNECTED | DISCONNECTED | link loss |
| CONNECTED | AP_MODE | AP-only flag |
| FAILED | SCANNING | retry |
| FAILED | AP_MODE | all failed |
| FAILED | INIT | backoff timeout |
| AP_MODE | SCANNING | periodic retry |
| DISCONNECTED | AP_MODE | AP-only |
| DISCONNECTED | INIT | reconnect delay |

---

### System Lifecycle
**File:** `core/actors/ActorSystem.h:57`
**States:** UNINITIALIZED, STARTING, RUNNING, STOPPING, STOPPED (accept)

| From | To | Trigger |
|------|-----|---------|
| UNINITIALIZED | STARTING | init |
| STARTING | RUNNING | all actors up |
| RUNNING | STOPPING | shutdown |
| STOPPING | STOPPED | all stopped |

---

### Audio Actor
**File:** `audio/AudioActor.h:79`
**States:** UNINITIALIZED, INITIALIZING, RUNNING, PAUSED, ERROR

| From | To | Trigger |
|------|-----|---------|
| UNINITIALIZED | INITIALIZING | onStart |
| INITIALIZING | RUNNING | I2S ok |
| INITIALIZING | ERROR | I2S fail |
| RUNNING | PAUSED | pause msg |
| PAUSED | RUNNING | resume msg |
| ERROR | UNINITIALIZED | shutdown |

---

### Noise Calibration
**File:** `audio/AudioTuning.h:54`
**States:** IDLE, REQUESTED, MEASURING, COMPLETE, FAILED

| From | To | Trigger |
|------|-----|---------|
| IDLE | REQUESTED | API call |
| REQUESTED | MEASURING | stabilized |
| MEASURING | COMPLETE | duration ok |
| MEASURING | FAILED | too noisy |
| COMPLETE | IDLE | applied |
| FAILED | IDLE | retry/reset |

---

### Sync Protocol
**File:** `sync/SyncProtocol.h:137`
**States:** INITIALIZING, DISCOVERING, ELECTING, LEADING, FOLLOWING, SYNCHRONIZED, RECONNECTING, ERROR

| From | To | Trigger |
|------|-----|---------|
| INITIALIZING | DISCOVERING | UUID loaded |
| INITIALIZING | ERROR | fatal |
| DISCOVERING | ELECTING | peers found |
| DISCOVERING | ERROR | fatal |
| ELECTING | LEADING | highest UUID |
| ELECTING | FOLLOWING | lower UUID |
| LEADING | SYNCHRONIZED | steady state |
| FOLLOWING | SYNCHRONIZED | steady state |
| SYNCHRONIZED | RECONNECTING | peer lost |
| SYNCHRONIZED | SYNCHRONIZED | LEADING/FOLLOWING sub-state (self) |
| SYNCHRONIZED | ERROR | fatal |
| RECONNECTING | DISCOVERING | timeout |
| RECONNECTING | SYNCHRONIZED | peer reachable |
| RECONNECTING | ERROR | fatal |

---

### Show Playback
**File:** `core/shows/ShowTypes.h:149` (ShowPlaybackState) + `core/actors/ShowDirectorActor.h`
**States:** IDLE, PLAYING, PAUSED, ENDED

| From | To | Trigger |
|------|-----|---------|
| IDLE | PLAYING | SHOW_START |
| PLAYING | PAUSED | SHOW_PAUSE |
| PAUSED | PLAYING | SHOW_RESUME |
| PLAYING | IDLE | SHOW_STOP |
| PLAYING | ENDED | duration elapsed |
| ENDED | IDLE | auto or manual |

---

### Show Cue Execution
**File:** `core/shows/CueScheduler.h` + `core/shows/ShowTypes.h:30`

Linear dispatcher, not a state machine. CueScheduler maintains a sorted PROGMEM cue list and a `nextIndex` pointer. On each tick, `getReadyCues(currentTimeMs)` returns up to 4 ready cues. Cue types dispatched:

| CueType | Action |
|---------|--------|
| CUE_EFFECT | Change effect on zone/global |
| CUE_PARAMETER_SWEEP | Interpolate parameter over time |
| CUE_ZONE_CONFIG | Configure zone settings |
| CUE_TRANSITION | Trigger TransitionEngine type |
| CUE_NARRATIVE | Modulate NarrativeEngine tempo/phase |
| CUE_PALETTE | Change color palette |
| CUE_MARKER | Sync point (no-op) |

---

### Effect Transitions
**File:** `effects/transitions/TransitionEngine.h` + `effects/transitions/TransitionTypes.h`

Two-state machine: **IDLE** and **ACTIVE**. `startTransition()` enters ACTIVE; `update()` returns false when complete, returning to IDLE. `cancel()` forces IDLE.

12 transition types (all center-origin radiating):

| Type | Default Duration |
|------|----------------:|
| FADE | 800ms |
| WIPE_OUT | 1200ms |
| WIPE_IN | 1200ms |
| DISSOLVE | 1500ms |
| PHASE_SHIFT | 1400ms |
| PULSEWAVE | 2000ms |
| IMPLOSION | 1500ms |
| IRIS | 1200ms |
| NUCLEAR | 2500ms |
| STARGATE | 3000ms |
| KALEIDOSCOPE | 1800ms |
| MANDALA | 2200ms |

---

### Network HAL
**File:** `hal/interface/INetworkDriver.h:36`

Type enum, not a full FSM. Platform-agnostic network state reported by `INetworkDriver::getState()`.

**States:** Disconnected, Connecting, Connected, Failed, APMode
**Network types:** None, WiFiStation, WiFiAP, Ethernet, EspHosted

Implementations: WiFiDriver_S3 (native WiFi), EthernetDriver_P4, EspHostedDriver_P4.

---

### Message Router
**File:** `core/bus/MessageBus.h` + `core/actors/Actor.h:50`

Singleton pub/sub dispatcher, not a state machine. Routes 16-byte `Message` structs between Actors via FreeRTOS queues.

**Message ranges:**

| Range | Category | Count |
|-------|----------|------:|
| 0x00-0x1F | Effect commands | 17 |
| 0x20-0x3F | Zone commands | 6 |
| 0x40-0x5F | Transition commands | 5 |
| 0x60-0x7F | System + Sync + Show commands | 13 |
| 0x80-0xBF | Events/Notifications | 22 |
| 0xC0-0xCF | Audio events | 4 |
| 0xD0-0xDF | Trinity sync commands | 4 |
| 0xE0-0xEF | Stimulus commands | 2 |

**Capacity:** 32 tracked types, 8 subscribers per type. Lock-free publish, mutex-protected subscribe.

---

### Narrative Phase (Audio Behavior)
**File:** `audio/AudioBehaviorSelector.h:65`

Phase enum driving audio-reactive behavior selection. Not a traditional FSM -- transitions are continuous based on audio energy thresholds.

**Phases:** REST, BUILD, HOLD, RELEASE

| Phase | Entry Condition |
|-------|----------------|
| REST | Low RMS (<0.15) and low flux (<0.1) sustained |
| BUILD | Rising flux (>0.3) near downbeat (phase >0.75 or <0.1) |
| HOLD | Peak energy sustained |
| RELEASE | Falling energy after peak |
