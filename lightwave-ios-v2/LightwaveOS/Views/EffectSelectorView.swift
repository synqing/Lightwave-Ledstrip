//
//  EffectSelectorView.swift
//  LightwaveOS
//
//  REDESIGNED visual gallery sheet for browsing and selecting effects.
//  Features: search, category filter pills, visual effect cards (72pt tall).
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import SwiftUI

struct EffectSelectorView: View {
    @Environment(AppViewModel.self) private var appVM
    @Environment(\.dismiss) private var dismiss

    @State private var searchText = ""
    @State private var selectedCategory = "All"

    var body: some View {
        NavigationStack {
            ZStack {
                Color.lwBase.ignoresSafeArea()

                VStack(spacing: 0) {
                    // Category filter pills
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: Spacing.sm) {
                            ForEach(categories, id: \.self) { category in
                                CategoryFilterPill(
                                    category: category,
                                    isSelected: selectedCategory == category
                                ) {
                                    selectedCategory = category
                                }
                            }
                        }
                        .padding(.horizontal, Spacing.md)
                        .padding(.vertical, Spacing.sm)
                    }
                    .background(Color.lwCard)

                    Divider()
                        .background(Color.lwTextTertiary.opacity(0.3))

                    // Effects gallery
                    if filteredAndGroupedEffects.isEmpty {
                        VStack(spacing: Spacing.md) {
                            Image(systemName: "magnifyingglass")
                                .font(.iconLarge)
                                .foregroundStyle(Color.lwTextTertiary)

                            Text("No effects found")
                                .font(.bodyValue)
                                .foregroundStyle(Color.lwTextSecondary)

                            if let error = appVM.effects.lastError {
                                Text(error)
                                    .font(.caption)
                                    .foregroundStyle(Color.red)
                                    .multilineTextAlignment(.center)
                                    .padding(.horizontal, Spacing.lg)

                                Button("Retry") {
                                    Task {
                                        await appVM.effects.loadEffects()
                                    }
                                }
                                .foregroundStyle(Color.lwGold)
                            }
                        }
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                    } else {
                        ScrollView {
                            VStack(alignment: .leading, spacing: Spacing.lg) {
                                ForEach(filteredAndGroupedEffects, id: \.category) { group in
                                    VStack(alignment: .leading, spacing: Spacing.sm) {
                                        // Category header
                                        Text("\(group.category.uppercased()) (\(group.effects.count))")
                                            .font(.sectionHeader)
                                            .foregroundStyle(Color.lwTextSecondary)
                                            .textCase(.uppercase)
                                            .tracking(0.8)
                                            .padding(.horizontal, Spacing.md)

                                        // Effect cards
                                        ForEach(group.effects) { effect in
                                            EffectCard(
                                                effect: effect,
                                                isSelected: isSelected(effect)
                                            ) {
                                                selectEffect(effect)
                                            }
                                            .padding(.horizontal, Spacing.md)
                                        }
                                    }
                                }
                            }
                            .padding(.vertical, Spacing.md)
                        }
                        .refreshable {
                            await appVM.effects.loadEffects()
                        }
                    }
                }
            }
            .navigationTitle(navigationTitle)
            .navigationBarTitleDisplayMode(.inline)
            .searchable(
                text: $searchText,
                placement: .navigationBarDrawer(displayMode: .always),
                prompt: "Search effects..."
            )
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Close") {
                        dismiss()
                    }
                    .foregroundStyle(Color.lwGold)
                }

                ToolbarItem(placement: .primaryAction) {
                    if appVM.effects.isLoading {
                        ProgressView()
                            .tint(Color.lwGold)
                    }
                }
            }
        }
    }

    // MARK: - Computed Properties

    private var categories: [String] {
        var cats: [String] = ["All"]
        if allEffects.contains(where: { $0.isAudioReactive }) {
            cats.append("Audio")
        }
        cats.append(contentsOf: Set(allEffects.map { $0.displayCategory }).sorted())
        return cats
    }

    private var allEffects: [EffectMetadata] {
        appVM.effects.allEffects
    }

    private var filteredAndGroupedEffects: [(category: String, effects: [EffectMetadata])] {
        var effects = allEffects

        // Filter by search text
        if !searchText.isEmpty {
            effects = effects.filter {
                $0.name.localizedCaseInsensitiveContains(searchText) ||
                $0.displayCategory.localizedCaseInsensitiveContains(searchText)
            }
        }

        // Filter by category
        if selectedCategory == "Audio" {
            effects = effects.filter { $0.isAudioReactive }
        } else if selectedCategory != "All" {
            effects = effects.filter { $0.displayCategory == selectedCategory }
        }

        // Group by category
        let grouped = Dictionary(grouping: effects) { $0.displayCategory }
        return grouped.sorted { $0.key < $1.key }
            .map { (category: $0.key, effects: $0.value.sorted { $0.name < $1.name }) }
    }

    private var navigationTitle: String {
        let count = allEffects.count
        return count > 0 ? "Effects (\(count))" : "Select Effect"
    }

    private func isSelected(_ effect: EffectMetadata) -> Bool {
        effect.id == appVM.effects.currentEffectId
    }

    // MARK: - Methods

    private func selectEffect(_ effect: EffectMetadata) {
        Task {
            await appVM.effects.setEffect(id: effect.id)
            dismiss()
        }
    }
}

// MARK: - Category Filter Pill

struct CategoryFilterPill: View {
    let category: String
    let isSelected: Bool
    let onTap: () -> Void

