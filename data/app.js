// K1-Lightwave Controller Web App
// Vanilla JavaScript - WebSocket + REST API

// ========== Configuration ==========
const CONFIG = {
    WS_URL: `ws://${window.location.hostname}/ws`,
    API_BASE: '/api',
    RECONNECT_INTERVAL: 3000,
    DEBOUNCE_SLIDER: 150,
    DEBOUNCE_PIPELINE: 250,
};

// ========== State ==========
const state = {
    ws: null,
    connected: false,
    reconnectTimer: null,
    zoneCount: 3,
    selectedZone: 1,
    zones: [
        { id: 1, effectId: 0, enabled: true, brightness: 255, speed: 25 },
        { id: 2, effectId: 0, enabled: true, brightness: 255, speed: 25 },
        { id: 3, effectId: 0, enabled: true, brightness: 255, speed: 25 },
        { id: 4, effectId: 0, enabled: true, brightness: 255, speed: 25 },
    ],
    paletteId: 0,
    transitions: false,
    effects: [],
    palettes: [],
    fps: 0,
    memoryFree: 0,
};

// Zone display configuration
const zoneConfig = {
    1: { color: 'accent-zone1', text: 'LEDs 65-94', visIds: ['vis-z1'] },
    2: { color: 'accent-zone2', text: 'LEDs 20-64 | 95-139', visIds: ['vis-z2-l', 'vis-z2-r'] },
    3: { color: 'accent-zone3', text: 'LEDs 0-19 | 140-159', visIds: ['vis-z3-l', 'vis-z3-r'] },
    4: { color: 'accent-zone3', text: 'LEDs (Zone 4 range)', visIds: ['vis-z3-l', 'vis-z3-r'] },
};

// ========== API Client ==========
const API = {
    async request(endpoint, options = {}) {
        try {
            const res = await fetch(`${CONFIG.API_BASE}${endpoint}`, {
                ...options,
                headers: { 'Content-Type': 'application/json', ...options.headers },
            });
            if (!res.ok) throw new Error(`HTTP ${res.status}`);
            return await res.json();
        } catch (e) {
            console.error(`API [${endpoint}]:`, e);
            return null;
        }
    },
    setBrightness: (v) => API.request('/brightness', { method: 'POST', body: JSON.stringify({ brightness: v }) }),
    setSpeed: (v) => API.request('/speed', { method: 'POST', body: JSON.stringify({ speed: v }) }),
    setPalette: (id) => API.request('/palette', { method: 'POST', body: JSON.stringify({ paletteId: id }) }),
    setPipeline: (i, s, c, v) => API.request('/pipeline', { method: 'POST', body: JSON.stringify({ intensity: i, saturation: s, complexity: c, variation: v }) }),
    toggleTransitions: (e) => API.request('/transitions', { method: 'POST', body: JSON.stringify({ enabled: e }) }),
    setZoneCount: (c) => API.request('/zone/count', { method: 'POST', body: JSON.stringify({ count: c }) }),
    setZoneEffect: (z, e) => API.request('/zone/config', { method: 'POST', body: JSON.stringify({ zoneId: z, effectId: e, enabled: true }) }),
    setZoneEnabled: (z, e) => API.request('/zone/config', { method: 'POST', body: JSON.stringify({ zoneId: z, enabled: e }) }),
    setZoneBrightness: (z, b) => API.request('/zone/brightness', { method: 'POST', body: JSON.stringify({ zoneId: z, brightness: b }) }),
    setZoneSpeed: (z, s) => API.request('/zone/speed', { method: 'POST', body: JSON.stringify({ zoneId: z, speed: s }) }),
    loadPreset: (id) => API.request('/zone/preset/load', { method: 'POST', body: JSON.stringify({ presetId: id }) }),
    saveConfig: () => API.request('/zone/config/save', { method: 'POST' }),
    getStatus: () => API.request('/status'),

    // Enhancement Engine APIs
    setColorBlend: (enabled) => API.request('/enhancement/color/blend', { method: 'POST', body: JSON.stringify({ enabled }) }),
    setColorDiffusion: (amount) => API.request('/enhancement/color/diffusion', { method: 'POST', body: JSON.stringify({ amount }) }),
    setColorRotation: (speed) => API.request('/enhancement/color/rotation', { method: 'POST', body: JSON.stringify({ speed }) }),
    setMotionPhase: (offset) => API.request('/enhancement/motion/phase', { method: 'POST', body: JSON.stringify({ offset }) }),
    setMotionAutoRotate: (enabled, speed) => API.request('/enhancement/motion/auto-rotate', { method: 'POST', body: JSON.stringify({ enabled, speed }) }),
    getEnhancementStatus: () => API.request('/enhancement/status'),
};

