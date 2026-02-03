//
//  EffectViewModel.swift
//  LightwaveOS
//
//  Effect selection and filtering with categoryId and isAudioReactive decoding.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import Foundation
import Observation

@MainActor
@Observable
class EffectViewModel {
    // MARK: - State

    var allEffects: [EffectMetadata] = []
    var currentEffectId: Int = 0
    var currentEffectName: String = ""
    var searchText: String = ""
    var selectedCategory: String = "All"
    var showAudioOnlyFilter: Bool = false

    // MARK: - Dependencies

    var restClient: RESTClient?

    // MARK: - Computed Properties

    /// All unique categories from loaded effects
    var categories: [String] {
        let cats = Set(allEffects.compactMap { $0.category })
        return ["All"] + cats.sorted()
    }

    /// Audio-reactive effects only
    var audioReactiveEffects: [EffectMetadata] {
        allEffects.filter { $0.isAudioReactive }
    }

    /// Filtered effects by category, search text, and audio-only flag
    func filteredEffects(category: String? = nil, searchText: String? = nil, audioOnly: Bool = false) -> [EffectMetadata] {
        var effects = allEffects

        // Filter by audio-reactive if requested
        if audioOnly {
            effects = effects.filter { $0.isAudioReactive }
        }

        // Filter by category
        let cat = category ?? selectedCategory
        if cat != "All" {
            effects = effects.filter { $0.displayCategory == cat }
        }

        // Filter by search text
        let search = searchText ?? self.searchText
        if !search.isEmpty {
            effects = effects.filter {
                $0.name.localizedCaseInsensitiveContains(search) ||
                $0.displayCategory.localizedCaseInsensitiveContains(search)
            }
        }

        return effects
    }

    /// Grouped effects by category
    var groupedEffects: [(String, [EffectMetadata])] {
        let filtered = filteredEffects()
        let grouped = Dictionary(grouping: filtered) { $0.displayCategory }
        return grouped.sorted { $0.key < $1.key }
            .map { ($0.key, $0.value.sorted { $0.name < $1.name }) }
    }

    // MARK: - API Methods

    func loadEffects() async {
        guard let client = restClient else { return }

        do {
            let response = try await client.getEffects(page: 1, limit: 200)

            // Decode with categoryId and isAudioReactive (API_AUDIT fix)
            self.allEffects = response.data.effects.map { effect in
                EffectMetadata(
                    id: effect.id,
                    name: effect.name,
                    category: effect.category,
                    categoryId: effect.categoryId,
                    isAudioReactive: effect.isAudioReactive ?? false,
                    categoryName: effect.categoryName
                )
            }
            print("Loaded \(allEffects.count) effects")

        } catch {
            print("Error loading effects: \(error)")
        }
    }

    func setEffect(id: Int) async {
        guard let client = restClient else { return }

        do {
            try await client.setEffect(id)

            // Optimistic update
            self.currentEffectId = id
            if let effect = allEffects.first(where: { $0.id == id }) {
                self.currentEffectName = effect.name
            }
            print("Set effect to \(id)")

        } catch {
            print("Error setting effect: \(error)")
        }
    }

    func nextEffect() async {
        guard !allEffects.isEmpty else { return }

        let currentIndex = allEffects.firstIndex(where: { $0.id == currentEffectId }) ?? -1
        let nextIndex = (currentIndex + 1) % allEffects.count
        let nextEffect = allEffects[nextIndex]

        await setEffect(id: nextEffect.id)
    }

    func previousEffect() async {
        guard !allEffects.isEmpty else { return }

        let currentIndex = allEffects.firstIndex(where: { $0.id == currentEffectId }) ?? 0
        let prevIndex = currentIndex > 0 ? currentIndex - 1 : allEffects.count - 1
        let prevEffect = allEffects[prevIndex]

        await setEffect(id: prevEffect.id)
    }
}
