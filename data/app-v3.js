// LightwaveOS Dashboard V3 - Integrated JavaScript
// WebSocket + REST API for ESP32-S3 Firmware

// ========== Configuration ==========
const CONFIG = {
    WS_URL: `ws://${window.location.hostname}/ws`,
    API_BASE: '/api',
    RECONNECT_INTERVAL: 3000,
    DEBOUNCE_SLIDER: 150,
    DEBOUNCE_PIPELINE: 250,
    PERFORMANCE_INTERVAL: 1000,
};

// ========== State ==========
const state = {
    ws: null,
    connected: false,
    reconnectTimer: null,

    // Current values
    effectId: 0,
    effectName: 'Loading...',
    brightness: 128,
    speed: 25,
    paletteId: 0,
    paletteName: 'Loading...',

    // Zone state
    zoneEnabled: false,
    zoneCount: 3,
    selectedZone: 0,
    zones: [
        { id: 0, enabled: true, effectId: 0, brightness: 255, speed: 25, paletteId: 0 },
        { id: 1, enabled: true, effectId: 0, brightness: 255, speed: 25, paletteId: 0 },
        { id: 2, enabled: true, effectId: 0, brightness: 255, speed: 25, paletteId: 0 },
        { id: 3, enabled: false, effectId: 0, brightness: 255, speed: 25, paletteId: 0 },
    ],

    // Data lists
    effects: [],
    palettes: [],
    transitions: [],

    // Performance
    fps: 0,
    cpuUsage: 0,
    freeHeap: 0,
    fragmentation: 0,

    // UI state
    currentTab: 'composer',
    currentCategory: 'all',
    transitionEnabled: false,
    currentTransition: 0,
};

// ========== API Layer ==========
const API = {
    request: async (endpoint, options = {}) => {
        try {
            const response = await fetch(`${CONFIG.API_BASE}${endpoint}`, {
                headers: { 'Content-Type': 'application/json' },
                ...options,
            });
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            return response.json();
        } catch (error) {
            console.error(`API ${endpoint}:`, error);
            showToast(`API error: ${error.message}`, 'error');
            throw error;
        }
    },

    // GET endpoints
    getStatus: () => API.request('/status'),
    getEffects: () => API.request('/effects'),
    getPalettes: () => API.request('/palettes'),
    getZoneStatus: () => API.request('/zone/status'),
    getZonePresets: () => API.request('/zone/presets'),
    getPerformance: () => API.request('/enhancement/status'),

    // POST endpoints
    setEffect: (id) => API.request('/effect', { method: 'POST', body: JSON.stringify({ effectId: id }) }),
    setBrightness: (val) => API.request('/brightness', { method: 'POST', body: JSON.stringify({ value: val }) }),
    setSpeed: (val) => API.request('/speed', { method: 'POST', body: JSON.stringify({ value: val }) }),
    setPalette: (id) => API.request('/palette', { method: 'POST', body: JSON.stringify({ paletteId: id }) }),
    setPipeline: (params) => API.request('/pipeline', { method: 'POST', body: JSON.stringify(params) }),
    setTransition: (params) => API.request('/transitions', { method: 'POST', body: JSON.stringify(params) }),

    // Zone endpoints
    setZoneEnabled: (enabled) => API.request('/zone/enable', { method: 'POST', body: JSON.stringify({ enable: enabled }) }),
    setZoneCount: (count) => API.request('/zone/count', { method: 'POST', body: JSON.stringify({ count }) }),
    setZoneEffect: (zoneId, effectId) => API.request('/zone/config', { method: 'POST', body: JSON.stringify({ zoneId, effectId, enabled: true }) }),
    setZoneBrightness: (zoneId, brightness) => API.request('/zone/brightness', { method: 'POST', body: JSON.stringify({ zoneId, brightness }) }),
    setZoneSpeed: (zoneId, speed) => API.request('/zone/speed', { method: 'POST', body: JSON.stringify({ zoneId, speed }) }),
    loadZonePreset: (presetId) => API.request('/zone/preset/load', { method: 'POST', body: JSON.stringify({ presetId }) }),
    saveZoneConfig: () => API.request('/zone/config/save', { method: 'POST' }),
};

