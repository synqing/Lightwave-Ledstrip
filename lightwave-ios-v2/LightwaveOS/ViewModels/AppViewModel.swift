//
//  AppViewModel.swift
//  LightwaveOS
//
//  Root ViewModel managing connection lifecycle and child ViewModels.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import Foundation
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

    enum ConnectionState: Equatable {
        case disconnected
        case discovering
        case connecting
        case connected
        case error(String)
    }

    var connectionState: ConnectionState = .disconnected
    var currentDevice: DeviceInfo?
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
    var edgeMixer: EdgeMixerViewModel
    var deviceStatus: DeviceStatusResponse.DeviceStatus?
    var deviceInfo: DeviceInfoResponse.DeviceInfo?
    var wsConnected: Bool = false

    /// Phase 1 — capability discovery. Populated after a successful connect via
    /// `RESTClient.getCapabilities()`. `nil` indicates either pre-connect or that
    /// capability discovery degraded gracefully (firmware did not expose either probe).
    var capabilities: DeviceCapabilities?

    // MARK: - Network

    private(set) var rest: RESTClient?
    let ws: WebSocketService
    let udpReceiver: UDPStreamReceiver
    let discovery: DeviceDiscoveryService

    // MARK: - Debug Log

    var debugLog: [DebugEntry] = []
    private let maxLogEntries = 100

    // MARK: - Zone Refresh Throttling

    private var zoneRefreshTask: Task<Void, Never>?
    private var lastZoneRefresh: Date = .distantPast
    private var statusPollTask: Task<Void, Never>?
    private var streamSubscriptionTask: Task<Void, Never>?
    private var udpFallbackActive = false
    private var udpSubscribeStart: Date?
    private let udpFallbackDelaySeconds: TimeInterval = 3.0
    private var udpHealthTask: Task<Void, Never>?

    /// Timestamp recorded when the app enters the background, used to detect stale connections on foreground.
    private var lastBackgroundedAt: Date?

    // MARK: - Initialization

    init() {
        self.effects = EffectViewModel()
        self.palettes = PaletteViewModel()
        self.parameters = ParametersViewModel()
        self.zones = ZoneViewModel()
        self.audio = AudioViewModel()
        self.transition = TransitionViewModel()
        self.colourCorrection = ColourCorrectionViewModel()
        self.edgeMixer = EdgeMixerViewModel()
        self.ws = WebSocketService()
        self.udpReceiver = UDPStreamReceiver()
        self.discovery = DeviceDiscoveryService()
    }

    // MARK: - Connection Management

    func connect(to device: DeviceInfo) async {
        log("Connecting to \(device.displayName) at \(device.cleanIP)...", category: "CONN")
        connectionState = .connecting
        currentDevice = device

        await connectManual(ip: device.cleanIP, port: device.port)
    }

    func connectManual(ip: String, port: Int = 80) async {
        // Strip interface scope suffix if present
        let cleanIP = ip.components(separatedBy: "%").first ?? ip
        connectionState = .connecting
        log("Manual connection to \(cleanIP):\(port)", category: "CONN")

        // Start UDP listener before subscribing (firmware will send to our port)
        udpReceiver.start()

        // Wire UDP frame handlers
        // Phase 3 disabled — heap-stability mitigation. WS subscribe calls are
        // suppressed in WebSocketService, so the firmware never sends UDP
        // frames. Closures are still registered but bodies are no-op'd
        // defensively in case a stale broadcast arrives during a reconnect
        // race. Re-enable by uncommenting the parse blocks below once the
        // broadcaster fix lands.
        // See docs/superpowers/ios-firmware-parity-phase-3-scoping.md.
        udpReceiver.onLedFrame = { [weak self] bytes in
            guard self != nil else { return }
            _ = bytes
            // guard bytes.count >= 966 else { return }
            // var ledData = [UInt8](repeating: 0, count: 960)
            // for i in 0..<480 { ledData[i] = bytes[5 + i] }
            // for i in 0..<480 { ledData[480 + i] = bytes[486 + i] }
            // self.ledData = ledData
            // if !self.isLEDStreamActive {
            //     self.isLEDStreamActive = true
            //     self.log("LED stream started (UDP)", category: "UDP")
            // }
        }

        udpReceiver.onAudioFrame = { [weak self] data in
            guard self != nil else { return }
            _ = data
            // if let frame = AudioMetricsFrame(data: data) {
            //     self.audio.handleMetricsFrame(frame)
            // }
        }

        // Create REST client
        let client = RESTClient(host: cleanIP, port: port)
        self.rest = client

        do {
            // Test connectivity with device info endpoint
            log("Testing connectivity...", category: "CONN")
            let response = try await client.getDeviceInfo()

            guard response.success else {
                throw NSError(domain: "LightwaveOS", code: -1, userInfo: [NSLocalizedDescriptionKey: "Device returned failure response"])
            }

            log("Device info received: \(response.data.firmware)", category: "CONN")
            self.deviceInfo = response.data

            // Connect WebSocket
            log("Connecting WebSocket...", category: "WS")
            guard let wsURL = URL(string: "ws://\(cleanIP):\(port)/ws") else {
                throw NSError(domain: "LightwaveOS", code: -2, userInfo: [NSLocalizedDescriptionKey: "Invalid WebSocket URL"])
            }
            let stream = await ws.connect(to: wsURL)
            consumeWebSocketEvents(stream)

            // Inject REST client into child ViewModels
            effects.restClient = client
            palettes.restClient = client
            parameters.restClient = client
            zones.restClient = client
            zones.ws = ws
            audio.restClient = client
            transition.restClient = client
            colourCorrection.restClient = client
            edgeMixer.ws = ws

            // Load initial state.
            //
            // The iOS app is the user's interface to K1 — every effect, every
            // palette must be available the moment the user opens a picker.
            // Firmware-side streaming responses (β — `firmware/heap-stability-day1`
            // commit 56110887) handle the heap impact of large list responses
            // by streaming the JSON to the TCP buffer instead of materialising
            // it in a single contiguous String. iOS therefore fetches the full
            // catalogue on connect; the firmware-side fix is what enables this
            // to be heap-safe.
            log("Loading effects list...", category: "INIT")
            await effects.loadEffects()

            log("Loading palettes list...", category: "INIT")
            await palettes.loadPalettes()

            log("Loading parameters...", category: "INIT")
            await parameters.loadParameters()

            log("Loading zones...", category: "INIT")
            await zones.loadZones()

            log("Loading colour correction...", category: "INIT")
            await colourCorrection.loadConfig()

            log("Loading audio tuning...", category: "INIT")
            await audio.loadAudioTuning()

            log("Loading edge mixer config...", category: "INIT")
            edgeMixer.loadConfig()

            startDeviceStatusPolling()

            // Connection successful
            connectionState = .connected
            log("Connected successfully", category: "CONN")

            // MARK: Phase 1 — capability discovery
            // Fire capability discovery off the connect path. This is best-effort:
            // RESTClient.getCapabilities() never throws, so on firmware builds that
            // do not expose openapi.json or firmware/version we silently degrade.
            startCapabilityDiscovery(client: client)

        } catch {
            log("Connection failed: \(error.localizedDescription)", category: "ERROR")
            connectionState = .error(error.localizedDescription)
            rest = nil
        }
    }

    // MARK: Phase 1 — capability discovery

    /// Probe the connected device for advertised capabilities and store the result.
    /// Fully best-effort: a `nil` result is logged as a single-line degradation
    /// notice and does not affect the connection state. Uses `[weak self]` to
    /// avoid retain cycles per the iOS hard constraints.
    private func startCapabilityDiscovery(client: RESTClient) {
        Task { [weak self] in
            let result = await client.getCapabilities()
            await MainActor.run { [weak self] in
                guard let self else { return }
                self.capabilities = result
                if let result {
                    let version = result.firmwareVersion ?? "unknown"
                    let pathCount = result.availablePaths?.count ?? 0
                    self.log(
                        "Capability discovery: firmware=\(version), advertised paths=\(pathCount)",
                        category: "CAPS"
                    )
                } else {
                    self.log("Capability discovery degraded — proceeding without", category: "CAPS")
                }
            }
        }
    }

    func disconnect() async {
        log("Disconnecting...", category: "CONN")
        await ws.disconnect()
        rest = nil
        currentDevice = nil
        connectionState = .disconnected
        statusPollTask?.cancel()
        statusPollTask = nil
        streamSubscriptionTask?.cancel()
        streamSubscriptionTask = nil
        udpReceiver.stop()
        deviceStatus = nil
        deviceInfo = nil
        wsConnected = false

        // Reset state
        effects.currentEffectId = 0
        effects.currentEffectName = ""
        palettes.currentPaletteId = 0
        audio.reset()
        edgeMixer.reset()
        ledData = Array(repeating: 0, count: 960)
        isLEDStreamActive = false
    }

    // MARK: - Scene Phase Lifecycle

    /// Pauses background-wasteful polling when the app is backgrounded.
    /// Does not tear down the WebSocket — it handles its own reconnection.
    func handleBackgrounding() {
        statusPollTask?.cancel()
        statusPollTask = nil
        udpHealthTask?.cancel()
        udpHealthTask = nil
        lastBackgroundedAt = Date()
        log("Backgrounded — paused polling", category: "LIFECYCLE")
        print("[AppViewModel] Backgrounded — paused polling")
    }

    /// Resumes polling when the app returns to the foreground.
    /// If the connection has gone stale (backgrounded >30s and WebSocket dropped),
    /// triggers a full reconnect to the last-known device.
    func handleForegrounding() async {
        guard connectionState == .connected else { return }

        // Detect stale connections after extended background periods
        if let backgroundedAt = lastBackgroundedAt,
           Date().timeIntervalSince(backgroundedAt) > 30 {
            let wsUp = await ws.isConnected
            if !wsUp, let device = currentDevice {
                log("Stale connection detected after background — reconnecting", category: "LIFECYCLE")
                print("[AppViewModel] Foregrounded — stale connection, reconnecting")
                await connect(to: device)
                return
            }
        }

        // Connection still healthy — just restart the polling tasks
        startDeviceStatusPolling()
        startUdpHealthMonitor()
        log("Foregrounded — resumed polling", category: "LIFECYCLE")
        print("[AppViewModel] Foregrounded — resumed polling")
    }

    // MARK: - Device Discovery

    /// Start device discovery and consume updates.
    /// Updates discoveredDevices and isDiscoverySearching from the actor stream.
    func startDeviceDiscovery() async {
        let stream = await discovery.startDiscovery()

        Task { @MainActor [weak self] in
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

    // MARK: - WebSocket Event Stream

    private func consumeWebSocketEvents(_ stream: AsyncStream<WebSocketService.Event>) {
        Task { @MainActor [weak self] in
            for await event in stream {
                guard let self = self else { break }
                switch event {
                case .status(let payload):
                    // Update current effect
                    if let effectId = payload.data["currentEffect"] as? Int {
                        self.effects.currentEffectId = effectId
                        if let effect = self.effects.allEffects.first(where: { $0.id == effectId }) {
                            self.effects.currentEffectName = effect.name
                        }
                    }

                    // Update current palette
                    if let paletteId = payload.data["currentPalette"] as? Int {
                        self.palettes.currentPaletteId = paletteId
                    }

                    // Update parameters
                    self.parameters.updateFromStatus(payload.data)

                    // Update audio
                    if let bpm = payload.data["bpm"] as? Double,
                       let confidence = payload.data["bpmConfidence"] as? Double {
                        self.audio.updateFromStatus(bpm: bpm, confidence: confidence)
                    }

                    // Update EdgeMixer from status broadcast
                    self.edgeMixer.updateFromStatus(payload.data)

                    // Refresh device-telemetry fields from the WS status push.
                    // Firmware emits fps, freeHeap, uptime, cpuPercent,
                    // framesRendered, wsClients on every periodic status
                    // broadcast (5 s cadence). Without this binding the
                    // DeviceTab telemetry is artificially stuck on the 30 s
                    // REST poll. We rebuild DeviceStatus from the WS payload,
                    // falling back to the existing value for fields the
                    // broadcast does not carry (network info, heapSize,
                    // cpuFreq) so the REST safety-net stays intact.
                    let prior = self.deviceStatus
                    let uptime = (payload.data["uptime"] as? Int)
                        ?? (payload.data["uptime"] as? Double).map { Int($0) }
                        ?? prior?.uptime
                        ?? 0
                    let freeHeap = (payload.data["freeHeap"] as? Int)
                        ?? (payload.data["freeHeap"] as? Double).map { Int($0) }
                        ?? prior?.freeHeap
                        ?? 0
                    let fps: Float? = (payload.data["fps"] as? Double).map { Float($0) }
                        ?? (payload.data["fps"] as? Int).map { Float($0) }
                        ?? prior?.fps
                    let cpuPercent: Float? = (payload.data["cpuPercent"] as? Double).map { Float($0) }
                        ?? (payload.data["cpuPercent"] as? Int).map { Float($0) }
                        ?? prior?.cpuPercent
                    let framesRendered = (payload.data["framesRendered"] as? Int)
                        ?? (payload.data["framesRendered"] as? Double).map { Int($0) }
                        ?? prior?.framesRendered
                    let wsClients = (payload.data["wsClients"] as? Int)
                        ?? (payload.data["wsClients"] as? Double).map { Int($0) }
                        ?? prior?.wsClients
                    self.deviceStatus = DeviceStatusResponse.DeviceStatus(
                        uptime: uptime,
                        freeHeap: freeHeap,
                        heapSize: prior?.heapSize,
                        cpuFreq: prior?.cpuFreq,
                        fps: fps,
                        cpuPercent: cpuPercent,
                        framesRendered: framesRendered,
                        network: prior?.network,
                        wsClients: wsClients
                    )

                    self.log("Status update received", category: "WS")

                case .edgeMixerUpdate(let payload):
                    self.edgeMixer.handleResponse(payload.data)

                case .beat(let payload):
                    self.audio.handleBeatEvent(payload.data)

                case .audioMetrics:
                    // Phase 3 disabled — heap-stability mitigation. Stream
                    // subscription is suppressed upstream in WebSocketService
                    // so firmware should not emit; defensive no-op covers any
                    // stale broadcast during reconnect.
                    break

                case .zoneUpdate(let payload):
                    self.zones.handleZoneUpdate(payload.data)
                    self.log("Zone update received", category: "WS")
                    self.throttledZoneRefresh()

                case .parameterUpdate(let payload):
                    self.parameters.updateFromStatus(payload.data)
                    self.log("Parameter update received", category: "WS")

                case .ledData:
                    // Phase 3 disabled — see .audioMetrics above. Defensive no-op.
                    break

                case .connected:
                    self.log("WebSocket connected", category: "WS")
                    self.wsConnected = true
                    self.udpFallbackActive = false
                    self.udpSubscribeStart = nil
                    // Heap-stability mitigation: K1 V2 gates the periodic
                    // 5-second `status` broadcast behind `status.subscribe`.
                    // Without this send, the iOS app receives no `.status`
                    // events from the firmware. See WebSocketService.swift
                    // `subscribeStatus()` for the wire-level details.
                    Task { [weak self] in
                        await self?.ws.subscribeStatus()
                    }
                    self.startStreamSubscriptions()
                    self.startUdpHealthMonitor()

                case .disconnected(let error):
                    self.log("WebSocket disconnected: \(error?.localizedDescription ?? "clean")", category: "WS")
                    self.wsConnected = false
                    self.streamSubscriptionTask?.cancel()
                    self.streamSubscriptionTask = nil
                    self.udpHealthTask?.cancel()
                    self.udpHealthTask = nil
                    self.isLEDStreamActive = false
                    self.audio.audioFrameCount = 0
                    self.udpReceiver.reset()
                    self.udpFallbackActive = false
                    self.udpSubscribeStart = nil

                // MARK: Phase 1 — broadcast handlers
                // Stubbed for Phase 1: the cases exist so Swift's exhaustiveness check
                // catches future drift. Phase 2 will wire each broadcast to the
                // appropriate child ViewModel update path.
                case .cameraModeChanged:
                    self.log("Phase 1: received cameraMode.changed — handler stubbed", category: "WS")

                case .factoryPresetsChanged:
                    self.log("Phase 1: received factoryPresets.changed — handler stubbed", category: "WS")

                case .effectPresetsSaved:
                    self.log("Phase 1: received effectPresets.saved — handler stubbed", category: "WS")

                case .effectPresetsDeleted:
                    self.log("Phase 1: received effectPresets.deleted — handler stubbed", category: "WS")

                case .colourCorrectionSetGamma:
                    self.log("Phase 1: received colorCorrection.setGamma — handler stubbed", category: "WS")

                case .colourCorrectionSetAutoExposure:
                    self.log("Phase 1: received colorCorrection.setAutoExposure — handler stubbed", category: "WS")

                case .colourCorrectionSetBrownGuardrail:
                    self.log("Phase 1: received colorCorrection.setBrownGuardrail — handler stubbed", category: "WS")

                // MARK: Phase 2 — STM and VRMS streams
                // Added 2026-05-01 (Task P2-2). Required only because Swift's
                // exhaustiveness check refuses to compile a partial Event
                // switch — wiring is to AudioViewModel handlers, no new
                // ViewModel needed.
                case .stmFrame(let frame):
                    self.audio.handleSTMFrame(frame)

                case .vrmsFrame(let frame, let timestamp):
                    self.audio.handleVRMSFrame(frame, timestamp: timestamp)

                case .stmSubscriptionAck(let payload):
                    let ok = (payload.data["status"] as? String == "ok") || (payload.data["success"] as? Bool == true)
                    self.log("STM subscription ack received (ok=\(ok))", category: "WS")

                case .vrmsSubscriptionAck(let payload):
                    let ok = (payload.data["status"] as? String == "ok") || (payload.data["success"] as? Bool == true)
                    self.log("VRMS subscription ack received (ok=\(ok))", category: "WS")

                // MARK: Phase 2 — show transport
                // P2-4 introduced two new event cases (showStatus, showAck).
                // ShowsView holds its own ShowViewModel and consumes these
                // events directly via WebSocketService when foregrounded;
                // these stubs exist to preserve the exhaustive switch and
                // log incoming traffic for debugging.
                case .showStatus:
                    self.log("Phase 2: received show.status — forwarded to ShowsView", category: "WS")

                case .showAck:
                    self.log("Phase 2: received show.* ack — forwarded to ShowsView", category: "WS")
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

    // MARK: - Stream Subscription Retry

    private func startStreamSubscriptions() {
        streamSubscriptionTask?.cancel()
        streamSubscriptionTask = Task { @MainActor [weak self] in
            guard let self else { return }
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

    // MARK: - Device Status Polling
    //
    // Heap-stability mitigation: K1 V2 now gates its periodic 5-second `status`
    // WebSocket broadcast behind `status.subscribe` (see firmware
    // streaming-and-status-gate). iOS subscribes on WS connect, so the
    // real-time portion of the status (effect/palette/parameters/audio/edge
    // mixer) is delivered push-based.
    //
    // The REST `getDeviceStatus()` call returns fields that the WS status
    // handler does NOT consume in `consumeWebSocketEvents` — fps, freeHeap,
    // heapSize, framesRendered, network.rssi, network.connected, uptime — used
    // by `DeviceTab` and `PersistentStatusBar` for telemetry. Rather than
    // remove polling entirely (option a) and lose that telemetry, we drop
    // the cadence to 30s as a safety-net (option b). Net effect: ~93%
    // reduction in status REST traffic (was every 2s, now every 30s) while
    // preserving every UI surface.
    private static let statusPollInterval: Duration = .seconds(30)

    private func startDeviceStatusPolling() {
        statusPollTask?.cancel()
        statusPollTask = Task { [weak self] in
            guard let self else { return }
            while !Task.isCancelled {
                await self.loadDeviceStatus()
                try? await Task.sleep(for: Self.statusPollInterval)
            }
        }
    }

    private func loadDeviceStatus() async {
        guard let client = rest else { return }
        do {
            let response = try await client.getDeviceStatus()
            self.deviceStatus = response.data
        } catch {
            // Keep last known status; avoid spamming logs
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

// MARK: - Debug Entry

struct DebugEntry: Identifiable, Sendable {
    let id = UUID()
    let timestamp: Date
    let category: String
    let message: String
}