// ========== WebSocket ==========
const WS = {
    connect() {
        if (state.ws) state.ws.close();
        console.log('WS connecting:', CONFIG.WS_URL);
        state.ws = new WebSocket(CONFIG.WS_URL);

        state.ws.onopen = () => {
            console.log('WS connected');
            state.connected = true;
            updateConnectionStatus();
            clearTimeout(state.reconnectTimer);
            WS.send({ type: 'getState' });
        };

        state.ws.onmessage = (e) => {
            try { WS.handleMessage(JSON.parse(e.data)); }
            catch (err) { console.error('WS parse:', err); }
        };

        state.ws.onerror = (e) => console.error('WS error:', e);

        state.ws.onclose = () => {
            console.log('WS disconnected');
            state.connected = false;
            updateConnectionStatus();
            state.reconnectTimer = setTimeout(() => WS.connect(), CONFIG.RECONNECT_INTERVAL);
        };
    },

    send(msg) {
        if (state.ws && state.ws.readyState === WebSocket.OPEN) {
            state.ws.send(JSON.stringify(msg));
        }
    },

    handleMessage(msg) {
        console.log('WS <-', msg);
        switch (msg.type) {
            case 'status':
            case 'performance':
                updatePerformance(msg.fps, msg.freeHeap || msg.memoryFree);
                break;
            case 'effectChange':
                updateCurrentEffect(msg.effectId, msg.name);
                break;
            case 'paletteChange':
                state.paletteId = msg.paletteId;
                document.getElementById('palette-select').value = msg.paletteId;
                break;
            case 'zone.state':
                // Sync zone state from server
                break;
            case 'error':
                console.error('Server:', msg.message);
                break;
        }
    }
};

// ========== UI Updates ==========
function updateConnectionStatus() {
    const txt = document.getElementById('status-text');
    const dot = document.getElementById('status-dot');
    if (state.connected) {
        txt.textContent = 'ONLINE';
        dot.className = 'w-2 h-2 rounded-full bg-green-500 shadow-[0_0_8px_rgba(34,221,136,0.6)] animate-pulse';
    } else {
        txt.textContent = 'OFFLINE';
        dot.className = 'w-2 h-2 rounded-full bg-red-500 shadow-[0_0_8px_rgba(255,85,85,0.6)]';
    }
}

function updatePerformance(fps, mem) {
    if (fps !== undefined) document.getElementById('status-fps').textContent = fps;
    if (mem !== undefined) document.getElementById('status-mem').textContent = `${Math.round(mem / 1024)}KB`;
}

function updateCurrentEffect(id, name) {
    document.getElementById('status-fx').textContent = name || `Effect ${id}`;
}

function updateSliderTrack(input) {
    const min = parseFloat(input.min) || 0;
    const max = parseFloat(input.max) || 100;
    const val = parseFloat(input.value);
    const pct = ((val - min) / (max - min)) * 100;
    input.style.background = `linear-gradient(to right, #00d4ff ${pct}%, #2f3849 ${pct}%)`;
}