// ========== WebSocket ==========
function connectWebSocket() {
    if (state.ws && state.ws.readyState === WebSocket.OPEN) return;

    console.log('[WS] Connecting...');
    state.ws = new WebSocket(CONFIG.WS_URL);

    state.ws.onopen = () => {
        console.log('[WS] Connected');
        state.connected = true;
        updateConnectionStatus(true);
        clearTimeout(state.reconnectTimer);
    };

    state.ws.onclose = () => {
        console.log('[WS] Disconnected');
        state.connected = false;
        updateConnectionStatus(false);
        scheduleReconnect();
    };

    state.ws.onerror = (error) => {
        console.error('[WS] Error:', error);
    };

    state.ws.onmessage = (event) => {
        try {
            const msg = JSON.parse(event.data);
            handleWsMessage(msg);
        } catch (e) {
            console.error('[WS] Parse error:', e);
        }
    };
}

function scheduleReconnect() {
    if (state.reconnectTimer) return;
    state.reconnectTimer = setTimeout(() => {
        state.reconnectTimer = null;
        connectWebSocket();
    }, CONFIG.RECONNECT_INTERVAL);
}

function sendWS(data) {
    if (state.ws && state.ws.readyState === WebSocket.OPEN) {
        state.ws.send(JSON.stringify(data));
        return true;
    }
    return false;
}

function handleWsMessage(msg) {
    switch (msg.type) {
        case 'status':
            syncStatus(msg);
            break;
        case 'effectChange':
            state.effectId = msg.effectId;
            state.effectName = msg.name;
            updateNowPlaying();
            break;
        case 'paletteChange':
            state.paletteId = msg.paletteId;
            updateCurrentPalette();
            break;
        case 'zone.state':
            syncZoneState(msg);
            break;
        case 'performance':
            syncPerformance(msg);
            break;
        case 'error':
            showToast(msg.message, 'error');
            break;
    }
}

// ========== State Sync ==========
function syncStatus(msg) {
    if (msg.effectId !== undefined) state.effectId = msg.effectId;
    if (msg.effectName !== undefined) state.effectName = msg.effectName;
    if (msg.brightness !== undefined) state.brightness = msg.brightness;
    if (msg.speed !== undefined) state.speed = msg.speed;
    if (msg.paletteId !== undefined) state.paletteId = msg.paletteId;
    if (msg.fps !== undefined) state.fps = msg.fps;
    if (msg.freeHeap !== undefined) state.freeHeap = msg.freeHeap;

    updateNowPlaying();
    updateSliders();
    updateSidebarStats();
}

function syncZoneState(msg) {
    if (msg.enabled !== undefined) state.zoneEnabled = msg.enabled;
    if (msg.zoneCount !== undefined) state.zoneCount = msg.zoneCount;

    if (msg.zones && Array.isArray(msg.zones)) {
        msg.zones.forEach((z) => {
            if (state.zones[z.id]) {
                Object.assign(state.zones[z.id], z);
            }
        });
    }

    updateZoneMixer();
}

function syncPerformance(msg) {
    if (msg.fps !== undefined) state.fps = msg.fps;
    if (msg.cpuUsage !== undefined) state.cpuUsage = msg.cpuUsage;
    if (msg.freeHeap !== undefined) state.freeHeap = msg.freeHeap;
    if (msg.fragmentation !== undefined) state.fragmentation = msg.fragmentation;

    updatePerformanceTab();
    updateSidebarStats();
}

// ========== UI Updates ==========
function updateConnectionStatus(connected) {
    const indicator = document.querySelector('.connection-indicator');
    const text = document.querySelector('.connection-text');
    if (indicator) {
        indicator.style.background = connected ? 'var(--success)' : 'var(--error)';
    }
    if (text) {
        text.textContent = connected ? 'Connected' : 'Disconnected';
    }
}

function updateNowPlaying() {
    const effectEl = document.getElementById('current-effect');
    const categoryEl = document.getElementById('current-category');

    if (effectEl) effectEl.textContent = state.effectName || `Effect ${state.effectId}`;
    if (categoryEl) {
        const effect = state.effects.find(e => e.id === state.effectId);
        categoryEl.textContent = effect ? effect.category : 'Unknown';
    }

    updateCurrentPalette();
}

function updateCurrentPalette() {
    const nameEl = document.getElementById('current-palette');
    const previewEl = document.getElementById('current-palette-preview');

    const palette = state.palettes.find(p => p.id === state.paletteId);
    if (nameEl) nameEl.textContent = palette ? palette.name : `Palette ${state.paletteId}`;
    if (previewEl && palette && palette.gradient) {
        previewEl.style.background = palette.gradient;
    }
}

