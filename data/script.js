// WebSocket connection
let socket = null;
let deviceStatus = null;
let reconnectInterval = null;
let reconnectAttempts = 0;
const maxReconnectAttempts = 5;

// DOM elements
const statusIndicator = document.getElementById('status-indicator');
const statusText = document.getElementById('status-text');
const deviceIp = document.getElementById('device-ip');
const paletteName = document.getElementById('palette-name');
const paletteGrid = document.getElementById('palette-grid');
const presetList = document.getElementById('preset-list');

// Parameter value elements
const brightnessValue = document.getElementById('brightness-value');
const fadeAmountValue = document.getElementById('fade-amount-value');
const fpsValue = document.getElementById('fps-value');
const paletteSpeedValue = document.getElementById('palette-speed-value');

// Parameter sliders
const brightnessSlider = document.getElementById('brightness');
const fadeAmountSlider = document.getElementById('fade-amount');
const fpsSlider = document.getElementById('fps');
const paletteSpeedSlider = document.getElementById('palette-speed');

// Preset controls
const presetNameInput = document.getElementById('preset-name');
const savePresetButton = document.getElementById('save-preset');

// Palette colors for previews (simplified approximations)
const paletteColors = [
    // Sunset_Real
    'linear-gradient(to right, #780000, #b32200, #ff6800, #a71600, #640067, #100082, #0000a0)',
    // es_rivendell_15
    'linear-gradient(to right, #010e05, #10240e, #38441e, #96c963, #96c963)',
    // es_ocean_breeze_036
    'linear-gradient(to right, #010607, #01636f, #90d1ff, #004952)',
    // rgi_15
    'linear-gradient(to right, #04011f, #370110, #c50307, #3b0211, #060222, #270621, #700d20, #380923, #160626)',
    // retro2_16
    'linear-gradient(to right, #bc8701, #2e0701)',
    // Analogous_1
    'linear-gradient(to right, #0300ff, #1700ff, #4300ff, #8e002d, #ff0000)',
    // es_pinksplash_08
    'linear-gradient(to right, #7e0bff, #c50116, #d29dac, #9d0370, #9d0370)',
    // Coral_reef
    'linear-gradient(to right, #28c7c5, #0a989b, #016f78, #2b7fa2, #0a496f, #012247)',
    // es_ocean_breeze_068
    'linear-gradient(to right, #649c99, #016389, #014454, #238ea8, #003f75, #010a0a)',
    // es_pinksplash_07
    'linear-gradient(to right, #e50101, #f2043f, #ff0cff, #f951fc, #ff0beb, #f40544, #e80105)',
    // es_vintage_01
    'linear-gradient(to right, #040101, #100001, #616803, #ff8313, #430904, #100001, #040101, #040101)',
    // departure
    'linear-gradient(to right, #080300, #170700, #4b2606, #a96326, #d5a977, #ffffff, #87ff8a, #16ff18, #00ff00, #008800, #003700, #003700)',
    // es_landscape_64
    'linear-gradient(to right, #000000, #021901, #0f7305, #4fd501, #7ed32f, #bcd1f7, #90b6cd, #3b75fa, #0125c0)',
    // es_landscape_33
    'linear-gradient(to right, #010500, #201701, #a13701, #e59001, #278e4a, #010401)',
    // rainbowsherbet
    'linear-gradient(to right, #ff2104, #ff4419, #ff0719, #ff5267, #fffff2, #2aff16, #57ff41)',
    // gr65_hult
    'linear-gradient(to right, #f7b0f7, #ff88ff, #dc1de2, #0752b2, #017c6d, #017c6d)',
    // gr64_hult
    'linear-gradient(to right, #017c6d, #015d4f, #344101, #737f01, #344101, #015648, #00372d, #00372d)',
    // GMT_drywet
    'linear-gradient(to right, #2f1e02, #d59318, #67db34, #03dbcf, #0130d6, #01016f, #010721)',
    // ib_jul01
    'linear-gradient(to right, #c20101, #011d12, #39831c, #710101)',
    // es_vintage_57
    'linear-gradient(to right, #020101, #120100, #451d01, #a7870a, #2e3804)',
    // ib15
    'linear-gradient(to right, #715b93, #9d584e, #d05521, #ff1d0b, #891f27, #3b2159)',
    // Fuschia_7
    'linear-gradient(to right, #2b0399, #640467, #bc0542, #a10b73, #8714b6)',
    // es_emerald_dragon_08
    'linear-gradient(to right, #61ff01, #2f8501, #0d2b01, #020a01)',
    // lava
    'linear-gradient(to right, #000000, #120000, #710000, #8e0301, #af1101, #d52c02, #ff5204, #ff7304, #ff9c04, #ffcb04, #ffff04, #ffff47, #ffffff)',
    // fire
    'linear-gradient(to right, #010100, #200500, #c01800, #dc6905, #fcff1f, #fcff6f, #ffffff)',
    // Colorfull
    'linear-gradient(to right, #0a5505, #1d6d12, #3b8a2a, #536334, #6e4240, #7b3141, #8b2342, #c07562, #ffff89, #64b49b, #1679ae)',
    // Magenta_Evening
    'linear-gradient(to right, #471b27, #820b33, #d50240, #e80142, #fc0145, #7b0233, #2e0923)',
    // Pink_Purple
    'linear-gradient(to right, #130227, #1a042d, #210634, #443e7d, #76bbf0, #a3d7f7, #d9f4ff, #9f95dd, #714ebc, #80399b, #92287b)',
    // Sunset_Real
    'linear-gradient(to right, #780000, #b32200, #ff6800, #a71612, #640067, #100082, #0000a0)',
    // es_autumn_19
    'linear-gradient(to right, #1a0101, #430401, #760e01, #899834, #714101, #85953b, #899834, #714101, #8b9a2e, #710d01, #370301, #110101, #110101)',
    // BlacK_Blue_Magenta_White
    'linear-gradient(to right, #000000, #00002d, #0000ff, #2a00ff, #ff00ff, #ff37ff, #ffffff)',
    // BlacK_Magenta_Red
    'linear-gradient(to right, #000000, #2a002d, #ff00ff, #ff002d, #ff0000)',
    // BlacK_Red_Magenta_Yellow
    'linear-gradient(to right, #000000, #2a0000, #ff0000, #ff002d, #ff00ff, #ff372d, #ffff00)',
    // Blue_Cyan_Yellow
    'linear-gradient(to right, #0000ff, #0037ff, #00ffff, #2aff2d, #ffff00)'
];

