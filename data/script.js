// LightwaveOS Web Control
// WebSocket connection and UI management

let ws = null;
let wsReconnectInterval = null;
let isConnected = false;
let currentEffect = 0;
let ledData = new Array(320).fill({r: 0, g: 0, b: 0});
let animationFrame = null;
let previewPaused = false;

// Configuration
const WS_PORT = 81;
const RECONNECT_INTERVAL = 5000;

// Effect definitions with icons
const effects = [
    { name: "Fire", icon: "🔥" },
    { name: "Ocean", icon: "🌊" },
    { name: "Wave", icon: "🌀" },
    { name: "Ripple", icon: "💧" },
    { name: "Strip Confetti", icon: "🎊" },
    { name: "Strip Juggle", icon: "🤹" },
    { name: "Strip Interference", icon: "📡" },
    { name: "Strip BPM", icon: "🎵" },
    { name: "Strip Plasma", icon: "🌈" },
    { name: "Confetti", icon: "🎉" },
    { name: "Sinelon", icon: "〰️" },
    { name: "Juggle", icon: "🔮" },
    { name: "BPM", icon: "💓" },
    { name: "Solid Blue", icon: "🔵" },
    { name: "Pulse Effect", icon: "💫" },
    { name: "Heartbeat", icon: "❤️" },
    { name: "Breathing", icon: "🫧" },
    { name: "Shockwave", icon: "💥" },
    { name: "Vortex", icon: "🌪️" },
    { name: "Collision", icon: "💫" },
    { name: "Gravity Well", icon: "🌌" }
];

// Palette definitions
const palettes = [
    { name: "Rainbow", colors: ["#FF0000", "#FF7F00", "#FFFF00", "#00FF00", "#0000FF", "#4B0082", "#9400D3"] },
    { name: "Ocean", colors: ["#000428", "#004e92", "#0077BE", "#0088CE", "#00A6FB", "#48CAE4", "#90E0EF"] },
    { name: "Fire", colors: ["#641E16", "#922B21", "#C0392B", "#E74C3C", "#EC7063", "#F1948A", "#FADBD8"] },
    { name: "Forest", colors: ["#0B5345", "#0E6655", "#148F77", "#17A589", "#1ABC9C", "#48C9B0", "#76D7C4"] },
    { name: "Sunset", colors: ["#512E5F", "#633974", "#76448A", "#884EA0", "#9B59B6", "#AF7AC5", "#C39BD3"] },
    { name: "Party", colors: ["#E91E63", "#9C27B0", "#673AB7", "#3F51B5", "#2196F3", "#00BCD4", "#009688"] },
    { name: "Lava", colors: ["#1A0000", "#330000", "#660000", "#990000", "#CC0000", "#FF3333", "#FF6666"] },
    { name: "Cloud", colors: ["#F8F9F9", "#F2F3F4", "#E5E7E9", "#D7DBDD", "#BFC9CA", "#AAB7B8", "#95A5A6"] },
    { name: "Galaxy", colors: ["#000000", "#0F0F0F", "#1E1E1E", "#2D2D2D", "#3C3C3C", "#4B4B4B", "#5A5A5A"] }
];

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    initializeUI();
    connectWebSocket();
    initializeCanvas();
    hideLoadingOverlay();
});

// Initialize UI components
function initializeUI() {
    // Populate effects
    populateEffects();
    
    // Populate palettes
    populatePalettes();
    
    // Setup event listeners
    setupEventListeners();
    
    // Initialize collapsible sections
    initializeCollapsibles();
}

// Populate effect grid
function populateEffects() {
    const effectGrid = document.getElementById('effect-grid');
    effectGrid.innerHTML = '';
    
    effects.forEach((effect, index) => {
        const card = document.createElement('div');
        card.className = 'effect-card';
        card.dataset.effectId = index;
        if (index === currentEffect) card.classList.add('active');
        
        card.innerHTML = `
            <span class="effect-icon">${effect.icon}</span>
            <span class="effect-name">${effect.name}</span>
        `;
        
        card.addEventListener('click', () => selectEffect(index));
        effectGrid.appendChild(card);
    });
}

// Populate palette grid
function populatePalettes() {
    const paletteGrid = document.getElementById('palette-grid');
    paletteGrid.innerHTML = '';
    
    palettes.forEach((palette, index) => {
        const card = document.createElement('div');
        card.className = 'palette-card';
        card.dataset.paletteId = index;
        
        const preview = document.createElement('div');
        preview.className = 'palette-preview';
        
        palette.colors.forEach(color => {
            const colorDiv = document.createElement('div');
            colorDiv.className = 'palette-color';
            colorDiv.style.backgroundColor = color;
            preview.appendChild(colorDiv);
        });
        
        card.appendChild(preview);
        card.innerHTML += `<div class="palette-name">${palette.name}</div>`;
        
        card.addEventListener('click', () => selectPalette(index));
        paletteGrid.appendChild(card);
    });
}