    var body: some View {
        Button(action: onTap) {
            Text(category)
                .font(.caption)
                .foregroundStyle(isSelected ? Color.lwBase : Color.lwTextPrimary)
                .padding(.horizontal, 16)
                .padding(.vertical, 8)
                .background(isSelected ? Color.lwGold : Color.lwElevated)
                .clipShape(Capsule())
        }
        .buttonStyle(.plain)
        .accessibilityAddTraits(isSelected ? .isSelected : [])
    }
}

// MARK: - Effect Card

struct EffectCard: View {
    let effect: EffectMetadata
    let isSelected: Bool
    let onTap: () -> Void

    var body: some View {
        Button(action: onTap) {
            HStack(spacing: Spacing.md) {
                VStack(alignment: .leading, spacing: Spacing.xs) {
                    Text(effect.name)
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                        .lineLimit(2)

                    HStack(spacing: Spacing.xs) {
                        EffectChip(title: effect.displayCategory)

                        if effect.isAudioReactive {
                            EffectChip(title: "Audio", icon: "waveform", isHighlighted: true)
                        }
                    }
                }

                Spacer()

                if isSelected {
                    Image(systemName: "checkmark.circle.fill")
                        .font(.iconMedium)
                        .foregroundStyle(Color.lwGold)
                }
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 16)
            .frame(height: 72)
            .background(
                RoundedRectangle(cornerRadius: 16)
                    .fill(Color.lwCardGradient)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 16)
                    .strokeBorder(isSelected ? Color.lwGold : Color.clear, lineWidth: 2)
            )
            .shadow(
                color: isSelected ? Color.lwGold.opacity(0.3) : Color.black.opacity(0.3),
                radius: isSelected ? 12 : 6,
                x: 0,
                y: 2
            )
        }
        .buttonStyle(.plain)
        .accessibilityLabel(Text(effect.name))
        .accessibilityHint(Text("Select effect"))
        .accessibilityAddTraits(isSelected ? .isSelected : [])
    }
}

// MARK: - Effect Chip

struct EffectChip: View {
    let title: String
    var icon: String? = nil
    var isHighlighted: Bool = false

    var body: some View {
        HStack(spacing: 4) {
            if let icon {
                Image(systemName: icon)
                    .font(.caption)
            }
            Text(title)
                .font(.caption)
        }
        .foregroundStyle(isHighlighted ? Color.lwBase : Color.lwTextSecondary)
        .padding(.horizontal, 8)
        .padding(.vertical, 4)
        .background(isHighlighted ? Color.lwGold : Color.lwElevated)
        .clipShape(Capsule())
    }
}

// MARK: - Mock Data

#if DEBUG
private let mockEffects: [EffectMetadata] = [
    // Light Guide Plate
    EffectMetadata(id: 0, name: "LGP Interference", category: "Light Guide Plate", categoryId: 1, isAudioReactive: false, categoryName: "Light Guide Plate"),
    EffectMetadata(id: 1, name: "LGP Holographic Auto-Cycle", category: "Light Guide Plate", categoryId: 1, isAudioReactive: true, categoryName: "Light Guide Plate"),
    EffectMetadata(id: 2, name: "LGP Chromatic Shear", category: "Light Guide Plate", categoryId: 1, isAudioReactive: false, categoryName: "Light Guide Plate"),

    // Particles
    EffectMetadata(id: 25, name: "Starfield", category: "Particles", categoryId: 5, isAudioReactive: false, categoryName: "Particles"),
    EffectMetadata(id: 26, name: "Fireflies", category: "Particles", categoryId: 5, isAudioReactive: true, categoryName: "Particles"),
    EffectMetadata(id: 27, name: "Embers", category: "Particles", categoryId: 5, isAudioReactive: false, categoryName: "Particles"),

    // Audio Reactive
    EffectMetadata(id: 12, name: "Beat Pulse", category: "Audio Reactive", categoryId: 3, isAudioReactive: true, categoryName: "Audio Reactive"),
    EffectMetadata(id: 13, name: "Spectrum Analyser", category: "Audio Reactive", categoryId: 3, isAudioReactive: true, categoryName: "Audio Reactive"),
    EffectMetadata(id: 14, name: "VU Meter", category: "Audio Reactive", categoryId: 3, isAudioReactive: true, categoryName: "Audio Reactive"),

    // Waves
    EffectMetadata(id: 40, name: "Ripple Enhanced", category: "Waves", categoryId: 7, isAudioReactive: false, categoryName: "Waves"),
    EffectMetadata(id: 41, name: "Ocean Waves", category: "Waves", categoryId: 7, isAudioReactive: true, categoryName: "Waves"),
    EffectMetadata(id: 42, name: "Standing Wave", category: "Waves", categoryId: 7, isAudioReactive: false, categoryName: "Waves"),

    // Ambient
    EffectMetadata(id: 60, name: "Gentle Breathing", category: "Ambient", categoryId: 9, isAudioReactive: false, categoryName: "Ambient"),
    EffectMetadata(id: 61, name: "Zen Garden", category: "Ambient", categoryId: 9, isAudioReactive: false, categoryName: "Ambient")
]
#endif

// MARK: - Preview

#Preview("Effect Selector") {
    EffectSelectorView()
        .environment({
            let vm = AppViewModel()
            #if DEBUG
            vm.effects.allEffects = mockEffects
            vm.effects.currentEffectId = 42
            #endif
            return vm
        }())
}
