//
//  GlassSurface.swift
//  LightwaveOS
//
//  Glass surface primitive providing material blur foundation.
//  Part of the "Liquid Glass" optical rendering system.
//

import SwiftUI

// MARK: - Glass Surface Style

/// Elevation-based glass surface styles
enum GlassSurfaceStyle {
    case base       // Lowest elevation - subtle blur
    case card       // Standard card surface
    case elevated   // Modal/sheet surfaces
    case hero       // LED preview, BPM hero

    var material: Material {
        switch self {
        case .base:     return .ultraThinMaterial
        case .card:     return .thinMaterial
        case .elevated: return .regularMaterial
        case .hero:     return .thickMaterial
        }
    }

    var backgroundOpacity: Double {
        switch self {
        case .base:     return 0.5
        case .card:     return 0.6
        case .elevated: return 0.7
        case .hero:     return 0.8
        }
    }
}

// MARK: - Glass Surface Modifier

/// Applies a glass surface effect with material blur and tinted background
struct GlassSurfaceModifier: ViewModifier {
    let style: GlassSurfaceStyle
    let cornerRadius: CGFloat

    func body(content: Content) -> some View {
        content
            .background {
                RoundedRectangle(cornerRadius: cornerRadius)
                    .fill(style.material)
                    .background(
                        RoundedRectangle(cornerRadius: cornerRadius)
                            .fill(Color.lwGlassTint.opacity(style.backgroundOpacity))
                    )
            }
    }
}

// MARK: - View Extension

extension View {
    /// Applies a glass surface with material blur foundation
    /// - Parameters:
    ///   - style: Elevation-based surface style
    ///   - cornerRadius: Corner radius for the surface
    func glassSurface(
        style: GlassSurfaceStyle = .card,
        cornerRadius: CGFloat = CornerRadius.card
    ) -> some View {
        modifier(GlassSurfaceModifier(style: style, cornerRadius: cornerRadius))
    }
}

// MARK: - Preview

#Preview("Glass Surfaces") {
    VStack(spacing: 24) {
        ForEach([GlassSurfaceStyle.base, .card, .elevated, .hero], id: \.self) { style in
            Text(String(describing: style))
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
                .padding()
                .frame(maxWidth: .infinity)
                .glassSurface(style: style, cornerRadius: CornerRadius.card)
        }
    }
    .padding()
    .background(Color.lwBase)
}
