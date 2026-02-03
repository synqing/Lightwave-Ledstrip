//
//  AudioMetricsFrame.swift
//  LightwaveOS
//
//  Binary frame parser for 464-byte WebSocket audio metrics frames
//  from ESP32-S3 LED controller.
//

import Foundation

// MARK: - AudioMetricsFrame

/// Represents a single 464-byte audio metrics frame from the ESP32-S3 controller.
///
/// Frame layout (little-endian):
/// - Header: magic (4), hopSeq (4), timestamp (4)
/// - RMS & flux: rms, flux, fastRMS, fastFlux (16 bytes)
/// - Frequency analysis: bands[8], heavyBands[8] (64 bytes)
/// - Chroma: chroma[12], heavyChroma[12] (96 bytes)
/// - Waveform: 128 × Int16 → normalised to Float (256 bytes)
/// - Beat tracking: bpmSmoothed, tempoConfidence, beatPhase01, beatTick, downbeatTick (14 bytes)
struct AudioMetricsFrame: Sendable {

    // MARK: - Constants

    /// Magic bytes: 0x00445541 (little-endian "AUD\0")
    static let magic: UInt32 = 0x00445541

    /// Expected frame size in bytes
    static let frameSize: Int = 464

    // MARK: - Properties

    /// Hop sequence number (increments each frame)
    private(set) var hopSeq: UInt32 = 0

    /// Timestamp in milliseconds
    private(set) var timestamp: UInt32 = 0

    /// RMS energy (root mean square amplitude)
    private(set) var rms: Float = 0

    /// Spectral flux (rate of change in spectrum)
    private(set) var flux: Float = 0

    /// Fast-response RMS (shorter averaging window)
    private(set) var fastRMS: Float = 0

    /// Fast-response spectral flux
    private(set) var fastFlux: Float = 0

    /// Goertzel frequency bands [Sub, Bass, Low, LMid, Mid, HMid, High, Air]
    private(set) var bands: [Float] = []

    /// Heavy-weighted frequency bands (emphasises percussive content)
    private(set) var heavyBands: [Float] = []

    /// Chroma vector (12 pitch classes, C through B)
    private(set) var chroma: [Float] = []

    /// Heavy-weighted chroma vector
    private(set) var heavyChroma: [Float] = []

    /// Normalised waveform samples (128 samples, -1.0 to 1.0)
    private(set) var waveform: [Float] = []

    /// Smoothed BPM estimate
    private(set) var bpmSmoothed: Float = 0

    /// Confidence in tempo estimate (0.0 to 1.0)
    private(set) var tempoConfidence: Float = 0

    /// Beat phase in 0.0 to 1.0 range (wraps at each beat)
    private(set) var beatPhase01: Float = 0

    /// Beat tick (1 on beat, 0 otherwise)
    private(set) var beatTick: UInt8 = 0

    /// Downbeat tick (1 on downbeat, 0 otherwise)
    private(set) var downbeatTick: UInt8 = 0

    // MARK: - Initialisation

    /// Creates an audio metrics frame from binary data.
    ///
    /// - Parameter data: 464-byte binary frame with little-endian fields
    /// - Returns: Parsed frame, or `nil` if validation fails
    init?(data: Data) {
        guard data.count == Self.frameSize else {
            return nil
        }

        let magicValue = data.withUnsafeBytes { $0.load(fromByteOffset: 0, as: UInt32.self) }
        guard magicValue == Self.magic else {
            return nil
        }

        data.withUnsafeBytes { buffer in
            // Header
            self.hopSeq = buffer.load(fromByteOffset: 4, as: UInt32.self)
            self.timestamp = buffer.load(fromByteOffset: 8, as: UInt32.self)

            // RMS & Flux
            self.rms = buffer.load(fromByteOffset: 12, as: Float.self)
            self.flux = buffer.load(fromByteOffset: 16, as: Float.self)
            self.fastRMS = buffer.load(fromByteOffset: 20, as: Float.self)
            self.fastFlux = buffer.load(fromByteOffset: 24, as: Float.self)

            // Frequency bands
            self.bands = Self.readFloatArray(from: data, offset: 28, count: 8)
            self.heavyBands = Self.readFloatArray(from: data, offset: 60, count: 8)

            // Chroma
            self.chroma = Self.readFloatArray(from: data, offset: 92, count: 12)
            self.heavyChroma = Self.readFloatArray(from: data, offset: 140, count: 12)

            // Waveform (Int16 → Float, normalised to -1.0...1.0)
            self.waveform = Self.readInt16ArrayAsFloat(from: data, offset: 192, count: 128)

            // Beat tracking
            self.bpmSmoothed = buffer.load(fromByteOffset: 448, as: Float.self)
            self.tempoConfidence = buffer.load(fromByteOffset: 452, as: Float.self)
            self.beatPhase01 = buffer.load(fromByteOffset: 456, as: Float.self)
            self.beatTick = buffer.load(fromByteOffset: 460, as: UInt8.self)
            self.downbeatTick = buffer.load(fromByteOffset: 461, as: UInt8.self)
        }
    }

