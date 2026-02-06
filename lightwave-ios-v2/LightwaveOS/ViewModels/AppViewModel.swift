//
//  AppViewModel.swift
//  LightwaveOS
//
//  Root ViewModel managing connection lifecycle and child ViewModels.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import Foundation
import Network
import Observation

/// App mode enum tracking the active tab
enum AppMode: String, Sendable {
    case play
    case zones
    case audio
    case device
}

@MainActor
@Observable
class AppViewModel {
    // MARK: - Connection State

    /// Connection state derived from ConnectionManager for UI observation
    var connectionState: ConnectionState {
        connectionManager.state
    }

    var currentDevice: DeviceInfo?
    var currentIP: String?  // Track IP for AP subnet detection (works for manual connections too)
    var currentMode: AppMode = .play

    // MARK: - LED Stream State

    var ledData: [UInt8] = Array(repeating: 0, count: 960) // 320 LEDs × 3 bytes (RGB)
    var isLEDStreamActive: Bool = false

    // MARK: - Device Discovery State

    var discoveredDevices: [DeviceInfo] = []
    var isDiscoverySearching: Bool = false

    // MARK: - Child ViewModels

    var effects: EffectViewModel
    var palettes: PaletteViewModel
    var parameters: ParametersViewModel
    var zones: ZoneViewModel
    var audio: AudioViewModel
    var transition: TransitionViewModel
    var colourCorrection: ColourCorrectionViewModel
    var deviceStatus: DeviceStatusResponse.DeviceStatus?
    var deviceInfo: DeviceInfoResponse.DeviceInfo?
    var wsConnected: Bool = false

    // MARK: - Network

    private(set) var rest: RESTClient?
    let ws: WebSocketService
    let udpReceiver: UDPStreamReceiver
    let discovery: DeviceDiscoveryService
    let connectionManager: ConnectionManager

    // MARK: - Debug Log

    var debugLog: [DebugEntry] = []
    private let maxLogEntries = 100

    // MARK: - Zone Refresh Throttling

    private var zoneRefreshTask: Task<Void, Never>?
    private var lastZoneRefresh: Date = .distantPast
    private var streamSubscriptionTask: Task<Void, Never>?
    private var udpFallbackActive = false
    private var udpSubscribeStart: Date?
    private let udpFallbackDelaySeconds: TimeInterval = 3.0
    private var udpHealthTask: Task<Void, Never>?
    private var wsEventTask: Task<Void, Never>?
    private var discoveryStreamTask: Task<Void, Never>?

    // MARK: - Initialization

    init() {
        self.effects = EffectViewModel()
        self.palettes = PaletteViewModel()
        self.parameters = ParametersViewModel()
        self.zones = ZoneViewModel()
        self.audio = AudioViewModel()
        self.transition = TransitionViewModel()
        self.colourCorrection = ColourCorrectionViewModel()
        self.ws = WebSocketService()
        self.udpReceiver = UDPStreamReceiver()
        self.discovery = DeviceDiscoveryService()
        self.connectionManager = ConnectionManager(ws: ws, discovery: discovery)

        // Set self as delegate AFTER init completes
        Task { @MainActor in
            self.connectionManager.delegate = self
        }
    }

    // MARK: - Connection API (delegates to ConnectionManager)

    /// Connect to a discovered device
    func connect(to device: DeviceInfo) async {
        log("Connecting to \(device.displayName) at \(device.cleanIP)...", category: "CONN")
        currentDevice = device
        let target = DeviceTarget.from(device)
        await connectionManager.connect(target: target)
    }

    /// Connect manually by IP
    func connectManual(ip: String, port: Int = 80) async {
        log("Manual connection to \(ip):\(port)", category: "CONN")
        let target = DeviceTarget.manual(ip: ip, port: port)
        await connectionManager.connect(target: target)
    }

    /// Disconnect from device
    func disconnect() async {
        log("Disconnecting...", category: "CONN")

        // Cancel local tasks
        wsEventTask?.cancel()
        wsEventTask = nil
        discoveryStreamTask?.cancel()
        discoveryStreamTask = nil
        streamSubscriptionTask?.cancel()
        streamSubscriptionTask = nil
        udpHealthTask?.cancel()
        udpHealthTask = nil
        zoneRefreshTask?.cancel()
        zoneRefreshTask = nil

        await connectionManager.disconnect()
    }

