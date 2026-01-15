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
    pendingZoneBrightnessChange: {}, // Object: { [zoneId]: timerId } for per-zone brightness debouncing
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

    // Beat tracking
    currentBpm: 0,
    bpmConfidence: 0,
    lastBeatTime: 0,

    // Shows state
    shows: {
        list: [],              // Array of {id, name, durationSeconds, type: 'builtin'|'custom'}
        current: null,         // Current show being edited/played {id, name, durationSeconds, scenes, isSaved}
        scenes: [],            // Array of TimelineScene objects
        isPlaying: false,
        isPaused: false,
        progress: 0,           // 0.0 - 1.0
        elapsedMs: 0,
        remainingMs: 0,
        currentChapter: 0,
        loading: false,
        error: null,
        dragState: {           // Drag-and-drop state
            isDragging: false,
            sceneId: null,
            startX: 0,
            offsetX: 0,
            currentZoneId: null,
            originalStartPercent: 0,
            originalZoneId: 0
        }
    },

    // Preset library throttling
    lastPresetListFetchMs: 0,
    presetListFetchThrottleMs: 3000,  // Only fetch once every 3 seconds max

    // Effect modifiers state
    modifiers: {
        stack: [],           // Array of {type, name, enabled, params}
        selectedType: null,  // Type being edited
        pendingUpdate: null  // Timer for parameter debouncing
    },

    // Color correction state
    colorCorrection: {
        mode: 3,                    // 0=OFF, 1=HSV, 2=RGB, 3=BOTH
        autoExposureEnabled: true,
        autoExposureTarget: 110,
        brownGuardrailEnabled: true,
        gammaEnabled: true,
        gammaValue: 2.2,
        currentPreset: 2            // 0=Off, 1=Subtle, 2=Balanced, 3=Aggressive
    },

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

        // Clear pending zone speed/brightness changes
        for (const zoneId in state.pendingZoneSpeedChange) {
            if (state.pendingZoneSpeedChange[zoneId]) {
                clearTimeout(state.pendingZoneSpeedChange[zoneId]);
            }
        }
        state.pendingZoneSpeedChange = {};

        for (const zoneId in state.pendingZoneBrightnessChange) {
            if (state.pendingZoneBrightnessChange[zoneId]) {
                clearTimeout(state.pendingZoneBrightnessChange[zoneId]);
            }
        }
        state.pendingZoneBrightnessChange = {};

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
            fetchShowsList();
            // Fetch active modifiers
            fetchModifiersList();
            // Fetch color correction config
            fetchColorCorrectionConfig();
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
                        // Audio config fields (fixes UI revert bug)
                        if (msgFlat.current.tempoSync !== undefined) {
                            state.zones.zones[zoneId].tempoSync = msgFlat.current.tempoSync;
                        }
                        if (msgFlat.current.audioBand !== undefined) {
                            state.zones.zones[zoneId].audioBand = msgFlat.current.audioBand;
                        }
                        if (msgFlat.current.beatTriggerEnabled !== undefined) {
                            state.zones.zones[zoneId].beatTriggerEnabled = msgFlat.current.beatTriggerEnabled;
                        }
                        if (msgFlat.current.beatTriggerInterval !== undefined) {
                            state.zones.zones[zoneId].beatTriggerInterval = msgFlat.current.beatTriggerInterval;
                        }
                        if (msgFlat.current.beatModulation !== undefined) {
                            state.zones.zones[zoneId].beatModulation = msgFlat.current.beatModulation;
                        }
                        if (msgFlat.current.tempoSpeedScale !== undefined) {
                            state.zones.zones[zoneId].tempoSpeedScale = msgFlat.current.tempoSpeedScale;
                        }
                        if (msgFlat.current.beatDecay !== undefined) {
                            state.zones.zones[zoneId].beatDecay = msgFlat.current.beatDecay;
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

        // Show commands responses
        case 'show.list':
            if (msgFlat.success && msgFlat.data) {
                const allShows = [
                    ...(msgFlat.data.builtin || []).map(s => ({
                        id: String(s.id),
                        name: s.name,
                        durationSeconds: s.durationSeconds || Math.floor((s.durationMs || 0) / 1000),
                        type: 'builtin',
                        isSaved: true
                    })),
                    ...(msgFlat.data.custom || []).map(s => ({
                        id: s.id,
                        name: s.name,
                        durationSeconds: s.durationSeconds || Math.floor((s.durationMs || 0) / 1000),
                        type: 'custom',
                        isSaved: s.isSaved !== false
                    }))
                ];
                state.shows.list = allShows;
                state.shows.loading = false;
                log(`[WS] ✅ Loaded ${allShows.length} shows`);
                updateShowsUI();
            }
            break;

        case 'show.get':
            if (msgFlat.success && msgFlat.data) {
                const show = {
                    id: String(msgFlat.data.id),
                    name: msgFlat.data.name,
                    durationSeconds: msgFlat.data.durationSeconds || Math.floor((msgFlat.data.durationMs || 0) / 1000),
                    scenes: msgFlat.data.scenes || [],
                    isSaved: msgFlat.data.type === 'builtin' || msgFlat.data.isSaved !== false,
                    type: msgFlat.data.type || 'custom'
                };
                state.shows.current = show;
                state.shows.scenes = show.scenes;
                state.shows.loading = false;
                log(`[WS] ✅ Loaded show: ${show.name}`);
                updateShowsUI();
                renderTimeline();
            }
            break;

        case 'show.create':
        case 'show.update':
            if (msgFlat.success && msgFlat.data) {
                const show = {
                    id: String(msgFlat.data.id),
                    name: msgFlat.data.name,
                    durationSeconds: msgFlat.data.durationSeconds || 0,
                    scenes: state.shows.scenes,
                    isSaved: true,
                    type: 'custom'
                };
                state.shows.current = show;
                state.shows.loading = false;
                log(`[WS] ✅ ${msgFlat.type === 'show.create' ? 'Created' : 'Updated'} show: ${show.name}`);
                fetchShowsList(); // Refresh list
                updateShowsUI();
            }
            break;

        case 'show.delete':
            if (msgFlat.success) {
                if (state.shows.current && String(state.shows.current.id) === String(msgFlat.showId)) {
                    state.shows.current = null;
                    state.shows.scenes = [];
                }
                state.shows.loading = false;
                log(`[WS] ✅ Deleted show: ${msgFlat.showId}`);
                fetchShowsList(); // Refresh list
                updateShowsUI();
                renderTimeline();
            }
            break;

        case 'show.current':
            if (msgFlat.success && msgFlat.data) {
                state.shows.isPlaying = msgFlat.data.isPlaying || false;
                state.shows.isPaused = msgFlat.data.isPaused || false;
                state.shows.progress = msgFlat.data.progress || 0;
                state.shows.elapsedMs = msgFlat.data.elapsedMs || 0;
                state.shows.remainingMs = msgFlat.data.remainingMs || 0;
                state.shows.currentChapter = msgFlat.data.currentChapter || 0;
                
                if (msgFlat.data.showId !== null && msgFlat.data.showId !== undefined) {
                    if (!state.shows.current || String(state.shows.current.id) !== String(msgFlat.data.showId)) {
                        fetchShowDetails(msgFlat.data.showId, 'scenes');
                    }
                }
                
                updateShowsUI();
                renderPlayhead();
            }
            break;

        // Show events (broadcast from firmware)
        case 'show.started':
            state.shows.isPlaying = true;
            state.shows.isPaused = false;
            if (msgFlat.showId !== undefined) {
                fetchShowDetails(msgFlat.showId, 'scenes');
            }
            updateShowsUI();
            log(`[WS] Show started: ${msgFlat.showName || msgFlat.showId}`);
            break;

        case 'show.stopped':
            state.shows.isPlaying = false;
            state.shows.isPaused = false;
            state.shows.progress = 0;
            state.shows.elapsedMs = 0;
            updateShowsUI();
            renderPlayhead();
            log(`[WS] Show stopped`);
            break;

        case 'show.paused':
            state.shows.isPaused = true;
            updateShowsUI();
            log(`[WS] Show paused`);
            break;

        case 'show.resumed':
            state.shows.isPaused = false;
            updateShowsUI();
            log(`[WS] Show resumed`);
            break;

        case 'show.progress':
            if (msgFlat.progress !== undefined) {
                state.shows.progress = msgFlat.progress;
            }
            if (msgFlat.elapsedMs !== undefined) {
                state.shows.elapsedMs = msgFlat.elapsedMs;
            }
            if (msgFlat.remainingMs !== undefined) {
                state.shows.remainingMs = msgFlat.remainingMs;
            }
            updateShowsUI();
            renderPlayhead();
            break;

        case 'show.chapterChanged':
            if (msgFlat.chapterIndex !== undefined) {
                state.shows.currentChapter = msgFlat.chapterIndex;
                updateShowsUI();
            }
            log(`[WS] Chapter changed: ${msgFlat.chapterIndex}`);
            break;

        // Color correction responses
        case 'colorCorrection.getConfig':
            if (msgFlat.mode !== undefined) {
                state.colorCorrection = {
                    mode: msgFlat.mode,
                    autoExposureEnabled: msgFlat.autoExposureEnabled || false,
                    autoExposureTarget: msgFlat.autoExposureTarget || 110,
                    brownGuardrailEnabled: msgFlat.brownGuardrailEnabled || false,
                    gammaEnabled: msgFlat.gammaEnabled || false,
                    gammaValue: msgFlat.gammaValue || 2.2
                };
                updateColorCorrectionUI();
                log(`[WS] Color correction config loaded: mode=${state.colorCorrection.mode}`);
            }
            break;

        case 'colorCorrection.setConfig':
            // Config updated successfully - refresh UI
            if (msgFlat.updated) {
                log(`[WS] Color correction config updated`);
                fetchColorCorrectionConfig();
            }
            break;

        case 'colorCorrection.setMode':
            // Mode changed successfully
            if (msgFlat.mode !== undefined) {
                state.colorCorrection.mode = msgFlat.mode;
                updateColorCorrectionUI();
                log(`[WS] Color correction mode: ${msgFlat.modeName || msgFlat.mode}`);
            }
            break;

        case 'colorCorrection.save':
            if (msgFlat.saved) {
                log(`[WS] Color correction saved to NVS`);
            }
            break;

        case 'colorCorrection.setPreset':
            // Preset applied - update state with new config
            if (msgFlat.preset !== undefined) {
                state.colorCorrection.currentPreset = msgFlat.preset;
                // If config was included, update all settings
                if (msgFlat.config) {
                    state.colorCorrection.mode = msgFlat.config.mode;
                    state.colorCorrection.autoExposureEnabled = msgFlat.config.autoExposureEnabled;
                    state.colorCorrection.autoExposureTarget = msgFlat.config.autoExposureTarget;
                    state.colorCorrection.brownGuardrailEnabled = msgFlat.config.brownGuardrailEnabled;
                    state.colorCorrection.gammaEnabled = msgFlat.config.gammaEnabled;
                    state.colorCorrection.gammaValue = msgFlat.config.gammaValue;
                }
                updateColorCorrectionUI();
                log(`[WS] Color correction preset: ${msgFlat.presetName || msgFlat.preset}`);
            }
            break;

        case 'colorCorrection.getPresets':
            // Presets list received
            if (msgFlat.currentPreset !== undefined) {
                state.colorCorrection.currentPreset = msgFlat.currentPreset;
                updateColorCorrectionUI();
                log(`[WS] Current preset: ${msgFlat.currentPresetName || msgFlat.currentPreset}`);
            }
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

// Zone brightness control handlers (per-zone)
const zoneBrightnessUpdateTimers = {};

function onZoneBrightnessChange(zoneId) {
    const slider = elements[`zone${zoneId}BrightnessSlider`];
    if (!slider) return;

    const newVal = parseInt(slider.value);
    if (!state.zones || !state.zones.zones || !state.zones.zones[zoneId]) return;

    const currentBrightness = state.zones.zones[zoneId].brightness || 128;
    if (newVal !== currentBrightness) {
        state.zones.zones[zoneId].brightness = newVal;
        const valueEl = elements[`zone${zoneId}BrightnessValue`];
        if (valueEl) valueEl.textContent = newVal;

        // Set pending flag to ignore stale server updates while dragging
        if (state.pendingZoneBrightnessChange[zoneId]) {
            clearTimeout(state.pendingZoneBrightnessChange[zoneId]);
        }
        state.pendingZoneBrightnessChange[zoneId] = setTimeout(() => {
            state.pendingZoneBrightnessChange[zoneId] = null;
        }, 1000);

        // Debounce API calls while dragging
        if (zoneBrightnessUpdateTimers[zoneId]) {
            clearTimeout(zoneBrightnessUpdateTimers[zoneId]);
        }

        zoneBrightnessUpdateTimers[zoneId] = setTimeout(() => {
            // Only send if zones are enabled (prevents flooding when disabled)
            if (!state.zones || !state.zones.enabled) {
                log(`[ZONE] Zone mode is disabled, not sending brightness change for zone ${zoneId}`);
                return;
            }

            // Try WebSocket
            if (state.connected && state.ws && state.ws.readyState === WebSocket.OPEN) {
                send({ type: 'zones.update', zoneId: zoneId, brightness: newVal });
            }

            // REST API backup (only if zones are enabled)
            fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zones/${zoneId}/brightness`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ brightness: newVal })
            })
            .then(data => {
                if (data.success) {
                    log(`[REST] Zone ${zoneId} brightness set to: ${newVal}`);
                }
            })
            .catch(e => {
                // 404 is OK if zones aren't fully initialized yet
                if (e.message && !e.message.includes('404')) {
                    log(`[REST] Error setting zone ${zoneId} brightness: ${e.message}`);
                }
            });
        }, 300); // Debounce time matches speed slider
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

function fetchZoneConfig() {
    // Fetch zone configuration persistence status
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zones/config`, {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' }
    })
    .then(data => {
        if (data.success && data.data) {
            log(`[REST] Zone Config: version=${data.data.configVersion}, checksumValid=${data.data.checksumValid}, enabled=${data.data.enabled}, zoneCount=${data.data.zoneCount}`);
            console.log('[ZONE CONFIG]', data.data);
            return data.data;
        }
    })
    .catch(e => {
        log(`[REST] Error fetching zone config: ${e.message}`);
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

// Zone colour mapping (used by updateZonesUI and renderZoneQuickControls)
const ZONE_COLORS = ['#06b6d4', '#22c55e', '#a855f7', '#3b82f6'];  // cyan, green, purple, blue

function updateZonesUI() {
    const zoneRow = document.getElementById('zoneControlsRow');
    const zoneBrightnessRow = document.getElementById('zoneBrightnessRow');
    const zoneQuickControlsRow = document.getElementById('zoneQuickControlsRow');
    const presetLibraryRow = document.getElementById('presetLibraryRow');

    // Show/hide zone controls based on state
    if (state.zones && state.zones.enabled && state.zones.zones && state.zones.zones.length > 0) {
        if (zoneRow) zoneRow.style.display = 'flex';
        if (zoneBrightnessRow) zoneBrightnessRow.style.display = 'flex';
        if (zoneQuickControlsRow) zoneQuickControlsRow.style.display = 'flex';
        if (presetLibraryRow) presetLibraryRow.style.display = 'flex';

        // Update each zone slider (up to 4 zones)
        for (let i = 0; i < Math.min(4, state.zones.zones.length); i++) {
            const zone = state.zones.zones[i];
            if (!zone) continue;

            // Update speed slider
            const speedSlider = elements[`zone${i}SpeedSlider`];
            const speedValue = elements[`zone${i}SpeedValue`];

            if (speedSlider && zone.speed !== undefined) {
                // Only update if not pending (user is not dragging)
                if (!state.pendingZoneSpeedChange[i]) {
                    speedSlider.value = zone.speed;
                }
            }
            if (speedValue && zone.speed !== undefined) {
                speedValue.textContent = zone.speed;
            }

            // Update brightness slider
            const brightnessSlider = elements[`zone${i}BrightnessSlider`];
            const brightnessValue = elements[`zone${i}BrightnessValue`];
            const zoneBrightness = zone.brightness !== undefined ? zone.brightness : 128;

            if (brightnessSlider) {
                // Only update if not pending (user is not dragging)
                if (!state.pendingZoneBrightnessChange[i]) {
                    brightnessSlider.value = zoneBrightness;
                }
            }
            if (brightnessValue) {
                brightnessValue.textContent = zoneBrightness;
            }
        }

        // Show/hide zone 3 sliders based on zone count
        const hasZone3 = state.zones.zones.length >= 4;
        const zone3SpeedSection = elements.zone3SpeedSlider ? elements.zone3SpeedSlider.closest('section') : null;
        const zone3BrightnessSection = elements.zone3BrightnessSlider ? elements.zone3BrightnessSlider.closest('section') : null;
        if (zone3SpeedSection) zone3SpeedSection.style.display = hasZone3 ? 'block' : 'none';
        if (zone3BrightnessSection) zone3BrightnessSection.style.display = hasZone3 ? 'block' : 'none';

        // Render zone quick controls
        renderZoneQuickControls();
        
        // Load preset list (throttled - only if enough time has passed)
        const now = Date.now();
        if (now - state.lastPresetListFetchMs >= state.presetListFetchThrottleMs) {
            fetchPresetList();
        }
    } else {
        if (zoneRow) zoneRow.style.display = 'none';
        if (zoneBrightnessRow) zoneBrightnessRow.style.display = 'none';
        if (zoneQuickControlsRow) zoneQuickControlsRow.style.display = 'none';
        if (presetLibraryRow) presetLibraryRow.style.display = 'none';
    }

    // Update zone mode UI
    updateZoneModeUI();
}

// Build palette options for dropdown
function buildPaletteOptions(selectedId) {
    let html = '';
    for (let i = 0; i < PALETTE_NAMES.length; i++) {
        const selected = i === selectedId ? 'selected' : '';
        html += `<option value="${i}" ${selected}>${PALETTE_NAMES[i]}</option>`;
    }
    return html;
}

// Build effect options for dropdown
function buildEffectOptions(selectedId) {
    let html = '';
    // Apply current filter to effects list
    let filteredEffects = state.effectsList || [];
    if (state.patternFilter === 'reactive') {
        filteredEffects = filteredEffects.filter(eff => eff.isAudioReactive === true);
    } else if (state.patternFilter === 'ambient') {
        filteredEffects = filteredEffects.filter(eff => eff.isAudioReactive !== true);
    }
    
    if (filteredEffects.length > 0) {
        for (const effect of filteredEffects) {
            const selected = effect.id === selectedId ? 'selected' : '';
            html += `<option value="${effect.id}" ${selected}>${effect.name}</option>`;
        }
    } else {
        // Fallback if effects list not loaded yet or filter results in empty list
        html = `<option value="${selectedId || 0}">Effect ${selectedId || 0}</option>`;
    }
    return html;
}

// Audio band options for zone routing
const AUDIO_BANDS = [
    { value: 0, label: 'Full' },
    { value: 1, label: 'Bass' },
    { value: 2, label: 'Mid' },
    { value: 3, label: 'High' }
];

// Beat divisor options for beat triggers
const BEAT_DIVISORS = [
    { value: 0, label: 'Off' },
    { value: 1, label: '1 Beat' },
    { value: 2, label: '2 Beats' },
    { value: 4, label: '4 Beats' },
    { value: 8, label: '8 Beats' }
];

// Render zone quick controls (palette and effect dropdowns per zone)
function renderZoneQuickControls() {
    const container = document.getElementById('zoneQuickControlsGrid');
    if (!container) return;
    if (!state.zones || !state.zones.zones) {
        container.innerHTML = '';
        return;
    }

    const zoneCount = state.zones.zones.length;
    let html = '';

    for (let i = 0; i < zoneCount; i++) {
        const zone = state.zones.zones[i];
        if (!zone) continue;

        const zoneColor = ZONE_COLORS[i % ZONE_COLORS.length];
        const currentPaletteId = zone.paletteId !== undefined ? zone.paletteId : state.paletteId;
        const currentEffectId = zone.effectId !== undefined ? zone.effectId : state.effectId;
        const currentAudioBand = zone.audioBand !== undefined ? zone.audioBand : 0;
        const tempoSync = zone.tempoSync || false;
        const beatDivisor = zone.beatDivisor !== undefined ? zone.beatDivisor : 0;

        // Build audio band options
        const audioBandOptions = AUDIO_BANDS.map(b =>
            `<option value="${b.value}" ${b.value === currentAudioBand ? 'selected' : ''}>${b.label}</option>`
        ).join('');

        // Build beat divisor options
        const beatDivisorOptions = BEAT_DIVISORS.map(b =>
            `<option value="${b.value}" ${b.value === beatDivisor ? 'selected' : ''}>${b.label}</option>`
        ).join('');

        html += `
            <div class="zone-quick-control" style="padding: var(--space-sm); border: 1px solid ${zoneColor}40; border-radius: 8px; background: ${zoneColor}10;">
                <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: var(--space-xs);">
                    <div class="card-header" style="color: ${zoneColor}; text-align: left; margin: 0;">Zone ${i}</div>
                    <div style="display: flex; gap: 2px;">
                        <button class="control-btn compact zone-reorder-btn" data-zone="${i}" data-dir="up" style="padding: 2px 6px; font-size: 0.625rem; ${i === 0 ? 'opacity: 0.3; pointer-events: none;' : ''}">▲</button>
                        <button class="control-btn compact zone-reorder-btn" data-zone="${i}" data-dir="down" style="padding: 2px 6px; font-size: 0.625rem; ${i === zoneCount - 1 ? 'opacity: 0.3; pointer-events: none;' : ''}">▼</button>
                    </div>
                </div>
                <div style="display: flex; flex-direction: column; gap: var(--space-xs);">
                    <div>
                        <label style="font-size: 0.65rem; color: var(--text-secondary); display: block; margin-bottom: 2px;">Pattern</label>
                        <select class="control-btn compact zone-quick-select" data-zone="${i}" data-type="effect" style="width: 100%;">
                            ${buildEffectOptions(currentEffectId)}
                        </select>
                    </div>
                    <div>
                        <label style="font-size: 0.65rem; color: var(--text-secondary); display: block; margin-bottom: 2px;">Palette</label>
                        <select class="control-btn compact zone-quick-select" data-zone="${i}" data-type="palette" style="width: 100%; font-size: 0.7rem; padding: 4px 6px;">
                            ${buildPaletteOptions(currentPaletteId)}
                        </select>
                    </div>
                    <div style="border-top: 1px solid ${zoneColor}30; padding-top: var(--space-xs); margin-top: var(--space-xs);">
                        <label style="font-size: 0.65rem; color: var(--text-secondary); display: block; margin-bottom: 2px;">Audio Band</label>
                        <select class="control-btn compact zone-quick-select" data-zone="${i}" data-type="audioBand" style="width: 100%;">
                            ${audioBandOptions}
                        </select>
                    </div>
                    <div style="display: flex; gap: var(--space-xs); align-items: center;">
                        <div style="flex: 1;">
                            <label style="font-size: 0.65rem; color: var(--text-secondary); display: block; margin-bottom: 2px;">Tempo Sync</label>
                            <button class="control-btn compact zone-tempo-toggle" data-zone="${i}" style="width: 100%; background: ${tempoSync ? zoneColor : 'var(--bg-elevated)'}; color: ${tempoSync ? 'var(--bg-base)' : 'var(--text-primary)'};">
                                ${tempoSync ? 'ON' : 'OFF'}
                            </button>
                        </div>
                        <div style="flex: 1;">
                            <label style="font-size: 0.65rem; color: var(--text-secondary); display: block; margin-bottom: 2px;">Beat Trigger</label>
                            <select class="control-btn compact zone-quick-select" data-zone="${i}" data-type="beatDivisor" style="width: 100%;">
                                ${beatDivisorOptions}
                            </select>
                        </div>
                    </div>
                </div>
            </div>
        `;
    }

    container.innerHTML = html;

    // Attach change handlers for dropdowns
    container.querySelectorAll('.zone-quick-select').forEach(select => {
        select.addEventListener('change', (e) => {
            const zoneId = parseInt(e.target.dataset.zone);
            const type = e.target.dataset.type;
            const value = parseInt(e.target.value);

            if (type === 'palette') {
                setZonePalette(zoneId, value);
            } else if (type === 'effect') {
                setZoneEffect(zoneId, value);
            } else if (type === 'audioBand') {
                setZoneAudioBand(zoneId, value);
            } else if (type === 'beatDivisor') {
                setZoneBeatDivisor(zoneId, value);
            }
        });
    });

    // Attach click handlers for tempo sync toggles
    container.querySelectorAll('.zone-tempo-toggle').forEach(btn => {
        btn.addEventListener('click', (e) => {
            const zoneId = parseInt(e.target.dataset.zone);
            const zone = state.zones.zones[zoneId];
            const newValue = !(zone && zone.tempoSync);
            setZoneTempoSync(zoneId, newValue);
        });
    });

    // Attach click handlers for zone reorder buttons
    container.querySelectorAll('.zone-reorder-btn').forEach(btn => {
        btn.addEventListener('click', (e) => {
            const zoneId = parseInt(e.target.dataset.zone);
            const dir = e.target.dataset.dir;
            reorderZone(zoneId, dir);
        });
    });
}

// ─────────────────────────────────────────────────────────────
// Zone Editor Functions
// ─────────────────────────────────────────────────────────────

// Zone editor constants (ZONE_COLORS is defined above updateZonesUI)
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

function setZoneAudioBand(zoneId, audioBand) {
    // Update local state immediately for responsive UI
    if (state.zones && state.zones.zones && state.zones.zones[zoneId]) {
        state.zones.zones[zoneId].audioBand = audioBand;
    }

    if (state.connected && state.ws) {
        send({ type: 'zones.update', zoneId, audioBand });
        log(`[WS] Zone ${zoneId} audioBand: ${audioBand}`);
    } else {
        fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zones/${zoneId}/audio`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ audioBand })
        })
        .then(data => {
            if (data.success) {
                log(`[REST] Zone ${zoneId} audioBand: ${audioBand}`);
            }
        })
        .catch(e => {
            log(`[REST] ❌ Error setting zone ${zoneId} audioBand: ${e.message}`);
        });
    }
}

function setZoneTempoSync(zoneId, tempoSync) {
    // Update local state immediately for responsive UI
    if (state.zones && state.zones.zones && state.zones.zones[zoneId]) {
        state.zones.zones[zoneId].tempoSync = tempoSync;
    }
    // Re-render to update button state
    renderZoneQuickControls();

    if (state.connected && state.ws) {
        send({ type: 'zones.update', zoneId, tempoSync });
        log(`[WS] Zone ${zoneId} tempoSync: ${tempoSync}`);
    } else {
        fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zones/${zoneId}/audio`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ tempoSync })
        })
        .then(data => {
            if (data.success) {
                log(`[REST] Zone ${zoneId} tempoSync: ${tempoSync}`);
            }
        })
        .catch(e => {
            log(`[REST] ❌ Error setting zone ${zoneId} tempoSync: ${e.message}`);
        });
    }
}

function setZoneBeatDivisor(zoneId, beatDivisor) {
    // Update local state immediately for responsive UI
    if (state.zones && state.zones.zones && state.zones.zones[zoneId]) {
        state.zones.zones[zoneId].beatDivisor = beatDivisor;
        // If beatDivisor > 0, beat trigger is enabled
        state.zones.zones[zoneId].beatTriggerEnabled = beatDivisor > 0;
    }

    if (state.connected && state.ws) {
        send({ type: 'zones.update', zoneId, beatDivisor, beatTriggerEnabled: beatDivisor > 0 });
        log(`[WS] Zone ${zoneId} beatDivisor: ${beatDivisor}`);
    } else {
        fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zones/${zoneId}/audio`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ beatDivisor, beatTriggerEnabled: beatDivisor > 0 })
        })
        .then(data => {
            if (data.success) {
                log(`[REST] Zone ${zoneId} beatDivisor: ${beatDivisor}`);
            }
        })
        .catch(e => {
            log(`[REST] ❌ Error setting zone ${zoneId} beatDivisor: ${e.message}`);
        });
    }
}

function reorderZone(zoneId, direction) {
    if (!state.zones || !state.zones.zones) return;

    const zoneCount = state.zones.zones.length;
    if (zoneCount < 2) return;

    // Calculate new order
    const currentOrder = [];
    for (let i = 0; i < zoneCount; i++) {
        currentOrder.push(i);
    }

    const targetIndex = direction === 'up' ? zoneId - 1 : zoneId + 1;
    if (targetIndex < 0 || targetIndex >= zoneCount) return;

    // Swap positions
    [currentOrder[zoneId], currentOrder[targetIndex]] = [currentOrder[targetIndex], currentOrder[zoneId]];

    log(`[ZONES] Reordering: ${currentOrder.join(',')}`);

    // Send reorder request
    if (state.connected && state.ws) {
        send({ type: 'zones.reorder', order: currentOrder });
    } else {
        fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zones/reorder`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ order: currentOrder })
        })
        .then(data => {
            if (data.success) {
                log(`[ZONES] ✅ Reordered`);
                fetchZonesState();
            } else {
                log(`[ZONES] ❌ Reorder failed: ${data.error?.message || 'Unknown error'}`);
                alert(`Reorder failed: ${data.error?.message || 'Zone 0 must contain center LEDs (79/80)'}`);
            }
        })
        .catch(e => {
            log(`[ZONES] ❌ Error: ${e.message}`);
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

// ─────────────────────────────────────────────────────────────
// Preset Library Management
// ─────────────────────────────────────────────────────────────

function fetchPresetList() {
    // Update throttle timestamp
    state.lastPresetListFetchMs = Date.now();

    // Use zone presets API (saves complete zone configuration)
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zone-presets`, {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' }
    })
    .then(data => {
        if (data.success && data.data && data.data.presets) {
            renderPresetList(data.data.presets);
        }
    })
    .catch(e => {
        log(`[ZONE PRESETS] Error fetching preset list: ${e.message}`);
        const presetList = document.getElementById('presetList');
        if (presetList) {
            presetList.innerHTML = '<div class="value-secondary compact" style="text-align: center; padding: var(--space-sm); color: var(--error);">Failed to load presets</div>';
        }
    });
}

function renderPresetList(presets) {
    const presetList = document.getElementById('presetList');
    if (!presetList) return;

    if (!presets || presets.length === 0) {
        presetList.innerHTML = '<div class="value-secondary compact" style="text-align: center; padding: var(--space-sm);">No zone presets saved yet</div>';
        return;
    }

    let html = '';
    for (const preset of presets) {
        const id = preset.id;
        const name = preset.name || 'Unnamed';
        const zoneCount = preset.zoneCount || '?';

        html += `
            <div style="display: flex; align-items: center; gap: var(--space-xs); padding: var(--space-xs); background: var(--bg-elevated); border-radius: 6px;">
                <div style="flex: 1; min-width: 0;">
                    <div style="font-size: 0.75rem; font-weight: 600; color: var(--text-primary);">${name}</div>
                    <div style="font-size: 0.625rem; color: var(--text-secondary); margin-top: 2px;">${zoneCount} zones • ID: ${id}</div>
                </div>
                <div style="display: flex; gap: var(--space-xs); flex-shrink: 0;">
                    <button class="control-btn compact" data-preset-id="${id}" data-preset-name="${name}" data-preset-action="load" style="font-size: 0.625rem; padding: 4px 8px; background: var(--gold); color: var(--bg-base);">Apply</button>
                    <button class="control-btn compact" data-preset-id="${id}" data-preset-name="${name}" data-preset-action="delete" style="font-size: 0.625rem; padding: 4px 8px; background: var(--error); color: var(--bg-base);">Delete</button>
                </div>
            </div>
        `;
    }
    presetList.innerHTML = html;

    // Attach event listeners
    presetList.querySelectorAll('[data-preset-action]').forEach(btn => {
        btn.addEventListener('click', (e) => {
            const action = e.target.dataset.presetAction;
            const id = parseInt(e.target.dataset.presetId);
            const name = e.target.dataset.presetName;
            if (action === 'load') {
                applyZonePreset(id, name);
            } else if (action === 'delete') {
                if (confirm(`Delete zone preset "${name}"?`)) {
                    deleteZonePreset(id, name);
                }
            }
        });
    });
}

function saveCurrentAsPreset() {
    const nameInput = document.getElementById('presetNameInput');
    const descInput = document.getElementById('presetDescriptionInput');

    if (!nameInput || !nameInput.value.trim()) {
        alert('Please enter a preset name');
        return;
    }

    const name = nameInput.value.trim();
    const description = descInput ? descInput.value.trim() : '';

    // Use zone presets API - saves current zone configuration
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zone-presets`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            name: name,
            description: description || undefined
        })
    })
    .then(data => {
        if (data.success) {
            log(`[ZONE PRESETS] ✅ Preset saved: ${name}`);
            if (nameInput) nameInput.value = '';
            if (descInput) descInput.value = '';
            fetchPresetList();
        } else {
            log(`[ZONE PRESETS] ❌ Failed to save preset: ${data.error?.message || 'Unknown error'}`);
            alert(`Failed to save preset: ${data.error?.message || 'Unknown error'}`);
        }
    })
    .catch(e => {
        log(`[ZONE PRESETS] ❌ Error: ${e.message}`);
        alert(`Error saving preset: ${e.message}`);
    });
}