// ========== Zone Logic ==========
function setZoneCount(count) {
    state.zoneCount = count;

    // Update zone count buttons
    for (let i = 1; i <= 4; i++) {
        const btn = document.getElementById(`zc-${i}`);
        if (btn) {
            btn.className = i === count
                ? 'w-6 h-6 text-xs text-black bg-accent-cyan font-semibold rounded'
                : 'w-6 h-6 text-xs text-text-secondary hover:text-white rounded transition-colors';
        }
    }

    // Update zone tabs visibility
    updateZoneTabs(count);

    // Update mixer channels
    updateMixerChannels(count);

    // If selected zone is now hidden, select zone 1
    if (state.selectedZone > count) {
        selectZone(1);
    }

    API.setZoneCount(count);
}

function updateZoneTabs(count) {
    const tab4 = document.getElementById('tab-z4');
    const tab3 = document.getElementById('tab-z3');

    if (tab4) tab4.classList.toggle('hidden', count < 4);
    if (tab3) tab3.classList.toggle('hidden', count < 3);
}

function updateMixerChannels(count) {
    // Zone 4 channel
    const z4 = document.getElementById('mixer-zone-4');
    if (z4) {
        if (count >= 4) {
            z4.classList.remove('opacity-30', 'pointer-events-none');
            z4.querySelectorAll('input').forEach(i => i.disabled = false);
        } else {
            z4.classList.add('opacity-30', 'pointer-events-none');
            z4.querySelectorAll('input').forEach(i => i.disabled = true);
        }
    }

    // Zone 3 channel
    const z3 = document.getElementById('mixer-zone-3');
    if (z3) {
        if (count >= 3) {
            z3.classList.remove('opacity-30', 'pointer-events-none');
            z3.querySelectorAll('input').forEach(i => i.disabled = false);
        } else {
            z3.classList.add('opacity-30', 'pointer-events-none');
            z3.querySelectorAll('input').forEach(i => i.disabled = true);
        }
    }
}

function selectZone(id) {
    state.selectedZone = id;
    const cfg = zoneConfig[id];

    // Update tabs
    for (let z = 1; z <= 4; z++) {
        const tab = document.getElementById(`tab-z${z}`);
        if (tab) {
            const zCfg = zoneConfig[z];
            if (z === id) {
                tab.className = `flex-1 pb-2 text-sm font-semibold text-text-primary flex justify-center items-center gap-1.5 border-b-2 border-${zCfg.color}`;
                tab.setAttribute('aria-selected', 'true');
            } else {
                tab.className = `flex-1 pb-2 text-sm text-text-secondary hover:text-text-primary transition-colors flex justify-center items-center gap-1.5 border-b-2 border-transparent${z > state.zoneCount ? ' hidden' : ''}`;
                tab.setAttribute('aria-selected', 'false');
            }
        }
    }

    // Update LED range badge
    const badge = document.getElementById('range-badge');
    if (badge) {
        badge.className = `w-full py-2 px-3 rounded text-xs font-mono font-medium text-center border bg-${cfg.color}/10 text-${cfg.color} border-${cfg.color}/20`;
        badge.textContent = cfg.text;
    }

    // Update visualization overlays
    document.querySelectorAll('.zone-overlay').forEach(el => el.style.opacity = '0');
    cfg.visIds.forEach(visId => {
        const el = document.getElementById(visId);
        if (el) el.style.opacity = '0.5';
    });

    // Load zone-specific values into sliders
    const zone = state.zones[id - 1];
    if (zone) {
        const bSlider = document.getElementById('zone-brightness-slider');
        const bLabel = document.getElementById('zone-brightness-label');
        const sSlider = document.getElementById('zone-speed-slider');
        const sLabel = document.getElementById('zone-speed-label');

        if (bSlider) { bSlider.value = zone.brightness; updateSliderTrack(bSlider); }
        if (bLabel) bLabel.textContent = zone.brightness;
        if (sSlider) { sSlider.value = zone.speed; updateSliderTrack(sSlider); }
        if (sLabel) sLabel.textContent = zone.speed;
    }
}

// ========== Zone Controls ==========
let zoneBrightnessTimeout, zoneSpeedTimeout;