// Palette names
const paletteNames = [
    "Sunset_Real", "es_rivendell_15", "es_ocean_breeze_036", "rgi_15", "retro2_16",
    "Analogous_1", "es_pinksplash_08", "Coral_reef", "es_ocean_breeze_068", "es_pinksplash_07",
    "es_vintage_01", "departure", "es_landscape_64", "es_landscape_33", "rainbowsherbet",
    "gr65_hult", "gr64_hult", "GMT_drywet", "ib_jul01", "es_vintage_57",
    "ib15", "Fuschia_7", "es_emerald_dragon_08", "lava", "fire",
    "Colorfull", "Magenta_Evening", "Pink_Purple", "es_autumn_19", "BlacK_Blue_Magenta_White",
    "BlacK_Magenta_Red", "BlacK_Red_Magenta_Yellow", "Blue_Cyan_Yellow"
];

// Stored presets
let presets = JSON.parse(localStorage.getItem('lightCrystalsPresets')) || {};

// Initialize the UI
function initUI() {
    // Initialize palette grid
    createPaletteGrid();
    
    // Initialize presets
    updatePresetList();
    
    // Add event listeners for mode buttons
    document.querySelectorAll('.mode-button').forEach(button => {
        button.addEventListener('click', () => {
            const mode = parseInt(button.dataset.mode);
            sendCommand('set_parameters', { visual_mode: mode });
        });
    });
    
    // Add event listeners for sliders
    brightnessSlider.addEventListener('input', () => {
        brightnessValue.textContent = brightnessSlider.value;
        sendCommand('set_parameters', { brightness: parseInt(brightnessSlider.value) });
    });
    
    fadeAmountSlider.addEventListener('input', () => {
        fadeAmountValue.textContent = fadeAmountSlider.value;
        sendCommand('set_parameters', { fade_amount: parseInt(fadeAmountSlider.value) });
    });
    
    fpsSlider.addEventListener('input', () => {
        fpsValue.textContent = fpsSlider.value;
        sendCommand('set_parameters', { fps: parseInt(fpsSlider.value) });
    });
    
    paletteSpeedSlider.addEventListener('input', () => {
        paletteSpeedValue.textContent = paletteSpeedSlider.value;
        sendCommand('set_parameters', { palette_speed: parseInt(paletteSpeedSlider.value) });
    });
    
    // Add event listeners for slider buttons
    document.querySelectorAll('.slider-with-buttons .decrement').forEach(button => {
        button.addEventListener('click', () => {
            const slider = button.parentElement.querySelector('input');
            slider.value = Math.max(parseInt(slider.min), parseInt(slider.value) - 1);
            slider.dispatchEvent(new Event('input'));
        });
    });
    
    document.querySelectorAll('.slider-with-buttons .increment').forEach(button => {
        button.addEventListener('click', () => {
            const slider = button.parentElement.querySelector('input');
            slider.value = Math.min(parseInt(slider.max), parseInt(slider.value) + 1);
            slider.dispatchEvent(new Event('input'));
        });
    });
    
    // Add event listener for save preset button
    savePresetButton.addEventListener('click', savePreset);
    
    // Connect to WebSocket
    connectWebSocket();
}

