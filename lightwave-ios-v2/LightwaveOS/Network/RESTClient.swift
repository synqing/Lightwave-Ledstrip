//
//  RESTClient.swift
//  LightwaveOS
//
//  URLSession-based async REST client for ESP32 API v1.
//  Pure Apple frameworks, no third-party dependencies.
//  iOS 17+, Swift 6. British English comments.
//

import Foundation

// MARK: - API Error Types

enum APIClientError: LocalizedError {
    case connectionFailed(Error)
    case invalidResponse
    case httpError(Int, String?)
    case decodingError(Error)
    case rateLimited
    case encodingError
    /// Wire-format zoneId is out of the post-B2 firmware range (1..3).
    /// Wire value 0 is RESERVED and rejected by the firmware. Raised by
    /// `RESTClient` per-zone setters before any network round-trip.
    case invalidZoneId(Int)

    var errorDescription: String? {
        switch self {
        case .connectionFailed(let error):
            return "Connection failed: \(error.localizedDescription)"
        case .invalidResponse:
            return "Invalid server response"
        case .httpError(let code, let message):
            return "HTTP \(code): \(message ?? "Unknown error")"
        case .decodingError(let error):
            return "Failed to decode response: \(error.localizedDescription)"
        case .rateLimited:
            return "Rate limit exceeded (20 req/s)"
        case .encodingError:
            return "Failed to encode request body"
        case .invalidZoneId(let zoneId):
            return "Invalid zoneId \(zoneId): wire-format zoneId must be 1..3 (0 is reserved)"
        }
    }
}

// MARK: - Response Models

struct DeviceInfoResponse: Codable, Sendable {
    let success: Bool
    let data: DeviceInfo
    let timestamp: Int?
    let version: String?

    struct DeviceInfo: Codable, Sendable {
        let firmware: String
        let firmwareVersionNumber: Int?
        let board: String
        let sdk: String
        let flashSize: Int?
        let sketchSize: Int?
        let freeSketch: Int?
        let architecture: String?

        // Computed properties for backward compatibility
        var firmwareVersion: String { firmware }
        var sdkVersion: String { sdk }
        var freeSketchSpace: Int? { freeSketch }
    }
}

struct DeviceStatusResponse: Codable, Sendable {
    let success: Bool
    let data: DeviceStatus
    let timestamp: Int?

    struct DeviceStatus: Codable, Sendable {
        let uptime: Int
        let freeHeap: Int
        let heapSize: Int?
        let cpuFreq: Int?
        let fps: Float?
        let cpuPercent: Float?
        let framesRendered: Int?
        let network: NetworkInfo?
        let wsClients: Int?

        struct NetworkInfo: Codable, Sendable {
            let connected: Bool?
            let apMode: Bool?
            let ip: String?
            let rssi: Int?
        }
    }
}

struct EffectsResponse: Codable, Sendable {
    let success: Bool
    let data: EffectsData
    let timestamp: Int?

    struct EffectsData: Codable, Sendable {
        let effects: [Effect]
        let total: Int
        let offset: Int?
        let limit: Int?
        let pagination: Pagination?

        struct Effect: Codable, Sendable {
            let id: Int
            let name: String
            let description: String?
            let category: String?
            let categoryId: Int?
            let categoryName: String?
            let isAudioReactive: Bool?
            let centerOrigin: Bool?
            let usesSpeed: Bool?
            let usesPalette: Bool?
            let zoneAware: Bool?
            let dualStrip: Bool?
            let physicsBased: Bool?

            // MARK: Phase 2 — isExperimental field
            //
            // Firmware emits `isExperimental` per effect (see
            // `firmware-v3/src/network/webserver/handlers/EffectHandlers.cpp:173`
            // — `effect["isExperimental"] = PatternRegistry::isExperimental(eid)`).
            // Optional so legacy K1 firmware payloads (pre-flag) decode without
            // throwing; nil is treated as non-experimental at the ViewModel layer.
            let isExperimental: Bool?
        }

        struct Pagination: Codable, Sendable {
            let page: Int?
            let limit: Int?
            let total: Int?
            let pages: Int?
        }
    }
}

struct EffectDetailResponse: Codable, Sendable {
    let success: Bool
    let data: EffectDetail
    let timestamp: Int?

    struct EffectDetail: Codable, Sendable {
        let id: Int
        let name: String
        let category: String?
        let description: String?
        let parameters: [String]?
    }
}

struct PalettesResponse: Codable, Sendable {
    let success: Bool
    let data: PalettesData
    let timestamp: Int?

    struct PalettesData: Codable, Sendable {
        let palettes: [Palette]
        let total: Int?
        let offset: Int?
        let limit: Int?

