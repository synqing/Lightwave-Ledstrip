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
    pendingFadeChange: null,       // Timer ID when fade change is pending
    pendingZoneSpeedChange: {},    // Object: { [zoneId]: timerId } for per-zone debouncing
    pendingZoneModeToggle: null,   // Timer ID when zone mode toggle is pending (prevents rapid toggles)

    // Current values (synced from server)
    effectId: 0,
    effectName: '',
    paletteId: 0,
    paletteName: '',
    brightness: 128,
    speed: 25,
    mood: 128,  // Sensory Bridge: 0=reactive, 255=smooth
    fadeAmount: 20,  // Trail fade amount

    // Zone state (null if zones not available/disabled)
    zones: null,  // { enabled: boolean, zoneCount: number, zones: Array<{id, speed, ...}>, segments: Array<{zoneId, s1LeftStart, ...}> }
    
    // Zone editor state
    zoneEditorSegments: [],  // Current editing segments
    zoneEditorPreset: null,  // Selected preset ID or null for custom
    
    // Palette and effect lists (cached for dropdowns)
    palettesList: [],  // Array of {id, name, category, ...}
    effectsList: [],   // Array of {id, name, category, ...}
    patternFilter: 'all',  // 'all', 'reactive', 'ambient'

    // Plugin state
    plugins: null,  // { registeredCount, loadedFromLittleFS, overrideModeEnabled, lastReloadOk, lastReloadMillis, errorCount, lastErrorSummary }

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
    SPEED_MAX: 100,  // Extended range
    SPEED_STEP: 1,
    MOOD_MIN: 0,
    MOOD_MAX: 255,
    MOOD_STEP: 1,
    FADE_MIN: 0,
    FADE_MAX: 255,

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
        if (state.pendingZoneModeToggle) {
            clearTimeout(state.pendingZoneModeToggle);
            state.pendingZoneModeToggle = null;
        }

        // Request initial state (re-fetch all state on reconnect)
        setTimeout(() => {
            send({ type: 'getStatus' });
            // Fetch current parameters (includes paletteId, uses hardcoded lookup for name)
            fetchCurrentParameters();
            // Fetch zones state
            fetchZonesState();
            // Fetch palettes and effects lists for dropdowns
            fetchPalettesList();
            fetchEffectsList();
            // Fetch plugin stats
            fetchPluginStats();
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
    // Handle WebSocket error envelopes
    if (msg.type === 'error' && msg.error) {
        const errorCode = msg.error.code || 'UNKNOWN';
        const errorMsg = msg.error.message || 'Unknown error';
        log(`[WS] ERROR: ${errorCode} - ${errorMsg}`);
        return;
    }

    // Unwrap standard WebSocket response envelope: {type, success, data:{...}}
    // The firmware's buildWsResponse() wraps payload fields under "data"
    let msgFlat = msg;
    if (msg.success === true && msg.data && typeof msg.data === 'object') {
        // Create flattened view: merge data fields into top level
        msgFlat = {
            type: msg.type,
            requestId: msg.requestId,
            success: msg.success,
            ...msg.data
        };
        log(`[WS] Unwrapped envelope for type: ${msg.type}`);
    }

    // Log with actual data - not just type
    if (msgFlat.type === 'beat.event') {
        log(`[BEAT] BPM:${msgFlat.bpm?.toFixed(1)} conf:${(msgFlat.confidence*100)?.toFixed(0)}% phase:${msgFlat.beat_phase?.toFixed(2)} beat#${msgFlat.beat_index} ${msgFlat.downbeat ? 'DOWNBEAT' : ''}`);
    } else {
        log('[WS] RECV: ' + msgFlat.type + (msgFlat.effectId !== undefined ? ` (effectId: ${msgFlat.effectId})` : '') + (msgFlat.brightness !== undefined ? ` (brightness: ${msgFlat.brightness})` : ''));
    }

    switch (msgFlat.type) {
        case 'status':
            // Full state sync - server uses effectId/paletteId
            // Skip effect updates if user just changed it (prevents flicker from stale server state)
            if (msgFlat.effectId !== undefined && !state.pendingEffectChange) {
                const oldId = state.effectId;
                if (msgFlat.effectId !== state.effectId) {
                    state.effectId = msgFlat.effectId;
                    log(`[UI] Effect changed: ${oldId} -> ${state.effectId}`);
                }
            }
            if (msgFlat.effectName !== undefined && !state.pendingEffectChange) state.effectName = msgFlat.effectName;
            // Skip brightness/speed updates if user is dragging the slider
            if (msgFlat.brightness !== undefined && !state.pendingBrightnessChange) {
                if (msgFlat.brightness !== state.brightness) {
                    state.brightness = msgFlat.brightness;
                }
            }
            if (msgFlat.speed !== undefined && !state.pendingSpeedChange) {
                if (msgFlat.speed !== state.speed) {
                    state.speed = msgFlat.speed;
                }
            }
            // Skip palette updates if user just changed it (prevents flicker from stale server state)
            if (msgFlat.paletteId !== undefined && !state.pendingPaletteChange) {
                const newPaletteId = parseInt(msgFlat.paletteId);
                if (!isNaN(newPaletteId) && newPaletteId !== state.paletteId) {
                    state.paletteId = newPaletteId;
                    // Use hardcoded lookup for name (instant, no API call)
                    state.paletteName = getPaletteName(newPaletteId);
                }
            }
            if (msgFlat.paletteName !== undefined && msgFlat.paletteName && !state.pendingPaletteChange) {
                state.paletteName = msgFlat.paletteName;
            }
            updateAllUI();
            break;

        case 'effectChange':
            if (msgFlat.effectId !== undefined) state.effectId = msgFlat.effectId;
            if (msgFlat.name !== undefined) state.effectName = msgFlat.name;
            updatePatternUI();
            break;

        case 'paletteChange':
            if (msgFlat.paletteId !== undefined) {
                state.paletteId = parseInt(msgFlat.paletteId);
                // Use hardcoded lookup for name (instant, no API call)
                state.paletteName = getPaletteName(state.paletteId);
            }
            if (msgFlat.name !== undefined) {
                state.paletteName = msgFlat.name;
            }
            updatePaletteUI();
            break;

        case 'zones.list':
            // Zones list response from WebSocket
            if (msgFlat.enabled !== undefined) {
                const wasEnabled = state.zones ? state.zones.enabled : false;
                state.zones = {
                    enabled: msgFlat.enabled,
                    zoneCount: msgFlat.zoneCount || (msgFlat.zones ? msgFlat.zones.length : 0),
                    zones: msgFlat.zones || [],
                    segments: msgFlat.segments || []  // Parse segments array
                };
                log(`[ZONES] List received: ${state.zones.zoneCount} zones, enabled=${state.zones.enabled}`);
                
                // If zone mode just changed, log it
                if (wasEnabled !== state.zones.enabled) {
                    log(`[ZONES] Zone mode changed: ${wasEnabled} -> ${state.zones.enabled}`);
                }
                
                // Update zone editor if segments are available
                if (state.zones.segments && state.zones.segments.length > 0) {
                    state.zoneEditorSegments = JSON.parse(JSON.stringify(state.zones.segments));
                    state.zoneEditorPreset = null;
                }
                
                updateZonesUI();
                updateZoneModeUI();
                updateZoneEditorUI();
            }
            break;

        case 'zones.changed':
            // Zone configuration changed (from zones.update)
            if (msgFlat.zoneId !== undefined && state.zones && state.zones.zones) {
                const zoneId = msgFlat.zoneId;
                if (state.zones.zones[zoneId]) {
                    // Update zone from msgFlat.current
                    if (msgFlat.current) {
                        if (msgFlat.current.speed !== undefined && !state.pendingZoneSpeedChange[zoneId]) {
                            state.zones.zones[zoneId].speed = msgFlat.current.speed;
                        }
                        // Update other fields if present
                        if (msgFlat.current.brightness !== undefined) {
                            state.zones.zones[zoneId].brightness = msgFlat.current.brightness;
                        }
                        if (msgFlat.current.effectId !== undefined) {
                            state.zones.zones[zoneId].effectId = msgFlat.current.effectId;
                        }
                    }
                    log(`[ZONES] Zone ${zoneId} updated: speed=${state.zones.zones[zoneId].speed}`);
                    updateZonesUI();
                }
            }
            break;

        case 'zone.enabledChanged':
            // Global zone mode enable/disable event
            if (msgFlat.enabled !== undefined && state.zones) {
                const wasEnabled = state.zones.enabled;
                state.zones.enabled = msgFlat.enabled;
                log(`[ZONE] Zone mode changed via event: ${wasEnabled} -> ${msgFlat.enabled}`);
                updateZonesUI();
                updateZoneModeUI();
            }
            break;

        case 'zones.effectChanged':
            // Zone effect changed event
            if (msgFlat.zoneId !== undefined && state.zones && state.zones.zones) {
                const zoneId = msgFlat.zoneId;
                if (state.zones.zones[zoneId]) {
                    // Update zone from msgFlat.current if present, otherwise use direct fields
                    if (msgFlat.current) {
                        if (msgFlat.current.effectId !== undefined) {
                            state.zones.zones[zoneId].effectId = msgFlat.current.effectId;
                        }
                        if (msgFlat.current.effectName !== undefined) {
                            // Store effectName if available
                        }
                        // Update other fields if present
                        if (msgFlat.current.brightness !== undefined) {
                            state.zones.zones[zoneId].brightness = msgFlat.current.brightness;
                        }
                        if (msgFlat.current.speed !== undefined) {
                            state.zones.zones[zoneId].speed = msgFlat.current.speed;
                        }
                        if (msgFlat.current.paletteId !== undefined) {
                            state.zones.zones[zoneId].paletteId = msgFlat.current.paletteId;
                        }
                    } else {
                        // Fallback: use direct fields if current object not present
                        if (msgFlat.effectId !== undefined) {
                            state.zones.zones[zoneId].effectId = msgFlat.effectId;
                        }
                    }
                    log(`[ZONES] Zone ${zoneId} effect changed: effectId=${state.zones.zones[zoneId].effectId}`);
                    updateZonesUI();
                }
            }
            break;

        case 'zone.paletteChanged':
            // Zone palette changed event
            if (msgFlat.zoneId !== undefined && state.zones && state.zones.zones) {
                const zoneId = msgFlat.zoneId;
                if (state.zones.zones[zoneId]) {
                    // Update zone from msgFlat.current if present, otherwise use direct fields
                    if (msgFlat.current) {
                        if (msgFlat.current.paletteId !== undefined) {
                            state.zones.zones[zoneId].paletteId = msgFlat.current.paletteId;
                        }
                        // Update other fields if present
                        if (msgFlat.current.effectId !== undefined) {
                            state.zones.zones[zoneId].effectId = msgFlat.current.effectId;
                        }
                        if (msgFlat.current.brightness !== undefined) {
                            state.zones.zones[zoneId].brightness = msgFlat.current.brightness;
                        }
                        if (msgFlat.current.speed !== undefined) {
                            state.zones.zones[zoneId].speed = msgFlat.current.speed;
                        }
                    } else {
                        // Fallback: use direct fields if current object not present
                        if (msgFlat.paletteId !== undefined) {
                            state.zones.zones[zoneId].paletteId = msgFlat.paletteId;
                        }
                    }
                    log(`[ZONES] Zone ${zoneId} palette changed: paletteId=${state.zones.zones[zoneId].paletteId}`);
                    updateZonesUI();
                }
            }
            break;

        case 'zone.state':
            // Fallback: treat zone.state same as zones.list
            if (msgFlat.enabled !== undefined) {
                const wasEnabled = state.zones ? state.zones.enabled : false;
                state.zones = {
                    enabled: msgFlat.enabled,
                    zoneCount: msgFlat.zoneCount || (msgFlat.zones ? msgFlat.zones.length : 0),
                    zones: msgFlat.zones || [],
                    segments: msgFlat.segments || []  // Parse segments array
                };
                log(`[ZONES] zone.state received: ${state.zones.zoneCount} zones, enabled=${state.zones.enabled}`);
                if (wasEnabled !== state.zones.enabled) {
                    log(`[ZONES] Zone mode changed: ${wasEnabled} -> ${state.zones.enabled}`);
                }
                
                // Update zone editor if segments are available
                if (state.zones.segments && state.zones.segments.length > 0) {
                    state.zoneEditorSegments = JSON.parse(JSON.stringify(state.zones.segments));
                    state.zoneEditorPreset = null;
                }
                
                updateZonesUI();
                updateZoneModeUI();
                updateZoneEditorUI();
            }
            break;

        case 'zones.layoutChanged':
            // Zone layout changed event (from zones.setLayout)
            if (msgFlat.success && msgFlat.zoneCount !== undefined) {
                log(`[ZONES] Layout changed: ${msgFlat.zoneCount} zones`);
                // Refresh zones state to get updated segments
                fetchZonesState();
            }
            break;
            break;

        case 'beat.event':
            handleBeatEvent(msgFlat);
            break;

        case 'plugins.stats':
        case 'plugins.reload.result':
            // Plugin stats response
            if (msgFlat.stats) {
                // Response from reload has stats nested
                state.plugins = msgFlat.stats;
            } else {
                // Direct stats response
                state.plugins = {
                    registeredCount: msgFlat.registeredCount,
                    loadedFromLittleFS: msgFlat.loadedFromLittleFS,
                    overrideModeEnabled: msgFlat.overrideModeEnabled,
                    disabledByOverride: msgFlat.disabledByOverride,
                    lastReloadOk: msgFlat.lastReloadOk,
                    lastReloadMillis: msgFlat.lastReloadMillis,
                    manifestCount: msgFlat.manifestCount,
                    errorCount: msgFlat.errorCount,
                    lastErrorSummary: msgFlat.lastErrorSummary
                };
            }
            // Store errors if present
            if (msgFlat.errors && msgFlat.errors.length > 0) {
                state.plugins.errors = msgFlat.errors;
            }
            updatePluginUI();
            break;

        default:
            // Don't log beat events as unknown (they may come with just 'beat.event' type)
            if (msgFlat.type !== 'beat.event') {
                log('[WS] Unknown: ' + msgFlat.type);
            }
            break;
    }
}

// ─────────────────────────────────────────────────────────────
// Button Throttling
// ─────────────────────────────────────────────────────────────

let lastButtonTime = 0;
const BUTTON_THROTTLE_MS = 150;
const ZONE_MODE_TOGGLE_THROTTLE_MS = 500; // Longer throttle for zone mode toggle to prevent flooding

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

// Set effect by ID
function setEffect(effectId) {
    if (!throttledAction(() => {
        log(`[BTN] Set Effect: ${effectId}`);
        
        // Set pending flag to ignore stale server updates (clear after 1 second)
        if (state.pendingEffectChange) clearTimeout(state.pendingEffectChange);
        state.pendingEffectChange = setTimeout(() => { state.pendingEffectChange = null; }, 1000);
        
        // REST API
        fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/effects/set`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ effectId: effectId })
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
    }, 'setEffect')) {
        return;
    }
}

// Set palette by ID
function setPalette(paletteId) {
    if (!throttledAction(() => {
        log(`[BTN] Set Palette: ${paletteId}`);
        
        state.paletteId = paletteId;
        // Use hardcoded lookup for name (instant, no API call)
        state.paletteName = getPaletteName(paletteId);
        updatePaletteUI();
        
        // Set pending flag to ignore stale server updates (clear after 1 second)
        if (state.pendingPaletteChange) clearTimeout(state.pendingPaletteChange);
        state.pendingPaletteChange = setTimeout(() => { state.pendingPaletteChange = null; }, 1000);
        
        // REST API
        fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/palettes/set`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ paletteId: paletteId })
        })
        .then(data => {
            if (data.success) {
                log(`[REST] ✅ Palette set to: ${state.paletteName} (${state.paletteId})`);
            }
        })
        .catch(e => log('[REST] Error after retries: ' + e.message));
    }, 'setPalette')) {
        return;
    }
}

