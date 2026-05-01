//
//  STMFrame.swift
//  LightwaveOS
//
//  Phase 2 — Task P2-2: STM (Spectral-Temporal Modulation) binary frame decoder.
//
//  Wire format (sourced from firmware):
//    `firmware-v3/src/network/webserver/StmStreamBroadcaster.h` lines 38-59
//    `firmware-v3/src/network/webserver/StmStreamBroadcaster.cpp` lines 78-115
//
//  Frame layout (little-endian, 250 bytes total, broadcast at ~30 FPS):
//    [0]      uint8   magic = 0xFD (distinct from LED 0xFE and audio metrics 0x41)
//    [1]      uint8   ready flag (1 = data is current, 0 = warming up)
//    [2..]    42 × float32  spectral bins   (168 bytes)
//    [..]     16 × float32  temporal bands  (64 bytes)
//    [..]     float32 spectralEnergy
//    [..]     float32 temporalEnergy
//    [..]     float32 spectralCentroid (bin-weighted)
//    [..]     float32 dominantBin (encoded as float32 from uint8)
//
//  Total = 1 + 1 + (42*4) + (16*4) + (4*4) = 250 bytes.
//  British English in comments throughout.
//

import Foundation

// MARK: - STMFrame

/// Decoded STM (Spectral-Temporal Modulation) frame from the firmware's
/// STM broadcaster. Sent on the WebSocket as a binary message at ~30 FPS
/// when a client has called `stm.subscribe`.
struct STMFrame: Sendable {

    // MARK: Wire-format constants

    /// Magic byte distinguishing STM frames from LED stream (0xFE) and audio metrics (0x41).
    static let magic: UInt8 = 0xFD

    /// Number of spectral bins (firmware: `ControlBusFrame::STM_SPECTRAL_BINS`).
    static let spectralBinCount: Int = 42

    /// Number of temporal bands (firmware: `ControlBusFrame::STM_MEL_BANDS`).
    static let temporalBandCount: Int = 16

    /// Total frame size in bytes.
    /// = 1 (magic) + 1 (ready) + 42*4 + 16*4 + 4*4 = 250.
    static let frameSize: Int =
        1 +
        1 +
        (spectralBinCount * MemoryLayout<Float>.size) +
        (temporalBandCount * MemoryLayout<Float>.size) +
        (4 * MemoryLayout<Float>.size)

    // MARK: Decoded fields

    /// True when the firmware's STM stage has produced valid data this frame.
    let ready: Bool

    /// Spectral magnitudes per bin, length = `spectralBinCount`.
    let spectral: [Float]

    /// Temporal-modulation envelope per mel band, length = `temporalBandCount`.
    let temporal: [Float]

    /// Sum of spectral magnitudes (firmware-side aggregate).
    let spectralEnergy: Float

    /// Sum of temporal magnitudes (firmware-side aggregate).
    let temporalEnergy: Float

    /// Bin-weighted spectral centroid (`Σ i*spectral[i] / Σ spectral[i]`).
    /// Computed by the firmware on every broadcast.
    let spectralCentroid: Float

    /// Index of the spectral bin with the largest magnitude. Encoded as
    /// `float32` on the wire even though it is conceptually a `uint8` in
    /// the range `0..<spectralBinCount`.
    let dominantBin: Float

    // MARK: Initialisation

    /// Decode a 250-byte STM frame. Returns `nil` if the buffer is the wrong
    /// size or the magic byte does not match.
    init?(data: Data) {
        guard data.count == STMFrame.frameSize else { return nil }
        guard data.first == STMFrame.magic else { return nil }

        let spectralBytes = STMFrame.spectralBinCount * MemoryLayout<Float>.size
        let temporalBytes = STMFrame.temporalBandCount * MemoryLayout<Float>.size

        let spectralOffset = 2
        let temporalOffset = spectralOffset + spectralBytes
        let scalarOffset   = temporalOffset + temporalBytes

        let readyFlag = data[1] != 0
        let spectral  = STMFrame.readFloatArray(from: data, offset: spectralOffset, count: STMFrame.spectralBinCount)
        let temporal  = STMFrame.readFloatArray(from: data, offset: temporalOffset, count: STMFrame.temporalBandCount)

        let spectralEnergy   = STMFrame.readFloat(from: data, offset: scalarOffset)
        let temporalEnergy   = STMFrame.readFloat(from: data, offset: scalarOffset + 4)
        let spectralCentroid = STMFrame.readFloat(from: data, offset: scalarOffset + 8)
        let dominantBin      = STMFrame.readFloat(from: data, offset: scalarOffset + 12)

        self.ready = readyFlag
        self.spectral = spectral
        self.temporal = temporal
        self.spectralEnergy = spectralEnergy
        self.temporalEnergy = temporalEnergy
        self.spectralCentroid = spectralCentroid
        self.dominantBin = dominantBin
    }