        struct Palette: Codable, Sendable {
            let id: Int
            let name: String
            let category: String?
            let flags: PaletteFlags?
            let avgBrightness: Int?
            let maxBrightness: Int?

            struct PaletteFlags: Codable, Sendable {
                let warm: Bool?
                let cool: Bool?
                let calm: Bool?
                let vivid: Bool?
                let cvdFriendly: Bool?
                let whiteHeavy: Bool?
            }
        }
    }
}

struct ParametersResponse: Codable, Sendable {
    let success: Bool
    let data: Parameters
    let timestamp: Int?

    struct Parameters: Codable, Sendable {
        let brightness: Int
        let speed: Int
        let paletteId: Int?
        let mood: Int?
        let fadeAmount: Int?
        let hue: Int?
        let saturation: Int?
        let intensity: Int?
        let complexity: Int?
        let variation: Int?
    }
}

// MARK: Phase 1 — parameterType field
//
// Per-effect parameter descriptor returned by `effects.parameters` (REST and
// WebSocket). Firmware commit 4398af3b (2026-03-24) added a numeric `type`
// field to each parameter object so clients can render type-appropriate
// controls (toggles for BOOL, steppers for INT, dropdowns for ENUM, sliders
// for FLOAT) instead of treating every value as a float.
//
// Wire shape (per `docs/protocol/k1-ws-contract.yaml`):
//
//     {
//         "name": "contrast",
//         "displayName": "Contrast",
//         "min": 0.0, "max": 3.0,
//         "default": 1.0, "value": 1.0,
//         "type": 0     // uint8: 0=FLOAT, 1=INT, 2=BOOL, 3=ENUM
//     }
//
// Forward-compatibility rules enforced by the custom decoder:
//   - Legacy payloads (no `type` field) decode with `parameterType == nil`.
//   - Numeric codes 0-3 decode to their corresponding `ParameterType` case.
//   - Unknown numeric codes (firmware may grow new types) decode to
//     `.unknown` — the decoder MUST NOT throw on unrecognised codes.

/// The runtime classification of an effect parameter, used to drive
/// type-appropriate UI controls.
enum ParameterType: Int, Codable, Sendable, Equatable {
    /// Continuous value; render as a slider.
    case float = 0
    /// Discrete integer; render as a stepper.
    case int = 1
    /// Boolean flag; render as a toggle.
    case bool = 2
    /// Enumerated choice; render as a dropdown / picker.
    case enumerated = 3
    /// Code emitted by firmware that this client does not understand.
    /// Treated as untyped — UI should fall back to a slider over [min, max].
    case unknown = -1

    init(from decoder: Decoder) throws {
        let raw = try decoder.singleValueContainer().decode(Int.self)
        // Map any unrecognised code to .unknown rather than throwing — this
        // keeps iOS forward-compatible with future firmware revisions.
        self = ParameterType(rawValue: raw) ?? .unknown
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        try container.encode(rawValue)
    }
}

/// A single effect parameter descriptor, including its current value, range,
/// default, and (post-`4398af3b`) its runtime type.
struct EffectParameter: Codable, Sendable, Equatable {
    /// Programmatic key (e.g. `"contrast"`).
    let name: String
    /// Human-readable label (e.g. `"Contrast"`). Optional for tolerance against
    /// older firmware revisions that may not have populated it.
    let displayName: String?
    /// Inclusive lower bound of the value range.
    let min: Float
    /// Inclusive upper bound of the value range.
    let max: Float
    /// Default value used when the parameter is reset.
    /// Decoded from the `default` JSON key (renamed to avoid the Swift keyword).
    let defaultValue: Float
    /// Current live value as known to firmware.
    let value: Float
    /// Runtime classification — `nil` for legacy responses that pre-date the
    /// `type` field on commit 4398af3b. New responses always populate this.
    let parameterType: ParameterType?

    private enum CodingKeys: String, CodingKey {
        case name
        case displayName
        case min
        case max
        case defaultValue = "default"
        case value
        case parameterType = "type"
    }
}

/// The full envelope returned by the `effects.parameters` request — a single
/// effect's identifier, name, and the list of its tunable parameters.
struct EffectParametersGet: Codable, Sendable, Equatable {
    let effectId: Int
    let name: String
    let hasParameters: Bool
    let parameters: [EffectParameter]
}

struct ZonesResponse: Codable, Sendable {
    let success: Bool
    let data: ZonesData
    let timestamp: Int?

    struct ZonesData: Codable, Sendable {
        let enabled: Bool
        let zoneCount: Int
        let segments: [Segment]?
        let zones: [Zone]
        let presets: [ZonePreset]?

        struct Segment: Codable, Sendable {
            let zoneId: Int
            let s1LeftStart: Int
            let s1LeftEnd: Int
            let s1RightStart: Int
            let s1RightEnd: Int
            let totalLeds: Int?
        }