// Pattern navigation - using REST API only (removed duplicate WebSocket)
function nextPattern() {
    if (!throttledAction(() => {
        log('[BTN] Next Pattern');
        
        // Get filtered effects list
        let filteredEffects = state.effectsList;
        if (state.patternFilter === 'reactive') {
            filteredEffects = state.effectsList.filter(eff => eff.isAudioReactive === true);
        } else if (state.patternFilter === 'ambient') {
            filteredEffects = state.effectsList.filter(eff => eff.isAudioReactive !== true);
        }
        
        if (filteredEffects.length === 0) {
            log('[BTN] No effects in current filter');
            return;
        }
        
        // Find current effect index in filtered list
        const currentIndex = filteredEffects.findIndex(eff => eff.id === state.effectId);
        const nextIndex = (currentIndex + 1) % filteredEffects.length;
        const nextId = filteredEffects[nextIndex].id;
        
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
        
        // Get filtered effects list
        let filteredEffects = state.effectsList;
        if (state.patternFilter === 'reactive') {
            filteredEffects = state.effectsList.filter(eff => eff.isAudioReactive === true);
        } else if (state.patternFilter === 'ambient') {
            filteredEffects = state.effectsList.filter(eff => eff.isAudioReactive !== true);
        }
        
        if (filteredEffects.length === 0) {
            log('[BTN] No effects in current filter');
            return;
        }
        
        // Find current effect index in filtered list
        const currentIndex = filteredEffects.findIndex(eff => eff.id === state.effectId);
        const prevIndex = (currentIndex - 1 + filteredEffects.length) % filteredEffects.length;
        const prevId = filteredEffects[prevIndex].id;
        
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

// Fade control - slider handler
let fadeUpdateTimer = null;
function onFadeChange() {
    const newVal = parseInt(elements.fadeSlider.value);
    if (newVal !== state.fadeAmount) {
        state.fadeAmount = newVal;
        updateFadeUI();

        // Set pending flag to ignore stale server updates while dragging
        if (state.pendingFadeChange) clearTimeout(state.pendingFadeChange);
        state.pendingFadeChange = setTimeout(() => { state.pendingFadeChange = null; }, 1000);

        // Debounce API calls while dragging
        if (fadeUpdateTimer) {
            clearTimeout(fadeUpdateTimer);
        }

        fadeUpdateTimer = setTimeout(() => {
            // REST API
            fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/parameters`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ fadeAmount: newVal })
            })
            .then(data => {
                if (data.success) {
                    log(`[REST] Fade set to: ${newVal}`);
                }
            })
            .catch(e => log('[REST] Error after retries: ' + e.message));
        }, 150);
    }
}

// Zone mode toggle handler - with aggressive throttling to prevent flooding
function toggleZoneMode() {
    // Prevent rapid toggling - check if already pending
    if (state.pendingZoneModeToggle) {
        log('[ZONE] Toggle already pending, ignoring rapid click');
        return;
    }

    // Use throttledAction for button-level throttling (150ms)
    if (!throttledAction(() => {
        const currentEnabled = state.zones ? state.zones.enabled : false;
        const newEnabled = !currentEnabled;
        
        log(`[ZONE] Toggling zone mode: ${currentEnabled} -> ${newEnabled}`);
        
        // Set pending flag to prevent rapid toggles (500ms cooldown)
        state.pendingZoneModeToggle = setTimeout(() => {
            state.pendingZoneModeToggle = null;
        }, 500);
        
        // Try WebSocket first
        if (state.connected && state.ws && state.ws.readyState === WebSocket.OPEN) {
            send({ type: 'zone.enable', enable: newEnabled });
            
            // Request updated zones state after a short delay
            setTimeout(() => {
                send({ type: 'zones.list' });
                fetchZonesState();
            }, 300);
        } else {
            log('[ZONE] WebSocket not connected, using REST API fallback');
            // REST API fallback
            fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zones/enabled`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ enabled: newEnabled })
            })
            .then(data => {
                if (data.success) {
                    log(`[REST] Zone mode set to: ${newEnabled}`);
                    // Refresh zones state after a short delay
                    setTimeout(() => {
                        fetchZonesState();
                    }, 300);
                } else {
                    log(`[REST] Error setting zone mode: ${data.error || 'Unknown error'}`);
                    // Clear pending flag on error
                    if (state.pendingZoneModeToggle) {
                        clearTimeout(state.pendingZoneModeToggle);
                        state.pendingZoneModeToggle = null;
                    }
                }
            })
            .catch(e => {
                log(`[REST] Error setting zone mode: ${e.message}`);
                // Clear pending flag on error
                if (state.pendingZoneModeToggle) {
                    clearTimeout(state.pendingZoneModeToggle);
                    state.pendingZoneModeToggle = null;
                }
            });
        }
        
        // Optimistically update UI (will be corrected by server response)
        if (state.zones) {
            state.zones.enabled = newEnabled;
            updateZoneModeUI();
            updateZonesUI();
        } else {
            // If zones state doesn't exist, fetch it
            fetchZonesState();
        }
    }, 'toggleZoneMode')) {
        return;
    }
}

