/**
 * LightwaveOS Simple Controller
 * Minimal WebSocket-based control interface
 */

// Debug logging - shows on page AND console
function log(msg) {
    console.log(msg);
    const el = document.getElementById('debugLog');
    if (el) {
        el.innerHTML += msg + '<br>';
        el.scrollTop = el.scrollHeight;
    }
}

log('[SCRIPT] app.js loaded at ' + new Date().toLocaleTimeString());

// ─────────────────────────────────────────────────────────────
// Hardcoded Palette Names (from firmware Palettes_MasterData.cpp)
// ─────────────────────────────────────────────────────────────
const PALETTE_NAMES = [
    // CPT-City Palettes (Artistic) - IDs 0-32
    "Sunset Real",           // 0
    "Rivendell",             // 1
    "Ocean Breeze 036",      // 2
    "RGI 15",                // 3
    "Retro 2",               // 4
    "Analogous 1",           // 5
    "Pink Splash 08",        // 6
    "Coral Reef",            // 7
    "Ocean Breeze 068",      // 8
    "Pink Splash 07",        // 9
    "Vintage 01",            // 10
    "Departure",             // 11
    "Landscape 64",          // 12
    "Landscape 33",          // 13
    "Rainbow Sherbet",       // 14
    "GR65 Hult",             // 15
    "GR64 Hult",             // 16
    "GMT Dry Wet",           // 17
    "IB Jul01",              // 18
    "Vintage 57",            // 19
    "IB15",                  // 20
    "Fuschia 7",             // 21
    "Emerald Dragon",        // 22
    "Lava",                  // 23
    "Fire",                  // 24
    "Colorful",              // 25
    "Magenta Evening",       // 26
    "Pink Purple",           // 27
    "Autumn 19",             // 28
    "Blue Magenta White",    // 29
    "Black Magenta Red",     // 30
    "Red Magenta Yellow",    // 31
    "Blue Cyan Yellow",      // 32
    // Crameri Palettes (Scientific) - IDs 33-56
    "Vik",                   // 33
    "Tokyo",                 // 34
    "Roma",                  // 35
    "Oleron",                // 36
    "Lisbon",                // 37
    "La Jolla",              // 38
    "Hawaii",                // 39
    "Devon",                 // 40
    "Cork",                  // 41
    "Broc",                  // 42
    "Berlin",                // 43
    "Bamako",                // 44
    "Acton",                 // 45
    "Batlow",                // 46
    "Bilbao",                // 47
    "Buda",                  // 48
    "Davos",                 // 49
    "GrayC",                 // 50
    "Imola",                 // 51
    "La Paz",                // 52
    "Nuuk",                  // 53
    "Oslo",                  // 54
    "Tofino",                // 55
    "Turku",                 // 56
    // R Colorspace Palettes (LGP-Optimized) - IDs 57-74
    "Viridis",               // 57
    "Plasma",                // 58
    "Inferno",               // 59
    "Magma",                 // 60
    "Cubhelix",              // 61
    "Abyss",                 // 62
    "Bathy",                 // 63
    "Ocean",                 // 64
    "Nighttime",             // 65
    "Seafloor",              // 66
    "IBCSO",                 // 67
    "Copper",                // 68
    "Hot",                   // 69
    "Cool",                  // 70
    "Earth",                 // 71
    "Sealand",               // 72
    "Split",                 // 73
    "Red2Green"              // 74
];

// Application State
const state = {
    ws: null,
    connected: false,
    reconnectTimer: null,
    deviceHost: null,  // Custom host (null = use page host)

    // Pending change flags (ignore stale server updates during user interactions)
    pendingPaletteChange: null,    // Timer ID when palette change is pending
    pendingEffectChange: null,     // Timer ID when effect change is pending
    pendingBrightnessChange: null, // Timer ID when brightness change is pending
    pendingSpeedChange: null,      // Timer ID when speed change is pending
    pendingMoodChange: null,       // Timer ID when mood change is pending

    // Current values (synced from server)
    effectId: 0,
    effectName: '',
    paletteId: 0,
    paletteName: '',
    brightness: 128,
    speed: 25,
    mood: 128,  // Sensory Bridge mood: 0=reactive, 255=smooth

    // Beat tracking
    currentBpm: 0,
    bpmConfidence: 0,
    lastBeatTime: 0,

    // Limits
    PALETTE_COUNT: 75,  // 0-74
    BRIGHTNESS_MIN: 0,
    BRIGHTNESS_MAX: 255,
    BRIGHTNESS_STEP: 10,
    SPEED_MIN: 1,
    SPEED_MAX: 100,  // Extended range (was 50)
    SPEED_STEP: 1,
    MOOD_MIN: 0,
    MOOD_MAX: 255,

    // Reconnect interval
    RECONNECT_MS: 3000
};

