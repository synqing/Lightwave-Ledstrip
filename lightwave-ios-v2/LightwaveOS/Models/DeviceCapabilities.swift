//
//  DeviceCapabilities.swift
//  LightwaveOS
//
//  Phase 1 capability discovery model.
//  Captures what the connected firmware advertises at connect time so that
//  the app can detect drift instead of silently desyncing.
//
//  Sources, in priority order:
//    1. GET /api/v1/openapi.json   (richest — version + advertised paths)
//    2. GET /api/v1/firmware/version (fallback — version + build metadata)
//
//  British English in comments. iOS 17+, Swift 6.
//

import Foundation

/// What the firmware tells us about itself at connect time.
/// All fields are optional so future firmware revisions can omit any of them
/// without breaking iOS clients.
struct DeviceCapabilities: Sendable, Equatable {
    /// Firmware version string (e.g. `"v3.7.0"`).
    var firmwareVersion: String?

    /// Short git commit hash if firmware reports one (e.g. `"6404cd77"`).
    var buildHash: String?

    /// Build date string as reported by firmware (e.g. `"2026-04-30"`).
    var buildDate: String?

    /// Sorted list of REST paths advertised by `/api/v1/openapi.json`.
    /// `nil` indicates the firmware did not expose openapi.json — Phase 2 feature
    /// gates that need this should treat `nil` as "unknown, assume legacy surface".
    var availablePaths: [String]?
}

// MARK: - Decoders

extension DeviceCapabilities {

    // MARK: firmware/version DTOs

    private struct FirmwareVersionEnvelope: Decodable {
        let success: Bool?
        let data: FirmwareVersionData?
    }

    private struct FirmwareVersionData: Decodable {
        let version: String?
        let build: String?
        let buildDate: String?
    }

    /// Decode a `/api/v1/firmware/version` response into a `DeviceCapabilities`.
    /// Returns `nil` on any decode error — capability discovery is best-effort.
    static func decodeFirmwareVersion(from data: Data) -> DeviceCapabilities? {
        let decoder = JSONDecoder()
        guard let envelope = try? decoder.decode(FirmwareVersionEnvelope.self, from: data),
              let payload = envelope.data else {
            return nil
        }
        return DeviceCapabilities(
            firmwareVersion: payload.version,
            buildHash: payload.build,
            buildDate: payload.buildDate,
            availablePaths: nil
        )
    }

    // MARK: openapi.json DTOs

    private struct OpenAPIEnvelope: Decodable {
        let openapi: String?
        let info: OpenAPIInfo?
        let paths: [String: AnyCodableValue]?
    }

    private struct OpenAPIInfo: Decodable {
        let title: String?
        let version: String?
    }

    /// Codable shim that swallows arbitrary JSON values.
    /// We only care about which keys exist in `paths`, not their content.
    private struct AnyCodableValue: Decodable {
        init(from decoder: Decoder) throws {
            // Drain whatever is here without decoding it.
            _ = try? decoder.singleValueContainer()
        }
    }

    /// Decode an OpenAPI 3.x document into a `DeviceCapabilities`.
    /// Extracts `info.version` and the keys of `paths`. Returns `nil` if the JSON
    /// is malformed; returns a value with `nil` fields if those parts are missing.
    static func decodeOpenAPI(from data: Data) -> DeviceCapabilities? {
        let decoder = JSONDecoder()
        guard let envelope = try? decoder.decode(OpenAPIEnvelope.self, from: data) else {
            return nil
        }
        let pathKeys: [String]? = {
            guard let paths = envelope.paths, !paths.isEmpty else { return nil }
            return paths.keys.sorted()
        }()
        return DeviceCapabilities(
            firmwareVersion: envelope.info?.version,
            buildHash: nil,
            buildDate: nil,
            availablePaths: pathKeys
        )
    }
}
