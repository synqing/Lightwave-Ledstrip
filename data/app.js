// ========== K1-Lightwave Controller Web App ==========
// Vanilla JavaScript - No frameworks
// WebSocket + REST API integration

// ========== Configuration ==========
const CONFIG = {
    WS_URL: `ws://${window.location.hostname}/ws`,
    API_BASE: `/api`,
    RECONNECT_INTERVAL: 3000,
    HEARTBEAT_INTERVAL: 30000,
    DEBOUNCE_SLIDER: 150,       // ms delay for slider changes
    DEBOUNCE_PIPELINE: 250,     // ms delay for pipeline sliders
};

// ========== State Management ==========
const state = {
    // Connection
    ws: null,
    connected: false,
    reconnectTimer: null,

    // Zone Composer
    zoneMode: false,
    zoneCount: 3,
    selectedZone: 1,
    zones: [
        { id: 1, effectId: 0, enabled: true },
        { id: 2, effectId: 0, enabled: true },
        { id: 3, effectId: 0, enabled: true },
    ],

    // Global Parameters
    brightness: 210,
    speed: 32,
    paletteId: 0,
    intensity: 128,
    saturation: 200,
    complexity: 45,
    variation: 180,
    transitions: false,

    // Effects Data
    effects: [],
    palettes: [],
    currentEffect: 0,

    // Performance
    fps: 0,
    memoryFree: 0,
};

// ========== Zone Configuration ==========
// Zone definitions matching AURA spec for 3-zone mode
// Default 3-zone mode uses custom layout (30+90+40 LEDs)
const zones = {
    1: {
        color: 'text-accent-zone1',
        bg: 'bg-accent-zone1',
        border: 'border-accent-zone1',
        text: 'LEDs 65-94',  // 3-ZONE: Zone 0 (center, 30 LEDs, continuous)
        ids: ['vis-z1']
    },
    2: {
        color: 'text-accent-zone2',
        bg: 'bg-accent-zone2',
        border: 'border-accent-zone2',
        text: 'LEDs 20-64 | 95-139',  // 3-ZONE: Zone 1 (90 LEDs, split)
        ids: ['vis-z2-l', 'vis-z2-r']
    },
    3: {
        color: 'text-accent-zone3',
        bg: 'bg-accent-zone3',
        border: 'border-accent-zone3',
        text: 'LEDs 0-19 | 140-159',  // 3-ZONE: Zone 2 (40 LEDs, split)
        ids: ['vis-z3-l', 'vis-z3-r']
    },
    4: {
        color: 'text-accent-zone3',  // Reuse zone3 color
        bg: 'bg-accent-zone3',
        border: 'border-accent-zone3',
        text: 'LEDs 0-19 | 140-159',  // 4-ZONE: Zone 3 (40 LEDs, split)
        ids: ['vis-z3-l', 'vis-z3-r']
    }
};

// ========== API Client ==========
const API = {
    async request(endpoint, options = {}) {
        try {
            const response = await fetch(`${CONFIG.API_BASE}${endpoint}`, {
                ...options,
                headers: {
                    'Content-Type': 'application/json',
                    ...options.headers,
                },
            });

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            return await response.json();
        } catch (error) {
            console.error(`API Error [${endpoint}]:`, error);
            return null;
        }
    },

    // Global Parameters
    async setBrightness(value) {
        return this.request('/brightness', {
            method: 'POST',
            body: JSON.stringify({ brightness: value }),
        });
    },

    async setSpeed(value) {
        return this.request('/speed', {
            method: 'POST',
            body: JSON.stringify({ speed: value }),
        });
    },

    async setPalette(paletteId) {
        return this.request('/palette', {
            method: 'POST',
            body: JSON.stringify({ paletteId }),
        });
    },

    async setPipeline(intensity, saturation, complexity, variation) {
        return this.request('/pipeline', {
            method: 'POST',
            body: JSON.stringify({ intensity, saturation, complexity, variation }),
        });
    },

    async toggleTransitions(enabled) {
        return this.request('/transitions', {
            method: 'POST',
            body: JSON.stringify({ enabled }),
        });
    },

    // Zone Control
    async setZoneCount(count) {
        return this.request('/zone/count', {
            method: 'POST',
            body: JSON.stringify({ count }),
        });
    },

    async setZoneEffect(zoneId, effectId) {
        return this.request('/zone/config', {
            method: 'POST',
            body: JSON.stringify({ zoneId, effectId, enabled: true }),
        });
    },

    async setZoneEnabled(zoneId, enabled) {
        return this.request('/zone/config', {
            method: 'POST',
            body: JSON.stringify({ zoneId, enabled }),
        });
    },

    async loadPreset(presetId) {
        return this.request('/zone/preset/load', {
            method: 'POST',
            body: JSON.stringify({ presetId }),
        });
    },

    async saveConfig() {
        return this.request('/zone/config/save', {
            method: 'POST',
        });
    },

    // System
    async getStatus() {
        return this.request('/status');
    },
};

