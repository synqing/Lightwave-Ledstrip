//
//  GlowStack.swift
//  LightwaveOS
//
//  Multi-layer glow primitive with compositingGroup support.
//  Part of the "Liquid Glass" optical rendering system.
//

import SwiftUI

// MARK: - Glow Level

/// Intensity levels for glow effects
/// Three-layer system: specular core + mid bloom + outer halo
enum GlowLevel {
    case subtle     // Always-on ambient
    case standard   // Hover/focus state
    case intense    // Active/selected state
    case beat       // Beat-reactive pulse

    // MARK: - Layer A: Specular Core (tight, bright energy)

    var coreRadius: CGFloat {
        switch self {
        case .subtle:   return 2
        case .standard: return 3
        case .intense:  return 4
        case .beat:     return 4
        }
    }

    var coreOpacity: Double {
        switch self {
        case .subtle:   return 0.15
        case .standard: return 0.18
        case .intense:  return 0.20
        case .beat:     return 0.20
        }
    }

    // MARK: - Layer B: Mid Bloom (main glow body)

    var midRadius: CGFloat {
        switch self {
        case .subtle:   return 8
        case .standard: return 10
        case .intense:  return 12
        case .beat:     return 12
        }
    }

    var midOpacity: Double {
        switch self {
        case .subtle:   return 0.25
        case .standard: return 0.30
        case .intense:  return 0.35
        case .beat:     return 0.40
        }
    }

    // MARK: - Layer C: Outer Halo (soft bloom, only for .intense and .beat)

    var outerRadius: CGFloat {
        switch self {
        case .subtle:   return 0  // No outer layer
        case .standard: return 0  // No outer layer
        case .intense:  return 20
        case .beat:     return 24
        }
    }

    var outerOpacity: Double {
        switch self {
        case .subtle:   return 0
        case .standard: return 0
        case .intense:  return 0.20
        case .beat:     return 0.25
        }
    }
}

// MARK: - Glow Stack Modifier

/// Applies a three-layer "instrument jewel" glow effect
/// - Layer A: Tight specular core for energy
/// - Layer B: Main glow body
/// - Layer C: Soft outer bloom (only for .intense and .beat)
struct GlowStackModifier: ViewModifier {
    let level: GlowLevel
    let colour: Color

    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    func body(content: Content) -> some View {
        content
            .compositingGroup()
            // Layer A: Specular core
            .shadow(
                color: reduceMotion ? .clear : colour.opacity(level.coreOpacity),
                radius: level.coreRadius,
                x: 0,
                y: 0
            )
            // Layer B: Mid bloom
            .shadow(
                color: reduceMotion ? .clear : colour.opacity(level.midOpacity),
                radius: level.midRadius,
                x: 0,
                y: 0
            )
            // Layer C: Outer halo (conditional)
            .shadow(
                color: reduceMotion ? .clear : colour.opacity(level.outerOpacity),
                radius: level.outerRadius,
                x: 0,
                y: 0
            )
    }
}

// MARK: - View Extension

extension View {
    /// Applies a three-layer "instrument jewel" glow effect
    /// - Parameters:
    ///   - level: Intensity level (subtle, standard, intense, beat)
    ///   - colour: Glow colour (default: lwGold)
    func glowStack(
        level: GlowLevel = .subtle,
        colour: Color = .lwGold
    ) -> some View {
        modifier(GlowStackModifier(level: level, colour: colour))
    }
}

// MARK: - Preview

#Preview("Glow Levels") {
    VStack(spacing: 48) {
        ForEach([GlowLevel.subtle, .standard, .intense, .beat], id: \.self) { level in
            Text(String(describing: level))
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
                .padding()
                .frame(width: 120)
                .background(Color.lwCard)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
                .glowStack(level: level, colour: .lwGold)
        }
    }
    .padding(48)
    .background(Color.lwBase)
}

#Preview("Glow Colours") {
    HStack(spacing: 32) {
        Circle()
            .fill(Color.lwGold)
            .frame(width: 40, height: 40)
            .glowStack(level: .standard, colour: .lwGold)

        Circle()
            .fill(Color.lwSuccess)
            .frame(width: 40, height: 40)
            .glowStack(level: .standard, colour: .lwSuccess)

        Circle()
            .fill(Color.lwError)
            .frame(width: 40, height: 40)
            .glowStack(level: .standard, colour: .lwError)

        Circle()
            .fill(Color.lwZone0)
            .frame(width: 40, height: 40)
            .glowStack(level: .standard, colour: .lwZone0)
    }
    .padding(48)
    .background(Color.lwBase)
}