// Create palette grid
function createPaletteGrid() {
    paletteGrid.innerHTML = '';
    
    for (let i = 0; i < paletteNames.length; i++) {
        const paletteItem = document.createElement('div');
        paletteItem.className = 'palette-item';
        paletteItem.dataset.index = i;
        
        const palettePreview = document.createElement('div');
        palettePreview.className = 'palette-preview';
        palettePreview.style.background = paletteColors[i] || `hsl(${(i * 30) % 360}, 70%, 50%)`;
        
        const paletteName = document.createElement('div');
        paletteName.className = 'palette-name';
        paletteName.textContent = paletteNames[i];
        
        paletteItem.appendChild(palettePreview);
        paletteItem.appendChild(paletteName);
        
        paletteItem.addEventListener('click', () => {
            sendCommand('set_parameters', { palette_index: i });
        });
        
        paletteGrid.appendChild(paletteItem);
    }
}

// Update preset list
function updatePresetList() {
    presetList.innerHTML = '';
    
    Object.entries(presets).forEach(([name, preset]) => {
        const presetItem = document.createElement('div');
        presetItem.className = 'preset-item';
        
        const presetName = document.createElement('span');
        presetName.textContent = name;
        
        const deleteButton = document.createElement('button');
        deleteButton.className = 'preset-delete';
        deleteButton.textContent = '×';
        deleteButton.addEventListener('click', (e) => {
            e.stopPropagation();
            deletePreset(name);
        });
        
        presetItem.appendChild(presetName);
        presetItem.appendChild(deleteButton);
        
        presetItem.addEventListener('click', () => {
            loadPreset(name);
        });
        
        presetList.appendChild(presetItem);
    });
}

// Save preset
function savePreset() {
    const name = presetNameInput.value.trim();
    
    if (!name) {
        alert('Please enter a preset name');
        return;
    }
    
    if (!deviceStatus) {
        alert('Not connected to device');
        return;
    }
    
    presets[name] = {
        brightness: deviceStatus.brightness,
        fade_amount: deviceStatus.fade_amount,
        fps: deviceStatus.fps,
        palette_speed: deviceStatus.palette_speed,
        palette_index: deviceStatus.palette_index,
        visual_mode: deviceStatus.visual_mode
    };
    
    localStorage.setItem('lightCrystalsPresets', JSON.stringify(presets));
    presetNameInput.value = '';
    updatePresetList();
}

