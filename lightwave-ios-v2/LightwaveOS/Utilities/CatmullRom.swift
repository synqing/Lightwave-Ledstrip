//
//  CatmullRom.swift
//  LightwaveOS
//
//  Created by Claude on 01/02/2026.
//

import Foundation

/// Centripetal Catmull-Rom spline interpolation.
/// Takes `values` (e.g., 8 frequency bands) and produces `outputCount` smoothly interpolated points.
///
/// This function pads the input array with duplicate endpoints and applies the standard Catmull-Rom
/// formula to create a smooth curve. The centripetal variant ensures no cusps or self-intersections.
///
/// - Parameters:
///   - values: Input control points (minimum 2 for meaningful interpolation)
///   - outputCount: Number of output samples (e.g., 64 for smooth curves)
/// - Returns: Array of interpolated Float values, clamped to 0...1 range
func catmullRom(values: [Float], outputCount: Int) -> [Float] {
    // Edge case: invalid output count
    guard outputCount > 0 else {
        return []
    }

    // Edge case: insufficient input values
    guard values.count >= 2 else {
        let fallback = values.first ?? 0.0
        return Array(repeating: fallback, count: outputCount)
    }

    // Pad the input array: prepend first value, append last value
    // This ensures we have control points before and after each segment
    var paddedValues = values
    paddedValues.insert(values.first!, at: 0)
    paddedValues.append(values.last!)

    var output = [Float]()
    output.reserveCapacity(outputCount)

    // Number of segments in the original (unpadded) array
    let segmentCount = values.count - 1

    for i in 0..<outputCount {
        // Map output index to position across the input range
        // We interpolate across the original values, not the padded array
        let normalised = Float(i) / Float(max(1, outputCount - 1))
        let position = normalised * Float(segmentCount)

        // Find which segment this position falls into
        let segmentIndex = min(Int(position), segmentCount - 1)

        // Local t within the segment [0, 1]
        let t = position - Float(segmentIndex)

        // Get the 4 control points (P0, P1, P2, P3)
        // In the padded array, segment 0 uses indices 0, 1, 2, 3
        // Segment n uses indices n, n+1, n+2, n+3
        let p0 = paddedValues[segmentIndex]
        let p1 = paddedValues[segmentIndex + 1]
        let p2 = paddedValues[segmentIndex + 2]
        let p3 = paddedValues[segmentIndex + 3]

        // Apply Catmull-Rom formula
        // result = 0.5 * (a + b*t + c*t² + d*t³)
        let a = 2.0 * p1
        let b = p2 - p0
        let c = 2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3
        let d = -p0 + 3.0 * p1 - 3.0 * p2 + p3

        let t2 = t * t
        let t3 = t2 * t

        let interpolated = 0.5 * (a + b * t + c * t2 + d * t3)

        // Clamp to 0...1 range for audio visualisation
        let clamped = max(0.0, min(1.0, interpolated))
        output.append(clamped)
    }

    return output
}