    // MARK: - App Lifecycle

    /// Handle app entering background
    func handleBackground() async {
        await pauseStreaming()
        await connectionManager.handleBackground()
    }

    /// Handle app returning to foreground
    func handleForeground() async {
        await connectionManager.handleForeground()
        await resumeStreaming()
    }

    // MARK: - Device Discovery

    /// Start device discovery and consume updates.
    /// Updates discoveredDevices and isDiscoverySearching from the actor stream.
    func startDeviceDiscovery() async {
        // Cancel old task BEFORE creating new stream to avoid orphan continuations
        discoveryStreamTask?.cancel()
        discoveryStreamTask = nil

        let stream = await discovery.startDiscovery()

        discoveryStreamTask = Task { @MainActor [weak self] in
            for await devices in stream {
                guard let self = self else { break }
                self.discoveredDevices = devices
                self.isDiscoverySearching = await self.discovery.isSearching
            }
        }
    }

    /// Stop device discovery.
    func stopDeviceDiscovery() async {
        await discovery.stopDiscovery()
        discoveredDevices = []
        isDiscoverySearching = false
    }

    // MARK: - Reconnection

    /// Attempt to reconnect: probe last-known IP first, then fall back to discovery.
    func attemptReconnection() {
        // Guard: don't start reconnection if already connected or connecting
        guard !connectionState.isConnecting && !connectionState.isReady else {
            log("Skipping reconnection — already \(connectionState)", category: "CONN")
            return
        }

        Task { [weak self] in
            guard let self else { return }

            // First: probe last-known device IP
            if let last = DeviceTarget.loadPersisted() {
                self.log("Probing last device at \(last.lastKnownIP):\(last.port)...", category: "CONN")
                if let device = await self.discovery.probeDevice(ip: last.lastKnownIP, port: last.port) {
                    guard !Task.isCancelled else { return }
                    await self.connect(to: device)
                    return
                }
            }

            guard !Task.isCancelled else { return }

            // Probe v2 AP gateway directly
            self.log("Probing v2 AP gateway...", category: "CONN")
            if let device = await self.discovery.probeDevice(ip: "192.168.4.1") {
                guard !Task.isCancelled else { return }
                await self.connect(to: device)
                return
            }

            guard !Task.isCancelled else { return }

            // Last resort: full Bonjour discovery
            self.log("Falling back to Bonjour discovery...", category: "CONN")
            await self.startDeviceDiscovery()
        }
    }

    // MARK: - WebSocket Event Stream

