//
//  LWCard.swift
//  LightwaveOS
//
//  Container card with gradient background and optional title header.
//

import SwiftUI

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
        .background(
            RoundedRectangle(cornerRadius: CornerRadius.card)
                .fill(Color.lwCardGradient)
        )
        .overlay(
            RoundedRectangle(cornerRadius: CornerRadius.card)
                .strokeBorder(accentBorder ?? Color.clear, lineWidth: accentBorder != nil ? 2 : 0)
        )
        .modifier(ShadowModifier(elevated: elevated))
    }
}

// MARK: - Shadow Modifier

private struct ShadowModifier: ViewModifier {
    let elevated: Bool

    func body(content: Content) -> some View {
        if elevated {
            content.elevatedShadow()
        } else {
            content.ambientShadow()
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