        struct Zone: Codable, Sendable {
            let id: Int
            let enabled: Bool?
            let effectId: Int
            let effectName: String?
            let brightness: Int?
            let speed: Int
            let paletteId: Int
            let paletteName: String?
            let blendMode: Int
            let blendModeName: String?
        }

        struct ZonePreset: Codable, Sendable {
            let id: Int
            let name: String
        }
    }
}

struct ColourCorrectionResponse: Codable, Sendable {
    let success: Bool
    let data: ColourCorrectionData
    let timestamp: Int?

    struct ColourCorrectionData: Codable, Sendable {
        let gammaEnabled: Bool?
        let gammaValue: Float?
        let autoExposureEnabled: Bool?
        let autoExposureTarget: Int?
        let brownGuardrailEnabled: Bool?
        let mode: Int?
    }
}

struct PresetsResponse: Codable, Sendable {
    let success: Bool
    let data: PresetsData
    let timestamp: Int?

    struct PresetsData: Codable, Sendable {
        let presets: [Preset]

        struct Preset: Codable, Sendable {
            let id: Int
            let name: String
        }
    }
}

struct AudioParametersResponse: Codable, Sendable {
    let success: Bool
    let data: AudioParameters
    let timestamp: Int?

    struct AudioParameters: Codable, Sendable {
        let gain: Double?
        let threshold: Double?
        let micType: Int?
    }
}

struct AudioTuningResponse: Codable, Sendable {
    let success: Bool
    let data: AudioTuningData
    let timestamp: Int?

    struct AudioTuningData: Codable, Sendable {
        let pipeline: Pipeline
        let controlBus: ControlBus
        let contract: Contract
        let state: State?

        struct Pipeline: Codable, Sendable {
            let dcAlpha: Double?
            let agcTargetRms: Double?
            let agcMinGain: Double?
            let agcMaxGain: Double?
            let agcAttack: Double?
            let agcRelease: Double?
            let agcClipReduce: Double?
            let agcIdleReturnRate: Double?
            let noiseFloorMin: Double?
            let noiseFloorRise: Double?
            let noiseFloorFall: Double?
            let gateStartFactor: Double?
            let gateRangeFactor: Double?
            let gateRangeMin: Double?
            let rmsDbFloor: Double?
            let rmsDbCeil: Double?
            let bandDbFloor: Double?
            let bandDbCeil: Double?
            let chromaDbFloor: Double?
            let chromaDbCeil: Double?
            let fluxScale: Double?
            let bandAttack: Double?
            let bandRelease: Double?
            let heavyBandAttack: Double?
            let heavyBandRelease: Double?
            let usePerBandNoiseFloor: Bool?
            let silenceHysteresisMs: Double?
            let silenceThreshold: Double?
            let perBandGains: [Double]?
            let perBandNoiseFloors: [Double]?
            let bins64Adaptive: Bins64Adaptive?
            let novelty: Novelty?

            struct Bins64Adaptive: Codable, Sendable {
                let scale: Double?
                let floor: Double?
                let rise: Double?
                let fall: Double?
                let decay: Double?
            }

            struct Novelty: Codable, Sendable {
                let useSpectralFlux: Bool?
                let spectralFluxScale: Double?
            }
        }

        struct ControlBus: Codable, Sendable {
            let alphaFast: Double?
            let alphaSlow: Double?
        }

        struct Contract: Codable, Sendable {
            let audioStalenessMs: Double?
            let bpmMin: Double?
            let bpmMax: Double?
            let bpmTau: Double?
            let confidenceTau: Double?
            let phaseCorrectionGain: Double?
            let barCorrectionGain: Double?
            let beatsPerBar: Int?
            let beatUnit: Int?
        }

        struct State: Codable, Sendable {
            let rmsRaw: Double?
            let rmsMapped: Double?
            let rmsPreGain: Double?
            let fluxMapped: Double?
            let agcGain: Double?
            let dcEstimate: Double?
            let noiseFloor: Double?
            let minSample: Double?
            let maxSample: Double?
            let peakCentered: Double?
            let meanSample: Double?
            let clipCount: Int?
        }
    }
}

struct AudioFFTResponse: Codable, Sendable {
    let success: Bool
    let data: AudioFFTData
    let timestamp: Int?

    struct AudioFFTData: Codable, Sendable {
        let rmsRaw: Double
        let rmsMapped: Double
        let rmsPreGain: Double
        let agcGain: Double
        let bands: [Double]
        let chroma: [Double]
    }
}

struct AudioTempoResponse: Codable, Sendable {
    let success: Bool
    let data: AudioTempoData
    let timestamp: Int?