// Zone speed control handlers (per-zone)
const zoneSpeedUpdateTimers = {};

function onZoneSpeedChange(zoneId) {
    const slider = elements[`zone${zoneId}SpeedSlider`];
    if (!slider) return;

    const newVal = parseInt(slider.value);
    if (!state.zones || !state.zones.zones || !state.zones.zones[zoneId]) return;

    const currentSpeed = state.zones.zones[zoneId].speed;
    if (newVal !== currentSpeed) {
        state.zones.zones[zoneId].speed = newVal;
        const valueEl = elements[`zone${zoneId}SpeedValue`];
        if (valueEl) valueEl.textContent = newVal;

        // Set pending flag to ignore stale server updates while dragging
        if (state.pendingZoneSpeedChange[zoneId]) {
            clearTimeout(state.pendingZoneSpeedChange[zoneId]);
        }
        state.pendingZoneSpeedChange[zoneId] = setTimeout(() => {
            state.pendingZoneSpeedChange[zoneId] = null;
        }, 1000);

        // Debounce API calls while dragging
        if (zoneSpeedUpdateTimers[zoneId]) {
            clearTimeout(zoneSpeedUpdateTimers[zoneId]);
        }

        zoneSpeedUpdateTimers[zoneId] = setTimeout(() => {
            // Only send if zones are enabled (prevents flooding when disabled)
            if (!state.zones || !state.zones.enabled) {
                log(`[ZONE] Zone mode is disabled, not sending speed change for zone ${zoneId}`);
                return;
            }

            // Try WebSocket
            if (state.connected && state.ws && state.ws.readyState === WebSocket.OPEN) {
                send({ type: 'zones.update', zoneId: zoneId, speed: newVal });
            }

            // REST API backup (only if zones are enabled)
            fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zones/${zoneId}/speed`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ speed: newVal })
            })
            .then(data => {
                if (data.success) {
                    log(`[REST] Zone ${zoneId} speed set to: ${newVal}`);
                }
            })
            .catch(e => {
                // 404 is OK if zones aren't fully initialized yet
                if (e.message && !e.message.includes('404')) {
                    log(`[REST] Error setting zone ${zoneId} speed: ${e.message}`);
                }
            });
        }, 300); // Increased from 150ms to 300ms to reduce flooding
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
    
    // Sync dropdown selection
    const select = document.getElementById('patternSelect');
    if (select && state.effectId !== undefined && state.effectId !== null) {
        select.value = state.effectId;
    }
}

// Get palette name from hardcoded lookup (instant, no API call)
function getPaletteName(paletteId) {
    if (paletteId >= 0 && paletteId < PALETTE_NAMES.length) {
        return PALETTE_NAMES[paletteId];
    }
    return `Palette ${paletteId}`;
}

// Fetch current parameters (including paletteId) from API
function fetchZonesState() {
    // Try WebSocket first
    if (state.connected && state.ws && state.ws.readyState === WebSocket.OPEN) {
        send({ type: 'zones.list' });
    }

    // REST API backup
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zones`, {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' }
    })
    .then(data => {
        if (data.success && data.data) {
            const zonesData = data.data;
            state.zones = {
                enabled: zonesData.enabled || false,
                zoneCount: zonesData.zoneCount || 0,
                zones: zonesData.zones || [],
                segments: zonesData.segments || []  // Parse segments array
            };
            log(`[REST] Zones state: ${zonesData.zoneCount} zones, enabled=${zonesData.enabled}`);
            
            // Update zone editor if segments are available
            if (state.zones.segments && state.zones.segments.length > 0) {
                state.zoneEditorSegments = JSON.parse(JSON.stringify(state.zones.segments));
                state.zoneEditorPreset = null;
            }
            
            updateZonesUI();
            updateZoneModeUI();
            updateZoneEditorUI();
        }
    })
    .catch(e => {
        // Zones may not be available - this is OK, just log and continue
        log(`[REST] Zones not available: ${e.message}`);
    });
}

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
                if (data.data.mood !== undefined) {
                    state.mood = data.data.mood;
                    updateMoodUI();
                }
                if (data.data.fadeAmount !== undefined) {
                    state.fadeAmount = data.data.fadeAmount;
                    updateFadeUI();
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
    
    // Sync dropdown selection
    const select = document.getElementById('paletteSelect');
    if (select && paletteId !== undefined && paletteId !== null) {
        select.value = paletteId;
    }
}

function updatePatternFilterUI() {
    const label = document.getElementById('patternFilterLabel');
    if (!label) return;
    
    const labels = {
        'all': 'All',
        'reactive': 'Audio',
        'ambient': 'Ambient'
    };
    label.textContent = labels[state.patternFilter] || 'All';
    
    // Optional: Add visual indicator (class for styling)
    const toggle = document.getElementById('patternFilterToggle');
    if (toggle) {
        toggle.classList.remove('filter-all', 'filter-reactive', 'filter-ambient');
        toggle.classList.add(`filter-${state.patternFilter}`);
    }
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

function updateFadeUI() {
    if (elements.fadeSlider) {
        elements.fadeSlider.value = state.fadeAmount;
    }
    if (elements.fadeValue) {
        elements.fadeValue.textContent = state.fadeAmount;
    }
}

function updateZoneModeUI() {
    if (!elements.zoneModeStatus || !elements.zoneModeInfo) return;

    if (state.zones) {
        elements.zoneModeStatus.textContent = state.zones.enabled ? 'ON' : 'OFF';
        elements.zoneModeInfo.textContent = state.zones.enabled 
            ? `${state.zones.zoneCount} zones` 
            : 'Disabled';
        
        // Update button state
        if (elements.zoneModeToggle) {
            elements.zoneModeToggle.disabled = false;
            elements.zoneModeToggle.textContent = state.zones.enabled ? 'Disable' : 'Enable';
        }
    } else {
        elements.zoneModeStatus.textContent = '--';
        elements.zoneModeInfo.textContent = 'Unknown';
        if (elements.zoneModeToggle) {
            elements.zoneModeToggle.disabled = true;
            elements.zoneModeToggle.textContent = 'Toggle';
        }
    }
}

function updateZonesUI() {
    const zoneRow = document.getElementById('zoneControlsRow');
    if (!zoneRow) return;

    // Show/hide zone controls based on state
    if (state.zones && state.zones.enabled && state.zones.zones && state.zones.zones.length > 0) {
        zoneRow.style.display = 'flex';
        
        // Update each zone slider (up to 3 zones)
        for (let i = 0; i < Math.min(3, state.zones.zones.length); i++) {
            const zone = state.zones.zones[i];
            if (!zone) continue;

            const slider = elements[`zone${i}SpeedSlider`];
            const value = elements[`zone${i}SpeedValue`];
            
            if (slider && zone.speed !== undefined) {
                // Only update if not pending (user is not dragging)
                if (!state.pendingZoneSpeedChange[i]) {
                    slider.value = zone.speed;
                }
            }
            if (value && zone.speed !== undefined) {
                value.textContent = zone.speed;
            }
        }
    } else {
        zoneRow.style.display = 'none';
    }
    
    // Update zone mode UI
    updateZoneModeUI();
}

// ─────────────────────────────────────────────────────────────
// Zone Editor Functions
// ─────────────────────────────────────────────────────────────

// Zone colour mapping
const ZONE_COLORS = ['#06b6d4', '#22c55e', '#a855f7', '#3b82f6'];  // cyan, green, purple, blue
const CENTER_LEFT = 79;
const CENTER_RIGHT = 80;
const MAX_LED = 159;

// Preset segment definitions (matching firmware ZoneDefinition.h)
const ZONE_PRESETS = {
    0: [ // Unified - uses 3-zone config
        { zoneId: 0, s1LeftStart: 65, s1LeftEnd: 79, s1RightStart: 80, s1RightEnd: 94, totalLeds: 30 },
        { zoneId: 1, s1LeftStart: 20, s1LeftEnd: 64, s1RightStart: 95, s1RightEnd: 139, totalLeds: 90 },
        { zoneId: 2, s1LeftStart: 0, s1LeftEnd: 19, s1RightStart: 140, s1RightEnd: 159, totalLeds: 40 }
    ],
    1: [ // Dual Split - uses 3-zone config
        { zoneId: 0, s1LeftStart: 65, s1LeftEnd: 79, s1RightStart: 80, s1RightEnd: 94, totalLeds: 30 },
        { zoneId: 1, s1LeftStart: 20, s1LeftEnd: 64, s1RightStart: 95, s1RightEnd: 139, totalLeds: 90 },
        { zoneId: 2, s1LeftStart: 0, s1LeftEnd: 19, s1RightStart: 140, s1RightEnd: 159, totalLeds: 40 }
    ],
    2: [ // Triple Rings - uses 3-zone config
        { zoneId: 0, s1LeftStart: 65, s1LeftEnd: 79, s1RightStart: 80, s1RightEnd: 94, totalLeds: 30 },
        { zoneId: 1, s1LeftStart: 20, s1LeftEnd: 64, s1RightStart: 95, s1RightEnd: 139, totalLeds: 90 },
        { zoneId: 2, s1LeftStart: 0, s1LeftEnd: 19, s1RightStart: 140, s1RightEnd: 159, totalLeds: 40 }
    ],
    3: [ // Quad Active - uses 4-zone config
        { zoneId: 0, s1LeftStart: 60, s1LeftEnd: 79, s1RightStart: 80, s1RightEnd: 99, totalLeds: 40 },
        { zoneId: 1, s1LeftStart: 40, s1LeftEnd: 59, s1RightStart: 100, s1RightEnd: 119, totalLeds: 40 },
        { zoneId: 2, s1LeftStart: 20, s1LeftEnd: 39, s1RightStart: 120, s1RightEnd: 139, totalLeds: 40 },
        { zoneId: 3, s1LeftStart: 0, s1LeftEnd: 19, s1RightStart: 140, s1RightEnd: 159, totalLeds: 40 }
    ],
    4: [ // Heartbeat Focus - uses 3-zone config
        { zoneId: 0, s1LeftStart: 65, s1LeftEnd: 79, s1RightStart: 80, s1RightEnd: 94, totalLeds: 30 },
        { zoneId: 1, s1LeftStart: 20, s1LeftEnd: 64, s1RightStart: 95, s1RightEnd: 139, totalLeds: 90 },
        { zoneId: 2, s1LeftStart: 0, s1LeftEnd: 19, s1RightStart: 140, s1RightEnd: 159, totalLeds: 40 }
    ]
};

// Get zone ID for a given LED
function getZoneForLed(ledIndex, isRight) {
    for (const seg of state.zoneEditorSegments) {
        if (isRight) {
            if (ledIndex >= seg.s1RightStart && ledIndex <= seg.s1RightEnd) {
                return seg.zoneId;
            }
        } else {
            if (ledIndex >= seg.s1LeftStart && ledIndex <= seg.s1LeftEnd) {
                return seg.zoneId;
            }
        }
    }
    return -1;
}

// Render LED strips with proper mirroring
function renderLedStrips() {
    const leftStrip = document.getElementById('leftLedStrip');
    const rightStrip = document.getElementById('rightLedStrip');
    if (!leftStrip || !rightStrip) return;

    leftStrip.innerHTML = '';
    rightStrip.innerHTML = '';

    // Left strip: 0-79, displayed REVERSED (79 at right edge meeting centre, 0 at left edge)
    // Add LEDs in reverse order (79 down to 0) so 79 appears at right edge after flex-row-reverse
    for (let i = 79; i >= 0; i--) {
        const led = document.createElement('div');
        const zoneId = getZoneForLed(i, false);
        const isCenter = i === CENTER_LEFT;
        
        led.style.flex = '1';
        led.style.minWidth = '2px';
        led.style.height = '100%';
        led.style.borderRadius = '1px';
        led.style.border = '1px solid';
        
        if (zoneId >= 0) {
            led.style.background = ZONE_COLORS[zoneId % ZONE_COLORS.length];
            led.style.borderColor = ZONE_COLORS[zoneId % ZONE_COLORS.length];
            led.style.opacity = '0.8';
        } else {
            led.style.background = 'var(--bg-elevated)';
            led.style.borderColor = 'var(--border-subtle)';
        }
        
        // Center LED indicator handled by divider line, no box-shadow needed
        led.title = `LED ${i}${isCenter ? ' (Centre)' : ''}`;
        leftStrip.appendChild(led);
    }

    // Right strip: 80-159, displayed normally (80 at left edge, 159 at right edge)
    for (let i = 80; i <= 159; i++) {
        const led = document.createElement('div');
        const zoneId = getZoneForLed(i, true);
        const isCenter = i === CENTER_RIGHT;
        
        led.style.flex = '1';
        led.style.minWidth = '2px';
        led.style.height = '100%';
        led.style.borderRadius = '1px';
        led.style.border = '1px solid';
        
        if (zoneId >= 0) {
            led.style.background = ZONE_COLORS[zoneId % ZONE_COLORS.length];
            led.style.borderColor = ZONE_COLORS[zoneId % ZONE_COLORS.length];
            led.style.opacity = '0.8';
        } else {
            led.style.background = 'var(--bg-elevated)';
            led.style.borderColor = 'var(--border-subtle)';
        }
        
        // Center LED indicator handled by divider line, no box-shadow needed
        led.title = `LED ${i}${isCenter ? ' (Centre)' : ''}`;
        rightStrip.appendChild(led);
    }
}

// Validate zone layout
function validateZoneLayout(segments) {
    const errors = [];
    
    if (!segments || segments.length === 0) {
        errors.push('At least one zone is required');
        return errors;
    }
    
    if (segments.length > 4) {
        errors.push('Maximum 4 zones allowed');
        return errors;
    }
    
    // Check each segment
    for (let i = 0; i < segments.length; i++) {
        const seg = segments[i];
        
        // Check zoneId matches index
        if (seg.zoneId !== i) {
            errors.push(`Zone ${i}: Zone ID must be ${i}`);
        }
        
        // Check bounds
        if (seg.s1LeftStart < 0 || seg.s1LeftStart > CENTER_LEFT) {
            errors.push(`Zone ${i}: Left start must be 0-${CENTER_LEFT}`);
        }
        if (seg.s1LeftEnd < seg.s1LeftStart || seg.s1LeftEnd > CENTER_LEFT) {
            errors.push(`Zone ${i}: Left end must be >= start and <= ${CENTER_LEFT}`);
        }
        if (seg.s1RightStart < CENTER_RIGHT || seg.s1RightStart > MAX_LED) {
            errors.push(`Zone ${i}: Right start must be ${CENTER_RIGHT}-${MAX_LED}`);
        }
        if (seg.s1RightEnd < seg.s1RightStart || seg.s1RightEnd > MAX_LED) {
            errors.push(`Zone ${i}: Right end must be >= start and <= ${MAX_LED}`);
        }
        
        // Check centre-origin symmetry
        const leftSize = seg.s1LeftEnd - seg.s1LeftStart + 1;
        const rightSize = seg.s1RightEnd - seg.s1RightStart + 1;
        if (leftSize !== rightSize) {
            errors.push(`Zone ${i}: Left and right segments must have equal size`);
        }
        const leftDistance = CENTER_LEFT - seg.s1LeftEnd;
        const rightDistance = seg.s1RightStart - CENTER_RIGHT;
        if (leftDistance !== rightDistance) {
            errors.push(`Zone ${i}: Segments must be symmetric around centre pair (79/80)`);
        }
        
        // Check minimum size
        if (leftSize < 1 || rightSize < 1) {
            errors.push(`Zone ${i}: Each segment must have at least 1 LED`);
        }
    }
    
    // Check for overlaps and coverage
    const leftCoverage = new Set();
    const rightCoverage = new Set();
    
    for (let i = 0; i < segments.length; i++) {
        const seg = segments[i];
        
        for (let led = seg.s1LeftStart; led <= seg.s1LeftEnd; led++) {
            if (leftCoverage.has(led)) {
                errors.push(`LED ${led} on left strip is assigned to multiple zones`);
            }
            leftCoverage.add(led);
        }
        
        for (let led = seg.s1RightStart; led <= seg.s1RightEnd; led++) {
            if (rightCoverage.has(led)) {
                errors.push(`LED ${led} on right strip is assigned to multiple zones`);
            }
            rightCoverage.add(led);
        }
    }
    
    // Check complete coverage
    for (let led = 0; led <= CENTER_LEFT; led++) {
        if (!leftCoverage.has(led)) {
            errors.push(`LED ${led} on left strip is not assigned to any zone`);
        }
    }
    for (let led = CENTER_RIGHT; led <= MAX_LED; led++) {
        if (!rightCoverage.has(led)) {
            errors.push(`LED ${led} on right strip is not assigned to any zone`);
        }
    }
    
    // Check centre pair inclusion
    let includesCentre = false;
    for (const seg of segments) {
        if (seg.s1LeftEnd >= CENTER_LEFT || seg.s1RightStart <= CENTER_RIGHT) {
            includesCentre = true;
            break;
        }
    }
    if (!includesCentre) {
        errors.push('At least one zone must include the centre pair (LEDs 79/80)');
    }
    
    return errors;
}

// Calculate mirrored right segment values from left segment values
function calculateMirroredRightSegment(seg) {
    // Validate left segment values
    if (seg.s1LeftStart > seg.s1LeftEnd || seg.s1LeftStart < 0 || seg.s1LeftEnd > CENTER_LEFT) {
        return seg; // Return unchanged if invalid
    }
    
    const leftSize = seg.s1LeftEnd - seg.s1LeftStart + 1;
    const distanceFromCenter = CENTER_LEFT - seg.s1LeftEnd;
    const s1RightStart = CENTER_RIGHT + distanceFromCenter;
    const s1RightEnd = s1RightStart + leftSize - 1;
    
    return {
        ...seg,
        s1RightStart: Math.max(CENTER_RIGHT, Math.min(MAX_LED, s1RightStart)),
        s1RightEnd: Math.max(s1RightStart, Math.min(MAX_LED, s1RightEnd))
    };
}

// Zone control functions (palette, effect, blend)
function setZonePalette(zoneId, paletteId) {
    if (state.connected && state.ws) {
        // WebSocket preferred
        send({ type: 'zone.setPalette', zoneId, paletteId });
        log(`[WS] Zone ${zoneId} palette: ${paletteId}`);
    } else {
        // REST fallback
        fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zones/${zoneId}/palette`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ paletteId })
        })
        .then(data => {
            if (data.success) {
                log(`[REST] Zone ${zoneId} palette: ${paletteId}`);
                fetchZonesState(); // Refresh zones state
            }
        })
        .catch(e => {
            log(`[REST] ❌ Error setting zone ${zoneId} palette: ${e.message}`);
        });
    }
}

function setZoneEffect(zoneId, effectId) {
    if (state.connected && state.ws) {
        // WebSocket preferred
        send({ type: 'zone.setEffect', zoneId, effectId });
        log(`[WS] Zone ${zoneId} effect: ${effectId}`);
    } else {
        // REST fallback
        fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zones/${zoneId}/effect`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ effectId })
        })
        .then(data => {
            if (data.success) {
                log(`[REST] Zone ${zoneId} effect: ${effectId}`);
                fetchZonesState(); // Refresh zones state
            }
        })
        .catch(e => {
            log(`[REST] ❌ Error setting zone ${zoneId} effect: ${e.message}`);
        });
    }
}

