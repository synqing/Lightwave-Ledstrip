//
//  WSInspector.swift
//  LightwaveOS
//
//  In-app WebSocket frame inspector. Captures send + receive frames into a
//  bounded ring buffer and republishes them to subscribers as an AsyncStream.
//
//  iOS 17+, Swift 6 with strict concurrency. British English comments.
//
//  Design notes:
//    * The inspector is an `actor` so concurrent writes from the WebSocket
//      receive loop and the application's send paths do not race. All public
//      operations are O(1) — `record` performs a single ring-buffer append
//      and an optional FIFO eviction; `snapshot` copies the buffer; filters
//      run on the snapshot in the calling task, never in the actor.
//    * Capacity is fixed at 200 frames (≈50 KB of memory at the average JSON
//      payload size we see in production traces). Configurable via init for
//      tests that exercise overflow behaviour.
//    * A shared singleton (`WSInspector.shared`) provides a no-injection
//      capture point for `WebSocketService`. Tests construct a fresh
//      instance and exercise the API directly without touching the shared
//      one.
//

import Foundation

// MARK: - WSInspector

/// Actor-scoped WebSocket frame ring buffer with subscription support.
///
/// Use the `record(...)` API from any context (including non-isolated
/// callers) — Swift's actor isolation serialises writes. Subscribers receive
/// the current snapshot of the buffer up to `bufferLimit` and then a live
/// stream of frames as they are recorded.
@available(iOS 17.0, *)
actor WSInspector {

    // MARK: - Shared instance

    /// Process-wide singleton used by `WebSocketService` capture hooks. Tests
    /// must NOT touch this instance — they build their own to keep state
    /// isolated.
    static let shared = WSInspector()

    // MARK: - Stored state

    /// Maximum number of frames retained in the ring buffer. Once exceeded
    /// the oldest entry is dropped FIFO.
    nonisolated let bufferLimit: Int

    /// Bounded buffer of captured frames, oldest first. The `WSInspector`
    /// view inverts this for display (newest at top) so the model layer can
    /// retain a natural insertion order.
    private var buffer: [WSFrame] = []

    /// Live subscribers. Each subscriber gets a yield call on every record;
    /// on cancellation the continuation is removed from this list.
    private var continuations: [UUID: AsyncStream<WSFrame>.Continuation] = [:]

    /// Optional clock injection for deterministic tests. Defaults to
    /// `Date.init` for production. Tests pass a fixed-clock closure.
    private let clock: @Sendable () -> Date

    // MARK: - Init

    /// Designated initialiser.
    /// - Parameters:
    ///   - bufferLimit: Maximum retained frames. Defaults to 200.
    ///   - clock: Closure returning "now". Defaults to `Date.init`.
    init(bufferLimit: Int = 200, clock: @Sendable @escaping () -> Date = Date.init) {
        self.bufferLimit = max(1, bufferLimit)
        self.clock = clock
    }

    // MARK: - Recording

    /// Record an outbound text frame. Non-blocking from the caller's
    /// perspective because the actor serialises the append.
    func recordOutboundText(_ text: String) {
        let parsed = WSFrame.parseTypeAndDeviceId(from: text)
        let frame = WSFrame(
            timestamp: clock(),
            direction: .outbound,
            messageType: parsed.messageType,
            typeLabel: parsed.typeLabel,
            payload: .text(text),
            deviceId: parsed.deviceId
        )
        appendAndPublish(frame)
    }

    /// Record an inbound text frame.
    func recordInboundText(_ text: String) {
        let parsed = WSFrame.parseTypeAndDeviceId(from: text)
        let frame = WSFrame(
            timestamp: clock(),
            direction: .inbound,
            messageType: parsed.messageType,
            typeLabel: parsed.typeLabel,
            payload: .text(text),
            deviceId: parsed.deviceId
        )
        appendAndPublish(frame)
    }

    /// Record an inbound binary frame. We never retain the raw payload —
    /// only the magic byte and length. Binary frames are too large to keep
    /// in a debug ring buffer (LED frames at 30 FPS would saturate it
    /// instantly).
    func recordInboundBinary(_ data: Data) {
        let magic = data.first
        let frame = WSFrame(
            timestamp: clock(),
            direction: .inbound,
            messageType: .unknown,
            typeLabel: WSFrame.binaryLabel(for: magic, byteCount: data.count),
            payload: .binary(byteCount: data.count, magicByte: magic),
            deviceId: nil
        )
        appendAndPublish(frame)
    }

    /// Record a pre-built frame. Primarily used by tests; production code
    /// uses the typed `record*` helpers above.
    func record(_ frame: WSFrame) {
        appendAndPublish(frame)
    }

    /// Internal append + eviction. Single ring-buffer maintenance point so
    /// the FIFO invariant is asserted in exactly one place.
    private func appendAndPublish(_ frame: WSFrame) {
        buffer.append(frame)
        if buffer.count > bufferLimit {
            buffer.removeFirst(buffer.count - bufferLimit)
        }
        for (_, continuation) in continuations {
            continuation.yield(frame)
        }
    }

    // MARK: - Read-side API

    /// Return the current ring buffer contents (oldest → newest).
    func snapshot() -> [WSFrame] {
        return buffer
    }

    /// Number of frames currently retained.
    func count() -> Int {
        return buffer.count
    }

    /// Drop every captured frame.
    func clear() {
        buffer.removeAll()
    }

    /// Subscribe to live frame events. The subscriber receives every NEW
    /// frame; the caller is responsible for fetching the existing snapshot
    /// separately if they need it. Cancellation is handled automatically
    /// when the consuming for-await loop terminates.
    func subscribe() -> AsyncStream<WSFrame> {
        let id = UUID()
        return AsyncStream { continuation in
            self.continuations[id] = continuation
            continuation.onTermination = { @Sendable [weak self] _ in
                Task { [weak self] in
                    await self?.unsubscribe(id: id)
                }
            }
        }
    }

    /// Internal subscriber removal. Bridges `onTermination` (synchronous
    /// `@Sendable`) into actor isolation.
    private func unsubscribe(id: UUID) {
        continuations.removeValue(forKey: id)
    }
}

