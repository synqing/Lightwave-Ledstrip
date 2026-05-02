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

    // MARK: - Pagination state (heap-stability mitigation)
    //
    // The connect-time effect hydration was previously a single 22.5 KB JSON
    // blob (limit=200) that fragmented K1 V2's internal heap and latched the
    // heap-shedding recovery. We now fetch the first `connectInitialLimit`
    // effects on connect (small enough to fit in the EffectPill / first
    // grid view) and lazy-load further pages as the user scrolls.
    //
    // `hasLoadedAllEffects` flips to true once a fetch returns fewer effects
    // than `pageLimit` — the firmware has nothing more to give. `isLoadingPage`
    // gates concurrent fetches so a fast scroll cannot trigger N parallel GETs.

    /// Cap applied on the connect-time fetch. Tuned to keep the response under
    /// ~3 KB which is comfortably below K1's largest free internal-heap block.
    static let connectInitialLimit = 20

    /// Page size for subsequent lazy fetches. Larger than the initial hydration
    /// because by then the heap is no longer under the connect-time pressure.
    static let pageLimit = 40

    /// Set when the most recent fetch returned fewer rows than `pageLimit`,
    /// indicating the firmware effect catalogue has been fully paged in.
    @ObservationIgnored
    private(set) var hasLoadedAllEffects = false

    /// Re-entry guard for paginated fetches. Prevents a fast scroll from
    /// firing multiple overlapping GETs.
    @ObservationIgnored
    private var isLoadingPage = false

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

    /// Connect-time hydration: fetches only `connectInitialLimit` effects so the
    /// JSON response stays small enough to avoid fragmenting K1 V2's internal
    /// heap. Resets the pagination cursors. Subsequent pages are loaded lazily
    /// via `loadNextPageIfNeeded(currentIndex:)` as the user scrolls or when
    /// `loadAllEffectsIfNeeded()` is invoked from a code path that needs the
    /// full catalogue.
    func loadEffects() async {
        guard let client = restClient else { return }

        // Reset pagination state on each (re)connect so a previously-loaded
        // catalogue does not bleed into a new device's hydration.
        hasLoadedAllEffects = false
        allEffects = []

        do {
            let response = try await client.getEffects(page: 1, limit: Self.connectInitialLimit)

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

            // If the firmware returned the full catalogue inside the initial
            // page (small builds with <connectInitialLimit effects), latch
            // hasLoadedAllEffects so we never bother paginating further.
            if response.data.effects.count < Self.connectInitialLimit {
                hasLoadedAllEffects = true
            }
            print("Loaded \(allEffects.count) effects (initial page)")

        } catch {
            print("Error loading effects: \(error)")
        }
    }

    /// Fetch the next page of effects if the user has scrolled close to the
    /// bottom of the loaded list. Idempotent: safe to call on every cell
    /// appearance. The trigger threshold is intentionally lenient — fetching
    /// one page early is far cheaper than the spinner the user would see if we
    /// waited for the absolute final cell.
    ///
    /// - Parameter currentIndex: The index of the cell that just appeared.
    func loadNextPageIfNeeded(currentIndex: Int) async {
        guard !hasLoadedAllEffects, !isLoadingPage else { return }
        // Trigger one full page before the end of the loaded list.
        let triggerThreshold = max(0, allEffects.count - Self.pageLimit / 2)
        guard currentIndex >= triggerThreshold else { return }
        await loadNextPage()
    }

    /// Load the entire effect catalogue by paging until exhausted. Called by
    /// surfaces that need every effect available before they can render
    /// correctly (e.g. zone effect picker that displays the full grouped list
    /// up-front). No-op once `hasLoadedAllEffects` is set.
    func loadAllEffectsIfNeeded() async {
        guard !hasLoadedAllEffects else { return }
        // Page through until the firmware reports an empty / short page.
        while !hasLoadedAllEffects && !isLoadingPage {
            await loadNextPage()
        }
    }

    /// Internal: fetch the next page based on the current loaded count.
    /// Caller is responsible for the `hasLoadedAllEffects` / `isLoadingPage`
    /// guards above.
    ///
    /// Page selection: the connect-time fetch returns `connectInitialLimit`
    /// rows and we then issue `pageLimit`-sized requests starting from offset
    /// `connectInitialLimit`. Because the firmware is page+limit-based rather
    /// than offset-based, we walk pages with `limit = pageLimit`, skip the
    /// initial-overlap rows by ID, and rely on the dedupe filter below to
    /// drop any rows already in `allEffects`.
    private func loadNextPage() async {
        guard let client = restClient, !hasLoadedAllEffects, !isLoadingPage else { return }
        isLoadingPage = true
        defer { isLoadingPage = false }

        // Compute the next page number. We walk pages of `pageLimit` rows;
        // the page that begins at the row immediately after the loaded count
        // is `(allEffects.count / pageLimit) + 1`. Integer-divide rounds
        // toward zero, which is exactly the behaviour we want here.
        let nextPage = (allEffects.count / Self.pageLimit) + 1

        do {
            let response = try await client.getEffects(page: nextPage, limit: Self.pageLimit)
            let newEffects = response.data.effects.map { effect in
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

            // De-duplicate against the existing list. The initial connect
            // fetch uses a smaller limit than the page fetch, so the first
            // paginated page can overlap rows we already hold.
            let existingIds = Set(allEffects.map(\.id))
            let appended = newEffects.filter { !existingIds.contains($0.id) }
            allEffects.append(contentsOf: appended)

            if newEffects.count < Self.pageLimit {
                hasLoadedAllEffects = true
            }
            print("Loaded next effects page \(nextPage): +\(appended.count) (total \(allEffects.count))")
        } catch {
            print("Error loading next effects page: \(error)")
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
    /// 150 ms is the project-wide slider debounce floor (see CLAUDE.md).
    private static let parameterDebounceInterval: Duration = .milliseconds(150)

    /// Fetch the tunable runtime parameters for `effectId` and store them in
    /// `currentParameters`. Replaces any previously loaded list.
    func loadEffectParameters(effectId: Int, client: RESTClient) async {
        do {
            let envelope = try await client.getEffectParameters(effectId: effectId)
            self.currentParameters = envelope.parameters
            print("Loaded \(envelope.parameters.count) parameters for effect \(effectId)")
        } catch {
            print("Error loading effect parameters for \(effectId): \(error)")
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
                print("Error setting runtime parameter \(name): \(error)")
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