function applyZonePreset(id, name) {
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zone-presets/apply?id=${id}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }
    })
    .then(data => {
        if (data.success) {
            log(`[ZONE PRESETS] ✅ Applied: ${name}`);
            // Refresh zones state to show updated configuration
            fetchZonesState();
        } else {
            log(`[ZONE PRESETS] ❌ Failed to apply: ${data.error?.message || 'Unknown error'}`);
            alert(`Failed to apply preset: ${data.error?.message || 'Unknown error'}`);
        }
    })
    .catch(e => {
        log(`[ZONE PRESETS] ❌ Error: ${e.message}`);
        alert(`Error applying preset: ${e.message}`);
    });
}

function deleteZonePreset(id, name) {
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/zone-presets/delete?id=${id}`, {
        method: 'DELETE',
        headers: { 'Content-Type': 'application/json' }
    })
    .then(data => {
        if (data.success) {
            log(`[ZONE PRESETS] ✅ Deleted: ${name}`);
            fetchPresetList();
        } else {
            log(`[ZONE PRESETS] ❌ Failed to delete: ${data.error?.message || 'Unknown error'}`);
            alert(`Failed to delete preset: ${data.error?.message || 'Unknown error'}`);
        }
    })
    .catch(e => {
        log(`[ZONE PRESETS] ❌ Error: ${e.message}`);
        alert(`Error deleting preset: ${e.message}`);
    });
}

function uploadPreset(file) {
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (e) => {
        try {
            const jsonData = JSON.parse(e.target.result);
            
            // Validate JSON structure
            if (!jsonData.name || !jsonData.zones || !jsonData.segments) {
                throw new Error('Invalid preset format');
            }

            // Upload preset
            fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/presets`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(jsonData)
            })
            .then(data => {
                if (data.success) {
                    log(`[PRESETS] ✅ Preset uploaded: ${jsonData.name}`);
                    fetchPresetList();
                    
                    // Clear file input
                    const uploadInput = document.getElementById('presetUploadInput');
                    if (uploadInput) uploadInput.value = '';
                    const fileNameSpan = document.getElementById('presetUploadFileName');
                    if (fileNameSpan) fileNameSpan.textContent = 'No file selected';
                } else {
                    log(`[PRESETS] ❌ Failed to upload preset: ${data.error?.message || 'Unknown error'}`);
                    alert(`Failed to upload preset: ${data.error?.message || 'Unknown error'}`);
                }
            })
            .catch(e => {
                log(`[PRESETS] ❌ Error: ${e.message}`);
                alert(`Error uploading preset: ${e.message}`);
            });
        } catch (err) {
            log(`[PRESETS] ❌ Invalid JSON file: ${err.message}`);
            alert(`Invalid preset file: ${err.message}`);
        }
    };
    reader.readAsText(file);
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
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/effects?limit=100`, {
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
// Shows API Functions
// ─────────────────────────────────────────────────────────────

function fetchShowsList() {
    state.shows.loading = true;
    state.shows.error = null;
    
    // Try WebSocket first
    if (state.connected && state.ws && state.ws.readyState === WebSocket.OPEN) {
        send({ type: 'show.list' });
    }
    
    // REST API backup
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/shows`, {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' }
    })
    .then(data => {
        if (data.success && data.data) {
            const allShows = [
                ...(data.data.builtin || []).map(s => ({
                    id: String(s.id),
                    name: s.name,
                    durationSeconds: s.durationSeconds || Math.floor((s.durationMs || 0) / 1000),
                    type: 'builtin',
                    isSaved: true
                })),
                ...(data.data.custom || []).map(s => ({
                    id: s.id,
                    name: s.name,
                    durationSeconds: s.durationSeconds || Math.floor((s.durationMs || 0) / 1000),
                    type: 'custom',
                    isSaved: s.isSaved !== false
                }))
            ];
            state.shows.list = allShows;
            state.shows.loading = false;
            log(`[REST] ✅ Loaded ${allShows.length} shows`);
            updateShowsUI();
        }
    })
    .catch(e => {
        state.shows.loading = false;
        state.shows.error = e.message;
        log(`[REST] ❌ Error fetching shows: ${e.message}`);
    });
}