    struct AudioTempoData: Codable, Sendable {
        let bpm: Double
        let confidence: Double
        let beatPhase: Double
        let barPhase: Double
        let beatInBar: Int
        let beatsPerBar: Int

        enum CodingKeys: String, CodingKey {
            case bpm
            case confidence
            case beatPhase = "beat_phase"
            case barPhase = "bar_phase"
            case beatInBar = "beat_in_bar"
            case beatsPerBar = "beats_per_bar"
        }
    }
}

struct AudioStateResponse: Codable, Sendable {
    let success: Bool
    let data: AudioStateData
    let timestamp: Int?

    struct AudioStateData: Codable, Sendable {
        let state: String
        let capturing: Bool
        let hopCount: Int
        let sampleIndex: UInt32
        let controlBus: ControlBus?

        struct ControlBus: Codable, Sendable {
            let silentScale: Double
            let isSilent: Bool
            let tempoLocked: Bool
            let tempoConfidence: Double
        }
    }
}

struct TransitionTypesResponse: Codable, Sendable {
    let success: Bool
    let data: TransitionTypesData
    let timestamp: Int?

    struct TransitionTypesData: Codable, Sendable {
        let types: [TransitionType]

        struct TransitionType: Codable, Sendable {
            let id: Int
            let name: String
        }
    }
}

struct GenericResponse: Codable, Sendable {
    let success: Bool
    let message: String?
    let timestamp: Int?
}

// MARK: - Network Endpoints (Wi-Fi setup / AP mode)

struct NetworkStatusResponse: Codable, Sendable {
    let success: Bool
    let data: NetworkStatusData
    let timestamp: Int?

    struct NetworkStatusData: Codable, Sendable {
        let connected: Bool
        let state: String?
        let apMode: Bool
        let ssid: String?
        let ip: String?
        let rssi: Int?
        let channel: Int?
        let apIP: String?
        let stats: NetworkStats?

        struct NetworkStats: Codable, Sendable {
            let connectionAttempts: Int?
            let successfulConnections: Int?
            let uptimeSeconds: Int?
        }
    }
}

struct NetworkScanResponse: Codable, Sendable {
    let success: Bool
    let data: NetworkScanData
    let timestamp: Int?

    struct NetworkScanData: Codable, Sendable {
        let networks: [ScanResult]
        let count: Int?
        let lastScanTime: Int?

        struct ScanResult: Codable, Sendable {
            let ssid: String
            let rssi: Int?
            let channel: Int?
            let encryption: String?
            let bssid: String?
        }
    }
}

struct NetworkSavedListResponse: Codable, Sendable {
    let success: Bool
    let data: NetworkSavedData
    let timestamp: Int?

    struct NetworkSavedData: Codable, Sendable {
        let networks: [String]
        let count: Int?
        let maxNetworks: Int?
    }
}

struct NetworkConnectResponse: Codable, Sendable {
    let success: Bool
    let data: NetworkConnectData?
    let timestamp: Int?

    struct NetworkConnectData: Codable, Sendable {
        let message: String?
        let ssid: String?
    }
}

// MARK: - REST Client

