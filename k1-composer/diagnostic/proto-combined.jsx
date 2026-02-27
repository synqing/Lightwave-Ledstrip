// ═══════════════════════════════════════════════════════════════════════════════
// K1 Composer — Combined Diagnostic Proto
// Merges Proto2 (LED History) + Proto3 (Temporal Scrub) into a single view.
//
// Loaded via <script type="text/babel"> in a standalone HTML page.
// React 18 + ReactDOM available globally via CDN.
// ═══════════════════════════════════════════════════════════════════════════════

const { useState, useEffect, useRef, useCallback } = React;

const BUFFER_SIZE = 360;
const NUM_LEDS = 80;

// ── Frame Generation (simulation fallback) ───────────────────────────────────

function generateFrame(T) {
  const bass   = 0.5 + 0.5 * Math.sin(T * 1.3) * Math.abs(Math.sin(T * 0.4));
  const mid    = 0.4 + 0.4 * Math.sin(T * 2.7 + 1.2);
  const treble = 0.3 + 0.3 * Math.sin(T * 5.1 + 2.4);
  const beatRaw = Math.sin(T * 2 * Math.PI * 1.8);
  const beat   = beatRaw > 0.85 ? 1 : 0;
  const conf   = 0.87 + Math.sin(T * 0.3) * 0.05;

  // Anomaly injection (periodic, based on modular time windows)
  const clipWindow = T % 7;
  const dimWindow  = T % 11;
  const isClipping = clipWindow > 5.8 && clipWindow < 6.0;
  const isDimming  = dimWindow > 9.2 && dimWindow < 9.5;
  const anomaly    = isClipping ? "clip" : isDimming ? "dim" : null;

  const leds = Array.from({ length: NUM_LEDS }, (_, i) => {
    const pos = i / NUM_LEDS;
    const specBin = Math.floor(pos * 32);
    const freqBand = specBin < 8 ? "bass" : specBin < 20 ? "mid" : "treble";

    // Stage 1: Audio input — raw spectral energy at this LED's frequency position
    const rawEnergy = freqBand === "bass" ? bass
      : freqBand === "mid" ? mid
      : treble;
    const audioInput = Math.max(0, Math.min(1, rawEnergy + (Math.random() - 0.5) * 0.04));

    // Stage 2: Spectrum mapping — gamma correction
    const gamma = 0.7;
    const spectrumOut = Math.pow(audioInput, gamma);

    // Stage 3: Beat modulation
    const beatMod = 1 + beat * 0.6 * Math.sin(pos * Math.PI);
    let beatOut = Math.min(1, spectrumOut * beatMod);

    // Apply anomaly distortions to the signal
    if (isClipping) beatOut = beatOut > 0.5 ? 1 : 0;
    if (isDimming)  beatOut *= 0.1;

    // Stage 4: Position mapping — spatial envelope
    const spatialEnv = 0.85 + 0.15 * Math.sin(pos * Math.PI * 2 + T * 0.5);
    const mappingOut = beatOut * spatialEnv;

    // Stage 5: Colour conversion
    const hue = pos * 280 + bass * 60;
    const sat = 85 + mid * 10;
    const lum = mappingOut * 55 + 3;

    // Stage 6: Output — post-FX vignette
    const vignette = 1 - Math.pow(Math.abs(pos - 0.5) * 1.4, 2) * 0.3;
    const finalLum = lum * vignette;

    // Issue detection
    const clipped = mappingOut > 0.98;
    const dimmed  = finalLum < 6 && audioInput > 0.3;

    return {
      index: i,
      pos,
      freqBand,
      specBin,
      stages: {
        audioInput: { value: audioInput,       label: "Audio Input",  unit: "",                        color: "#4af" },
        spectrum:   { value: spectrumOut,       label: "Spectrum",     unit: "g0.7",                    color: "#6cf" },
        beat:       { value: beatOut,           label: "Beat Mod",     unit: "x" + beatMod.toFixed(2),  color: "#fa6" },
        mapping:    { value: mappingOut,        label: "Position Map", unit: "env",                     color: "#af7" },
        color:      { value: mappingOut,        label: "Colour",       unit: "H" + Math.round(hue),     color: "#c8f", hsl: [hue, sat, lum] },
        output:     { value: finalLum / 60,     label: "LED Output",   unit: "vig",                     color: "#fff", hsl: [hue, sat, finalLum] },
      },
      clipped,
      dimmed,
      hsl: [hue, sat, finalLum],
    };
  });

  return { T, bass, mid, treble, beat, conf, anomaly, leds };
}