// ========== WebSocket Client ==========
const WS = {
    connect() {
        if (state.ws) {
            state.ws.close();
        }

        console.log('Connecting to WebSocket:', CONFIG.WS_URL);
        state.ws = new WebSocket(CONFIG.WS_URL);

        state.ws.onopen = () => {
            console.log('WebSocket connected');
            state.connected = true;
            updateConnectionStatus();
            clearTimeout(state.reconnectTimer);

            // Request initial state
            this.send({ type: 'getState' });
        };

        state.ws.onmessage = (event) => {
            try {
                const message = JSON.parse(event.data);
                this.handleMessage(message);
            } catch (error) {
                console.error('WebSocket message parse error:', error);
            }
        };

        state.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
        };

        state.ws.onclose = () => {
            console.log('WebSocket disconnected');
            state.connected = false;
            updateConnectionStatus();

            // Auto-reconnect
            state.reconnectTimer = setTimeout(() => {
                this.connect();
            }, CONFIG.RECONNECT_INTERVAL);
        };
    },

    send(message) {
        if (state.ws && state.ws.readyState === WebSocket.OPEN) {
            state.ws.send(JSON.stringify(message));
            console.log('WS ->', message);
        } else {
            console.warn('WebSocket not connected');
        }
    },

    handleMessage(message) {
        console.log('WS <-', message);

        switch (message.type) {
            case 'status':
                updatePerformance(message.freeHeap);
                break;

            case 'performance':
                updatePerformance(message.fps, message.memoryFree);
                break;

            case 'effectChange':
                updateCurrentEffect(message.effectId, message.name);
                break;

            case 'brightnessChange':
                state.brightness = message.brightness;
                updateSliderValue('slider-brightness', 'val-brightness', message.brightness);
                break;

            case 'speedChange':
                state.speed = message.speed;
                updateSliderValue('slider-speed', 'val-speed', message.speed);
                break;

            case 'paletteChange':
                state.paletteId = message.paletteId;
                document.getElementById('palette-select').value = message.paletteId;
                break;

            case 'zone.state':
                // Update zone state from server
                break;

            case 'error':
                console.error('Server error:', message.message);
                break;

            default:
                console.warn('Unknown message type:', message.type);
        }
    }
};

// ========== UI Update Functions ==========
function updateConnectionStatus() {
    const statusText = document.getElementById('status-text');
    const statusDot = document.getElementById('status-dot');

    if (state.connected) {
        statusText.textContent = 'ONLINE';
        statusText.className = 'text-xs font-semibold tracking-wide text-text-secondary';
        statusDot.className = 'w-2 h-2 rounded-full bg-green-500 shadow-[0_0_8px_rgba(34,221,136,0.6)] animate-pulse';
    } else {
        statusText.textContent = 'OFFLINE';
        statusText.className = 'text-xs font-semibold tracking-wide text-text-secondary';
        statusDot.className = 'w-2 h-2 rounded-full bg-red-500 shadow-[0_0_8px_rgba(255,85,85,0.6)]';
    }
}

function updatePerformance(fps, memoryFree) {
    if (fps !== undefined) {
        state.fps = fps;
        document.getElementById('status-fps').textContent = fps;
    }

    if (memoryFree !== undefined) {
        state.memoryFree = memoryFree;
        const memMB = Math.round(memoryFree / 1024);
        document.getElementById('status-mem').textContent = `${memMB}KB`;
    }
}

function updateCurrentEffect(effectId, name) {
    state.currentEffect = effectId;
    const statusFx = document.getElementById('status-fx');
    if (statusFx) {
        statusFx.textContent = name || `Effect ${effectId}`;
    }
}

function updateSliderValue(sliderId, labelId, value) {
    const slider = document.getElementById(sliderId);
    const label = document.getElementById(labelId);
    if (slider && label) {
        slider.value = value;
        label.textContent = value;
        updateSliderTrack(slider);
    }
}