function setZoneBlend(zoneId, blendMode) {
    if (state.connected && state.ws) {
        // WebSocket preferred (will be implemented in next todo)
        send({ type: 'zone.setBlend', zoneId, blendMode });
        log(`[WS] Zone ${zoneId} blend: ${blendMode}`);
    } else {
        // REST fallback
        fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zones/${zoneId}/blend`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ blendMode })
        })
        .then(data => {
            if (data.success) {
                log(`[REST] Zone ${zoneId} blend: ${blendMode}`);
                fetchZonesState(); // Refresh zones state
            }
        })
        .catch(e => {
            log(`[REST] ❌ Error setting zone ${zoneId} blend: ${e.message}`);
        });
    }
}

// Render zone segment editors
function renderZoneSegments() {
    const container = document.getElementById('zoneSegmentsList');
    if (!container) return;
    
    container.innerHTML = '';
    
    state.zoneEditorSegments.forEach((seg, index) => {
        const zoneDiv = document.createElement('div');
        zoneDiv.style.background = 'var(--bg-elevated)';
        zoneDiv.style.border = `1px solid ${ZONE_COLORS[seg.zoneId % ZONE_COLORS.length]}`;
        zoneDiv.style.borderRadius = '8px';
        zoneDiv.style.padding = 'var(--space-sm)';
        
        // Get current zone data (if available)
        const zoneData = state.zones && state.zones.zones && state.zones.zones[seg.zoneId] ? state.zones.zones[seg.zoneId] : null;
        const currentPaletteId = zoneData ? (zoneData.paletteId !== undefined ? zoneData.paletteId : 0) : 0;
        const currentEffectId = zoneData ? (zoneData.effectId !== undefined ? zoneData.effectId : 0) : 0;
        const currentBlendMode = zoneData ? (zoneData.blendMode !== undefined ? zoneData.blendMode : 0) : 0;
        
        // Build palette dropdown options
        let paletteOptions = '<option value="0">Global</option>';
        if (state.palettesList && state.palettesList.length > 0) {
            state.palettesList.forEach(pal => {
                paletteOptions += `<option value="${pal.id}" ${pal.id === currentPaletteId ? 'selected' : ''}>${pal.name}</option>`;
            });
        }
        
        // Build effect dropdown options
        let effectOptions = '';
        if (state.effectsList && state.effectsList.length > 0) {
            state.effectsList.forEach(eff => {
                effectOptions += `<option value="${eff.id}" ${eff.id === currentEffectId ? 'selected' : ''}>${eff.name || `Effect ${eff.id}`}</option>`;
            });
        } else {
            effectOptions = `<option value="${currentEffectId}">Effect ${currentEffectId}</option>`;
        }
        
        // Blend mode options (hardcoded from BlendMode.h)
        const blendModes = [
            { value: 0, name: 'Overwrite' },
            { value: 1, name: 'Additive' },
            { value: 2, name: 'Multiply' },
            { value: 3, name: 'Screen' },
            { value: 4, name: 'Overlay' },
            { value: 5, name: 'Alpha' },
            { value: 6, name: 'Lighten' },
            { value: 7, name: 'Darken' }
        ];
        let blendOptions = '';
        blendModes.forEach(mode => {
            blendOptions += `<option value="${mode.value}" ${mode.value === currentBlendMode ? 'selected' : ''}>${mode.name}</option>`;
        });
        
        zoneDiv.innerHTML = `
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: var(--space-xs);">
                <div class="value-display compact" style="font-size: 0.75rem;">Zone ${seg.zoneId}</div>
                <div class="value-secondary compact" style="font-size: 0.5625rem;">${seg.totalLeds} LEDs</div>
            </div>
            <div style="display: grid; grid-template-columns: 1fr 1fr; gap: var(--space-sm); margin-bottom: var(--space-sm);">
                <div>
                    <div class="value-secondary compact" style="font-size: 0.5625rem; margin-bottom: 2px;">Left Segment</div>
                    <div style="display: flex; gap: var(--space-xs);">
                        <div style="flex: 1;">
                            <div class="value-secondary compact" style="font-size: 0.5rem; margin-bottom: 2px;">Start</div>
                            <input type="number" min="0" max="${CENTER_LEFT}" value="${seg.s1LeftStart}" 
                                   data-zone="${index}" data-field="s1LeftStart" 
                                   style="width: 100%; padding: 4px 6px; background: var(--bg-card); border: 1px solid var(--border-subtle); border-radius: 4px; color: var(--text-primary); font-size: 0.75rem;">
                        </div>
                        <div style="flex: 1;">
                            <div class="value-secondary compact" style="font-size: 0.5rem; margin-bottom: 2px;">End</div>
                            <input type="number" min="${seg.s1LeftStart}" max="${CENTER_LEFT}" value="${seg.s1LeftEnd}" 
                                   data-zone="${index}" data-field="s1LeftEnd" 
                                   style="width: 100%; padding: 4px 6px; background: var(--bg-card); border: 1px solid var(--border-subtle); border-radius: 4px; color: var(--text-primary); font-size: 0.75rem;">
                        </div>
                    </div>
                </div>
                <div>
                    <div class="value-secondary compact" style="font-size: 0.5625rem; margin-bottom: 2px;">Right Segment</div>
                    <div style="display: flex; gap: var(--space-xs);">
                        <div style="flex: 1;">
                            <div class="value-secondary compact" style="font-size: 0.5rem; margin-bottom: 2px;">Start</div>
                            <input type="number" min="${CENTER_RIGHT}" max="${MAX_LED}" value="${seg.s1RightStart}" 
                                   data-zone="${index}" data-field="s1RightStart" 
                                   style="width: 100%; padding: 4px 6px; background: var(--bg-card); border: 1px solid var(--border-subtle); border-radius: 4px; color: var(--text-primary); font-size: 0.75rem;">
                        </div>
                        <div style="flex: 1;">
                            <div class="value-secondary compact" style="font-size: 0.5rem; margin-bottom: 2px;">End</div>
                            <input type="number" min="${seg.s1RightStart}" max="${MAX_LED}" value="${seg.s1RightEnd}" 
                                   data-zone="${index}" data-field="s1RightEnd" 
                                   style="width: 100%; padding: 4px 6px; background: var(--bg-card); border: 1px solid var(--border-subtle); border-radius: 4px; color: var(--text-primary); font-size: 0.75rem;">
                        </div>
                    </div>
                </div>
            </div>
            <div style="display: grid; grid-template-columns: 1fr 1fr 1fr; gap: var(--space-sm);">
                <div>
                    <div class="value-secondary compact" style="font-size: 0.5625rem; margin-bottom: 2px;">Palette</div>
                    <select data-zone="${seg.zoneId}" data-type="palette" 
                            style="width: 100%; padding: 4px 6px; background: var(--bg-card); border: 1px solid var(--border-subtle); border-radius: 4px; color: var(--text-primary); font-size: 0.75rem;">
                        ${paletteOptions}
                    </select>
                </div>
                <div>
                    <div class="value-secondary compact" style="font-size: 0.5625rem; margin-bottom: 2px;">Effect</div>
                    <select data-zone="${seg.zoneId}" data-type="effect" 
                            style="width: 100%; padding: 4px 6px; background: var(--bg-card); border: 1px solid var(--border-subtle); border-radius: 4px; color: var(--text-primary); font-size: 0.75rem;">
                        ${effectOptions}
                    </select>
                </div>
                <div>
                    <div class="value-secondary compact" style="font-size: 0.5625rem; margin-bottom: 2px;">Blend Mode</div>
                    <select data-zone="${seg.zoneId}" data-type="blend" 
                            style="width: 100%; padding: 4px 6px; background: var(--bg-card); border: 1px solid var(--border-subtle); border-radius: 4px; color: var(--text-primary); font-size: 0.75rem;">
                        ${blendOptions}
                    </select>
                </div>
            </div>
        `;
        
        container.appendChild(zoneDiv);
    });
    
    // Attach input handlers
    container.querySelectorAll('input').forEach(input => {
        input.addEventListener('input', (e) => {
            const zoneIndex = parseInt(e.target.dataset.zone);
            const field = e.target.dataset.field;
            const value = parseInt(e.target.value) || 0;
            
            state.zoneEditorSegments[zoneIndex][field] = value;
            state.zoneEditorPreset = null;  // Clear preset selection
            
            // Recalculate totalLeds
            const seg = state.zoneEditorSegments[zoneIndex];
            const leftSize = seg.s1LeftEnd - seg.s1LeftStart + 1;
            const rightSize = seg.s1RightEnd - seg.s1RightStart + 1;
            seg.totalLeds = leftSize + rightSize; // Per-strip count (strip 2 mirrors strip 1)
            
            // Update UI
            renderLedStrips();
            renderZoneSegments();
            updateZoneEditorValidation();
        });
    });
    
    // Attach dropdown handlers for palette, effect, and blend
    container.querySelectorAll('select[data-type]').forEach(select => {
        select.addEventListener('change', (e) => {
            const zoneId = parseInt(e.target.dataset.zone);
            const type = e.target.dataset.type;
            const value = parseInt(e.target.value);
            
            if (type === 'palette') {
                setZonePalette(zoneId, value);
            } else if (type === 'effect') {
                setZoneEffect(zoneId, value);
            } else if (type === 'blend') {
                setZoneBlend(zoneId, value);
            }
        });
    });
}

// Update zone editor validation
function updateZoneEditorValidation() {
    const errors = validateZoneLayout(state.zoneEditorSegments);
    const errorDiv = document.getElementById('zoneValidationErrors');
    const errorList = document.getElementById('zoneValidationErrorsList');
    const applyBtn = document.getElementById('zoneApplyButton');
    
    if (!errorDiv || !errorList || !applyBtn) return;
    
    if (errors.length > 0) {
        errorDiv.style.display = 'block';
        errorList.innerHTML = '';
        errors.forEach(err => {
            const li = document.createElement('li');
            li.className = 'value-secondary compact';
            li.style.fontSize = '0.5625rem';
            li.style.color = 'var(--error)';
            li.textContent = err;
            errorList.appendChild(li);
        });
        applyBtn.disabled = true;
    } else {
        errorDiv.style.display = 'none';
        applyBtn.disabled = false;
    }
}

// Update zone editor UI
function updateZoneEditorUI() {
    const editorRow = document.getElementById('zoneEditorRow');
    if (!editorRow) return;
    
    // Show/hide based on zones enabled
    if (state.zones && state.zones.enabled && state.zoneEditorSegments.length > 0) {
        editorRow.style.display = 'flex';
        renderLedStrips();
        renderZoneSegments();
        updateZoneEditorValidation();
        
        // Update zone count selector
        const zoneCountSelect = document.getElementById('zoneCountSelect');
        if (zoneCountSelect && state.zoneEditorSegments.length > 0) {
            zoneCountSelect.value = state.zoneEditorSegments.length;
        }
        
        // Update preset selector
        const presetSelect = document.getElementById('zonePresetSelect');
        if (presetSelect) {
            presetSelect.value = state.zoneEditorPreset !== null ? state.zoneEditorPreset : -1;
        }
    } else {
        editorRow.style.display = 'none';
    }
}

// Handle preset selection
// Generate valid segments for a given zone count (1-4)
function generateZoneSegments(zoneCount) {
    if (zoneCount < 1 || zoneCount > 4) {
        log(`[ZONE EDITOR] Invalid zone count: ${zoneCount}`);
        return null;
    }
    
    const segments = [];
    const LEDsPerSide = 80; // 0-79 left, 80-159 right
    const centerLeft = 79;
    const centerRight = 80;
    
    // Distribute LEDs evenly across zones, centre-out
    // Each zone gets approximately LEDsPerSide / zoneCount LEDs per side
    const ledsPerZone = Math.floor(LEDsPerSide / zoneCount);
    const remainder = LEDsPerSide % zoneCount;
    
    // Build zones from centre outward
    let leftEnd = centerLeft;
    let rightStart = centerRight;
    
    for (let i = 0; i < zoneCount; i++) {
        // Calculate zone size (give remainder to outermost zones)
        const zoneSize = ledsPerZone + (i >= zoneCount - remainder ? 1 : 0);
        
        // Left segment (descending from centre)
        const leftStart = leftEnd - zoneSize + 1;
        
        // Right segment (ascending from centre)
        const rightEnd = rightStart + zoneSize - 1;
        
        segments.push({
            zoneId: i,
            s1LeftStart: leftStart,
            s1LeftEnd: leftEnd,
            s1RightStart: rightStart,
            s1RightEnd: rightEnd,
            totalLeds: zoneSize * 2 // Both sides
        });
        
        // Move outward for next zone
        leftEnd = leftStart - 1;
        rightStart = rightEnd + 1;
    }
    
    // Reverse to get centre-out order (zone 0 = innermost)
    segments.reverse();
    
    // Re-assign zone IDs to match order
    segments.forEach((seg, idx) => {
        seg.zoneId = idx;
    });
    
    return segments;
}

function handleZoneCountSelect(zoneCount) {
    const segments = generateZoneSegments(zoneCount);
    if (segments) {
        state.zoneEditorSegments = segments;
        state.zoneEditorPreset = null; // Clear preset when using custom count
        renderLedStrips();
        renderZoneSegments();
        updateZoneEditorValidation();
        log(`[ZONE EDITOR] Generated ${zoneCount} zones`);
    }
}

function handleZonePresetSelect(presetId) {
    if (presetId === -1) {
        state.zoneEditorPreset = null;
        return;
    }
    
    const preset = ZONE_PRESETS[presetId];
    if (preset) {
        state.zoneEditorSegments = JSON.parse(JSON.stringify(preset));
        state.zoneEditorPreset = presetId;
        // Update zone count selector to match preset
        const zoneCountSelect = document.getElementById('zoneCountSelect');
        if (zoneCountSelect && preset.length > 0) {
            zoneCountSelect.value = preset.length;
        }
        renderLedStrips();
        renderZoneSegments();
        updateZoneEditorValidation();
    }
}

// Apply zone layout
function applyZoneLayout() {
    const errors = validateZoneLayout(state.zoneEditorSegments);
    if (errors.length > 0) {
        log(`[ZONE EDITOR] Validation failed: ${errors.join(', ')}`);
        return;
    }
    
    log(`[ZONE EDITOR] Applying layout with ${state.zoneEditorSegments.length} zones`);
    
    // Try WebSocket first
    if (state.connected && state.ws && state.ws.readyState === WebSocket.OPEN) {
        send({ type: 'zones.setLayout', zones: state.zoneEditorSegments });
        log(`[ZONE EDITOR] Sent via WebSocket`);
    } else {
        // REST API fallback
        fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zones/layout`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ zones: state.zoneEditorSegments })
        })
        .then(data => {
            if (data.success) {
                log(`[ZONE EDITOR] ✅ Layout applied via REST`);
                // Refresh zones state
                fetchZonesState();
            } else {
                log(`[ZONE EDITOR] ❌ Failed: ${JSON.stringify(data)}`);
            }
        })
        .catch(e => {
            log(`[ZONE EDITOR] ❌ Error: ${e.message}`);
        });
    }
}