function fetchShowDetails(showId, format = 'scenes') {
    state.shows.loading = true;
    state.shows.error = null;
    
    // Try WebSocket first
    if (state.connected && state.ws && state.ws.readyState === WebSocket.OPEN) {
        send({ type: 'show.get', showId: showId, format: format });
    }
    
    // REST API backup
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/shows/${showId}?format=${format}`, {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' }
    })
    .then(data => {
        if (data.success && data.data) {
            const show = {
                id: String(data.data.id),
                name: data.data.name,
                durationSeconds: data.data.durationSeconds || Math.floor((data.data.durationMs || 0) / 1000),
                scenes: data.data.scenes || [],
                isSaved: data.data.type === 'builtin' || data.data.isSaved !== false,
                type: data.data.type || (parseInt(showId) < 100 ? 'builtin' : 'custom')
            };
            state.shows.current = show;
            state.shows.scenes = show.scenes;
            state.shows.loading = false;
            log(`[REST] ✅ Loaded show: ${show.name}`);
            updateShowsUI();
            renderTimeline();
        }
    })
    .catch(e => {
        state.shows.loading = false;
        state.shows.error = e.message;
        log(`[REST] ❌ Error fetching show details: ${e.message}`);
    });
}

function createShow(name, durationSeconds, scenes) {
    state.shows.loading = true;
    state.shows.error = null;
    
    const payload = {
        name: name,
        durationSeconds: durationSeconds,
        scenes: scenes.map(s => ({
            id: s.id,
            zoneId: s.zoneId,
            effectName: s.effectName,
            startTimePercent: s.startTimePercent,
            durationPercent: s.durationPercent,
            accentColor: s.accentColor || 'primary'
        }))
    };
    
    // Try WebSocket first
    if (state.connected && state.ws && state.ws.readyState === WebSocket.OPEN) {
        send({ type: 'show.create', ...payload });
    }
    
    // REST API backup
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/shows`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(data => {
        if (data.success && data.data) {
            const newShow = {
                id: data.data.id,
                name: data.data.name || name,
                durationSeconds: durationSeconds,
                scenes: scenes,
                isSaved: true,
                type: 'custom'
            };
            state.shows.current = newShow;
            state.shows.scenes = scenes;
            state.shows.loading = false;
            log(`[REST] ✅ Created show: ${newShow.name}`);
            fetchShowsList(); // Refresh list
            updateShowsUI();
        }
    })
    .catch(e => {
        state.shows.loading = false;
        state.shows.error = e.message;
        log(`[REST] ❌ Error creating show: ${e.message}`);
    });
}

