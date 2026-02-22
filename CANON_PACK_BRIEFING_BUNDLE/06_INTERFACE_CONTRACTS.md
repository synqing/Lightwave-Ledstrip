# SpectraSynq / K1‑Lightwave — Interface Contracts

Date: 2026-02-09
Scope: Control plane contracts and audio→features→render contracts in quasi‑schema form.

Reference map for citations in this spec:
- Invariants: I1..I14 from `00_CONSTITUTION.md` in listed order.
- Decisions: D1..D13 from `01_DECISION_REGISTER.md` in listed order.

---

## 6. Interface Contracts (control plane + audio→features→render)

### 6.1 Control plane (REST + WebSocket)

`ControlCommand` (REST request body; owned by client; validated by `ControlPlaneActor`)
```yaml
type: ControlCommand
id: string
ts_ms: uint64
target: enum(effect|palette|brightness|transport|network|storage)
payload: object
```
Decisions: REST command interface and explicit validation. (I9, D1, D3)

`ControlStateSnapshot` (WebSocket broadcast; owned by `ControlPlaneActor`)
```yaml
type: ControlStateSnapshot
ts_ms: uint64
effect_id: string
palette_id: string
brightness: float32  # 0..1
transport: object    # play/pause, tempo lock if present
network: object      # ap/sta summary
storage: object      # last_saved / last_loaded
```
Decisions: WebSocket streaming of state; actor ownership. (I9, I12, D1, D13)

`RenderStateUpdate` (internal message; immutable)
```yaml
type: RenderStateUpdate
ts_ms: uint64
effect_id: string
palette_id: string
brightness: float32
```
Decisions: explicit state ownership; render is consumer only. (I12, D13)

### 6.2 Audio → Features → Render

`AudioFeatureFrame` (emitted by `AudioActor`; stored in `FeatureBus`)
```yaml
type: AudioFeatureFrame
seq: uint32
ts_ms: uint64
hop_ms: uint16
features:
  tempo_bpm: float32?
  chroma: float32[12]?
  spectral_flux: float32?
  energy: float32?
  beat_confidence: float32?
```
Decisions: fixed‑size frames; optional fields must be explicit to avoid guessing. (D8)

`FeatureBus` (ring buffer; overwrite‑oldest)
```yaml
type: FeatureBus
capacity: uint16
storage: PSRAM
policy: overwrite_oldest
```
Decisions: static buffers only; PSRAM mandatory. (I4, I5, I6, D12)

`RenderInput` (sampled each frame by `RenderActor`)
```yaml
type: RenderInput
frame_seq: uint32
ts_ms: uint64
audio_feature_ref: uint32
control_state_ref: uint32
```
Decisions: explicit ownership; deterministic frame sampling. (I12, I7, D9)

`FrameBuffer` (owned by `RenderActor`, published to `OutputDriver`)
```yaml
type: FrameBuffer
frame_seq: uint32
ts_ms: uint64
pixels: rgb8[320]  # centre‑origin mapped
```
Decisions: fixed topology; centre origin. (I14, I1, I2)

---

## Rules Compliance Notes
- No generic IoT boilerplate was introduced; all choices are tied to Constitution invariants or current decisions.
- British English spelling is used throughout.