function updateSliders() {
    const brightnessSlider = document.getElementById('brightness-slider');
    const brightnessValue = document.getElementById('brightness-value');
    const speedSlider = document.getElementById('speed-slider');
    const speedValue = document.getElementById('speed-value');

    if (brightnessSlider) brightnessSlider.value = state.brightness;
    if (brightnessValue) brightnessValue.textContent = state.brightness;
    if (speedSlider) speedSlider.value = state.speed;
    if (speedValue) speedValue.textContent = state.speed;
}

function updateSidebarStats() {
    const fpsEl = document.getElementById('stat-fps');
    const cpuEl = document.getElementById('stat-cpu');

    if (fpsEl) fpsEl.textContent = state.fps;
    if (cpuEl) cpuEl.textContent = state.cpuUsage;
}

function updateZoneMixer() {
    const zoneColors = ['var(--zone-1)', 'var(--zone-2)', 'var(--zone-3)', 'var(--zone-4)'];

    // Update zone bars and channel visibility
    for (let i = 0; i < 4; i++) {
        const bar = document.querySelector(`.zone-bar[data-zone="${i}"]`);
        const channel = document.querySelector(`.zone-channel[data-zone="${i}"]`);
        const zone = state.zones[i];

        if (bar) {
            const height = zone.enabled ? (zone.brightness / 255 * 100) : 0;
            bar.style.height = `${Math.max(5, height)}%`;
        }

        if (channel) {
            channel.classList.toggle('opacity-30', i >= state.zoneCount);
            channel.classList.toggle('pointer-events-none', i >= state.zoneCount);
        }
    }

    // Update zone count selector buttons
    document.querySelectorAll('#zone-count-selector button').forEach(btn => {
        const count = parseInt(btn.dataset.count);
        if (count === state.zoneCount) {
            btn.className = 'w-6 h-6 text-xs rounded bg-[var(--gold)] text-black font-semibold';
        } else {
            btn.className = 'w-6 h-6 text-xs rounded bg-[var(--bg-elevated)] text-[var(--text-secondary)] hover:bg-white/10';
        }
    });

    // Update zone system toggle
    const zoneToggle = document.getElementById('zone-system-toggle');
    if (zoneToggle) {
        zoneToggle.classList.toggle('active', state.zoneEnabled);
    }

    // Update selected zone controls
    updateSelectedZoneControls();
}

function updateSelectedZoneControls() {
    const zone = state.zones[state.selectedZone];
    if (!zone) return;

    const zoneColors = ['var(--zone-1)', 'var(--zone-2)', 'var(--zone-3)', 'var(--zone-4)'];
    const zoneNames = ['Center', 'Middle', 'Outer', 'Edge'];

    // Update selected zone indicator
    const dot = document.getElementById('selected-zone-dot');
    const name = document.getElementById('selected-zone-name');
    if (dot) dot.style.background = zoneColors[state.selectedZone];
    if (name) name.textContent = `Zone ${state.selectedZone} - ${zoneNames[state.selectedZone]}`;

    // Update zone effect dropdown
    const effectSelect = document.getElementById('zone-effect-select');
    if (effectSelect && state.effects.length > 0) {
        effectSelect.value = zone.effectId;
    }

    // Update zone sliders
    const brightnessSlider = document.getElementById('zone-brightness-slider');
    const brightnessValue = document.getElementById('zone-brightness-value');
    const speedSlider = document.getElementById('zone-speed-slider');
    const speedValue = document.getElementById('zone-speed-value');

    if (brightnessSlider) brightnessSlider.value = zone.brightness;
    if (brightnessValue) brightnessValue.textContent = zone.brightness;
    if (speedSlider) speedSlider.value = zone.speed;
    if (speedValue) speedValue.textContent = zone.speed;

    // Highlight selected zone channel
    document.querySelectorAll('.zone-channel').forEach((ch, i) => {
        ch.classList.toggle('ring-2', i === state.selectedZone);
        ch.classList.toggle('ring-[var(--gold)]', i === state.selectedZone);
    });
}

function selectZone(zoneId) {
    if (zoneId >= state.zoneCount) return;
    state.selectedZone = zoneId;
    updateSelectedZoneControls();
}