function updateShow(showId, name, durationSeconds, scenes) {
    state.shows.loading = true;
    state.shows.error = null;
    
    const payload = {
        name: name,
        durationSeconds: durationSeconds,
        scenes: scenes.map(s => ({
            id: s.id,
            zoneId: s.zoneId,
            effectName: s.effectName,
            startTimePercent: s.startTimePercent,
            durationPercent: s.durationPercent,
            accentColor: s.accentColor || 'primary'
        }))
    };
    
    // Try WebSocket first
    if (state.connected && state.ws && state.ws.readyState === WebSocket.OPEN) {
        send({ type: 'show.update', showId: showId, ...payload });
    }
    
    // REST API backup
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/shows/${showId}`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(data => {
        if (data.success && data.data) {
            const updatedShow = {
                id: String(showId),
                name: data.data.name || name,
                durationSeconds: durationSeconds,
                scenes: scenes,
                isSaved: true,
                type: 'custom'
            };
            state.shows.current = updatedShow;
            state.shows.scenes = scenes;
            state.shows.loading = false;
            log(`[REST] ✅ Updated show: ${updatedShow.name}`);
            fetchShowsList(); // Refresh list
            updateShowsUI();
        }
    })
    .catch(e => {
        state.shows.loading = false;
        state.shows.error = e.message;
        log(`[REST] ❌ Error updating show: ${e.message}`);
    });
}

function deleteShow(showId) {
    state.shows.loading = true;
    state.shows.error = null;
    
    // Try WebSocket first
    if (state.connected && state.ws && state.ws.readyState === WebSocket.OPEN) {
        send({ type: 'show.delete', showId: showId });
    }
    
    // REST API backup
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/shows/${showId}`, {
        method: 'DELETE',
        headers: { 'Content-Type': 'application/json' }
    })
    .then(data => {
        if (data.success) {
            if (state.shows.current && String(state.shows.current.id) === String(showId)) {
                state.shows.current = null;
                state.shows.scenes = [];
            }
            state.shows.loading = false;
            log(`[REST] ✅ Deleted show: ${showId}`);
            fetchShowsList(); // Refresh list
            updateShowsUI();
            renderTimeline();
        }
    })
    .catch(e => {
        state.shows.loading = false;
        state.shows.error = e.message;
        log(`[REST] ❌ Error deleting show: ${e.message}`);
    });
}

function controlShow(action, showId, timeMs) {
    state.shows.error = null;
    
    // Validate showId for start action
    if (action === 'start' && showId !== undefined) {
        // Only numeric IDs are supported (built-in shows only)
        if (typeof showId === 'string' || isNaN(showId)) {
            log(`[SHOWS] ❌ Cannot start custom show: ${showId}. Only built-in shows (0-9) are supported.`);
            state.shows.error = 'Custom shows cannot be played yet. Only built-in shows are supported.';
            return;
        }
    }
    
    const payload = { action: action };
    if (action === 'start' && showId !== undefined && !isNaN(showId)) {
        payload.showId = showId;
    }
    if (action === 'seek' && timeMs !== undefined) {
        payload.timeMs = timeMs;
    }
    
    // Try WebSocket first
    if (state.connected && state.ws && state.ws.readyState === WebSocket.OPEN) {
        send({ type: 'show.control', ...payload });
    }
    
    // REST API backup
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/shows/control`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(data => {
        if (data.success) {
            log(`[REST] ✅ Show control: ${action}`);
            if (action === 'start') {
                fetchCurrentShowStatus();
            }
        }
    })
    .catch(e => {
        state.shows.error = e.message;
        log(`[REST] ❌ Error controlling show: ${e.message}`);
    });
}

function fetchCurrentShowStatus() {
    // Try WebSocket first
    if (state.connected && state.ws && state.ws.readyState === WebSocket.OPEN) {
        send({ type: 'show.current' });
    }
    
    // REST API backup
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/shows/current`, {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' }
    })
    .then(data => {
        if (data.success && data.data) {
            state.shows.isPlaying = data.data.isPlaying || false;
            state.shows.isPaused = data.data.isPaused || false;
            state.shows.progress = data.data.progress || 0;
            state.shows.elapsedMs = data.data.elapsedMs || 0;
            state.shows.remainingMs = data.data.remainingMs || 0;
            state.shows.currentChapter = data.data.currentChapter || 0;
            
            if (data.data.showId !== null && data.data.showId !== undefined) {
                if (!state.shows.current || String(state.shows.current.id) !== String(data.data.showId)) {
                    // Load show details if not already loaded
                    fetchShowDetails(data.data.showId, 'scenes');
                }
            }
            
            updateShowsUI();
            renderPlayhead();
        }
    })
    .catch(e => {
        // Ignore errors for status polling
    });
}

// ─────────────────────────────────────────────────────────────
// Shows UI Functions
// ─────────────────────────────────────────────────────────────

function updateShowsUI() {
    // Update show selector dropdown
    const showSelect = document.getElementById('showSelect');
    if (showSelect) {
        const currentValue = showSelect.value;
        showSelect.innerHTML = '<option value="">Select Show...</option>' +
            state.shows.list.map(s => 
                `<option value="${s.id}">${s.name} (${s.type})</option>`
            ).join('');
        if (currentValue) {
            showSelect.value = currentValue;
        }
    }
    
    // Update save button state
    const saveBtn = document.getElementById('showSaveBtn');
    if (saveBtn) {
        saveBtn.disabled = !state.shows.current || state.shows.current.isSaved || state.shows.current.type === 'builtin';
    }
    
    // Update transport controls
    const playBtn = document.getElementById('showPlayBtn');
    const pauseBtn = document.getElementById('showPauseBtn');
    if (playBtn && pauseBtn) {
        if (state.shows.isPlaying && !state.shows.isPaused) {
            playBtn.style.display = 'none';
            pauseBtn.style.display = 'inline-block';
        } else {
            playBtn.style.display = 'inline-block';
            pauseBtn.style.display = 'none';
        }
    }
    
    // Update time display
    updateShowTimeDisplay();
    
    // Update scene list
    renderSceneList();
}