function setZoneBrightness(value) {
    const zoneId = state.selectedZone - 1;
    const brightness = parseInt(value);

    if (state.zones[zoneId]) state.zones[zoneId].brightness = brightness;

    document.getElementById('zone-brightness-label').textContent = brightness;
    updateSliderTrack(document.getElementById('zone-brightness-slider'));

    // Sync mixer
    const mixerLabel = document.getElementById(`mixer-b-label-${zoneId}`);
    const mixerSlider = document.getElementById(`mixer-b-${zoneId}`);
    if (mixerLabel) mixerLabel.textContent = brightness;
    if (mixerSlider) mixerSlider.value = brightness;

    updateZoneVisualization();

    clearTimeout(zoneBrightnessTimeout);
    zoneBrightnessTimeout = setTimeout(() => API.setZoneBrightness(zoneId, brightness), CONFIG.DEBOUNCE_SLIDER);
}

function setZoneSpeed(value) {
    const zoneId = state.selectedZone - 1;
    const speed = parseInt(value);

    if (state.zones[zoneId]) state.zones[zoneId].speed = speed;

    document.getElementById('zone-speed-label').textContent = speed;
    updateSliderTrack(document.getElementById('zone-speed-slider'));

    // Sync mixer
    const mixerLabel = document.getElementById(`mixer-s-label-${zoneId}`);
    const mixerSlider = document.getElementById(`mixer-s-${zoneId}`);
    if (mixerLabel) mixerLabel.textContent = speed;
    if (mixerSlider) mixerSlider.value = speed;

    clearTimeout(zoneSpeedTimeout);
    zoneSpeedTimeout = setTimeout(() => API.setZoneSpeed(zoneId, speed), CONFIG.DEBOUNCE_SLIDER);
}

function updateZoneVisualization() {
    for (let i = 0; i < state.zones.length; i++) {
        const zone = state.zones[i];
        const cfg = zoneConfig[i + 1];
        if (cfg && cfg.visIds) {
            const opacity = zone.enabled ? zone.brightness / 255 : 0;
            cfg.visIds.forEach(id => {
                const el = document.getElementById(id);
                if (el && i + 1 === state.selectedZone) el.style.opacity = opacity;
            });
        }
    }
}

// ========== Mixer Controls ==========
const mixerTimeouts = { b: {}, s: {} };

function setMixerBrightness(zoneId, value) {
    const brightness = parseInt(value);

    if (state.zones[zoneId]) state.zones[zoneId].brightness = brightness;

    document.getElementById(`mixer-b-label-${zoneId}`).textContent = brightness;

    // Sync zone card if this zone is selected
    if (zoneId === state.selectedZone - 1) {
        document.getElementById('zone-brightness-label').textContent = brightness;
        const slider = document.getElementById('zone-brightness-slider');
        if (slider) { slider.value = brightness; updateSliderTrack(slider); }
    }

    updateZoneVisualization();

    clearTimeout(mixerTimeouts.b[zoneId]);
    mixerTimeouts.b[zoneId] = setTimeout(() => API.setZoneBrightness(zoneId, brightness), CONFIG.DEBOUNCE_SLIDER);
}

function setMixerSpeed(zoneId, value) {
    const speed = parseInt(value);

    if (state.zones[zoneId]) state.zones[zoneId].speed = speed;

    document.getElementById(`mixer-s-label-${zoneId}`).textContent = speed;

    // Sync zone card if this zone is selected
    if (zoneId === state.selectedZone - 1) {
        document.getElementById('zone-speed-label').textContent = speed;
        const slider = document.getElementById('zone-speed-slider');
        if (slider) { slider.value = speed; updateSliderTrack(slider); }
    }

    clearTimeout(mixerTimeouts.s[zoneId]);
    mixerTimeouts.s[zoneId] = setTimeout(() => API.setZoneSpeed(zoneId, speed), CONFIG.DEBOUNCE_SLIDER);
}

// ========== Effect Navigation ==========
function prevEffect() {
    const sel = document.getElementById('effect-select');
    if (sel && sel.selectedIndex > 0) {
        sel.selectedIndex--;
        setZoneEffect();
    }
}

function nextEffect() {
    const sel = document.getElementById('effect-select');
    if (sel && sel.selectedIndex < sel.options.length - 1) {
        sel.selectedIndex++;
        setZoneEffect();
    }
}

