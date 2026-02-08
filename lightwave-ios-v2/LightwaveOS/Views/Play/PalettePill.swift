//
//  PalettePill.swift
//  LightwaveOS
//
//  Full-width tappable card with prev/next buttons and palette grid sheet.
//  Shows horizontal gradient swatch from palette colours.
//

import SwiftUI

struct PalettePill: View {
    @Environment(AppViewModel.self) private var appVM
    @State private var showPaletteSelector = false

    var body: some View {
        VStack(alignment: .leading, spacing: Spacing.sm) {
            // Section header and count — tappable to open sheet
            Button {
                showPaletteSelector = true
            } label: {
                HStack {
                    Text("PALETTE")
                        .font(.sectionHeader)
                        .foregroundStyle(Color.lwTextTertiary)
                        .textCase(.uppercase)
                        .tracking(0.8)

                    Spacer()

                    HStack(spacing: Spacing.xs) {
                        Text("\(appVM.palettes.currentPaletteId)/\(appVM.palettes.allPalettes.count)")
                            .font(.caption)
                            .foregroundStyle(Color.lwTextSecondary)

                        Image(systemName: "chevron.right")
                            .font(.iconSmall)
                            .foregroundStyle(Color.lwGold)
                    }
                }
            }
            .buttonStyle(.plain)
            .accessibilityLabel(Text("Select palette"))
            .accessibilityValue(Text(appVM.palettes.currentPaletteName))
            .accessibilityHint(Text("Opens the palette selector."))

            // Gradient swatch with prev/next buttons
            HStack(spacing: Spacing.sm) {
                // Previous button
                Button {
                    Task { await appVM.palettes.previousPalette() }
                } label: {
                    Image(systemName: "chevron.left")
                        .font(.iconSmall)
                        .foregroundStyle(Color.lwGold)
                        .frame(width: 32, height: 32)
                }
                .buttonStyle(.plain)
                .accessibilityLabel(Text("Previous palette"))

                // Gradient swatch — tappable to open sheet
                Button {
                    showPaletteSelector = true
                } label: {
                    if let palette = appVM.palettes.allPalettes.first(where: { $0.id == appVM.palettes.currentPaletteId }) {
                        LinearGradient(
                            colors: palette.gradientColors,
                            startPoint: .leading,
                            endPoint: .trailing
                        )
                        .frame(height: 12)
                        .clipShape(Capsule())
                    } else {
                        LinearGradient(
                            colors: [Color.gray, Color.white],
                            startPoint: .leading,
                            endPoint: .trailing
                        )
                        .frame(height: 12)
                        .clipShape(Capsule())
                    }
                }
                .buttonStyle(.plain)

                // Next button
                Button {
                    Task { await appVM.palettes.nextPalette() }
                } label: {
                    Image(systemName: "chevron.right")
                        .font(.iconSmall)
                        .foregroundStyle(Color.lwGold)
                        .frame(width: 32, height: 32)
                }
                .buttonStyle(.plain)
                .accessibilityLabel(Text("Next palette"))
            }

            // Palette name
            Text(appVM.palettes.currentPaletteName)
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 12)
        .background(
            RoundedRectangle(cornerRadius: CornerRadius.card)
                .fill(Color.lwCardGradient)
        )
        // Liquid Glass treatment
        .glassSurface(style: .card, cornerRadius: CornerRadius.card)
        .innerShadow(cornerRadius: CornerRadius.card)
        .innerHighlight(style: .subtle, cornerRadius: CornerRadius.card)
        .gradientStroke(style: .subtle, cornerRadius: CornerRadius.card)
        .grainOverlay(.subtle)
        .clipShape(RoundedRectangle(cornerRadius: CornerRadius.card))
        .shadowStack(style: .card)
        .sheet(isPresented: $showPaletteSelector) {
            PaletteSelectorView()
        }
    }

    private var paletteAccent: some View {
        EmptyView()
            .frame(width: 36, height: 36)
            .opacity(0.9)
            .allowsHitTesting(false)
    }
}

// MARK: - Preview

#Preview("Palette Pill") {
    VStack(spacing: Spacing.lg) {
        PalettePill()

        PalettePill()
            .environment({
                let vm = AppViewModel()
                vm.palettes.currentPaletteId = 2
                return vm
            }())
    }
    .padding(Spacing.lg)
    .background(Color.lwBase)
    .environment(AppViewModel())
}