function updateAllUI() {
    updateConnectionUI();
    updatePatternUI();
    updatePaletteUI();
    updateBrightnessUI();
    updateSpeedUI();
    updateMoodUI();
    updateFadeUI();
    updateZonesUI();
    updateZoneModeUI();
    updateZoneEditorUI();
    updatePluginUI();
}

// ─────────────────────────────────────────────────────────────
// Plugin UI
// ─────────────────────────────────────────────────────────────

function updatePluginUI() {
    const section = document.getElementById('pluginSection');
    const effectCount = document.getElementById('pluginEffectCount');
    const modeStatus = document.getElementById('pluginModeStatus');
    const reloadStatus = document.getElementById('pluginReloadStatus');
    const errorList = document.getElementById('pluginErrorList');

    if (!section || !state.plugins) {
        if (section) section.style.display = 'none';
        return;
    }

    // Show the section
    section.style.display = '';

    // Effect count
    effectCount.textContent = `${state.plugins.registeredCount || 0} effects`;

    // Mode status
    if (state.plugins.overrideModeEnabled) {
        modeStatus.textContent = `Override mode (${state.plugins.disabledByOverride || 0} disabled)`;
    } else {
        modeStatus.textContent = `Additive mode (${state.plugins.manifestCount || 0} manifests)`;
    }

    // Reload status
    if (state.plugins.lastReloadMillis > 0) {
        const reloadTime = new Date(state.plugins.lastReloadMillis).toLocaleTimeString();
        const status = state.plugins.lastReloadOk ? 'OK' : 'FAIL';
        reloadStatus.textContent = `Last reload: ${status} @ ${reloadTime}`;
        reloadStatus.style.display = '';
        reloadStatus.style.color = state.plugins.lastReloadOk ? 'var(--success)' : 'var(--error)';
    } else {
        reloadStatus.style.display = 'none';
    }

    // Error list
    if (state.plugins.errors && state.plugins.errors.length > 0) {
        errorList.innerHTML = state.plugins.errors.map(e =>
            `<div style="color: var(--error); margin-bottom: 4px;">${e.file}: ${e.error}</div>`
        ).join('');
        errorList.style.display = '';
    } else if (state.plugins.errorCount > 0) {
        errorList.innerHTML = `<div style="color: var(--error);">${state.plugins.errorCount} manifest errors</div>`;
        if (state.plugins.lastErrorSummary) {
            errorList.innerHTML += `<div style="color: var(--error); font-size: 0.65rem; margin-top: 2px;">${state.plugins.lastErrorSummary}</div>`;
        }
        errorList.style.display = '';
    } else {
        errorList.style.display = 'none';
    }
}

