//
//  LWCard.swift
//  LightwaveOS
//
//  Container card with gradient background and optional title header.
//  Now uses "Liquid Glass" primitives for premium visual treatment.
//

import SwiftUI

/// Card component with Liquid Glass treatment.
/// Uses the 8-layer optical stack from the glass primitives.
struct LWCard<Content: View>: View {
    var title: String?
    var elevated: Bool = false
    var accentBorder: Color?
    @ViewBuilder var content: () -> Content

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
        // Layer 1: Base shape with gradient
        .background(
            RoundedRectangle(cornerRadius: CornerRadius.card)
                .fill(Color.lwCardGradient)
        )
        // Layer 2: Glass surface (material blur)
        .glassSurface(style: .card, cornerRadius: CornerRadius.card)
        // Layer 3: Inner shadow for depth
        .innerShadow(cornerRadius: CornerRadius.card)
        // Layer 4: Inner highlight (specular lip)
        .innerHighlight(style: .subtle, cornerRadius: CornerRadius.card)
        // Layer 5: Gradient stroke border
        .gradientStroke(style: accentBorder != nil ? .zone(accentBorder!) : .subtle, cornerRadius: CornerRadius.card)
        // Layer 6: Grain overlay for texture
        .grainOverlay(.subtle)
        // Composite all card body layers to prevent scroll artifacts
        .compositingGroup()
        // Layer 7: Clip to shape
        .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        // Layer 8: Shadow stack
        .modifier(GlassShadowModifier(elevated: elevated))
    }
}

// MARK: - Shadow Modifier

private struct GlassShadowModifier: ViewModifier {
    let elevated: Bool

    func body(content: Content) -> some View {
        if elevated {
            content.shadowStack(style: .elevated)
        } else {
            content.shadowStack(style: .card)
        }
    }
}

// MARK: - Preview

#Preview("Card Variants") {
    ScrollView {
        VStack(spacing: Spacing.md) {
            LWCard(title: "System Status") {
                VStack(alignment: .leading, spacing: Spacing.sm) {
                    HStack {
                        Text("Uptime")
                            .foregroundStyle(Color.lwTextSecondary)
                        Spacer()
                        Text("2h 34m")
                            .foregroundStyle(Color.lwTextPrimary)
                    }

                    HStack {
                        Text("FPS")
                            .foregroundStyle(Color.lwTextSecondary)
                        Spacer()
                        Text("120")
                            .foregroundStyle(Color.lwSuccess)
                    }

                    HStack {
                        Text("Free Heap")
                            .foregroundStyle(Color.lwTextSecondary)
                        Spacer()
                        Text("142 KB")
                            .foregroundStyle(Color.lwTextPrimary)
                    }
                }
                .font(.bodyValue)
            }

            LWCard {
                VStack(spacing: Spacing.xs) {
                    Text("Card without title")
                        .foregroundStyle(Color.lwTextPrimary)

                    Text("This demonstrates a card with no header")
                        .font(.caption)
                        .foregroundStyle(Color.lwTextSecondary)
                }
            }

            LWCard(title: "Elevated Card", elevated: true) {
                Text("This card uses the elevated shadow style")
                    .foregroundStyle(Color.lwTextPrimary)
                    .font(.bodyValue)
            }

            LWCard(title: "Zone 0", accentBorder: .lwZone0) {
                VStack(alignment: .leading, spacing: Spacing.sm) {
                    Text("80 LEDs · L[40-79] R[80-119]")
                        .font(.caption)
                        .foregroundStyle(Color.lwTextSecondary)

                    HStack {
                        Text("Effect")
                            .foregroundStyle(Color.lwTextSecondary)
                        Spacer()
                        Text("Ripple Enhanced")
                            .foregroundStyle(Color.lwTextPrimary)
                    }
                }
                .font(.bodyValue)
            }

            LWCard(title: "Zone 2", elevated: true, accentBorder: .lwZone2) {
                Text("Accent border with elevated shadow")
                    .foregroundStyle(Color.lwTextPrimary)
                    .font(.bodyValue)
            }
        }
        .padding(Spacing.lg)
    }
    .background(Color.lwBase)
}