// DOM Element References (cached on init)
const elements = {};

// ─────────────────────────────────────────────────────────────
// WebSocket Connection
// ─────────────────────────────────────────────────────────────

function connect() {
    // Use custom host if set, otherwise use page host
    let host = state.deviceHost || window.location.hostname || 'lightwaveos.local';
    
    // Remove http:// or https:// if present
    host = host.replace(/^https?:\/\//, '');
    // Remove trailing slash
    host = host.replace(/\/$/, '');
    
    const wsUrl = `ws://${host}/ws`;

    log('[WS] Connecting to: ' + wsUrl);

    try {
        state.ws = new WebSocket(wsUrl);
    } catch (e) {
        console.error('[WS] Failed to create WebSocket:', e);
        scheduleReconnect();
        return;
    }

    state.ws.onopen = () => {
        log('[WS] Connected! Ready to send commands.');
        state.connected = true;
        updateConnectionUI();

        // Reset pending flags on reconnect (prevent stale state)
        if (state.pendingEffectChange) {
            clearTimeout(state.pendingEffectChange);
            state.pendingEffectChange = null;
        }
        if (state.pendingPaletteChange) {
            clearTimeout(state.pendingPaletteChange);
            state.pendingPaletteChange = null;
        }
        if (state.pendingBrightnessChange) {
            clearTimeout(state.pendingBrightnessChange);
            state.pendingBrightnessChange = null;
        }
        if (state.pendingSpeedChange) {
            clearTimeout(state.pendingSpeedChange);
            state.pendingSpeedChange = null;
        }

        // Request initial state (re-fetch all state on reconnect)
        setTimeout(() => {
            send({ type: 'getStatus' });
            // Fetch current parameters (includes paletteId, uses hardcoded lookup for name)
            fetchCurrentParameters();
        }, 100);
    };

    state.ws.onclose = (e) => {
        log(`[WS] Disconnected: code ${e.code}${e.reason ? ', reason: ' + e.reason : ''}`);
        state.connected = false;
        updateConnectionUI();
        if (e.code !== 1000) {  // Only reconnect if not a normal close
            scheduleReconnect();
        }
    };

    state.ws.onerror = (e) => {
        log('[WS] Connection error - check device IP and ensure WebSocket is enabled');
        console.error('[WS] Error:', e);
    };

    state.ws.onmessage = (e) => {
        // Handle binary messages (audio frames)
        if (e.data instanceof ArrayBuffer) {
            handleBinaryFrame(e.data);
            return;
        }
        if (e.data instanceof Blob) {
            // Convert Blob to ArrayBuffer and handle
            e.data.arrayBuffer().then(buffer => handleBinaryFrame(buffer));
            return;
        }

        try {
            const msg = JSON.parse(e.data);
            handleMessage(msg);
        } catch (err) {
            console.error('[WS] Parse error:', err, e.data);
        }
    };
}

function send(msg) {
    if (!state.ws) {
        log('[WS] ERROR: WebSocket not initialized. Click CONNECT first.');
        return false;
    }
    
    if (state.ws.readyState === WebSocket.OPEN) {
        const jsonMsg = JSON.stringify(msg);
        log('[WS] SEND: ' + jsonMsg);
        try {
            state.ws.send(jsonMsg);
            return true;
        } catch (e) {
            log('[WS] ERROR: Failed to send message: ' + e.message);
            return false;
        }
    } else {
        const states = ['CONNECTING', 'OPEN', 'CLOSING', 'CLOSED'];
        log(`[WS] ERROR: Not connected! State: ${states[state.ws.readyState]}. Click CONNECT first.`);
        return false;
    }
}

function scheduleReconnect() {
    if (state.reconnectTimer) return;

    console.log(`[WS] Reconnecting in ${state.RECONNECT_MS}ms...`);
    state.reconnectTimer = setTimeout(() => {
        state.reconnectTimer = null;
        connect();
    }, state.RECONNECT_MS);
}

// ─────────────────────────────────────────────────────────────
// Binary Frame Handler (Audio Data)
// ─────────────────────────────────────────────────────────────

function handleBinaryFrame(buffer) {
    // Binary audio frames are structured data from the ESP32
    // For now we just acknowledge receipt - detailed parsing can be added as needed
    // The primary audio data comes via JSON beat.event messages
}

// ─────────────────────────────────────────────────────────────
// Beat Event Handlers
// ─────────────────────────────────────────────────────────────

function pulseBeatIndicator(strength) {
    const indicator = elements.beatIndicator;
    if (!indicator) return;

    // Scale the pulse based on strength (0.0 - 1.0)
    const scale = 1 + (strength * 0.5);  // 1.0 to 1.5x scale
    const opacity = 0.5 + (strength * 0.5);  // 0.5 to 1.0 opacity

    // Apply immediate pulse effect
    indicator.style.transform = `scale(${scale})`;
    indicator.style.opacity = opacity;

    // Add the pulse class for animation
    indicator.classList.add('pulse');

    // Reset after animation
    setTimeout(() => {
        indicator.style.transform = 'scale(1)';
        indicator.style.opacity = '0.6';
        indicator.classList.remove('pulse');
    }, 150);
}

function updateBpmDisplay(bpm, confidence) {
    state.currentBpm = bpm;
    state.bpmConfidence = confidence;

    const bpmElement = elements.bpmValue;
    if (!bpmElement) return;

    // Update the BPM value
    if (bpm > 0) {
        bpmElement.textContent = Math.round(bpm);
    } else {
        bpmElement.textContent = '--';
    }

    // Update confidence indicator via opacity
    const bpmDisplay = elements.bpmDisplay;
    if (bpmDisplay) {
        // Confidence affects the glow/visibility
        const minOpacity = 0.4;
        const maxOpacity = 1.0;
        const opacity = minOpacity + (confidence * (maxOpacity - minOpacity));
        bpmDisplay.style.opacity = opacity;
    }
}

function handleBeatEvent(msg) {
    // Update BPM display
    if (msg.bpm !== undefined) {
        updateBpmDisplay(msg.bpm, msg.confidence || 0);
    }

    // Pulse the beat indicator on tick
    if (msg.tick) {
        // Downbeats get stronger pulse
        const strength = msg.downbeat ? 1.0 : 0.6;
        pulseBeatIndicator(strength);
        state.lastBeatTime = Date.now();
    }
}

// ─────────────────────────────────────────────────────────────
// Message Handler
// ─────────────────────────────────────────────────────────────

function handleMessage(msg) {
    // Log with actual data - not just type
    if (msg.type === 'beat.event') {
        log(`[BEAT] BPM:${msg.bpm?.toFixed(1)} conf:${(msg.confidence*100)?.toFixed(0)}% phase:${msg.beat_phase?.toFixed(2)} beat#${msg.beat_index} ${msg.downbeat ? 'DOWNBEAT' : ''}`);
    } else {
        log('[WS] RECV: ' + msg.type + (msg.effectId !== undefined ? ` (effectId: ${msg.effectId})` : '') + (msg.brightness !== undefined ? ` (brightness: ${msg.brightness})` : ''));
    }

    switch (msg.type) {
        case 'status':
            // Full state sync - server uses effectId/paletteId
            // Skip effect updates if user just changed it (prevents flicker from stale server state)
            if (msg.effectId !== undefined && !state.pendingEffectChange) {
                const oldId = state.effectId;
                if (msg.effectId !== state.effectId) {
                    state.effectId = msg.effectId;
                    log(`[UI] Effect changed: ${oldId} -> ${state.effectId}`);
                }
            }
            if (msg.effectName !== undefined && !state.pendingEffectChange) state.effectName = msg.effectName;
            // Skip brightness/speed updates if user is dragging the slider
            if (msg.brightness !== undefined && !state.pendingBrightnessChange) {
                if (msg.brightness !== state.brightness) {
                    state.brightness = msg.brightness;
                }
            }
            if (msg.speed !== undefined && !state.pendingSpeedChange) {
                if (msg.speed !== state.speed) {
                    state.speed = msg.speed;
                }
            }
            // Skip palette updates if user just changed it (prevents flicker from stale server state)
            if (msg.paletteId !== undefined && !state.pendingPaletteChange) {
                const newPaletteId = parseInt(msg.paletteId);
                if (!isNaN(newPaletteId) && newPaletteId !== state.paletteId) {
                    state.paletteId = newPaletteId;
                    // Use hardcoded lookup for name (instant, no API call)
                    state.paletteName = getPaletteName(newPaletteId);
                }
            }
            if (msg.paletteName !== undefined && msg.paletteName && !state.pendingPaletteChange) {
                state.paletteName = msg.paletteName;
            }
            updateAllUI();
            break;

        case 'effectChange':
            if (msg.effectId !== undefined) state.effectId = msg.effectId;
            if (msg.name !== undefined) state.effectName = msg.name;
            updatePatternUI();
            break;

        case 'paletteChange':
            if (msg.paletteId !== undefined) {
                state.paletteId = parseInt(msg.paletteId);
                // Use hardcoded lookup for name (instant, no API call)
                state.paletteName = getPaletteName(state.paletteId);
            }
            if (msg.name !== undefined) {
                state.paletteName = msg.name;
            }
            updatePaletteUI();
            break;

        case 'beat.event':
            handleBeatEvent(msg);
            break;

        default:
            // Don't log beat events as unknown (they may come with just 'beat.event' type)
            if (msg.type !== 'beat.event') {
                log('[WS] Unknown: ' + msg.type);
            }
            break;
    }
}

// ─────────────────────────────────────────────────────────────
// Button Throttling
// ─────────────────────────────────────────────────────────────

let lastButtonTime = 0;
const BUTTON_THROTTLE_MS = 150;

// ─────────────────────────────────────────────────────────────
// Retry Logic with Exponential Backoff
// ─────────────────────────────────────────────────────────────

async function fetchWithRetry(url, options, maxRetries = 3) {
    const delays = [500, 1000, 2000]; // Exponential backoff delays in ms
    
    for (let attempt = 0; attempt <= maxRetries; attempt++) {
        try {
            const response = await fetch(url, options);
            if (response.ok) {
                return await response.json();
            }
            // If not OK but not a network error, return the error response
            if (attempt < maxRetries) {
                log(`[RETRY] Request failed (${response.status}), retrying in ${delays[attempt]}ms... (attempt ${attempt + 1}/${maxRetries})`);
                await new Promise(resolve => setTimeout(resolve, delays[attempt]));
                continue;
            }
            return await response.json(); // Return error response on final attempt
        } catch (error) {
            if (attempt < maxRetries) {
                log(`[RETRY] Network error: ${error.message}, retrying in ${delays[attempt]}ms... (attempt ${attempt + 1}/${maxRetries})`);
                await new Promise(resolve => setTimeout(resolve, delays[attempt]));
                continue;
            }
            throw error; // Re-throw on final attempt
        }
    }
}

function throttledAction(action, actionName) {
    const now = Date.now();
    if (now - lastButtonTime < BUTTON_THROTTLE_MS) {
        log(`[THROTTLE] Ignoring rapid ${actionName} click (${now - lastButtonTime}ms since last)`);
        return false;
    }
    lastButtonTime = now;
    action();
    return true;
}

// ─────────────────────────────────────────────────────────────
// Control Handlers
// ─────────────────────────────────────────────────────────────

// Pattern navigation - using REST API only (removed duplicate WebSocket)
function nextPattern() {
    if (!throttledAction(() => {
        log('[BTN] Next Pattern');
        
        // Use current effect ID from state, or default to 0
        const currentId = state.effectId || 0;
        const nextId = (currentId + 1) % 75;
        
        // Set pending flag to ignore stale server updates (clear after 1 second)
        if (state.pendingEffectChange) clearTimeout(state.pendingEffectChange);
        state.pendingEffectChange = setTimeout(() => { state.pendingEffectChange = null; }, 1000);
        
        // REST API only - removed duplicate WebSocket send
        fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/effects/set`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ effectId: nextId })
        })
        .then(data => {
            if (data.success) {
                state.effectId = data.data.effectId;
                state.effectName = data.data.name;
                updatePatternUI();
                log(`[REST] ✅ Effect changed to: ${data.data.name} (${data.data.effectId})`);
            } else {
                log(`[REST] ❌ Failed: ${JSON.stringify(data)}`);
            }
        })
        .catch(e => log('[REST] ❌ Error after retries: ' + e.message));
    }, 'nextPattern')) {
        return;
    }
}

function prevPattern() {
    if (!throttledAction(() => {
        log('[BTN] Prev Pattern');
        
        // Use current effect ID from state, or default to 0
        const currentId = state.effectId || 0;
        const prevId = (currentId - 1 + 75) % 75;
        
        // Set pending flag to ignore stale server updates (clear after 1 second)
        if (state.pendingEffectChange) clearTimeout(state.pendingEffectChange);
        state.pendingEffectChange = setTimeout(() => { state.pendingEffectChange = null; }, 1000);
        
        // REST API only - removed duplicate WebSocket send
        fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/effects/set`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ effectId: prevId })
        })
        .then(data => {
            if (data.success) {
                state.effectId = data.data.effectId;
                state.effectName = data.data.name;
                updatePatternUI();
                log(`[REST] ✅ Effect changed to: ${data.data.name} (${data.data.effectId})`);
            } else {
                log(`[REST] ❌ Failed: ${JSON.stringify(data)}`);
            }
        })
        .catch(e => log('[REST] ❌ Error after retries: ' + e.message));
    }, 'prevPattern')) {
        return;
    }
}

