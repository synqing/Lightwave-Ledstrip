//
//  DotRidgeCanvas.swift
//  LightwaveOS
//
//  SwiftUI Canvas-based "dot ridge" audio visualisation.
//  Renders dots at frequency band positions with elevation from amplitude,
//  creating a 3D-ish ridge plot effect.
//

import SwiftUI

// MARK: - DotRidgeCanvas

/// Dot ridge audio visualisation for 8-band spectrum data
///
/// Displays a 3D-ish ridge plot where:
/// - 8 columns represent frequency bands (Sub to Air, left to right)
/// - 35 rows represent history frames (newest bottom, oldest top)
/// - Dot elevation is driven by band intensity
/// - Background lattice shows base dot positions
/// - Connecting lines between adjacent dots create ridge effect
/// - Age fade applied to older frames (back to front rendering)
struct DotRidgeCanvas: View {
    @Environment(AudioViewModel.self) private var audio

    let palette: PaletteLUT

    // MARK: - Constants

    private let numLines = 35
    private let bands = 8

    // MARK: - Body

    var body: some View {
        TimelineView(.animation) { timeline in
            Canvas { ctx, size in
                // Fill background with lwBase
                ctx.fill(
                    Path(CGRect(origin: .zero, size: size)),
                    with: .color(Color.lwBase)
                )

                // Calculate spacing
                let lineSpacing = size.height / CGFloat(numLines + 3)
                let bandSpacing = size.width / CGFloat(bands - 1)

                // Step 1: Draw background lattice dots
                drawBackgroundLattice(
                    ctx: &ctx,
                    size: size,
                    lineSpacing: lineSpacing,
                    bandSpacing: bandSpacing
                )

                // Step 2: Draw active dots (BACK-TO-FRONT, i.e., oldest first)
                drawActiveDots(
                    ctx: &ctx,
                    size: size,
                    lineSpacing: lineSpacing,
                    bandSpacing: bandSpacing
                )
            }
        }
    }

    // MARK: - Drawing Helpers

    /// Draw background lattice dots at base positions
    private func drawBackgroundLattice(
        ctx: inout GraphicsContext,
        size: CGSize,
        lineSpacing: CGFloat,
        bandSpacing: CGFloat
    ) {
        for line in 0..<numLines {
            let baseY = size.height - CGFloat(numLines - line) * lineSpacing

            for band in 0..<bands {
                let x = CGFloat(band) * bandSpacing

                // Draw small circle at base position
                let circle = Path(
                    ellipseIn: CGRect(
                        x: x - 1.5,
                        y: baseY - 1.5,
                        width: 3.0,
                        height: 3.0
                    )
                )

                ctx.fill(circle, with: .color(Color.white.opacity(0.04)))
            }
        }
    }