// Load preset
function loadPreset(name) {
    const preset = presets[name];
    
    if (!preset) return;
    
    sendCommand('set_parameters', preset);
}

// Delete preset
function deletePreset(name) {
    if (confirm(`Delete preset "${name}"?`)) {
        delete presets[name];
        localStorage.setItem('lightCrystalsPresets', JSON.stringify(presets));
        updatePresetList();
    }
}

// Connect to WebSocket
function connectWebSocket() {
    // Close existing connection if any
    if (socket) {
        socket.close();
    }
    
    // Update UI to show connecting state
    setConnectionStatus('connecting');
    
    // Get WebSocket URL
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const host = window.location.hostname;
    const port = window.location.port ? `:${window.location.port}` : '';
    const wsUrl = `${protocol}//${host}${port}/ws`;
    
    // Create new WebSocket connection
    socket = new WebSocket(wsUrl);
    
    // WebSocket event handlers
    socket.onopen = () => {
        console.log('WebSocket connected');
        setConnectionStatus('connected');
        deviceIp.textContent = host;
        reconnectAttempts = 0;
        
        // Clear reconnect interval if it exists
        if (reconnectInterval) {
            clearInterval(reconnectInterval);
            reconnectInterval = null;
        }
    };
    
    socket.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            updateDeviceStatus(data);
        } catch (error) {
            console.error('Error parsing WebSocket message:', error);
        }
    };
    
    socket.onclose = () => {
        console.log('WebSocket disconnected');
        setConnectionStatus('disconnected');
        
        // Try to reconnect if not already trying
        if (!reconnectInterval && reconnectAttempts < maxReconnectAttempts) {
            reconnectInterval = setInterval(() => {
                reconnectAttempts++;
                console.log(`Reconnect attempt ${reconnectAttempts}/${maxReconnectAttempts}`);
                
                if (reconnectAttempts >= maxReconnectAttempts) {
                    clearInterval(reconnectInterval);
                    reconnectInterval = null;
                    console.log('Max reconnect attempts reached');
                } else {
                    connectWebSocket();
                }
            }, 5000);
        }
    };
    
    socket.onerror = (error) => {
        console.error('WebSocket error:', error);
    };
}

// Set connection status
function setConnectionStatus(status) {
    statusIndicator.className = status;
    
    if (status === 'connected') {
        statusText.textContent = 'Connected';
    } else if (status === 'connecting') {
        statusText.textContent = 'Connecting...';
    } else {
        statusText.textContent = 'Disconnected';
    }
}

// Update device status
function updateDeviceStatus(status) {
    deviceStatus = status;
    
    // Update UI with new status
    brightnessSlider.value = status.brightness;
    brightnessValue.textContent = status.brightness;
    
    fadeAmountSlider.value = status.fade_amount;
    fadeAmountValue.textContent = status.fade_amount;
    
    fpsSlider.value = status.fps;
    fpsValue.textContent = status.fps;
    
    paletteSpeedSlider.value = status.palette_speed;
    paletteSpeedValue.textContent = status.palette_speed;
    
    paletteName.textContent = status.palette_name;
    
    // Update active visual mode
    document.querySelectorAll('.mode-button').forEach(button => {
        if (parseInt(button.dataset.mode) === status.visual_mode) {
            button.classList.add('active');
        } else {
            button.classList.remove('active');
        }
    });
    
    // Update active palette
    document.querySelectorAll('.palette-item').forEach(item => {
        if (parseInt(item.dataset.index) === status.palette_index) {
            item.classList.add('active');
        } else {
            item.classList.remove('active');
        }
    });
}

// Send command to device
function sendCommand(command, parameters) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
        console.error('WebSocket not connected');
        return;
    }
    
    const message = {
        command: command,
        parameters: parameters
    };
    
    socket.send(JSON.stringify(message));
}

// Initialize when DOM is loaded
document.addEventListener('DOMContentLoaded', initUI);
