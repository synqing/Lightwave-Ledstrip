//
//  WSFrame.swift
//  LightwaveOS
//
//  Decoded WebSocket frame model used by the in-app WS Inspector.
//  iOS 17+, Swift 6 with strict concurrency. British English comments.
//
//  A frame captures one logical WebSocket message — inbound or outbound —
//  along with sufficient metadata for filtering, search and display in the
//  debug panel. Frames are immutable value types and are safe to cross actor
//  boundaries (`Sendable`).
//

import Foundation

// MARK: - Direction

/// Direction of travel for a captured frame.
///
/// The two cases mirror the WebSocket peer relationship: `outbound` is sent by
/// this iOS client to the K1 device; `inbound` is received from the device.
enum WSFrameDirection: String, Sendable, CaseIterable {
    case inbound
    case outbound
}

// MARK: - Payload Kind

/// Underlying payload representation of a captured frame.
///
/// Text frames carry JSON in the iOS protocol. Binary frames carry one of three
/// streams: LED data (magic 0xFE), audio metrics (magic 0x00445541) or STM
/// (magic 0xFD). The inspector preserves the raw byte count + magic byte for
/// binary frames rather than the full payload — capturing 30 FPS LED frames
/// at full fidelity would burn memory for no debugging value.
enum WSFramePayload: Sendable, Equatable {
    case text(String)
    case binary(byteCount: Int, magicByte: UInt8?)
}

// MARK: - WSFrame

/// A single captured WebSocket frame, decoded and tagged for inspection.
///
/// Frames are produced by hooks in `WebSocketService` and published into the
/// `WSInspector` ring buffer. The struct is intentionally lightweight — heavy
/// fields (raw payloads, binary buffers) are stored compactly so a 200-entry
/// ring buffer remains under ~50 KB even when filled with verbose JSON.
struct WSFrame: Identifiable, Sendable, Equatable {
    /// Stable identity for SwiftUI list diffing.
    let id: UUID

    /// Wall-clock timestamp at the moment the frame was observed.
    let timestamp: Date

    /// Direction (inbound / outbound).
    let direction: WSFrameDirection

    /// Decoded WebSocket message type (when the JSON `type` field matches a
    /// known case) or `.unknown` for unrecognised text frames and all binary
    /// frames. The inspector exposes this for chip-based filtering.
    let messageType: WebSocketMessageType

    /// Convenience accessor for the wire-level type string. Falls back to a
    /// stable label for binary frames so filter chips can group them.
    let typeLabel: String

    /// Underlying payload (text JSON or binary metadata).
    let payload: WSFramePayload

    /// Optional client / device identifier extracted from the JSON payload.
    /// The firmware emits `deviceId` on most broadcasts; we also accept
    /// `clientId` for inbound acks. `nil` when neither is present.
    let deviceId: String?

    // MARK: - Initialisers

    init(id: UUID = UUID(),
         timestamp: Date = Date(),
         direction: WSFrameDirection,
         messageType: WebSocketMessageType,
         typeLabel: String,
         payload: WSFramePayload,
         deviceId: String?) {
        self.id = id
        self.timestamp = timestamp
        self.direction = direction
        self.messageType = messageType
        self.typeLabel = typeLabel
        self.payload = payload
        self.deviceId = deviceId
    }

    // MARK: - Convenience constructors

    /// Decode an outbound text frame from a JSON-serialised string. The
    /// `type` field, if present, drives `messageType` and `typeLabel`.
    /// Falls back gracefully when the text is not valid JSON — used by
    /// the inspector hook to capture the raw send even if downstream
    /// decoding would fail.
    static func decodeText(_ text: String, direction: WSFrameDirection) -> WSFrame {
        let parsed = parseTypeAndDeviceId(from: text)
        return WSFrame(
            direction: direction,
            messageType: parsed.messageType,
            typeLabel: parsed.typeLabel,
            payload: .text(text),
            deviceId: parsed.deviceId
        )
    }

    /// Decode a binary frame from raw `Data`. The magic byte is preserved so
    /// the inspector can label LED / audio / STM streams without retaining
    /// the full payload.
    static func decodeBinary(_ data: Data, direction: WSFrameDirection) -> WSFrame {
        let magic = data.first
        return WSFrame(
            direction: direction,
            messageType: .unknown,
            typeLabel: binaryLabel(for: magic, byteCount: data.count),
            payload: .binary(byteCount: data.count, magicByte: magic),
            deviceId: nil
        )
    }

    // MARK: - Internal parsing

    /// Inspect a raw JSON string and pull out `type` + `deviceId` / `clientId`.
    /// Used at frame-capture time so the inspector does not need to re-parse
    /// the same JSON on every filter pass.
    static func parseTypeAndDeviceId(from text: String) -> (messageType: WebSocketMessageType, typeLabel: String, deviceId: String?) {
        guard let data = text.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return (.unknown, "raw-text", nil)
        }

        let typeString = (json["type"] as? String) ?? "unknown"
        let messageType = WebSocketMessageType(rawValue: typeString) ?? .unknown
        let deviceId = (json["deviceId"] as? String) ?? (json["clientId"] as? String)
        return (messageType, typeString, deviceId)
    }

    /// Stable label for a binary frame given its magic byte. Mirrors the
    /// magic-byte tags used in `WebSocketService.handleBinaryMessage`.
    static func binaryLabel(for magic: UInt8?, byteCount: Int) -> String {
        guard let magic = magic else { return "binary[\(byteCount)B]" }
        switch magic {
        case 0xFE:
            return "led_data[\(byteCount)B]"
        case 0xFD:
            return "stm_frame[\(byteCount)B]"
        case 0x41:
            // Low byte of little-endian 0x00445541 (audio metrics magic).
            return "audio_metrics[\(byteCount)B]"
        default:
            return String(format: "binary[0x%02X][%dB]", magic, byteCount)
        }
    }
}

// MARK: - Display helpers

extension WSFrame {

    /// Single-line summary for the list cell. Truncated for readability.
    var summary: String {
        switch payload {
        case .text(let json):
            // Strip whitespace and truncate to keep list rows compact.
            let stripped = json.replacingOccurrences(of: "\n", with: " ")
                               .trimmingCharacters(in: .whitespacesAndNewlines)
            if stripped.count <= 120 {
                return stripped
            }
            let endIndex = stripped.index(stripped.startIndex, offsetBy: 117)
            return String(stripped[..<endIndex]) + "..."
        case .binary(let byteCount, let magic):
            if let magic = magic {
                return String(format: "binary frame, magic=0x%02X, size=%d B", magic, byteCount)
            }
            return "binary frame, size=\(byteCount) B"
        }
    }

    /// Full payload text for the detail view. Pretty-prints JSON when possible.
    var fullPayloadText: String {
        switch payload {
        case .text(let raw):
            if let data = raw.data(using: .utf8),
               let object = try? JSONSerialization.jsonObject(with: data),
               let pretty = try? JSONSerialization.data(withJSONObject: object,
                                                         options: [.prettyPrinted, .sortedKeys]),
               let prettyString = String(data: pretty, encoding: .utf8) {
                return prettyString
            }
            return raw
        case .binary(let byteCount, let magic):
            if let magic = magic {
                return String(format: "<binary frame: magic=0x%02X, size=%d bytes>", magic, byteCount)
            }
            return "<binary frame: size=\(byteCount) bytes>"
        }
    }
}
