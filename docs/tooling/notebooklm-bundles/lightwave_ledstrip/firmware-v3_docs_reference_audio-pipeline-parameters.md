---
abstract: "Complete inventory of all audio pipeline parameters: ControlBusFrame fields (90+ fields, 13 groups), ESV11 backend DSP constants, Goertzel/tempo/beat tuning, adapter mapping chain, and runtime-tunable TempoParams. Authoritative reference for effect development and audio debugging."
---

# Audio Pipeline Parameter Reference

Source: ControlBus.h, EsV11Backend, EsV11Adapter, PipelineAdapter, audio_config.h, global_defines.h
Last verified: 2026-03-23 from source

## Signal Chain

Microphone > I2S DMA > AudioActor (Core 0) > ESV11 Backend > EsV11Adapter > ControlBusFrame > Effects (Core 1)

Production backend: ESV11 (64-bin Goertzel). PipelineCore is BROKEN — do not use.

## Section 1: Input Stage

SAMPLE_RATE = 32000 (I2S sample rate, Hz)
CHUNK_SIZE = 128 (samples per DMA hop = 4ms)
DC_BLOCKER_R = 0.999019 (pole coefficient, 5 Hz cutoff at 32kHz)
DC_BLOCKER_G = 0.999509 (gain)
BIT_SHIFT = 10 (IDF4 legacy driver) or 14 (IDF5 i2s_std)
recip_scale = 1/131072.0 (18-bit signed range normalisation)
DMA_BUFFER_COUNT = 4
DMA_BUFFER_SAMPLES = 512

## Section 2: Goertzel Spectral Analysis

NUM_FREQS = 64 (frequency bins, ~55 Hz to ~4.8 kHz, musical spacing)
SAMPLE_HISTORY_LENGTH = 10240 (float ring buffer, 320ms window at 32kHz)
BOTTOM_NOTE = 12 (starting index in 186-element notes[] table, corresponds to ~55 Hz)
NOTE_STEP = 2 (half-step spacing between bins)
Window function: Gaussian, sigma = 0.8, length 4096
Bandwidth per bin: neighbor_distance_hz * 4.0
Interlacing: odd/even bin groups on alternating chunks (halves per-chunk cost)
NUM_AVERAGE_SAMPLES = 2 (2-hop running average of raw magnitudes)
NUM_SPECTROGRAM_AVERAGE_SAMPLES = 12 (12-hop rolling average > spectrogram_smooth[], the primary output)
Per-bin frequency scaling: scale = (bin/64)^4 * 0.9975 + 0.0025 (high-frequency boost)
Noise calibration: NOISE_CALIBRATION_WAIT_FRAMES=256, NOISE_CALIBRATION_ACTIVE_FRAMES=512
Noise history: 10-slot rolling average, updated every 1000ms, multiplied by 0.90 before use
kNoiseAlpha = 1 - pow(0.99, 200/currentRate) (~0.008 at 250 Hz chunk rate)
kAutorangerAlpha = 1 - pow(0.995, 200/currentRate) (~0.005 at 250 Hz)
Autorange minimum max_val: 0.0025
Chromagram: first 60 bins folded by i%12 (5 octaves, 12 pitch classes), each /5.0, autoranged with min max 0.2

## Section 3: VU Meter

NUM_VU_LOG_SAMPLES = 20 (noise floor estimation depth)
NUM_VU_SMOOTH_SAMPLES = 12 (output smoothing depth)
Noise floor = long-term average * 0.90
Log update interval: 250ms
Minimum max_amplitude_cap: 0.000025
kCapAlpha = 1 - pow(0.9, 200/currentRate) (rise/fall tracking)
Boot fast-fill: first 2000ms fills entire log for fast convergence

## Section 4: Tempo / Beat Detection

NUM_TEMPI = 96 (resonator bins, 1 BPM per bin)
TEMPO_LOW = 48 BPM
TEMPO_HIGH = 144 BPM
NOVELTY_LOG_HZ = 50 (novelty sampling rate)
NOVELTY_HISTORY_LENGTH = 1024
BEAT_SHIFT_PERCENT = 0.16 (phase offset, radians fraction)
REFERENCE_FPS = 100 (internal delta scaling reference)
Novelty curve: log1p(mean spectral flux across all 64 bins)
Silence window: kSilenceFrames = 128 (2.56s at 50Hz)
Silence threshold: silence_level_raw > 0.5
Silence decay: reduce_tempo_history(silence_level * 0.10) per novelty update
kTempoAlpha = 1 - pow(0.975, 200/currentRate) (~0.025 at 250 Hz)
Tempo magnitude minimum max_val: 0.02
Tempo magnitude sharpening: mag^3 (cubic)

