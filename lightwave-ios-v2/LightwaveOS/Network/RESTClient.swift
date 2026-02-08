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

// MARK: - Effect Preset Response Models

struct EffectPresetsResponse: Codable, Sendable {
    let success: Bool
    let data: EffectPresetsData
    let timestamp: Int?

    struct EffectPresetsData: Codable, Sendable {
        let presets: [EffectPresetSummary]
        let count: Int?
    }
}

/// Simple preset model for REST API list responses
/// Note: Use EffectPreset from Models/ for full preset data with parameters
struct EffectPresetSummary: Codable, Sendable, Identifiable, Hashable {
    let id: Int
    let name: String
    let effectId: Int
    let paletteId: Int
    let timestamp: Int?

    var formattedDate: String? {
        guard let timestamp = timestamp else { return nil }
        let date = Date(timeIntervalSince1970: TimeInterval(timestamp))
        let formatter = DateFormatter()
        formatter.dateStyle = .medium
        formatter.timeStyle = .short
        return formatter.string(from: date)
    }
}

struct EffectPresetDetailResponse: Codable, Sendable {
    let success: Bool
    let data: EffectPresetDetail
    let timestamp: Int?

    struct EffectPresetDetail: Codable, Sendable {
        let id: Int
        let name: String
        let effectId: Int
        let paletteId: Int
        let brightness: Int
        let speed: Int
        let parameters: EffectPresetParameters?
        let timestamp: Int?

        struct EffectPresetParameters: Codable, Sendable {
            let mood: Int?
            let trails: Int?
            let hue: Int?
            let saturation: Int?
            let intensity: Int?
            let complexity: Int?
            let variation: Int?
        }
    }
}

// MARK: - Zone Preset Response Models

struct ZonePresetsResponse: Codable, Sendable {
    let success: Bool
    let data: ZonePresetsData
    let timestamp: Int?

    struct ZonePresetsData: Codable, Sendable {
        let presets: [ZonePresetSummary]
        let count: Int?
    }
}

struct ZonePresetSummary: Codable, Sendable, Identifiable, Hashable {
    let id: Int
    let name: String
    let zoneCount: Int
    let timestamp: Int?

    var formattedDate: String? {
        guard let timestamp = timestamp else { return nil }
        let date = Date(timeIntervalSince1970: TimeInterval(timestamp))
        let formatter = DateFormatter()
        formatter.dateStyle = .medium
        formatter.timeStyle = .short
        return formatter.string(from: date)
    }
}

struct ZonePresetDetailResponse: Codable, Sendable {
    let success: Bool
    let data: ZonePresetDetail
    let timestamp: Int?

    struct ZonePresetDetail: Codable, Sendable {
        let id: Int
        let name: String
        let zoneCount: Int
        let zones: [ZonePresetEntry]
        let timestamp: Int?

        struct ZonePresetEntry: Codable, Sendable {
            let effectId: Int
            let paletteId: Int
            let brightness: Int
            let speed: Int
            let blendMode: Int
            let segments: ZoneSegments

            struct ZoneSegments: Codable, Sendable {
                let s1LeftStart: Int
                let s1LeftEnd: Int
                let s1RightStart: Int
                let s1RightEnd: Int
            }
        }
    }
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

// MARK: - Shows Endpoints

struct ShowsResponse: Codable, Sendable {
    let success: Bool
    let data: ShowsData
    let timestamp: Int?

    struct ShowsData: Codable, Sendable {
        let shows: [ShowMetadata]
        let count: Int?

        struct ShowMetadata: Codable, Sendable, Identifiable {
            let id: Int
            let name: String
            let duration: Int  // milliseconds
            let looping: Bool
            let stepCount: Int?

            var formattedDuration: String {
                let seconds = duration / 1000
                let minutes = seconds / 60
                let remainingSeconds = seconds % 60
                return String(format: "%d:%02d", minutes, remainingSeconds)
            }
        }
    }
}

struct CurrentShowResponse: Codable, Sendable {
    let success: Bool
    let data: CurrentShowData
    let timestamp: Int?

    struct CurrentShowData: Codable, Sendable {
        let playing: Bool
        let paused: Bool?
        let showId: Int?
        let showName: String?
        let duration: Int?
        let elapsed: Int?
        let remaining: Int?
        let progress: Double?  // 0.0-1.0
        let chapter: Int?
        let chapterName: String?
        let tension: Int?
        let looping: Bool?
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

    func getZones() async throws -> ZonesResponse {
        try await request("GET", path: "zones")
    }

    func setZonesEnabled(_ enabled: Bool) async throws {
        let _: GenericResponse = try await request("POST", path: "zones/enabled", body: ["enabled": enabled])
    }

    func setZoneEffect(zoneId: Int, effectId: Int) async throws {
        let _: GenericResponse = try await request("POST", path: "zones/\(zoneId)/effect", body: ["effectId": effectId])
    }

    func setZoneBrightness(zoneId: Int, brightness: Int) async throws {
        let _: GenericResponse = try await request("POST", path: "zones/\(zoneId)/brightness", body: ["brightness": brightness])
    }