// Palette navigation - using REST API only (removed duplicate WebSocket)
function nextPalette() {
    if (!throttledAction(() => {
        log('[BTN] Next Palette');
        state.paletteId = (state.paletteId + 1) % state.PALETTE_COUNT;
        // Use hardcoded lookup for name (instant, no API call)
        state.paletteName = getPaletteName(state.paletteId);
        updatePaletteUI();
        
        // Set pending flag to ignore stale server updates (clear after 1 second)
        if (state.pendingPaletteChange) clearTimeout(state.pendingPaletteChange);
        state.pendingPaletteChange = setTimeout(() => { state.pendingPaletteChange = null; }, 1000);
        
        // REST API only - removed duplicate WebSocket send
        fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/palettes/set`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ paletteId: state.paletteId })
        })
        .then(data => {
            if (data.success) {
                log(`[REST] ✅ Palette set to: ${state.paletteName} (${state.paletteId})`);
            }
        })
        .catch(e => log('[REST] Error after retries: ' + e.message));
    }, 'nextPalette')) {
        return;
    }
}

function prevPalette() {
    if (!throttledAction(() => {
        log('[BTN] Prev Palette');
        state.paletteId = (state.paletteId - 1 + state.PALETTE_COUNT) % state.PALETTE_COUNT;
        // Use hardcoded lookup for name (instant, no API call)
        state.paletteName = getPaletteName(state.paletteId);
        updatePaletteUI();
        
        // Set pending flag to ignore stale server updates (clear after 1 second)
        if (state.pendingPaletteChange) clearTimeout(state.pendingPaletteChange);
        state.pendingPaletteChange = setTimeout(() => { state.pendingPaletteChange = null; }, 1000);
        
        // REST API only - removed duplicate WebSocket send
        fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/palettes/set`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ paletteId: state.paletteId })
        })
        .then(data => {
            if (data.success) {
                log(`[REST] ✅ Palette set to: ${state.paletteName} (${state.paletteId})`);
            }
        })
        .catch(e => log('[REST] Error after retries: ' + e.message));
    }, 'prevPalette')) {
        return;
    }
}

