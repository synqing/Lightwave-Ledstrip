/**
 * @file esv11-processor.js
 * @brief AudioWorklet processor — accumulates 256-sample hops and posts
 * raw sample data to the main thread for Goertzel DSP processing.
 *
 * The actual DSP runs on the main thread (GoertzelProcessor, VuProcessor,
 * TempoProcessor) to avoid AudioWorklet bundling complexity in the POC.
 * This worklet just handles the real-time audio capture + hop accumulation.
 */

/* eslint-disable no-undef */

const HOP_SIZE = 256;
const WAVEFORM_N = 128;

class Esv11Processor extends AudioWorkletProcessor {
  constructor() {
    super();
    this._hopBuffer = new Float32Array(HOP_SIZE);
    this._hopPos = 0;
  }

  process(inputs, _outputs) {
    const input = inputs[0];
    if (!input || !input[0]) return true;

    const samples = input[0];

    for (let i = 0; i < samples.length; i++) {
      this._hopBuffer[this._hopPos] = samples[i];
      this._hopPos++;

      if (this._hopPos >= HOP_SIZE) {
        this._hopPos = 0;

        // Extract waveform (last 128 samples as int16)
        const waveform = new Int16Array(WAVEFORM_N);
        for (let j = 0; j < WAVEFORM_N; j++) {
          const s = this._hopBuffer[HOP_SIZE - WAVEFORM_N + j];
          waveform[j] = Math.max(-32768, Math.min(32767, Math.round(s * 32767)));
        }

        // Post raw samples + waveform to main thread
        this.port.postMessage({
          type: 'hop',
          samples: this._hopBuffer.slice(),  // Copy — buffer will be reused
          waveform: waveform,
          timestamp: currentTime * 1000,     // ms
        });
      }
    }

    return true;
  }
}

registerProcessor('esv11-processor', Esv11Processor);