// ── StageRow — renders one stage in the signal chain ─────────────────────────

function StageRow({ name, data, prevValue, isLast }) {
  if (!data) return null;

  const delta = prevValue !== null ? data.value - prevValue : null;
  const deltaSign  = delta === null ? "" : delta > 0.01 ? "\u25B2" : delta < -0.01 ? "\u25BC" : "\u2248";
  const deltaColor = delta === null ? "#666" : delta > 0.01 ? "#6cf" : delta < -0.01 ? "#f88" : "#666";

  const barW = Math.round(Math.min(1, Math.max(0, data.value)) * 100);
  const isHSL = !!data.hsl;
  const displayColor = isHSL
    ? "hsl(" + data.hsl[0] + ", " + data.hsl[1] + "%, " + data.hsl[2] + "%)"
    : data.color;

  return React.createElement("div", {
    style: {
      padding: "10px 0",
      borderBottom: isLast ? "none" : "1px solid rgba(255,255,255,0.05)",
    },
  },
    // Label row: dot + name + delta + value + unit
    React.createElement("div", {
      style: { display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 6 },
    },
      React.createElement("div", { style: { display: "flex", alignItems: "center", gap: 8 } },
        React.createElement("div", {
          style: {
            width: 6, height: 6, borderRadius: "50%",
            background: data.color, boxShadow: "0 0 6px " + data.color,
          },
        }),
        React.createElement("span", {
          style: { fontSize: 10, color: "#aaa", fontFamily: "monospace" },
        }, name),
      ),
      React.createElement("div", { style: { display: "flex", gap: 8, alignItems: "center" } },
        delta !== null && React.createElement("span", {
          style: { fontSize: 9, color: deltaColor, fontFamily: "monospace" },
        }, deltaSign + " " + Math.abs(delta).toFixed(3)),
        React.createElement("span", {
          style: { fontSize: 10, fontFamily: "'JetBrains Mono', monospace", color: "#ddd" },
        }, data.value.toFixed(3)),
        data.unit && React.createElement("span", {
          style: { fontSize: 8, color: "#555", fontFamily: "monospace" },
        }, data.unit),
      ),
    ),
    // Progress bar
    React.createElement("div", {
      style: { height: 3, background: "rgba(255,255,255,0.05)", borderRadius: 2, overflow: "hidden" },
    },
      React.createElement("div", {
        style: {
          width: barW + "%", height: "100%",
          background: isHSL ? displayColor : data.color,
          borderRadius: 2, transition: "width 0.1s",
        },
      }),
    ),
    // Colour swatch for HSL stages
    isHSL && React.createElement("div", {
      style: {
        marginTop: 5, height: 8, borderRadius: 2,
        background: "linear-gradient(90deg, hsl(" + data.hsl[0] + ", " + data.hsl[1] + "%, 3%), " + displayColor + ")",
      },
    }),
  );
}

// ── Sparkline — SVG waveform with optional fill ──────────────────────────────

function Sparkline({ data, color, height, filled }) {
  if (!data || data.length < 2) return null;

  const h = height || 30;
  const w = 200;
  const max = Math.max.apply(null, data.concat([0.01]));
  const pts = data.map(function (v, i) {
    return (i / (data.length - 1)) * w + "," + (h - (v / max) * h);
  });
  const pathD = "M " + pts.join(" L ");
  const fillPath = pathD + " L " + w + "," + h + " L 0," + h + " Z";

  return React.createElement("svg", {
    width: "100%", height: h,
    viewBox: "0 0 " + w + " " + h,
    preserveAspectRatio: "none",
  },
    filled && React.createElement("path", {
      d: fillPath, fill: color, opacity: 0.12,
    }),
    React.createElement("path", {
      d: pathD, stroke: color, strokeWidth: 1.5, fill: "none", opacity: 0.8,
    }),
  );
}

// ── CombinedProto — main component ──────────────────────────────────────────