function setZoneCount(count) {
    state.zoneCount = count;
    sendWS({ type: 'zone.setCount', count });
    updateZoneMixer();
}

function toggleZoneSystem() {
    state.zoneEnabled = !state.zoneEnabled;
    sendWS({ type: 'zone.enable', enable: state.zoneEnabled });
    updateZoneMixer();
}

let zoneBrightnessTimeout, zoneSpeedTimeout;

function setZoneBrightness(value) {
    const zoneId = state.selectedZone;
    state.zones[zoneId].brightness = parseInt(value);
    document.getElementById('zone-brightness-value').textContent = value;

    // Update bar immediately
    const bar = document.querySelector(`.zone-bar[data-zone="${zoneId}"]`);
    if (bar) bar.style.height = `${Math.max(5, value / 255 * 100)}%`;

    clearTimeout(zoneBrightnessTimeout);
    zoneBrightnessTimeout = setTimeout(() => {
        sendWS({ type: 'zone.setBrightness', zoneId, brightness: parseInt(value) });
    }, CONFIG.DEBOUNCE_SLIDER);
}

function setZoneSpeed(value) {
    const zoneId = state.selectedZone;
    state.zones[zoneId].speed = parseInt(value);
    document.getElementById('zone-speed-value').textContent = value;

    clearTimeout(zoneSpeedTimeout);
    zoneSpeedTimeout = setTimeout(() => {
        sendWS({ type: 'zone.setSpeed', zoneId, speed: parseInt(value) });
    }, CONFIG.DEBOUNCE_SLIDER);
}

function setZoneEffect(effectId) {
    const zoneId = state.selectedZone;
    state.zones[zoneId].effectId = parseInt(effectId);
    sendWS({ type: 'zone.setEffect', zoneId, effectId: parseInt(effectId) });
}

function populateZoneEffectDropdown() {
    const select = document.getElementById('zone-effect-select');
    if (!select || state.effects.length === 0) return;

    select.innerHTML = state.effects.map(e =>
        `<option value="${e.id}">${e.name}</option>`
    ).join('');

    // Set current value
    const zone = state.zones[state.selectedZone];
    if (zone) select.value = zone.effectId;
}

function updatePerformanceTab() {
    // Update gauge values - these would need SVG stroke-dashoffset updates
    // For now just update text values
    const elements = {
        fps: state.fps,
        cpu: state.cpuUsage,
        heap: Math.round(state.freeHeap / 1024),
        frag: state.fragmentation,
    };

    // Update if elements exist
    Object.entries(elements).forEach(([key, value]) => {
        const el = document.getElementById(`perf-${key}`);
        if (el) el.textContent = value;
    });
}

// ========== UI Population ==========
async function loadEffects() {
    try {
        const data = await API.getEffects();
        state.effects = data.effects || [];
        populateEffectList();
        populateEffectsGrid();
        populateZoneEffectDropdown();
        console.log(`Loaded ${state.effects.length} effects`);
    } catch (e) {
        console.error('Load effects:', e);
    }
}

async function loadPalettes() {
    try {
        const data = await API.getPalettes();
        state.palettes = data.palettes || [];

        // Add gradient info (firmware doesn't send this, so we use defaults)
        const defaultGradients = {
            'Ocean Breeze': 'linear-gradient(90deg, #0a0607, #016f8f, #91d1ff)',
            'Sunset Real': 'linear-gradient(90deg, #120126, #4a0d67, #f0451e, #ffaa00)',
            'Fire': 'linear-gradient(90deg, #000, #8b0000, #ff4500, #ffff00)',
            'Lava': 'linear-gradient(90deg, #000, #8b0000, #ff4500, #ffd700)',
        };

        state.palettes.forEach(p => {
            p.gradient = defaultGradients[p.name] ||
                'linear-gradient(90deg, #333, #666, #999)';
        });

        populatePaletteGrid();
        updateCurrentPalette();
        console.log(`Loaded ${state.palettes.length} palettes`);
    } catch (e) {
        console.error('Load palettes:', e);
    }
}

async function loadStatus() {
    try {
        const data = await API.getStatus();
        syncStatus(data);
    } catch (e) {
        console.error('Load status:', e);
    }
}

async function loadZoneStatus() {
    try {
        const data = await API.getZoneStatus();
        syncZoneState(data);
    } catch (e) {
        console.error('Load zone status:', e);
    }
}

