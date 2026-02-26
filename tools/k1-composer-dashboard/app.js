const ui = {
  hostInput: document.getElementById("hostInput"),
  connectBtn: document.getElementById("connectBtn"),
  acquireBtn: document.getElementById("acquireBtn"),
  releaseBtn: document.getElementById("releaseBtn"),
  reacquireBtn: document.getElementById("reacquireBtn"),
  statusBtn: document.getElementById("statusBtn"),
  emergencyBtn: document.getElementById("emergencyBtn"),
  connectBadge: document.getElementById("connectBadge"),
  leaseBadge: document.getElementById("leaseBadge"),
  streamBadge: document.getElementById("streamBadge"),
  ownerBadge: document.getElementById("ownerBadge"),
  leaseId: document.getElementById("leaseId"),
  leaseTtl: document.getElementById("leaseTtl"),
  hbState: document.getElementById("hbState"),
  streamMeta: document.getElementById("streamMeta"),
  streamQueue: document.getElementById("streamQueue"),
  timeline: document.getElementById("timeline"),
  playBtn: document.getElementById("playBtn"),
  pauseBtn: document.getElementById("pauseBtn"),
  resetBtn: document.getElementById("resetBtn"),
  speedButtons: Array.from(document.querySelectorAll(".speed-group button")),
  leaseStateBlock: document.getElementById("leaseStateBlock"),
  streamStateBlock: document.getElementById("streamStateBlock"),
  stripA: document.getElementById("stripA"),
  stripB: document.getElementById("stripB"),
  algoFlow: document.getElementById("algoFlow"),
  effectSelect: document.getElementById("effectSelect"),
  mappingBadge: document.getElementById("mappingBadge"),
  effectNameLabel: document.getElementById("effectNameLabel"),
  sourcePathLabel: document.getElementById("sourcePathLabel"),
  phaseLabel: document.getElementById("phaseLabel"),
  codeVisualiser: document.getElementById("codeVisualiser"),
  codeVars: document.getElementById("codeVars"),
  codeTrace: document.getElementById("codeTrace"),
  tunerGrid: document.getElementById("tunerGrid"),
  diagMetrics: document.getElementById("diagMetrics"),
  queueSpark: document.getElementById("queueSpark"),
  cadenceSpark: document.getElementById("cadenceSpark"),
  dropSpark: document.getElementById("dropSpark"),
  telemetryLog: document.getElementById("telemetryLog"),
  toast: document.getElementById("toast"),
};

const LEDS_PER_STRIP = 160;
const STRIPS = 2;
const TOTAL_LEDS = LEDS_PER_STRIP * STRIPS;
const BYTES_PER_LED = 3;
const FRAME_PAYLOAD_BYTES = TOTAL_LEDS * BYTES_PER_LED;
const FRAME_HEADER_BYTES = 16;
const FRAME_TOTAL_BYTES = FRAME_HEADER_BYTES + FRAME_PAYLOAD_BYTES;
const CATALOG_GENERATED_INDEX_URL = "./src/code-catalog/generated/index.json";
const CATALOG_SEED_URL = "./src/code-catalog/effects.json";
const PHASE_ORDER = ["input", "mapping", "modulation", "render", "post", "output"];

const SENSITIVE_TELEMETRY_KEYS = new Set([
  "leasetoken",
  "x-control-lease",
  "x-control-lease-id",
  "authorization",
  "apikey",
]);

const paramConfig = [
  { key: "speed", label: "Speed", min: 1, max: 50, value: 20 },
  { key: "intensity", label: "Intensity", min: 0, max: 255, value: 170 },
  { key: "saturation", label: "Saturation", min: 0, max: 255, value: 180 },
  { key: "complexity", label: "Complexity", min: 0, max: 255, value: 160 },
  { key: "variation", label: "Variation", min: 0, max: 255, value: 100 },
  { key: "brightness", label: "Brightness", min: 0, max: 255, value: 180 },
];

const state = {
  host: ui.hostInput.value.trim(),
  ws: null,
  connected: false,
  clientName: "K1 Composer Dashboard",
  clientInstanceId: crypto.randomUUID(),
  lease: {
    active: false,
    leaseId: "",
    leaseToken: "",
    scope: "global",
    ownerWsClientId: 0,
    ownerClientName: "",
    ownerInstanceId: "",
    remainingMs: 0,
    ttlMs: 5000,
    heartbeatIntervalMs: 1000,
    takeoverAllowed: false,
  },
  stream: {
    active: false,
    sessionId: "",
    ownerWsClientId: 0,
    targetFps: 120,
    staleTimeoutMs: 750,
    frameContractVersion: 1,
    pixelFormat: "rgb888",
    ledCount: TOTAL_LEDS,
    headerBytes: FRAME_HEADER_BYTES,
    payloadBytes: FRAME_PAYLOAD_BYTES,
    maxPayloadBytes: FRAME_PAYLOAD_BYTES,
    mailboxDepth: 2,
    lastFrameSeq: 0,
    lastFrameRxMs: 0,
    framesRx: 0,
    framesRendered: 0,
    framesDroppedMailbox: 0,
    framesInvalid: 0,
    framesBlockedLease: 0,
    staleTimeouts: 0,
    framesSent: 0,
    framesDroppedBackpressure: 0,
    queueDropThreshold: 32768,
    lastBackpressureLogMs: 0,
    stateLabel: "idle",
  },
  statusPollTimer: null,
  heartbeatTimer: null,
  leaseCountdownTimer: null,
  pending: new Map(),
  reqSeq: 0,
  eventBuffer: [],
  traceBuffer: [],
  catalog: {
    loaded: false,
    schemaVersion: "",
    generatedAtUtc: "",
    firmwareSourceHash: "",
    byEffectId: new Map(),
    byEffectHex: new Map(),
    currentEffectId: "",
    currentEffectData: null,
  },
  diagnostics: {
    sampleBuffer: [],
    historyBuffer: [],
    maxSamples: 12000,
    maxHistory: 1440,
    timer: null,
    aggregationMs: 250,
    windowShortMs: 10000,
    windowLongMs: 60000,
    lastEmitMs: 0,
    lastSentTs: 0,
    latest: {
      queueNow: 0,
      queueP95: 0,
      queueMax: 0,
      sendCadenceHz: 0,
      sendJitterMs: 0,
      dropRatio10s: 0,
      dropRatio60s: 0,
      attempted10s: 0,
      sent10s: 0,
      dropped10s: 0,
      attempted60s: 0,
      sent60s: 0,
      dropped60s: 0,
    },
  },
  transport: {
    playing: false,
    speed: 1,
    timeline: 0,
    raf: null,
    lastTs: 0,
    lastFrameSentMs: 0,
    seq: 0,
  },
  sim: {
    params: Object.fromEntries(paramConfig.map((p) => [p.key, p.value])),
    wasmReady: false,
    lastTraceMs: 0,
  },
};