function updateShowTimeDisplay() {
    const timeDisplay = document.getElementById('showTimeDisplay');
    if (!timeDisplay || !state.shows.current) return;
    
    const totalSeconds = state.shows.current.durationSeconds || 0;
    if (totalSeconds === 0) {
        timeDisplay.textContent = '0:00 / 0:00';
        return;
    }
    
    const elapsedSeconds = Math.floor((state.shows.elapsedMs || 0) / 1000);
    const totalSecondsDisplay = Math.floor(totalSeconds);
    
    const formatTime = (seconds) => {
        const mins = Math.floor(seconds / 60);
        const secs = seconds % 60;
        return `${mins}:${secs.toString().padStart(2, '0')}`;
    };
    
    timeDisplay.textContent = `${formatTime(elapsedSeconds)} / ${formatTime(totalSecondsDisplay)}`;
    
    // Sync seek slider with progress
    const seekSlider = document.getElementById('showSeekSlider');
    if (seekSlider) {
        if (state.shows.progress > 0) {
            seekSlider.value = Math.round(state.shows.progress * 100);
        }
        // Update max value when show changes
        seekSlider.max = 100; // Keep at 100 since we use percentage
    }
}

// ─────────────────────────────────────────────────────────────
// Timeline Rendering
// ─────────────────────────────────────────────────────────────

const ZONE_LABELS = [
    { id: 0, label: 'GLOBAL', color: 'var(--gold)' },
    { id: 1, label: 'ZONE 1', color: '#00d4ff' },
    { id: 2, label: 'ZONE 2', color: 'var(--success)' },
    { id: 3, label: 'ZONE 3', color: 'var(--text-secondary)' },
    { id: 4, label: 'ZONE 4', color: 'var(--gold)' }
];

const SCENE_COLORS = {
    0: { bg: 'rgba(255, 184, 77, 0.3)', border: 'rgba(255, 184, 77, 0.4)', text: 'var(--gold)' },
    1: { bg: 'rgba(0, 212, 255, 0.3)', border: 'rgba(0, 212, 255, 0.4)', text: '#00d4ff' },
    2: { bg: 'rgba(77, 255, 184, 0.3)', border: 'rgba(77, 255, 184, 0.4)', text: 'var(--success)' },
    3: { bg: 'rgba(156, 163, 176, 0.3)', border: 'rgba(156, 163, 176, 0.4)', text: 'var(--text-secondary)' },
    4: { bg: 'rgba(255, 184, 77, 0.3)', border: 'rgba(255, 184, 77, 0.4)', text: 'var(--gold)' }
};

function renderTimeline() {
    if (!state.shows.current) {
        const tracksEl = document.getElementById('timelineTracks');
        if (tracksEl) tracksEl.innerHTML = '<div class="value-secondary compact" style="text-align: center; padding: var(--space-md);">No show selected</div>';
        const rulerEl = document.getElementById('timeRuler');
        if (rulerEl) rulerEl.innerHTML = '';
        renderSceneList(); // Clear scene list
        return;
    }
    
    renderTimeRuler();
    renderZoneTracks();
    renderScenes();
    renderPlayhead();
    renderSceneList();
}

function renderTimeRuler() {
    const rulerEl = document.getElementById('timeRuler');
    if (!rulerEl || !state.shows.current) {
        if (rulerEl) rulerEl.innerHTML = '';
        return;
    }
    
    const durationSeconds = state.shows.current.durationSeconds || 120;
    if (durationSeconds <= 0) {
        rulerEl.innerHTML = '';
        return;
    }
    
    const markers = [];
    
    // Add markers every 30 seconds, with major markers every 60 seconds
    for (let seconds = 0; seconds <= durationSeconds; seconds += 30) {
        const percent = (seconds / durationSeconds) * 100;
        const mins = Math.floor(seconds / 60);
        const secs = seconds % 60;
        const isMajor = seconds % 60 === 0;
        markers.push({
            percent: percent,
            label: `${mins}:${secs.toString().padStart(2, '0')}`,
            isMajor: isMajor
        });
    }
    
    rulerEl.innerHTML = markers.map(m => 
        `<span style="position: absolute; left: ${m.percent}%; transform: translateX(-50%); font-size: 0.6rem; color: var(--text-secondary); ${m.isMajor ? 'font-weight: 600;' : ''}">${m.label}</span>`
    ).join('');
}

function renderZoneTracks() {
    const tracksEl = document.getElementById('timelineTracks');
    if (!tracksEl) return;
    
    tracksEl.innerHTML = ZONE_LABELS.map(zone => {
        const scenes = getScenesForZone(zone.id);
        return `
            <div class="timeline-track" data-track="${zone.id}" data-zone-id="${zone.id}" 
                 style="position: relative; min-height: 3rem; padding: var(--space-sm); margin-bottom: var(--space-xs); 
                        background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.05); border-radius: 4px;">
                <div style="position: absolute; left: var(--space-sm); top: 50%; transform: translateY(-50%); 
                            font-size: 0.6rem; color: ${zone.color}; writing-mode: vertical-rl; text-orientation: mixed;">
                    ${zone.label}
                </div>
                <div style="margin-left: 3rem; position: relative; height: 2rem;">
                    <!-- Scenes will be rendered here -->
                </div>
            </div>
        `;
    }).join('');
}

function renderScenes() {
    if (!state.shows.current) return;
    
    ZONE_LABELS.forEach(zone => {
        const trackEl = document.querySelector(`[data-zone-id="${zone.id}"]`);
        if (!trackEl) return;
        
        const scenesContainer = trackEl.querySelector('div > div');
        if (!scenesContainer) return;
        
        // Clear container to remove old event listeners
        scenesContainer.innerHTML = '';
        
        const scenes = getScenesForZone(zone.id);
        const colors = SCENE_COLORS[zone.id] || SCENE_COLORS[0];
        
        // Create scene elements
        scenes.forEach(scene => {
            const isDragging = state.shows.dragState.isDragging && state.shows.dragState.sceneId === scene.id;
            const sceneEl = document.createElement('div');
            sceneEl.className = 'timeline-scene';
            sceneEl.setAttribute('data-scene-id', scene.id);
            sceneEl.style.cssText = `
                position: absolute; 
                left: ${scene.startTimePercent}%; 
                width: ${scene.durationPercent}%; 
                height: 100%; 
                background: ${colors.bg}; 
                border: 1px solid ${colors.border}; 
                border-radius: 4px; 
                cursor: grab; 
                user-select: none; 
                ${isDragging ? 'opacity: 0.3;' : 'opacity: 1;'}
                display: flex; 
                align-items: center; 
                justify-content: center; 
                padding: 0 var(--space-xs);
            `;
            sceneEl.setAttribute('tabindex', '0');
            sceneEl.setAttribute('role', 'button');
            sceneEl.setAttribute('aria-label', `Scene: ${scene.effectName}, Zone ${zone.label}, Start: ${scene.startTimePercent.toFixed(1)}%, Duration: ${scene.durationPercent.toFixed(1)}%`);
            
            const span = document.createElement('span');
            span.style.cssText = `font-size: 0.6rem; color: ${colors.text}; text-overflow: ellipsis; overflow: hidden; white-space: nowrap;`;
            span.textContent = scene.effectName;
            sceneEl.appendChild(span);
            
            // Attach event listeners
            sceneEl.addEventListener('pointerdown', (e) => handleScenePointerDown(e, scene));
            sceneEl.addEventListener('keydown', (e) => handleSceneKeyDown(e, scene));
            
            scenesContainer.appendChild(sceneEl);
        });
    });
}

function renderPlayhead() {
    const playheadEl = document.getElementById('timelinePlayhead');
    const timelineContainer = document.querySelector('.timeline-container');
    if (!playheadEl || !timelineContainer || !state.shows.current) {
        if (playheadEl) playheadEl.style.display = 'none';
        return;
    }
    
    if (state.shows.isPlaying || (state.shows.progress > 0 && state.shows.progress <= 1)) {
        const progressPercent = Math.min(100, Math.max(0, state.shows.progress * 100));
        playheadEl.style.display = 'block';
        playheadEl.style.left = `${progressPercent}%`;
    } else {
        playheadEl.style.display = 'none';
    }
}

function getScenesForZone(zoneId) {
    if (!state.shows.current || !state.shows.scenes) return [];
    return state.shows.scenes.filter(scene => scene.zoneId === zoneId);
}

// ─────────────────────────────────────────────────────────────
// Drag-and-Drop Implementation
// ─────────────────────────────────────────────────────────────

function handleScenePointerDown(e, scene) {
    if (!state.connected) return;
    
    e.preventDefault();
    e.stopPropagation();
    
    const target = e.currentTarget;
    target.setPointerCapture(e.pointerId);
    
    const rect = target.getBoundingClientRect();
    const offsetX = e.clientX - rect.left;
    
    state.shows.dragState = {
        isDragging: true,
        sceneId: scene.id,
        startX: e.clientX,
        offsetX: offsetX,
        currentZoneId: scene.zoneId,
        originalStartPercent: scene.startTimePercent,
        originalZoneId: scene.zoneId
    };
    
    // Add global event listeners
    document.addEventListener('pointermove', handleScenePointerMove);
    document.addEventListener('pointerup', handleScenePointerUp);
    
    renderScenes(); // Re-render to show dragging state
}

function handleScenePointerMove(e) {
    if (!state.shows.dragState.isDragging) return;
    
    // Detect zone from Y position
    const hoveredZone = detectZoneFromY(e.clientY);
    if (hoveredZone !== null && hoveredZone !== state.shows.dragState.currentZoneId) {
        state.shows.dragState.currentZoneId = hoveredZone;
    }
    
    // Calculate percentage position
    const trackEl = document.querySelector(`[data-zone-id="${state.shows.dragState.currentZoneId}"]`);
    if (trackEl) {
        const scenesContainer = trackEl.querySelector('div > div');
        if (scenesContainer) {
            const rect = scenesContainer.getBoundingClientRect();
            const relativeX = e.clientX - rect.left - state.shows.dragState.offsetX;
            const percent = (relativeX / rect.width) * 100;
            const snappedPercent = snapToGrid(Math.max(0, Math.min(100, percent)));
            
            // Update scene position temporarily (will be applied on pointerup)
            const scene = state.shows.scenes.find(s => s.id === state.shows.dragState.sceneId);
            if (scene) {
                scene.startTimePercent = snappedPercent;
                scene.zoneId = state.shows.dragState.currentZoneId;
                renderScenes();
            }
        }
    }
}

function handleScenePointerUp(e) {
    if (!state.shows.dragState.isDragging) {
        document.removeEventListener('pointermove', handleScenePointerMove);
        document.removeEventListener('pointerup', handleScenePointerUp);
        return;
    }
    
    const dragState = state.shows.dragState;
    const scene = state.shows.scenes.find(s => s.id === dragState.sceneId);
    
    if (scene) {
        // Calculate final position
        const trackEl = document.querySelector(`[data-zone-id="${dragState.currentZoneId}"]`);
        if (trackEl) {
            const scenesContainer = trackEl.querySelector('div > div');
            if (scenesContainer) {
                const rect = scenesContainer.getBoundingClientRect();
                const relativeX = e.clientX - rect.left - dragState.offsetX;
                const rawPercent = (relativeX / rect.width) * 100;
                const snappedPercent = snapToGrid(Math.max(0, Math.min(100 - scene.durationPercent, rawPercent)));
                const finalZone = dragState.currentZoneId !== null ? dragState.currentZoneId : dragState.originalZoneId;
                
                // Only update if position changed
                if (snappedPercent !== dragState.originalStartPercent || finalZone !== dragState.originalZoneId) {
                    updateScenePosition(dragState.sceneId, snappedPercent, finalZone);
                } else {
                    // Restore original position
                    scene.startTimePercent = dragState.originalStartPercent;
                    scene.zoneId = dragState.originalZoneId;
                }
            }
        }
    }
    
    // Reset drag state
    state.shows.dragState = {
        isDragging: false,
        sceneId: null,
        startX: 0,
        offsetX: 0,
        currentZoneId: null,
        originalStartPercent: 0,
        originalZoneId: 0
    };
    
    document.removeEventListener('pointermove', handleScenePointerMove);
    document.removeEventListener('pointerup', handleScenePointerUp);
    
    renderScenes();
}