function setZoneEffect() {
    const sel = document.getElementById('effect-select');
    if (!sel) return;
    API.setZoneEffect(state.selectedZone - 1, parseInt(sel.value));
}

function enableZone() {
    const zoneId = state.selectedZone - 1;
    if (state.zones[zoneId]) state.zones[zoneId].enabled = true;
    updateZoneVisualization();
    API.setZoneEnabled(zoneId, true);
}

function disableZone() {
    const zoneId = state.selectedZone - 1;
    if (state.zones[zoneId]) state.zones[zoneId].enabled = false;
    updateZoneVisualization();
    API.setZoneEnabled(zoneId, false);
}

// ========== Global Settings ==========
function setPalette() {
    const sel = document.getElementById('palette-select');
    if (sel) {
        state.paletteId = parseInt(sel.value);
        API.setPalette(state.paletteId);
    }
}

function toggleTransitions() {
    const toggle = document.getElementById('toggle-transitions');
    if (toggle) {
        state.transitions = toggle.checked;
        API.toggleTransitions(state.transitions);
    }
}

// ========== Pipeline ==========
let pipelineTimeout;

function updatePipeline(input, labelId) {
    document.getElementById(labelId).textContent = input.value;
    updateSliderTrack(input);

    clearTimeout(pipelineTimeout);
    pipelineTimeout = setTimeout(() => {
        const i = parseInt(document.getElementById('slider-intensity').value);
        const s = parseInt(document.getElementById('slider-saturation').value);
        const c = parseInt(document.getElementById('slider-complexity').value);
        const v = parseInt(document.getElementById('slider-variation').value);
        API.setPipeline(i, s, c, v);
    }, CONFIG.DEBOUNCE_PIPELINE);
}

function togglePipeline() {
    const content = document.getElementById('pipeline-content');
    const chevron = document.getElementById('pipeline-chevron');
    if (!content) return;

    if (content.style.maxHeight && content.style.maxHeight !== '0px') {
        content.style.maxHeight = '0px';
        if (chevron) chevron.style.transform = 'rotate(0deg)';
    } else {
        content.style.maxHeight = content.scrollHeight + 'px';
        if (chevron) chevron.style.transform = 'rotate(180deg)';
    }
}

// ========== Enhancement Engines ==========
function toggleEnhancements() {
    const content = document.getElementById('enhancements-content');
    const chevron = document.getElementById('enhancements-chevron');
    if (!content) return;

    if (content.style.maxHeight && content.style.maxHeight !== '0px') {
        content.style.maxHeight = '0px';
        if (chevron) chevron.style.transform = 'rotate(0deg)';
    } else {
        content.style.maxHeight = content.scrollHeight + 'px';
        if (chevron) chevron.style.transform = 'rotate(180deg)';
    }
}

let diffusionTimeout, rotationTimeout, phaseTimeout, autoRotateSpeedTimeout;

function toggleCrossBlend() {
    const toggle = document.getElementById('toggle-blend');
    if (toggle) {
        API.setColorBlend(toggle.checked);
    }
}

function updateDiffusion(input) {
    const value = parseInt(input.value);
    document.getElementById('val-diffusion').textContent = value;
    updateSliderTrack(input);

    clearTimeout(diffusionTimeout);
    diffusionTimeout = setTimeout(() => API.setColorDiffusion(value), CONFIG.DEBOUNCE_SLIDER);
}

function updateRotation(input) {
    const value = parseInt(input.value);
    const speed = value / 10.0; // Convert to float (0.0-10.0 degrees/frame)
    document.getElementById('val-rotation').textContent = speed.toFixed(1);
    updateSliderTrack(input);

    clearTimeout(rotationTimeout);
    rotationTimeout = setTimeout(() => API.setColorRotation(speed), CONFIG.DEBOUNCE_SLIDER);
}

function updatePhase(input) {
    const value = parseInt(input.value);
    document.getElementById('val-phase').textContent = value;
    updateSliderTrack(input);

    clearTimeout(phaseTimeout);
    phaseTimeout = setTimeout(() => API.setMotionPhase(value), CONFIG.DEBOUNCE_SLIDER);
}