// ========== Zone Logic ==========
function selectZone(id) {
    state.selectedZone = id;

    // Reset Tabs
    [1, 2, 3].forEach(z => {
        const btn = document.getElementById(`tab-z${z}`);
        if (!btn) return;
        btn.setAttribute('aria-selected', 'false');
        btn.className = "flex-1 pb-2 text-sm text-text-secondary hover:text-text-primary transition-colors flex justify-center items-center gap-2 border-b-2 border-transparent";
    });

    // Activate Tab
    const activeBtn = document.getElementById(`tab-z${id}`);
    if (activeBtn) {
        activeBtn.setAttribute('aria-selected', 'true');
        activeBtn.className = `flex-1 pb-2 text-sm font-semibold text-text-primary flex justify-center items-center gap-2 border-b-2 ${zones[id].border}`;
    }

    // Update Badge
    const badge = document.getElementById('range-badge');
    if (badge) {
        badge.className = `w-full py-2 px-3 rounded text-xs font-mono font-medium text-center border transition-all duration-300 ${zones[id].bg}/10 ${zones[id].color} ${zones[id].border}/20`;
        badge.innerText = zones[id].text;
    }

    // Update Visualization Overlays
    document.querySelectorAll('.zone-overlay').forEach(el => {
        el.style.opacity = '0';
    });

    // Show active zone segments
    zones[id].ids.forEach(visId => {
        const overlay = document.getElementById(visId);
        if (overlay) {
            overlay.style.opacity = '0.5';
        }
    });
}

function setZoneCount(count) {
    state.zoneCount = count;

    // Update UI button states
    for (let i = 1; i <= 4; i++) {
        const btn = document.querySelector(`button[onclick="setZoneCount(${i})"]`);
        if (btn) {
            if (i === count) {
                btn.className = 'w-6 h-6 text-xs text-black bg-accent-cyan font-semibold rounded shadow-sm';
            } else {
                btn.className = 'w-6 h-6 text-xs text-text-secondary hover:text-white rounded transition-colors';
            }
        }
    }

    // Send to server
    API.setZoneCount(count);
}

function enableZone() {
    const zoneId = state.selectedZone - 1; // Convert to 0-indexed for API
    API.setZoneEnabled(zoneId, true);
}

function disableZone() {
    const zoneId = state.selectedZone - 1; // Convert to 0-indexed for API
    API.setZoneEnabled(zoneId, false);
}

function setZoneEffect() {
    const effectSelect = document.getElementById('effect-select');
    if (!effectSelect) return;

    const effectId = parseInt(effectSelect.value);
    const zoneId = state.selectedZone - 1; // Convert to 0-indexed for API

    API.setZoneEffect(zoneId, effectId);
}

function prevEffect() {
    const effectSelect = document.getElementById('effect-select');
    if (!effectSelect) return;

    const currentIndex = effectSelect.selectedIndex;
    if (currentIndex > 0) {
        effectSelect.selectedIndex = currentIndex - 1;
        setZoneEffect();
    }
}

function nextEffect() {
    const effectSelect = document.getElementById('effect-select');
    if (!effectSelect) return;

    const currentIndex = effectSelect.selectedIndex;
    if (currentIndex < effectSelect.options.length - 1) {
        effectSelect.selectedIndex = currentIndex + 1;
        setZoneEffect();
    }
}

// ========== Global Parameters ==========
let brightnessTimeout, speedTimeout, pipelineTimeout;

function updateSlider(input, labelId) {
    const label = document.getElementById(labelId);
    if (label) {
        label.innerText = input.value;
    }
    updateSliderTrack(input);

    // Debounced API calls based on slider type
    const sliderId = input.id;

    if (sliderId === 'slider-brightness') {
        clearTimeout(brightnessTimeout);
        brightnessTimeout = setTimeout(() => {
            state.brightness = parseInt(input.value);
            API.setBrightness(state.brightness);
        }, CONFIG.DEBOUNCE_SLIDER);
    }
    else if (sliderId === 'slider-speed') {
        clearTimeout(speedTimeout);
        speedTimeout = setTimeout(() => {
            state.speed = parseInt(input.value);
            API.setSpeed(state.speed);
        }, CONFIG.DEBOUNCE_SLIDER);
    }
    else if (sliderId.startsWith('slider-')) {
        // Pipeline sliders (intensity, saturation, complexity, variation)
        clearTimeout(pipelineTimeout);
        pipelineTimeout = setTimeout(() => {
            const intensity = parseInt(document.getElementById('slider-intensity').value);
            const saturation = parseInt(document.getElementById('slider-saturation').value);
            const complexity = parseInt(document.getElementById('slider-complexity').value);
            const variation = parseInt(document.getElementById('slider-variation').value);

            state.intensity = intensity;
            state.saturation = saturation;
            state.complexity = complexity;
            state.variation = variation;

            API.setPipeline(intensity, saturation, complexity, variation);
        }, CONFIG.DEBOUNCE_PIPELINE);
    }
}