### Octave-Aware Tempo Selection

octave_double_persist = 50 calls (~200ms) before half-time promotion commits
Double gate: raw_bpm < 80 AND tempo_confidence > 0.06
Ratio threshold at BPM <= 72: 0.56 (double candidate must be >= 56% of raw winner energy)
Ratio threshold at BPM > 72: 0.72
Edge rebound: raw_bpm >= 138 AND confidence < 0.35 > looks in 76-84 BPM window, rebound_mag >= raw_mag * 0.70
210-BPM alias rescue: 128 <= raw_bpm <= 136 AND confidence < 0.32 > looks in 102-108 BPM, rescue_mag >= 0.09 AND >= raw_mag * 0.10 AND > selected_score * 0.80
Half-time rescue: raw_bpm >= 132, half_mag >= selected_score * 0.92

### Bin Stabilisation (TempoParams — RUNTIME TUNABLE via serial)

These are the ONLY parameters modifiable at runtime without reflash.

gateBase = 1.2 (minimum magnitude ratio for new bin to beat stable bin)
gateScale = 0.8 (additional gate as stability grows; max gate = gateBase + gateScale = 2.0)
gateTau = 4.0s (gate growth time constant: holdSec / (holdSec + gateTau))
confFloor = 0.10 (below this raw confidence, bin switches are frozen)
validationThr = 0.08 (raw confidence must exceed this at least once to validate stable bin)
stabilityTau = 4.0s (stability confidence time constant)
holdUs = 200000 (200ms minimum between bin switches)
octaveRuns = 4 (consecutive hops an octave candidate must persist)
decayFloor = 0.005 (below this magnitude, stable bin is decayed — escape hatch allows switch)
octRatioLo = 1.9 (lower bound for candidate/stable BPM ratio for octave family)
octRatioHi = 2.1 (upper bound for octave family ratio)
wsSepFloor = 0.001 (denominator floor for winner-separation confidence)
confDecay = 0.994 (stored but not consumed in current code path — residual)
genericPersistUs = 1000000 (1s — non-octave candidate must be consistent winner for this duration)
Composite confidence = max(rawConf, stabilityConf, winnerSeparation)

## Section 5: EsBeatClock (render-rate phase interpolation)

Default BPM: 120.0 (before first audio frame)
Beat strength decay tau: 0.30s (exponential decay between ticks)
Phase advancement: bpm / 60.0 * dt_s per render frame (~120 FPS)
Beat-in-bar: modulo 4 (4/4 metre assumption)

## Section 6: EsV11Adapter — Backend to ControlBus Mapping

### RMS/Flux
vu_level > clamp01(sqrt(max(0, vu_level)) * 1.25) > frame.rms, frame.fast_rms
novelty_norm_last > clamp01 > frame.flux, frame.fast_flux

### Spectrum (bins64)
spectrogram_smooth[0..63] > clamp01 > autorange follower (decay alpha from 0.995, rise alpha from 0.25, floor 0.05, all @ 50Hz ref via retunedAlpha) > frame.bins64[64], frame.bins64Adaptive[64]
Activity gate: vu_level >= 0.01 enables autorange; below threshold, multiply by 1 instead
Raw values also stored: frame.es_bins64_raw[64]

### Octave Bands
frame.bins64 grouped in 8-bin blocks > mean of each block > frame.bands[0..7]
bands[0] = mean(bins64[0..7]) = sub-bass (~60 Hz)
bands[1] = mean(bins64[8..15]) = bass (~120 Hz)
bands[2] = mean(bins64[16..23]) = low-mid (~240 Hz)
bands[3] = mean(bins64[24..31]) = mid (~480 Hz)
bands[4] = mean(bins64[32..39]) = upper-mid (~960 Hz)
bands[5] = mean(bins64[40..47]) = presence (~1.9 kHz)
bands[6] = mean(bins64[48..55]) = brilliance (~3.8 kHz)
bands[7] = mean(bins64[56..63]) = air (~7.8 kHz)
Treble shorthand: (bands[5] + bands[6] + bands[7]) * (1.0f / 3.0f)

### Heavy (slow envelope)
bands[0..7] > EMA with alpha retunedAlpha(0.05, 50Hz, HOP_RATE_HZ) > frame.heavy_bands[0..7]
chroma[0..11] > same heavy_alpha > frame.heavy_chroma[0..11]