@available(iOS 17.0, *)
actor RESTClient {
    private let baseURL: String
    private let port: Int
    private let session: URLSession

    // MARK: - Initialization

    init(host: String, port: Int = 80) {
        self.baseURL = "http://\(host)"
        self.port = port

        let config = URLSessionConfiguration.default
        config.timeoutIntervalForRequest = 15.0
        config.timeoutIntervalForResource = 30.0
        config.requestCachePolicy = .reloadIgnoringLocalCacheData
        config.urlCache = nil

        self.session = URLSession(configuration: config)
    }

    /// Test-only initialiser allowing injection of a pre-configured `URLSession`.
    /// Production code uses `init(host:port:)`. Tests use this to wire a session
    /// whose `protocolClasses` route through a `URLProtocol` mock.
    init(host: String, port: Int = 80, session: URLSession) {
        self.baseURL = "http://\(host)"
        self.port = port
        self.session = session
    }

    // MARK: - Generic Request Method

    private func request<T: Decodable>(
        _ method: String,
        path: String,
        body: [String: Any]? = nil
    ) async throws -> T {
        let urlString = "\(baseURL):\(port)/api/v1/\(path)"
        guard let url = URL(string: urlString) else {
            throw APIClientError.invalidResponse
        }

        var request = URLRequest(url: url)
        request.httpMethod = method
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.setValue("application/json", forHTTPHeaderField: "Accept")

        if let body = body {
            do {
                request.httpBody = try JSONSerialization.data(withJSONObject: body)
            } catch {
                throw APIClientError.encodingError
            }
        }

        let (data, response) = try await session.data(for: request)

        guard let httpResponse = response as? HTTPURLResponse else {
            throw APIClientError.invalidResponse
        }

        switch httpResponse.statusCode {
        case 200...299:
            break
        case 429:
            throw APIClientError.rateLimited
        default:
            let message = String(data: data, encoding: .utf8)
            throw APIClientError.httpError(httpResponse.statusCode, message)
        }

        do {
            let decoder = JSONDecoder()
            // Device firmware sends camelCase, not snake_case
            return try decoder.decode(T.self, from: data)
        } catch {
            #if DEBUG
            if let rawString = String(data: data, encoding: .utf8) {
                print("❌ Decode failed (\(T.self)). Raw response:\n\(rawString)")
            }
            #endif
            throw APIClientError.decodingError(error)
        }
    }

    // MARK: - Device Endpoints

    func getDeviceInfo() async throws -> DeviceInfoResponse {
        try await request("GET", path: "device/info")
    }

    func getDeviceStatus() async throws -> DeviceStatusResponse {
        try await request("GET", path: "device/status")
    }

    // MARK: - Effects Endpoints

    func getEffects(page: Int = 1, limit: Int = 50, details: Bool = false) async throws -> EffectsResponse {
        var path = "effects?page=\(page)&limit=\(limit)"
        if details {
            path += "&details=true"
        }
        return try await request("GET", path: path)
    }

    func setEffect(_ id: Int) async throws {
        let _: GenericResponse = try await request("POST", path: "effects/set", body: ["effectId": id])
    }

    func getEffectMetadata(_ id: Int) async throws -> EffectDetailResponse {
        try await request("GET", path: "effects/metadata?id=\(id)")
    }

    // MARK: - Palettes Endpoints

    func getPalettes(page: Int = 1, limit: Int = 75) async throws -> PalettesResponse {
        try await request("GET", path: "palettes?page=\(page)&limit=\(limit)")
    }

    func setPalette(_ id: Int) async throws {
        let _: GenericResponse = try await request("POST", path: "palettes/set", body: ["paletteId": id])
    }

    // MARK: - Parameters Endpoints

    func getParameters() async throws -> ParametersResponse {
        try await request("GET", path: "parameters")
    }

    func setParameters(_ params: [String: Int]) async throws {
        let _: GenericResponse = try await request("POST", path: "parameters", body: params)
    }

    func setBrightness(_ value: Int) async throws {
        try await setParameters(["brightness": value])
    }

    func setSpeed(_ value: Int) async throws {
        try await setParameters(["speed": value])
    }

    func setParameter(name: String, value: Int) async throws {
        try await setParameters([name: value])
    }

    // MARK: - Zones Endpoints
    //
    // Wire-format note (2026-05-02 migration): the `zoneId` argument on each
    // setter is the 1-indexed wire identifier (1..3) and is interpolated
    // verbatim into the URL path (`/api/v1/zones/{zoneId}/...`). Firmware
    // post-B2 (`d53092ad`) accepts only 1, 2, 3 on the wire. Wire value 0
    // is RESERVED — firmware rejects it with HTTP 400 / `INVALID_VALUE`,
    // and `GET /api/v1/zones/0` returns 404 NOT_FOUND because the path
    // regex is now `[1-3]`. iOS internal `ZoneConfig.id` and
    // `ZoneSegment.zoneId` mirror the wire format so no translation step
    // is required at this boundary. The defensive guards below reject
    // out-of-range zoneIds before they reach the firmware.

    /// Validate that `zoneId` is in the 1-indexed wire range and throw
    /// `APIClientError.invalidZoneId` if not. Centralises the boundary
    /// check across all per-zone setters.
    private static func validateWireZoneId(_ zoneId: Int) throws {
        guard (1...3).contains(zoneId) else {
            throw APIClientError.invalidZoneId(zoneId)
        }
    }

    func getZones() async throws -> ZonesResponse {
        try await request("GET", path: "zones")
    }

    func setZonesEnabled(_ enabled: Bool) async throws {
        let _: GenericResponse = try await request("POST", path: "zones/enabled", body: ["enabled": enabled])
    }

    func setZoneEffect(zoneId: Int, effectId: Int) async throws {
        try Self.validateWireZoneId(zoneId)
        let _: GenericResponse = try await request("POST", path: "zones/\(zoneId)/effect", body: ["effectId": effectId])
    }

    func setZoneBrightness(zoneId: Int, brightness: Int) async throws {
        try Self.validateWireZoneId(zoneId)
        let _: GenericResponse = try await request("POST", path: "zones/\(zoneId)/brightness", body: ["brightness": brightness])
    }

    func setZoneSpeed(zoneId: Int, speed: Int) async throws {
        try Self.validateWireZoneId(zoneId)
        let _: GenericResponse = try await request("POST", path: "zones/\(zoneId)/speed", body: ["speed": speed])
    }

    func setZonePalette(zoneId: Int, paletteId: Int) async throws {
        try Self.validateWireZoneId(zoneId)
        let _: GenericResponse = try await request("POST", path: "zones/\(zoneId)/palette", body: ["paletteId": paletteId])
    }

    func setZoneBlendMode(zoneId: Int, blendMode: Int) async throws {
        try Self.validateWireZoneId(zoneId)
        let _: GenericResponse = try await request("POST", path: "zones/\(zoneId)/blend", body: ["blendMode": blendMode])
    }

    func setZoneLayout(zones: [[String: Int]]) async throws {
        // Defensive: each layout segment carries its own zoneId. Reject the
        // batch if any segment uses the reserved 0 wire value before the
        // request is dispatched.
        for seg in zones {
            if let zid = seg["zoneId"] {
                try Self.validateWireZoneId(zid)
            }
        }
        let _: GenericResponse = try await request("POST", path: "zones/layout", body: ["zones": zones])
    }

    func loadZonePreset(id: Int) async throws {
        let _: GenericResponse = try await request("POST", path: "presets/zones/\(id)/load", body: [:])
    }

    // MARK: - Transition Endpoints

    func triggerTransition(toEffect: Int, type: Int? = nil, duration: Int? = nil) async throws {
        var body: [String: Any] = ["toEffect": toEffect]
        if let type = type {
            body["type"] = type
        }
        if let duration = duration {
            body["duration"] = duration
        }
        let _: GenericResponse = try await request("POST", path: "transitions/trigger", body: body)
    }

    func getTransitionTypes() async throws -> TransitionTypesResponse {
        try await request("GET", path: "transitions/types")
    }

    // MARK: - Colour Correction Endpoints

    func getColourCorrection() async throws -> ColourCorrectionResponse {
        try await request("GET", path: "colorCorrection/config")
    }

    func setColourCorrection(
        gammaEnabled: Bool? = nil,
        gammaValue: Float? = nil,
        autoExposureEnabled: Bool? = nil,
        autoExposureTarget: Int? = nil,
        brownGuardrailEnabled: Bool? = nil,
        mode: Int? = nil
    ) async throws {
        var body: [String: Any] = [:]
        if let gammaEnabled = gammaEnabled {
            body["gammaEnabled"] = gammaEnabled
        }
        if let gammaValue = gammaValue {
            body["gammaValue"] = gammaValue
        }
        if let autoExposureEnabled = autoExposureEnabled {
            body["autoExposureEnabled"] = autoExposureEnabled
        }
        if let autoExposureTarget = autoExposureTarget {
            body["autoExposureTarget"] = autoExposureTarget
        }
        if let brownGuardrailEnabled = brownGuardrailEnabled {
            body["brownGuardrailEnabled"] = brownGuardrailEnabled
        }
        if let mode = mode {
            body["mode"] = mode
        }
        let _: GenericResponse = try await request("POST", path: "colorCorrection/config", body: body)
    }

    // MARK: - Presets Endpoints

    func getPresets() async throws -> PresetsResponse {
        try await request("GET", path: "presets")
    }

    func loadPreset(name: String) async throws {
        let encodedName = name.addingPercentEncoding(withAllowedCharacters: .urlPathAllowed) ?? name
        let _: GenericResponse = try await request("POST", path: "presets/\(encodedName)/load", body: [:])
    }

    func savePreset(name: String) async throws {
        let _: GenericResponse = try await request("POST", path: "presets/save-current", body: ["name": name])
    }

    // MARK: - Audio Endpoints

    func getAudioParameters() async throws -> AudioParametersResponse {
        try await request("GET", path: "audio/parameters")
    }

    func setAudioParameters(gain: Double? = nil, threshold: Double? = nil, micType: Int? = nil) async throws {
        var body: [String: Any] = [:]
        if let gain = gain {
            body["gain"] = gain
        }
        if let threshold = threshold {
            body["threshold"] = threshold
        }
        if let micType = micType {
            body["micType"] = micType
        }
        let _: GenericResponse = try await request("POST", path: "audio/parameters", body: body)
    }

    func getAudioTuning() async throws -> AudioTuningResponse {
        try await request("GET", path: "audio/parameters")
    }

    func patchAudioTuning(_ payload: [String: any Sendable]) async throws {
        var body: [String: Any] = [:]
        for (key, value) in payload {
            body[key] = value
        }
        let _: GenericResponse = try await request("PATCH", path: "audio/parameters", body: body)
    }

    func getAudioFFT() async throws -> AudioFFTResponse {
        try await request("GET", path: "audio/fft")
    }

    func getAudioTempo() async throws -> AudioTempoResponse {
        try await request("GET", path: "audio/tempo")
    }

    func getAudioState() async throws -> AudioStateResponse {
        try await request("GET", path: "audio/state")
    }

    // MARK: - Network Endpoints (Wi-Fi setup)

    func getNetworkStatus() async throws -> NetworkStatusResponse {
        try await request("GET", path: "network/status")
    }

    func getNetworkScan() async throws -> NetworkScanResponse {
        try await request("GET", path: "network/scan")
    }

    func getSavedNetworks() async throws -> NetworkSavedListResponse {
        try await request("GET", path: "network/networks")
    }

    func addNetwork(ssid: String, password: String) async throws {
        let _: GenericResponse = try await request("POST", path: "network/networks", body: [
            "ssid": ssid,
            "password": password
        ])
    }

    func connectToNetwork(ssid: String, password: String, save: Bool = true) async throws {
        var body: [String: Any] = ["ssid": ssid]
        if !password.isEmpty {
            body["password"] = password
        }
        body["save"] = save
        let _: NetworkConnectResponse = try await request("POST", path: "network/connect", body: body)
    }

    func disconnectFromNetwork() async throws {
        let _: GenericResponse = try await request("POST", path: "network/disconnect", body: [:])
    }

    // MARK: Phase 1 — capability discovery

    /// Probe the connected device for its advertised capabilities.
    ///
    /// Probe order:
    ///   1. `GET /api/v1/openapi.json`        — richest, contract-aware
    ///   2. `GET /api/v1/firmware/version`    — fallback, version-only
    ///   3. `nil`                              — degrade gracefully
    ///
    /// This call is BEST-EFFORT: it never throws. Any transport, status, or decode
    /// failure collapses to `nil` so that capability discovery cannot break the
    /// connect flow. AppViewModel logs a single line on `nil` and proceeds.
    func getCapabilities() async -> DeviceCapabilities? {
        if let viaOpenAPI = await fetchOpenAPICapabilities() {
            return viaOpenAPI
        }
        if let viaVersion = await fetchFirmwareVersionCapabilities() {
            return viaVersion
        }
        return nil
    }

    /// Fetch and decode `/api/v1/openapi.json`. Returns `nil` on any failure.
    private func fetchOpenAPICapabilities() async -> DeviceCapabilities? {
        guard let data = await rawGet(path: "openapi.json") else { return nil }
        return DeviceCapabilities.decodeOpenAPI(from: data)
    }

    /// Fetch and decode `/api/v1/firmware/version`. Returns `nil` on any failure.
    private func fetchFirmwareVersionCapabilities() async -> DeviceCapabilities? {
        guard let data = await rawGet(path: "firmware/version") else { return nil }
        return DeviceCapabilities.decodeFirmwareVersion(from: data)
    }

    /// Best-effort raw GET against `/api/v1/<path>`. Returns the response body on
    /// 2xx, `nil` for any non-2xx status or transport error. Never throws.
    private func rawGet(path: String) async -> Data? {
        let urlString = "\(baseURL):\(port)/api/v1/\(path)"
        guard let url = URL(string: urlString) else { return nil }

        var request = URLRequest(url: url)
        request.httpMethod = "GET"
        request.setValue("application/json", forHTTPHeaderField: "Accept")

        do {
            let (data, response) = try await session.data(for: request)
            guard let http = response as? HTTPURLResponse,
                  (200...299).contains(http.statusCode) else {
                return nil
            }
            return data
        } catch {
            return nil
        }
    }

    // MARK: Phase 2 — runtime parameter set
    //
    // End-user surface for live effect tuning. Per F-4 of the parity spec, every
    // parameter with a non-empty `displayName` is exposed via a type-appropriate
    // control: FLOAT → slider, INT → stepper, BOOL → toggle, ENUM → picker (integer
    // keyed for now since firmware does not yet emit case names). The sheet is
    // driven by these two endpoints:
    //
    //     GET  /api/v1/effects/parameters?effectId=<id>   → EffectParametersGet
    //     POST /api/v1/effects/parameters                 → {effectId, parameters: {<name>: <value>}}
    //
    // The encode helper is a pure free static so tests can lock the wire shape
    // without spinning up a URLProtocol mock.

    /// Fetch the tunable runtime parameters for a single effect.
    /// - Parameter effectId: The numeric effect identifier as returned by
    ///   `/api/v1/effects`.
    /// - Returns: The decoded `EffectParametersGet` envelope.
    func getEffectParameters(effectId: Int) async throws -> EffectParametersGet {
        // Firmware exposes the parameters list as the `data` payload of a
        // standard envelope. Decode the envelope, then return its inner value.
        struct Envelope: Codable, Sendable {
            let success: Bool
            let data: EffectParametersGet
            let timestamp: Int?
        }
        let response: Envelope = try await request(
            "GET",
            path: "effects/parameters?effectId=\(effectId)"
        )
        return response.data
    }

    /// Push a new value for a single runtime parameter.
    /// - Parameters:
    ///   - effectId: The numeric effect identifier the parameter belongs to.
    ///   - name: The programmatic parameter key (e.g. `"contrast"`).
    ///   - value: The new value. The firmware expects a numeric form regardless
    ///     of the underlying `ParameterType`; for BOOL pass `0.0` or `1.0`, for
    ///     INT/ENUM pass a whole number, for FLOAT pass any value within the
    ///     parameter's declared range.
    func setRuntimeParameter(effectId: Int, name: String, value: Double) async throws {
        let body = Self.encodeRuntimeParameterBody(
            effectId: effectId,
            name: name,
            value: value
        )
        // Reach the JSON-Serialization codepath of `request(_:path:body:)` by
        // round-tripping the bytes back into a `[String: Any]` so the body has
        // the exact shape the contract documents.
        guard let dict = try JSONSerialization.jsonObject(with: body) as? [String: Any] else {
            throw APIClientError.encodingError
        }
        let _: GenericResponse = try await request(
            "POST",
            path: "effects/parameters",
            body: dict
        )
    }

    /// Encode the runtime-set body to JSON bytes.
    ///
    /// Wire shape (per `docs/protocol/k1-rest-contract.yaml`):
    /// ```
    /// {"effectId": 4878, "parameters": {"contrast": 1.5}}
    /// ```
    ///
    /// Exposed as a `static` so unit tests can lock the shape without touching
    /// the network layer. The body is deterministic and self-contained — no
    /// shared state, no actor isolation, no URL state.
    static func encodeRuntimeParameterBody(
        effectId: Int,
        name: String,
        value: Double
    ) -> Data {
        // JSONSerialization preserves Double precision and avoids Swift's
        // `Codable` JSONEncoder ambiguity around `[String: Any]`-shaped bodies.
        let payload: [String: Any] = [
            "effectId": effectId,
            "parameters": [name: value]
        ]
        // The payload is a fixed, well-formed `[String: Any]` — JSONSerialization
        // cannot fail on it. If it ever does, surfacing an empty `Data` is a
        // strictly better failure mode than a fatal error during a slider drag.
        return (try? JSONSerialization.data(withJSONObject: payload)) ?? Data()
    }

    // MARK: Phase 2 — preset CRUD
    //
    // The preset surface uses REST for reads (list + single fetch) and WS for
    // mutations (saveCurrent / load / delete) so iOS receives the firmware's
    // broadcast confirmations (`effectPresets.saved` etc., wired in
    // `WebSocketService.swift`'s Phase 1 broadcast block).
    //
    // Endpoints (per `firmware-v3/src/network/webserver/V1ApiRoutes.cpp`
    // lines 1267-1382):
    //   GET    /api/v1/effect-presets         → list
    //   GET    /api/v1/effect-presets/get?id= → detail (unused by P2-3 today)
    //   POST   /api/v1/effect-presets/apply   → covered by WS effectPresets.load
    //   DELETE /api/v1/effect-presets/delete  → covered by WS effectPresets.delete
    //   GET    /api/v1/zone-presets           → list
    //   GET    /api/v1/zone-presets/get?id=   → detail (unused by P2-3 today)
    //   POST   /api/v1/zone-presets/apply     → covered by WS zonePresets.load
    //   DELETE /api/v1/zone-presets/delete    → covered by WS zonePresets.delete

    /// Fetch the user-saved effect-preset list.
    func getEffectPresets() async throws -> EffectPresetsListResponse {
        try await request("GET", path: "effect-presets")
    }

    /// Fetch the zone-preset list (built-in plus user-saved).
    func getZonePresets() async throws -> ZonePresetsListResponse {
        try await request("GET", path: "zone-presets")
    }

    // MARK: Phase 2 — show transport
    //
    // Read endpoints for the shows surface. Mutations (play / pause / resume /
    // stop / seek) go over WebSocket so iOS receives broadcast confirmations
    // and benefits from low-latency dispatch — see `WebSocketService` Phase 2
    // additions. Out-of-scope for Phase 2: upload, delete, cue.inject.

    /// `GET /api/v1/shows` — list built-in and uploaded shows.
    func getShows() async throws -> ShowsListResponse {
        try await request("GET", path: "shows")
    }

    /// `GET /api/v1/shows/current` — current playback frame (playing show,
    /// elapsed/duration timestamps, paused flag).
    func getCurrentShow() async throws -> CurrentShowResponse {
        try await request("GET", path: "shows/current")
    }
}

