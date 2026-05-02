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
    case zonesEnabledChanged = "zones.enabledChanged"
    case parametersChanged = "parameters.changed"
    case effectsChanged = "effects.changed"
    case effectsList = "effects.list"
    case palettesList = "palettes.list"
    case deviceStatus = "device.status"
    case colourCorrectionConfig = "colorCorrection.getConfig"
    case audioSubscribed = "audio.subscribed"
    case audioUnsubscribed = "audio.unsubscribed"
    case ledStreamSubscribed = "ledStream.subscribed"
    case edgeMixerGet = "edge_mixer.get"
    case edgeMixerSet = "edge_mixer.set"
    case edgeMixerSave = "edge_mixer.save"

    // MARK: Phase 1 — broadcast cases
    // Added 2026-04-30 to cover broadcasts emitted by firmware that iOS previously
    // dropped silently. Phase 2 will wire UI handlers; for now these exist so the
    // decode path produces a typed Event rather than falling through to .unknown.
    case cameraModeChanged = "cameraMode.changed"
    case factoryPresetsChanged = "factoryPresets.changed"
    case effectPresetsSaved = "effectPresets.saved"
    case effectPresetsDeleted = "effectPresets.deleted"
    case colourCorrectionSetGamma = "colorCorrection.setGamma"
    case colourCorrectionSetAutoExposure = "colorCorrection.setAutoExposure"
    case colourCorrectionSetBrownGuardrail = "colorCorrection.setBrownGuardrail"

    // MARK: Phase 2 — STM and VRMS streams
    // Added 2026-05-01 (Task P2-2). Wire bindings:
    //   - stm.subscribed / stm.unsubscribed: text acks emitted by
    //     `WsStmCommands.cpp` after a client toggles STM streaming.
    //   - vrms.subscribed / vrms.unsubscribed: text acks from
    //     `WsStreamCommands.cpp` (FEATURE_VRMS_METRICS gate).
    //   - vrms.frame: 10 Hz JSON broadcast carrying perceptual metrics
    //     emitted by `WebServer.cpp` line 894.
    // STM uses a 250-byte BINARY frame with magic 0xFD (no text type — handled
    // entirely by `handleBinaryMessage`).
    case stmSubscribed = "stm.subscribed"
    case stmUnsubscribed = "stm.unsubscribed"
    case vrmsSubscribed = "vrms.subscribed"
    case vrmsUnsubscribed = "vrms.unsubscribed"
    case vrmsFrame = "vrms.frame"

    // MARK: Phase 2 — show transport WS commands
    //
    // Show playback transport (P2-4). The firmware emits `show.status` as the
    // canonical playback push and acknowledges each transport command by
    // echoing a `cmd`-prefixed object with `success: true`. iOS only needs the
    // raw values listed here on the inbound path; outbound dispatch uses the
    // string commands directly via `WebSocketService.send(...)`.
    case showStatus = "show.status"
    case showList = "show.list"
    case showPlay = "show.play"
    case showPause = "show.pause"
    case showResume = "show.resume"
    case showStop = "show.stop"
    case showSeek = "show.seek"

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
        case edgeMixerUpdate(WebSocketPayload)
        case connected
        case disconnected(Error?)

        // MARK: Phase 1 — broadcast cases
        // Added 2026-04-30. Each case carries the raw payload as a `WebSocketPayload`
        // so consumers can inspect `data` ad-hoc until Phase 2 introduces typed DTOs.
        case cameraModeChanged(WebSocketPayload)
        case factoryPresetsChanged(WebSocketPayload)
        case effectPresetsSaved(WebSocketPayload)
        case effectPresetsDeleted(WebSocketPayload)
        case colourCorrectionSetGamma(WebSocketPayload)
        case colourCorrectionSetAutoExposure(WebSocketPayload)
        case colourCorrectionSetBrownGuardrail(WebSocketPayload)

        // MARK: Phase 2 — STM and VRMS streams
        // Added 2026-05-01 (Task P2-2). Carry decoded frame structs so consumers
        // do not have to repeat the binary/JSON parsing.
        case stmFrame(STMFrame)
        case vrmsFrame(VRMSFrame, timestamp: UInt32)
        case stmSubscriptionAck(WebSocketPayload)
        case vrmsSubscriptionAck(WebSocketPayload)

        // MARK: Phase 2 — show transport WS commands
        // Firmware emits `show.status` on every playback transition. The other
        // show.* responses are command acks; we surface them as `.showAck` so
        // a UI can confirm dispatch without each command needing its own case.
        case showStatus(WebSocketPayload)
        case showAck(WebSocketPayload)
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

    // MARK: - Public Methods

    /// Connect to the WebSocket server and return an event stream
    func connect(to url: URL) -> AsyncStream<Event> {
        // Guard double-connect: clean up any existing connection first
        if webSocketTask != nil || eventContinuation != nil {
            disconnect()
        }

        currentURL = url
        shouldReconnect = true
        reconnectAttempts = 0

        return AsyncStream<Event> { continuation in
            self.eventContinuation = continuation
            self.performConnect()
        }
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

    /// Send a command with optional parameters.
    ///
    /// `params` accepts `[String: any Sendable]` so call sites in
    /// non-actor contexts pass values across the actor boundary cleanly
    /// under Swift 6 strict concurrency. Internally we widen to
    /// `[String: Any]` for `JSONSerialization`, which only ever runs on
    /// this actor.
    func send(_ command: String, params: [String: any Sendable] = [:]) {
        guard webSocketTask != nil else {
            print("WS send dropped (not connected): \(command)")
            return
        }
        var message: [String: Any] = ["type": command]
        for (key, value) in params {
            message[key] = value
        }
        sendRaw(message)
    }

    /// Subscribe to periodic status broadcasts.
    ///
    /// Required for the iOS app to receive `.status` events from K1 V2
    /// (commit firmware/streaming-and-status-gate). The firmware now gates
    /// its 5-second status-broadcast loop behind this subscribe — without it
    /// no periodic `.status` messages arrive, so the app would only see the
    /// status payload on explicit changes. Sent automatically by
    /// `AppViewModel.consumeWebSocketEvents` on the `.connected` event;
    /// no other call site should need it.
    func subscribeStatus() {
        send("status.subscribe")
    }

    /// Unsubscribe from periodic status broadcasts. The companion to
    /// `subscribeStatus()` above. Currently unused at the call-site level —
    /// the WebSocket disconnect on backgrounding implicitly tears down the
    /// firmware-side subscription — but exposed for completeness and future
    /// use (e.g. a low-power mode that pauses telemetry without dropping the
    /// socket).
    func unsubscribeStatus() {
        send("status.unsubscribe")
    }

    /// Subscribe to LED stream
    ///
    /// Phase 3 disabled — heap-stability mitigation. K1 allocates a per-client
    /// LED message queue (~7680 B = 8 frame slots × 960 B) on subscribe;
    /// suppressing the upstream subscribe prevents the internal-heap
    /// fragmentation that starves the firmware heap-shedding recovery path.
    /// Re-enable by uncommenting the `send` line below once the broadcaster
    /// fix lands. See docs/superpowers/ios-firmware-parity-phase-3-scoping.md.
    func subscribeLEDStream(udpPort: UInt16 = 41234) {
        _ = udpPort
        // send("ledStream.subscribe", params: ["udpPort": Int(udpPort)])
    }

    /// Subscribe to LED stream via WebSocket (no UDP transport)
    ///
    /// Phase 3 disabled — see `subscribeLEDStream(udpPort:)` above.
    func subscribeLEDStreamWS() {
        // send("ledStream.subscribe")
    }

    /// Unsubscribe from LED stream
    func unsubscribeLEDStream() {
        send("ledStream.unsubscribe")
    }

    /// Subscribe to audio metrics stream
    ///
    /// Phase 3 disabled — see `subscribeLEDStream(udpPort:)` above.
    func subscribeAudioStream(udpPort: UInt16 = 41234) {
        _ = udpPort
        // send("audio.subscribe", params: ["udpPort": Int(udpPort)])
    }

    /// Subscribe to audio metrics stream via WebSocket (no UDP transport)
    ///
    /// Phase 3 disabled — see `subscribeLEDStream(udpPort:)` above.
    func subscribeAudioStreamWS() {
        // send("audio.subscribe")
    }

    /// Unsubscribe from audio metrics stream
    func unsubscribeAudioStream() {
        send("audio.unsubscribe")
    }

    /// Trigger a transition effect
    func triggerTransition(type: Int, duration: Int? = nil, toEffect: Int? = nil) {
        var params: [String: any Sendable] = ["type": type]
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
        var params: [String: any Sendable] = [:]
        if let requestId = requestId {
            params["requestId"] = requestId
        }
        send("audio.parameters.get", params: params)
    }

    /// Patch audio tuning parameters
    func sendAudioParametersSet(payload: [String: any Sendable], requestId: String? = nil) {
        var params = payload
        if let requestId = requestId {
            params["requestId"] = requestId
        }
        send("audio.parameters.set", params: params)
    }

    // MARK: Phase 2 — STM and VRMS streams

    /// Subscribe to the firmware's STM (Spectral-Temporal Modulation) binary
    /// stream. Frames arrive at ~30 FPS as 250-byte binary WebSocket messages
    /// with magic 0xFD. The firmware emits a `stm.subscribed` text ack on
    /// success, surfaced as `Event.stmSubscriptionAck`.
    func subscribeSTM(requestId: String? = nil) {
        var params: [String: any Sendable] = [:]
        if let requestId = requestId {
            params["requestId"] = requestId
        }
        send("stm.subscribe", params: params)
    }

    /// Unsubscribe from the STM binary stream.
    func unsubscribeSTM(requestId: String? = nil) {
        var params: [String: any Sendable] = [:]
        if let requestId = requestId {
            params["requestId"] = requestId
        }
        send("stm.unsubscribe", params: params)
    }

    /// Subscribe to the firmware's VRMS (Visual RMS) perceptual-metrics
    /// stream. Frames arrive at 10 Hz as JSON text messages of type
    /// `vrms.frame`, decoded by `handleTextMessage` into `VRMSFrame`.
    func subscribeVRMS(requestId: String? = nil) {
        var params: [String: any Sendable] = [:]
        if let requestId = requestId {
            params["requestId"] = requestId
        }
        send("vrms.subscribe", params: params)
    }

    /// Unsubscribe from the VRMS metrics stream.
    func unsubscribeVRMS(requestId: String? = nil) {
        var params: [String: any Sendable] = [:]
        if let requestId = requestId {
            params["requestId"] = requestId
        }
        send("vrms.unsubscribe", params: params)
    }

    // MARK: Phase 2 — show transport WS commands
    //
    // Outbound transport dispatch helpers. Each maps to a single-line `send`
    // call to the firmware. Returning `Void` and being implicitly async (actor
    // isolation) keeps the call sites compact. The firmware ack (`cmd` +
    // `success`) is delivered via `.showAck` on the event stream; the
    // canonical state update arrives as `.showStatus`.

    /// `show.list` — request the catalogue of built-in and uploaded shows.
    /// REST `getShows()` is the preferred read path; this is provided for
    /// parity / debugging.
    func sendShowList() {
        send("show.list")
    }

    /// `show.play` — start playback of `showId`.
    func sendShowPlay(showId: String) {
        send("show.play", params: ["showId": showId])
    }

    /// `show.pause` — pause the currently playing show.
    func sendShowPause() {
        send("show.pause")
    }

    /// `show.resume` — resume from a paused show.
    func sendShowResume() {
        send("show.resume")
    }

    /// `show.stop` — stop the currently playing show.
    func sendShowStop() {
        send("show.stop")
    }

    /// `show.seek` — seek to `timeMs` within the current show.
    func sendShowSeek(timeMs: Int) {
        send("show.seek", params: ["timeMs": timeMs])
    }

    /// `show.status` — request a fresh playback status frame from the
    /// firmware (firmware also broadcasts unsolicited).
    func sendShowStatus() {
        send("show.status")
    }

    // MARK: - Private Connection Management

    private func performConnect() {
        guard let url = currentURL else { return }

        closeConnection()

        guard let session = session else { return }

        let task = session.webSocketTask(with: url)
        webSocketTask = task

        task.resume()
        isConnected = true
        reconnectAttempts = 0

        // Yield connected event
        eventContinuation?.yield(.connected)

        // Start receiving messages
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
        while let task = webSocketTask, task.state == .running {
            do {
                let message = try await task.receive()
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
        // Inspector capture — non-blocking, O(1) actor send. Records the raw
        // text BEFORE JSON parsing so malformed frames are still observable
        // in the inspector. See WSInspector.swift for the ring-buffer details.
        Task.detached(priority: .background) {
            await WSInspector.shared.recordInboundText(text)
        }

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

        case .zonesList, .zonesChanged, .zonesStateChanged, .zonesEffectChanged, .zonesLayoutChanged, .zoneEnabledChanged, .zonesEnabledChanged:
            eventContinuation?.yield(.zoneUpdate(payload))

        case .parametersChanged:
            eventContinuation?.yield(.parameterUpdate(payload))

        case .edgeMixerGet, .edgeMixerSet:
            eventContinuation?.yield(.edgeMixerUpdate(payload))

        case .edgeMixerSave:
            #if DEBUG
            print("[WS] EdgeMixer save ack")
            #endif

        case .deviceStatus, .effectsChanged, .effectsList, .palettesList, .colourCorrectionConfig:
            // These are handled via REST API, not event stream
            break

        case .audioSubscribed, .audioUnsubscribed, .ledStreamSubscribed:
            #if DEBUG
            let success = json["success"] as? Bool ?? (json["status"] as? String == "ok")
            let errorCode = (json["error"] as? [String: Any])?["code"] as? String
            print("[WS] Subscription response: \(messageType.rawValue) success=\(success) error=\(errorCode ?? "none")")
            #endif

        // MARK: Phase 1 — broadcast decoding
        // Each branch yields a typed Event with the raw JSON payload attached so
        // downstream consumers can introspect fields as needed. Phase 2 will
        // tighten payload modelling; for now `WebSocketPayload` is sufficient.
        case .cameraModeChanged:
            eventContinuation?.yield(.cameraModeChanged(payload))

        case .factoryPresetsChanged:
            eventContinuation?.yield(.factoryPresetsChanged(payload))

        case .effectPresetsSaved:
            eventContinuation?.yield(.effectPresetsSaved(payload))

        case .effectPresetsDeleted:
            eventContinuation?.yield(.effectPresetsDeleted(payload))

        case .colourCorrectionSetGamma:
            eventContinuation?.yield(.colourCorrectionSetGamma(payload))

        case .colourCorrectionSetAutoExposure:
            eventContinuation?.yield(.colourCorrectionSetAutoExposure(payload))

        case .colourCorrectionSetBrownGuardrail:
            eventContinuation?.yield(.colourCorrectionSetBrownGuardrail(payload))

        // MARK: Phase 2 — STM and VRMS streams (text message dispatch)
        case .stmSubscribed, .stmUnsubscribed:
            eventContinuation?.yield(.stmSubscriptionAck(payload))

        case .vrmsSubscribed, .vrmsUnsubscribed:
            eventContinuation?.yield(.vrmsSubscriptionAck(payload))

        case .vrmsFrame:
            // The firmware emits VRMS as a JSON text message rather than a
            // binary frame (see WebServer.cpp lines 884-924). Decode the
            // envelope into our typed `VRMSFrame` struct.
            if let frame = decodeVRMSFrame(from: data) {
                eventContinuation?.yield(.vrmsFrame(frame.metrics, timestamp: frame.timestamp))
            }

        // MARK: Phase 2 — show transport WS commands
        // `show.status` is the canonical playback frame and is yielded as a
        // dedicated event. `show.list`, `show.play`, `show.pause`, `show.resume`,
        // `show.stop`, `show.seek` arrive as command acks (with `cmd` and
        // `success` keys); we yield them via `.showAck` so consumers can
        // decide whether to surface them in UI.
        case .showStatus:
            eventContinuation?.yield(.showStatus(payload))

        case .showList, .showPlay, .showPause, .showResume, .showStop, .showSeek:
            eventContinuation?.yield(.showAck(payload))

        case .unknown:
            break
        }
    }

    /// Decode a `vrms.frame` JSON envelope from raw text-message bytes.
    private func decodeVRMSFrame(from data: Data) -> VRMSEnvelope? {
        try? JSONDecoder().decode(VRMSEnvelope.self, from: data)
    }

    // LED stream constants matching firmware LedStreamConfig
    private static let ledFrameV1Size = 966       // v1: 4-byte header + 2×(1 + 160×3)
    private static let ledFrameLegacySize = 961   // v0: 1 magic + 320×3
    private static let ledMagicByte: UInt8 = 0xFE
    private static let ledsPerStrip = 160
    private static let rgbPerStrip = 160 * 3      // 480 bytes

    private func handleBinaryMessage(_ data: Data) {
        // Inspector capture — non-blocking, O(1) actor send. We pass the
        // raw `Data` so the inspector can read the magic byte and length;
        // it does NOT retain the underlying bytes (would saturate the ring
        // buffer at 30 FPS LED streaming).
        Task.detached(priority: .background) {
            await WSInspector.shared.recordInboundBinary(data)
        }

        // MARK: Phase 2 — STM and VRMS streams (binary dispatch)
        // STM frame: 250 bytes, magic 0xFD (first byte). Distinct from LED 0xFE
        // and audio metrics 0x41 (low byte of little-endian 0x00445541).
        if data.count == STMFrame.frameSize, data.first == STMFrame.magic {
            if let frame = STMFrame(data: data) {
                eventContinuation?.yield(.stmFrame(frame))
                return
            }
            // Length matched but magic-aware decode failed — fall through and
            // drop silently rather than misroute as another frame type.
            return
        }

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
        // Inspector capture — non-blocking, O(1) actor send. Records on
        // every dispatch attempt; failures are still useful in the
        // inspector because they show what the app TRIED to send.
        Task.detached(priority: .background) {
            await WSInspector.shared.recordOutboundText(text)
        }

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

    // MARK: Phase 2 — preset CRUD WS commands
    //
    // Typed helpers around `send(_:params:)` for the effect / zone preset
    // commands consumed by `PresetsViewModel`. Each method dispatches the
    // exact WS command name registered by the firmware in
    // `WsEffectPresetCommands.cpp` / `WsZonePresetCommands.cpp`. Broadcast
    // responses (`effectPresets.saved` / `effectPresets.deleted` /
    // `zonePresets.saved` / `zonePresets.deleted`) are decoded by the Phase 1
    // broadcast cases at the top of this file.

    /// Save the current live effect configuration into a slot.
    func saveCurrentEffectPreset(slot: Int, name: String) {
        send("effectPresets.saveCurrent", params: ["slot": slot, "name": name])
    }

    /// Apply a stored effect preset by slot id.
    func loadEffectPreset(id: Int) {
        send("effectPresets.load", params: ["id": id])
    }

    /// Delete an effect preset by slot id.
    func deleteEffectPreset(id: Int) {
        send("effectPresets.delete", params: ["id": id])
    }

    /// Save the current live zone configuration into a slot.
    func saveCurrentZonePreset(slot: Int, name: String) {
        send("zonePresets.saveCurrent", params: ["slot": slot, "name": name])
    }

    /// Apply a stored zone preset by id.
    func loadZonePreset(id: Int) {
        send("zonePresets.load", params: ["id": id])
    }

    /// Delete a user-saved zone preset by id. Built-in presets are
    /// rejected by the firmware — UI must not call this for `builtin == true`
    /// rows.
    func deleteZonePreset(id: Int) {
        send("zonePresets.delete", params: ["id": id])
    }
}

// MARK: - Phase 2 — preset CRUD test seam

/// Conform `WebSocketService` to the `PresetWebSocketCommanding` protocol so
/// `PresetsViewModel` can dispatch commands without depending on the concrete
/// actor type. Tests substitute a recording double.
///
/// The protocol takes `[String: any Sendable]` so the parameter map can cross
/// the actor boundary under Swift 6 strict concurrency. Inside the actor the
/// existing synchronous `send(_:params:)` is reused — the dictionary is
/// upcast to `[String: Any]` once isolation is established.
@available(iOS 17.0, *)
extension WebSocketService: PresetWebSocketCommanding {
    func sendCommand(_ command: String, params: [String: any Sendable]) async {
        // `send(_:params:)` now accepts the same Sendable map type, so the
        // protocol pass-through is a direct call — no upcast required.
        send(command, params: params)
    }
}

