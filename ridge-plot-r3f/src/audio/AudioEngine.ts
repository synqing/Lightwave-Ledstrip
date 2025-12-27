export class AudioEngine {
  private ctx: AudioContext | null = null;
  private analyser: AnalyserNode | null = null;
  private sink: GainNode | null = null;

  // Multi-oscillator synth for full-spectrum visualization
  private osc1: OscillatorNode | null = null;  // Bass
  private osc2: OscillatorNode | null = null;  // Mid
  private osc3: OscillatorNode | null = null;  // Treble
  private lfo: OscillatorNode | null = null;
  private lfoGain1: GainNode | null = null;
  private lfoGain2: GainNode | null = null;
  private lfoGain3: GainNode | null = null;
  private mediaSource: MediaStreamAudioSourceNode | null = null;

  private stream: MediaStream | null = null;

  private _freq: Uint8Array<ArrayBuffer> = new Uint8Array(0);

  constructor(private readonly fftSize: number = 512) {}

  get ready(): boolean {
    return !!this.analyser && !!this.ctx;
  }

  get frequencyData(): Uint8Array<ArrayBuffer> {
    return this._freq;
  }

  get binCount(): number {
    return this._freq.length;
  }

  private async ensureContext(): Promise<void> {
    if (!this.ctx) {
      const Ctx = window.AudioContext || (window as any).webkitAudioContext;
      this.ctx = new Ctx();

      this.analyser = this.ctx.createAnalyser();
      this.analyser.fftSize = this.fftSize;
      // Lower smoothing = more responsive to transients (was 0.8, too sluggish)
      this.analyser.smoothingTimeConstant = 0.4;

      // frequencyBinCount is fftSize/2 for byte frequency data visualizers.
      this._freq = new Uint8Array(this.analyser.frequencyBinCount);

      // Keep the graph "alive" across browsers while staying silent.
      // (Some engines won't process nodes that aren't connected to a destination.)
      this.sink = this.ctx.createGain();
      this.sink.gain.value = 0;
      this.analyser.connect(this.sink);
      this.sink.connect(this.ctx.destination);
    }

    if (this.ctx.state === "suspended") {
      await this.ctx.resume();
    }
  }

  private disconnectAllSources(): void {
    try {
      this.osc1?.disconnect();
      this.osc2?.disconnect();
      this.osc3?.disconnect();
      this.lfo?.disconnect();
      this.lfoGain1?.disconnect();
      this.lfoGain2?.disconnect();
      this.lfoGain3?.disconnect();
      this.mediaSource?.disconnect();
    } catch {}

    this.osc1 = null;
    this.osc2 = null;
    this.osc3 = null;
    this.lfo = null;
    this.lfoGain1 = null;
    this.lfoGain2 = null;
    this.lfoGain3 = null;
    this.mediaSource = null;

    if (this.stream) {
      for (const t of this.stream.getTracks()) t.stop();
      this.stream = null;
    }
  }

  async initSynth(): Promise<void> {
    await this.ensureContext();
    this.disconnectAllSources();

    const ctx = this.ctx!;
    const analyser = this.analyser!;

    // Multi-frequency synth to fill the full spectrum (not just bins 0-2!)
    // Bass oscillator (sawtooth for harmonics)
    const osc1 = ctx.createOscillator();
    osc1.type = "sawtooth";
    osc1.frequency.value = 110;  // ~86Hz bin = bin 1

    // Mid oscillator (square for odd harmonics)
    const osc2 = ctx.createOscillator();
    osc2.type = "square";
    osc2.frequency.value = 440;  // ~440Hz = bin 5

    // Treble oscillator (sine for clean high)
    const osc3 = ctx.createOscillator();
    osc3.type = "sine";
    osc3.frequency.value = 1760; // ~1.7kHz = bin 20

    // LFO to modulate all frequencies (creates sweeping motion)
    const lfo = ctx.createOscillator();
    lfo.type = "sine";
    lfo.frequency.value = 0.25; // Slower sweep

    // Different modulation depths for each band
    const lfoGain1 = ctx.createGain();
    lfoGain1.gain.value = 60;   // ±60Hz for bass
    const lfoGain2 = ctx.createGain();
    lfoGain2.gain.value = 180;  // ±180Hz for mid
    const lfoGain3 = ctx.createGain();
    lfoGain3.gain.value = 600;  // ±600Hz for treble

    // Wire up LFO modulation
    lfo.connect(lfoGain1);
    lfo.connect(lfoGain2);
    lfo.connect(lfoGain3);
    lfoGain1.connect(osc1.frequency);
    lfoGain2.connect(osc2.frequency);
    lfoGain3.connect(osc3.frequency);

    // Connect all oscillators to analyser
    osc1.connect(analyser);
    osc2.connect(analyser);
    osc3.connect(analyser);

    // Start everything
    osc1.start();
    osc2.start();
    osc3.start();
    lfo.start();

    // Store references for cleanup
    this.osc1 = osc1;
    this.osc2 = osc2;
    this.osc3 = osc3;
    this.lfo = lfo;
    this.lfoGain1 = lfoGain1;
    this.lfoGain2 = lfoGain2;
    this.lfoGain3 = lfoGain3;
  }

  async initMic(): Promise<void> {
    await this.ensureContext();
    this.disconnectAllSources();

    const ctx = this.ctx!;
    const analyser = this.analyser!;

    const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    const src = ctx.createMediaStreamSource(stream);

    src.connect(analyser);
    this.stream = stream;
    this.mediaSource = src; // Store reference for proper cleanup
  }

  update(): void {
    if (!this.analyser) return;
    // Pull byte frequency data into our pre-allocated Uint8Array.
    this.analyser.getByteFrequencyData(this._freq);
  }

  dispose(): void {
    this.disconnectAllSources();
    if (this.ctx) {
      this.ctx.close();
      this.ctx = null;
    }
    this.analyser = null;
    this.sink = null;
  }
}

export const audioEngine = new AudioEngine(512); // 512 FFT => 256 bins