function toggleAutoRotate() {
    const toggle = document.getElementById('toggle-autorotate');
    const speedContainer = document.getElementById('autorotate-speed-container');
    const speedSlider = document.getElementById('slider-autorotate-speed');

    if (toggle && speedContainer) {
        const enabled = toggle.checked;
        speedContainer.style.display = enabled ? 'flex' : 'none';

        // Recalculate max-height when showing/hiding auto-rotate speed
        const content = document.getElementById('enhancements-content');
        if (content && content.style.maxHeight !== '0px') {
            content.style.maxHeight = content.scrollHeight + 'px';
        }

        const speed = speedSlider ? parseFloat(speedSlider.value) : 10.0;
        API.setMotionAutoRotate(enabled, speed);
    }
}

function updateAutoRotateSpeed(input) {
    const value = parseFloat(input.value);
    document.getElementById('val-autorotate-speed').textContent = value;
    updateSliderTrack(input);

    clearTimeout(autoRotateSpeedTimeout);
    autoRotateSpeedTimeout = setTimeout(() => {
        const enabled = document.getElementById('toggle-autorotate')?.checked || false;
        API.setMotionAutoRotate(enabled, value);
    }, CONFIG.DEBOUNCE_SLIDER);
}

// ========== Presets ==========
function loadPreset(id) { API.loadPreset(id); }
function savePreset() { API.saveConfig(); }
function resetToDefaults() { setZoneCount(3); }

// ========== Data Loading ==========
async function loadEffects() {
    try {
        const res = await fetch(`${CONFIG.API_BASE}/effects?start=0&count=100`);
        if (!res.ok) return;
        const data = await res.json();
        state.effects = data.effects || [];

        const sel = document.getElementById('effect-select');
        if (sel) {
            sel.innerHTML = '';
            state.effects.forEach(fx => {
                const opt = document.createElement('option');
                opt.value = fx.id;
                opt.textContent = fx.name;
                sel.appendChild(opt);
            });
        }
        console.log(`Loaded ${state.effects.length} effects`);
    } catch (e) {
        console.error('Load effects:', e);
    }
}

async function loadPalettes() {
    try {
        const res = await fetch(`${CONFIG.API_BASE}/palettes`);
        if (!res.ok) return;
        const data = await res.json();
        state.palettes = data.palettes || [];

        const sel = document.getElementById('palette-select');
        if (sel) {
            sel.innerHTML = '';
            state.palettes.forEach(p => {
                const opt = document.createElement('option');
                opt.value = p.id;
                opt.textContent = p.name;
                sel.appendChild(opt);
            });
        }
        console.log(`Loaded ${state.palettes.length} palettes`);
    } catch (e) {
        console.error('Load palettes:', e);
    }
}

// ========== Initialization ==========
document.addEventListener('DOMContentLoaded', async () => {
    console.log('K1-Lightwave initializing...');

    // Initialize slider tracks
    document.querySelectorAll('.slider-input').forEach(updateSliderTrack);

    // Load data
    await loadEffects();
    await loadPalettes();

    // Connect WebSocket
    WS.connect();

    // Set initial states
    updateZoneTabs(state.zoneCount);
    updateMixerChannels(state.zoneCount);
    selectZone(1);
    updateConnectionStatus();

    // Sync mixer to initial state
    for (let i = 0; i < 4; i++) {
        const zone = state.zones[i];
        const bLabel = document.getElementById(`mixer-b-label-${i}`);
        const sLabel = document.getElementById(`mixer-s-label-${i}`);
        const bSlider = document.getElementById(`mixer-b-${i}`);
        const sSlider = document.getElementById(`mixer-s-${i}`);

        if (bLabel) bLabel.textContent = zone.brightness;
        if (sLabel) sLabel.textContent = zone.speed;
        if (bSlider) bSlider.value = zone.brightness;
        if (sSlider) sSlider.value = zone.speed;
    }

    console.log('K1-Lightwave initialized');
});