function CombinedProto() {
  const bufferRef = useRef([]);
  const tRef      = useRef(0);
  const rafRef    = useRef(null);

  const [buffer, setBuffer]         = useState([]);
  const [scrubPos, setScrubPos]     = useState(null);
  const [isLive, setIsLive]         = useState(true);
  const [selectedLED, setSelectedLED] = useState(null);
  const [frozenFrame, setFrozenFrame] = useState(null);
  const [host, setHost]             = useState("");
  const [mode, setMode]             = useState("sim"); // "sim" | "live"
  const [hoveredLED, setHoveredLED] = useState(null);

  // ── Simulation loop ──────────────────────────────────────────────────────

  useEffect(function () {
    if (mode !== "sim") return;

    var tick = function () {
      tRef.current += 0.016;
      var f = generateFrame(tRef.current);
      bufferRef.current.push(f);
      if (bufferRef.current.length > BUFFER_SIZE) {
        bufferRef.current.shift();
      }
      // Throttle React state updates to every other frame for performance
      if (bufferRef.current.length % 2 === 0) {
        setBuffer(bufferRef.current.slice());
      }
      rafRef.current = requestAnimationFrame(tick);
    };

    rafRef.current = requestAnimationFrame(tick);
    return function () {
      if (rafRef.current) cancelAnimationFrame(rafRef.current);
    };
  }, [mode]);

  // ── Live data stream via DiagnosticDataAdapter ─────────────────────────
  // Called unconditionally at top level (Rules of Hooks). The hook
  // internally no-ops when host is null/empty.
  var liveHost = mode === "live" && host ? host : null;
  window.DiagnosticDataAdapter.useDiagnosticStream(liveHost, bufferRef, setBuffer);

  // ── Connect / disconnect handler ─────────────────────────────────────────

  var handleConnect = useCallback(function () {
    if (mode === "live") {
      // Disconnect — return to simulation
      setMode("sim");
      bufferRef.current = [];
      setBuffer([]);
    } else if (host.trim()) {
      // Attempt live connection
      setMode("live");
      bufferRef.current = [];
      setBuffer([]);
    }
  }, [mode, host]);

  // ── Display frame resolution ─────────────────────────────────────────────

  var displayFrame = isLive
    ? buffer[buffer.length - 1]
    : buffer[Math.floor((scrubPos != null ? scrubPos : 1) * (buffer.length - 1))];

  var inspectFrame = frozenFrame != null ? frozenFrame : displayFrame;

  // ── LED click / clear handlers ───────────────────────────────────────────

  var handleLEDClick = useCallback(function (index) {
    setFrozenFrame(displayFrame || null);
    setSelectedLED(index);
  }, [displayFrame]);

  var handleClear = useCallback(function () {
    setFrozenFrame(null);
    setSelectedLED(null);
  }, []);

  // ── Scrubber handlers ────────────────────────────────────────────────────

  var handleSliderInput = useCallback(function (e) {
    var v = parseFloat(e.target.value);
    setScrubPos(v);
    setIsLive(v >= 0.995);
  }, []);

  var goLive = useCallback(function () {
    setIsLive(true);
    setScrubPos(null);
  }, []);

  // ── Derived data ─────────────────────────────────────────────────────────

  var sliderVal   = isLive ? 1 : (scrubPos != null ? scrubPos : 1);
  var bassHistory = buffer.map(function (f) { return f.bass; });
  var beatHistory = buffer.map(function (f) { return f.beat; });
  var anomalies   = buffer
    .map(function (f, i) { return { i: i, type: f.anomaly }; })
    .filter(function (a) { return a.type; });

  // Resolve the LED to inspect from the inspection frame
  var led = selectedLED !== null && inspectFrame
    ? inspectFrame.leds[selectedLED]
    : null;

  var stageEntries = led ? [
    ["Audio Input",  led.stages.audioInput,  null],
    ["Spectrum",     led.stages.spectrum,     led.stages.audioInput.value],
    ["Beat Mod",     led.stages.beat,         led.stages.spectrum.value],
    ["Position Map", led.stages.mapping,      led.stages.beat.value],
    ["Colour",       led.stages.color,        led.stages.mapping.value],
    ["LED Output",   led.stages.output,       led.stages.color.value],
  ] : [];

  // ── Status indicator logic ───────────────────────────────────────────────

  var indicatorColor = mode === "sim" ? "#f44"
    : isLive ? "#4af"
    : "#fa6";

  var modeLabel = mode === "sim" ? "SIMULATION"
    : isLive ? "LIVE"
    : "SCRUBBING";

  // ── Render ───────────────────────────────────────────────────────────────

  return React.createElement("div", {
    style: {
      minHeight: "100vh",
      background: "#080810",
      fontFamily: "'JetBrains Mono', monospace",
      padding: "28px 24px",
      display: "flex",
      flexDirection: "column",
      gap: 20,
      color: "#ccc",
    },
  },
    // Global styles
    React.createElement("style", null,
      "@import url('https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@300;400;500&display=swap');\n" +
      "@keyframes pulse { 0%,100%{opacity:1} 50%{opacity:0.4} }\n" +
      ".led-cell:hover { transform: scaleY(1.6); z-index: 2; cursor: crosshair; }\n" +
      ".led-cell { transition: transform 0.08s; transform-origin: center; }\n" +
      "input[type=range] { -webkit-appearance: none; width: 100%; height: 3px;\n" +
      "  background: rgba(255,255,255,0.1); border-radius: 2px; outline: none; cursor: pointer; }\n" +
      "input[type=range]::-webkit-slider-thumb { -webkit-appearance: none;\n" +
      "  width: 14px; height: 14px; border-radius: 50%;\n" +
      "  background: #fff; border: 2px solid #333; cursor: pointer; }\n" +
      "input[type=range]::-webkit-slider-thumb:hover { background: #4af; }\n"
    ),

    // ── Header ─────────────────────────────────────────────────────────────
    React.createElement("div", null,
      React.createElement("div", {
        style: { display: "flex", alignItems: "center", gap: 12, marginBottom: 6, flexWrap: "wrap" },
      },
        // Host input
        React.createElement("input", {
          type: "text",
          placeholder: "192.168.1.100",
          value: host,
          onChange: function (e) { setHost(e.target.value); },
          style: {
            width: 140, padding: "4px 8px", borderRadius: 4,
            background: "rgba(255,255,255,0.06)", border: "1px solid rgba(255,255,255,0.1)",
            color: "#ccc", fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
            outline: "none",
          },
        }),
        // Connect / Disconnect button
        React.createElement("button", {
          onClick: handleConnect,
          style: {
            padding: "4px 10px", borderRadius: 4, border: "none",
            background: mode === "live" ? "rgba(255,80,80,0.15)" : "rgba(74,175,255,0.15)",
            color: mode === "live" ? "#f88" : "#4af",
            fontFamily: "monospace", fontSize: 9, cursor: "pointer", letterSpacing: "0.1em",
          },
        }, mode === "live" ? "DISCONNECT" : "CONNECT"),
        // Live indicator dot
        React.createElement("div", {
          style: {
            width: 8, height: 8, borderRadius: "50%",
            background: indicatorColor,
            boxShadow: "0 0 8px " + indicatorColor,
            animation: isLive && mode === "live" ? "pulse 1s ease-in-out infinite alternate" : "none",
          },
        }),
        // Mode label
        React.createElement("span", {
          style: { fontSize: 11, color: indicatorColor, letterSpacing: "0.15em" },
        }, "K1 \u00B7 COMPOSER \u00B7 " + modeLabel),
        // "SIMULATED" badge when in sim mode
        mode === "sim" && React.createElement("span", {
          style: {
            fontSize: 8, color: "#f44",
            background: "rgba(255,60,60,0.1)",
            padding: "2px 6px", borderRadius: 3, letterSpacing: "0.1em",
          },
        }, "SIMULATED"),
        // GO LIVE button (visible when scrubbing)
        !isLive && React.createElement("button", {
          onClick: goLive,
          style: {
            marginLeft: "auto", padding: "3px 10px", borderRadius: 4, border: "none",
            background: "rgba(74,175,255,0.15)", color: "#4af",
            fontFamily: "monospace", fontSize: 9, cursor: "pointer", letterSpacing: "0.1em",
          },
        }, "\u25CF GO LIVE"),
      ),
      React.createElement("div", { style: { height: 1, background: "rgba(255,255,255,0.06)" } }),
    ),

    // ── Two-column layout ──────────────────────────────────────────────────
    React.createElement("div", {
      style: { display: "flex", gap: 20, flex: 1 },
    },

      // ── LEFT column ────────────────────────────────────────────────────
      React.createElement("div", { style: { flex: 1, display: "flex", flexDirection: "column", gap: 16 } },

        // Strip label + anomaly badge
        React.createElement("div", {
          style: { fontSize: 9, color: "#444", letterSpacing: "0.1em" },
        },
          selectedLED !== null
            ? "LED #" + selectedLED + " SELECTED \u00B7 CLICK ANOTHER OR CLEAR TO RESUME"
            : "MASTER OUTPUT \u00B7 CLICK ANY LED TO TRACE ITS SIGNAL PATH",
          // Anomaly badge on current display frame
          displayFrame && displayFrame.anomaly && React.createElement("span", {
            style: {
              marginLeft: 10,
              color: displayFrame.anomaly === "clip" ? "#f44" : "#fa6",
              background: displayFrame.anomaly === "clip" ? "rgba(255,60,60,0.1)" : "rgba(255,160,0,0.1)",
              padding: "1px 6px", borderRadius: 3, fontSize: 8,
            },
          }, "\u26A0 " + (displayFrame.anomaly === "clip" ? "CLIPPING" : "DIM ANOMALY")),
        ),

        // Interactive LED strip (80 LEDs)
        React.createElement("div", {
          style: {
            display: "flex", height: 40, gap: 1,
            background: "rgba(0,0,0,0.4)",
            borderRadius: 6, overflow: "hidden",
            padding: "6px 4px",
            border: "1px solid rgba(255,255,255,0.07)",
          },
        },
          ((inspectFrame ? inspectFrame.leds : null) || Array(NUM_LEDS).fill(null)).map(function (ledData, i) {
            var isSelected = selectedLED === i;
            var isHovered  = hoveredLED === i;
            var colour = ledData
              ? "hsl(" + ledData.hsl[0] + ", " + ledData.hsl[1] + "%, " + ledData.hsl[2] + "%)"
              : "#111";
            var hasIssue = ledData && (ledData.clipped || ledData.dimmed);

            return React.createElement("div", {
              key: i,
              className: "led-cell",
              onClick: function () { handleLEDClick(i); },
              onMouseEnter: function () { setHoveredLED(i); },
              onMouseLeave: function () { setHoveredLED(null); },
              style: {
                flex: 1,
                background: isSelected ? "#fff" : colour,
                borderRadius: 1,
                border: isSelected ? "none" : hasIssue ? "1px solid rgba(255,100,100,0.5)" : "none",
                boxShadow: isSelected ? "0 0 8px rgba(255,255,255,0.8)" : "none",
                position: "relative",
              },
            });
          }),
        ),

        // Issue dots row
        React.createElement("div", {
          style: { display: "flex", height: 6, gap: 1, padding: "0 4px" },
        },
          ((inspectFrame ? inspectFrame.leds : null) || Array(NUM_LEDS).fill(null)).map(function (ledData, i) {
            return React.createElement("div", {
              key: i,
              style: {
                flex: 1,
                background: ledData && ledData.clipped ? "#f44" : ledData && ledData.dimmed ? "#fa0" : "transparent",
                borderRadius: 1,
                opacity: 0.7,
              },
            });
          }),
        ),

        // Stats row (bass / beat / confidence)
        React.createElement("div", {
          style: { display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: 10 },
        },
          [
            { label: "Bass Energy", value: displayFrame ? displayFrame.bass.toFixed(3) : "\u2014", color: "#4af" },
            { label: "Beat",        value: displayFrame && displayFrame.beat > 0 ? "HIT" : "\u2014", color: "#fa6" },
            { label: "Confidence",  value: displayFrame ? displayFrame.conf.toFixed(3) : "\u2014", color: "#c8f" },
          ].map(function (s, i) {
            return React.createElement("div", {
              key: i,
              style: {
                background: "rgba(255,255,255,0.03)",
                border: "1px solid rgba(255,255,255,0.07)",
                borderRadius: 8, padding: "8px 12px",
              },
            },
              React.createElement("div", { style: { fontSize: 8, color: "#555", marginBottom: 2 } }, s.label),
              React.createElement("div", { style: { fontSize: 14, color: s.color } }, s.value),
            );
          }),
        ),

        // ── Timeline area ────────────────────────────────────────────────
        React.createElement("div", {
          style: {
            background: "rgba(0,0,0,0.3)",
            border: "1px solid rgba(255,255,255,0.07)",
            borderRadius: 10,
            padding: 16,
            flex: 1,
          },
        },
          React.createElement("div", {
            style: { fontSize: 9, color: "#555", marginBottom: 12, letterSpacing: "0.1em" },
          }, "TIMELINE \u00B7 " + buffer.length + " FRAMES BUFFERED \u00B7 " + (buffer.length / 60).toFixed(1) + "s"),

          // Bass sparkline + beat markers + anomaly markers
          React.createElement("div", {
            style: { position: "relative", marginBottom: 4 },
          },
            React.createElement(Sparkline, { data: bassHistory, color: "#4af", height: 40, filled: true }),

            // Beat markers (thin amber vertical lines)
            React.createElement("div", {
              style: { position: "absolute", inset: 0, pointerEvents: "none" },
            },
              beatHistory.map(function (b, i) {
                if (b <= 0) return null;
                return React.createElement("div", {
                  key: i,
                  style: {
                    position: "absolute",
                    left: (i / Math.max(1, buffer.length)) * 100 + "%",
                    top: 0, bottom: 0, width: 1,
                    background: "rgba(255,160,60,0.3)",
                  },
                });
              }),
            ),

            // Anomaly markers (clickable — jump to that frame)
            anomalies.map(function (a, idx) {
              return React.createElement("div", {
                key: "anom-" + idx,
                title: a.type === "clip" ? "Clipping" : "Dim anomaly",
                onClick: function () {
                  setScrubPos(a.i / Math.max(1, buffer.length));
                  setIsLive(false);
                },
                style: {
                  position: "absolute",
                  left: (a.i / Math.max(1, buffer.length)) * 100 + "%",
                  top: 0, bottom: 0, width: 2,
                  background: a.type === "clip" ? "rgba(255,60,60,0.6)" : "rgba(255,160,0,0.5)",
                  cursor: "pointer",
                },
              });
            }),

            // Playhead
            React.createElement("div", {
              style: {
                position: "absolute",
                left: sliderVal * 100 + "%",
                top: -2, bottom: -2, width: 2,
                background: isLive ? "#4af" : "#fff",
                boxShadow: "0 0 6px " + (isLive ? "#4af" : "#fff"),
                pointerEvents: "none",
              },
            }),
          ),

          // LED heatmap strip (average hue/luminance per frame over time)
          React.createElement("div", {
            style: { marginBottom: 12, position: "relative" },
          },
            React.createElement("div", {
              style: { fontSize: 8, color: "#444", marginBottom: 4 },
            }, "LED OUTPUT OVER TIME"),
            React.createElement("div", {
              style: {
                height: 24, display: "flex", borderRadius: 3, overflow: "hidden",
                border: "1px solid rgba(255,255,255,0.05)",
              },
            },
              buffer.filter(function (_, i) { return i % 3 === 0; }).map(function (f, i) {
                var avgHue = f.leds.reduce(function (acc, l) { return acc + l.hsl[0]; }, 0) / f.leds.length;
                var avgLum = f.leds.reduce(function (acc, l) { return acc + l.hsl[2]; }, 0) / f.leds.length;
                return React.createElement("div", {
                  key: i,
                  style: { flex: 1, background: "hsl(" + avgHue + ", 80%, " + avgLum + "%)" },
                });
              }),
            ),
            // Playhead overlay on heatmap
            React.createElement("div", {
              style: {
                position: "absolute",
                left: sliderVal * 100 + "%",
                top: 0, bottom: 0, width: 2,
                background: "rgba(255,255,255,0.7)",
                pointerEvents: "none",
                transform: "translateX(-50%)",
              },
            }),
          ),

          // Scrub slider
          React.createElement("div", { style: { position: "relative" } },
            React.createElement("input", {
              type: "range",
              min: 0, max: 1, step: 0.001,
              value: sliderVal,
              onChange: handleSliderInput,
              style: { width: "100%" },
            }),
            React.createElement("div", {
              style: {
                display: "flex", justifyContent: "space-between",
                fontSize: 8, color: "#444", marginTop: 4,
              },
            },
              React.createElement("span", null, "\u2190 " + (buffer.length / 60).toFixed(1) + "s ago"),
              React.createElement("span", null, "NOW \u2192"),
            ),
          ),

          // Anomaly legend
          React.createElement("div", {
            style: { marginTop: 10, display: "flex", gap: 16, fontSize: 8, color: "#555" },
          },
            React.createElement("span", null,
              React.createElement("span", { style: { color: "#f44" } }, "\u25A0"),
              " Clipping \u2014 click to jump",
            ),
            React.createElement("span", null,
              React.createElement("span", { style: { color: "#fa6" } }, "\u25A0"),
              " Dim anomaly \u2014 click to jump",
            ),
            React.createElement("span", null,
              React.createElement("span", { style: { color: "rgba(255,160,60,0.5)" } }, "|"),
              " Beat hit",
            ),
          ),
        ),

        // CLEAR button (visible when an LED is frozen/selected)
        frozenFrame && React.createElement("button", {
          onClick: handleClear,
          style: {
            padding: "8px 16px", borderRadius: 6,
            background: "rgba(255,255,255,0.06)", border: "1px solid rgba(255,255,255,0.12)",
            color: "#888", fontFamily: "monospace", fontSize: 10, cursor: "pointer",
            letterSpacing: "0.1em",
          },
        }, "\u2715 CLEAR SELECTION \u00B7 RESUME LIVE"),
      ),

      // ── RIGHT column — LED History Panel (280px) ───────────────────────
      React.createElement("div", {
        style: {
          width: 280,
          background: selectedLED !== null ? "rgba(255,255,255,0.02)" : "rgba(0,0,0,0.2)",
          border: "1px solid " + (selectedLED !== null ? "rgba(255,255,255,0.1)" : "rgba(255,255,255,0.04)"),
          borderRadius: 12,
          padding: selectedLED !== null ? 16 : 0,
          transition: "all 0.3s",
          overflow: "hidden",
          flexShrink: 0,
        },
      },
        selectedLED === null
          // Empty state
          ? React.createElement("div", {
              style: {
                height: "100%", display: "flex", alignItems: "center",
                justifyContent: "center", color: "#333", fontSize: 10,
                textAlign: "center", padding: 20,
              },
            }, "Click any LED to inspect its signal chain")
          // Populated state
          : led ? React.createElement(React.Fragment, null,
              // LED header
              React.createElement("div", { style: { marginBottom: 14 } },
                React.createElement("div", {
                  style: { fontSize: 9, color: "#555", marginBottom: 4 },
                }, "LED HISTORY"),
                React.createElement("div", {
                  style: { display: "flex", alignItems: "baseline", gap: 8 },
                },
                  React.createElement("span", {
                    style: { fontSize: 18, color: "#ddd" },
                  }, "#" + selectedLED),
                  React.createElement("span", {
                    style: { fontSize: 9, color: "#555" },
                  }, "pos " + led.pos.toFixed(3) + " \u00B7 " + led.freqBand + " \u00B7 bin " + led.specBin),
                ),
                // Final colour preview swatch with glow
                React.createElement("div", {
                  style: {
                    marginTop: 10, height: 20, borderRadius: 4,
                    background: "hsl(" + led.hsl[0] + ", " + led.hsl[1] + "%, " + led.hsl[2] + "%)",
                    boxShadow: "0 0 12px hsl(" + led.hsl[0] + ", " + led.hsl[1] + "%, " + led.hsl[2] + "%)",
                  },
                }),
                // Issue badges
                (led.clipped || led.dimmed) && React.createElement("div", {
                  style: { display: "flex", gap: 6, marginTop: 8 },
                },
                  led.clipped && React.createElement("span", {
                    style: {
                      fontSize: 8, color: "#f88",
                      background: "rgba(255,80,80,0.1)",
                      padding: "2px 6px", borderRadius: 3,
                    },
                  }, "\u26A0 CLIPPING"),
                  led.dimmed && React.createElement("span", {
                    style: {
                      fontSize: 8, color: "#fa6",
                      background: "rgba(255,160,0,0.1)",
                      padding: "2px 6px", borderRadius: 3,
                    },
                  }, "\u26A0 UNEXPECTED DIM"),
                ),
              ),
              // Divider
              React.createElement("div", {
                style: { height: 1, background: "rgba(255,255,255,0.06)", marginBottom: 2 },
              }),
              // Stage chain (6 stages)
              stageEntries.map(function (entry, i) {
                return React.createElement(StageRow, {
                  key: entry[0],
                  name: entry[0],
                  data: entry[1],
                  prevValue: entry[2],
                  isLast: i === stageEntries.length - 1,
                });
              }),
            )
          : null,
      ),
    ),

    // Footer
    React.createElement("div", {
      style: { fontSize: 9, color: "#2a2a3a", letterSpacing: "0.08em" },
    }, "COMBINED DIAGNOSTIC \u00B7 LED HISTORY + TEMPORAL SCRUB \u00B7 TAP LED TO TRACE \u00B7 DRAG SLIDER TO REWIND"),
  );
}

// ── Mount ────────────────────────────────────────────────────────────────────

var root = ReactDOM.createRoot(document.getElementById("diagnostic-root"));
root.render(React.createElement(CombinedProto));
