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

    /// Weak reference to the parent so VM-level events surface in the in-app
    /// DebugLogView and TestFlight reports rather than disappearing into stdout.
    /// Set by `AppViewModel.init` immediately after construction.
    weak var appVM: AppViewModel?

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

    /// Filtered effects by category, search text, and audio-only flag.
    /// Experimental effects are excluded by default; the power-user toggle
    /// `showExperimental` (see Phase 2 — picker enhancements MARK block at
    /// the end of this file) reveals them with a visual cue in the picker.
    func filteredEffects(category: String? = nil, searchText: String? = nil, audioOnly: Bool = false) -> [EffectMetadata] {
        var effects = allEffects

        // Hide experimental effects unless the power-user toggle is on.
        if !showExperimental {
            effects = effects.filter { !$0.isExperimental }
        }

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
        guard let client = restClient else {
            appVM?.log("loadEffects skipped — restClient is nil", category: "EFFECTS-ERROR")
            return
        }

        appVM?.log("GET /api/v1/effects (page=1, limit=200) — sending", category: "EFFECTS")
        do {
            let response = try await client.getEffects(page: 1, limit: 200)

            // Decode with categoryId, isAudioReactive (API_AUDIT fix), and
            // isExperimental (Phase 2 — picker enhancements). Legacy firmware
            // omits `isExperimental` — Optional decode treats absent as nil →
            // non-experimental, preserving backward compatibility.
            self.allEffects = response.data.effects.map { effect in
                EffectMetadata(
                    id: effect.id,
                    name: effect.name,
                    category: effect.category,
                    categoryId: effect.categoryId,
                    isAudioReactive: effect.isAudioReactive ?? false,
                    isExperimental: effect.isExperimental ?? false,
                    categoryName: effect.categoryName
                )
            }
            let totalCount = allEffects.count
            let experimentalCount = allEffects.filter { $0.isExperimental }.count
            let visibleCount = totalCount - experimentalCount
            appVM?.log(
                "Loaded \(totalCount) effects (visible: \(visibleCount), experimental: \(experimentalCount), showExperimental: \(showExperimental))",
                category: "EFFECTS"
            )
            if visibleCount == 0 && totalCount > 0 {
                appVM?.log(
                    "WARN: every effect is flagged experimental — picker will appear empty unless showExperimental is enabled",
                    category: "EFFECTS-WARN"
                )
            }

        } catch let DecodingError.keyNotFound(key, context) {
            appVM?.log("Effects decode failed: missing key '\(key.stringValue)' at \(context.codingPath.map(\.stringValue).joined(separator: "."))", category: "EFFECTS-ERROR")
        } catch let DecodingError.typeMismatch(type, context) {
            appVM?.log("Effects decode failed: type mismatch \(type) at \(context.codingPath.map(\.stringValue).joined(separator: "."))", category: "EFFECTS-ERROR")
        } catch let DecodingError.dataCorrupted(context) {
            appVM?.log("Effects decode failed: data corrupted at \(context.codingPath.map(\.stringValue).joined(separator: ".")) — \(context.debugDescription)", category: "EFFECTS-ERROR")
        } catch {
            appVM?.log("Effects load failed: \(error.localizedDescription)", category: "EFFECTS-ERROR")
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
            appVM?.log("Set effect to \(id)", category: "EFFECTS")

        } catch {
            appVM?.log("Set effect failed (\(id)): \(error.localizedDescription)", category: "EFFECTS-ERROR")
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

    // MARK: Phase 2 — runtime parameters
    //
    // End-user runtime tuning surface for the currently selected effect. The
    // sheet view (`EffectParameterSheet`) reads `currentParameters` and writes
    // back through `setRuntimeParameter`. Slider drags are debounced 150 ms so
    // a continuous gesture produces at most one REST POST every 150 ms — well
    // under the firmware's 20 req/s rate limit.

    /// The list of runtime parameters for the most recently loaded effect.
    /// Empty until `loadEffectParameters(effectId:client:)` populates it.
    var currentParameters: [EffectParameter] = []

    /// Pending values keyed by parameter name — held until the 150 ms debounce
    /// window elapses, then flushed to the network in a single round-trip.
    /// Observation excludes this so SwiftUI does not re-render on every keystroke.
    @ObservationIgnored
    private var pendingParameterValues: [String: Double] = [:]

    /// Per-parameter debounce timers. One in-flight task per parameter so a
    /// drag on `contrast` does not cancel a separate drag on `intensity`.
    @ObservationIgnored
    private var debounceTasks: [String: Task<Void, Never>] = [:]

    /// The minimum interval between consecutive POSTs for the same parameter.
    /// 150 ms is the project-wide slider debounce floor.
    private static let parameterDebounceInterval: Duration = .milliseconds(150)

    /// Fetch the tunable runtime parameters for `effectId` and store them in
    /// `currentParameters`. Replaces any previously loaded list.
    func loadEffectParameters(effectId: Int, client: RESTClient) async {
        do {
            let envelope = try await client.getEffectParameters(effectId: effectId)
            self.currentParameters = envelope.parameters
            appVM?.log("Loaded \(envelope.parameters.count) parameters for effect \(effectId)", category: "EFFECTS")
        } catch {
            appVM?.log("Effect parameters load failed for \(effectId): \(error.localizedDescription)", category: "EFFECTS-ERROR")
            self.currentParameters = []
        }
    }

    /// Queue a runtime parameter update. Coalesces rapid changes via a 150 ms
    /// debounce window; only the latest pending value for `name` is sent.
    /// - Parameters:
    ///   - name: The programmatic parameter key.
    ///   - value: The new value.
    ///   - client: The active REST client. `nil` is tolerated — the call
    ///     becomes a no-op so callers do not need a guard at every call site.
    func setRuntimeParameter(name: String, value: Double, client: RESTClient?) {
        guard let client = client else { return }

        // Optimistic local update — keeps the UI in sync with the dragged value
        // without waiting for the round-trip. The next reload will reconcile
        // against firmware truth.
        if let idx = currentParameters.firstIndex(where: { $0.name == name }) {
            let p = currentParameters[idx]
            currentParameters[idx] = EffectParameter.replacingValue(of: p, with: Float(value))
        }

        // Capture the latest desired value for this name and (re)start a
        // debounce task. Cancelling any previous task means callers can drag
        // freely; only the trailing value reaches the network.
        pendingParameterValues[name] = value

        debounceTasks[name]?.cancel()

        let effectId = self.currentEffectId
        debounceTasks[name] = Task { [weak self] in
            try? await Task.sleep(for: Self.parameterDebounceInterval)
            guard !Task.isCancelled else { return }
            guard let self = self else { return }

            // Read back the latest pending value at flush time — newer drags
            // may have overwritten it during the sleep.
            guard let latest = self.pendingParameterValues.removeValue(forKey: name) else {
                return
            }

            do {
                try await client.setRuntimeParameter(
                    effectId: effectId,
                    name: name,
                    value: latest
                )
            } catch {
                self.appVM?.log("Runtime parameter \(name) failed: \(error.localizedDescription)", category: "EFFECTS-ERROR")
            }
        }
    }

    // MARK: Phase 2 — picker enhancements
    //
    // F-2 power-user toggle. By default the picker hides effects tagged
    // `isExperimental` by firmware (see PatternRegistry::isExperimental). The
    // `showExperimental` flag flips the picker into a power-user mode where
    // experimentals are visible and visually flagged. Off by default for safety
    // — Captain's intent is that experimental effects do not appear in the
    // production rotation without explicit opt-in.
    //
    // Wired up by `EffectSelectorView` via a toolbar toggle button; observable
    // through @Observable so the picker re-renders when the flag flips.

    /// Power-user toggle. When true, `filteredEffects()` includes effects
    /// tagged `isExperimental` by firmware. Defaults to false.
    var showExperimental: Bool = false

    /// Flip the power-user toggle. Used by the picker's toolbar control.
    func toggleShowExperimental() {
        showExperimental.toggle()
    }
}

// MARK: - EffectParameter mutation helper (file-private)

private extension EffectParameter {
    /// Build a copy of `original` with a new live `value`. `EffectParameter`
    /// has `let` properties (it is decoded from the wire), so we round-trip
    /// through the encoder rather than introducing a memberwise initialiser
    /// that would widen the public API.
    static func replacingValue(of original: EffectParameter, with newValue: Float) -> EffectParameter {
        // Encode → mutate the dictionary → decode. Cheap (one parameter at
        // a time) and keeps `EffectParameter`'s definition untouched.
        guard let data = try? JSONEncoder().encode(original),
              var dict = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return original
        }
        dict["value"] = newValue
        guard let mutated = try? JSONSerialization.data(withJSONObject: dict),
              let decoded = try? JSONDecoder().decode(EffectParameter.self, from: mutated) else {
            return original
        }
        return decoded
    }
}