function updateSliderTrack(input) {
    const min = input.min || 0;
    const max = input.max || 100;
    const val = input.value;
    const percentage = ((val - min) / (max - min)) * 100;

    input.style.background = `linear-gradient(to right, #00d4ff ${percentage}%, #2f3849 ${percentage}%)`;
}

function setPalette() {
    const paletteSelect = document.getElementById('palette-select');
    if (!paletteSelect) return;

    const paletteId = parseInt(paletteSelect.value);
    state.paletteId = paletteId;
    API.setPalette(paletteId);
}

function toggleTransitions() {
    const toggle = document.getElementById('toggle-transitions');
    if (!toggle) return;

    state.transitions = toggle.checked;
    API.toggleTransitions(state.transitions);
}

// ========== Collapsible Panels ==========
function toggleEditRange() {
    const panel = document.getElementById('edit-range-panel');
    if (!panel) return;

    if (panel.style.maxHeight && panel.style.maxHeight !== '0px') {
        panel.style.maxHeight = '0px';
        panel.style.opacity = '0';
    } else {
        panel.style.maxHeight = panel.scrollHeight + "px";
        panel.style.opacity = '1';
    }
}

function togglePipeline() {
    const content = document.getElementById('pipeline-content');
    const chevron = document.getElementById('pipeline-chevron');
    if (!content || !chevron) return;

    if (content.style.maxHeight && content.style.maxHeight !== '0px') {
        content.style.maxHeight = '0px';
        chevron.style.transform = 'rotate(0deg)';
    } else {
        content.style.maxHeight = content.scrollHeight + "px";
        chevron.style.transform = 'rotate(180deg)';
    }
}

// ========== Preset Management ==========
function loadPreset(presetId) {
    API.loadPreset(presetId);
}

function savePreset() {
    API.saveConfig();
}

// ========== Initialization ==========
async function loadEffects() {
    try {
        // Request all effects at once with high count parameter
        // Backend supports pagination, requesting 100 to ensure we get all 47+ effects
        const response = await fetch(`${CONFIG.API_BASE}/effects?start=0&count=100`);
        if (!response.ok) return;

        const data = await response.json();
        state.effects = data.effects || [];

        const effectSelect = document.getElementById('effect-select');
        if (effectSelect) {
            effectSelect.innerHTML = '';
            state.effects.forEach(effect => {
                const option = document.createElement('option');
                option.value = effect.id;
                option.textContent = effect.name;
                effectSelect.appendChild(option);
            });
        }

        console.log(`Loaded ${state.effects.length} of ${data.total || state.effects.length} effects`);
    } catch (error) {
        console.error('Failed to load effects:', error);
    }
}

async function loadPalettes() {
    try {
        const response = await fetch(`${CONFIG.API_BASE}/palettes`);
        if (!response.ok) return;

        const data = await response.json();
        state.palettes = data.palettes || [];

        const paletteSelect = document.getElementById('palette-select');
        if (paletteSelect) {
            paletteSelect.innerHTML = '';
            state.palettes.forEach(palette => {
                const option = document.createElement('option');
                option.value = palette.id;
                option.textContent = palette.name;
                paletteSelect.appendChild(option);
            });
        }

        console.log(`Loaded ${state.palettes.length} palettes`);
    } catch (error) {
        console.error('Failed to load palettes:', error);
    }
}

document.addEventListener('DOMContentLoaded', async () => {
    console.log('K1-Lightwave Controller initializing...');

    // Initialize slider backgrounds
    document.querySelectorAll('.slider-input').forEach(input => {
        updateSliderTrack(input);
    });

    // Load effects and palettes from API
    await loadEffects();
    await loadPalettes();

    // Connect to WebSocket
    WS.connect();

    // Set initial zone selection
    selectZone(1);

    // Update connection status
    updateConnectionStatus();

    console.log('K1-Lightwave Controller initialized');
});
