/**
 * DiagnosticDataAdapter.js
 *
 * WebSocket data adapter for the Composer Diagnostic Panel (Scenario B).
 * Opens its own WebSocket connection, subscribes to LED and audio binary
 * streams, parses binary frames, and builds frame objects in the exact
 * shape the diagnostic UI component expects.
 *
 * Exports:
 *   useDiagnosticStream(host, bufferRef, setBuffer)
 *
 * Frame protocol:
 *   - LED frame:   966 bytes, magic 0xFE at offset 0
 *   - Audio frame: 464 bytes, magic 0x00445541 ("AUD\0" LE) at offset 0
 *
 * NOTE: Wrapped in an IIFE to avoid duplicate `const` declarations in the
 * global scope when Babel transpiles proto-combined.jsx (which also
 * destructures React hooks at the top level).
 */

(function () {
  const { useEffect, useRef } = React;

  // ---------------------------------------------------------------------------
  // Constants
  // ---------------------------------------------------------------------------

  const BUFFER_SIZE = 360;

  const LED_FRAME_SIZE   = 966;
  const AUDIO_FRAME_SIZE = 464;
  const LED_MAGIC        = 0xFE;
  const LED_VERSION      = 0x01;
  const AUDIO_MAGIC_U32  = 0x00445541; // "AUD\0" as uint32 LE

  const LEDS_PER_STRIP = 160;
  const SAMPLED_LEDS   = 80;

  // ---------------------------------------------------------------------------
  // rgbToHsl — converts 0-255 RGB to [h 0-360, s 0-100, l 0-100]
  // ---------------------------------------------------------------------------

  function rgbToHsl(r, g, b) {
    r /= 255; g /= 255; b /= 255;
    const max = Math.max(r, g, b), min = Math.min(r, g, b);
    const l = (max + min) / 2;
    let h = 0, s = 0;
    if (max !== min) {
      const d = max - min;
      s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
      switch (max) {
        case r: h = ((g - b) / d + (g < b ? 6 : 0)) / 6; break;
        case g: h = ((b - r) / d + 2) / 6; break;
        case b: h = ((r - g) / d + 4) / 6; break;
      }
    }
    return [h * 360, s * 100, l * 100];
  }

  // ---------------------------------------------------------------------------
  // parseLedFrame — extract strip0 (160 RGB objects) from a 966-byte frame
  // ---------------------------------------------------------------------------

  function parseLedFrame(buf) {
    if (buf.byteLength !== LED_FRAME_SIZE) return null;

    const view = new DataView(buf);
    const magic   = view.getUint8(0);
    const version = view.getUint8(1);

    if (magic !== LED_MAGIC || version !== LED_VERSION) return null;

    // Strip 0 RGB data starts at offset 5, 3 bytes per LED, 160 LEDs.
    const strip0 = new Array(LEDS_PER_STRIP);
    const bytes  = new Uint8Array(buf);
    for (let i = 0; i < LEDS_PER_STRIP; i++) {
      const off = 5 + i * 3;
      strip0[i] = { r: bytes[off], g: bytes[off + 1], b: bytes[off + 2] };
    }
    return strip0;
  }

  // ---------------------------------------------------------------------------
  // parseAudioFrame — extract audio telemetry from a 464-byte frame
  // ---------------------------------------------------------------------------

  function parseAudioFrame(buf) {
    if (buf.byteLength !== AUDIO_FRAME_SIZE) return null;

    const view = new DataView(buf, 0, buf.byteLength);
    const magic = view.getUint32(0, true);
    if (magic !== AUDIO_MAGIC_U32) return null;

    const hopSeq         = view.getUint32(4, true);
    const timestampMs    = view.getUint32(8, true);
    const rms            = view.getFloat32(12, true);
    const flux           = view.getFloat32(16, true);
    const fastRms        = view.getFloat32(20, true);
    const fastFlux       = view.getFloat32(24, true);

    // 8 frequency bands at offset 28, float32 each
    const bands = new Array(8);
    for (let i = 0; i < 8; i++) {
      bands[i] = view.getFloat32(28 + i * 4, true);
    }

    // 8 heavy bands at offset 60
    const heavyBands = new Array(8);
    for (let i = 0; i < 8; i++) {
      heavyBands[i] = view.getFloat32(60 + i * 4, true);
    }

    // 12 chroma bins at offset 92
    const chroma = new Array(12);
    for (let i = 0; i < 12; i++) {
      chroma[i] = view.getFloat32(92 + i * 4, true);
    }

    // Tail fields
    const bpmSmoothed     = view.getFloat32(448, true);
    const tempoConfidence = view.getFloat32(452, true);
    const beatPhase01     = view.getFloat32(456, true);
    const beatTick        = view.getUint8(460);
    const downbeatTick    = view.getUint8(461);

    return {
      hopSeq,
      timestampMs,
      rms,
      flux,
      fastRms,
      fastFlux,
      bands,
      heavyBands,
      chroma,
      bpmSmoothed,
      tempoConfidence,
      beatPhase01,
      beatTick,
      downbeatTick,
    };
  }

  // ---------------------------------------------------------------------------
  // buildFrame — assemble the diagnostic frame from strip0 + audio data
  // ---------------------------------------------------------------------------

  function buildFrame(strip0, audio) {
    // Derive band averages for the frame-level summary
    const bass   = (audio.bands[0] + audio.bands[1]) / 2;
    const mid    = (audio.bands[2] + audio.bands[3] + audio.bands[4]) / 3;
    const treble = (audio.bands[5] + audio.bands[6] + audio.bands[7]) / 3;

    // Build the 80 sampled LEDs (every other LED from strip0)
    let hasClip = false;
    let hasDim  = false;

    const leds = new Array(SAMPLED_LEDS);

    for (let i = 0; i < SAMPLED_LEDS; i++) {
      const srcLed = strip0[i * 2];
      const [h, s, l] = rgbToHsl(srcLed.r, srcLed.g, srcLed.b);
      const pos = i / 79; // 0.0 to 1.0 across the 80 sampled positions

      // Determine frequency band assignment for this LED position
      let freqBand, bandEnergy;
      if (i < 20) {
        freqBand   = "bass";
        bandEnergy = bass;
      } else if (i < 50) {
        freqBand   = "mid";
        bandEnergy = mid;
      } else {
        freqBand   = "treble";
        bandEnergy = treble;
      }

      // Clipping / dimming detection
      const clipped = l > 97;
      const dimmed  = l < 5 && bandEnergy > 0.25;
      if (clipped) hasClip = true;
      if (dimmed)  hasDim  = true;

      // Per-LED stage derivation
      const audioInputVal = Math.min(1, bandEnergy * (1 + audio.fastFlux * 0.3));
      const spectrumVal   = Math.pow(audioInputVal, 0.7);
      const beatModVal    = Math.min(1, spectrumVal * (1 + audio.beatPhase01 * 0.6));
      const mappingVal    = beatModVal * (0.85 + 0.15 * Math.sin(pos * Math.PI));
      const outputLum     = l / 100; // normalised luminance from actual LED colour

      leds[i] = {
        index:    i,
        pos:      pos,
        freqBand: freqBand,
        specBin:  Math.floor(pos * 32),
        stages: {
          audioInput: { value: audioInputVal, label: "Band Energy", unit: freqBand,                             color: "#4af" },
          spectrum:   { value: spectrumVal,   label: "Approx \u03B3", unit: "est",                              color: "#6cf" },
          beat:       { value: beatModVal,    label: "Beat Est",      unit: `\u03C6${audio.beatPhase01.toFixed(2)}`, color: "#fa6" },
          mapping:    { value: mappingVal,    label: "Pos Est",       unit: "env",                              color: "#af7" },
          color:      { value: mappingVal,     label: "Color",         unit: `H${Math.round(h)}`,                color: "#c8f", hsl: [h, s, l] },
          output:     { value: outputLum,     label: "LED Output",    unit: "real",                             color: "#eee", hsl: [h, s, l] },
        },
        clipped: clipped,
        dimmed:  dimmed,
        hsl:     [h, s, l],
      };
    }

    // Frame-level anomaly flag: clip takes priority over dim
    let anomaly = null;
    if (hasClip)     anomaly = "clip";
    else if (hasDim) anomaly = "dim";

    return {
      T:       audio.hopSeq,
      bass:    bass,
      mid:     mid,
      treble:  treble,
      beat:    audio.beatTick ? 1 : 0,
      conf:    audio.tempoConfidence,
      anomaly: anomaly,
      leds:    leds,
    };
  }

  // ---------------------------------------------------------------------------
  // useDiagnosticStream — React hook that manages the WebSocket lifecycle
  // ---------------------------------------------------------------------------

  /**
   * Opens a dedicated WebSocket to the ESP32 device, subscribes to LED and
   * audio binary streams, parses incoming frames, and pushes assembled
   * diagnostic frame objects into the caller's ring buffer.
   *
   * @param {string|null} host    - Device hostname or IP (e.g. "192.168.4.1").
   *                                Pass null/empty to stay disconnected.
   * @param {React.MutableRefObject} bufferRef - Ref whose .current is the
   *                                frame ring buffer array.
   * @param {function} setBuffer  - State setter to trigger re-renders
   *                                (called every other frame for efficiency).
   */
  function useDiagnosticStream(host, bufferRef, setBuffer) {
    // Retain the latest audio data between frames so we can pair it
    // with LED frames that arrive on a different cadence.
    const latestAudioRef = useRef(null);

    // Frame counter — used to throttle setBuffer calls to every other frame.
    const frameCountRef = useRef(0);

    useEffect(() => {
      // Guard: don't connect if host is absent.
      if (!host) return;

      const ws = new WebSocket(`ws://${host}/ws`);
      ws.binaryType = "arraybuffer";

      // ------ connection opened ------
      ws.addEventListener("open", () => {
        ws.send(JSON.stringify({ type: "ledStream.subscribe" }));
        ws.send(JSON.stringify({ type: "audio.subscribe" }));
      });

      // ------ incoming message handler ------
      ws.addEventListener("message", async (event) => {
        let buf;

        // Handle both ArrayBuffer and Blob payloads.
        if (event.data instanceof ArrayBuffer) {
          buf = event.data;
        } else if (event.data instanceof Blob) {
          buf = await event.data.arrayBuffer();
        } else {
          // JSON text messages — not our concern; ignore silently.
          return;
        }

        if (buf.byteLength < 4) return; // too small to identify

        const firstByte = new Uint8Array(buf)[0];

        // ---- Audio frame identification (4-byte magic) ----
        if (buf.byteLength === AUDIO_FRAME_SIZE) {
          const magic32 = new DataView(buf).getUint32(0, true);
          if (magic32 === AUDIO_MAGIC_U32) {
            const audio = parseAudioFrame(buf);
            if (audio) {
              latestAudioRef.current = audio;
            }
            return;
          }
        }

        // ---- LED frame identification (1-byte magic) ----
        if (firstByte === LED_MAGIC && buf.byteLength === LED_FRAME_SIZE) {
          const strip0 = parseLedFrame(buf);
          if (!strip0) return;

          // We need audio data to build a complete frame.
          const audio = latestAudioRef.current;
          if (!audio) return;

          const frame = buildFrame(strip0, audio);

          // Push into the ring buffer, capping at BUFFER_SIZE.
          const buffer = bufferRef.current;
          buffer.push(frame);
          if (buffer.length > BUFFER_SIZE) {
            buffer.splice(0, buffer.length - BUFFER_SIZE);
          }

          // Trigger a React re-render every other frame to reduce overhead.
          frameCountRef.current += 1;
          if (frameCountRef.current % 2 === 0) {
            setBuffer([...buffer]);
          }
        }
      });

      // ------ cleanup on unmount or host change ------
      return () => {
        // Attempt graceful unsubscribe before closing.
        if (ws.readyState === WebSocket.OPEN) {
          try {
            ws.send(JSON.stringify({ type: "ledStream.unsubscribe" }));
            ws.send(JSON.stringify({ type: "audio.unsubscribe" }));
          } catch (_) {
            // Socket may have closed between the readyState check and send;
            // swallow any error — we're tearing down anyway.
          }
        }
        ws.close();

        // Reset frame counter for the next connection.
        frameCountRef.current = 0;
      };
    }, [host, bufferRef, setBuffer]);
  }

  // Expose on window for non-module script access
  window.DiagnosticDataAdapter = { useDiagnosticStream };

})();