### Chroma
chromagram[0..11] > clamp01 > autorange follower (decay 0.995, rise 0.35, floor 0.08, gated by activity) > frame.chroma[0..11]
Raw values: frame.es_chroma_raw[0..11]

### Waveform
waveform[128] > memcpy > frame.waveform[128] (int16, -32768..32767)

### Percussion Onset
snareEnergy = mean(bins64[5..10]) (~150-300 Hz), clamped
hihatEnergy = mean(bins64[50..60]) (~6-12 kHz), clamped
snareTrigger = (delta > 0.08 AND snareEnergy > 0.10)
hihatTrigger = (delta > 0.08 AND hihatEnergy > 0.05)

### Tempo/Beat
es.top_bpm > direct > frame.tempoBpm, frame.es_bpm
es.tempo_confidence > clamp01 > frame.tempoConfidence, frame.es_tempo_confidence
es.beat_tick > direct bool > frame.tempoBeatTick, frame.es_beat_tick
es.beat_strength > clamp01 > frame.tempoBeatStrength, frame.es_beat_strength
es.phase_radians > (phase + pi) / (2*pi), wrapped, clamped [0,1) > frame.es_phase01_at_audio_t
beat counter modulo 4 > frame.es_beat_in_bar
beat_tick AND beat_in_bar==0 > frame.es_downbeat_tick

### Sensory Bridge Sidecar (ESv11 only)
4-frame waveform ring buffer history
Peak follower: attack 0.25, release 0.005 @ 50Hz ref, floor 750 int16 units
sb_waveform_peak_scaled = (peak - 750) / follower, with separate attack/release smoothing
sb_note_chromagram: bins64Adaptive octave-folded (6 octaves * 12 notes), capped at 1.0 per bin

## Section 7: Derived Features (ControlBus::applyDerivedFeatures, backend-agnostic)

### Chord Detection
Input: chroma[0..11]
Algorithm: find dominant pitch class, check +3/+4 semitones (third) and +6/+7/+8 (fifth)
Output: chordState.rootNote (0-11), chordState.type (NONE=0/MAJOR=1/MINOR=2/DIMINISHED=3/AUGMENTED=4), chordState.confidence (triad energy ratio 0..1), chordState.rootStrength, chordState.thirdStrength, chordState.fifthStrength

### Liveliness
rawLiveliness = tempoConfidence * 0.6 + fast_flux * 0.4
EMA with tau = 0.30s, alpha = 1 - exp(-dt/tau)
Output: frame.liveliness (0..1)

### Musical Saliency (FEATURE_MUSICAL_SALIENCY)
saliency.harmonicNovelty: raw harmonic change (chord/key), 0..1
saliency.rhythmicNovelty: raw beat pattern change, 0..1
saliency.timbralNovelty: raw spectral character change, 0..1
saliency.dynamicNovelty: raw loudness envelope change, 0..1
saliency.overallSaliency: weighted composite, 0..1
saliency.dominantType: HARMONIC=0, RHYTHMIC=1, TIMBRAL=2, DYNAMIC=3
saliency.harmonicNoveltySmooth: asymmetric-smoothed (fast rise, slow fall) — USE THIS in effects, not raw
saliency.rhythmicNoveltySmooth: smoothed rhythmic — USE THIS
saliency.timbralNoveltySmooth: smoothed timbral — USE THIS
saliency.dynamicNoveltySmooth: smoothed dynamic — USE THIS
History fields: prevChordRoot, prevChordType, prevFlux, prevRms, beatIntervalHistory[4], beatIntervalIdx, lastBeatTimeMs

### Style Detection (MIS Phase 2)
currentStyle: UNKNOWN=0, RHYTHMIC_DRIVEN=1, HARMONIC_DRIVEN=2, MELODIC_DRIVEN=3, TEXTURE_DRIVEN=4, DYNAMIC_DRIVEN=5
styleConfidence: 0..1

### Motion Semantics (Stage 2)
timing_jitter: CV of inter-onset intervals, 0=regular, 1=irregular. IOI ring buffer 16 samples, min IOI gate 80ms.
syncopation_level: onset deviation from metric grid, 0=on-beat, 1=off-beat. EMA alpha 0.15.
pitch_contour_dir: spectral centroid movement, -1=descending, 0=flat, +1=ascending. Lambda=5 (~200ms tau).

### Silence Detection
Input: rmsUngated vs silence_threshold
Hysteresis timer with smooth fade, EMA alpha for ~400ms transition
silentScale: 1.0=active, 0.0=silent (multiply with brightness, do not branch on isSilent)
isSilent: bool