    func setZoneSpeed(zoneId: Int, speed: Int) async throws {
        let _: GenericResponse = try await request("POST", path: "zones/\(zoneId)/speed", body: ["speed": speed])
    }

    func setZonePalette(zoneId: Int, paletteId: Int) async throws {
        let _: GenericResponse = try await request("POST", path: "zones/\(zoneId)/palette", body: ["paletteId": paletteId])
    }

    func setZoneBlendMode(zoneId: Int, blendMode: Int) async throws {
        let _: GenericResponse = try await request("POST", path: "zones/\(zoneId)/blend", body: ["blendMode": blendMode])
    }

    func setZoneLayout(zones: [[String: Int]]) async throws {
        let _: GenericResponse = try await request("POST", path: "zones/layout", body: ["zones": zones])
    }

    func loadZonePreset(id: Int) async throws {
        let _: GenericResponse = try await request("POST", path: "zones/preset", body: ["presetId": id])
    }

    func setZoneCount(_ count: Int) async throws {
        let _: GenericResponse = try await request("POST", path: "zones/count", body: ["count": count])
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

    func loadPreset(id: Int) async throws {
        let _: GenericResponse = try await request("POST", path: "presets/\(id)/load", body: [:])
    }

    func savePreset(name: String) async throws {
        let _: GenericResponse = try await request("POST", path: "presets/save-current", body: ["name": name])
    }

    func deletePreset(id: Int) async throws {
        let _: GenericResponse = try await request("DELETE", path: "presets/\(id)")
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

    func patchAudioTuning(_ payload: [String: Any]) async throws {
        let _: GenericResponse = try await request("PATCH", path: "audio/parameters", body: payload)
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

    // MARK: - Effect Preset Endpoints

    /// List all effect presets
    func getEffectPresets() async throws -> EffectPresetsResponse {
        try await request("GET", path: "presets/effects")
    }

    /// Get effect preset detail by ID
    func getEffectPreset(id: Int) async throws -> EffectPresetDetailResponse {
        try await request("GET", path: "presets/effects/\(id)")
    }

    /// Save current effect as a preset
    /// - Parameters:
    ///   - slot: Preset slot (0-15)
    ///   - name: User-friendly preset name
    func saveEffectPreset(slot: Int, name: String) async throws {
        let _: GenericResponse = try await request("POST", path: "presets/effects/save-current", body: [
            "slot": slot,
            "name": name
        ])
    }

    /// Load an effect preset by ID
    func loadEffectPreset(id: Int) async throws {
        let _: GenericResponse = try await request("POST", path: "presets/effects/\(id)/load", body: [:])
    }

    /// Delete an effect preset by ID
    func deleteEffectPreset(id: Int) async throws {
        let _: GenericResponse = try await request("DELETE", path: "presets/effects/\(id)")
    }

    // MARK: - Zone Preset Endpoints

    /// List all zone presets
    func getZonePresets() async throws -> ZonePresetsResponse {
        try await request("GET", path: "presets/zones")
    }

    /// Get zone preset detail by ID
    func getZonePreset(id: Int) async throws -> ZonePresetDetailResponse {
        try await request("GET", path: "presets/zones/\(id)")
    }

    /// Save current zone configuration as a preset
    /// - Parameters:
    ///   - slot: Preset slot (0-15)
    ///   - name: User-friendly preset name
    func saveZonePreset(slot: Int, name: String) async throws {
        let _: GenericResponse = try await request("POST", path: "presets/zones/save-current", body: [
            "slot": slot,
            "name": name
        ])
    }

    /// Load a zone preset by ID (applies zone configuration)
    func loadZonePresetById(id: Int) async throws {
        let _: GenericResponse = try await request("POST", path: "presets/zones/\(id)/load", body: [:])
    }

    /// Delete a zone preset by ID
    func deleteZonePreset(id: Int) async throws {
        let _: GenericResponse = try await request("DELETE", path: "presets/zones/\(id)")
    }

    // MARK: - Shows Endpoints

    /// List all available shows
    func getShows() async throws -> ShowsResponse {
        try await request("GET", path: "shows")
    }

    /// Get current show playback status
    func getCurrentShow() async throws -> CurrentShowResponse {
        try await request("GET", path: "shows/current")
    }

    /// Start playing a show by ID
    func playShow(id: Int) async throws {
        let _: GenericResponse = try await request("POST", path: "shows/control", body: [
            "action": "start",
            "showId": id
        ])
    }

    /// Pause current show (toggle pause/resume)
    func pauseShow() async throws {
        let _: GenericResponse = try await request("POST", path: "shows/control", body: [
            "action": "pause"
        ])
    }

    /// Stop current show
    func stopShow() async throws {
        let _: GenericResponse = try await request("POST", path: "shows/control", body: [
            "action": "stop"
        ])
    }

    /// Seek to position in current show
    func seekShow(timeMs: Int) async throws {
        let _: GenericResponse = try await request("POST", path: "shows/control", body: [
            "action": "seek",
            "timeMs": timeMs
        ])
    }
}