    // MARK: Helpers

    private static func readFloat(from data: Data, offset: Int) -> Float {
        data.withUnsafeBytes { buffer in
            buffer.loadUnaligned(fromByteOffset: offset, as: Float.self)
        }
    }

    private static func readFloatArray(from data: Data, offset: Int, count: Int) -> [Float] {
        var out = [Float](repeating: 0, count: count)
        data.withUnsafeBytes { buffer in
            for i in 0..<count {
                out[i] = buffer.loadUnaligned(fromByteOffset: offset + i * MemoryLayout<Float>.size, as: Float.self)
            }
        }
        return out
    }
}

// MARK: - Test helpers

extension STMFrame {
    /// Encode a synthetic STM frame for unit-test fixtures. Produces a 250-byte
    /// `Data` buffer matching the firmware wire format byte-for-byte.
    static func encodeForTesting(
        ready: Bool,
        spectral: [Float],
        temporal: [Float],
        spectralEnergy: Float,
        temporalEnergy: Float,
        spectralCentroid: Float,
        dominantBin: Float
    ) -> Data {
        precondition(spectral.count == STMFrame.spectralBinCount,
                     "spectral array must have exactly \(STMFrame.spectralBinCount) entries")
        precondition(temporal.count == STMFrame.temporalBandCount,
                     "temporal array must have exactly \(STMFrame.temporalBandCount) entries")

        var out = Data(count: STMFrame.frameSize)
        out.withUnsafeMutableBytes { rawBuf in
            guard let base = rawBuf.baseAddress else { return }

            // Magic + ready flag.
            base.storeBytes(of: STMFrame.magic, toByteOffset: 0, as: UInt8.self)
            base.storeBytes(of: UInt8(ready ? 1 : 0), toByteOffset: 1, as: UInt8.self)

            var cursor = 2
            for v in spectral {
                base.storeBytes(of: v, toByteOffset: cursor, as: Float.self)
                cursor += MemoryLayout<Float>.size
            }
            for v in temporal {
                base.storeBytes(of: v, toByteOffset: cursor, as: Float.self)
                cursor += MemoryLayout<Float>.size
            }
            base.storeBytes(of: spectralEnergy,   toByteOffset: cursor + 0,  as: Float.self)
            base.storeBytes(of: temporalEnergy,   toByteOffset: cursor + 4,  as: Float.self)
            base.storeBytes(of: spectralCentroid, toByteOffset: cursor + 8,  as: Float.self)
            base.storeBytes(of: dominantBin,      toByteOffset: cursor + 12, as: Float.self)
        }
        return out
    }

    /// Mock frame for SwiftUI previews and exploratory tests.
    static let mock: STMFrame = {
        let data = STMFrame.encodeForTesting(
            ready: true,
            spectral: (0..<STMFrame.spectralBinCount).map { i in
                let phase = Float(i) * .pi / Float(STMFrame.spectralBinCount)
                return abs(sin(phase)) * 0.8
            },
            temporal: (0..<STMFrame.temporalBandCount).map { i in
                let phase = Float(i) * .pi / Float(STMFrame.temporalBandCount)
                return abs(cos(phase)) * 0.6
            },
            spectralEnergy: 12.5,
            temporalEnergy: 7.25,
            spectralCentroid: 18.0,
            dominantBin: 17.0
        )
        return STMFrame(data: data)!
    }()
}