// Brightness control - slider handler
let brightnessUpdateTimer = null;
function onBrightnessChange() {
    const newVal = parseInt(elements.brightnessSlider.value);
    if (newVal !== state.brightness) {
        state.brightness = newVal;
        updateBrightnessUI();
        
        // Set pending flag to ignore stale server updates while dragging
        if (state.pendingBrightnessChange) clearTimeout(state.pendingBrightnessChange);
        state.pendingBrightnessChange = setTimeout(() => { state.pendingBrightnessChange = null; }, 1000);
        
        // Debounce API calls while dragging
        if (brightnessUpdateTimer) {
            clearTimeout(brightnessUpdateTimer);
        }
        
        brightnessUpdateTimer = setTimeout(() => {
            // Try WebSocket
            if (state.connected && state.ws && state.ws.readyState === WebSocket.OPEN) {
                send({ type: 'setBrightness', value: newVal });
            }
            
            // REST API backup
            fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/parameters`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ brightness: newVal })
            })
            .then(data => {
                if (data.success) {
                    log(`[REST] Brightness set to: ${newVal}`);
                }
            })
            .catch(e => log('[REST] Error after retries: ' + e.message));
        }, 150);
    }
}

// Speed control - slider handler
let speedUpdateTimer = null;
function onSpeedChange() {
    const newVal = parseInt(elements.speedSlider.value);
    if (newVal !== state.speed) {
        state.speed = newVal;
        updateSpeedUI();

        // Set pending flag to ignore stale server updates while dragging
        if (state.pendingSpeedChange) clearTimeout(state.pendingSpeedChange);
        state.pendingSpeedChange = setTimeout(() => { state.pendingSpeedChange = null; }, 1000);

        // Debounce API calls while dragging
        if (speedUpdateTimer) {
            clearTimeout(speedUpdateTimer);
        }

        speedUpdateTimer = setTimeout(() => {
            // Try WebSocket
            if (state.connected && state.ws && state.ws.readyState === WebSocket.OPEN) {
                send({ type: 'setSpeed', value: newVal });
            }

            // REST API backup
            fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/parameters`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ speed: newVal })
            })
            .then(data => {
                if (data.success) {
                    log(`[REST] Speed set to: ${newVal}`);
                }
            })
            .catch(e => log('[REST] Error after retries: ' + e.message));
        }, 150);
    }
}

