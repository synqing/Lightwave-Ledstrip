//
//  GlassCard.swift
//  LightwaveOS
//
//  8-layer optical stack glass card component.
//  Part of the "Liquid Glass" optical rendering system.
//
//  Layer Stack (bottom to top):
//  1. AmbientOcclusionShadowStack - Drop shadow stack
//  2. GlassSurface - Material blur foundation
//  3. Base gradient fill - lwCardGradient at 0.6 opacity
//  4. InnerShadow - Depth inset cue
//  5. InnerHighlight - Specular lip
//  6. GradientStroke - 1px edge definition
//  7. Content - Child views
//  8. GrainOverlay - Microtexture finish
//

import SwiftUI

// MARK: - Glass Card Variant

/// Visual variants for GlassCard
enum GlassCardVariant {
    /// Full 8-layer optical stack
    case standard

    /// Standard + edge bloom + beat response
    case hero

    /// Reduced layers for nested cards (skip layers 5, 8)
    case compact
}

// MARK: - Glass Card

/// A glass card with an 8-layer optical stack for premium depth and texture
struct GlassCard<Content: View>: View {
    // MARK: Properties

    var title: String?
    var variant: GlassCardVariant
    var strokeStyle: GradientStrokeStyle
    var glowLevel: GlowLevel?
    var glowColour: Color
    var isLoading: Bool
    var isBeating: Bool
    var isDownbeat: Bool
    @ViewBuilder var content: () -> Content

    // MARK: Initialiser

    init(
        title: String? = nil,
        variant: GlassCardVariant = .standard,
        strokeStyle: GradientStrokeStyle = .standard,
        glowLevel: GlowLevel? = nil,
        glowColour: Color = .lwGold,
        isLoading: Bool = false,
        isBeating: Bool = false,
        isDownbeat: Bool = false,
        @ViewBuilder content: @escaping () -> Content
    ) {
        self.title = title
        self.variant = variant
        self.strokeStyle = strokeStyle
        self.glowLevel = glowLevel
        self.glowColour = glowColour
        self.isLoading = isLoading
        self.isBeating = isBeating
        self.isDownbeat = isDownbeat
        self.content = content
    }

    // MARK: Computed Properties

    private var cornerRadius: CGFloat {
        switch variant {
        case .hero: return CornerRadius.hero
        default: return CornerRadius.card
        }
    }

    private var surfaceStyle: GlassSurfaceStyle {
        switch variant {
        case .standard: return .card
        case .hero: return .hero
        case .compact: return .card
        }
    }

    private var shadowStyle: ShadowStackStyle {
        switch variant {
        case .standard: return .card
        case .hero: return .hero
        case .compact: return .card
        }
    }

    // MARK: Body

    var body: some View {
        VStack(alignment: .leading, spacing: Spacing.sm) {
            if let title = title {
                Text(title)
                    .font(.sectionHeader)
                    .foregroundStyle(Color.lwTextTertiary)
                    .textCase(.uppercase)
                    .tracking(0.8)
            }

            content()
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 12)
        // Layer 2: Glass surface (material blur)
        .glassSurface(style: surfaceStyle, cornerRadius: cornerRadius)
        // Layer 3: Base gradient fill
        .background(
            RoundedRectangle(cornerRadius: cornerRadius)
                .fill(Color.lwCardGradient)
                .opacity(0.6)
        )
        // Layer 4: Inner shadow
        .innerShadow(style: .subtle, cornerRadius: cornerRadius)
        // Layer 5: Inner highlight (skip for compact)
        .modifier(ConditionalInnerHighlight(
            enabled: variant != .compact,
            cornerRadius: cornerRadius
        ))
        // Layer 6: Gradient stroke
        .gradientStroke(style: strokeStyle, cornerRadius: cornerRadius)
        // Layer 7: Shimmer (loading state)
        .shimmerSweep(isActive: isLoading)
        // Layer 8: Grain overlay (skip for compact)
        .modifier(ConditionalGrainOverlay(enabled: variant != .compact))
        // Composite all card body layers to prevent scroll artifacts
        .compositingGroup()
        // Clip to shape
        .clipShape(RoundedRectangle(cornerRadius: cornerRadius))
        // Layer 1: Shadow stack + optional glow
        .shadowStack(style: shadowStyle)
        .modifier(ConditionalGlow(level: glowLevel, colour: glowColour))
        // Hero bloom
        .modifier(HeroBloom(enabled: variant == .hero, colour: glowColour))
        // Beat pulse (hero variant)
        .modifier(ConditionalEnergyPulse(
            enabled: variant == .hero,
            isBeating: isBeating,
            isDownbeat: isDownbeat,
            colour: glowColour
        ))
    }
}