    // MARK: - Private Helpers

    /// Reads a contiguous array of Float values from binary data.
    ///
    /// - Parameters:
    ///   - data: Source binary data
    ///   - offset: Starting byte offset
    ///   - count: Number of Float elements to read
    /// - Returns: Array of Float values
    private static func readFloatArray(from data: Data, offset: Int, count: Int) -> [Float] {
        data.withUnsafeBytes { buffer in
            let floatBuffer = UnsafeBufferPointer(
                start: buffer.baseAddress!.advanced(by: offset).assumingMemoryBound(to: Float.self),
                count: count
            )
            return Array(floatBuffer)
        }
    }

    /// Reads an array of Int16 values and normalises them to Float (-1.0 to 1.0).
    ///
    /// - Parameters:
    ///   - data: Source binary data
    ///   - offset: Starting byte offset
    ///   - count: Number of Int16 elements to read
    /// - Returns: Normalised Float array
    private static func readInt16ArrayAsFloat(from data: Data, offset: Int, count: Int) -> [Float] {
        data.withUnsafeBytes { buffer in
            let int16Buffer = UnsafeBufferPointer(
                start: buffer.baseAddress!.advanced(by: offset).assumingMemoryBound(to: Int16.self),
                count: count
            )
            return int16Buffer.map { Float($0) / Float(Int16.max) }
        }
    }
}

// MARK: - Mock Data

extension AudioMetricsFrame {
    /// Mock frame for previews and testing.
    ///
    /// Contains sine-wave frequency bands and zero waveform.
    static let mock: AudioMetricsFrame = {
        var data = Data(count: frameSize)

        data.withUnsafeMutableBytes { buffer in
            // Magic
            buffer.storeBytes(of: magic, toByteOffset: 0, as: UInt32.self)

            // Header
            buffer.storeBytes(of: UInt32(1), toByteOffset: 4, as: UInt32.self) // hopSeq
            buffer.storeBytes(of: UInt32(1000), toByteOffset: 8, as: UInt32.self) // timestamp

            // RMS & Flux
            buffer.storeBytes(of: Float(0.5), toByteOffset: 12, as: Float.self) // rms
            buffer.storeBytes(of: Float(0.3), toByteOffset: 16, as: Float.self) // flux
            buffer.storeBytes(of: Float(0.6), toByteOffset: 20, as: Float.self) // fastRMS
            buffer.storeBytes(of: Float(0.4), toByteOffset: 24, as: Float.self) // fastFlux

            // Bands (sine wave across 8 bands)
            for i in 0..<8 {
                let value = Float(sin(Double(i) * .pi / 4.0)) * 0.5 + 0.5
                buffer.storeBytes(of: value, toByteOffset: 28 + i * 4, as: Float.self)
            }

            // Heavy bands (offset sine wave)
            for i in 0..<8 {
                let value = Float(sin(Double(i) * .pi / 4.0 + .pi / 8.0)) * 0.4 + 0.4
                buffer.storeBytes(of: value, toByteOffset: 60 + i * 4, as: Float.self)
            }

            // Chroma (12 pitch classes, sine pattern)
            for i in 0..<12 {
                let value = Float(sin(Double(i) * .pi / 6.0)) * 0.3 + 0.3
                buffer.storeBytes(of: value, toByteOffset: 92 + i * 4, as: Float.self)
            }

            // Heavy chroma (offset pattern)
            for i in 0..<12 {
                let value = Float(sin(Double(i) * .pi / 6.0 + .pi / 12.0)) * 0.25 + 0.25
                buffer.storeBytes(of: value, toByteOffset: 140 + i * 4, as: Float.self)
            }

            // Waveform (zeros)
            for i in 0..<128 {
                buffer.storeBytes(of: Int16(0), toByteOffset: 192 + i * 2, as: Int16.self)
            }

            // Beat tracking
            buffer.storeBytes(of: Float(120.0), toByteOffset: 448, as: Float.self) // bpmSmoothed
            buffer.storeBytes(of: Float(0.85), toByteOffset: 452, as: Float.self) // tempoConfidence
            buffer.storeBytes(of: Float(0.25), toByteOffset: 456, as: Float.self) // beatPhase01
            buffer.storeBytes(of: UInt8(0), toByteOffset: 460, as: UInt8.self) // beatTick
            buffer.storeBytes(of: UInt8(1), toByteOffset: 461, as: UInt8.self) // downbeatTick
        }

        return AudioMetricsFrame(data: data)!
    }()
}