function populateEffectList() {
    const container = document.getElementById('effect-list');
    if (!container) return;

    const filtered = state.currentCategory === 'all'
        ? state.effects
        : state.effects.filter(e => e.category === state.currentCategory);

    container.innerHTML = filtered.slice(0, 15).map(e => `
        <div class="effect-item ${e.id === state.effectId ? 'active' : ''}"
             data-id="${e.id}" onclick="selectEffect(${e.id})">
            <div class="flex items-center justify-between">
                <div>
                    <div class="text-sm font-medium">${e.name}</div>
                    <div class="text-[10px] text-[var(--text-muted)] capitalize">${e.category || 'classic'}</div>
                </div>
                ${e.id === state.effectId ? '<span class="iconify text-[var(--gold)]" data-icon="lucide:check" data-width="14"></span>' : ''}
            </div>
        </div>
    `).join('');
}

function populateEffectsGrid() {
    const container = document.getElementById('effects-grid');
    if (!container) return;

    const filtered = state.currentCategory === 'all'
        ? state.effects
        : state.effects.filter(e => e.category === state.currentCategory);

    container.innerHTML = filtered.map(e => `
        <div class="effect-item ${e.id === state.effectId ? 'active' : ''}"
             data-id="${e.id}" onclick="selectEffect(${e.id})">
            <div class="text-sm font-medium">${e.name}</div>
            <div class="text-[10px] text-[var(--text-muted)] capitalize">${e.category || 'classic'}</div>
        </div>
    `).join('');
}

function populatePaletteGrid() {
    const container = document.getElementById('palette-grid');
    if (!container) return;

    container.innerHTML = state.palettes.map(p => `
        <div class="palette-swatch ${p.id === state.paletteId ? 'selected' : ''}"
             data-id="${p.id}" onclick="selectPalette(${p.id})"
             style="background: ${p.gradient};">
            <span class="palette-name">${p.name}</span>
        </div>
    `).join('');
}

// ========== Control Handlers ==========
let brightnessTimeout, speedTimeout;

function setBrightness(value) {
    state.brightness = parseInt(value);
    document.getElementById('brightness-value').textContent = state.brightness;

    clearTimeout(brightnessTimeout);
    brightnessTimeout = setTimeout(() => {
        sendWS({ type: 'setBrightness', value: state.brightness });
    }, CONFIG.DEBOUNCE_SLIDER);
}

function setSpeed(value) {
    state.speed = parseInt(value);
    document.getElementById('speed-value').textContent = state.speed;

    clearTimeout(speedTimeout);
    speedTimeout = setTimeout(() => {
        sendWS({ type: 'setSpeed', value: state.speed });
    }, CONFIG.DEBOUNCE_SLIDER);
}

function selectEffect(id) {
    state.effectId = id;
    const effect = state.effects.find(e => e.id === id);
    if (effect) state.effectName = effect.name;

    sendWS({ type: 'setEffect', effectId: id });

    updateNowPlaying();
    populateEffectList();
    populateEffectsGrid();
}

function selectPalette(id) {
    state.paletteId = id;
    sendWS({ type: 'setPalette', paletteId: id });

    updateCurrentPalette();
    populatePaletteGrid();
}

function filterEffects(category) {
    state.currentCategory = category;

    // Update chip states
    document.querySelectorAll('.chip[data-category]').forEach(chip => {
        chip.classList.toggle('active', chip.dataset.category === category);
    });

    populateEffectList();
    populateEffectsGrid();
}

// ========== Tab Navigation ==========
function switchTab(tabId) {
    state.currentTab = tabId;

    // Update nav items
    document.querySelectorAll('.nav-item[data-tab]').forEach(item => {
        item.classList.toggle('active', item.dataset.tab === tabId);
    });

    // Update tab content
    document.querySelectorAll('.tab-content').forEach(tab => {
        tab.classList.toggle('active', tab.id === `tab-${tabId}`);
    });
}

// ========== Preset Handlers ==========
const PRESET_NAMES = ['Unified', 'Dual Split', 'Triple Rings', 'Quad Active', 'LGP Showcase'];

