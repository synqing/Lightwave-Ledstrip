/**
 * @file audio-manager.ts
 * @brief Top-level audio coordinator — mic capture, Goertzel DSP, adapter, beat clock.
 *
 * Two modes:
 *   - Live: real microphone input → AudioWorklet (hop accumulation) → main thread
 *     Goertzel/VU/Tempo DSP → AudioAdapter → ControlBusFrame
 *   - Simulated: synthetic sine-wave test data (no mic required)
 *
 * The Goertzel DSP runs on the main thread (not in the worklet) to avoid
 * AudioWorklet bundling complexity in the POC. Cost: ~3ms per frame at 125 Hz
 * hop rate — within the 16.67ms budget.
 */

import type { ControlBusFrame } from '../engine/types';
import { AudioAdapter } from './adapter';
import type { RawDspOutputs } from './adapter';
import { BeatClock } from './beat-clock';
import { GoertzelProcessor } from './worklet/goertzel-processor';
import { VuProcessor } from './worklet/vu-processor';
import { TempoProcessor } from './worklet/tempo-processor';
import { SAMPLE_RATE, CHUNK_SIZE, HOP_SIZE } from './constants';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const WORKLET_URL = '/worklet/esv11-processor.js';
const NUM_BANDS = 8;
const NUM_CHROMA = 12;
const BINS_64_COUNT = 64;
const WAVEFORM_N = 128;

// ---------------------------------------------------------------------------
// Simulation helpers
// ---------------------------------------------------------------------------

function generateSimulatedFrame(time: number, hopSeq: number): ControlBusFrame {
  const basePhase = time * 0.5 * Math.PI * 2;
  const rms = 0.3 + 0.2 * Math.sin(basePhase);

  const bands = new Float32Array(NUM_BANDS);
  const heavyBands = new Float32Array(NUM_BANDS);
  for (let i = 0; i < NUM_BANDS; i++) {
    const bandPhase = basePhase + i * 0.4;
    bands[i] = 0.2 + 0.15 * Math.sin(bandPhase);
    heavyBands[i] = 0.18 + 0.10 * Math.sin(bandPhase * 0.7);
  }

  const chroma = new Float32Array(NUM_CHROMA);
  const heavyChroma = new Float32Array(NUM_CHROMA);
  const dominantNote = Math.floor((time * 2) % NUM_CHROMA);
  for (let i = 0; i < NUM_CHROMA; i++) {
    const dist = Math.min(Math.abs(i - dominantNote), NUM_CHROMA - Math.abs(i - dominantNote));
    chroma[i] = Math.max(0, 0.6 - dist * 0.15);
    heavyChroma[i] = (chroma[i] ?? 0) * 0.8;
  }

  const bins64 = new Float32Array(BINS_64_COUNT);
  for (let i = 0; i < BINS_64_COUNT; i++) {
    const decay = Math.exp(-i * 0.04);
    const shimmer = 0.05 * Math.sin(basePhase + i * 0.3);
    bins64[i] = Math.max(0, Math.min(1, rms * decay + shimmer));
  }

  const simBpm = 120;
  const beatPeriod = 60.0 / simBpm;
  const beatPhase = (time % beatPeriod) / beatPeriod;
  const beatTick = beatPhase < 0.02;
  const beatStrength = beatTick ? 1.0 : Math.exp(-(beatPhase * beatPeriod) / 0.30);

  return {
    rms, fastRms: rms,
    flux: 0.1 + 0.05 * Math.sin(basePhase * 1.3),
    fastFlux: 0.1 + 0.05 * Math.sin(basePhase * 1.3),
    liveliness: 0,
    bands, heavyBands, chroma, heavyChroma, bins64,
    beatPhase, beatStrength, beatTick,
    downbeatTick: beatTick && Math.floor((time / beatPeriod) % 4) === 0,
    bpm: simBpm, tempoConfidence: 0.85,
    snareEnergy: 0, hihatEnergy: 0, snareTrigger: false, hihatTrigger: false,
    silentScale: 1.0, isSilent: false,
    hopSeq,
  };
}

// ---------------------------------------------------------------------------
// AudioManager
// ---------------------------------------------------------------------------

export class AudioManager {
  private audioCtx: AudioContext | null = null;
  private workletNode: AudioWorkletNode | null = null;
  private sourceNode: MediaStreamAudioSourceNode | null = null;
  private mediaStream: MediaStream | null = null;

  // DSP processors (run on main thread for POC)
  private goertzel: GoertzelProcessor;
  private vu: VuProcessor;
  private tempo: TempoProcessor;
  private adapter: AudioAdapter;
  private beatClock: BeatClock;

  private latestFrame: ControlBusFrame;
  private hopSeq = 0;
  private active = false;
  private simulated = false;
  private simulatedTime = 0;
  private startTimeMs = 0;

  constructor() {
    this.goertzel = new GoertzelProcessor();
    this.vu = new VuProcessor();
    this.tempo = new TempoProcessor();
    this.adapter = new AudioAdapter();
    this.beatClock = new BeatClock();
    this.latestFrame = generateSimulatedFrame(0, 0);
  }

  // -----------------------------------------------------------------------
  // Lifecycle
  // -----------------------------------------------------------------------