    private func consumeWebSocketEvents(_ stream: AsyncStream<WebSocketService.Event>) {
        wsEventTask?.cancel()
        wsEventTask = Task { @MainActor [weak self] in
            for await event in stream {
                guard let self = self else { break }
                switch event {
                case .status(let payload):
                    // Update current effect
                    if let effectId = (payload.data["effectId"] as? Int)
                        ?? (payload.data["currentEffect"] as? Int) {
                        self.effects.currentEffectId = effectId
                        if let effect = self.effects.allEffects.first(where: { $0.id == effectId }) {
                            self.effects.currentEffectName = effect.name
                        }
                    }

                    // Update current palette
                    if let paletteId = (payload.data["paletteId"] as? Int)
                        ?? (payload.data["currentPalette"] as? Int) {
                        self.palettes.currentPaletteId = paletteId
                    }

                    // Update parameters
                    self.parameters.updateFromStatus(payload.data)

                    // Update audio
                    if let bpm = payload.data["bpm"] as? Double,
                       let confidence = payload.data["bpmConfidence"] as? Double {
                        self.audio.updateFromStatus(bpm: bpm, confidence: confidence)
                    }

                    self.log("Status update received", category: "WS")

                case .beat(let payload):
                    self.audio.handleBeatEvent(payload.data)

                case .audioMetrics(let frame):
                    self.audio.handleMetricsFrame(frame)

                case .zoneUpdate(let payload):
                    self.zones.handleZoneUpdate(payload.data)
                    self.log("Zone update received", category: "WS")
                    self.throttledZoneRefresh()

                case .parameterUpdate(let payload):
                    self.parameters.updateFromStatus(payload.data)
                    self.log("Parameter update received", category: "WS")

                case .ledData(let data):
                    guard data.count == 960 else {
                        self.log("Invalid LED data: \(data.count) bytes", category: "WS")
                        return
                    }
                    self.ledData = [UInt8](data)
                    if !self.isLEDStreamActive {
                        self.isLEDStreamActive = true
                        self.log("LED stream started", category: "WS")
                    }

                case .connected:
                    self.log("WebSocket connected", category: "WS")
                    self.wsConnected = true
                    self.udpFallbackActive = false
                    self.udpSubscribeStart = nil
                    self.startStreamSubscriptions()
                    self.startUdpHealthMonitor()

                case .disconnected(let error):
                    self.log("WebSocket disconnected: \(error?.localizedDescription ?? "clean")", category: "WS")
                    // NOTE: wsConnected is a computed property from connectionState, no assignment needed
                    self.streamSubscriptionTask?.cancel()
                    self.streamSubscriptionTask = nil
                    self.udpHealthTask?.cancel()
                    self.udpHealthTask = nil
                    self.isLEDStreamActive = false
                    self.audio.audioFrameCount = 0
                    self.udpReceiver.reset()
                    self.udpFallbackActive = false
                    self.udpSubscribeStart = nil

                    // Notify ConnectionManager to trigger reconnection
                    Task {
                        await self.connectionManager.notifyWebSocketDisconnected()
                    }
                }
            }
        }
    }

    // MARK: - UDP Health Monitor

    /// If UDP dies mid-session (common under weak WiFi), fall back to WS streaming without spamming re-subscribes.
    private func startUdpHealthMonitor() {
        udpHealthTask?.cancel()
        udpHealthTask = Task { @MainActor [weak self] in
            guard let self else { return }

            var lastLed = self.udpReceiver.ledFrameCount
            var lastAudio = self.udpReceiver.audioFrameCount
            var lastProgress = Date()

            while !Task.isCancelled && self.wsConnected {
                try? await Task.sleep(for: .seconds(1))
                guard !Task.isCancelled else { break }

                if self.udpFallbackActive {
                    continue
                }

                let ledNow = self.udpReceiver.ledFrameCount
                let audioNow = self.udpReceiver.audioFrameCount

                if ledNow != lastLed || audioNow != lastAudio {
                    lastLed = ledNow
                    lastAudio = audioNow
                    lastProgress = Date()
                    continue
                }

                // No progress for a while -> fallback.
                if Date().timeIntervalSince(lastProgress) >= self.udpFallbackDelaySeconds {
                    self.udpFallbackActive = true
                    await self.ws.subscribeLEDStreamWS()
                    await self.ws.subscribeAudioStreamWS()
                    self.log("UDP stalled mid-session, falling back to WS streaming", category: "UDP")
                }
            }
        }
    }

    // MARK: - Background/Foreground Streaming

    private var wasStreamingBeforeBackground = false

    /// Pause streaming when app enters background to save battery.
    func pauseStreaming() async {
        guard connectionState.isReady else { return }
        wasStreamingBeforeBackground = isLEDStreamActive || audio.audioFrameCount > 0

        if wasStreamingBeforeBackground {
            log("Pausing streaming (app backgrounded)", category: "BG")
            await ws.unsubscribeLEDStream()
            await ws.unsubscribeAudioStream()
            streamSubscriptionTask?.cancel()
            streamSubscriptionTask = nil
            udpHealthTask?.cancel()
            udpHealthTask = nil
        }
    }

    /// Resume streaming when app returns to foreground.
    func resumeStreaming() async {
        guard connectionState.isReady, wasStreamingBeforeBackground else { return }
        log("Resuming streaming (app foregrounded)", category: "BG")
        wasStreamingBeforeBackground = false
        startStreamSubscriptions()
        startUdpHealthMonitor()
    }

    // MARK: - Stream Subscription Retry

