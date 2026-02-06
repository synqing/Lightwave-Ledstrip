//
//  WebSocketService.swift
//  LightwaveOS
//
//  Actor-based WebSocket service for real-time communication with ESP32 device.
//  Handles JSON text messages and binary LED stream data.
//  Auto-reconnect with exponential backoff.
//  iOS 17+, Swift 6 with strict concurrency. British English comments.
//

import Foundation

// MARK: - WebSocket Message Types

enum WebSocketMessageType: String {
    case status
    case beatEvent = "beat.event"
    case zonesList = "zones.list"
    case zonesChanged = "zones.changed"
    case zonesStateChanged = "zones.stateChanged"
    case zonesEffectChanged = "zones.effectChanged"
    case zonesLayoutChanged = "zones.layoutChanged"
    case zoneEnabledChanged = "zone.enabledChanged"
    case parametersChanged = "parameters.changed"
    case effectsChanged = "effects.changed"
    case effectsList = "effects.list"
    case palettesList = "palettes.list"
    case deviceStatus = "device.status"
    case colourCorrectionConfig = "colorCorrection.getConfig"
    case audioSubscribed = "audio.subscribed"
    case audioUnsubscribed = "audio.unsubscribed"
    case ledStreamSubscribed = "ledStream.subscribed"
    case unknown
}

// MARK: - WebSocket Service Actor