function fetchPluginStats() {
    if (state.ws && state.ws.readyState === WebSocket.OPEN) {
        state.ws.send(JSON.stringify({ type: 'plugins.stats' }));
    }
}

function reloadPlugins() {
    if (state.ws && state.ws.readyState === WebSocket.OPEN) {
        log('[PLUGINS] Requesting reload...');
        state.ws.send(JSON.stringify({ type: 'plugins.reload' }));
    }
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
// Palette and Effect Lists (for dropdowns)
// ─────────────────────────────────────────────────────────────

function fetchPalettesList() {
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/palettes?limit=75`, {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' }
    })
    .then(data => {
        if (data.success && data.data && data.data.palettes) {
            state.palettesList = data.data.palettes;
            log(`[REST] ✅ Loaded ${state.palettesList.length} palettes`);
            populatePalettesDropdown();
        }
    })
    .catch(e => {
        log(`[REST] ❌ Error fetching palettes: ${e.message}`);
    });
}

function fetchEffectsList() {
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/effects`, {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' }
    })
    .then(data => {
        if (data.success && data.data && data.data.effects) {
            state.effectsList = data.data.effects;
            log(`[REST] ✅ Loaded ${state.effectsList.length} effects`);
            updatePatternFilterUI();
            populateEffectsDropdown();
        }
    })
    .catch(e => {
        log(`[REST] ❌ Error fetching effects: ${e.message}`);
    });
}