// MARK: - Conditional Modifiers

private struct ConditionalInnerHighlight: ViewModifier {
    let enabled: Bool
    let cornerRadius: CGFloat

    func body(content: Content) -> some View {
        if enabled {
            content.innerHighlight(style: .standard, cornerRadius: cornerRadius)
        } else {
            content
        }
    }
}

private struct ConditionalGrainOverlay: ViewModifier {
    let enabled: Bool

    func body(content: Content) -> some View {
        if enabled {
            content.grainOverlay(.subtle)
        } else {
            content
        }
    }
}

private struct ConditionalGlow: ViewModifier {
    let level: GlowLevel?
    let colour: Color

    func body(content: Content) -> some View {
        if let level = level {
            content.glowStack(level: level, colour: colour)
        } else {
            content
        }
    }
}

private struct HeroBloom: ViewModifier {
    let enabled: Bool
    let colour: Color

    func body(content: Content) -> some View {
        if enabled {
            content
                .shadow(
                    color: colour.opacity(GlassTokens.bloomOpacity),
                    radius: GlassTokens.bloomBlur,
                    x: 0,
                    y: 0
                )
        } else {
            content
        }
    }
}

private struct ConditionalEnergyPulse: ViewModifier {
    let enabled: Bool
    let isBeating: Bool
    let isDownbeat: Bool
    let colour: Color

    func body(content: Content) -> some View {
        if enabled {
            content.energyPulse(isActive: isBeating, isDownbeat: isDownbeat, colour: colour)
        } else {
            content
        }
    }
}

// MARK: - Previews

#Preview("Standard GlassCard") {
    GlassCard(title: "Expression") {
        VStack(alignment: .leading, spacing: Spacing.sm) {
            Text("Mood: Reactive")
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
            Text("Trails: 50%")
                .font(.cardLabel)
                .foregroundStyle(Color.lwTextSecondary)
        }
    }
    .padding()
    .background(Color.lwBase)
}

#Preview("Hero GlassCard") {
    GlassCard(
        title: "LED Preview",
        variant: .hero,
        glowLevel: .subtle
    ) {
        RoundedRectangle(cornerRadius: CornerRadius.nested)
            .fill(Color.lwElevated)
            .frame(height: 100)
            .overlay {
                Text("320 LEDs")
                    .font(.heroNumeral)
                    .foregroundStyle(Color.lwGold)
            }
    }
    .padding()
    .background(Color.lwBase)
}

#Preview("Compact GlassCard") {
    GlassCard(variant: .compact) {
        Text("Nested content")
            .font(.cardLabel)
            .foregroundStyle(Color.lwTextSecondary)
    }
    .padding()
    .background(Color.lwBase)
}

#Preview("GlassCard with Zone Stroke") {
    VStack(spacing: Spacing.md) {
        ForEach(0..<4, id: \.self) { zone in
            GlassCard(
                title: "Zone \(zone)",
                strokeStyle: .zone(Color.zoneColor(zone))
            ) {
                Text("80 LEDs")
                    .font(.bodyValue)
                    .foregroundStyle(Color.lwTextPrimary)
            }
        }
    }
    .padding()
    .background(Color.lwBase)
}

#Preview("Loading GlassCard") {
    GlassCard(
        title: "Loading...",
        isLoading: true
    ) {
        VStack(spacing: Spacing.sm) {
            RoundedRectangle(cornerRadius: 4)
                .fill(Color.lwElevated)
                .frame(height: 20)
            RoundedRectangle(cornerRadius: 4)
                .fill(Color.lwElevated)
                .frame(width: 200, height: 16)
        }
    }
    .padding()
    .background(Color.lwBase)
}