    private func startStreamSubscriptions() {
        streamSubscriptionTask?.cancel()
        streamSubscriptionTask = Task { @MainActor [weak self] in
            guard let self else { return }

            // On AP subnet, skip UDP — soft AP UDP routing is unreliable.
            // Go straight to WS streaming to avoid the 3s dead-time and
            // potential double-subscription (UDP + WS fallback simultaneously).
            let isAPSubnet = self.currentIP == "192.168.4.1"
            if isAPSubnet {
                self.log("On AP subnet — using WS streaming (UDP unreliable over soft AP)", category: "UDP")
                self.udpFallbackActive = true
                await self.ws.subscribeLEDStreamWS()
                await self.ws.subscribeAudioStreamWS()
                return
            }

            var attempt = 0
            let maxAttempts = 8
            let udpReady = await self.udpReceiver.waitUntilReady(timeout: 2.0)

            if !udpReady {
                self.log("UDP listener not ready, falling back to WS streaming", category: "UDP")
                self.udpFallbackActive = true
                await self.ws.subscribeLEDStreamWS()
                await self.ws.subscribeAudioStreamWS()
                return
            }

            while !Task.isCancelled && attempt < maxAttempts {
                if self.udpFallbackActive {
                    break
                }

                let udpLedActive = self.udpReceiver.ledFrameCount > 0
                let udpAudioActive = self.udpReceiver.audioFrameCount > 0

                // Always subscribe on first attempt — firmware is idempotent for re-subscribes
                let needsAudio = attempt == 0 || !udpAudioActive
                let needsLED = attempt == 0 || !udpLedActive

                if !needsAudio && !needsLED {
                    break
                }

                attempt += 1

                if needsLED {
                    await self.ws.subscribeLEDStream()
                }
                if needsAudio {
                    await self.ws.subscribeAudioStream()
                }

                if self.udpSubscribeStart == nil {
                    self.udpSubscribeStart = Date()
                }

                self.log("UDP stream subscribe attempt \(attempt)", category: "UDP")

                if let startedAt = self.udpSubscribeStart,
                   Date().timeIntervalSince(startedAt) >= self.udpFallbackDelaySeconds {
                    let ledIdle = self.udpReceiver.ledFrameCount == 0
                    let audioIdle = self.udpReceiver.audioFrameCount == 0
                    if ledIdle || audioIdle {
                        self.udpFallbackActive = true
                        if ledIdle {
                            await self.ws.subscribeLEDStreamWS()
                        }
                        if audioIdle {
                            await self.ws.subscribeAudioStreamWS()
                        }
                        self.log("UDP idle after \(Int(self.udpFallbackDelaySeconds))s, falling back to WS streaming", category: "UDP")
                        break
                    }
                }

                try? await Task.sleep(for: .seconds(1))
            }

            if !Task.isCancelled && !self.udpFallbackActive {
                if self.udpReceiver.audioFrameCount == 0 {
                    self.log("UDP audio stream still idle after subscribe attempts", category: "UDP")
                }
                if self.udpReceiver.ledFrameCount == 0 {
                    self.log("UDP LED stream still idle after subscribe attempts", category: "UDP")
                }
            }
        }
    }

    // MARK: - Zone Refresh Throttling

    /// Throttle zone REST refreshes to at most once every 2 seconds.
    /// Applies delta from WS immediately; defers full REST reload.
    private func throttledZoneRefresh() {
        guard Date().timeIntervalSince(lastZoneRefresh) > 2.0 else { return }
        zoneRefreshTask?.cancel()
        zoneRefreshTask = Task { [weak self] in
            guard let self = self else { return }
            try? await Task.sleep(for: .seconds(0.5))
            guard !Task.isCancelled else { return }
            await self.zones.loadZones()
            self.lastZoneRefresh = Date()
        }
    }

    // MARK: - Helper for Initial State Loading