function detectZoneFromY(clientY) {
    const tracks = document.querySelectorAll('.timeline-track');
    for (let i = 0; i < tracks.length; i++) {
        const track = tracks[i];
        const rect = track.getBoundingClientRect();
        if (clientY >= rect.top && clientY <= rect.bottom) {
            const zoneId = parseInt(track.getAttribute('data-zone-id'));
            return zoneId;
        }
    }
    return null;
}

function snapToGrid(percent) {
    return Math.round(percent / 1.0) * 1.0;
}

function updateScenePosition(sceneId, newStartPercent, newZoneId) {
    const scene = state.shows.scenes.find(s => s.id === sceneId);
    if (!scene) return;
    
    scene.startTimePercent = newStartPercent;
    scene.zoneId = newZoneId;
    
    if (state.shows.current) {
        state.shows.current.isSaved = false;
        state.shows.current.scenes = state.shows.scenes;
    }
    
    updateShowsUI();
    renderScenes();
}

function handleSceneKeyDown(e, scene) {
    if (!state.connected) return;
    
    switch (e.key) {
        case 'Enter':
        case ' ':
            e.preventDefault();
            // Open scene editor (to be implemented)
            break;
        case 'ArrowLeft':
            e.preventDefault();
            updateScenePosition(scene.id, Math.max(0, scene.startTimePercent - (e.shiftKey ? 5 : 1)), scene.zoneId);
            break;
        case 'ArrowRight':
            e.preventDefault();
            updateScenePosition(scene.id, Math.min(100 - scene.durationPercent, scene.startTimePercent + (e.shiftKey ? 5 : 1)), scene.zoneId);
            break;
        case 'ArrowUp':
            e.preventDefault();
            {
                const zoneIds = ZONE_LABELS.map(z => z.id);
                const currentIndex = zoneIds.indexOf(scene.zoneId);
                const prevZone = currentIndex > 0 ? zoneIds[currentIndex - 1] : zoneIds[zoneIds.length - 1];
                updateScenePosition(scene.id, scene.startTimePercent, prevZone);
            }
            break;
        case 'ArrowDown':
            e.preventDefault();
            {
                const zoneIds = ZONE_LABELS.map(z => z.id);
                const currentIndex = zoneIds.indexOf(scene.zoneId);
                const nextZone = currentIndex < zoneIds.length - 1 ? zoneIds[currentIndex + 1] : zoneIds[0];
                updateScenePosition(scene.id, scene.startTimePercent, nextZone);
            }
            break;
        case 'Delete':
            e.preventDefault();
            if (window.confirm(`Delete scene "${scene.effectName}"?`)) {
                deleteScene(scene.id);
            }
            break;
    }
}

// ─────────────────────────────────────────────────────────────
// Scene Management
// ─────────────────────────────────────────────────────────────

function addScene(zoneId, effectName) {
    if (!state.shows.current) {
        log('[SHOWS] No show selected');
        return;
    }
    
    const newScene = {
        id: 'scene-' + Date.now(),
        zoneId: zoneId,
        effectName: effectName || 'New Scene',
        startTimePercent: 0,
        durationPercent: 10,
        accentColor: ZONE_LABELS.find(z => z.id === zoneId)?.color || 'primary'
    };
    
    state.shows.scenes.push(newScene);
    state.shows.current.scenes = state.shows.scenes;
    state.shows.current.isSaved = false;
    
    updateShowsUI();
    renderScenes();
}

function deleteScene(sceneId) {
    state.shows.scenes = state.shows.scenes.filter(s => s.id !== sceneId);
    if (state.shows.current) {
        state.shows.current.scenes = state.shows.scenes;
        state.shows.current.isSaved = false;
    }
    
    updateShowsUI();
    renderScenes();
}

function updateScene(sceneId, updates) {
    const scene = state.shows.scenes.find(s => s.id === sceneId);
    if (!scene) return;
    
    Object.assign(scene, updates);
    if (state.shows.current) {
        state.shows.current.scenes = state.shows.scenes;
        state.shows.current.isSaved = false;
    }
    
    updateShowsUI();
    renderScenes();
}

function duplicateScene(sceneId) {
    const scene = state.shows.scenes.find(s => s.id === sceneId);
    if (!scene) return;
    
    const newScene = {
        ...scene,
        id: 'scene-' + Date.now(),
        startTimePercent: Math.min(100 - scene.durationPercent, scene.startTimePercent + scene.durationPercent)
    };
    
    state.shows.scenes.push(newScene);
    if (state.shows.current) {
        state.shows.current.scenes = state.shows.scenes;
        state.shows.current.isSaved = false;
    }
    
    updateShowsUI();
    renderScenes();
    renderSceneList();
}

// ─────────────────────────────────────────────────────────────
// Scene List Rendering
// ─────────────────────────────────────────────────────────────

function renderSceneList() {
    const sceneListEl = document.getElementById('sceneList');
    if (!sceneListEl) return;
    
    if (!state.shows.current || !state.shows.scenes || state.shows.scenes.length === 0) {
        sceneListEl.innerHTML = '<div class="value-secondary compact" style="text-align: center; padding: var(--space-sm);">No scenes. Add scenes by dragging effects onto the timeline.</div>';
        return;
    }
    
    sceneListEl.innerHTML = state.shows.scenes.map((scene, index) => {
        const zoneLabel = ZONE_LABELS.find(z => z.id === scene.zoneId)?.label || `Zone ${scene.zoneId}`;
        const colors = SCENE_COLORS[scene.zoneId] || SCENE_COLORS[0];
        return `
            <div class="scene-list-item" style="background: ${colors.bg}; border-color: ${colors.border};">
                <div style="flex: 1;">
                    <div style="font-size: 0.75rem; color: ${colors.text}; font-weight: 600;">${scene.effectName}</div>
                    <div style="font-size: 0.65rem; color: var(--text-secondary); margin-top: 2px;">
                        ${zoneLabel} • ${scene.startTimePercent.toFixed(1)}% • ${scene.durationPercent.toFixed(1)}% duration
                    </div>
                </div>
                <button class="control-btn compact" style="font-size: 0.7rem; padding: var(--space-xs) var(--space-sm);" 
                        onclick="deleteScene('${scene.id}')" aria-label="Delete scene">
                    ✕
                </button>
            </div>
        `;
    }).join('');
}

// ─────────────────────────────────────────────────────────────
// Shows Tab UI Handlers
// ─────────────────────────────────────────────────────────────

function toggleShowsTab() {
    const showsTabRow = document.getElementById('showsTabRow');
    if (!showsTabRow) return;
    
    const isVisible = showsTabRow.style.display !== 'none';
    showsTabRow.style.display = isVisible ? 'none' : 'block';
    
    if (!isVisible) {
        // Load shows when tab is opened
        fetchShowsList();
    }
}

function onShowSelectChange() {
    const select = elements.showSelect;
    if (!select || !select.value) {
        state.shows.current = null;
        state.shows.scenes = [];
        renderTimeline();
        return;
    }
    
    fetchShowDetails(select.value, 'scenes');
}

function onShowNew() {
    const name = prompt('Enter show name:', 'New Show');
    if (!name) return;
    
    const durationSeconds = parseInt(prompt('Enter duration in seconds:', '120')) || 120;
    
    const newShow = {
        id: 'new-' + Date.now(),
        name: name,
        durationSeconds: durationSeconds,
        scenes: [],
        isSaved: false,
        type: 'custom'
    };
    
    state.shows.current = newShow;
    state.shows.scenes = [];
    
    updateShowsUI();
    renderTimeline();
}

function onShowSave() {
    if (!state.shows.current) {
        log('[SHOWS] No show to save');
        return;
    }
    
    const show = state.shows.current;
    
    if (show.type === 'builtin') {
        log('[SHOWS] Cannot save built-in shows');
        return;
    }
    
    if (show.id.startsWith('new-')) {
        // Create new show
        createShow(show.name, show.durationSeconds, show.scenes);
    } else {
        // Update existing show
        updateShow(show.id, show.name, show.durationSeconds, show.scenes);
    }
}

// ─────────────────────────────────────────────────────────────
// Effect Modifiers API Functions
// ─────────────────────────────────────────────────────────────

/**
 * Get default parameters for a modifier type
 */
function getModifierDefaults(type) {
    const defaults = {
        speed: { multiplier: 1.5 },
        intensity: { baseIntensity: 1.0, source: 'CONSTANT' },
        color_shift: { hueOffset: 64 },
        mirror: {},
        glitch: { intensity: 0.3 },
        blur: { radius: 2, strength: 0.8, mode: 'BOX' },
        trail: { fadeRate: 20, mode: 'CONSTANT' },
        saturation: { saturation: 200, mode: 'ABSOLUTE' },
        strobe: { dutyCycle: 0.3, subdivision: 4, mode: 'BEAT_SYNC' }
    };
    return defaults[type] || {};
}

/**
 * Fetch current modifiers list from device
 */
function fetchModifiersList() {
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/modifiers/list`, {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' }
    })
    .then(data => {
        if (data.success && data.data) {
            state.modifiers.stack = data.data.modifiers || [];
            log(`[REST] Loaded ${state.modifiers.stack.length} modifiers`);
            updateModifiersUI();
        }
    })
    .catch(e => {
        log(`[REST] Error fetching modifiers: ${e.message}`);
    });
}

/**
 * Update the modifiers UI to reflect current stack
 */
function updateModifiersUI() {
    const container = document.getElementById('modifierStack');
    if (!container) return;

    if (state.modifiers.stack.length === 0) {
        container.innerHTML = '<div class="value-secondary compact" style="text-align: center;">No modifiers active</div>';
    } else {
        // Build stack display - each modifier as a removable chip
        const html = state.modifiers.stack.map(mod => {
            const displayName = mod.type.replace('_', ' ').replace(/\b\w/g, c => c.toUpperCase());
            return `
                <div class="modifier-chip" data-type="${mod.type}" style="display: inline-flex; align-items: center; gap: 6px; padding: 4px 10px; margin: 3px; background: rgba(255,208,80,0.15); border: 1px solid var(--gold); border-radius: 12px; cursor: pointer;">
                    <span style="font-size: 12px; color: var(--gold);">${displayName}</span>
                    <button class="modifier-remove-btn" data-type="${mod.type}" style="background: none; border: none; color: var(--text-secondary); cursor: pointer; padding: 0 2px; font-size: 14px;">&times;</button>
                </div>
            `;
        }).join('');
        container.innerHTML = html;

        // Attach click handlers for editing
        container.querySelectorAll('.modifier-chip').forEach(chip => {
            chip.addEventListener('click', (e) => {
                // Don't trigger edit if clicking the remove button
                if (e.target.classList.contains('modifier-remove-btn')) return;
                const type = chip.dataset.type;
                openModifierEditor(type);
            });
        });

        // Attach remove handlers
        container.querySelectorAll('.modifier-remove-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                e.stopPropagation();
                const type = btn.dataset.type;
                removeModifier(type);
            });
        });
    }

    // Update add buttons to show which are active
    document.querySelectorAll('.modifier-add-btn').forEach(btn => {
        const type = btn.dataset.modifier;
        const isActive = state.modifiers.stack.some(m => m.type === type);
        if (isActive) {
            btn.style.background = 'rgba(255,208,80,0.3)';
            btn.style.borderColor = 'var(--gold)';
        } else {
            btn.style.background = '';
            btn.style.borderColor = '';
        }
    });
}

/**
 * Add a modifier to the stack
 */
function addModifier(type) {
    // Check if already active
    if (state.modifiers.stack.some(m => m.type === type)) {
        log(`[MODIFIERS] ${type} already active, opening editor`);
        openModifierEditor(type);
        return;
    }

    const params = getModifierDefaults(type);
    const payload = { type, ...params };

    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/modifiers/add`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(data => {
        if (data.success) {
            log(`[REST] Added modifier: ${type}`);
            fetchModifiersList();
        } else {
            log(`[REST] Failed to add modifier: ${data.error || 'Unknown error'}`);
        }
    })
    .catch(e => {
        log(`[REST] Error adding modifier: ${e.message}`);
    });
}

