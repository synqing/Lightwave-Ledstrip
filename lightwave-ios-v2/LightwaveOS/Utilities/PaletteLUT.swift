//
//  PaletteLUT.swift
//  LightwaveOS
//
//  Colour palette lookup tables for audio spectrum visualisations.
//  Provides 4 scientific palettes (Viridis, Plasma, Sunset Real, Berlin) as 256-entry RGB tables.
//

import SwiftUI

// MARK: - PaletteLUT

/// Colour palette represented as a 256-entry RGB lookup table
struct PaletteLUT: Sendable {
    let name: String
    let lut: [SIMD3<UInt8>]  // 256 RGB entries

    // MARK: - Initialisation

    init(name: String, lut: [SIMD3<UInt8>]) {
        precondition(lut.count == 256, "LUT must have exactly 256 entries")
        self.name = name
        self.lut = lut
    }

    // MARK: - Colour Sampling

    /// Get SwiftUI Color at normalised position t (0...1)
    func color(at t: Float) -> Color {
        let rgb = self.rgb(at: t)
        return Color(
            red: Double(rgb.x) / 255.0,
            green: Double(rgb.y) / 255.0,
            blue: Double(rgb.z) / 255.0
        )
    }

    /// Get raw RGB at normalised position t (0...1), clamped to valid range
    func rgb(at t: Float) -> SIMD3<UInt8> {
        let index = Int(max(0, min(255, t * 255)))
        return lut[index]
    }

    // MARK: - LUT Construction

    /// Build 256-entry LUT from gradient stops
    /// Each stop: [position (0-255), R, G, B]
    static func buildLUT(stops: [[UInt8]]) -> [SIMD3<UInt8>] {
        precondition(stops.count >= 2, "Need at least 2 gradient stops")
        precondition(stops.allSatisfy { $0.count == 4 }, "Each stop must have [pos, R, G, B]")

        var lut: [SIMD3<UInt8>] = []
        lut.reserveCapacity(256)

        for i in 0...255 {
            let position = UInt8(i)

            // Find the two stops that bracket this position
            var stopA = stops[0]
            var stopB = stops[stops.count - 1]

            for j in 0..<(stops.count - 1) {
                if position >= stops[j][0] && position <= stops[j + 1][0] {
                    stopA = stops[j]
                    stopB = stops[j + 1]
                    break
                }
            }

            // Linear interpolation between stops
            let posA = Float(stopA[0])
            let posB = Float(stopB[0])
            let t: Float

            if posB > posA {
                t = (Float(position) - posA) / (posB - posA)
            } else {
                t = 0.0  // Same position, use stopA
            }

            // Interpolate RGB components
            let r = UInt8(Float(stopA[1]) + t * Float(Int(stopB[1]) - Int(stopA[1])))
            let g = UInt8(Float(stopA[2]) + t * Float(Int(stopB[2]) - Int(stopA[2])))
            let b = UInt8(Float(stopA[3]) + t * Float(Int(stopB[3]) - Int(stopA[3])))

            lut.append(SIMD3<UInt8>(r, g, b))
        }

        return lut
    }

    // MARK: - Static Palettes

    /// Viridis: Perceptually uniform, blue to yellow-green
    static let viridis: PaletteLUT = {
        let stops: [[UInt8]] = [
            [0, 68, 1, 84],
            [36, 71, 39, 117],
            [73, 62, 74, 137],
            [109, 49, 104, 142],
            [146, 38, 130, 142],
            [182, 53, 183, 121],
            [219, 144, 215, 67],
            [255, 253, 231, 37]
        ]
        return PaletteLUT(name: "Viridis", lut: buildLUT(stops: stops))
    }()

    /// Plasma: High contrast, purple to orange-yellow
    static let plasma: PaletteLUT = {
        let stops: [[UInt8]] = [
            [0, 13, 8, 135],
            [36, 75, 3, 161],
            [73, 126, 3, 168],
            [109, 168, 34, 150],
            [146, 203, 70, 121],
            [182, 229, 107, 93],
            [219, 248, 149, 64],
            [255, 240, 249, 33]
        ]
        return PaletteLUT(name: "Plasma", lut: buildLUT(stops: stops))
    }()

    /// Sunset Real: Dark red to bright orange
    static let sunsetReal: PaletteLUT = {
        let stops: [[UInt8]] = [
            [0, 120, 0, 0],
            [22, 179, 22, 0],
            [51, 255, 104, 0],
            [85, 167, 22, 18],
            [135, 100, 0, 103],
            [198, 16, 0, 130],
            [255, 0, 0, 160]
        ]
        return PaletteLUT(name: "Sunset Real", lut: buildLUT(stops: stops))
    }()

    /// Berlin: Cool blue to warm pink
    static let berlin: PaletteLUT = {
        let stops: [[UInt8]] = [
            [0, 121, 171, 237],
            [36, 54, 133, 173],
            [73, 29, 76, 98],
            [109, 16, 26, 32],
            [146, 25, 11, 9],
            [182, 90, 28, 6],
            [219, 156, 79, 61],
            [255, 221, 141, 134]
        ]
        return PaletteLUT(name: "Berlin", lut: buildLUT(stops: stops))
    }()

    /// All palettes in cycle order
    static let all: [PaletteLUT] = [
        viridis,
        plasma,
        sunsetReal,
        berlin
    ]
}