  async start(): Promise<void> {
    if (this.active) return;

    try {
      this.audioCtx = new AudioContext({ sampleRate: SAMPLE_RATE });

      this.mediaStream = await navigator.mediaDevices.getUserMedia({
        audio: {
          sampleRate: SAMPLE_RATE,
          echoCancellation: false,
          noiseSuppression: false,
          autoGainControl: false,
        },
      });

      await this.audioCtx.audioWorklet.addModule(WORKLET_URL);

      this.workletNode = new AudioWorkletNode(this.audioCtx, 'esv11-processor');

      // Receive raw samples from worklet, run DSP on main thread
      this.workletNode.port.onmessage = (event: MessageEvent) => {
        const data = event.data as { type?: string; samples?: Float32Array; waveform?: Int16Array; timestamp?: number };
        if (data.type === 'hop' && data.samples instanceof Float32Array) {
          this.onHopReceived(data.samples, data.waveform ?? new Int16Array(WAVEFORM_N), data.timestamp ?? 0);
        }
      };

      this.sourceNode = this.audioCtx.createMediaStreamSource(this.mediaStream);
      this.sourceNode.connect(this.workletNode);

      // Initialise DSP processors
      this.goertzel.reset();
      this.vu.init();
      this.tempo.init();
      this.adapter.reset();
      this.beatClock.reset();
      this.startTimeMs = performance.now();

      this.active = true;
      this.simulated = false;
    } catch (err) {
      console.warn('[AudioManager] Mic access failed, using simulated mode:', err);
      this.startSimulated();
    }
  }

  startSimulated(): void {
    if (this.active) return;
    this.adapter.reset();
    this.beatClock.reset();
    this.simulated = true;
    this.simulatedTime = 0;
    this.active = true;
  }

  stop(): void {
    if (!this.active) return;

    if (this.sourceNode) { this.sourceNode.disconnect(); this.sourceNode = null; }
    if (this.workletNode) { this.workletNode.port.onmessage = null; this.workletNode.disconnect(); this.workletNode = null; }
    if (this.mediaStream) { for (const track of this.mediaStream.getTracks()) track.stop(); this.mediaStream = null; }
    if (this.audioCtx) { void this.audioCtx.close(); this.audioCtx = null; }

    this.adapter.reset();
    this.beatClock.reset();
    this.active = false;
    this.simulated = false;
  }

  // -----------------------------------------------------------------------
  // Frame tick (called at 60fps)
  // -----------------------------------------------------------------------

  tick(dt: number): void {
    if (!this.active) return;

    if (this.simulated) {
      this.simulatedTime += dt;
      this.latestFrame = this.simulateFrame(this.simulatedTime);
    }

    this.beatClock.tick(dt);

    this.latestFrame = {
      ...this.latestFrame,
      beatPhase: this.beatClock.beatPhase,
      beatTick: this.beatClock.beatTick,
      beatStrength: this.beatClock.beatStrength,
      downbeatTick: this.beatClock.downbeatTick,
      bpm: this.beatClock.currentBpm,
      tempoConfidence: this.beatClock.tempoConfidence,
    };
  }

  // -----------------------------------------------------------------------
  // Accessors
  // -----------------------------------------------------------------------

  getFrame(): ControlBusFrame { return this.latestFrame; }
  get isActive(): boolean { return this.active; }
  get isSimulated(): boolean { return this.simulated; }

  // -----------------------------------------------------------------------
  // Live DSP — process raw samples from worklet on main thread
  // -----------------------------------------------------------------------

  private onHopReceived(samples: Float32Array, waveform: Int16Array, _timestamp: number): void {
    const currentTimeMs = performance.now() - this.startTimeMs;
    const chunkDtMs = (CHUNK_SIZE / SAMPLE_RATE) * 1000; // ~4ms per chunk

    // Feed samples into Goertzel (two chunks per hop)
    const chunk1 = samples.subarray(0, CHUNK_SIZE);
    const chunk2 = samples.subarray(CHUNK_SIZE, HOP_SIZE);
    this.goertzel.processSamples(chunk1);
    this.goertzel.processSamples(chunk2);

    // Run Goertzel spectral analysis
    this.goertzel.calculateMagnitudes();

    // Run VU meter (reads from Goertzel's sample history)
    this.vu.update(
      this.goertzel.getSampleHistory(),
      this.goertzel.getSampleHistoryLength(),
      currentTimeMs,
    );

    // Get spectrogram for tempo novelty + chromagram
    const spectrogram = this.goertzel.getSpectrogram();
    const chromagram = this.goertzel.getChromagram();

    // Compute novelty (positive spectral flux) from the Goertzel bins
    const noveltyRaw = this.goertzel.computeNovelty();

    // Run tempo tracker
    this.tempo.updateNovelty(spectrogram, this.vu.getVuLevel(), chunkDtMs);
    this.tempo.calculateTempi();
    this.tempo.stabiliseAndRefresh(currentTimeMs);

    // Build RawDspOutputs for the adapter
    const raw: RawDspOutputs = {
      spectrogramSmooth: spectrogram,
      chromagram: chromagram,
      vuLevel: this.vu.getVuLevel(),
      noveltyNormLast: Math.min(1.0, noveltyRaw * 10.0), // Rough normalisation
      topBpm: this.tempo.getTopBpm(),
      tempoConfidence: this.tempo.getTempoConfidence(),
      phaseRadians: this.tempo.getPhaseRadians(),
      beatTick: this.tempo.getBeatTick(),
      beatStrength: this.tempo.getBeatStrength(),
      waveform: waveform,
    };

    this.hopSeq++;
    const frame = this.adapter.buildFrame(raw, this.hopSeq);

    this.beatClock.onAudioFrame(
      frame.bpm, frame.beatPhase, frame.beatTick, frame.beatStrength, frame.tempoConfidence,
    );

    this.latestFrame = frame;
  }

  // -----------------------------------------------------------------------
  // Simulated frame
  // -----------------------------------------------------------------------

  private simulateFrame(time: number): ControlBusFrame {
    this.hopSeq++;
    const frame = generateSimulatedFrame(time, this.hopSeq);
    this.beatClock.onAudioFrame(frame.bpm, frame.beatPhase, frame.beatTick, frame.beatStrength, frame.tempoConfidence);
    return frame;
  }
}
