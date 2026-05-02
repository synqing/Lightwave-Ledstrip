//
//  ZoneEffectSelectorView.swift
//  LightwaveOS
//
//  Zone-scoped effect selector matching the main effect gallery style.
//

import SwiftUI

struct ZoneEffectSelectorView: View {
    @Environment(AppViewModel.self) private var appVM
    @Environment(\.dismiss) private var dismiss

    let zoneId: Int

    @State private var searchText = ""
    @State private var selectedCategory = "All"
    @State private var audioOnly = false
    @AppStorage private var storedAudioOnly: Bool

    init(zoneId: Int) {
        self.zoneId = zoneId
        self._storedAudioOnly = AppStorage(wrappedValue: false, "zoneEffectAudioOnly.zone\(zoneId)")
    }

    var body: some View {
        NavigationStack {
            ZStack {
                Color.lwBase.ignoresSafeArea()

                VStack(spacing: 0) {
                    // Zone header chip
                    HStack {
                        ZoneSelectorChip(title: "ZONE \(zoneId + 1)", colour: Color.zoneColor(zoneId))
                        Spacer()
                    }
                    .padding(.horizontal, Spacing.md)
                    .padding(.top, Spacing.sm)

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

                    // Quick filters
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: Spacing.sm) {
                            QuickFilterPill(
                                title: "Audio-only",
                                isSelected: audioOnly
                            ) {
                                audioOnly.toggle()
                            }
                        }
                        .padding(.horizontal, Spacing.md)
                        .padding(.bottom, Spacing.sm)
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
                                            .foregroundStyle(Color.lwTextTertiary)
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
                    }
                }
            }
            .navigationTitle("Select Effect")
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
            }
            .onAppear {
                audioOnly = storedAudioOnly
            }
            .onChange(of: audioOnly) { _, newValue in
                storedAudioOnly = newValue
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

        // Quick filter: audio-only
        if audioOnly {
            effects = effects.filter { $0.isAudioReactive }
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

    private func isSelected(_ effect: EffectMetadata) -> Bool {
        effect.id == currentEffectId
    }

    private var currentEffectId: Int? {
        appVM.zones.zones.first(where: { $0.id == zoneId })?.effectId
    }

    // MARK: - Methods

    private func selectEffect(_ effect: EffectMetadata) {
        Task {
            await appVM.zones.setZoneEffect(
                zoneId: zoneId,
                effectId: effect.id,
                effectName: effect.name
            )
            dismiss()
        }
    }
}

// MARK: - Zone Chip

private struct ZoneSelectorChip: View {
    let title: String
    let colour: Color

    var body: some View {
        Text(title)
            .font(.subGroupHeader)
            .tracking(1.2)
            .foregroundStyle(colour)
            .padding(.horizontal, 12)
            .padding(.vertical, 6)
            .background(
                Capsule()
                    .fill(Color.lwCard)
                    .overlay(
                        Capsule()
                            .strokeBorder(colour.opacity(0.8), lineWidth: 1.5)
                    )
            )
    }
}

// MARK: - Quick Filter Pill

private struct QuickFilterPill: View {
    let title: String
    let isSelected: Bool
    let onTap: () -> Void

    var body: some View {
        Button(action: onTap) {
            Text(title)
                .font(.caption)
                .foregroundStyle(isSelected ? Color.lwBase : Color.lwTextPrimary)
                .padding(.horizontal, 14)
                .padding(.vertical, 7)
                .background(isSelected ? Color.lwGold : Color.lwElevated)
                .clipShape(Capsule())
        }
        .buttonStyle(.plain)
        .accessibilityAddTraits(isSelected ? .isSelected : [])
    }
}