// Setup event listeners
function setupEventListeners() {
    // Sliders
    const sliders = ['brightness', 'speed', 'intensity', 'saturation', 'transition-time'];
    sliders.forEach(id => {
        const slider = document.getElementById(id);
        const valueDisplay = document.getElementById(`${id}-value`);
        
        slider.addEventListener('input', (e) => {
            let value = e.target.value;
            if (id === 'transition-time') {
                valueDisplay.textContent = `${value}ms`;
            } else {
                valueDisplay.textContent = value;
            }
            sendParameter(id, value);
        });
    });
    
    // Toggles
    const toggles = ['random-transitions', 'optimized-effects', 'auto-cycle'];
    toggles.forEach(id => {
        const toggle = document.getElementById(id);
        toggle.addEventListener('change', (e) => {
            sendParameter(id, e.target.checked);
        });
    });
    
    // Buttons
    document.getElementById('power-toggle').addEventListener('click', togglePower);
    document.getElementById('next-effect').addEventListener('click', nextEffect);
    document.getElementById('prev-effect').addEventListener('click', prevEffect);
    document.getElementById('save-preset').addEventListener('click', savePreset);
    document.getElementById('emergency-stop').addEventListener('click', emergencyStop);
    document.getElementById('preview-toggle').addEventListener('click', togglePreview);
    document.getElementById('sync-toggle').addEventListener('click', toggleSync);
}