function selectPreset(index) {
    document.querySelectorAll('.preset-btn').forEach((btn, i) => {
        if (btn.querySelector('.iconify')) return; // Skip save button
        btn.classList.toggle('active', i === index);
    });

    // Load zone preset via WebSocket for faster response
    sendWS({ type: 'zone.loadPreset', presetId: index });
    showToast(`Loading preset: ${PRESET_NAMES[index] || `P${index}`}`, 'info');
}

function saveZoneConfig() {
    sendWS({ type: 'zone.save' });
    showToast('Zone configuration saved', 'success');
}

// ========== Toast Notifications ==========
function showToast(message, type = 'info') {
    // Create toast element if needed
    let container = document.getElementById('toast-container');
    if (!container) {
        container = document.createElement('div');
        container.id = 'toast-container';
        container.style.cssText = 'position:fixed;bottom:20px;right:20px;z-index:9999;';
        document.body.appendChild(container);
    }

    const toast = document.createElement('div');
    const bgColor = type === 'error' ? 'var(--error)' : type === 'warning' ? 'var(--gold)' : 'var(--info)';
    toast.style.cssText = `
        background: ${bgColor};
        color: ${type === 'warning' ? 'black' : 'white'};
        padding: 12px 20px;
        border-radius: 8px;
        margin-top: 8px;
        font-size: 14px;
        font-family: var(--font-tech);
        box-shadow: 0 4px 12px rgba(0,0,0,0.3);
        animation: slideIn 0.3s ease;
    `;
    toast.textContent = message;
    container.appendChild(toast);

    setTimeout(() => {
        toast.style.animation = 'slideOut 0.3s ease';
        setTimeout(() => toast.remove(), 300);
    }, 3000);
}

// ========== Canvas Visualization ==========
let vizCanvas, vizCtx;

function setupCanvas() {
    vizCanvas = document.getElementById('viz-canvas');
    if (!vizCanvas) return;

    vizCtx = vizCanvas.getContext('2d');
    const dpr = window.devicePixelRatio || 1;
    const rect = vizCanvas.getBoundingClientRect();
    vizCanvas.width = rect.width * dpr;
    vizCanvas.height = rect.height * dpr;
    vizCtx.scale(dpr, dpr);
}

function drawVisualizer() {
    if (!vizCanvas || !vizCtx) return;

    const width = vizCanvas.width / (window.devicePixelRatio || 1);
    const height = vizCanvas.height / (window.devicePixelRatio || 1);
    vizCtx.clearRect(0, 0, width, height);

    vizCtx.fillStyle = '#0a0a0a';
    vizCtx.fillRect(0, 0, width, height);

    const ledCount = 160;
    const center = ledCount / 2;
    const ledW = width / ledCount;
    const ledH = height * 0.6;
    const stripY = height / 2;
    const time = Date.now() / 1000;

    const colors = ['#4DFFB8', '#FFB84D', '#4D88FF', '#FF4D88'];

    for (let i = 0; i < ledCount; i++) {
        const distFromCenter = Math.abs(i - center);
        const normalizedDist = distFromCenter / center;

        // Zone assignment
        let colorIndex, baseAlpha;
        if (distFromCenter < 20) {
            colorIndex = 0;
            baseAlpha = 0.9;
        } else if (distFromCenter < 40) {
            colorIndex = 1;
            baseAlpha = 0.7;
        } else if (distFromCenter < 60) {
            colorIndex = 2;
            baseAlpha = 0.5;
        } else {
            colorIndex = 3;
            baseAlpha = 0.3;
        }

        // CENTER ORIGIN animation
        const wavePhase = distFromCenter * 0.15 - time * 3;
        const wave = (Math.sin(wavePhase) + 1) / 2;
        const pulsePhase = distFromCenter * 0.08 - time * 2;
        const pulse = (Math.sin(pulsePhase) + 1) / 2;

        const intensity = 0.2 + 0.8 * wave * (0.5 + 0.5 * pulse);
        const finalAlpha = baseAlpha * intensity * (state.brightness / 255);

        vizCtx.fillStyle = colors[colorIndex];
        vizCtx.globalAlpha = Math.max(0.05, finalAlpha);
        vizCtx.fillRect(i * ledW + 0.5, stripY - ledH/2, ledW - 1, ledH);
    }

    // Center marker
    const centerX = center * ledW;
    const gradient = vizCtx.createRadialGradient(centerX, stripY, 0, centerX, stripY, 40);
    gradient.addColorStop(0, 'rgba(77, 255, 184, 0.3)');
    gradient.addColorStop(1, 'rgba(77, 255, 184, 0)');
    vizCtx.globalAlpha = 0.5 + 0.3 * Math.sin(time * 2);
    vizCtx.fillStyle = gradient;
    vizCtx.fillRect(centerX - 40, stripY - 40, 80, 80);

    vizCtx.globalAlpha = 1;
    requestAnimationFrame(drawVisualizer);
}