function populateEffectsDropdown() {
    const select = document.getElementById('patternSelect');
    if (!select || !state.effectsList || state.effectsList.length === 0) return;
    
    // Filter effects based on current filter mode
    let filteredEffects = state.effectsList;
    if (state.patternFilter === 'reactive') {
        filteredEffects = state.effectsList.filter(eff => eff.isAudioReactive === true);
    } else if (state.patternFilter === 'ambient') {
        filteredEffects = state.effectsList.filter(eff => eff.isAudioReactive !== true);
    }
    
    select.innerHTML = filteredEffects.map(eff => 
        `<option value="${eff.id}">${eff.name || `Effect ${eff.id}`}</option>`
    ).join('');
    
    // Set current selection if effectId is known and is in filtered list
    if (state.effectId !== undefined && state.effectId !== null) {
        const currentInFilter = filteredEffects.some(eff => eff.id === state.effectId);
        if (currentInFilter) {
            select.value = state.effectId;
        } else if (filteredEffects.length > 0) {
            // If current effect not in filter, select first in filtered list
            select.value = filteredEffects[0].id;
        }
    }
}

function populatePalettesDropdown() {
    const select = document.getElementById('paletteSelect');
    if (!select || !state.palettesList || state.palettesList.length === 0) return;
    
    select.innerHTML = state.palettesList.map(pal => 
        `<option value="${pal.id}">${pal.name || `Palette ${pal.id}`}</option>`
    ).join('');
    
    // Set current selection if paletteId is known
    if (state.paletteId !== undefined && state.paletteId !== null) {
        select.value = state.paletteId;
    }
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
    elements.fadeSlider = document.getElementById('fadeSlider');
    elements.fadeValue = document.getElementById('fadeValue');
    elements.configBar = document.getElementById('configBar');
    elements.deviceHost = document.getElementById('deviceHost');
    elements.connectBtn = document.getElementById('connectBtn');
    elements.beatIndicator = document.getElementById('beatIndicator');
    elements.bpmDisplay = document.getElementById('bpmDisplay');
    elements.bpmValue = document.getElementById('bpmValue');
    
    // Zone mode controls
    elements.zoneModeStatus = document.getElementById('zoneModeStatus');
    elements.zoneModeInfo = document.getElementById('zoneModeInfo');
    elements.zoneModeToggle = document.getElementById('zoneModeToggle');
    
    // Zone speed sliders
    elements.zone0SpeedSlider = document.getElementById('zone0SpeedSlider');
    elements.zone0SpeedValue = document.getElementById('zone0SpeedValue');
    elements.zone1SpeedSlider = document.getElementById('zone1SpeedSlider');
    elements.zone1SpeedValue = document.getElementById('zone1SpeedValue');
    elements.zone2SpeedSlider = document.getElementById('zone2SpeedSlider');
    elements.zone2SpeedValue = document.getElementById('zone2SpeedValue');

    // Bind button events
    document.getElementById('patternPrev').addEventListener('click', prevPattern);
    document.getElementById('patternNext').addEventListener('click', nextPattern);
    document.getElementById('palettePrev').addEventListener('click', prevPalette);
    document.getElementById('paletteNext').addEventListener('click', nextPalette);
    
    // Bind filter toggle event
    const patternFilterToggle = document.getElementById('patternFilterToggle');
    if (patternFilterToggle) {
        patternFilterToggle.addEventListener('click', () => {
            // Cycle: all -> reactive -> ambient -> all
            if (state.patternFilter === 'all') {
                state.patternFilter = 'reactive';
            } else if (state.patternFilter === 'reactive') {
                state.patternFilter = 'ambient';
            } else {
                state.patternFilter = 'all';
            }
            updatePatternFilterUI();
            populateEffectsDropdown();
        });
    }
    
    // Bind dropdown change events
    const patternSelect = document.getElementById('patternSelect');
    if (patternSelect) {
        patternSelect.addEventListener('change', (e) => {
            const effectId = parseInt(e.target.value);
            if (!isNaN(effectId)) {
                setEffect(effectId);
            }
        });
    }
    
    const paletteSelect = document.getElementById('paletteSelect');
    if (paletteSelect) {
        paletteSelect.addEventListener('change', (e) => {
            const paletteId = parseInt(e.target.value);
            if (!isNaN(paletteId)) {
                setPalette(paletteId);
            }
        });
    }
    if (elements.zoneModeToggle) {
        elements.zoneModeToggle.addEventListener('click', toggleZoneMode);
    }

    // Plugin reload button
    const pluginReloadBtn = document.getElementById('pluginReloadBtn');
    if (pluginReloadBtn) {
        pluginReloadBtn.addEventListener('click', reloadPlugins);
    }
    
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
    if (elements.fadeSlider) {
        elements.fadeSlider.addEventListener('input', onFadeChange);
        elements.fadeSlider.addEventListener('change', onFadeChange);
    }
    
    // Bind zone speed slider events
    if (elements.zone0SpeedSlider) {
        elements.zone0SpeedSlider.addEventListener('input', () => onZoneSpeedChange(0));
    }
    if (elements.zone1SpeedSlider) {
        elements.zone1SpeedSlider.addEventListener('input', () => onZoneSpeedChange(1));
    }
    if (elements.zone2SpeedSlider) {
        elements.zone2SpeedSlider.addEventListener('input', () => onZoneSpeedChange(2));
    }
    
    // Bind zone editor events
    const zoneCountSelect = document.getElementById('zoneCountSelect');
    const zonePresetSelect = document.getElementById('zonePresetSelect');
    const zoneApplyButton = document.getElementById('zoneApplyButton');
    if (zoneCountSelect) {
        zoneCountSelect.addEventListener('change', (e) => {
            handleZoneCountSelect(parseInt(e.target.value));
        });
    }
    if (zonePresetSelect) {
        zonePresetSelect.addEventListener('change', (e) => {
            handleZonePresetSelect(parseInt(e.target.value));
        });
    }
    if (zoneApplyButton) {
        zoneApplyButton.addEventListener('click', applyZoneLayout);
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