// WebSocket connection
function connectWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.hostname}:${WS_PORT}`;
    
    try {
        ws = new WebSocket(wsUrl);
        
        ws.onopen = () => {
            console.log('WebSocket connected');
            isConnected = true;
            updateConnectionStatus(true);
            showToast('Connected to LightwaveOS', 'success');
            
            // Request current state
            sendCommand('get_state');
        };
        
        ws.onclose = () => {
            console.log('WebSocket disconnected');
            isConnected = false;
            updateConnectionStatus(false);
            showToast('Disconnected from device', 'error');
            
            // Attempt reconnection
            if (!wsReconnectInterval) {
                wsReconnectInterval = setInterval(connectWebSocket, RECONNECT_INTERVAL);
            }
        };
        
        ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            showToast('Connection error', 'error');
        };
        
        ws.onmessage = (event) => {
            handleMessage(JSON.parse(event.data));
        };
        
    } catch (error) {
        console.error('Failed to create WebSocket:', error);
        showToast('Failed to connect', 'error');
    }
}

// Handle incoming WebSocket messages
function handleMessage(data) {
    switch (data.type) {
        case 'state':
            updateUIState(data);
            break;
            
        case 'led_data':
            updateLEDData(data.leds);
            break;
            
        case 'performance':
            updatePerformanceMetrics(data);
            break;
            
        case 'effect_change':
            currentEffect = data.effect;
            updateEffectSelection();
            showToast(`Effect changed to ${effects[currentEffect].name}`, 'info');
            break;
            
        case 'error':
            showToast(data.message, 'error');
            break;
    }
}

// Update UI state from device
function updateUIState(state) {
    // Update sliders
    if (state.brightness !== undefined) {
        document.getElementById('brightness').value = state.brightness;
        document.getElementById('brightness-value').textContent = state.brightness;
    }
    
    if (state.speed !== undefined) {
        document.getElementById('speed').value = state.speed;
        document.getElementById('speed-value').textContent = state.speed;
    }
    
    if (state.intensity !== undefined) {
        document.getElementById('intensity').value = state.intensity;
        document.getElementById('intensity-value').textContent = state.intensity;
    }
    
    if (state.saturation !== undefined) {
        document.getElementById('saturation').value = state.saturation;
        document.getElementById('saturation-value').textContent = state.saturation;
    }
    
    // Update toggles
    if (state.randomTransitions !== undefined) {
        document.getElementById('random-transitions').checked = state.randomTransitions;
    }
    
    if (state.optimizedEffects !== undefined) {
        document.getElementById('optimized-effects').checked = state.optimizedEffects;
    }
    
    // Update current effect
    if (state.currentEffect !== undefined) {
        currentEffect = state.currentEffect;
        updateEffectSelection();
    }
    
    // Update performance metrics
    if (state.fps !== undefined) {
        document.getElementById('fps-counter').textContent = `${state.fps.toFixed(1)} FPS`;
    }
    
    if (state.heap !== undefined) {
        const heapKB = (state.heap / 1024).toFixed(1);
        document.getElementById('heap-status').textContent = `${heapKB} KB`;
    }
}

// Send command to device
function sendCommand(command, params = {}) {
    if (!isConnected || !ws) return;
    
    const message = {
        command: command,
        ...params
    };
    
    ws.send(JSON.stringify(message));
}

// Send parameter update
function sendParameter(param, value) {
    sendCommand('set_parameter', { parameter: param, value: value });
}

// Effect selection
function selectEffect(effectId) {
    sendCommand('set_effect', { effect: effectId });
    currentEffect = effectId;
    updateEffectSelection();
}

function updateEffectSelection() {
    document.querySelectorAll('.effect-card').forEach(card => {
        card.classList.toggle('active', parseInt(card.dataset.effectId) === currentEffect);
    });
}

// Palette selection
function selectPalette(paletteId) {
    sendCommand('set_palette', { palette: paletteId });
    
    document.querySelectorAll('.palette-card').forEach(card => {
        card.classList.toggle('active', parseInt(card.dataset.paletteId) === paletteId);
    });
}

// Button actions
function togglePower() {
    sendCommand('toggle_power');
    const btn = document.getElementById('power-toggle');
    btn.classList.toggle('active');
}

function nextEffect() {
    const nextId = (currentEffect + 1) % effects.length;
    selectEffect(nextId);
}

function prevEffect() {
    const prevId = currentEffect > 0 ? currentEffect - 1 : effects.length - 1;
    selectEffect(prevId);
}

function savePreset() {
    const name = prompt('Enter preset name:');
    if (name) {
        sendCommand('save_preset', { name: name });
        showToast(`Preset "${name}" saved`, 'success');
    }
}

function emergencyStop() {
    sendCommand('emergency_stop');
    showToast('Emergency stop activated', 'warning');
}

// LED Preview Canvas
function initializeCanvas() {
    const canvas = document.getElementById('led-canvas');
    const ctx = canvas.getContext('2d');
    
    // Set canvas size
    canvas.width = 800;
    canvas.height = 100;
    
    // Start animation loop
    animateLEDs();
}

function animateLEDs() {
    if (previewPaused) {
        animationFrame = requestAnimationFrame(animateLEDs);
        return;
    }
    
    const canvas = document.getElementById('led-canvas');
    const ctx = canvas.getContext('2d');
    
    // Clear canvas
    ctx.fillStyle = '#1a1a2e';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    
    // Draw LEDs
    const ledSize = canvas.width / 320;
    const y = canvas.height / 2;
    
    ledData.forEach((led, index) => {
        const x = index * ledSize + ledSize / 2;
        
        // Draw glow effect
        const gradient = ctx.createRadialGradient(x, y, 0, x, y, ledSize * 2);
        gradient.addColorStop(0, `rgba(${led.r}, ${led.g}, ${led.b}, 0.8)`);
        gradient.addColorStop(1, `rgba(${led.r}, ${led.g}, ${led.b}, 0)`);
        
        ctx.fillStyle = gradient;
        ctx.fillRect(x - ledSize * 2, y - ledSize * 2, ledSize * 4, ledSize * 4);
        
        // Draw LED
        ctx.fillStyle = `rgb(${led.r}, ${led.g}, ${led.b})`;
        ctx.fillRect(x - ledSize / 2, y - 10, ledSize, 20);
    });
    
    animationFrame = requestAnimationFrame(animateLEDs);
}

function updateLEDData(leds) {
    if (leds && leds.length === 320) {
        ledData = leds;
    }
}

function togglePreview() {
    previewPaused = !previewPaused;
    const btn = document.getElementById('preview-toggle');
    btn.innerHTML = previewPaused ? 
        '<i class="fas fa-play"></i> Resume Preview' : 
        '<i class="fas fa-pause"></i> Pause Preview';
}

function toggleSync() {
    const syncEnabled = document.getElementById('sync-toggle').classList.toggle('active');
    sendCommand('toggle_sync', { enabled: syncEnabled });
}

// Performance metrics
function updatePerformanceMetrics(metrics) {
    if (metrics.frameTime !== undefined) {
        document.getElementById('frame-time').textContent = `${metrics.frameTime.toFixed(2)} ms`;
    }
    
    if (metrics.cpuUsage !== undefined) {
        document.getElementById('cpu-usage').textContent = `${metrics.cpuUsage.toFixed(1)} %`;
    }
    
    if (metrics.optimizationGain !== undefined) {
        document.getElementById('opt-gain').textContent = `${metrics.optimizationGain.toFixed(1)} x`;
    }
}

// Collapsible sections
function initializeCollapsibles() {
    document.querySelectorAll('.collapsible-header').forEach(header => {
        header.addEventListener('click', () => {
            header.parentElement.classList.toggle('collapsed');
        });
    });
}

// Connection status
function updateConnectionStatus(connected) {
    const status = document.getElementById('connection-status');
    status.textContent = connected ? 'Connected' : 'Disconnected';
    status.className = 'status-text ' + (connected ? 'connected' : 'disconnected');
}

// Toast notifications
function showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    
    const icon = {
        success: 'fas fa-check-circle',
        error: 'fas fa-exclamation-circle',
        warning: 'fas fa-exclamation-triangle',
        info: 'fas fa-info-circle'
    }[type];
    
    toast.innerHTML = `
        <i class="${icon}"></i>
        <span>${message}</span>
    `;
    
    container.appendChild(toast);
    
    // Auto remove after 3 seconds
    setTimeout(() => {
        toast.style.opacity = '0';
        setTimeout(() => toast.remove(), 300);
    }, 3000);
}

// Loading overlay
function hideLoadingOverlay() {
    setTimeout(() => {
        document.getElementById('loading-overlay').classList.add('hidden');
    }, 1000);
}

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (ws) {
        ws.close();
    }
    if (animationFrame) {
        cancelAnimationFrame(animationFrame);
    }
});