    /// Draw active dots with elevation, glow, and connecting lines
    private func drawActiveDots(
        ctx: inout GraphicsContext,
        size: CGSize,
        lineSpacing: CGFloat,
        bandSpacing: CGFloat
    ) {
        // Render back-to-front (oldest first) for proper layering
        for line in stride(from: numLines - 1, through: 0, by: -1) {
            // Calculate ring buffer index for this line
            let idx = (audio.writeRow - 1 - line + 60) % 60

            // Guard against invalid indices
            guard idx >= 0 && idx < audio.ringBuffer.count else { continue }
            let row = audio.ringBuffer[idx]

            // Calculate age factor for this line
            let age = Float(line) / Float(numLines)  // 0 = newest, 1 = oldest
            let fade = pow(1.0 - age, 1.5)

            let baseY = size.height - CGFloat(numLines - line) * lineSpacing

            // Store dot positions for connecting lines
            var dotPositions: [(x: CGFloat, y: CGFloat, val: Float)] = []

                // Draw dots for each band
                for band in 0..<bands {
                    guard band < row.count else { continue }

                    let val = max(0.0, row[band])
                    // Boost low-level detail a bit so the ridge actually moves with quiet material.
                    let shaped = pow(val, 0.70)
                    let elevation = CGFloat(shaped * 42.0 * fade)
                    let y = baseY - elevation
                    let x = CGFloat(band) * bandSpacing
                    let radius = 1.2 + CGFloat(shaped * 4.0 * fade)

                    // Store position for connecting lines
                    dotPositions.append((x: x, y: y, val: shaped))

                    // Glow effect (only for high values on recent lines)
                    if shaped > 0.28 && line < 10 {
                        let glowCircle = Path(
                            ellipseIn: CGRect(
                                x: x - (radius + 3),
                                y: y - (radius + 3),
                                width: (radius + 3) * 2,
                                height: (radius + 3) * 2
                            )
                        )

                        let glowColour = palette.color(at: shaped * fade)
                        ctx.fill(glowCircle, with: .color(glowColour.opacity(0.08 * Double(fade))))
                    }

                // Core dot
                let dotCircle = Path(
                    ellipseIn: CGRect(
                        x: x - radius,
                        y: y - radius,
                        width: radius * 2,
                        height: radius * 2
                    )
                )

                    let dotColour = palette.color(at: shaped * fade)
                    let dotOpacity = 0.3 + 0.7 * Double(fade)
                    ctx.fill(dotCircle, with: .color(dotColour.opacity(dotOpacity)))
                }

            // Draw connecting lines between adjacent dots
            if fade > 0.2 {
                for i in 0..<(dotPositions.count - 1) {
                    let from = dotPositions[i]
                    let to = dotPositions[i + 1]

                    // Average value for line colour
                    let avgVal = (from.val + to.val) / 2.0
                    let lineColour = palette.color(at: avgVal * fade)

                    var linePath = Path()
                    linePath.move(to: CGPoint(x: from.x, y: from.y))
                    linePath.addLine(to: CGPoint(x: to.x, y: to.y))

                    ctx.stroke(
                        linePath,
                        with: .color(lineColour.opacity(0.15 * Double(fade))),
                        lineWidth: 0.5
                    )
                }
            }
        }
    }
}

// MARK: - Preview

#Preview("Dot Ridge Canvas") {
    let audio = AudioViewModel()

    // Pre-populate ringBuffer with mock beat pattern data
    for i in 0..<60 {
        var frame: [Float] = []
        for b in 0..<8 {
            // Create beat pulse pattern (every 15 frames)
            let beat = (i % 15 < 3) ? Float(0.8) : Float(0.0)

            // Add sine wave variation for visual interest
            let baseValue = sin(Float(i) * 0.2 + Float(b) * 0.8) * 0.5 + 0.3

            // Combine beat with base variation
            let value = max(beat, baseValue)
            frame.append(value)
        }
        audio.ringBuffer[i] = frame
    }
    audio.writeRow = 0

    return VStack(spacing: Spacing.md) {
        // Viridis palette
        VStack(alignment: .leading, spacing: Spacing.xs) {
            Text("Viridis")
                .font(.caption)
                .foregroundStyle(Color.lwTextSecondary)

            DotRidgeCanvas(palette: .viridis)
                .frame(height: 220)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        }

        // Plasma palette
        VStack(alignment: .leading, spacing: Spacing.xs) {
            Text("Plasma")
                .font(.caption)
                .foregroundStyle(Color.lwTextSecondary)

            DotRidgeCanvas(palette: .plasma)
                .frame(height: 220)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        }

        // Sunset Real palette
        VStack(alignment: .leading, spacing: Spacing.xs) {
            Text("Sunset Real")
                .font(.caption)
                .foregroundStyle(Color.lwTextSecondary)

            DotRidgeCanvas(palette: .sunsetReal)
                .frame(height: 220)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        }

        // Berlin palette
        VStack(alignment: .leading, spacing: Spacing.xs) {
            Text("Berlin")
                .font(.caption)
                .foregroundStyle(Color.lwTextSecondary)

            DotRidgeCanvas(palette: .berlin)
                .frame(height: 220)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        }
    }
    .padding(Spacing.lg)
    .background(Color.lwBase)
    .environment(audio)
}
