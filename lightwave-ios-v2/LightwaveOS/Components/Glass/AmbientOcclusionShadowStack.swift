//
//  AmbientOcclusionShadowStack.swift
//  LightwaveOS
//
//  Multi-layer shadow stack for ambient occlusion effect.
//  Part of the "Liquid Glass" optical rendering system.
//

import SwiftUI

// MARK: - Shadow Stack Style

/// Elevation-based shadow stack styles
enum ShadowStackStyle {
    case card       // Standard card
    case elevated   // Modals, sheets
    case hero       // LED preview, BPM hero
}

// MARK: - Shadow Stack Modifier

/// Applies multiple shadow layers for ambient occlusion effect
struct AmbientOcclusionShadowStackModifier: ViewModifier {
    let style: ShadowStackStyle

    @ViewBuilder
    func body(content: Content) -> some View {
        switch style {
        case .card:
            content
                .shadow(
                    color: Color.black.opacity(GlassTokens.shadowCardContactOpacity),
                    radius: GlassTokens.shadowCardContactRadius,
                    x: GlassTokens.shadowCardContactX,
                    y: GlassTokens.shadowCardContactY
                )
                .shadow(
                    color: Color.black.opacity(GlassTokens.shadowCardSpreadOpacity),
                    radius: GlassTokens.shadowCardSpreadRadius,
                    x: GlassTokens.shadowCardSpreadX,
                    y: GlassTokens.shadowCardSpreadY
                )

        case .elevated:
            content
                .shadow(
                    color: Color.black.opacity(GlassTokens.shadowElevatedContactOpacity),
                    radius: GlassTokens.shadowElevatedContactRadius,
                    x: GlassTokens.shadowElevatedContactX,
                    y: GlassTokens.shadowElevatedContactY
                )
                .shadow(
                    color: Color.black.opacity(GlassTokens.shadowElevatedSpreadOpacity),
                    radius: GlassTokens.shadowElevatedSpreadRadius,
                    x: GlassTokens.shadowElevatedSpreadX,
                    y: GlassTokens.shadowElevatedSpreadY
                )
                .shadow(
                    color: Color.black.opacity(GlassTokens.shadowElevatedDiffuseOpacity),
                    radius: GlassTokens.shadowElevatedDiffuseRadius,
                    x: GlassTokens.shadowElevatedDiffuseX,
                    y: GlassTokens.shadowElevatedDiffuseY
                )

        case .hero:
            content
                .shadow(
                    color: Color.black.opacity(GlassTokens.shadowHeroContactOpacity),
                    radius: GlassTokens.shadowHeroContactRadius,
                    x: GlassTokens.shadowHeroContactX,
                    y: GlassTokens.shadowHeroContactY
                )
                .shadow(
                    color: Color.black.opacity(GlassTokens.shadowHeroSpreadOpacity),
                    radius: GlassTokens.shadowHeroSpreadRadius,
                    x: GlassTokens.shadowHeroSpreadX,
                    y: GlassTokens.shadowHeroSpreadY
                )
                .shadow(
                    color: Color.black.opacity(GlassTokens.shadowHeroDiffuseOpacity),
                    radius: GlassTokens.shadowHeroDiffuseRadius,
                    x: GlassTokens.shadowHeroDiffuseX,
                    y: GlassTokens.shadowHeroDiffuseY
                )
        }
    }
}

// MARK: - View Extension

extension View {
    /// Applies a multi-layer shadow stack for ambient occlusion
    /// - Parameter style: Elevation-based shadow style
    func shadowStack(style: ShadowStackStyle = .card) -> some View {
        modifier(AmbientOcclusionShadowStackModifier(style: style))
    }
}

// MARK: - Preview

#Preview("Shadow Stacks") {
    VStack(spacing: 48) {
        Text("Card")
            .font(.bodyValue)
            .foregroundStyle(Color.lwTextPrimary)
            .padding()
            .frame(maxWidth: .infinity)
            .background(Color.lwCard)
            .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
            .shadowStack(style: .card)

        Text("Elevated")
            .font(.bodyValue)
            .foregroundStyle(Color.lwTextPrimary)
            .padding()
            .frame(maxWidth: .infinity)
            .background(Color.lwCard)
            .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
            .shadowStack(style: .elevated)

        Text("Hero")
            .font(.bodyValue)
            .foregroundStyle(Color.lwTextPrimary)
            .padding()
            .frame(maxWidth: .infinity)
            .background(Color.lwCard)
            .clipShape(RoundedRectangle(cornerRadius: CornerRadius.hero))
            .shadowStack(style: .hero)
    }
    .padding(32)
    .background(Color.lwBase)
}