    private func loadInitialState() async {
        // Start UDP listener
        udpReceiver.start()
        wireUDPHandlers()

        // NOTE: WebSocket events are consumed in connectionDidBecomeReady()
        // DO NOT call ws.connect() here - that creates a SECOND connection!

        // Load initial data
        log("Loading effects list...", category: "INIT")
        await effects.loadEffects()

        log("Loading palettes list...", category: "INIT")
        await palettes.loadPalettes()

        log("Loading parameters...", category: "INIT")
        if let paletteId = await parameters.loadParameters() {
            palettes.currentPaletteId = paletteId
        }

        log("Loading zones...", category: "INIT")
        await zones.loadZones()

        log("Loading colour correction...", category: "INIT")
        await colourCorrection.loadConfig()

        log("Loading audio tuning...", category: "INIT")
        await audio.loadAudioTuning()

        // Start streaming
        startStreamSubscriptions()
        startUdpHealthMonitor()

        log("Connected successfully", category: "CONN")
    }

    private func wireUDPHandlers() {
        udpReceiver.onLedFrame = { [weak self] bytes in
            guard let self = self else { return }
            guard bytes.count >= 966 else { return }
            var ledData = [UInt8](repeating: 0, count: 960)
            for i in 0..<480 {
                ledData[i] = bytes[5 + i]
            }
            for i in 0..<480 {
                ledData[480 + i] = bytes[486 + i]
            }
            self.ledData = ledData
            if !self.isLEDStreamActive {
                self.isLEDStreamActive = true
                self.log("LED stream started (UDP)", category: "UDP")
            }
        }

        udpReceiver.onAudioFrame = { [weak self] data in
            guard let self = self else { return }
            if let frame = AudioMetricsFrame(data: data) {
                self.audio.handleMetricsFrame(frame)
            }
        }
    }

    // MARK: - Debug Logging

    func log(_ message: String, category: String = "APP") {
        let entry = DebugEntry(
            timestamp: Date(),
            category: category,
            message: message
        )

        debugLog.append(entry)

        // Trim old entries
        if debugLog.count > maxLogEntries {
            debugLog.removeFirst(debugLog.count - maxLogEntries)
        }

        // Also print to console in debug builds
        #if DEBUG
        print("[\(category)] \(message)")
        #endif
    }

    func clearLog() {
        debugLog.removeAll()
    }
}

// MARK: - ConnectionManagerDelegate

extension AppViewModel: ConnectionManagerDelegate {
    func connectionDidBecomeReady(rest: RESTClient, ws: WebSocketService, eventStream: AsyncStream<WebSocketService.Event>) {
        // Inject REST client into child ViewModels
        self.rest = rest
        effects.restClient = rest
        palettes.restClient = rest
        parameters.restClient = rest
        zones.restClient = rest
        zones.ws = ws
        audio.restClient = rest
        transition.restClient = rest
        colourCorrection.restClient = rest

        // Consume WebSocket events - we are the ONLY consumer
        consumeWebSocketEvents(eventStream)

        // Load initial state (no longer calls ws.connect())
        Task {
            await loadInitialState()
        }
    }

    func connectionDidDisconnect() {
        // Clean up child ViewModels
        rest = nil
        effects.restClient = nil
        palettes.restClient = nil
        parameters.restClient = nil
        zones.restClient = nil
        audio.restClient = nil
        transition.restClient = nil
        colourCorrection.restClient = nil

        parameters.disconnect()
        zones.disconnect()
        colourCorrection.disconnect()
        audio.reset()

        // Reset local state
        currentDevice = nil
        currentIP = nil
        deviceStatus = nil
        deviceInfo = nil
        wsConnected = false
        ledData = Array(repeating: 0, count: 960)
        isLEDStreamActive = false
        udpFallbackActive = false
        udpSubscribeStart = nil

        // Reset effect and palette state
        effects.currentEffectId = 0
        effects.currentEffectName = ""
        palettes.currentPaletteId = 0

        // Stop UDP
        udpReceiver.stop()
        udpReceiver.onLedFrame = nil
        udpReceiver.onAudioFrame = nil
    }

    func connectionStateDidChange(_ state: ConnectionState) {
        // Update derived properties
        if state.isReady {
            wsConnected = true
            if let target = connectionManager.target {
                currentIP = target.lastKnownIP
            }
        } else {
            wsConnected = false
        }

        // Log state change
        log("Connection state: \(state)", category: "CONN")
    }
}

// MARK: - Debug Entry

struct DebugEntry: Identifiable, Sendable {
    let id = UUID()
    let timestamp: Date
    let category: String
    let message: String
}
