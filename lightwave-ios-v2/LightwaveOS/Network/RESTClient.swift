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
}
