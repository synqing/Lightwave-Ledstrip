//
//  WSInspectorTests.swift
//  LightwaveOSTests
//
//  Unit tests for the in-app WebSocket frame inspector.
//
//  Coverage targets:
//    * `WSFrame` decode helpers — text JSON parse, binary magic-byte tagging,
//      summary truncation, pretty-print payload.
//    * `WSInspector` actor — record + snapshot round-trip, ring-buffer FIFO
//      eviction, clear, subscriber stream.
//    * `WSInspectorFilter` — direction / messageType / typeLabels / deviceId
//      / timestamp range and the empty-filter fast path.
//

import XCTest
@testable import LightwaveOS

@available(iOS 17.0, *)
final class WSInspectorTests: XCTestCase {

    // MARK: - WSFrame text decode

    /// `decodeText` parses a JSON envelope with a known `type` field into the
    /// matching `WebSocketMessageType`, propagating the wire-level `typeLabel`.
    func testDecodeText_parsesKnownMessageType() {
        let json = #"{"type":"status","fps":119}"#
        let frame = WSFrame.decodeText(json, direction: .inbound)

        XCTAssertEqual(frame.direction, .inbound)
        XCTAssertEqual(frame.messageType, .status)
        XCTAssertEqual(frame.typeLabel, "status")
        XCTAssertNil(frame.deviceId)
    }

    /// `decodeText` falls back to `.unknown` for an unrecognised `type`.
    func testDecodeText_unknownTypeFallsBack() {
        let json = #"{"type":"made.up.event"}"#
        let frame = WSFrame.decodeText(json, direction: .outbound)

        XCTAssertEqual(frame.messageType, .unknown)
        XCTAssertEqual(frame.typeLabel, "made.up.event")
    }

    /// Malformed JSON still produces a frame so the inspector can show what
    /// the app tried to send.
    func testDecodeText_invalidJsonProducesRawTextFrame() {
        let raw = "not really json"
        let frame = WSFrame.decodeText(raw, direction: .outbound)

        XCTAssertEqual(frame.messageType, .unknown)
        XCTAssertEqual(frame.typeLabel, "raw-text")

        if case let .text(text) = frame.payload {
            XCTAssertEqual(text, raw)
        } else {
            XCTFail("Expected text payload")
        }
    }

    /// `deviceId` and `clientId` keys both populate the deviceId field.
    func testDecodeText_extractsDeviceIdAndClientId() {
        let withDevice = WSFrame.decodeText(#"{"type":"status","deviceId":"K1-AB12"}"#,
                                            direction: .inbound)
        XCTAssertEqual(withDevice.deviceId, "K1-AB12")

        let withClient = WSFrame.decodeText(#"{"type":"effects.list","clientId":"ios-99"}"#,
                                            direction: .inbound)
        XCTAssertEqual(withClient.deviceId, "ios-99")
    }

    // MARK: - WSFrame binary decode

    /// LED stream binary frames (magic 0xFE) get tagged `led_data[…B]`.
    func testDecodeBinary_ledMagic() {
        let payload = Data([0xFE] + Array(repeating: UInt8(0), count: 965))
        let frame = WSFrame.decodeBinary(payload, direction: .inbound)

        XCTAssertEqual(frame.direction, .inbound)
        XCTAssertEqual(frame.typeLabel, "led_data[966B]")
        if case let .binary(byteCount, magic) = frame.payload {
            XCTAssertEqual(byteCount, 966)
            XCTAssertEqual(magic, 0xFE)
        } else {
            XCTFail("Expected binary payload")
        }
    }

    /// STM frames (magic 0xFD) get tagged `stm_frame[…B]`.
    func testDecodeBinary_stmMagic() {
        let payload = Data([0xFD] + Array(repeating: UInt8(0), count: 249))
        let frame = WSFrame.decodeBinary(payload, direction: .inbound)
        XCTAssertEqual(frame.typeLabel, "stm_frame[250B]")
    }

    /// Audio metrics frames (magic byte 0x41 — low byte of LE 0x00445541)
    /// get tagged `audio_metrics[…B]`.
    func testDecodeBinary_audioMagic() {
        let payload = Data([0x41] + Array(repeating: UInt8(0), count: 463))
        let frame = WSFrame.decodeBinary(payload, direction: .inbound)
        XCTAssertEqual(frame.typeLabel, "audio_metrics[464B]")
    }