function nowMs() {
  return Math.floor(performance.now());
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function percentile(values, p) {
  if (!values.length) return 0;
  const sorted = [...values].sort((a, b) => a - b);
  const idx = clamp(Math.floor((sorted.length - 1) * p), 0, sorted.length - 1);
  return sorted[idx];
}

function stddev(values) {
  if (!values.length) return 0;
  const mean = values.reduce((acc, val) => acc + val, 0) / values.length;
  const variance = values.reduce((acc, val) => acc + ((val - mean) * (val - mean)), 0) / values.length;
  return Math.sqrt(variance);
}

function lineInRanges(line, ranges) {
  if (!Array.isArray(ranges)) return false;
  for (const pair of ranges) {
    if (!Array.isArray(pair) || pair.length !== 2) continue;
    const start = Number(pair[0]) || 0;
    const end = Number(pair[1]) || 0;
    if (line >= start && line <= end) return true;
  }
  return false;
}

function makeReqId(prefix = "req") {
  state.reqSeq += 1;
  return `${prefix}-${state.reqSeq}-${Date.now()}`;
}

function toast(message) {
  ui.toast.textContent = message;
  ui.toast.classList.add("show");
  window.clearTimeout(toast.timer);
  toast.timer = window.setTimeout(() => ui.toast.classList.remove("show"), 2200);
}

function sanitiseTelemetry(value) {
  if (Array.isArray(value)) {
    return value.map((item) => sanitiseTelemetry(item));
  }

  if (value && typeof value === "object") {
    const out = {};
    Object.entries(value).forEach(([key, nested]) => {
      if (SENSITIVE_TELEMETRY_KEYS.has(String(key).toLowerCase())) {
        out[key] = "[REDACTED]";
      } else {
        out[key] = sanitiseTelemetry(nested);
      }
    });
    return out;
  }

  return value;
}

function logEvent(event, payload = {}) {
  const safePayload = sanitiseTelemetry(payload);
  const line = `${new Date().toISOString()} ${event} ${JSON.stringify(safePayload)}`;
  state.eventBuffer.unshift(line);
  state.eventBuffer = state.eventBuffer.slice(0, 160);
  ui.telemetryLog.textContent = state.eventBuffer.join("\n");
}

function normaliseLegacyPhaseMap(phaseLineMap = {}) {
  const out = {};
  PHASE_ORDER.forEach((phase) => {
    const lineList = Array.isArray(phaseLineMap[phase]) ? phaseLineMap[phase] : [];
    out[phase] = lineList
      .map((line) => Number(line))
      .filter((line) => Number.isFinite(line) && line > 0)
      .map((line) => [line, line]);
  });
  return out;
}

function normalisePhaseRanges(phaseRanges = {}, renderRange = [1, 1]) {
  const out = {};
  PHASE_ORDER.forEach((phase) => {
    const ranges = Array.isArray(phaseRanges[phase]) ? phaseRanges[phase] : [];
    const normalised = [];
    ranges.forEach((pair) => {
      if (!Array.isArray(pair) || pair.length !== 2) return;
      const start = Number(pair[0]) || 0;
      const end = Number(pair[1]) || 0;
      if (start > 0 && end >= start) {
        normalised.push([start, end]);
      }
    });
    out[phase] = normalised;
  });

  const hasAny = PHASE_ORDER.some((phase) => out[phase].length > 0);
  if (!hasAny) {
    const [startLine, endLine] = renderRange;
    const span = Math.max(1, endLine - startLine + 1);
    const split = {
      input: [0.0, 0.12],
      mapping: [0.12, 0.28],
      modulation: [0.28, 0.46],
      render: [0.46, 0.88],
      post: [0.88, 0.95],
      output: [0.95, 1.0],
    };
    PHASE_ORDER.forEach((phase) => {
      const [startPct, endPct] = split[phase];
      const begin = startLine + Math.floor(span * startPct);
      let end = startLine + Math.floor(span * endPct) - 1;
      if (phase === "output") end = endLine;
      out[phase] = [[Math.max(startLine, begin), Math.min(endLine, Math.max(begin, end))]];
    });
  }

  return out;
}

function buildFallbackSource(displayName = "UnknownEffect") {
  return [
    `// ${displayName}`,
    "// Generated source is unavailable for this entry.",
    "// Regenerate catalogue with scripts/generate_effect_catalog.py.",
    "void render(plugins::EffectContext& ctx) {",
    "  (void)ctx;",
    "}",
  ].join("\n");
}

function hydrateEffectMeta(raw = {}, sourceTextOverride = null) {
  const effectId = String(raw.effectId || "").trim();
  const renderRangeRaw = Array.isArray(raw.renderRange) ? raw.renderRange : [1, 1];
  const renderRange = [
    Math.max(1, Number(renderRangeRaw[0]) || 1),
    Math.max(1, Number(renderRangeRaw[1]) || 1),
  ];
  if (renderRange[1] < renderRange[0]) renderRange[1] = renderRange[0];

  const sourceText = sourceTextOverride ?? raw.sourceText ?? buildFallbackSource(raw.displayName);
  const sourceLines = String(sourceText).split("\n");
  const clampedRenderRange = [
    clamp(renderRange[0], 1, Math.max(1, sourceLines.length)),
    clamp(renderRange[1], 1, Math.max(1, sourceLines.length)),
  ];
  if (clampedRenderRange[1] < clampedRenderRange[0]) {
    clampedRenderRange[1] = clampedRenderRange[0];
  }

  return {
    schemaVersion: String(raw.schemaVersion || "1.0.0"),
    generatedAtUtc: raw.generatedAtUtc || "",
    firmwareSourceHash: raw.firmwareSourceHash || "",
    effectId,
    effectIdHex: String(raw.effectIdHex || "0x0000").toUpperCase(),
    className: raw.className || effectId || "UnknownClass",
    displayName: raw.displayName || raw.className || effectId || "Unknown Effect",
    headerPath: raw.headerPath || "",
    sourcePath: raw.sourcePath || "",
    renderRange: clampedRenderRange,
    phaseRanges: normalisePhaseRanges(raw.phaseRanges, clampedRenderRange),
    mappingConfidence: raw.mappingConfidence || "low",
    mappingWarnings: Array.isArray(raw.mappingWarnings) ? raw.mappingWarnings : [],
    sourceText: sourceLines.join("\n"),
    artifactPath: raw.artifactPath || "",
  };
}

async function loadEffectCatalogue() {
  const tryGenerated = async () => {
    const response = await fetch(CATALOG_GENERATED_INDEX_URL, { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`Generated catalogue unavailable (${response.status})`);
    }
    const index = await response.json();
    if (!Array.isArray(index.effects)) {
      throw new Error("Generated catalogue has no effects array");
    }

    state.catalog.loaded = true;
    state.catalog.schemaVersion = String(index.schemaVersion || "");
    state.catalog.generatedAtUtc = index.generatedAtUtc || "";
    state.catalog.firmwareSourceHash = index.firmwareSourceHash || "";
    state.catalog.byEffectId.clear();
    state.catalog.byEffectHex.clear();

    index.effects.forEach((entry) => {
      const hydrated = hydrateEffectMeta(entry);
      state.catalog.byEffectId.set(hydrated.effectId, hydrated);
      state.catalog.byEffectHex.set(hydrated.effectIdHex.toUpperCase(), hydrated.effectId);
    });
  };

  const trySeedFallback = async () => {
    const response = await fetch(CATALOG_SEED_URL, { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`Seed catalogue unavailable (${response.status})`);
    }
    const seed = await response.json();
    if (!Array.isArray(seed.effects)) {
      throw new Error("Seed catalogue has no effects array");
    }

    state.catalog.loaded = true;
    state.catalog.schemaVersion = String(seed.schemaVersion || "1.0.0");
    state.catalog.generatedAtUtc = "";
    state.catalog.firmwareSourceHash = "";
    state.catalog.byEffectId.clear();
    state.catalog.byEffectHex.clear();

    seed.effects.forEach((entry) => {
      const legacy = {
        ...entry,
        effectIdHex: entry.effectIdHex || "0x0000",
        renderRange: [1, Math.max(2, Object.keys(entry.phaseLineMap || {}).length + 6)],
        phaseRanges: normaliseLegacyPhaseMap(entry.phaseLineMap || {}),
        mappingConfidence: "low",
        mappingWarnings: ["Loaded from seed catalogue fallback; source text unavailable."],
      };
      const hydrated = hydrateEffectMeta(legacy, buildFallbackSource(entry.displayName || entry.effectId));
      state.catalog.byEffectId.set(hydrated.effectId, hydrated);
      state.catalog.byEffectHex.set(hydrated.effectIdHex.toUpperCase(), hydrated.effectId);
    });
  };

  try {
    await tryGenerated();
    logEvent("catalogue.loaded", {
      source: "generated",
      schemaVersion: state.catalog.schemaVersion,
      effectCount: state.catalog.byEffectId.size,
      generatedAtUtc: state.catalog.generatedAtUtc,
    });
    return;
  } catch (error) {
    logEvent("catalogue.generated_unavailable", { message: error.message });
  }

  await trySeedFallback();
  logEvent("catalogue.loaded", {
    source: "seed_fallback",
    schemaVersion: state.catalog.schemaVersion,
    effectCount: state.catalog.byEffectId.size,
  });
}

function listCatalogueEffects() {
  return Array.from(state.catalog.byEffectId.values()).sort((a, b) => {
    const ah = Number.parseInt(String(a.effectIdHex).replace("0X", "").replace("0x", ""), 16);
    const bh = Number.parseInt(String(b.effectIdHex).replace("0X", "").replace("0x", ""), 16);
    if (Number.isFinite(ah) && Number.isFinite(bh) && ah !== bh) return ah - bh;
    return a.displayName.localeCompare(b.displayName);
  });
}

async function setCurrentEffect(effectId) {
  if (!effectId || !state.catalog.byEffectId.has(effectId)) return;

  const base = state.catalog.byEffectId.get(effectId);
  let hydrated = base;

  if (!base.sourceText || base.sourceText.trim() === "" || base.artifactPath) {
    if (base.artifactPath) {
      try {
        const response = await fetch(`./src/code-catalog/generated/${base.artifactPath}`, { cache: "no-store" });
        if (response.ok) {
          const payload = await response.json();
          hydrated = hydrateEffectMeta(payload);
          state.catalog.byEffectId.set(hydrated.effectId, hydrated);
        }
      } catch (error) {
        logEvent("catalogue.effect_load_failed", {
          effectId,
          message: error.message,
        });
      }
    }
  }

  state.catalog.currentEffectId = hydrated.effectId;
  state.catalog.currentEffectData = hydrated;
  ui.effectSelect.value = hydrated.effectId;
  createCodeVisualiser();
  highlightAlgorithmPhase(derivePhase());
}

function renderMappingBadge(effect) {
  if (!effect) {
    ui.mappingBadge.className = "badge muted";
    ui.mappingBadge.textContent = "mapping: n/a";
    ui.mappingBadge.title = "";
    return;
  }
  const confidence = String(effect.mappingConfidence || "low").toLowerCase();
  const label = `mapping: ${confidence}`;
  if (confidence === "high") {
    ui.mappingBadge.className = "badge exclusive";
    ui.mappingBadge.textContent = label;
    ui.mappingBadge.title = "";
    return;
  }
  if (confidence === "medium") {
    ui.mappingBadge.className = "badge locked";
    ui.mappingBadge.textContent = label;
    ui.mappingBadge.title = "";
    return;
  }
  ui.mappingBadge.className = "badge muted";
  ui.mappingBadge.textContent = label;
  ui.mappingBadge.title = Array.isArray(effect.mappingWarnings)
    ? effect.mappingWarnings.join(" | ")
    : "";
}

function populateEffectSelect() {
  const effects = listCatalogueEffects();
  ui.effectSelect.innerHTML = "";
  effects.forEach((effect) => {
    const option = document.createElement("option");
    option.value = effect.effectId;
    option.textContent = `${effect.effectIdHex} ${effect.displayName}`;
    ui.effectSelect.appendChild(option);
  });
}

function setConnectionBadge(connected) {
  ui.connectBadge.textContent = connected ? "Connected" : "Disconnected";
  ui.connectBadge.className = `badge ${connected ? "online" : "offline"}`;
}

function setLeaseModeBadge(mode) {
  ui.leaseBadge.className = `badge ${mode}`;
  if (mode === "exclusive") {
    ui.leaseBadge.textContent = "Exclusive";
  } else if (mode === "locked") {
    ui.leaseBadge.textContent = "Locked";
  } else {
    ui.leaseBadge.textContent = "Observe";
  }
}

function setStreamBadge(mode) {
  if (mode === "active") {
    ui.streamBadge.className = "badge exclusive";
    ui.streamBadge.textContent = "Stream Live";
    return;
  }
  if (mode === "error") {
    ui.streamBadge.className = "badge locked";
    ui.streamBadge.textContent = "Stream Lost";
    return;
  }
  ui.streamBadge.className = "badge muted";
  ui.streamBadge.textContent = "Stream Idle";
}

function canMutate() {
  return state.connected && state.lease.active && state.lease.ownerInstanceId === state.clientInstanceId;
}

function updateControls() {
  const exclusive = canMutate();
  ui.acquireBtn.disabled = !state.connected || exclusive;
  ui.releaseBtn.disabled = !exclusive;
  ui.reacquireBtn.disabled = !state.connected;
  ui.emergencyBtn.disabled = !exclusive;

  const sliders = ui.tunerGrid.querySelectorAll('input[type="range"]');
  sliders.forEach((el) => {
    el.disabled = !exclusive;
  });

  if (!state.connected) {
    setLeaseModeBadge("muted");
  } else if (!state.lease.active) {
    setLeaseModeBadge("muted");
  } else if (exclusive) {
    setLeaseModeBadge("exclusive");
  } else {
    setLeaseModeBadge("locked");
  }

  if (state.stream.active) {
    setStreamBadge("active");
  } else if (state.stream.stateLabel === "lost") {
    setStreamBadge("error");
  } else {
    setStreamBadge("idle");
  }
}

function updateLeaseFromPayload(payload = {}, includeToken = false) {
  if (typeof payload.active === "boolean") state.lease.active = payload.active;
  if (payload.leaseId !== undefined) state.lease.leaseId = payload.leaseId || "";
  if (includeToken && payload.leaseToken) state.lease.leaseToken = payload.leaseToken;
  if (payload.scope) state.lease.scope = payload.scope;
  if (payload.ownerWsClientId !== undefined) state.lease.ownerWsClientId = Number(payload.ownerWsClientId) || 0;
  if (payload.ownerClientName !== undefined) state.lease.ownerClientName = payload.ownerClientName || "";
  if (payload.ownerInstanceId !== undefined) state.lease.ownerInstanceId = payload.ownerInstanceId || "";
  if (payload.remainingMs !== undefined) state.lease.remainingMs = Number(payload.remainingMs) || 0;
  if (payload.ttlMs !== undefined) state.lease.ttlMs = Number(payload.ttlMs) || 0;
  if (payload.heartbeatIntervalMs !== undefined) {
    state.lease.heartbeatIntervalMs = Number(payload.heartbeatIntervalMs) || 1000;
  }
  if (payload.takeoverAllowed !== undefined) state.lease.takeoverAllowed = Boolean(payload.takeoverAllowed);

  renderLeaseState();
}

function updateStreamFromPayload(payload = {}) {
  if (payload.active !== undefined) state.stream.active = Boolean(payload.active);
  if (payload.sessionId !== undefined) state.stream.sessionId = payload.sessionId || "";
  if (payload.ownerWsClientId !== undefined) state.stream.ownerWsClientId = Number(payload.ownerWsClientId) || 0;
  if (payload.targetFps !== undefined) state.stream.targetFps = Number(payload.targetFps) || state.stream.targetFps;
  if (payload.staleTimeoutMs !== undefined) state.stream.staleTimeoutMs = Number(payload.staleTimeoutMs) || state.stream.staleTimeoutMs;
  if (payload.frameContractVersion !== undefined) state.stream.frameContractVersion = Number(payload.frameContractVersion) || 1;
  if (payload.pixelFormat !== undefined) state.stream.pixelFormat = String(payload.pixelFormat || "rgb888");
  if (payload.ledCount !== undefined) state.stream.ledCount = Number(payload.ledCount) || TOTAL_LEDS;
  if (payload.headerBytes !== undefined) state.stream.headerBytes = Number(payload.headerBytes) || FRAME_HEADER_BYTES;
  if (payload.payloadBytes !== undefined) state.stream.payloadBytes = Number(payload.payloadBytes) || FRAME_PAYLOAD_BYTES;
  if (payload.maxPayloadBytes !== undefined) state.stream.maxPayloadBytes = Number(payload.maxPayloadBytes) || FRAME_PAYLOAD_BYTES;
  if (payload.mailboxDepth !== undefined) state.stream.mailboxDepth = Number(payload.mailboxDepth) || 2;
  if (payload.lastFrameSeq !== undefined) state.stream.lastFrameSeq = Number(payload.lastFrameSeq) || 0;
  if (payload.lastFrameRxMs !== undefined) state.stream.lastFrameRxMs = Number(payload.lastFrameRxMs) || 0;
  if (payload.framesRx !== undefined) state.stream.framesRx = Number(payload.framesRx) || 0;
  if (payload.framesRendered !== undefined) state.stream.framesRendered = Number(payload.framesRendered) || 0;
  if (payload.framesDroppedMailbox !== undefined) state.stream.framesDroppedMailbox = Number(payload.framesDroppedMailbox) || 0;
  if (payload.framesInvalid !== undefined) state.stream.framesInvalid = Number(payload.framesInvalid) || 0;
  if (payload.framesBlockedLease !== undefined) state.stream.framesBlockedLease = Number(payload.framesBlockedLease) || 0;
  if (payload.staleTimeouts !== undefined) state.stream.staleTimeouts = Number(payload.staleTimeouts) || 0;

  state.stream.stateLabel = state.stream.active ? "active" : state.stream.stateLabel;
  renderStreamState();
}

function renderLeaseState() {
  ui.leaseId.textContent = `leaseId: ${state.lease.leaseId || "-"}`;
  ui.leaseTtl.textContent = `remaining: ${Math.max(0, Math.floor(state.lease.remainingMs || 0))}ms`;
  ui.ownerBadge.textContent = `owner: ${state.lease.ownerClientName || "n/a"}`;

  const rows = [
    ["active", String(Boolean(state.lease.active))],
    ["scope", state.lease.scope || "global"],
    ["ownerClientName", state.lease.ownerClientName || "-"],
    ["ownerInstanceId", state.lease.ownerInstanceId || "-"],
    ["ownerWsClientId", String(state.lease.ownerWsClientId || 0)],
    ["ttlMs", String(state.lease.ttlMs || 0)],
    ["heartbeatIntervalMs", String(state.lease.heartbeatIntervalMs || 0)],
    ["remainingMs", String(Math.max(0, Math.floor(state.lease.remainingMs || 0)))],
  ];

  ui.leaseStateBlock.innerHTML = "";
  rows.forEach(([key, value]) => {
    const cell = document.createElement("div");
    cell.textContent = `${key}: ${value}`;
    ui.leaseStateBlock.appendChild(cell);
  });

  updateControls();
}

function renderStreamState() {
  ui.streamMeta.textContent = `stream: ${state.stream.stateLabel}`;
  ui.streamQueue.textContent = `queue: ${state.ws ? state.ws.bufferedAmount : 0}B`;

  const rows = [
    ["active", String(Boolean(state.stream.active))],
    ["sessionId", state.stream.sessionId || "-"],
    ["ownerWsClientId", String(state.stream.ownerWsClientId || 0)],
    ["targetFps", String(state.stream.targetFps)],
    ["staleTimeoutMs", String(state.stream.staleTimeoutMs)],
    ["frameContractVersion", String(state.stream.frameContractVersion)],
    ["pixelFormat", state.stream.pixelFormat],
    ["ledCount", String(state.stream.ledCount)],
    ["headerBytes", String(state.stream.headerBytes)],
    ["payloadBytes", String(state.stream.payloadBytes)],
    ["mailboxDepth", String(state.stream.mailboxDepth)],
    ["framesSent", String(state.stream.framesSent)],
    ["framesDroppedBackpressure", String(state.stream.framesDroppedBackpressure)],
    ["framesRx", String(state.stream.framesRx)],
    ["framesRendered", String(state.stream.framesRendered)],
    ["framesDroppedMailbox", String(state.stream.framesDroppedMailbox)],
    ["framesInvalid", String(state.stream.framesInvalid)],
    ["framesBlockedLease", String(state.stream.framesBlockedLease)],
    ["staleTimeouts", String(state.stream.staleTimeouts)],
    ["dropRatio10s", `${(state.diagnostics.latest.dropRatio10s * 100).toFixed(2)}%`],
    ["sendCadenceHz", state.diagnostics.latest.sendCadenceHz.toFixed(2)],
    ["sendJitterMs", state.diagnostics.latest.sendJitterMs.toFixed(2)],
  ];

  ui.streamStateBlock.innerHTML = "";
  rows.forEach(([key, value]) => {
    const cell = document.createElement("div");
    cell.textContent = `${key}: ${value}`;
    ui.streamStateBlock.appendChild(cell);
  });

  updateControls();
}

function drawSparkline(canvas, values, opts = {}) {
  if (!canvas) return;
  const ctx = canvas.getContext("2d");
  if (!ctx) return;

  const width = canvas.width;
  const height = canvas.height;
  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = "#0f172a";
  ctx.fillRect(0, 0, width, height);

  if (!values.length) return;

  const padding = 6;
  const min = Number.isFinite(opts.min) ? opts.min : Math.min(...values);
  const maxRaw = Number.isFinite(opts.max) ? opts.max : Math.max(...values);
  const max = maxRaw <= min ? min + 1 : maxRaw;

  ctx.strokeStyle = opts.stroke || "#22d3ee";
  ctx.lineWidth = 1.6;
  ctx.beginPath();
  values.forEach((value, idx) => {
    const x = padding + ((width - padding * 2) * idx) / Math.max(1, values.length - 1);
    const yNorm = (value - min) / (max - min);
    const y = height - padding - yNorm * (height - padding * 2);
    if (idx === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });
  ctx.stroke();
}

function diagnosticsClassForCell(key, value) {
  if (key === "dropRatio10s" || key === "dropRatio60s") {
    if (value >= 0.2) return "critical";
    if (value >= 0.05) return "warn";
    return "";
  }
  if (key === "queueP95") {
    if (value >= state.stream.queueDropThreshold * 0.9) return "critical";
    if (value >= state.stream.queueDropThreshold * 0.6) return "warn";
    return "";
  }
  if (key === "sendJitterMs") {
    if (value >= 16) return "critical";
    if (value >= 8) return "warn";
    return "";
  }
  return "";
}

function renderDiagnosticsPanel() {
  const latest = state.diagnostics.latest;
  const metrics = [
    ["queueNow", `${latest.queueNow.toFixed(0)}B`],
    ["queueP95", `${latest.queueP95.toFixed(0)}B`],
    ["queueMax", `${latest.queueMax.toFixed(0)}B`],
    ["sendCadenceHz", `${latest.sendCadenceHz.toFixed(2)}Hz`],
    ["sendJitterMs", `${latest.sendJitterMs.toFixed(2)}ms`],
    ["dropRatio10s", `${(latest.dropRatio10s * 100).toFixed(2)}%`],
    ["dropRatio60s", `${(latest.dropRatio60s * 100).toFixed(2)}%`],
    ["attempted10s", String(latest.attempted10s)],
    ["sent10s", String(latest.sent10s)],
    ["dropped10s", String(latest.dropped10s)],
    ["attempted60s", String(latest.attempted60s)],
    ["sent60s", String(latest.sent60s)],
  ];

  ui.diagMetrics.innerHTML = "";
  metrics.forEach(([key, value]) => {
    const cell = document.createElement("div");
    const cellClass = diagnosticsClassForCell(key, latest[key] ?? 0);
    cell.className = `diag-cell${cellClass ? ` ${cellClass}` : ""}`;
    cell.textContent = `${key}: ${value}`;
    ui.diagMetrics.appendChild(cell);
  });

  const history = state.diagnostics.historyBuffer;
  drawSparkline(
    ui.queueSpark,
    history.map((entry) => entry.queueNow),
    { min: 0, stroke: "#60a5fa" },
  );
  drawSparkline(
    ui.cadenceSpark,
    history.map((entry) => entry.sendCadenceHz),
    { min: 0, stroke: "#22c55e" },
  );
  drawSparkline(
    ui.dropSpark,
    history.map((entry) => entry.dropRatio10s * 100),
    { min: 0, max: 100, stroke: "#f59e0b" },
  );
}

function recordDiagnosticsSample(sample) {
  state.diagnostics.sampleBuffer.push(sample);
  if (state.diagnostics.sampleBuffer.length > state.diagnostics.maxSamples) {
    state.diagnostics.sampleBuffer.splice(0, state.diagnostics.sampleBuffer.length - state.diagnostics.maxSamples);
  }
}

function diagnosticsWindowStats(samples) {
  if (!samples.length) {
    return {
      queueNow: 0,
      queueP95: 0,
      queueMax: 0,
      sendCadenceHz: 0,
      sendJitterMs: 0,
      dropRatio: 0,
      attempted: 0,
      sent: 0,
      dropped: 0,
    };
  }

  const queueValues = samples.map((sample) => sample.bufferedAmount);
  const attempted = samples.reduce((acc, sample) => acc + (sample.attempted || 0), 0);
  const sent = samples.reduce((acc, sample) => acc + (sample.sent || 0), 0);
  const dropped = samples.reduce((acc, sample) => acc + (sample.dropped || 0), 0);
  const sendIntervals = samples
    .map((sample) => sample.sendIntervalMs)
    .filter((value) => Number.isFinite(value) && value > 0);

  const firstTs = samples[0].ts;
  const lastTs = samples[samples.length - 1].ts;
  const elapsedSeconds = Math.max(0.001, (lastTs - firstTs) / 1000);
  const sendCadenceHz = sent / elapsedSeconds;
  const dropRatio = attempted > 0 ? dropped / attempted : 0;

  return {
    queueNow: samples[samples.length - 1].bufferedAmount,
    queueP95: percentile(queueValues, 0.95),
    queueMax: Math.max(...queueValues),
    sendCadenceHz,
    sendJitterMs: stddev(sendIntervals),
    dropRatio,
    attempted,
    sent,
    dropped,
  };
}

function aggregateDiagnostics() {
  const now = nowMs();
  const shortSamples = state.diagnostics.sampleBuffer.filter((sample) => sample.ts >= now - state.diagnostics.windowShortMs);
  const longSamples = state.diagnostics.sampleBuffer.filter((sample) => sample.ts >= now - state.diagnostics.windowLongMs);
  const shortStats = diagnosticsWindowStats(shortSamples);
  const longStats = diagnosticsWindowStats(longSamples);

  state.diagnostics.latest = {
    queueNow: shortStats.queueNow,
    queueP95: shortStats.queueP95,
    queueMax: shortStats.queueMax,
    sendCadenceHz: shortStats.sendCadenceHz,
    sendJitterMs: shortStats.sendJitterMs,
    dropRatio10s: shortStats.dropRatio,
    dropRatio60s: longStats.dropRatio,
    attempted10s: shortStats.attempted,
    sent10s: shortStats.sent,
    dropped10s: shortStats.dropped,
    attempted60s: longStats.attempted,
    sent60s: longStats.sent,
    dropped60s: longStats.dropped,
  };

  state.diagnostics.historyBuffer.push({
    ts: now,
    queueNow: state.diagnostics.latest.queueNow,
    sendCadenceHz: state.diagnostics.latest.sendCadenceHz,
    dropRatio10s: state.diagnostics.latest.dropRatio10s,
  });
  if (state.diagnostics.historyBuffer.length > state.diagnostics.maxHistory) {
    state.diagnostics.historyBuffer.splice(0, state.diagnostics.historyBuffer.length - state.diagnostics.maxHistory);
  }

  renderDiagnosticsPanel();

  if ((shortSamples.length > 0 || state.stream.active) && now - state.diagnostics.lastEmitMs >= 1000) {
    state.diagnostics.lastEmitMs = now;
    logEvent("render.diagnostics.window", {
      bufferedAmountNow: state.diagnostics.latest.queueNow,
      bufferedAmountP95: state.diagnostics.latest.queueP95,
      bufferedAmountMax: state.diagnostics.latest.queueMax,
      sendCadenceHz: state.diagnostics.latest.sendCadenceHz,
      sendJitterMs: state.diagnostics.latest.sendJitterMs,
      dropRatio: state.diagnostics.latest.dropRatio10s,
      attempted: state.diagnostics.latest.attempted10s,
      sent: state.diagnostics.latest.sent10s,
      dropped: state.diagnostics.latest.dropped10s,
      windowMs: state.diagnostics.windowShortMs,
    });
    logEvent("render.frame.drop_ratio_window", {
      dropRatio10s: state.diagnostics.latest.dropRatio10s,
      dropRatio60s: state.diagnostics.latest.dropRatio60s,
      attempted10s: state.diagnostics.latest.attempted10s,
      attempted60s: state.diagnostics.latest.attempted60s,
    });
  }
}

function startDiagnosticsLoop() {
  window.clearInterval(state.diagnostics.timer);
  state.diagnostics.timer = window.setInterval(aggregateDiagnostics, state.diagnostics.aggregationMs);
}

function startLeaseCountdown() {
  window.clearInterval(state.leaseCountdownTimer);
  state.leaseCountdownTimer = window.setInterval(() => {
    if (state.lease.remainingMs > 0) {
      state.lease.remainingMs = Math.max(0, state.lease.remainingMs - 100);
      renderLeaseState();
    }
  }, 100);
}

function stopLeaseCountdown() {
  window.clearInterval(state.leaseCountdownTimer);
  state.leaseCountdownTimer = null;
}

function stopHeartbeat() {
  window.clearInterval(state.heartbeatTimer);
  state.heartbeatTimer = null;
  ui.hbState.textContent = "heartbeat: idle";
}

async function startHeartbeat() {
  stopHeartbeat();
  if (!canMutate()) return;

  const tick = async () => {
    if (!canMutate()) {
      stopHeartbeat();
      return;
    }

    try {
      const result = await sendWsCommand("control.heartbeat", {
        leaseId: state.lease.leaseId,
        leaseToken: state.lease.leaseToken,
      });
      ui.hbState.textContent = "heartbeat: ok";
      updateLeaseFromPayload(result, false);
      logEvent("control.lease.heartbeat", result);
    } catch (error) {
      ui.hbState.textContent = "heartbeat: failed";
      state.lease.active = false;
      state.lease.leaseToken = "";
      state.stream.stateLabel = "lost";
      state.stream.active = false;
      renderLeaseState();
      renderStreamState();
      toast(`Lease heartbeat failed: ${error.message}`);
      logEvent("control.lease.heartbeat_failed", { message: error.message });
      stopHeartbeat();
    }
  };

  await tick();
  state.heartbeatTimer = window.setInterval(tick, Math.max(300, state.lease.heartbeatIntervalMs || 1000));
}

function stopStatusPolling() {
  window.clearInterval(state.statusPollTimer);
  state.statusPollTimer = null;
}

function startStatusPolling() {
  stopStatusPolling();
  state.statusPollTimer = window.setInterval(async () => {
    if (!state.connected) return;
    await refreshLeaseStatus();
    await refreshStreamStatus();
    await refreshDeviceStatus();
  }, 1500);
}

function cleanupWs() {
  if (state.ws) {
    state.ws.onopen = null;
    state.ws.onclose = null;
    state.ws.onerror = null;
    state.ws.onmessage = null;
    state.ws.close();
  }

  state.ws = null;
  state.connected = false;
  state.pending.clear();
  stopHeartbeat();
  stopStatusPolling();
  stopLeaseCountdown();
  setConnectionBadge(false);
  updateControls();
}

function wsUrl() {
  const host = state.host || "lightwaveos.local";
  return `ws://${host}/ws`;
}

function apiUrl(path) {
  const host = state.host || "lightwaveos.local";
  return `http://${host}${path}`;
}

function connect() {
  cleanupWs();
  state.host = ui.hostInput.value.trim();
  const ws = new WebSocket(wsUrl());
  ws.binaryType = "arraybuffer";
  state.ws = ws;

  ws.onopen = async () => {
    state.connected = true;
    setConnectionBadge(true);
    startLeaseCountdown();
    renderLeaseState();
    renderStreamState();
    toast("Connected to device");
    logEvent("ws.connect", { host: state.host });
    await refreshLeaseStatus();
    await refreshStreamStatus();
    await refreshDeviceStatus();
    startStatusPolling();
  };

  ws.onclose = () => {
    const hadLease = state.lease.active;
    const hadStream = state.stream.active;
    cleanupWs();
    state.lease.active = false;
    state.lease.leaseToken = "";
    state.lease.leaseId = "";
    state.stream.active = false;
    state.stream.sessionId = "";
    state.stream.stateLabel = "lost";
    renderLeaseState();
    renderStreamState();

    if (hadLease || hadStream) {
      toast("Connection closed: lease/stream session lost");
      logEvent("control.lease.lost", { reason: "ws_disconnected" });
      logEvent("render.stream.stopped", { reason: "ws_disconnected" });
    }
  };

  ws.onerror = () => {
    logEvent("ws.error", {});
  };

  ws.onmessage = (event) => {
    if (typeof event.data !== "string") {
      return;
    }

    let msg;
    try {
      msg = JSON.parse(event.data);
    } catch {
      return;
    }

    if (msg.requestId && state.pending.has(msg.requestId)) {
      const pending = state.pending.get(msg.requestId);
      state.pending.delete(msg.requestId);
      if (msg.success === false) {
        pending.reject(new Error(msg.error?.message || msg.error?.code || "Request failed"));
      } else {
        pending.resolve(msg.data ?? msg);
      }
      return;
    }

    handleUnsolicitedMessage(msg);
  };
}

function handleUnsolicitedMessage(msg) {
  if (msg.type === "control.stateChanged" && msg.data) {
    const previouslyExclusive = canMutate();
    updateLeaseFromPayload(msg.data, false);
    logEvent(`control.lease.${msg.event || "changed"}`, msg.data);

    if (previouslyExclusive && !canMutate()) {
      stopHeartbeat();
      state.stream.stateLabel = "lost";
      state.stream.active = false;
      renderStreamState();
      toast(`Lease lost: ${msg.event || "state_changed"}`);
    }
    return;
  }

  if (msg.type === "render.stream.stateChanged" && msg.data) {
    updateStreamFromPayload(msg.data);
    state.stream.stateLabel = msg.data.active ? "active" : (msg.data.reason ? "lost" : "idle");
    renderStreamState();
    logEvent(msg.event || "render.stream.stateChanged", msg.data);

    if (!msg.data.active && msg.data.reason) {
      toast(`Render stream stopped: ${msg.data.reason}`);
    }
    return;
  }

  if (msg.type === "status" || msg.type === "device.status") {
    applyStatusFrame(msg.data || msg);
    return;
  }

  if (msg.type === "error") {
    logEvent("ws.error", msg.error || {});
    if (msg.error?.code === "CONTROL_LOCKED") {
      toast(`Locked by ${msg.error.ownerClientName || "another client"}`);
    }
  }
}

function sendWsCommand(type, payload = {}) {
  if (!state.ws || state.ws.readyState !== WebSocket.OPEN) {
    return Promise.reject(new Error("WebSocket not connected"));
  }

  const requestId = makeReqId(type.replace(/\W/g, "_"));
  const body = { type, requestId, ...payload };

  return new Promise((resolve, reject) => {
    state.pending.set(requestId, { resolve, reject });
    state.ws.send(JSON.stringify(body));

    window.setTimeout(() => {
      if (state.pending.has(requestId)) {
        state.pending.delete(requestId);
        reject(new Error(`Timeout waiting for ${type}`));
      }
    }, 4000);
  });
}

async function fetchJson(path, options = {}) {
  const headers = { "Content-Type": "application/json", ...(options.headers || {}) };
  const response = await fetch(apiUrl(path), { ...options, headers });
  const payload = await response.json().catch(() => ({}));
  if (!response.ok || payload.success === false) {
    const message = payload?.error?.message || `HTTP ${response.status}`;
    throw new Error(message);
  }
  return payload;
}

async function fetchMutating(path, body) {
  const headers = {};
  if (state.lease.active) {
    if (!state.lease.leaseToken) {
      throw new Error("No lease token in memory");
    }
    headers["X-Control-Lease"] = state.lease.leaseToken;
    if (state.lease.leaseId) {
      headers["X-Control-Lease-Id"] = state.lease.leaseId;
    }
  }

  return fetchJson(path, {
    method: "POST",
    headers,
    body: JSON.stringify(body),
  });
}

async function refreshLeaseStatus() {
  try {
    const byWs = state.connected ? await sendWsCommand("control.status") : null;
    if (byWs) {
      updateLeaseFromPayload(byWs, false);
      return;
    }
  } catch {
    // Fall through to REST.
  }

  try {
    const rest = await fetchJson("/api/v1/control/status");
    updateLeaseFromPayload(rest.data || {}, false);
  } catch (error) {
    logEvent("control.status.fetch_failed", { message: error.message });
  }
}

async function refreshStreamStatus() {
  if (!state.connected) return;
  try {
    const payload = await sendWsCommand("render.stream.status");
    updateStreamFromPayload(payload || {});
    state.stream.stateLabel = state.stream.active ? "active" : "idle";
    renderStreamState();
  } catch (error) {
    logEvent("render.stream.status_failed", { message: error.message });
  }
}

async function refreshDeviceStatus() {
  if (!state.connected) return;
  try {
    const byWs = await sendWsCommand("device.getStatus");
    applyStatusFrame(byWs || {});
  } catch {
    try {
      const rest = await fetchJson("/api/v1/device/status");
      applyStatusFrame(rest.data || {});
    } catch (error) {
      logEvent("device.status.fetch_failed", { message: error.message });
    }
  }
}

async function acquireLease() {
  const result = await sendWsCommand("control.acquire", {
    clientName: state.clientName,
    clientInstanceId: state.clientInstanceId,
    scope: "global",
  });
  updateLeaseFromPayload(result, true);
  toast("Exclusive control acquired");
  logEvent("control.lease.acquired", result);
  await startHeartbeat();
  await startExternalStream();
}

async function releaseLease(reason = "user_release") {
  if (state.stream.active) {
    await stopExternalStream("lease_release").catch(() => {});
  }

  if (!state.lease.leaseId || !state.lease.leaseToken) {
    state.lease.active = false;
    renderLeaseState();
    return;
  }

  const result = await sendWsCommand("control.release", {
    leaseId: state.lease.leaseId,
    leaseToken: state.lease.leaseToken,
    reason,
  });

  state.lease.active = false;
  state.lease.leaseToken = "";
  state.lease.leaseId = "";
  stopHeartbeat();
  renderLeaseState();
  toast("Control lease released");
  logEvent("control.lease.released", result);
}

async function emergencyStop() {
  await fetchMutating("/api/v1/parameters", { brightness: 0 });
  state.sim.params.brightness = 0;
  toast("Emergency stop: brightness set to 0");
  logEvent("control.emergency_stop", {});
}

async function startExternalStream() {
  if (!canMutate()) {
    throw new Error("Acquire control before starting render stream");
  }

  const response = await sendWsCommand("render.stream.start", {
    targetFps: state.stream.targetFps,
    staleTimeoutMs: state.stream.staleTimeoutMs,
    desiredPixelFormat: "rgb888",
    desiredLedCount: TOTAL_LEDS,
    clientName: state.clientName,
    clientInstanceId: state.clientInstanceId,
  });

  updateStreamFromPayload(response);
  state.stream.stateLabel = "active";
  renderStreamState();
  logEvent("render.stream.started", response);
}

async function stopExternalStream(reason = "user_stop") {
  if (!state.connected || !state.stream.active) {
    state.stream.active = false;
    state.stream.stateLabel = "idle";
    renderStreamState();
    return;
  }

  try {
    const response = await sendWsCommand("render.stream.stop", { reason });
    updateStreamFromPayload(response);
    state.stream.active = false;
    state.stream.stateLabel = "idle";
    renderStreamState();
    logEvent("render.stream.stopped", response);
  } catch (error) {
    state.stream.active = false;
    state.stream.stateLabel = "lost";
    renderStreamState();
    throw error;
  }
}

function applyStatusFrame(status = {}) {
  if (status.controlLease) {
    updateLeaseFromPayload(status.controlLease, false);
  }

  if (status.renderStream) {
    updateStreamFromPayload(status.renderStream);
  }

  if (status.renderCounters) {
    updateStreamFromPayload(status.renderCounters);
  }

  if (Number.isInteger(status.effectId)) {
    const hex = `0x${(status.effectId & 0xffff).toString(16).toUpperCase().padStart(4, "0")}`;
    const mapped = state.catalog.byEffectHex.get(hex);
    if (mapped && mapped !== state.catalog.currentEffectId) {
      void setCurrentEffect(mapped);
    }
  }

  const phase = derivePhase();
  highlightAlgorithmPhase(phase);
}

function derivePhase() {
  const t = Number(ui.timeline.value) / 1000;
  if (t < 0.16) return "input";
  if (t < 0.36) return "mapping";
  if (t < 0.56) return "modulation";
  if (t < 0.84) return "render";
  if (t < 0.95) return "post";
  return "output";
}

function highlightAlgorithmPhase(name) {
  const nodes = ui.algoFlow.querySelectorAll("[data-node]");
  nodes.forEach((el) => {
    el.classList.toggle("is-active", el.dataset.node === name);
  });

  ui.phaseLabel.textContent = `phase: ${name}`;

  const current = state.catalog.currentEffectData;
  const phaseRanges = current?.phaseRanges?.[name] || [];
  const rows = ui.codeVisualiser.querySelectorAll(".code-line");
  rows.forEach((row) => {
    const lineNum = Number(row.dataset.line || "0");
    row.classList.toggle("is-active", lineInRanges(lineNum, phaseRanges));
  });
}

function createCodeVisualiser() {
  const current = state.catalog.currentEffectData;
  if (!current) {
    ui.codeVisualiser.innerHTML = "";
    ui.effectNameLabel.textContent = "effect: -";
    ui.sourcePathLabel.textContent = "source: -";
    renderMappingBadge(null);
    return;
  }

  const lines = String(current.sourceText || "").split("\n");
  ui.codeVisualiser.innerHTML = "";
  lines.forEach((line, idx) => {
    const row = document.createElement("div");
    row.className = "code-line";
    row.dataset.line = String(idx + 1);

    const num = document.createElement("span");
    num.className = "code-line-num";
    num.textContent = String(idx + 1).padStart(2, "0");

    const body = document.createElement("span");
    body.className = "code-line-body";
    body.textContent = line;

    row.append(num, body);
    ui.codeVisualiser.appendChild(row);
  });

  ui.effectNameLabel.textContent = `effect: ${current.displayName} (${current.effectId})`;
  ui.sourcePathLabel.textContent = `source: ${current.sourcePath || "-"}`;
  renderMappingBadge(current);
}

function updateCodeVars(vars) {
  const pairs = [
    ["beat", vars.beat.toFixed(3)],
    ["bass", vars.bass.toFixed(3)],
    ["warp", vars.warp.toFixed(3)],
    ["phase", vars.phase],
    ["speed", String(state.sim.params.speed)],
    ["intensity", String(state.sim.params.intensity)],
    ["saturation", String(state.sim.params.saturation)],
    ["variation", String(state.sim.params.variation)],
  ];

  ui.codeVars.innerHTML = "";
  pairs.forEach(([k, v]) => {
    const node = document.createElement("div");
    node.className = "code-var";
    node.textContent = `${k}: ${v}`;
    ui.codeVars.appendChild(node);
  });
}

function appendCodeTrace(message) {
  state.traceBuffer.unshift(`${new Date().toISOString()} ${message}`);
  state.traceBuffer = state.traceBuffer.slice(0, 28);
  ui.codeTrace.textContent = state.traceBuffer.join("\n");
}

function createTuner() {
  ui.tunerGrid.innerHTML = "";
  paramConfig.forEach((cfg) => {
    const card = document.createElement("div");
    card.className = "param";

    const head = document.createElement("div");
    head.className = "param-head";

    const label = document.createElement("span");
    label.textContent = cfg.label;

    const value = document.createElement("span");
    value.textContent = String(cfg.value);

    head.append(label, value);

    const slider = document.createElement("input");
    slider.type = "range";
    slider.min = String(cfg.min);
    slider.max = String(cfg.max);
    slider.value = String(cfg.value);
    slider.disabled = true;

    slider.addEventListener("input", () => {
      value.textContent = slider.value;
      state.sim.params[cfg.key] = Number(slider.value);
    });

    slider.addEventListener("change", async () => {
      if (!canMutate()) return;
      try {
        const payload = { [cfg.key]: Number(slider.value) };
        await fetchMutating("/api/v1/parameters", payload);
        logEvent("parameter.update", payload);
      } catch (error) {
        toast(error.message);
      }
    });

    card.append(head, slider);
    ui.tunerGrid.appendChild(card);
  });
}

function hsvToRgb(h, s, v) {
  const hh = ((h % 360) + 360) % 360;
  const ss = Math.max(0, Math.min(1, s));
  const vv = Math.max(0, Math.min(1, v));

  const c = vv * ss;
  const x = c * (1 - Math.abs(((hh / 60) % 2) - 1));
  const m = vv - c;

  let r = 0;
  let g = 0;
  let b = 0;

  if (hh < 60) {
    r = c;
    g = x;
  } else if (hh < 120) {
    r = x;
    g = c;
  } else if (hh < 180) {
    g = c;
    b = x;
  } else if (hh < 240) {
    g = x;
    b = c;
  } else if (hh < 300) {
    r = x;
    b = c;
  } else {
    r = c;
    b = x;
  }

  return [
    Math.round((r + m) * 255),
    Math.round((g + m) * 255),
    Math.round((b + m) * 255),
  ];
}

function simulateEffectFrame(timelineNorm) {
  const speedNorm = Math.max(0.05, state.sim.params.speed / 50);
  const beat = 0.5 + 0.5 * Math.sin(timelineNorm * Math.PI * 2 * speedNorm * 2.0);
  const bass = 0.5 + 0.5 * Math.sin(timelineNorm * Math.PI * 2 * speedNorm * 0.7 + 0.8);
  const warp = (state.sim.params.complexity / 255) * (0.4 + beat * 0.6);
  const sat = state.sim.params.saturation / 255;
  const intensity = state.sim.params.intensity / 255;
  const variation = state.sim.params.variation / 255;
  const brightness = state.sim.params.brightness / 255;

  const frame = new Uint8Array(FRAME_PAYLOAD_BYTES);
  const centre = (LEDS_PER_STRIP - 1) * 0.5;

  for (let strip = 0; strip < STRIPS; strip += 1) {
    const stripOffset = strip * LEDS_PER_STRIP;
    const stripPhase = strip === 0 ? 0 : Math.PI * 0.9;

    for (let i = 0; i < LEDS_PER_STRIP; i += 1) {
      const distance = Math.abs(i - centre) / centre;
      const radial = 1.0 - distance;
      const wave = Math.sin((timelineNorm * 16.0 + distance * 8.0 + stripPhase) * (1.0 + warp));
      const shimmer = Math.cos((timelineNorm * 9.0 + distance * 5.0) * (0.6 + variation * 1.7));
      const hue = (210 + wave * 56 + shimmer * 28 + beat * 44 + strip * 18 + i * 0.55) % 360;
      const value = Math.max(
        0.01,
        brightness * intensity * (0.25 + radial * 0.75) * (0.55 + 0.45 * bass) * (0.65 + 0.35 * Math.max(0, wave)),
      );

      const rgb = hsvToRgb(hue, sat, value);
      const ledIndex = stripOffset + i;
      const px = ledIndex * 3;
      frame[px + 0] = rgb[0];
      frame[px + 1] = rgb[1];
      frame[px + 2] = rgb[2];
    }
  }

  return {
    frame,
    vars: {
      beat,
      bass,
      warp,
      phase: derivePhase(),
    },
  };
}

function updateStripPreview(frameBytes) {
  paintStripFromFrame(ui.stripA, frameBytes, 0, false);
  paintStripFromFrame(ui.stripB, frameBytes, LEDS_PER_STRIP, true);
}

function paintStripFromFrame(container, frameBytes, stripOffset, reverse) {
  if (container.childElementCount === 0) {
    for (let i = 0; i < 64; i += 1) {
      const led = document.createElement("span");
      container.appendChild(led);
    }
  }

  const leds = Array.from(container.children);
  const visualCount = leds.length;

  leds.forEach((node, visualIdx) => {
    const mapped = Math.floor((visualIdx / visualCount) * LEDS_PER_STRIP);
    const stripIndex = reverse ? LEDS_PER_STRIP - 1 - mapped : mapped;
    const ledIndex = stripOffset + stripIndex;
    const px = ledIndex * 3;
    const r = frameBytes[px + 0] || 0;
    const g = frameBytes[px + 1] || 0;
    const b = frameBytes[px + 2] || 0;
    node.style.background = `rgb(${r}, ${g}, ${b})`;
  });
}

function buildFramePacket(seq, frameBytes) {
  const packet = new Uint8Array(FRAME_TOTAL_BYTES);
  packet[0] = 0x4b; // K
  packet[1] = 0x31; // 1
  packet[2] = 0x46; // F
  packet[3] = 0x31; // 1
  packet[4] = 1; // contractVersion
  packet[5] = 1; // pixelFormat rgb888
  packet[6] = 0;
  packet[7] = 0;

  packet[8] = seq & 0xff;
  packet[9] = (seq >>> 8) & 0xff;
  packet[10] = (seq >>> 16) & 0xff;
  packet[11] = (seq >>> 24) & 0xff;

  packet[12] = TOTAL_LEDS & 0xff;
  packet[13] = (TOTAL_LEDS >>> 8) & 0xff;
  packet[14] = 0;
  packet[15] = 0;

  packet.set(frameBytes, FRAME_HEADER_BYTES);
  return packet.buffer;
}

function maybeSendFrame(frameBytes) {
  if (!state.connected || !state.ws || state.ws.readyState !== WebSocket.OPEN) return;
  if (!state.stream.active) return;
  if (!canMutate()) return;

  const now = nowMs();
  const minFrameSpacingMs = 1000 / Math.max(1, state.stream.targetFps || 120);
  if (now - state.transport.lastFrameSentMs < minFrameSpacingMs) {
    return;
  }

  const buffered = state.ws.bufferedAmount;
  ui.streamQueue.textContent = `queue: ${buffered}B`;
  const sample = {
    ts: now,
    bufferedAmount: buffered,
    attempted: 1,
    sent: 0,
    dropped: 0,
    sendIntervalMs: null,
  };

  if (buffered > state.stream.queueDropThreshold) {
    state.stream.framesDroppedBackpressure += 1;
    sample.dropped = 1;
    recordDiagnosticsSample(sample);
    if (now - state.stream.lastBackpressureLogMs > 250) {
      state.stream.lastBackpressureLogMs = now;
      logEvent("render.frame.dropped.backpressure", {
        bufferedAmount: buffered,
        queueDropThreshold: state.stream.queueDropThreshold,
      });
      renderStreamState();
    }
    return;
  }

  state.transport.seq = (state.transport.seq + 1) >>> 0;
  const packet = buildFramePacket(state.transport.seq, frameBytes);
  state.ws.send(packet);
  state.transport.lastFrameSentMs = now;
  state.stream.framesSent += 1;
  state.stream.lastFrameSeq = state.transport.seq;
  state.stream.stateLabel = "active";
  sample.sent = 1;
  if (state.diagnostics.lastSentTs > 0) {
    sample.sendIntervalMs = now - state.diagnostics.lastSentTs;
  }
  state.diagnostics.lastSentTs = now;
  recordDiagnosticsSample(sample);

  if ((state.stream.framesSent & 0x1f) === 0) {
    logEvent("render.frame.sent", {
      seq: state.transport.seq,
      bufferedAmount: buffered,
    });
  }

  if ((state.stream.framesSent & 0x0f) === 0) {
    renderStreamState();
  }
}

function runSimulationAndRender() {
  const timelineNorm = Number(ui.timeline.value) / 1000;
  const phase = derivePhase();
  highlightAlgorithmPhase(phase);

  const sim = simulateEffectFrame(timelineNorm);
  updateCodeVars(sim.vars);
  updateStripPreview(sim.frame);
  const now = nowMs();
  if (now - state.sim.lastTraceMs >= 120) {
    state.sim.lastTraceMs = now;
    appendCodeTrace(
      `phase=${sim.vars.phase} beat=${sim.vars.beat.toFixed(3)} bass=${sim.vars.bass.toFixed(3)} warp=${sim.vars.warp.toFixed(3)}`,
    );
  }

  maybeSendFrame(sim.frame);
}

function transportTick(ts) {
  if (!state.transport.playing) {
    state.transport.raf = null;
    state.transport.lastTs = 0;
    return;
  }

  if (!state.transport.lastTs) state.transport.lastTs = ts;
  const dt = ts - state.transport.lastTs;
  state.transport.lastTs = ts;

  state.transport.timeline = (state.transport.timeline + dt * 0.08 * state.transport.speed) % 1000;
  ui.timeline.value = String(Math.floor(state.transport.timeline));
  runSimulationAndRender();

  state.transport.raf = requestAnimationFrame(transportTick);
}

function playTimeline() {
  if (state.transport.playing) return;
  state.transport.playing = true;
  state.transport.raf = requestAnimationFrame(transportTick);
}

function pauseTimeline() {
  state.transport.playing = false;
  if (state.transport.raf) {
    cancelAnimationFrame(state.transport.raf);
    state.transport.raf = null;
  }
  state.transport.lastTs = 0;
}

function resetTimeline() {
  state.transport.timeline = 0;
  ui.timeline.value = "0";
  runSimulationAndRender();
}

function bindEvents() {
  ui.connectBtn.addEventListener("click", connect);

  ui.statusBtn.addEventListener("click", async () => {
    await refreshLeaseStatus();
    await refreshStreamStatus();
    await refreshDeviceStatus();
  });

  ui.acquireBtn.addEventListener("click", async () => {
    try {
      await acquireLease();
    } catch (error) {
      toast(error.message);
      logEvent("control.lease.acquire_failed", { message: error.message });
    }
  });

  ui.releaseBtn.addEventListener("click", async () => {
    try {
      await releaseLease("user_release");
    } catch (error) {
      toast(error.message);
      logEvent("control.lease.release_failed", { message: error.message });
    }
  });

  ui.reacquireBtn.addEventListener("click", async () => {
    try {
      await acquireLease();
    } catch (error) {
      toast(error.message);
      logEvent("control.lease.reacquire_failed", { message: error.message });
    }
  });

  ui.emergencyBtn.addEventListener("click", async () => {
    try {
      await emergencyStop();
    } catch (error) {
      toast(error.message);
      logEvent("control.emergency_stop_failed", { message: error.message });
    }
  });

  ui.playBtn.addEventListener("click", playTimeline);
  ui.pauseBtn.addEventListener("click", pauseTimeline);
  ui.resetBtn.addEventListener("click", resetTimeline);

  ui.speedButtons.forEach((btn) => {
    btn.addEventListener("click", () => {
      const speed = Number(btn.dataset.speed || "1");
      state.transport.speed = speed;
      ui.speedButtons.forEach((b) => b.classList.toggle("is-active", b === btn));
      logEvent("timeline.speed", { speed });
    });
  });

  ui.timeline.addEventListener("input", () => {
    state.transport.timeline = Number(ui.timeline.value);
    runSimulationAndRender();
  });

  ui.effectSelect.addEventListener("change", async () => {
    const next = String(ui.effectSelect.value || "");
    if (!next) return;
    await setCurrentEffect(next);
    logEvent("visualiser.effect_changed", { effectId: next });
  });

  window.addEventListener("beforeunload", async () => {
    stopStatusPolling();
    window.clearInterval(state.diagnostics.timer);

    if (state.stream.active) {
      try {
        await stopExternalStream("window_unload");
      } catch {
        // Best effort.
      }
    }

    if (canMutate()) {
      try {
        await releaseLease("window_unload");
      } catch {
        // Best effort.
      }
    }

    cleanupWs();
  });
}

async function initWasmRuntime() {
  try {
    const mod = await import("./wasm/wasm-loader.js");
    if (mod && typeof mod.initialiseComposerWasm === "function") {
      await mod.initialiseComposerWasm();
      state.sim.wasmReady = true;
      logEvent("wasm.runtime.ready", {});
      return;
    }
    logEvent("wasm.runtime.unavailable", { reason: "missing_initialiser" });
  } catch (error) {
    logEvent("wasm.runtime.unavailable", { reason: error.message });
  }
}

async function init() {
  createTuner();
  await loadEffectCatalogue();
  populateEffectSelect();
  const defaultEffect =
    state.catalog.byEffectId.has("EID_LGP_PERLIN_INTERFERENCE_WEAVE")
      ? "EID_LGP_PERLIN_INTERFERENCE_WEAVE"
      : (listCatalogueEffects()[0]?.effectId || "");
  if (defaultEffect) {
    await setCurrentEffect(defaultEffect);
  } else {
    createCodeVisualiser();
  }
  renderLeaseState();
  renderStreamState();
  renderDiagnosticsPanel();
  bindEvents();
  startDiagnosticsLoop();
  runSimulationAndRender();
  initWasmRuntime();
  logEvent("dashboard.init", { clientInstanceId: state.clientInstanceId });
}

init().catch((error) => {
  logEvent("dashboard.init_failed", { message: error.message });
  toast(`Initialisation failed: ${error.message}`);
});