@available(iOS 17.0, *)
actor WebSocketService {
    // MARK: - Event Stream Types

    /// Event stream for WebSocket state and messages
    enum Event: Sendable {
        case status(WebSocketPayload)
        case beat(WebSocketPayload)
        case zoneUpdate(WebSocketPayload)
        case parameterUpdate(WebSocketPayload)
        case ledData(Data)
        case audioMetrics(AudioMetricsFrame)
        case connected
        case disconnected(Error?)
    }

    /// Sendable wrapper for [String: Any] JSON payloads
    struct WebSocketPayload: @unchecked Sendable {
        let data: [String: Any]
    }

    // MARK: - Public State

    private(set) var isConnected: Bool = false

    // MARK: - Private State

    private var webSocketTask: URLSessionWebSocketTask?
    private var session: URLSession?
    private var shouldReconnect = true
    private var reconnectAttempts = 0
    private var reconnectTask: Task<Void, Never>?
    private var receiveTask: Task<Void, Never>?
    private var monitorTask: Task<Void, Never>?

    private let initialBackoff: TimeInterval = 3.0
    private let maxBackoff: TimeInterval = 30.0
    private let backoffMultiplier: Double = 2.0

    private var currentURL: URL?
    private var eventContinuation: AsyncStream<Event>.Continuation?

    // MARK: - Initialization

    init() {
        let config = URLSessionConfiguration.default
        config.timeoutIntervalForRequest = 30.0
        self.session = URLSession(configuration: config)
    }

    deinit {
        // Finish any lingering continuation to unblock consumers
        eventContinuation?.finish()
        session?.invalidateAndCancel()
    }

    // MARK: - Public Methods

    /// Connect to the WebSocket server and return an event stream
    /// - Parameters:
    ///   - url: The WebSocket URL to connect to
    ///   - autoReconnect: If false, WebSocketService will NOT auto-reconnect on disconnect.
    ///                    Set to false when ConnectionManager manages reconnection.
    func connect(to url: URL, autoReconnect: Bool = false) -> AsyncStream<Event> {
        // Guard double-connect: clean up any existing connection first
        if webSocketTask != nil || eventContinuation != nil {
            disconnect()
        }

        currentURL = url
        shouldReconnect = autoReconnect
        reconnectAttempts = 0

        // Use makeStream() to avoid actor isolation issues in AsyncStream closure
        let (stream, continuation) = AsyncStream<Event>.makeStream()
        self.eventContinuation = continuation
        self.performConnect()
        return stream
    }

    /// Disconnect from the WebSocket server
    func disconnect() {
        shouldReconnect = false
        reconnectTask?.cancel()
        reconnectTask = nil
        receiveTask?.cancel()
        receiveTask = nil
        monitorTask?.cancel()
        monitorTask = nil
        closeConnection()
        eventContinuation?.finish()
        eventContinuation = nil
    }

    /// Graceful disconnect: unsubscribe from streams before closing.
    /// Tells the v2 to stop queuing frames immediately rather than waiting
    /// for TCP close detection (which can take 10-30 seconds).
    func gracefulDisconnect() async {
        shouldReconnect = false
        reconnectTask?.cancel()
        reconnectTask = nil

        // Send unsubscribe commands before closing (best-effort)
        if webSocketTask != nil, webSocketTask?.state == .running {
            await sendText("{\"type\":\"ledStream.unsubscribe\"}")
            await sendText("{\"type\":\"audio.unsubscribe\"}")
            // Brief delay for sends to flush through TCP
            try? await Task.sleep(for: .milliseconds(100))
        }

        receiveTask?.cancel()
        receiveTask = nil
        monitorTask?.cancel()
        monitorTask = nil
        closeConnection()
        eventContinuation?.finish()
        eventContinuation = nil
    }

    /// Send a command with optional parameters
    func send(_ command: String, params: [String: Any] = [:]) {
        guard let task = webSocketTask, task.state == .running else {
            return
        }
        var message: [String: Any] = ["type": command]
        message.merge(params) { _, new in new }
        sendRaw(message)
    }

    /// Subscribe to LED stream
    func subscribeLEDStream(udpPort: UInt16 = 41234) {
        send("ledStream.subscribe", params: ["udpPort": Int(udpPort)])
    }

    /// Subscribe to LED stream via WebSocket (no UDP transport)
    func subscribeLEDStreamWS() {
        send("ledStream.subscribe")
    }

    /// Unsubscribe from LED stream
    func unsubscribeLEDStream() {
        send("ledStream.unsubscribe")
    }

    /// Subscribe to audio metrics stream
    func subscribeAudioStream(udpPort: UInt16 = 41234) {
        send("audio.subscribe", params: ["udpPort": Int(udpPort)])
    }

    /// Subscribe to audio metrics stream via WebSocket (no UDP transport)
    func subscribeAudioStreamWS() {
        send("audio.subscribe")
    }

    /// Unsubscribe from audio metrics stream
    func unsubscribeAudioStream() {
        send("audio.unsubscribe")
    }

    /// Trigger a transition effect
    func triggerTransition(type: Int, duration: Int? = nil, toEffect: Int? = nil) {
        var params: [String: Any] = ["type": type]
        if let duration = duration {
            params["duration"] = duration
        }
        if let toEffect = toEffect {
            params["toEffect"] = toEffect
        }
        send("transition.trigger", params: params)
    }

    /// Fetch current audio tuning parameters
    func sendAudioParametersGet(requestId: String? = nil) {
        var params: [String: Any] = [:]
        if let requestId = requestId {
            params["requestId"] = requestId
        }
        send("audio.parameters.get", params: params)
    }

    /// Patch audio tuning parameters
    func sendAudioParametersSet(payload: [String: Any], requestId: String? = nil) {
        var params = payload
        if let requestId = requestId {
            params["requestId"] = requestId
        }
        send("audio.parameters.set", params: params)
    }

    // MARK: - Private Connection Management

    private func performConnect() {
        guard let url = currentURL else { return }

        // Guard: don't clobber an active connection (e.g. during reconnect race)
        if let task = webSocketTask, task.state == .running {
            return
        }

        closeConnection()

        guard let session = session else { return }

        let task = session.webSocketTask(with: url)
        webSocketTask = task

        task.resume()
        // NOTE: isConnected and .connected event are deferred until the first
        // successful receive() — that confirms the WS handshake actually completed.

        // Start receiving messages (will yield .connected on first message)
        receiveTask = Task {
            await receiveMessages()
        }

        // Start connection monitoring
        monitorTask = Task {
            await monitorConnection()
        }
    }

    private func closeConnection() {
        webSocketTask?.cancel(with: .goingAway, reason: nil)
        webSocketTask = nil

        if isConnected {
            isConnected = false
            eventContinuation?.yield(.disconnected(nil))
        }
    }

    // MARK: - Message Receiving

    private func receiveMessages() async {
        var handshakeConfirmed = false
        while let task = webSocketTask, task.state == .running {
            do {
                let message = try await task.receive()
                // First successful receive confirms the WS handshake completed
                if !handshakeConfirmed {
                    handshakeConfirmed = true
                    isConnected = true
                    reconnectAttempts = 0
                    eventContinuation?.yield(.connected)
                }
                handleMessage(message)
            } catch {
                handleConnectionError(error)
                break
            }
        }
    }

    private func handleMessage(_ message: URLSessionWebSocketTask.Message) {
        switch message {
        case .string(let text):
            handleTextMessage(text)

        case .data(let data):
            handleBinaryMessage(data)

        @unknown default:
            break
        }
    }

    private func handleTextMessage(_ text: String) {
        guard let data = text.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return
        }

        let messageType = (json["type"] as? String).flatMap { WebSocketMessageType(rawValue: $0) } ?? .unknown
        let payload = WebSocketPayload(data: json)

        switch messageType {
        case .status:
            eventContinuation?.yield(.status(payload))

        case .beatEvent:
            eventContinuation?.yield(.beat(payload))

        case .zonesList, .zonesChanged, .zonesStateChanged, .zonesEffectChanged, .zonesLayoutChanged, .zoneEnabledChanged:
            eventContinuation?.yield(.zoneUpdate(payload))

        case .parametersChanged:
            eventContinuation?.yield(.parameterUpdate(payload))

        case .deviceStatus, .effectsChanged, .effectsList, .palettesList, .colourCorrectionConfig:
            // These are handled via REST API, not event stream
            break

        case .audioSubscribed, .audioUnsubscribed, .ledStreamSubscribed:
            #if DEBUG
            let success = json["success"] as? Bool ?? (json["status"] as? String == "ok")
            let errorCode = (json["error"] as? [String: Any])?["code"] as? String
            print("[WS] Subscription response: \(messageType.rawValue) success=\(success) error=\(errorCode ?? "none")")
            #endif

        case .unknown:
            break
        }
    }

    // LED stream constants matching firmware LedStreamConfig
    private static let ledFrameV1Size = 966       // v1: 4-byte header + 2×(1 + 160×3)
    private static let ledFrameLegacySize = 961   // v0: 1 magic + 320×3
    private static let ledMagicByte: UInt8 = 0xFE
    private static let ledsPerStrip = 160
    private static let rgbPerStrip = 160 * 3      // 480 bytes

    private func handleBinaryMessage(_ data: Data) {
        // Audio metrics frame: 464 bytes, magic 0x00445541
        if data.count == AudioMetricsFrame.frameSize {
            let magic = data.withUnsafeBytes { $0.load(as: UInt32.self) }
            if magic == AudioMetricsFrame.magic {
                if let frame = AudioMetricsFrame(data: data) {
                    eventContinuation?.yield(.audioMetrics(frame))
                    return
                }
            }
        }

        guard data.first == Self.ledMagicByte else { return }

        // LED stream v1 format (966 bytes): [0xFE][version][numStrips][ledsPerStrip][stripId][RGB×160][stripId][RGB×160]
        if data.count == Self.ledFrameV1Size {
            var rgb = Data(capacity: 960)
            // Strip 0: header(4) + stripId(1) = offset 5, length 480
            let strip0Start = 5
            rgb.append(data[strip0Start ..< strip0Start + Self.rgbPerStrip])
            // Strip 1: strip0Start + 480 + stripId(1) = offset 486, length 480
            let strip1Start = strip0Start + Self.rgbPerStrip + 1
            rgb.append(data[strip1Start ..< strip1Start + Self.rgbPerStrip])
            eventContinuation?.yield(.ledData(rgb))
            return
        }

        // Legacy format (961 bytes): [0xFE][RGB×320]
        if data.count == Self.ledFrameLegacySize {
            eventContinuation?.yield(.ledData(Data(data.dropFirst())))
            return
        }
    }

    // MARK: - Message Sending

    private func sendRaw(_ message: [String: Any]) {
        guard let data = try? JSONSerialization.data(withJSONObject: message),
              let text = String(data: data, encoding: .utf8) else {
            return
        }

        Task {
            await sendText(text)
        }
    }

    private func sendText(_ text: String) async {
        do {
            try await webSocketTask?.send(.string(text))
        } catch {
            // Connection error will be handled by receive loop or monitor
            #if DEBUG
            print("[WS] send failed: \(error.localizedDescription)")
            #endif
        }
    }

    // MARK: - Connection Monitoring & Reconnection

    private func monitorConnection() async {
        // Periodically ping to detect disconnection
        while shouldReconnect {
            try? await Task.sleep(nanoseconds: 10_000_000_000) // 10 seconds

            guard let task = webSocketTask, task.state == .running else {
                // Task is not running — trigger reconnect
                if shouldReconnect {
                    isConnected = false
                    scheduleReconnect()
                }
                return
            }

            task.sendPing { [weak self] error in
                if let error = error {
                    Task {
                        await self?.handleConnectionError(error)
                    }
                }
            }
        }
    }

    private func handleConnectionError(_ error: Error) {
        isConnected = false
        eventContinuation?.yield(.disconnected(error))

        if shouldReconnect {
            scheduleReconnect()
        }
    }

    private func scheduleReconnect() {
        reconnectTask?.cancel()

        let backoff = min(
            initialBackoff * pow(backoffMultiplier, Double(reconnectAttempts)),
            maxBackoff
        )

        reconnectAttempts += 1

        reconnectTask = Task {
            try? await Task.sleep(for: .seconds(backoff))

            guard !Task.isCancelled, shouldReconnect else { return }

            performConnect()
        }
    }
}