// ========== Initialization ==========
async function init() {
    console.log('LightwaveOS V3 Dashboard initializing...');

    // Setup canvas
    setupCanvas();
    requestAnimationFrame(drawVisualizer);

    // Connect WebSocket
    connectWebSocket();

    // Load initial data
    await Promise.all([
        loadEffects(),
        loadPalettes(),
        loadStatus(),
        loadZoneStatus(),
    ]);

    // Setup event listeners
    setupEventListeners();

    console.log('Dashboard initialized');
}

function setupEventListeners() {
    // Tab navigation
    document.querySelectorAll('.nav-item[data-tab]').forEach(item => {
        item.addEventListener('click', () => switchTab(item.dataset.tab));
    });

    // Sliders
    const brightnessSlider = document.getElementById('brightness-slider');
    const speedSlider = document.getElementById('speed-slider');

    if (brightnessSlider) {
        brightnessSlider.addEventListener('input', (e) => setBrightness(e.target.value));
    }
    if (speedSlider) {
        speedSlider.addEventListener('input', (e) => setSpeed(e.target.value));
    }

    // Category chips
    document.querySelectorAll('.chip[data-category]').forEach(chip => {
        chip.addEventListener('click', () => filterEffects(chip.dataset.category));
    });

    // Preset buttons
    let presetIndex = 0;
    document.querySelectorAll('.preset-btn').forEach((btn) => {
        if (btn.querySelector('.iconify[data-icon="lucide:save"]')) {
            // Save button
            btn.addEventListener('click', () => saveZoneConfig());
        } else {
            // Preset button
            const idx = presetIndex++;
            btn.addEventListener('click', () => selectPreset(idx));
            btn.title = PRESET_NAMES[idx] || `Preset ${idx}`;
        }
    });

    // Toggle switches
    document.querySelectorAll('.toggle').forEach(toggle => {
        toggle.addEventListener('click', () => {
            // Handle specific toggles
            if (toggle.id === 'zone-system-toggle') {
                toggleZoneSystem();
            } else if (toggle.id === 'transition-random') {
                toggle.classList.toggle('active');
                state.transitionEnabled = toggle.classList.contains('active');
                sendWS({ type: 'toggleTransitions', enabled: state.transitionEnabled });
            } else {
                toggle.classList.toggle('active');
            }
        });
    });

    // Zone count selector
    document.querySelectorAll('#zone-count-selector button').forEach(btn => {
        btn.addEventListener('click', () => setZoneCount(parseInt(btn.dataset.count)));
    });

    // Zone channel selection
    document.querySelectorAll('.zone-channel').forEach(channel => {
        channel.addEventListener('click', () => selectZone(parseInt(channel.dataset.zone)));
    });

    // Zone controls
    const zoneBrightnessSlider = document.getElementById('zone-brightness-slider');
    const zoneSpeedSlider = document.getElementById('zone-speed-slider');
    const zoneEffectSelect = document.getElementById('zone-effect-select');

    if (zoneBrightnessSlider) {
        zoneBrightnessSlider.addEventListener('input', (e) => setZoneBrightness(e.target.value));
    }
    if (zoneSpeedSlider) {
        zoneSpeedSlider.addEventListener('input', (e) => setZoneSpeed(e.target.value));
    }
    if (zoneEffectSelect) {
        zoneEffectSelect.addEventListener('change', (e) => setZoneEffect(e.target.value));
    }

    // Window resize
    window.addEventListener('resize', setupCanvas);
}

// Start when DOM ready
document.addEventListener('DOMContentLoaded', init);

// Add toast animation styles
const style = document.createElement('style');
style.textContent = `
    @keyframes slideIn {
        from { transform: translateX(100%); opacity: 0; }
        to { transform: translateX(0); opacity: 1; }
    }
    @keyframes slideOut {
        from { transform: translateX(0); opacity: 1; }
        to { transform: translateX(100%); opacity: 0; }
    }
`;
document.head.appendChild(style);
