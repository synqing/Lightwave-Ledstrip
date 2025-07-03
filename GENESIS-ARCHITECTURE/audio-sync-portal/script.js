class AudioSyncController {
    constructor() {
        this.ws = null;
        this.audioPlayer = document.getElementById('audioPlayer');
        this.jsonFile = null;
        this.mp3File = null;
        this.syncOffset = 0;
        this.syncStartTime = 0;
        this.isSynced = false;
        this.syncInterval = null;
        
        // Enhanced features
        this.networkLatency = 0;
        this.latencySamples = [];
        this.uploadChunkSize = 1024 * 1024; // 1MB chunks
        this.activeUploads = new Map();
        
        this.initializeEventListeners();
        this.connectWebSocket();
    }
    
    initializeEventListeners() {
        // File inputs
        document.getElementById('jsonFile').addEventListener('change', (e) => {
            this.jsonFile = e.target.files[0];
            document.getElementById('jsonStatus').textContent = 
                `${this.jsonFile.name} (${this.formatFileSize(this.jsonFile.size)})`;
            this.updateUploadButton();
        });
        
        document.getElementById('mp3File').addEventListener('change', (e) => {
            this.mp3File = e.target.files[0];
            document.getElementById('mp3Status').textContent = this.mp3File.name;
            this.updateUploadButton();
        });
        
        // Upload button
        document.getElementById('uploadBtn').addEventListener('click', () => {
            this.uploadFiles();
        });
        
        // Sync controls
        document.getElementById('syncPlayBtn').addEventListener('click', () => {
            this.startSyncPlayback();
        });
        
        document.getElementById('stopBtn').addEventListener('click', () => {
            this.stopPlayback();
        });
        
        // Calibration
        document.getElementById('syncOffset').addEventListener('input', (e) => {
            this.syncOffset = parseInt(e.target.value);
            document.getElementById('offsetValue').textContent = this.syncOffset;
        });
        
        // Audio player events
        this.audioPlayer.addEventListener('timeupdate', () => {
            if (this.audioPlayer.duration) {
                const progress = (this.audioPlayer.currentTime / this.audioPlayer.duration) * 100;
                document.getElementById('progressFill').style.width = progress + '%';
            }
        });
        
        // Advanced calibration button
        const calibrateBtn = document.getElementById('calibrateBtn');
        if (calibrateBtn) {
            calibrateBtn.addEventListener('click', () => {
                this.performLatencyCalibration();
            });
        }
    }
    
    connectWebSocket() {
        const host = window.location.hostname || 'lightwaveos.local';
        const wsUrl = `ws://${host}:81`;
        
        this.ws = new WebSocket(wsUrl);
        
        this.ws.onopen = () => {
            console.log('WebSocket connected');
            this.updateConnectionStatus(true);
            this.showStatus('Connected to LightwaveOS', 'success');
            
            // Start measuring network latency
            this.measureNetworkLatency();
        };
        
        this.ws.onclose = () => {
            console.log('WebSocket disconnected');
            this.updateConnectionStatus(false);
            this.showStatus('Disconnected from device', 'error');
            setTimeout(() => this.connectWebSocket(), 3000);
        };
        
        this.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            this.showStatus('Connection error', 'error');
        };
        
        this.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                this.handleWebSocketMessage(data);
            } catch (e) {
                console.error('Failed to parse WebSocket message:', e);
            }
        };
    }
    
    handleWebSocketMessage(data) {
        switch (data.type) {
            case 'sync_status':
                this.handleSyncStatus(data);
                break;
                
            case 'upload_progress':
                this.updateUploadProgress(data);
                break;
                
            case 'upload_complete':
                this.handleUploadComplete(data);
                break;
                
            case 'pong':
                this.handlePongResponse(data);
                break;
                
            case 'sync_metrics':
                this.updateSyncMetrics(data);
                break;
                
            case 'drift_correction':
                this.applyDriftCorrection(data);
                break;
                
            case 'beat':
                this.visualizeBeat(data);
                break;
                
            case 'error':
                this.showStatus(data.message, 'error');
                break;
        }
    }
    
    async uploadFiles() {
        if (!this.jsonFile || !this.mp3File) return;
        
        try {
            // Check file size
            const jsonSize = this.jsonFile.size;
            const isLargeFile = jsonSize > 5 * 1024 * 1024; // > 5MB
            
            this.showStatus(`Uploading ${this.formatFileSize(jsonSize)} JSON file...`, 'info');
            
            // Upload JSON using appropriate method
            if (isLargeFile) {
                await this.uploadLargeJSON(this.jsonFile);
            } else {
                await this.uploadSmallJSON(this.jsonFile);
            }
            
            // Load MP3 into audio player
            const mp3URL = URL.createObjectURL(this.mp3File);
            this.audioPlayer.src = mp3URL;
            this.audioPlayer.load();
            
            // Update UI
            document.getElementById('trackName').textContent = this.mp3File.name;
            document.getElementById('syncPlayBtn').disabled = false;
            document.getElementById('stopBtn').disabled = false;
            
            this.showStatus('Files loaded successfully', 'success');
            
        } catch (error) {
            this.showStatus('Upload failed: ' + error.message, 'error');
        }
    }
    
    async uploadSmallJSON(file) {
        const jsonData = await this.readFileAsText(file);
        
        // Validate JSON
        const data = JSON.parse(jsonData);
        const duration = data.global_metrics?.duration_ms || 0;
        
        const host = window.location.hostname || 'lightwaveos.local';
        
        const response = await fetch(`http://${host}/upload/audio_data`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'X-File-Name': file.name,
                'X-Duration-Ms': duration
            },
            body: jsonData
        });
        
        if (!response.ok) {
            throw new Error('JSON upload failed');
        }
        
        // Notify ESP32 to load the file
        this.sendCommand({
            cmd: 'load_audio_data',
            filename: '/audio/' + file.name,
            streaming: false
        });
    }
    
    async uploadLargeJSON(file) {
        const uploadId = this.generateUploadId();
        const chunks = Math.ceil(file.size / this.uploadChunkSize);
        
        this.showStatus(`Uploading large file in ${chunks} chunks...`, 'info');
        
        // Create progress tracker
        const progressTracker = {
            uploadId,
            totalChunks: chunks,
            uploadedChunks: 0,
            startTime: Date.now()
        };
        
        this.activeUploads.set(uploadId, progressTracker);
        
        // Upload chunks
        for (let i = 0; i < chunks; i++) {
            const start = i * this.uploadChunkSize;
            const end = Math.min(start + this.uploadChunkSize, file.size);
            const chunk = file.slice(start, end);
            
            await this.uploadChunk(uploadId, chunk, i, chunks, file.name);
            
            progressTracker.uploadedChunks++;
            this.updateLocalProgress(uploadId);
        }
        
        // Finalize upload
        await this.finalizeChunkedUpload(uploadId, file.name);
    }
    
    async uploadChunk(uploadId, chunk, chunkIndex, totalChunks, filename) {
        const host = window.location.hostname || 'lightwaveos.local';
        const formData = new FormData();
        formData.append('chunk', chunk);
        
        const response = await fetch(`http://${host}/upload/audio_data/chunk`, {
            method: 'POST',
            headers: {
                'X-Upload-ID': uploadId,
                'X-Chunk-Index': chunkIndex,
                'X-Total-Chunks': totalChunks,
                'X-Chunk-Size': this.uploadChunkSize,
                'X-File-Name': filename
            },
            body: formData
        });
        
        if (!response.ok) {
            throw new Error(`Chunk ${chunkIndex} upload failed`);
        }
        
        return response.json();
    }
    
    async finalizeChunkedUpload(uploadId, filename) {
        const host = window.location.hostname || 'lightwaveos.local';
        
        const response = await fetch(`http://${host}/upload/audio_data/chunk`, {
            method: 'POST',
            headers: {
                'X-Upload-ID': uploadId,
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ finalize: true })
        });
        
        if (!response.ok) {
            throw new Error('Failed to finalize upload');
        }
        
        const result = await response.json();
        
        // Clean up tracker
        this.activeUploads.delete(uploadId);
        
        // Notify ESP32 to load with streaming
        this.sendCommand({
            cmd: 'load_audio_data',
            filename: result.filename,
            streaming: true  // Use streaming parser for large files
        });
        
        return result;
    }
    
    updateLocalProgress(uploadId) {
        const tracker = this.activeUploads.get(uploadId);
        if (!tracker) return;
        
        const progress = (tracker.uploadedChunks / tracker.totalChunks) * 100;
        const elapsed = Date.now() - tracker.startTime;
        const speed = (tracker.uploadedChunks * this.uploadChunkSize) / (elapsed / 1000);
        
        document.getElementById('uploadProgressBar').style.width = progress + '%';
        document.getElementById('uploadProgressText').textContent = 
            `${progress.toFixed(1)}% (${this.formatFileSize(speed)}/s)`;
    }
    
    updateUploadProgress(data) {
        const progress = data.progress;
        document.getElementById('uploadProgressBar').style.width = progress + '%';
        document.getElementById('uploadProgressText').textContent = 
            `${progress.toFixed(1)}% (${data.chunks_received}/${data.total_chunks} chunks)`;
    }
    
    async measureNetworkLatency() {
        this.latencySamples = [];
        
        for (let i = 0; i < 10; i++) {
            const sample = await this.measureSingleLatency(i);
            if (sample > 0) {
                this.latencySamples.push(sample);
            }
            await new Promise(resolve => setTimeout(resolve, 100));
        }
        
        // Calculate statistics
        if (this.latencySamples.length > 0) {
            this.networkLatency = this.calculateTrimmedMean(this.latencySamples);
            document.getElementById('latencyDisplay').textContent = 
                `${this.networkLatency.toFixed(1)}ms`;
            
            // Send latency report to device
            this.sendCommand({
                cmd: 'measure_latency',
                type: 'latency_report',
                latency: this.networkLatency
            });
        }
    }
    
    measureSingleLatency(sequence) {
        return new Promise((resolve) => {
            const startTime = performance.now();
            const timeout = setTimeout(() => resolve(-1), 1000);
            
            const handler = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    if (data.type === 'pong' && data.sequence === sequence) {
                        clearTimeout(timeout);
                        const latency = performance.now() - startTime;
                        this.ws.removeEventListener('message', handler);
                        resolve(latency);
                    }
                } catch (e) {}
            };
            
            this.ws.addEventListener('message', handler);
            
            this.sendCommand({
                cmd: 'measure_latency',
                type: 'ping',
                sequence: sequence,
                timestamp: Date.now()
            });
        });
    }
    
    calculateTrimmedMean(samples) {
        if (samples.length === 0) return 0;
        
        // Sort samples
        const sorted = [...samples].sort((a, b) => a - b);
        
        // Remove top and bottom 20%
        const trimCount = Math.floor(sorted.length * 0.2);
        const trimmed = sorted.slice(trimCount, sorted.length - trimCount);
        
        // Calculate mean
        const sum = trimmed.reduce((a, b) => a + b, 0);
        return sum / trimmed.length;
    }
    
    async performLatencyCalibration() {
        this.showStatus('Performing network calibration...', 'info');
        
        // Disable controls during calibration
        document.getElementById('calibrateBtn').disabled = true;
        
        // Measure latency multiple times
        await this.measureNetworkLatency();
        
        // Re-enable controls
        document.getElementById('calibrateBtn').disabled = false;
        
        this.showStatus(`Calibration complete. Latency: ${this.networkLatency.toFixed(1)}ms`, 'success');
    }
    
    async startSyncPlayback() {
        // Pre-buffer audio
        this.audioPlayer.currentTime = 0;
        await this.audioPlayer.play();
        this.audioPlayer.pause();
        
        // Calculate synchronized start time
        const countdown = 3000;
        const targetStartTime = Date.now() + countdown;
        
        // Send sync command with enhanced timing info
        this.sendCommand({
            cmd: 'prepare_sync_play',
            start_time: targetStartTime,
            offset: this.syncOffset,
            network_latency: this.networkLatency,
            client_time: Date.now()
        });
        
        // Show countdown
        this.showCountdown(countdown);
        
        // Schedule audio playback with compensation
        const compensatedDelay = countdown - this.networkLatency / 2 - 50;
        setTimeout(() => {
            this.audioPlayer.play();
            this.isSynced = true;
            this.syncStartTime = Date.now();
            this.updateSyncStatus('Synced', true);
            this.startSyncMonitor();
        }, compensatedDelay);
    }
    
    startSyncMonitor() {
        let driftCorrections = 0;
        
        this.syncInterval = setInterval(() => {
            if (!this.isSynced) {
                clearInterval(this.syncInterval);
                return;
            }
            
            const currentTime = Date.now() - this.syncStartTime;
            const audioTime = this.audioPlayer.currentTime * 1000;
            const drift = Math.abs(currentTime - audioTime);
            
            // Send enhanced sync heartbeat
            this.sendCommand({
                cmd: 'sync_heartbeat',
                time_ms: audioTime,
                drift_ms: drift,
                corrections: driftCorrections
            });
            
            // Update UI
            this.updateTimeDisplay();
            
            // Advanced drift correction
            if (drift > 100 && driftCorrections < 5) {
                this.updateSyncStatus(`Drift: ${drift.toFixed(0)}ms (correcting...)`, false);
                driftCorrections++;
            } else if (drift < 50) {
                this.updateSyncStatus('Synced', true);
                driftCorrections = Math.max(0, driftCorrections - 1);
            }
            
        }, 100);
    }
    
    applyDriftCorrection(data) {
        if (!this.isSynced || !this.audioPlayer) return;
        
        const suggestedRate = data.suggested_rate;
        
        // Apply temporary playback rate adjustment
        this.audioPlayer.playbackRate = suggestedRate;
        
        // Reset after correction period
        setTimeout(() => {
            this.audioPlayer.playbackRate = 1.0;
        }, 1000);
        
        console.log(`Applied drift correction: rate=${suggestedRate}`);
    }
    
    updateSyncMetrics(data) {
        // Update advanced sync metrics display
        const metricsEl = document.getElementById('syncMetrics');
        if (metricsEl) {
            metricsEl.innerHTML = `
                <div>Device Time: ${data.device_time.toFixed(0)}ms</div>
                <div>Client Time: ${data.client_time.toFixed(0)}ms</div>
                <div>Drift: ${data.drift.toFixed(1)}ms</div>
                <div>Network Latency: ${data.network_latency.toFixed(1)}ms</div>
                <div>On Beat: ${data.on_beat ? '🎵' : '-'}</div>
            `;
        }
    }
    
    visualizeBeat(data) {
        // Flash beat indicator
        const beatIndicator = document.getElementById('beatIndicator');
        if (beatIndicator) {
            beatIndicator.classList.add('beat-flash');
            setTimeout(() => {
                beatIndicator.classList.remove('beat-flash');
            }, 100);
        }
        
        // Update beat confidence
        const confidenceEl = document.getElementById('beatConfidence');
        if (confidenceEl) {
            confidenceEl.style.width = (data.confidence * 100) + '%';
        }
    }
    
    // Helper methods
    formatFileSize(bytes) {
        const units = ['B', 'KB', 'MB', 'GB'];
        let size = bytes;
        let unitIndex = 0;
        
        while (size >= 1024 && unitIndex < units.length - 1) {
            size /= 1024;
            unitIndex++;
        }
        
        return `${size.toFixed(1)} ${units[unitIndex]}`;
    }
    
    generateUploadId() {
        return 'upload_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
    }
    
    readFileAsText(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = (e) => resolve(e.target.result);
            reader.onerror = reject;
            reader.readAsText(file);
        });
    }
    
    showCountdown(ms) {
        let remaining = ms / 1000;
        const countdownEl = document.createElement('div');
        countdownEl.className = 'countdown-overlay';
        countdownEl.innerHTML = `
            <div class="countdown-number">${remaining}</div>
            <div class="countdown-text">Starting synchronized playback...</div>
        `;
        document.body.appendChild(countdownEl);
        
        const numberEl = countdownEl.querySelector('.countdown-number');
        
        const interval = setInterval(() => {
            remaining--;
            if (remaining <= 0) {
                clearInterval(interval);
                countdownEl.classList.add('fade-out');
                setTimeout(() => countdownEl.remove(), 500);
            } else {
                numberEl.textContent = remaining;
                if (remaining === 1) {
                    numberEl.classList.add('pulse');
                }
            }
        }, 1000);
    }
    
    stopPlayback() {
        this.audioPlayer.pause();
        this.audioPlayer.currentTime = 0;
        this.isSynced = false;
        
        if (this.syncInterval) {
            clearInterval(this.syncInterval);
            this.syncInterval = null;
        }
        
        this.sendCommand({
            cmd: 'stop_playback'
        });
        
        this.updateSyncStatus('Stopped', false);
        document.getElementById('progressFill').style.width = '0%';
        this.updateTimeDisplay();
    }
    
    sendCommand(data) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(data));
        } else {
            this.showStatus('Not connected to device', 'error');
        }
    }
    
    updateConnectionStatus(connected) {
        const indicator = document.getElementById('connectionIndicator');
        const text = document.getElementById('connectionText');
        
        if (connected) {
            indicator.classList.add('connected');
            text.textContent = 'Connected';
        } else {
            indicator.classList.remove('connected');
            text.textContent = 'Disconnected';
        }
    }
    
    updateSyncStatus(text, synced) {
        document.getElementById('syncText').textContent = text;
        const indicator = document.getElementById('syncIndicator');
        indicator.className = synced ? 'indicator synced' : 'indicator not-synced';
    }
    
    updateTimeDisplay() {
        const current = this.formatTime(this.audioPlayer.currentTime);
        const duration = this.formatTime(this.audioPlayer.duration || 0);
        document.getElementById('trackTime').textContent = `${current} / ${duration}`;
    }
    
    formatTime(seconds) {
        const mins = Math.floor(seconds / 60);
        const secs = Math.floor(seconds % 60);
        return `${mins}:${secs.toString().padStart(2, '0')}`;
    }
    
    showStatus(message, type = 'info') {
        const messagesEl = document.getElementById('statusMessages');
        const messageEl = document.createElement('div');
        messageEl.className = `status-message ${type}`;
        messageEl.textContent = message;
        
        messagesEl.appendChild(messageEl);
        
        // Auto-remove after 5 seconds
        setTimeout(() => {
            messageEl.style.opacity = '0';
            setTimeout(() => messageEl.remove(), 300);
        }, 5000);
        
        // Keep only last 5 messages
        while (messagesEl.children.length > 5) {
            messagesEl.removeChild(messagesEl.firstChild);
        }
    }
}

// Initialize enhanced controller on page load
document.addEventListener('DOMContentLoaded', () => {
    window.audioSync = new AudioSyncController();
});