/**
 * Remove a modifier from the stack
 */
function removeModifier(type) {
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/modifiers/remove`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ type })
    })
    .then(data => {
        if (data.success) {
            log(`[REST] Removed modifier: ${type}`);
            // Close editor if this modifier was being edited
            if (state.modifiers.selectedType === type) {
                closeModifierEditor();
            }
            fetchModifiersList();
        } else {
            log(`[REST] Failed to remove modifier: ${data.error || 'Unknown error'}`);
        }
    })
    .catch(e => {
        log(`[REST] Error removing modifier: ${e.message}`);
    });
}

/**
 * Clear all modifiers from the stack
 */
function clearAllModifiers() {
    fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/modifiers/clear`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }
    })
    .then(data => {
        if (data.success) {
            log(`[REST] Cleared all modifiers`);
            closeModifierEditor();
            fetchModifiersList();
        } else {
            log(`[REST] Failed to clear modifiers: ${data.error || 'Unknown error'}`);
        }
    })
    .catch(e => {
        log(`[REST] Error clearing modifiers: ${e.message}`);
    });
}

/**
 * Open the parameter editor for a modifier
 */
function openModifierEditor(type) {
    const modifier = state.modifiers.stack.find(m => m.type === type);
    if (!modifier) {
        log(`[MODIFIERS] Modifier ${type} not found in stack`);
        return;
    }

    state.modifiers.selectedType = type;
    const editorRow = document.getElementById('modifierEditorRow');
    const titleEl = document.getElementById('modifierEditorTitle');
    const paramsContainer = document.getElementById('modifierParamsContainer');

    if (!editorRow || !paramsContainer) return;

    // Set title
    const displayName = type.replace('_', ' ').replace(/\b\w/g, c => c.toUpperCase());
    if (titleEl) titleEl.textContent = `Edit ${displayName}`;

    // Build parameter inputs based on modifier type
    let html = '';
    const params = modifier.params || getModifierDefaults(type);

    switch (type) {
        case 'speed':
            html = `
                <label class="value-secondary" style="display: block; margin-bottom: 4px;">Multiplier (0.1 - 3.0)</label>
                <input type="range" id="modParam_multiplier" min="0.1" max="3.0" step="0.1" value="${params.multiplier || 1.5}" style="width: 100%;">
                <div class="value-secondary" id="modParam_multiplier_val" style="text-align: center;">${params.multiplier || 1.5}x</div>
            `;
            break;

        case 'intensity':
            html = `
                <label class="value-secondary" style="display: block; margin-bottom: 4px;">Base Intensity (0.0 - 2.0)</label>
                <input type="range" id="modParam_baseIntensity" min="0" max="2.0" step="0.1" value="${params.baseIntensity || 1.0}" style="width: 100%;">
                <div class="value-secondary" id="modParam_baseIntensity_val" style="text-align: center;">${params.baseIntensity || 1.0}</div>
                <label class="value-secondary" style="display: block; margin-top: 8px; margin-bottom: 4px;">Source</label>
                <select id="modParam_source" style="width: 100%; padding: 6px; background: var(--bg-card); color: var(--text-primary); border: 1px solid var(--text-secondary); border-radius: 4px;">
                    <option value="CONSTANT" ${params.source === 'CONSTANT' ? 'selected' : ''}>Constant</option>
                    <option value="BEAT_PHASE" ${params.source === 'BEAT_PHASE' ? 'selected' : ''}>Beat Phase</option>
                </select>
            `;
            break;

        case 'color_shift':
            html = `
                <label class="value-secondary" style="display: block; margin-bottom: 4px;">Hue Offset (0 - 255)</label>
                <input type="range" id="modParam_hueOffset" min="0" max="255" step="1" value="${params.hueOffset || 64}" style="width: 100%;">
                <div class="value-secondary" id="modParam_hueOffset_val" style="text-align: center;">${params.hueOffset || 64}</div>
            `;
            break;

        case 'mirror':
            html = '<div class="value-secondary" style="text-align: center; padding: 12px;">Mirror has no parameters</div>';
            break;

        case 'glitch':
            html = `
                <label class="value-secondary" style="display: block; margin-bottom: 4px;">Intensity (0.0 - 1.0)</label>
                <input type="range" id="modParam_intensity" min="0" max="1.0" step="0.05" value="${params.intensity || 0.3}" style="width: 100%;">
                <div class="value-secondary" id="modParam_intensity_val" style="text-align: center;">${params.intensity || 0.3}</div>
            `;
            break;

        case 'blur':
            html = `
                <label class="value-secondary" style="display: block; margin-bottom: 4px;">Radius (1 - 10)</label>
                <input type="range" id="modParam_radius" min="1" max="10" step="1" value="${params.radius || 2}" style="width: 100%;">
                <div class="value-secondary" id="modParam_radius_val" style="text-align: center;">${params.radius || 2}</div>
                <label class="value-secondary" style="display: block; margin-top: 8px; margin-bottom: 4px;">Strength (0.0 - 1.0)</label>
                <input type="range" id="modParam_strength" min="0" max="1.0" step="0.1" value="${params.strength || 0.8}" style="width: 100%;">
                <div class="value-secondary" id="modParam_strength_val" style="text-align: center;">${params.strength || 0.8}</div>
                <label class="value-secondary" style="display: block; margin-top: 8px; margin-bottom: 4px;">Mode</label>
                <select id="modParam_mode" style="width: 100%; padding: 6px; background: var(--bg-card); color: var(--text-primary); border: 1px solid var(--text-secondary); border-radius: 4px;">
                    <option value="BOX" ${params.mode === 'BOX' ? 'selected' : ''}>Box</option>
                    <option value="GAUSSIAN" ${params.mode === 'GAUSSIAN' ? 'selected' : ''}>Gaussian</option>
                </select>
            `;
            break;

        case 'trail':
            html = `
                <label class="value-secondary" style="display: block; margin-bottom: 4px;">Fade Rate (1 - 100)</label>
                <input type="range" id="modParam_fadeRate" min="1" max="100" step="1" value="${params.fadeRate || 20}" style="width: 100%;">
                <div class="value-secondary" id="modParam_fadeRate_val" style="text-align: center;">${params.fadeRate || 20}</div>
                <label class="value-secondary" style="display: block; margin-top: 8px; margin-bottom: 4px;">Mode</label>
                <select id="modParam_mode" style="width: 100%; padding: 6px; background: var(--bg-card); color: var(--text-primary); border: 1px solid var(--text-secondary); border-radius: 4px;">
                    <option value="CONSTANT" ${params.mode === 'CONSTANT' ? 'selected' : ''}>Constant</option>
                    <option value="BEAT_SYNC" ${params.mode === 'BEAT_SYNC' ? 'selected' : ''}>Beat Sync</option>
                </select>
            `;
            break;

        case 'saturation':
            html = `
                <label class="value-secondary" style="display: block; margin-bottom: 4px;">Saturation (0 - 255)</label>
                <input type="range" id="modParam_saturation" min="0" max="255" step="1" value="${params.saturation || 200}" style="width: 100%;">
                <div class="value-secondary" id="modParam_saturation_val" style="text-align: center;">${params.saturation || 200}</div>
                <label class="value-secondary" style="display: block; margin-top: 8px; margin-bottom: 4px;">Mode</label>
                <select id="modParam_mode" style="width: 100%; padding: 6px; background: var(--bg-card); color: var(--text-primary); border: 1px solid var(--text-secondary); border-radius: 4px;">
                    <option value="ABSOLUTE" ${params.mode === 'ABSOLUTE' ? 'selected' : ''}>Absolute</option>
                    <option value="RELATIVE" ${params.mode === 'RELATIVE' ? 'selected' : ''}>Relative</option>
                </select>
            `;
            break;

        case 'strobe':
            html = `
                <label class="value-secondary" style="display: block; margin-bottom: 4px;">Duty Cycle (0.1 - 0.9)</label>
                <input type="range" id="modParam_dutyCycle" min="0.1" max="0.9" step="0.1" value="${params.dutyCycle || 0.3}" style="width: 100%;">
                <div class="value-secondary" id="modParam_dutyCycle_val" style="text-align: center;">${params.dutyCycle || 0.3}</div>
                <label class="value-secondary" style="display: block; margin-top: 8px; margin-bottom: 4px;">Subdivision (1 - 8)</label>
                <input type="range" id="modParam_subdivision" min="1" max="8" step="1" value="${params.subdivision || 4}" style="width: 100%;">
                <div class="value-secondary" id="modParam_subdivision_val" style="text-align: center;">${params.subdivision || 4}</div>
                <label class="value-secondary" style="display: block; margin-top: 8px; margin-bottom: 4px;">Mode</label>
                <select id="modParam_mode" style="width: 100%; padding: 6px; background: var(--bg-card); color: var(--text-primary); border: 1px solid var(--text-secondary); border-radius: 4px;">
                    <option value="BEAT_SYNC" ${params.mode === 'BEAT_SYNC' ? 'selected' : ''}>Beat Sync</option>
                    <option value="FREE_RUN" ${params.mode === 'FREE_RUN' ? 'selected' : ''}>Free Run</option>
                </select>
            `;
            break;

        default:
            html = '<div class="value-secondary" style="text-align: center; padding: 12px;">No parameters available</div>';
    }

    paramsContainer.innerHTML = html;

    // Attach live update handlers to sliders
    paramsContainer.querySelectorAll('input[type="range"]').forEach(slider => {
        slider.addEventListener('input', () => {
            const valEl = document.getElementById(slider.id + '_val');
            if (valEl) {
                let suffix = '';
                if (slider.id.includes('multiplier')) suffix = 'x';
                valEl.textContent = slider.value + suffix;
            }
        });
    });

    // Show editor
    editorRow.style.display = '';
}

/**
 * Close the modifier parameter editor
 */
function closeModifierEditor() {
    state.modifiers.selectedType = null;
    const editorRow = document.getElementById('modifierEditorRow');
    if (editorRow) {
        editorRow.style.display = 'none';
    }
}

/**
 * Apply modifier parameter changes
 */
function applyModifierChanges() {
    const type = state.modifiers.selectedType;
    if (!type) {
        log('[MODIFIERS] No modifier selected');
        return;
    }

    // Collect parameters from form
    const params = {};
    const paramsContainer = document.getElementById('modifierParamsContainer');
    if (!paramsContainer) return;

    // Get all inputs
    paramsContainer.querySelectorAll('input[type="range"], select').forEach(input => {
        const key = input.id.replace('modParam_', '');
        if (input.type === 'range') {
            // Parse as float for decimals, int for whole numbers
            const val = parseFloat(input.value);
            params[key] = val;
        } else {
            params[key] = input.value;
        }
    });

    const payload = { type, ...params };

    // Debounce rapid changes
    if (state.modifiers.pendingUpdate) {
        clearTimeout(state.modifiers.pendingUpdate);
    }

    state.modifiers.pendingUpdate = setTimeout(() => {
        fetchWithRetry(`http://${state.deviceHost || '192.168.0.16'}/api/v1/modifiers/update`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        })
        .then(data => {
            if (data.success) {
                log(`[REST] Updated modifier: ${type}`);
                fetchModifiersList();
            } else {
                log(`[REST] Failed to update modifier: ${data.error || 'Unknown error'}`);
            }
        })
        .catch(e => {
            log(`[REST] Error updating modifier: ${e.message}`);
        });

        state.modifiers.pendingUpdate = null;
    }, 100);
}

