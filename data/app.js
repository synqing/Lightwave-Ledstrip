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
        { id: 1, effectId: 0, enabled: true, brightness: 255, speed: 25, paletteId: 0 },
        { id: 2, effectId: 0, enabled: true, brightness: 255, speed: 25, paletteId: 0 },
        { id: 3, effectId: 0, enabled: true, brightness: 255, speed: 25, paletteId: 0 },
        { id: 4, effectId: 0, enabled: true, brightness: 255, speed: 25, paletteId: 0 },
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
    setZonePalette: (z, p) => API.request('/zone/palette', { method: 'POST', body: JSON.stringify({ zoneId: z, paletteId: p }) }),
    loadPreset: (id) => API.request('/zone/preset/load', { method: 'POST', body: JSON.stringify({ presetId: id }) }),
    saveConfig: () => API.request('/zone/config/save', { method: 'POST' }),
    getStatus: () => API.request('/status'),

    // User Preset APIs (Phase C.1)
    getUserPresets: () => API.request('/api/v1/presets/user'),
    saveUserPreset: (slot, name) => API.request('/api/v1/presets/user', { method: 'POST', body: JSON.stringify({ slot, name }) }),
    loadUserPreset: (slot) => API.request('/api/v1/presets/load', { method: 'POST', body: JSON.stringify({ type: 'user', slot }) }),
    deleteUserPreset: (slot) => API.request('/api/v1/presets/user', { method: 'DELETE', body: JSON.stringify({ slot }) }),

    // Effect Metadata API (Phase C.2)
    getEffectMetadata: (id) => API.request(`/api/v1/effects/metadata?id=${id}`),

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
                syncZoneState(msg);
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

// Sync zone state from server broadcast
function syncZoneState(msg) {
    // Update zone count if changed
    if (msg.zoneCount !== undefined && msg.zoneCount !== state.zoneCount) {
        state.zoneCount = msg.zoneCount;
        for (let i = 1; i <= 4; i++) {
            const btn = document.getElementById(`zc-${i}`);
            if (btn) {
                btn.className = i === msg.zoneCount
                    ? 'w-6 h-6 text-xs text-black bg-accent-cyan font-semibold rounded'
                    : 'w-6 h-6 text-xs text-text-secondary hover:text-white rounded transition-colors';
            }
        }
        updateZoneTabs(msg.zoneCount);
        updateMixerChannels(msg.zoneCount);
    }

    // Sync each zone's state
    if (msg.zones && Array.isArray(msg.zones)) {
        msg.zones.forEach((zoneData) => {
            const idx = zoneData.id;
            if (state.zones[idx]) {
                state.zones[idx].enabled = zoneData.enabled;
                state.zones[idx].effectId = zoneData.effectId;
                state.zones[idx].brightness = zoneData.brightness;
                state.zones[idx].speed = zoneData.speed;
                state.zones[idx].paletteId = zoneData.paletteId !== undefined ? zoneData.paletteId : 0;

                // Update mixer UI sliders and labels
                const bLabel = document.getElementById(`mixer-b-label-${idx}`);
                const sLabel = document.getElementById(`mixer-s-label-${idx}`);
                const bSlider = document.getElementById(`mixer-b-${idx}`);
                const sSlider = document.getElementById(`mixer-s-${idx}`);

                if (bLabel) bLabel.textContent = zoneData.brightness;
                if (sLabel) sLabel.textContent = zoneData.speed;
                if (bSlider) { bSlider.value = zoneData.brightness; updateSliderTrack(bSlider); }
                if (sSlider) { sSlider.value = zoneData.speed; updateSliderTrack(sSlider); }
            }
        });

        // Update zone card if selected zone was affected
        const zone = state.zones[state.selectedZone - 1];
        if (zone) {
            const bSlider = document.getElementById('zone-brightness-slider');
            const bLabel = document.getElementById('zone-brightness-label');
            const sSlider = document.getElementById('zone-speed-slider');
            const sLabel = document.getElementById('zone-speed-label');
            const effectSelect = document.getElementById('effect-select');
            const paletteSelect = document.getElementById('zone-palette-select');

            if (bSlider) { bSlider.value = zone.brightness; updateSliderTrack(bSlider); }
            if (bLabel) bLabel.textContent = zone.brightness;
            if (sSlider) { sSlider.value = zone.speed; updateSliderTrack(sSlider); }
            if (sLabel) sLabel.textContent = zone.speed;
            if (effectSelect) effectSelect.value = zone.effectId;
            if (paletteSelect) paletteSelect.value = zone.paletteId;

            // Fetch metadata for the selected zone's effect
            if (idx === state.selectedZone - 1) {
                fetchEffectMetadata(zone.effectId);
            }
        }
    }

    updateZoneVisualization();
    console.log('Zone state synced from server');
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
        const pSelect = document.getElementById('zone-palette-select');

        if (bSlider) { bSlider.value = zone.brightness; updateSliderTrack(bSlider); }
        if (bLabel) bLabel.textContent = zone.brightness;
        if (sSlider) { sSlider.value = zone.speed; updateSliderTrack(sSlider); }
        if (sLabel) sLabel.textContent = zone.speed;
        if (pSelect) pSelect.value = zone.paletteId;
    }
}