// Mood control - slider handler (Sensory Bridge: 0=reactive, 255=smooth)
let moodUpdateTimer = null;
function onMoodChange() {
    const newVal = parseInt(elements.moodSlider.value);
    if (newVal !== state.mood) {
        state.mood = newVal;
        updateMoodUI();

        // Set pending flag to ignore stale server updates while dragging
        if (state.pendingMoodChange) clearTimeout(state.pendingMoodChange);
        state.pendingMoodChange = setTimeout(() => { state.pendingMoodChange = null; }, 1000);

        // Debounce API calls while dragging
        if (moodUpdateTimer) {
            clearTimeout(moodUpdateTimer);
        }

        moodUpdateTimer = setTimeout(() => {
            // Try WebSocket
            if (state.connected && state.ws && state.ws.readyState === WebSocket.OPEN) {
                send({ type: 'setMood', value: newVal });
            }

            // REST API backup
            fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/parameters`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ mood: newVal })
            })
            .then(data => {
                if (data.success) {
                    log(`[REST] Mood set to: ${newVal}`);
                }
            })
            .catch(e => log('[REST] Error after retries: ' + e.message));
        }, 150);
    }
}

// ─────────────────────────────────────────────────────────────
// UI Update Functions
// ─────────────────────────────────────────────────────────────

function updateConnectionUI() {
    const dot = elements.statusDot;
    const text = elements.statusText;

    if (state.connected) {
        dot.classList.add('connected');
        dot.classList.remove('disconnected');
        text.textContent = 'Connected';
    } else {
        dot.classList.remove('connected');
        dot.classList.add('disconnected');
        text.textContent = 'Disconnected';
    }
}

function updatePatternUI() {
    const name = state.effectName || `Effect ${state.effectId}`;
    elements.patternName.textContent = name;
    elements.currentEffect.textContent = name;
}

// Get palette name from hardcoded lookup (instant, no API call)
function getPaletteName(paletteId) {
    if (paletteId >= 0 && paletteId < PALETTE_NAMES.length) {
        return PALETTE_NAMES[paletteId];
    }
    return `Palette ${paletteId}`;
}

// Fetch current parameters (including paletteId, mood) from API
function fetchCurrentParameters() {
    const url = `http://${state.deviceHost || '192.168.0.16'}/api/v1/parameters`;
    fetchWithRetry(url)
        .then(data => {
            if (data && data.success && data.data) {
                // Update state with all parameters
                if (data.data.brightness !== undefined) {
                    state.brightness = data.data.brightness;
                    updateBrightnessUI();
                }
                if (data.data.speed !== undefined) {
                    state.speed = data.data.speed;
                    updateSpeedUI();
                }
                if (data.data.mood !== undefined) {
                    state.mood = data.data.mood;
                    updateMoodUI();
                    log(`[REST] ✅ Mood: ${state.mood}`);
                }
                if (data.data.paletteId !== undefined) {
                    const paletteId = parseInt(data.data.paletteId);
                    if (!isNaN(paletteId)) {
                        state.paletteId = paletteId;
                        // Use hardcoded lookup for name
                        state.paletteName = getPaletteName(paletteId);
                        updatePaletteUI();
                        log(`[REST] ✅ Palette: ${state.paletteName} (${paletteId})`);
                    }
                }
            }
        })
        .catch(e => {
            log(`[REST] ❌ Error fetching parameters after retries: ${e.message}`);
        });
}

function updatePaletteUI() {
    // Ensure paletteId is a valid number
    const paletteId = (typeof state.paletteId === 'number' && !isNaN(state.paletteId)) ? state.paletteId : 0;
    
    // Get palette name from hardcoded lookup (instant, never fails)
    const paletteName = getPaletteName(paletteId);
    
    elements.paletteName.textContent = paletteName;
    elements.paletteId.textContent = `${paletteId + 1}/${state.PALETTE_COUNT}`;
}

function updateBrightnessUI() {
    if (elements.brightnessSlider) {
        elements.brightnessSlider.value = state.brightness;
    }
    if (elements.brightnessValue) {
        elements.brightnessValue.textContent = state.brightness;
    }
}

function updateSpeedUI() {
    if (elements.speedSlider) {
        elements.speedSlider.value = state.speed;
    }
    if (elements.speedValue) {
        elements.speedValue.textContent = state.speed;
    }
}

function updateMoodUI() {
    if (elements.moodSlider) {
        elements.moodSlider.value = state.mood;
    }
    if (elements.moodValue) {
        elements.moodValue.textContent = state.mood;
    }
}

function updateAllUI() {
    updateConnectionUI();
    updatePatternUI();
    updatePaletteUI();
    updateBrightnessUI();
    updateSpeedUI();
    updateMoodUI();
}

// ─────────────────────────────────────────────────────────────
// Auto-Discovery
// ─────────────────────────────────────────────────────────────

async function discoverDevice() {
    log('[DISCOVER] Searching for LightwaveOS device...');
    elements.statusText.textContent = 'Searching...';

    try {
        const response = await fetch('/api/discover', {
            method: 'GET',
            cache: 'no-cache'
        });
        const data = await response.json();

        if (data.success && data.ip) {
            log(`[DISCOVER] Found device at ${data.ip}`);
            return data.ip;
        }
    } catch (e) {
        log(`[DISCOVER] Server discovery failed: ${e.message}`);
    }

    // Fallback: try lightwaveos.local directly (works on macOS/iOS)
    try {
        log('[DISCOVER] Trying lightwaveos.local directly...');
        const testUrl = 'http://lightwaveos.local/api/v1/device/info';
        const response = await fetch(testUrl, {
            method: 'GET',
            mode: 'cors',
            cache: 'no-cache',
            signal: AbortSignal.timeout(3000)
        });
        if (response.ok) {
            log('[DISCOVER] lightwaveos.local is reachable');
            return 'lightwaveos.local';
        }
    } catch (e) {
        log(`[DISCOVER] lightwaveos.local not reachable: ${e.message}`);
    }

    return null;
}

// ─────────────────────────────────────────────────────────────
// Initialization
// ─────────────────────────────────────────────────────────────

function init() {
    // Cache DOM elements
    elements.statusDot = document.getElementById('statusDot');
    elements.statusText = document.getElementById('statusText');
    elements.currentEffect = document.getElementById('currentEffect');
    elements.patternName = document.getElementById('patternName');
    elements.paletteName = document.getElementById('paletteName');
    elements.paletteId = document.getElementById('paletteId');
    elements.brightnessSlider = document.getElementById('brightnessSlider');
    elements.brightnessValue = document.getElementById('brightnessValue');
    elements.speedSlider = document.getElementById('speedSlider');
    elements.speedValue = document.getElementById('speedValue');
    elements.moodSlider = document.getElementById('moodSlider');
    elements.moodValue = document.getElementById('moodValue');
    elements.configBar = document.getElementById('configBar');
    elements.deviceHost = document.getElementById('deviceHost');
    elements.connectBtn = document.getElementById('connectBtn');
    elements.beatIndicator = document.getElementById('beatIndicator');
    elements.bpmDisplay = document.getElementById('bpmDisplay');
    elements.bpmValue = document.getElementById('bpmValue');

    // Bind button events
    document.getElementById('patternPrev').addEventListener('click', prevPattern);
    document.getElementById('patternNext').addEventListener('click', nextPattern);
    document.getElementById('palettePrev').addEventListener('click', prevPalette);
    document.getElementById('paletteNext').addEventListener('click', nextPalette);
    
    // Bind slider events
    if (elements.brightnessSlider) {
        elements.brightnessSlider.addEventListener('input', onBrightnessChange);
        elements.brightnessSlider.addEventListener('change', onBrightnessChange);
    }
    if (elements.speedSlider) {
        elements.speedSlider.addEventListener('input', onSpeedChange);
        elements.speedSlider.addEventListener('change', onSpeedChange);
    }
    if (elements.moodSlider) {
        elements.moodSlider.addEventListener('input', onMoodChange);
        elements.moodSlider.addEventListener('change', onMoodChange);
    }

    // Check if we're running on the device or remotely
    // Only hide config bar if we're actually ON the device (192.168.0.16 or lightwaveos.local)
    const isOnDevice = window.location.hostname === 'lightwaveos.local' ||
                       window.location.hostname === '192.168.0.16';

    if (isOnDevice) {
        // Running on device - hide config bar, auto-connect
        elements.configBar.classList.add('hidden');
        connect();
    } else {
        // Running remotely - try auto-discovery first
        elements.statusText.textContent = 'Searching...';

        // Load saved host from localStorage as fallback
        const savedHost = localStorage.getItem('lightwaveHost');
        if (savedHost) {
            elements.deviceHost.value = savedHost;
        }

        // Connect button handler
        const connectWithHost = (host) => {
            if (host) {
                state.deviceHost = host;
                localStorage.setItem('lightwaveHost', host);
                elements.deviceHost.value = host;
                log('[CONFIG] Device host set to: ' + host);

                // Close existing connection if any
                if (state.ws) {
                    state.ws.close();
                    state.ws = null;
                }
                if (state.reconnectTimer) {
                    clearTimeout(state.reconnectTimer);
                    state.reconnectTimer = null;
                }

                // Small delay to ensure cleanup
                setTimeout(() => {
                    connect();
                }, 100);
            } else {
                log('[CONFIG] ERROR: Please enter a device IP or hostname');
            }
        };

        elements.connectBtn.addEventListener('click', () => {
            connectWithHost(elements.deviceHost.value.trim());
        });

        // Enter key to connect
        elements.deviceHost.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                elements.connectBtn.click();
            }
        });

        // Auto-discover device
        discoverDevice().then(discoveredHost => {
            if (discoveredHost) {
                log(`[DISCOVER] Auto-connecting to ${discoveredHost}`);
                connectWithHost(discoveredHost);
            } else {
                // Fall back to saved host or show manual input
                if (savedHost) {
                    log(`[DISCOVER] Using saved host: ${savedHost}`);
                    elements.statusText.textContent = 'Click Connect';
                } else {
                    elements.statusText.textContent = 'Enter device IP';
                }
            }
        });
    }

    log('[App] Initialized');
}

// Start when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
} else {
    init();
}
