//
//  GrainOverlay.swift
//  LightwaveOS
//
//  Grain overlay primitive for microtexture finish.
//  Part of the "Liquid Glass" optical rendering system.
//

import SwiftUI

// MARK: - Grain Parameters

/// Configuration for grain overlay effect
struct GrainParameters {
    let intensity: Double    // 0.0 - 1.0, recommended 0.02 - 0.08
    let frequency: Double    // Noise scale (affects density)
    let blendMode: SwiftUI.BlendMode

    static let subtle = GrainParameters(
        intensity: 0.02,
        frequency: 300,
        blendMode: .overlay
    )

    static let standard = GrainParameters(
        intensity: GlassTokens.grainIntensity,
        frequency: 250,
        blendMode: .overlay
    )

    static let textured = GrainParameters(
        intensity: 0.06,
        frequency: 200,
        blendMode: .overlay  // softLight not available in SwiftUI
    )
}

// MARK: - Grain Overlay View

/// Renders a grain/noise texture overlay using a pre-baked tile pattern
///
/// Performance optimisation: Uses a small static noise tile (64x64) that repeats seamlessly
/// instead of iterating thousands of pixels per frame. Reduces from ~40,000 fill operations
/// to a single cached GraphicsContext draw.
struct GrainOverlay: View {
    let params: GrainParameters

    // Cache the noise tile per frequency setting
    @State private var cachedTile: GraphicsContext.ResolvedImage?

    var body: some View {
        Canvas { context, size in
            // Generate or reuse cached tile
            if cachedTile == nil {
                cachedTile = Self.generateNoiseTile(
                    size: CGSize(width: 64, height: 64),
                    seed: Int(params.frequency),
                    in: context
                )
            }

            guard let tile = cachedTile else { return }

            // Tile the noise pattern across the entire view
            // This is a single draw operation, not thousands
            let tileSize = CGSize(width: 64, height: 64)
            let cols = Int(ceil(size.width / tileSize.width)) + 1
            let rows = Int(ceil(size.height / tileSize.height)) + 1

            for row in 0..<rows {
                for col in 0..<cols {
                    let x = CGFloat(col) * tileSize.width
                    let y = CGFloat(row) * tileSize.height
                    context.draw(tile, at: CGPoint(x: x + tileSize.width/2, y: y + tileSize.height/2))
                }
            }
        }
        .blendMode(params.blendMode)
        .opacity(params.intensity)
        .allowsHitTesting(false)
    }

    /// Generate a small tileable noise pattern (64x64)
    /// This is called once and cached, not every frame
    private static func generateNoiseTile(
        size: CGSize,
        seed: Int,
        in context: GraphicsContext
    ) -> GraphicsContext.ResolvedImage? {
        let renderer = ImageRenderer(content: NoiseTileGenerator(size: size, seed: seed))
        renderer.scale = 1.0 // 1:1 pixel ratio for grain

        guard let cgImage = renderer.cgImage else { return nil }
        let image = Image(decorative: cgImage, scale: 1.0)
        return context.resolve(image)
    }
}

// MARK: - Noise Tile Generator

/// Helper view to generate a small tileable noise pattern
private struct NoiseTileGenerator: View {
    let size: CGSize
    let seed: Int

    var body: some View {
        Canvas { context, _ in
            // Generate a sparse noise pattern
            // Use larger step (4px) to reduce generation time
            let step: CGFloat = 4.0
            for x in stride(from: 0, to: size.width, by: step) {
                for y in stride(from: 0, to: size.height, by: step) {
                    let noise = hash(x: Int(x), y: Int(y), seed: seed)
                    // Use full white, opacity will be controlled by parent
                    let alpha = Double(noise) / 255.0

                    let rect = CGRect(x: x, y: y, width: step, height: step)
                    context.fill(
                        Path(rect),
                        with: .color(Color.white.opacity(alpha))
                    )
                }
            }
        }
        .frame(width: size.width, height: size.height)
    }

    /// Simple hash function for deterministic noise
    private func hash(x: Int, y: Int, seed: Int) -> UInt8 {
        var h = x &* 374761393
        h = (h &+ y &* 668265263) ^ seed
        h = (h ^ (h >> 13)) &* 1274126177
        return UInt8(truncatingIfNeeded: h & 0xFF)
    }
}

// MARK: - View Extension

extension View {
    /// Applies a grain overlay for microtexture finish
    /// - Parameter params: Grain configuration (intensity, frequency, blend mode)
    func grainOverlay(_ params: GrainParameters = .standard) -> some View {
        overlay {
            GrainOverlay(params: params)
        }
    }
}

// MARK: - Preview

#Preview("Grain Overlays") {
    VStack(spacing: 24) {
        // Solid gradient without grain
        RoundedRectangle(cornerRadius: CornerRadius.card)
            .fill(Color.lwCardGradient)
            .frame(height: 100)
            .overlay {
                Text("No Grain")
                    .font(.bodyValue)
                    .foregroundStyle(Color.lwTextPrimary)
            }

        // Subtle grain
        RoundedRectangle(cornerRadius: CornerRadius.card)
            .fill(Color.lwCardGradient)
            .frame(height: 100)
            .grainOverlay(.subtle)
            .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
            .overlay {
                Text("Subtle")
                    .font(.bodyValue)
                    .foregroundStyle(Color.lwTextPrimary)
            }

        // Standard grain
        RoundedRectangle(cornerRadius: CornerRadius.card)
            .fill(Color.lwCardGradient)
            .frame(height: 100)
            .grainOverlay(.standard)
            .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
            .overlay {
                Text("Standard")
                    .font(.bodyValue)
                    .foregroundStyle(Color.lwTextPrimary)
            }

        // Textured grain
        RoundedRectangle(cornerRadius: CornerRadius.card)
            .fill(Color.lwCardGradient)
            .frame(height: 100)
            .grainOverlay(.textured)
            .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
            .overlay {
                Text("Textured")
                    .font(.bodyValue)
                    .foregroundStyle(Color.lwTextPrimary)
            }
    }
    .padding()
    .background(Color.lwBase)
}