// ========== Zone Controls ==========
let zoneBrightnessTimeout, zoneSpeedTimeout, zonePaletteTimeout;

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

function setZonePalette(value) {
    const zoneId = state.selectedZone - 1;
    const paletteId = parseInt(value);

    if (state.zones[zoneId]) state.zones[zoneId].paletteId = paletteId;

    // Send via WebSocket for faster response
    sendWS({ type: 'zone.setPalette', zoneId, paletteId });
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
    const effectId = parseInt(sel.value);
    API.setZoneEffect(state.selectedZone - 1, effectId);
    fetchEffectMetadata(effectId);
}

// ========== Effect Info (Phase C.2) ==========
let effectInfoExpanded = false;

function toggleEffectInfo() {
    effectInfoExpanded = !effectInfoExpanded;
    const content = document.getElementById('effect-info-content');
    const chevron = document.getElementById('effect-info-chevron');
    if (content) content.classList.toggle('hidden', !effectInfoExpanded);
    if (chevron) chevron.style.transform = effectInfoExpanded ? 'rotate(180deg)' : '';
}

function fetchEffectMetadata(effectId) {
    API.getEffectMetadata(effectId).then(r => {
        if (r.success && r.data) {
            displayEffectMetadata(r.data);
        }
    }).catch(() => {
        // Silently fail - metadata is optional
    });
}