// MARK: - WSInspectorFilter

/// Pure-value filter spec applied client-side to a `[WSFrame]` snapshot.
///
/// Filters compose with logical AND. A nil / empty field means "no
/// constraint on this dimension" so a default-constructed filter passes
/// every frame through.
struct WSInspectorFilter: Sendable, Equatable {
    /// Restrict to a specific direction. `nil` = either direction allowed.
    var direction: WSFrameDirection?

    /// Restrict to specific message types. Empty set = any type allowed.
    /// Filtering on `.unknown` matches both unknown JSON envelopes and all
    /// binary frames; chip-based UI usually wants to filter on `typeLabel`
    /// instead — see `typeLabels`.
    var messageTypes: Set<WebSocketMessageType> = []

    /// Restrict to specific typeLabels (the wire-level type string). Empty
    /// set = any label allowed. Use this when the chip set should include
    /// binary frame labels like `led_data[966B]`.
    var typeLabels: Set<String> = []

    /// Restrict to frames whose deviceId contains this substring (case
    /// insensitive). Empty / nil = no constraint.
    var deviceIdSearch: String? = nil

    /// Restrict to frames at or after this timestamp. `nil` = no lower
    /// bound.
    var startTime: Date? = nil

    /// Restrict to frames at or before this timestamp. `nil` = no upper
    /// bound.
    var endTime: Date? = nil

    /// True if this filter has no active constraints (all defaults).
    var isEmpty: Bool {
        return direction == nil
            && messageTypes.isEmpty
            && typeLabels.isEmpty
            && (deviceIdSearch?.isEmpty ?? true)
            && startTime == nil
            && endTime == nil
    }

    /// Apply the filter to a snapshot. Pure function; no side effects.
    func apply(to frames: [WSFrame]) -> [WSFrame] {
        // Fast path: no constraints means a verbatim copy.
        guard !isEmpty else { return frames }

        return frames.filter { frame in
            if let direction = direction, frame.direction != direction {
                return false
            }
            if !messageTypes.isEmpty, !messageTypes.contains(frame.messageType) {
                return false
            }
            if !typeLabels.isEmpty, !typeLabels.contains(frame.typeLabel) {
                return false
            }
            if let search = deviceIdSearch, !search.isEmpty {
                let lower = search.lowercased()
                let frameId = frame.deviceId?.lowercased() ?? ""
                if !frameId.contains(lower) {
                    return false
                }
            }
            if let start = startTime, frame.timestamp < start {
                return false
            }
            if let end = endTime, frame.timestamp > end {
                return false
            }
            return true
        }
    }
}