### Scene Parameters (TranslationEngine, FEATURE_TRANSLATION_ENGINE)
scene.motion_type: DRIFT, FLOW, BLOOM, RECOIL, LOCK, DECAY (MotionPrimitive enum)
scene.brightness_scale: 0.15..1.25
scene.motion_rate: 0.60..1.80
scene.motion_depth: 0..1
scene.hue_shift_speed: 0..1
scene.diffusion: 0..1
scene.beat_pulse: 0..1
scene.tension: 0..1
scene.phrase_progress: 0..1
scene.transition_progress: 0..1
scene.phrase_boundary: bool, true on phrase boundary frame
scene.timing_reliable: bool, true when tempo tracking is reliable

## Section 8: ControlBusFrame Complete Field List

### Identity / Sequencing
t: AudioTime (sample_index uint64, sample_rate_hz uint32, monotonic_us uint64) — monotonic audio timestamp
hop_seq: uint32_t — monotonic hop counter, detects frame staleness

### Core Scalars
rms: float 0..1 — slow-smoothed RMS
fast_rms: float 0..1 — fast-smoothed RMS (tracks transients)
flux: float 0..1 — slow-smoothed spectral flux
fast_flux: float 0..1 — fast-smoothed spectral flux
liveliness: float 0..1 — global speed trim (conf*0.6 + flux*0.4, tau 0.3s)

### Octave Bands
bands[8]: float[8] 0..1 — asymmetric attack/release smoothed, zone-AGC normalised. band 0=sub-bass, 7=air. PRIMARY frequency access path.
heavy_bands[8]: float[8] 0..1 — extra-smoothed for sustained glow (LGP_SMOOTH path)

### Chroma
chroma[12]: float[12] 0..1 — smoothed pitch class energy. 0=C, 1=C#, ..., 11=B. Backend-agnostic.
heavy_chroma[12]: float[12] 0..1 — extra-smoothed chroma

### Waveform
waveform[128]: int16_t[128] — time-domain PCM, -32768..32767

### Sensory Bridge Parity
sb_waveform[128]: int16_t[128] — SB 3.1.0 post-capture-scaling waveform
sb_waveform_peak_scaled: float — current scaled peak
sb_waveform_peak_scaled_last: float — previous frame scaled peak
sb_note_chromagram[12]: float[12] 0..1 — SB chroma representation
sb_chromagram_max_val: float — max in SB chromagram
sb_spectrogram[64]: float[64] — SB normalised 64-bin spectrogram
sb_spectrogram_smooth[64]: float[64] — SB smoothed 64-bin spectrogram
sb_chromagram_smooth[12]: float[12] 0..1 — SB smoothed chroma
sb_hue_position: float — SB-derived hue position scalar
sb_hue_shifting_mix: float — SB hue shifting mix (default -0.35)

### Chord Detection
chordState.rootNote: uint8_t 0-11 (C=0..B=11)
chordState.type: ChordType uint8_t (NONE=0, MAJOR=1, MINOR=2, DIMINISHED=3, AUGMENTED=4)
chordState.confidence: float 0..1 — triad energy ratio
chordState.rootStrength: float — root pitch class energy
chordState.thirdStrength: float — third interval energy
chordState.fifthStrength: float — fifth interval energy

### Musical Saliency
saliency.harmonicNovelty: float 0..1 — raw harmonic
saliency.rhythmicNovelty: float 0..1 — raw rhythmic
saliency.timbralNovelty: float 0..1 — raw timbral
saliency.dynamicNovelty: float 0..1 — raw dynamic
saliency.overallSaliency: float 0..1 — weighted composite
saliency.dominantType: uint8_t (HARMONIC=0, RHYTHMIC=1, TIMBRAL=2, DYNAMIC=3)
saliency.harmonicNoveltySmooth: float 0..1 — USE THIS in effects
saliency.rhythmicNoveltySmooth: float 0..1 — USE THIS in effects
saliency.timbralNoveltySmooth: float 0..1 — USE THIS in effects
saliency.dynamicNoveltySmooth: float 0..1 — USE THIS in effects
saliency.prevChordRoot: uint8_t — history
saliency.prevChordType: uint8_t — history
saliency.prevFlux: float — history
saliency.prevRms: float — history
saliency.beatIntervalHistory[4]: float[4] — ring buffer of last 4 beat intervals (ms)
saliency.beatIntervalIdx: uint8_t — write head
saliency.lastBeatTimeMs: float — timestamp of last beat (ms)