function displayEffectMetadata(meta) {
    const categoryEl = document.getElementById('effect-category');
    const descEl = document.getElementById('effect-description');
    const featuresEl = document.getElementById('effect-features');

    if (categoryEl) categoryEl.textContent = meta.category || '-';
    if (descEl) descEl.textContent = meta.description || '-';

    if (featuresEl && meta.features) {
        const badges = [];
        if (meta.features.centerOrigin) badges.push({ label: 'CENTER ORIGIN', color: 'bg-green-500/20 text-green-400' });
        if (meta.features.usesPalette) badges.push({ label: 'PALETTE', color: 'bg-purple-500/20 text-purple-400' });
        if (meta.features.usesSpeed) badges.push({ label: 'SPEED', color: 'bg-blue-500/20 text-blue-400' });
        if (meta.features.zoneAware) badges.push({ label: 'ZONE AWARE', color: 'bg-amber-500/20 text-amber-400' });
        if (meta.features.physicsBased) badges.push({ label: 'PHYSICS', color: 'bg-pink-500/20 text-pink-400' });
        if (meta.features.dualStrip) badges.push({ label: 'DUAL STRIP', color: 'bg-cyan-500/20 text-cyan-400' });

        featuresEl.innerHTML = badges.map(b =>
            `<span class="px-2 py-0.5 rounded text-[10px] font-medium ${b.color}">${b.label}</span>`
        ).join('');
    }
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
// User preset state
let userPresets = [];

function loadPreset(id) { API.loadPreset(id); }

function loadUserPreset(slot) {
    API.loadUserPreset(slot).then(r => {
        if (r.success) {
            console.log('Loaded user preset', slot);
        }
    });
}

function savePreset() {
    // Show save modal
    document.getElementById('preset-modal').classList.remove('hidden');
    document.getElementById('preset-name-input').value = '';
    document.getElementById('preset-name-input').focus();
}

function closePresetModal() {
    document.getElementById('preset-modal').classList.add('hidden');
}

function confirmSavePreset() {
    const name = document.getElementById('preset-name-input').value.trim();
    if (!name) {
        alert('Please enter a preset name');
        return;
    }

    // Find first empty slot, or use slot 0
    let slot = userPresets.findIndex(p => !p.saved);
    if (slot === -1) slot = 0;

    API.saveUserPreset(slot, name).then(r => {
        if (r.success) {
            closePresetModal();
            refreshUserPresets();
        } else {
            alert('Failed to save preset: ' + (r.error?.message || 'Unknown error'));
        }
    });
}

function deleteUserPreset(slot, event) {
    event.stopPropagation();
    if (!confirm('Delete this preset?')) return;

    API.deleteUserPreset(slot).then(r => {
        if (r.success) {
            refreshUserPresets();
        }
    });
}

function refreshUserPresets() {
    API.getUserPresets().then(r => {
        if (r.success && r.data.presets) {
            userPresets = r.data.presets;
            updateUserPresetButtons();
        }
    });
}

function updateUserPresetButtons() {
    const container = document.getElementById('user-presets-container');
    if (!container) return;

    container.innerHTML = userPresets.map((p, i) => {
        if (p.saved) {
            return `<button onclick="loadUserPreset(${i})" class="group relative px-3 py-1.5 rounded-lg bg-accent-cyan/10 border border-accent-cyan/30 text-xs font-medium text-accent-cyan hover:bg-accent-cyan/20 transition-all whitespace-nowrap">
                <span>${p.name.substring(0, 8)}</span>
                <span onclick="deleteUserPreset(${i}, event)" class="absolute -top-1 -right-1 hidden group-hover:flex w-4 h-4 items-center justify-center rounded-full bg-red-500 text-white text-[10px]">&times;</span>
            </button>`;
        } else {
            return `<button onclick="saveToSlot(${i})" class="px-3 py-1.5 rounded-lg bg-elevated border border-dashed border-border-subtle text-xs font-medium text-text-muted hover:border-accent-cyan/50 transition-all whitespace-nowrap opacity-50">
                +
            </button>`;
        }
    }).join('');
}

function saveToSlot(slot) {
    const name = prompt('Enter preset name:');
    if (!name || !name.trim()) return;

    API.saveUserPreset(slot, name.trim()).then(r => {
        if (r.success) {
            refreshUserPresets();
        } else {
            alert('Failed to save: ' + (r.error?.message || 'Unknown error'));
        }
    });
}

function resetToDefaults() { setZoneCount(3); }

// ========== Firmware Update ==========
function toggleFirmware() {
    const content = document.getElementById('firmware-content');
    const chevron = document.getElementById('firmware-chevron');
    if (!content) return;

    if (content.style.maxHeight && content.style.maxHeight !== '0px') {
        content.style.maxHeight = '0px';
        if (chevron) chevron.style.transform = 'rotate(0deg)';
    } else {
        content.style.maxHeight = content.scrollHeight + 'px';
        if (chevron) chevron.style.transform = 'rotate(180deg)';
    }
}

function formatFileSize(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
}

function showFirmwareStatus(message, type) {
    const statusEl = document.getElementById('firmware-status');
    if (!statusEl) return;

    statusEl.classList.remove('hidden', 'bg-green-500/20', 'text-green-400', 'bg-red-500/20', 'text-red-400', 'bg-yellow-500/20', 'text-yellow-400', 'bg-accent-cyan/20', 'text-accent-cyan');

    if (type === 'success') {
        statusEl.classList.add('bg-green-500/20', 'text-green-400');
    } else if (type === 'error') {
        statusEl.classList.add('bg-red-500/20', 'text-red-400');
    } else if (type === 'warning') {
        statusEl.classList.add('bg-yellow-500/20', 'text-yellow-400');
    } else {
        statusEl.classList.add('bg-accent-cyan/20', 'text-accent-cyan');
    }

    statusEl.textContent = message;
}

function updateFirmwareProgress(percent) {
    const bar = document.getElementById('firmware-progress-bar');
    const label = document.getElementById('firmware-progress-label');
    if (bar) bar.style.width = percent + '%';
    if (label) label.textContent = Math.round(percent) + '%';
}

function uploadFirmware() {
    const fileInput = document.getElementById('firmware-file');
    const uploadBtn = document.getElementById('firmware-upload-btn');
    const progressContainer = document.getElementById('firmware-progress-container');

    if (!fileInput || !fileInput.files || fileInput.files.length === 0) {
        showFirmwareStatus('Please select a firmware file', 'error');
        return;
    }

    const file = fileInput.files[0];

    // Validate file size (ESP32 has ~3.2MB per OTA slot)
    if (file.size > 3200 * 1024) {
        showFirmwareStatus('File too large (max 3.2 MB)', 'error');
        return;
    }

    // Disable upload button during upload
    if (uploadBtn) uploadBtn.disabled = true;
    if (progressContainer) progressContainer.classList.remove('hidden');

    showFirmwareStatus('Uploading firmware...', 'info');
    updateFirmwareProgress(0);

    // Create FormData and upload with XMLHttpRequest for progress
    const formData = new FormData();
    formData.append('update', file);

    const xhr = new XMLHttpRequest();

    xhr.upload.addEventListener('progress', (e) => {
        if (e.lengthComputable) {
            const percent = (e.loaded / e.total) * 100;
            updateFirmwareProgress(percent);
        }
    });

    xhr.addEventListener('load', () => {
        if (xhr.status === 200) {
            updateFirmwareProgress(100);
            showFirmwareStatus('Update successful! Rebooting...', 'success');

            // Device will reboot, show reconnecting message after delay
            setTimeout(() => {
                showFirmwareStatus('Device rebooting. Please wait...', 'warning');
            }, 2000);

            // Try to reconnect after device reboots
            setTimeout(() => {
                window.location.reload();
            }, 10000);
        } else {
            showFirmwareStatus(`Upload failed: ${xhr.statusText || 'Unknown error'}`, 'error');
            if (uploadBtn) uploadBtn.disabled = false;
        }
    });

    xhr.addEventListener('error', () => {
        showFirmwareStatus('Upload failed: Network error', 'error');
        if (uploadBtn) uploadBtn.disabled = false;
    });

    xhr.addEventListener('abort', () => {
        showFirmwareStatus('Upload cancelled', 'warning');
        if (uploadBtn) uploadBtn.disabled = false;
    });

    xhr.open('POST', '/update');
    xhr.send(formData);
}

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

        // Populate global palette dropdown
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

        // Populate zone palette dropdown (0 = global, 1+ = specific palette)
        const zoneSel = document.getElementById('zone-palette-select');
        if (zoneSel) {
            zoneSel.innerHTML = '<option value="0">Use Global Palette</option>';
            state.palettes.forEach(p => {
                const opt = document.createElement('option');
                opt.value = p.id + 1;  // Offset by 1 (0 = global)
                opt.textContent = p.name;
                zoneSel.appendChild(opt);
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
    refreshUserPresets();  // Load user presets for preset bar

    // Connect WebSocket
    WS.connect();

    // Set initial states
    updateZoneTabs(state.zoneCount);
    updateMixerChannels(state.zoneCount);
    selectZone(1);
    updateConnectionStatus();

    // Initialize firmware file input handler
    const firmwareFileInput = document.getElementById('firmware-file');
    if (firmwareFileInput) {
        firmwareFileInput.addEventListener('change', (e) => {
            const file = e.target.files[0];
            const infoEl = document.getElementById('firmware-info');
            const filenameEl = document.getElementById('firmware-filename');
            const filesizeEl = document.getElementById('firmware-filesize');
            const uploadBtn = document.getElementById('firmware-upload-btn');
            const statusEl = document.getElementById('firmware-status');
            const progressContainer = document.getElementById('firmware-progress-container');

            if (file) {
                // Show file info
                if (infoEl) infoEl.classList.remove('hidden');
                if (filenameEl) filenameEl.textContent = file.name;
                if (filesizeEl) filesizeEl.textContent = formatFileSize(file.size);
                if (uploadBtn) uploadBtn.disabled = false;

                // Reset status and progress
                if (statusEl) statusEl.classList.add('hidden');
                if (progressContainer) progressContainer.classList.add('hidden');

                // Validate file extension
                if (!file.name.toLowerCase().endsWith('.bin')) {
                    showFirmwareStatus('Please select a .bin file', 'warning');
                    if (uploadBtn) uploadBtn.disabled = true;
                }
            } else {
                if (infoEl) infoEl.classList.add('hidden');
                if (uploadBtn) uploadBtn.disabled = true;
            }
        });
    }

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
