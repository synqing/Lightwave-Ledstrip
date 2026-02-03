//
//  APIResponse.swift
//  LightwaveOS
//
//  Generic API response wrapper for v1 REST endpoints.
//

import Foundation

/// Generic wrapper for API v1 responses
struct APIResponse<T: Decodable>: Decodable, Sendable where T: Sendable {
    /// Indicates if the request was successful
    let success: Bool

    /// Response payload (nil on error)
    let data: T?

    /// Unix timestamp of the response
    let timestamp: Int?

    /// API version string
    let version: String?

    /// Error details (present when success=false)
    let error: APIError?
}

/// Error details from API responses
struct APIError: Decodable, Sendable {
    /// Error code identifier
    let code: String

    /// Human-readable error message
    let message: String
}

// MARK: - Convenience Extensions

extension APIResponse {
    /// Unwrap data or throw an error
    func unwrap() throws -> T {
        if success, let data = data {
            return data
        } else if let error = error {
            throw LightwaveAPIError.serverError(code: error.code, message: error.message)
        } else {
            throw LightwaveAPIError.invalidResponse
        }
    }
}

// MARK: - API Error Types

enum LightwaveAPIError: Error, LocalizedError {
    case invalidResponse
    case serverError(code: String, message: String)
    case networkError(Error)
    case decodingError(Error)

    var errorDescription: String? {
        switch self {
        case .invalidResponse:
            return "Invalid response from server"
        case .serverError(let code, let message):
            return "\(code): \(message)"
        case .networkError(let error):
            return "Network error: \(error.localizedDescription)"
        case .decodingError(let error):
            return "Failed to decode response: \(error.localizedDescription)"
        }
    }
}