### Style Detection
currentStyle: MusicStyle uint8_t (UNKNOWN=0, RHYTHMIC_DRIVEN=1, HARMONIC_DRIVEN=2, MELODIC_DRIVEN=3, TEXTURE_DRIVEN=4, DYNAMIC_DRIVEN=5)
styleConfidence: float 0..1

### Scene Parameters (pre-computed motion semantics)
scene.motion_type: MotionPrimitive uint8_t (DRIFT, FLOW, BLOOM, RECOIL, LOCK, DECAY)
scene.brightness_scale: float 0.15..1.25
scene.motion_rate: float 0.60..1.80
scene.motion_depth: float 0..1
scene.hue_shift_speed: float 0..1
scene.diffusion: float 0..1
scene.beat_pulse: float 0..1
scene.tension: float 0..1
scene.phrase_progress: float 0..1
scene.transition_progress: float 0..1
scene.phrase_boundary: bool — true on phrase boundary
scene.timing_reliable: bool — true when tempo is reliable

### Percussion Onset
snareEnergy: float 0..1 — 150-300 Hz band
hihatEnergy: float 0..1 — 6-12 kHz band
snareTrigger: bool — true on snare onset
hihatTrigger: bool — true on hi-hat onset

### Goertzel Spectrum (64-bin, 110-4186 Hz)
bins64[64]: float[64] 0..1 — raw normalised magnitudes
bins64Adaptive[64]: float[64] 0..1 — adaptive normalised (Sensory Bridge max-follower)

### FFT Spectrum (PipelineCore only — broken backend)
bins256[256]: float[256] 0..1 — normalised FFT magnitudes, 62.5 Hz spacing at 32kHz/512pt
binHz: float — frequency resolution (sampleRate / fftSize)

### Tempo (backend-neutral)
tempoLocked: bool — tracker lock state
tempoConfidence: float 0..1 — tracker confidence (effects should prefer MusicalGrid.confidence)
tempoBeatTick: bool — beat tick gated by lock
tempoDownbeatTick: bool — downbeat tick (4/4 grid)
tempoBpm: float — BPM estimate (default 120.0)
tempoBeatStrength: float 0..1 — beat event strength (0 = no beat this hop)

### ESv11 Tempo Extras
es_bpm: float — ESv11 BPM estimate
es_tempo_confidence: float 0..1
es_beat_tick: bool — ESv11 beat tick
es_beat_strength: float 0..1
es_phase01_at_audio_t: float [0,1) — continuous beat phase for smooth animation
es_beat_in_bar: uint8_t — beat position in bar (0-3)
es_downbeat_tick: bool — bar boundary

### ESv11 Raw Signals (unprocessed ES pipeline outputs for parity effects)
es_vu_level_raw: float 0..1 — direct ES VU before LWLS normalisation
es_bins64_raw[64]: float[64] 0..1 — direct ES spectrogram_smooth
es_chroma_raw[12]: float[12] 0..1 — direct ES chromagram

### Silence
silentScale: float — 1.0=active, 0.0=silent. Multiply with brightness.
isSilent: bool — true when silence threshold exceeded with hysteresis

### Motion Semantics (Stage 2)
timing_jitter: float — IOI coefficient of variation, 0=regular, 1=irregular
syncopation_level: float — onset deviation from grid, 0=on-beat, 1=off-beat
pitch_contour_dir: float — spectral centroid direction, -1=descending, +1=ascending

## Section 9: Effect Author Quick Reference

Tier 1 (high-level, recommended): scene.* fields — pre-computed motion semantics, plug and play
Tier 2 (most common): bands[8], chroma[12], tempoBeatTick, rms, liveliness, saliency.*Smooth
Tier 3 (specialist): bins64[64], waveform[128], es_phase01_at_audio_t, snareTrigger, hihatTrigger

Rules:
- Use bands[0..7] for frequency, NOT bins64 or bins256 (bands are backend-agnostic)
- Use saliency.*Smooth fields, NOT raw saliency.* fields (smoothed have proper attack/release)
- Use silentScale as a multiplier, do not branch on isSilent
- heavy_bands/heavy_chroma are pre-smoothed with 80ms rise — do NOT add additional smoothing (causes 630ms+ latency)
- All fields are 0..1 normalised unless noted otherwise
- dt-decay pattern: var = effects::chroma::dtDecay(var, rate, rawDt) instead of var *= rate

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-23 | agent:research-swarm | Created from ControlBus.h, ESV11 backend, EsV11Adapter, PipelineAdapter source analysis |