    /// Empty / unrecognised binary frames fall back to a generic label.
    func testDecodeBinary_emptyDataNoMagic() {
        let frame = WSFrame.decodeBinary(Data(), direction: .inbound)
        XCTAssertEqual(frame.typeLabel, "binary[0B]")
        if case let .binary(byteCount, magic) = frame.payload {
            XCTAssertEqual(byteCount, 0)
            XCTAssertNil(magic)
        } else {
            XCTFail("Expected binary payload")
        }
    }

    /// Unknown magic bytes produce a hex-tagged generic label.
    func testDecodeBinary_unknownMagic() {
        let payload = Data([0x77, 0x00, 0x00])
        let frame = WSFrame.decodeBinary(payload, direction: .inbound)
        XCTAssertEqual(frame.typeLabel, "binary[0x77][3B]")
    }

    // MARK: - WSFrame summary + pretty-print

    /// Long text payloads truncate at 117 characters with a trailing "...".
    func testSummary_truncatesLongText() {
        let long = String(repeating: "a", count: 200)
        let frame = WSFrame.decodeText(long, direction: .inbound)
        XCTAssertEqual(frame.summary.count, 120)
        XCTAssertTrue(frame.summary.hasSuffix("..."))
    }

    /// Short text payloads are returned verbatim.
    func testSummary_shortTextIsVerbatim() {
        let frame = WSFrame.decodeText(#"{"type":"status"}"#, direction: .inbound)
        XCTAssertEqual(frame.summary, #"{"type":"status"}"#)
    }

    /// Binary frames produce a deterministic summary string with magic + size.
    func testSummary_binaryFormatting() {
        let frame = WSFrame.decodeBinary(Data([0xFE, 0x00]), direction: .inbound)
        XCTAssertTrue(frame.summary.contains("0xFE"))
        XCTAssertTrue(frame.summary.contains("2 B"))
    }

    /// `fullPayloadText` pretty-prints a JSON text frame.
    func testFullPayloadText_prettyPrintsJson() {
        let frame = WSFrame.decodeText(#"{"a":1,"b":2}"#, direction: .inbound)
        let pretty = frame.fullPayloadText
        XCTAssertTrue(pretty.contains("\n"), "Pretty-printed JSON should be multi-line")
        XCTAssertTrue(pretty.contains("\"a\""))
    }

    /// `fullPayloadText` returns the raw string when JSON parse fails.
    func testFullPayloadText_invalidJsonIsVerbatim() {
        let frame = WSFrame.decodeText("not json", direction: .inbound)
        XCTAssertEqual(frame.fullPayloadText, "not json")
    }

    // MARK: - WSInspector ring buffer

    /// Records survive a snapshot read and preserve insertion order.
    func testInspector_recordsAndSnapshots() async {
        let inspector = WSInspector(bufferLimit: 10)

        await inspector.recordOutboundText(#"{"type":"status.subscribe"}"#)
        await inspector.recordInboundText(#"{"type":"status","fps":120}"#)

        let snap = await inspector.snapshot()
        XCTAssertEqual(snap.count, 2)
        XCTAssertEqual(snap[0].direction, .outbound)
        XCTAssertEqual(snap[0].messageType, .unknown) // status.subscribe is not enumerated
        XCTAssertEqual(snap[1].direction, .inbound)
        XCTAssertEqual(snap[1].messageType, .status)
    }

    /// Once `bufferLimit` is exceeded the oldest frame is evicted (FIFO).
    func testInspector_ringBufferFifoEviction() async {
        let inspector = WSInspector(bufferLimit: 3)

        for i in 0..<5 {
            await inspector.recordOutboundText(#"{"type":"frame","i":\#(i)}"#)
        }

        let snap = await inspector.snapshot()
        XCTAssertEqual(snap.count, 3, "Buffer must be capped at bufferLimit")

        // Oldest two (i=0, i=1) should be evicted; remaining payloads are i=2,3,4.
        let payloadStrings: [String] = snap.compactMap { frame in
            if case let .text(text) = frame.payload { return text }
            return nil
        }
        XCTAssertTrue(payloadStrings[0].contains("\"i\":2"))
        XCTAssertTrue(payloadStrings[1].contains("\"i\":3"))
        XCTAssertTrue(payloadStrings[2].contains("\"i\":4"))
    }

    /// `clear()` empties the ring buffer.
    func testInspector_clearEmptiesBuffer() async {
        let inspector = WSInspector(bufferLimit: 5)

        await inspector.recordOutboundText(#"{"type":"a"}"#)
        await inspector.recordOutboundText(#"{"type":"b"}"#)

        var count = await inspector.count()
        XCTAssertEqual(count, 2)

        await inspector.clear()
        count = await inspector.count()
        XCTAssertEqual(count, 0)

        let snap = await inspector.snapshot()
        XCTAssertTrue(snap.isEmpty)
    }

    /// Binary captures end up in the ring buffer with a binary payload.
    func testInspector_recordInboundBinaryAppends() async {
        let inspector = WSInspector(bufferLimit: 10)

        await inspector.recordInboundBinary(Data([0xFE, 0x00, 0x00]))

        let snap = await inspector.snapshot()
        XCTAssertEqual(snap.count, 1)
        XCTAssertEqual(snap[0].direction, .inbound)
        if case let .binary(byteCount, magic) = snap[0].payload {
            XCTAssertEqual(byteCount, 3)
            XCTAssertEqual(magic, 0xFE)
        } else {
            XCTFail("Expected binary payload")
        }
    }

    /// Subscribers receive subsequent frames (not historical ones).
    func testInspector_subscriberReceivesNewFrames() async {
        let inspector = WSInspector(bufferLimit: 5)
        // Pre-existing frame that should NOT arrive on the subscription.
        await inspector.recordInboundText(#"{"type":"old"}"#)

        let stream = await inspector.subscribe()
        var iterator = stream.makeAsyncIterator()

        // Push one new frame after subscription.
        await inspector.recordInboundText(#"{"type":"status"}"#)

        let received = await iterator.next()
        XCTAssertNotNil(received)
        XCTAssertEqual(received?.messageType, .status)
    }

    /// `bufferLimit` sanitises invalid input — zero or negative caps round to 1.
    func testInspector_bufferLimitCannotBeZero() async {
        let inspector = WSInspector(bufferLimit: 0)
        XCTAssertEqual(inspector.bufferLimit, 1)

        await inspector.recordOutboundText(#"{"type":"a"}"#)
        await inspector.recordOutboundText(#"{"type":"b"}"#)
        let snap = await inspector.snapshot()
        XCTAssertEqual(snap.count, 1, "bufferLimit clamped to 1, only newest retained")
    }

    /// Subscribers stop receiving frames once the inspector is released —
    /// confirms the `onTermination` cleanup path runs without leaking.
    func testInspector_subscriberCleanupAfterCancel() async {
        let inspector = WSInspector(bufferLimit: 5)
        let stream = await inspector.subscribe()

        let task = Task {
            for await _ in stream { /* drain */ }
        }
        task.cancel()
        // Give the cancellation a beat to propagate; assert no crash.
        try? await Task.sleep(nanoseconds: 50_000_000)
        await inspector.recordOutboundText(#"{"type":"after"}"#)
        let count = await inspector.count()
        XCTAssertEqual(count, 1)
    }

    // MARK: - WSInspectorFilter

    private func makeFrame(direction: WSFrameDirection,
                           messageType: WebSocketMessageType,
                           typeLabel: String,
                           deviceId: String? = nil,
                           timestamp: Date = Date()) -> WSFrame {
        return WSFrame(
            timestamp: timestamp,
            direction: direction,
            messageType: messageType,
            typeLabel: typeLabel,
            payload: .text(#"{"type":"\#(typeLabel)"}"#),
            deviceId: deviceId
        )
    }

    /// An empty filter passes every frame through unchanged.
    func testFilter_emptyPassesAll() {
        let filter = WSInspectorFilter()
        XCTAssertTrue(filter.isEmpty)

        let frames = [
            makeFrame(direction: .inbound, messageType: .status, typeLabel: "status"),
            makeFrame(direction: .outbound, messageType: .unknown, typeLabel: "status.subscribe")
        ]
        XCTAssertEqual(filter.apply(to: frames).count, 2)
    }

    /// Direction filter eliminates the opposite direction.
    func testFilter_direction() {
        var filter = WSInspectorFilter()
        filter.direction = .inbound

        let frames = [
            makeFrame(direction: .inbound, messageType: .status, typeLabel: "status"),
            makeFrame(direction: .outbound, messageType: .unknown, typeLabel: "send")
        ]
        let out = filter.apply(to: frames)
        XCTAssertEqual(out.count, 1)
        XCTAssertEqual(out[0].direction, .inbound)
    }

    /// `messageTypes` filter is a set membership check.
    func testFilter_messageTypeSet() {
        var filter = WSInspectorFilter()
        filter.messageTypes = [.status, .beatEvent]

        let frames = [
            makeFrame(direction: .inbound, messageType: .status, typeLabel: "status"),
            makeFrame(direction: .inbound, messageType: .parametersChanged, typeLabel: "parameters.changed"),
            makeFrame(direction: .inbound, messageType: .beatEvent, typeLabel: "beat.event")
        ]
        let out = filter.apply(to: frames)
        XCTAssertEqual(out.count, 2)
        XCTAssertTrue(out.allSatisfy { $0.messageType == .status || $0.messageType == .beatEvent })
    }

    /// `typeLabels` filter exposes wire-level labels (incl. binary tags).
    func testFilter_typeLabelSet() {
        var filter = WSInspectorFilter()
        filter.typeLabels = ["led_data[966B]"]

        let frames = [
            makeFrame(direction: .inbound, messageType: .status, typeLabel: "status"),
            makeFrame(direction: .inbound, messageType: .unknown, typeLabel: "led_data[966B]")
        ]
        let out = filter.apply(to: frames)
        XCTAssertEqual(out.count, 1)
        XCTAssertEqual(out[0].typeLabel, "led_data[966B]")
    }

    /// Empty deviceIdSearch is treated as "no constraint".
    func testFilter_emptyDeviceIdNoConstraint() {
        var filter = WSInspectorFilter()
        filter.deviceIdSearch = ""

        let frames = [makeFrame(direction: .inbound, messageType: .status, typeLabel: "status")]
        XCTAssertEqual(filter.apply(to: frames).count, 1)
    }

    /// deviceIdSearch is case-insensitive substring match.
    func testFilter_deviceIdSubstringMatch() {
        var filter = WSInspectorFilter()
        filter.deviceIdSearch = "k1"

        let frames = [
            makeFrame(direction: .inbound, messageType: .status, typeLabel: "status", deviceId: "K1-AB12"),
            makeFrame(direction: .inbound, messageType: .status, typeLabel: "status", deviceId: "tab5-7"),
            makeFrame(direction: .inbound, messageType: .status, typeLabel: "status", deviceId: nil)
        ]
        let out = filter.apply(to: frames)
        XCTAssertEqual(out.count, 1)
        XCTAssertEqual(out[0].deviceId, "K1-AB12")
    }

    /// Timestamp range filters at endpoints (inclusive both ends).
    func testFilter_timestampRange() {
        let now = Date(timeIntervalSince1970: 1_000_000_000)
        let earlier = now.addingTimeInterval(-60)
        let later = now.addingTimeInterval(60)

        let frames = [
            makeFrame(direction: .inbound, messageType: .status, typeLabel: "status", timestamp: earlier),
            makeFrame(direction: .inbound, messageType: .status, typeLabel: "status", timestamp: now),
            makeFrame(direction: .inbound, messageType: .status, typeLabel: "status", timestamp: later)
        ]

        var filter = WSInspectorFilter()
        filter.startTime = now.addingTimeInterval(-30)
        filter.endTime = now.addingTimeInterval(30)
        let out = filter.apply(to: frames)
        XCTAssertEqual(out.count, 1)
        XCTAssertEqual(out[0].timestamp, now)
    }

    /// startTime alone with no endTime filters lower bound only.
    func testFilter_timestampStartOnly() {
        let now = Date()
        let earlier = now.addingTimeInterval(-60)
        let later = now.addingTimeInterval(60)

        let frames = [
            makeFrame(direction: .inbound, messageType: .status, typeLabel: "status", timestamp: earlier),
            makeFrame(direction: .inbound, messageType: .status, typeLabel: "status", timestamp: later)
        ]

        var filter = WSInspectorFilter()
        filter.startTime = now
        let out = filter.apply(to: frames)
        XCTAssertEqual(out.count, 1)
        XCTAssertEqual(out[0].timestamp, later)
    }

    /// Composing direction + messageType + deviceId filters with AND semantics.
    func testFilter_composesWithAnd() {
        var filter = WSInspectorFilter()
        filter.direction = .inbound
        filter.messageTypes = [.status]
        filter.deviceIdSearch = "K1"

        let frames = [
            makeFrame(direction: .outbound, messageType: .status, typeLabel: "status", deviceId: "K1-A"),
            makeFrame(direction: .inbound, messageType: .beatEvent, typeLabel: "beat.event", deviceId: "K1-A"),
            makeFrame(direction: .inbound, messageType: .status, typeLabel: "status", deviceId: "tab5-1"),
            makeFrame(direction: .inbound, messageType: .status, typeLabel: "status", deviceId: "K1-A")
        ]
        let out = filter.apply(to: frames)
        XCTAssertEqual(out.count, 1)
        XCTAssertEqual(out[0].deviceId, "K1-A")
        XCTAssertEqual(out[0].messageType, .status)
        XCTAssertEqual(out[0].direction, .inbound)
    }
}
