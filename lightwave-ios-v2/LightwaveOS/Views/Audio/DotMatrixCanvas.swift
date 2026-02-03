//
//  DotMatrixCanvas.swift
//  LightwaveOS
//
//  SwiftUI Canvas-based dot matrix audio visualisation.
//  Renders an 8×40 grid where dot SIZE represents amplitude.
//

import SwiftUI

// MARK: - DotMatrixCanvas

/// Dot matrix visualisation for 8-band audio spectrum data
///
/// Displays a scrolling dot matrix where:
/// - 8 rows represent frequency bands (Sub to Air, bottom to top)
/// - 40 columns represent history frames (newest right, oldest left)
/// - Dot size varies with band intensity (30% to 100% of max radius)
/// - Background lattice shows all grid positions at low opacity
/// - Colours are mapped via PaletteLUT with age-based fade
struct DotMatrixCanvas: View {
    @Environment(AudioViewModel.self) private var audio

    let palette: PaletteLUT

    // MARK: - Constants

    private let cols = 40
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

                // Calculate cell dimensions
                let cellW = size.width / CGFloat(cols)
                let cellH = size.height / CGFloat(bands)
                let maxR = min(cellW, cellH) * 0.35

                // Step 1: Background lattice dots
                drawBackgroundLattice(ctx: &ctx, size: size, cellW: cellW, cellH: cellH)

                // Step 2: Active dots
                drawActiveDots(ctx: &ctx, size: size, cellW: cellW, cellH: cellH, maxR: maxR)
            }
        }
    }

    // MARK: - Background Lattice

    private func drawBackgroundLattice(
        ctx: inout GraphicsContext,
        size: CGSize,
        cellW: CGFloat,
        cellH: CGFloat
    ) {
        for col in 0..<cols {
            for band in 0..<bands {
                let cx = (CGFloat(col) + 0.5) * cellW
                let cy = (CGFloat(bands - 1 - band) + 0.5) * cellH  // Band 0 at bottom

                let circle = Path { path in
                    path.addEllipse(in: CGRect(
                        x: cx - 1.5,
                        y: cy - 1.5,
                        width: 3.0,
                        height: 3.0
                    ))
                }

                ctx.fill(circle, with: .color(Color.white.opacity(0.05)))
            }
        }
    }

    // MARK: - Active Dots

    private func drawActiveDots(
        ctx: inout GraphicsContext,
        size: CGSize,
        cellW: CGFloat,
        cellH: CGFloat,
        maxR: CGFloat
    ) {
        for col in 0..<cols {
            // Map column to ring buffer index (newest on RIGHT)
            let histIdx = (audio.writeRow - 1 - (cols - 1 - col) + 60) % 60

            // Age fade: newer frames are brighter
            let age = Float(cols - 1 - col) / Float(cols)
            let fade = pow(1.0 - age, 1.5)

            // Access ring buffer row (guard against invalid index)
            guard histIdx >= 0 && histIdx < audio.ringBuffer.count else { continue }
            let row = audio.ringBuffer[histIdx]

            // Render each band in this column
            for band in 0..<bands {
                guard band < row.count else { continue }

                // Get band value and apply shaping
                let val = row[band]
                let shaped = pow(val, 1.15)  // Slight contrast boost

                // Apply age fade
                let brightness = shaped * fade

                // Calculate dot radius (minimum 30% of maxR)
                let r = maxR * (0.3 + 0.7 * CGFloat(brightness))

                // Map to palette colour
                let colour = palette.color(at: brightness)

                // Calculate dot centre position
                let cx = (CGFloat(col) + 0.5) * cellW
                let cy = (CGFloat(bands - 1 - band) + 0.5) * cellH  // Band 0 at bottom

                // Draw circle
                let circle = Path { path in
                    path.addEllipse(in: CGRect(
                        x: cx - r,
                        y: cy - r,
                        width: r * 2,
                        height: r * 2
                    ))
                }

                ctx.fill(circle, with: .color(colour))
            }
        }
    }
}

// MARK: - Preview

#Preview("Dot Matrix Canvas") {
    let audio = AudioViewModel()

    // Pre-populate ringBuffer with mock sine wave data
    for i in 0..<60 {
        var frame: [Float] = []
        for b in 0..<8 {
            // Generate varied patterns using sine waves
            let value = sin(Float(i) * 0.12 + Float(b) * 0.6) * 0.5 + 0.4
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

            DotMatrixCanvas(palette: .viridis)
                .frame(height: 200)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        }

        // Plasma palette
        VStack(alignment: .leading, spacing: Spacing.xs) {
            Text("Plasma")
                .font(.caption)
                .foregroundStyle(Color.lwTextSecondary)

            DotMatrixCanvas(palette: .plasma)
                .frame(height: 200)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        }

        // Sunset Real palette
        VStack(alignment: .leading, spacing: Spacing.xs) {
            Text("Sunset Real")
                .font(.caption)
                .foregroundStyle(Color.lwTextSecondary)

            DotMatrixCanvas(palette: .sunsetReal)
                .frame(height: 200)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        }

        // Berlin palette
        VStack(alignment: .leading, spacing: Spacing.xs) {
            Text("Berlin")
                .font(.caption)
                .foregroundStyle(Color.lwTextSecondary)

            DotMatrixCanvas(palette: .berlin)
                .frame(height: 200)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        }
    }
    .padding(Spacing.lg)
    .background(Color.lwBase)
    .environment(audio)
}