// ─────────────────────────────────────────────────────────────
// Color Correction API Functions
// ─────────────────────────────────────────────────────────────

/**
 * Fetch color correction config from device via WebSocket
 */
function fetchColorCorrectionConfig() {
    send({ type: 'colorCorrection.getConfig' });
}

/**
 * Update the color correction UI to reflect current state
 */
function updateColorCorrectionUI() {
    const cc = state.colorCorrection;

    const presetSelect = document.getElementById('ccPreset');
    const modeSelect = document.getElementById('ccMode');
    const autoExposureCheckbox = document.getElementById('ccAutoExposure');
    const brownGuardrailCheckbox = document.getElementById('ccBrownGuardrail');
    const gammaCheckbox = document.getElementById('ccGamma');
    const aeTargetSlider = document.getElementById('ccAETarget');
    const aeTargetVal = document.getElementById('ccAETargetVal');
    const gammaSlider = document.getElementById('ccGammaSlider');
    const gammaVal = document.getElementById('ccGammaVal');

    if (presetSelect) presetSelect.value = cc.currentPreset || 2;
    if (modeSelect) modeSelect.value = cc.mode;
    if (autoExposureCheckbox) autoExposureCheckbox.checked = cc.autoExposureEnabled;
    if (brownGuardrailCheckbox) brownGuardrailCheckbox.checked = cc.brownGuardrailEnabled;
    if (gammaCheckbox) gammaCheckbox.checked = cc.gammaEnabled;
    if (aeTargetSlider) aeTargetSlider.value = cc.autoExposureTarget;
    if (aeTargetVal) aeTargetVal.textContent = cc.autoExposureTarget;
    if (gammaSlider) gammaSlider.value = Math.round(cc.gammaValue * 10);
    if (gammaVal) gammaVal.textContent = cc.gammaValue.toFixed(1);
}

/**
 * Send color correction config update via WebSocket
 */
function setColorCorrectionConfig(params) {
    send({ type: 'colorCorrection.setConfig', ...params });
}

/**
 * Save color correction config to NVS via WebSocket
 */
function saveColorCorrection() {
    send({ type: 'colorCorrection.save' });
    log('[COLOR] Saved color correction config to device');
}

/**
 * Apply a color correction preset
 * @param {number} preset - Preset ID (0=Off, 1=Subtle, 2=Balanced, 3=Aggressive)
 * @param {boolean} save - Whether to persist to NVS
 */
function setColorCorrectionPreset(preset, save = false) {
    send({ type: 'colorCorrection.setPreset', preset, save });
    state.colorCorrection.currentPreset = preset;
    log(`[COLOR] Applying preset: ${['Off', 'Subtle', 'Balanced', 'Aggressive'][preset] || preset}`);
}

/**
 * Fetch available presets and current preset via WebSocket
 */
function fetchColorCorrectionPresets() {
    send({ type: 'colorCorrection.getPresets' });
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
    elements.zone3SpeedSlider = document.getElementById('zone3SpeedSlider');
    elements.zone3SpeedValue = document.getElementById('zone3SpeedValue');

    // Zone brightness sliders
    elements.zone0BrightnessSlider = document.getElementById('zone0BrightnessSlider');
    elements.zone0BrightnessValue = document.getElementById('zone0BrightnessValue');
    elements.zone1BrightnessSlider = document.getElementById('zone1BrightnessSlider');
    elements.zone1BrightnessValue = document.getElementById('zone1BrightnessValue');
    elements.zone2BrightnessSlider = document.getElementById('zone2BrightnessSlider');
    elements.zone2BrightnessValue = document.getElementById('zone2BrightnessValue');
    elements.zone3BrightnessSlider = document.getElementById('zone3BrightnessSlider');
    elements.zone3BrightnessValue = document.getElementById('zone3BrightnessValue');

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
            // Refresh zone quick controls to apply filter
            if (state.zones && state.zones.enabled) {
                renderZoneQuickControls();
            }
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
    
    // Shows tab event handlers
    if (elements.showsTabToggle) {
        elements.showsTabToggle.addEventListener('click', toggleShowsTab);
    }
    
    if (elements.showSelect) {
        elements.showSelect.addEventListener('change', onShowSelectChange);
    }
    
    if (elements.showNewBtn) {
        elements.showNewBtn.addEventListener('click', onShowNew);
    }
    
    if (elements.showSaveBtn) {
        elements.showSaveBtn.addEventListener('click', onShowSave);
    }
    
    if (elements.showPlayBtn) {
        elements.showPlayBtn.addEventListener('click', () => {
            if (state.shows.current) {
                // Only built-in shows can be played (ShowNode limitation)
                const showId = state.shows.current.type === 'builtin' 
                    ? parseInt(state.shows.current.id) 
                    : null;
                if (showId !== null && !isNaN(showId)) {
                    controlShow('start', showId);
                } else {
                    log('[SHOWS] Custom shows cannot be played yet (ShowNode limitation)');
                    alert('Custom shows cannot be played yet. Only built-in shows (0-9) are supported for playback.');
                }
            }
        });
    }
    
    if (elements.showPauseBtn) {
        elements.showPauseBtn.addEventListener('click', () => {
            controlShow('pause');
        });
    }
    
    if (elements.showStopBtn) {
        elements.showStopBtn.addEventListener('click', () => {
            controlShow('stop');
        });
    }
    
    if (elements.showSeekSlider) {
        elements.showSeekSlider.addEventListener('input', (e) => {
            if (state.shows.current && state.shows.current.durationSeconds > 0) {
                const percent = parseFloat(e.target.value);
                const timeMs = Math.floor((percent / 100) * state.shows.current.durationSeconds * 1000);
                controlShow('seek', undefined, timeMs);
            }
        });
    }
    
    // Poll for show status during playback
    setInterval(() => {
        if (state.shows.isPlaying && state.connected) {
            fetchCurrentShowStatus();
        }
    }, 100);
    
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
    if (elements.zone3SpeedSlider) {
        elements.zone3SpeedSlider.addEventListener('input', () => onZoneSpeedChange(3));
    }

    // Bind zone brightness slider events
    if (elements.zone0BrightnessSlider) {
        elements.zone0BrightnessSlider.addEventListener('input', () => onZoneBrightnessChange(0));
    }
    if (elements.zone1BrightnessSlider) {
        elements.zone1BrightnessSlider.addEventListener('input', () => onZoneBrightnessChange(1));
    }
    if (elements.zone2BrightnessSlider) {
        elements.zone2BrightnessSlider.addEventListener('input', () => onZoneBrightnessChange(2));
    }
    if (elements.zone3BrightnessSlider) {
        elements.zone3BrightnessSlider.addEventListener('input', () => onZoneBrightnessChange(3));
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

    // Bind preset library events
    const presetSaveButton = document.getElementById('presetSaveButton');
    const presetUploadButton = document.getElementById('presetUploadButton');
    const presetUploadInput = document.getElementById('presetUploadInput');
    const presetNameInput = document.getElementById('presetNameInput');
    
    if (presetSaveButton) {
        presetSaveButton.addEventListener('click', saveCurrentAsPreset);
    }
    
    if (presetNameInput) {
        presetNameInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                saveCurrentAsPreset();
            }
        });
    }
    
    if (presetUploadButton && presetUploadInput) {
        presetUploadButton.addEventListener('click', () => {
            presetUploadInput.click();
        });
        
        presetUploadInput.addEventListener('change', (e) => {
            const file = e.target.files[0];
            if (file) {
                const fileNameSpan = document.getElementById('presetUploadFileName');
                if (fileNameSpan) {
                    fileNameSpan.textContent = file.name;
                }
                uploadPreset(file);
            }
        });
    }

    // Bind modifier events
    document.querySelectorAll('.modifier-add-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            const type = btn.dataset.modifier;
            if (type) {
                addModifier(type);
            }
        });
    });

    const modifierClearBtn = document.getElementById('modifierClearBtn');
    if (modifierClearBtn) {
        modifierClearBtn.addEventListener('click', clearAllModifiers);
    }

    const modifierApplyBtn = document.getElementById('modifierApplyBtn');
    if (modifierApplyBtn) {
        modifierApplyBtn.addEventListener('click', applyModifierChanges);
    }

    const modifierCloseBtn = document.getElementById('modifierCloseBtn');
    if (modifierCloseBtn) {
        modifierCloseBtn.addEventListener('click', closeModifierEditor);
    }

    // Bind color correction events
    const ccPreset = document.getElementById('ccPreset');
    if (ccPreset) {
        ccPreset.addEventListener('change', (e) => {
            setColorCorrectionPreset(parseInt(e.target.value));
        });
    }

    const ccMode = document.getElementById('ccMode');
    if (ccMode) {
        ccMode.addEventListener('change', (e) => {
            setColorCorrectionConfig({ mode: parseInt(e.target.value) });
        });
    }

    const ccAutoExposure = document.getElementById('ccAutoExposure');
    if (ccAutoExposure) {
        ccAutoExposure.addEventListener('change', (e) => {
            setColorCorrectionConfig({ autoExposureEnabled: e.target.checked });
        });
    }

    const ccBrownGuardrail = document.getElementById('ccBrownGuardrail');
    if (ccBrownGuardrail) {
        ccBrownGuardrail.addEventListener('change', (e) => {
            setColorCorrectionConfig({ brownGuardrailEnabled: e.target.checked });
        });
    }

    const ccGamma = document.getElementById('ccGamma');
    if (ccGamma) {
        ccGamma.addEventListener('change', (e) => {
            setColorCorrectionConfig({ gammaEnabled: e.target.checked });
        });
    }

    const ccAETarget = document.getElementById('ccAETarget');
    const ccAETargetVal = document.getElementById('ccAETargetVal');
    if (ccAETarget) {
        ccAETarget.addEventListener('input', (e) => {
            if (ccAETargetVal) ccAETargetVal.textContent = e.target.value;
        });
        ccAETarget.addEventListener('change', (e) => {
            setColorCorrectionConfig({ autoExposureTarget: parseInt(e.target.value) });
        });
    }

    const ccGammaSlider = document.getElementById('ccGammaSlider');
    const ccGammaVal = document.getElementById('ccGammaVal');
    if (ccGammaSlider) {
        ccGammaSlider.addEventListener('input', (e) => {
            const val = (parseInt(e.target.value) / 10).toFixed(1);
            if (ccGammaVal) ccGammaVal.textContent = val;
        });
        ccGammaSlider.addEventListener('change', (e) => {
            setColorCorrectionConfig({ gammaValue: parseInt(e.target.value) / 10 });
        });
    }

    const ccSaveBtn = document.getElementById('ccSaveBtn');
    if (ccSaveBtn) {
        ccSaveBtn.addEventListener('click', saveColorCorrection);
    }

    // Check if we're running on the device or remotely
    // Only hide config bar if we're actually ON the device (192.168.0.16 or lightwaveos.local)
    const isOnDevice = window.location.hostname === 'lightwaveos.local' ||
                       window.location.hostname === '192.168.0.16';

    if (isOnDevice) {
        // Running on device - hide config bar, auto-connect
        // CRITICAL: Set deviceHost to current hostname for API calls
        state.deviceHost = window.location.hostname;
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
