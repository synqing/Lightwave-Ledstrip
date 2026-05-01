//
//  EffectParameterSheet.swift
//  LightwaveOS
//
//  Phase 2 Task P2-1: end-user runtime parameter sheet.
//
//  Renders one row per visible `EffectParameter` returned by the firmware's
//  `effects.parameters` endpoint. Per F-4 of the parity spec:
//
//      FLOAT       → Slider over [min, max]
//      INT         → Stepper
//      BOOL        → Toggle
//      ENUM        → Picker (integer-keyed; firmware does not yet expose case names)
//      .unknown    → fallback Slider
//      displayName == nil OR empty → hidden (skipped entirely)
//
//  Slider on-change is debounced through `EffectViewModel.setRuntimeParameter`,
//  which holds for 150 ms before issuing the REST POST so a continuous drag
//  produces at most one network round-trip every 150 ms. Toggles, steppers and
//  pickers fire immediately on change because they emit discrete events that
//  cannot meaningfully be coalesced.
//

import SwiftUI

// MARK: - Control kind

/// The UI control-kind a single `EffectParameter` should be rendered as.
/// Driven by the F-4 mapping; used by `EffectParameterSheet` to switch on
/// the row layout per parameter.
enum EffectParameterControlKind: Equatable, Sendable {
    /// Continuous numeric value — render as `Slider`.
    case slider
    /// Discrete integer — render as `Stepper`.
    case stepper
    /// Boolean flag — render as `Toggle`.
    case toggle
    /// Enumerated choice — render as integer-keyed `Picker`.
    case picker
    /// Parameter has no display name and must not be shown.
    case hidden
}

// MARK: - EffectParameter UI extension

extension EffectParameter {
    /// Decide which control to render for this parameter.
    ///
    /// - Hidden parameters (no displayName) report `.hidden` so the sheet can
    ///   skip them in the same switch that drives every other case.
    /// - `nil` and `.unknown` parameter types fall back to `.slider` per F-4 —
    ///   the slider over `[min, max]` is the safest default for an untyped value.
    func controlKind() -> EffectParameterControlKind {
        guard isVisible() else { return .hidden }

        switch parameterType {
        case .float?:
            return .slider
        case .int?:
            return .stepper
        case .bool?:
            return .toggle
        case .enumerated?:
            return .picker
        case .unknown?, .none:
            // Fallback path: legacy firmware (nil) or a future enum case the
            // client has not learned about (.unknown) still gets a usable
            // continuous control.
            return .slider
        }
    }

    /// Whether this parameter should be exposed to the end user.
    ///
    /// A parameter is hidden when it lacks a populated `displayName`. Firmware
    /// uses this to surface only the tunable knobs intended for end users; the
    /// rest are internal scratch values that should never appear in the UI.
    func isVisible() -> Bool {
        guard let label = displayName, !label.isEmpty else { return false }
        return true
    }
}

// MARK: - The sheet

struct EffectParameterSheet: View {
    /// Identifier of the effect whose parameters to load. The sheet refetches
    /// whenever this binding changes so the parent can recycle the view.
    @Binding var effectId: Int

    @Environment(\.dismiss) private var dismiss
    @Environment(AppViewModel.self) private var appVM

    /// Indicates whether the initial parameter fetch is still in flight.
    @State private var isLoading: Bool = true
    /// User-facing error message if the fetch fails.
    @State private var loadError: String?

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: Spacing.md) {
                    if isLoading {
                        loadingState
                    } else if let message = loadError {
                        errorState(message: message)
                    } else if visibleParameters.isEmpty {
                        emptyState
                    } else {
                        ForEach(visibleParameters, id: \.name) { parameter in
                            ParameterRow(
                                parameter: parameter,
                                effectId: effectId
                            )
                            .environment(appVM)
                        }
                    }
                }
                .padding(Spacing.lg)
            }
            .background(Color.lwBase.ignoresSafeArea())
            .navigationTitle("Customise Effect")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") { dismiss() }
                        .foregroundStyle(Color.lwGold)
                }
            }
            .task(id: effectId) {
                await reload()
            }
        }
    }

    // MARK: - Derived state

    private var visibleParameters: [EffectParameter] {
        appVM.effects.currentParameters.filter { $0.isVisible() }
    }

    // MARK: - Subviews

    private var loadingState: some View {
        VStack(spacing: Spacing.md) {
            ProgressView()
                .tint(Color.lwGold)
            Text("Loading parameters…")
                .font(.body)
                .foregroundStyle(Color.lwTextSecondary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, Spacing.lg)
    }

    private var emptyState: some View {
        VStack(spacing: Spacing.sm) {
            Image(systemName: "slider.horizontal.3")
                .font(.system(size: 32))
                .foregroundStyle(Color.lwTextTertiary)
            Text("No tunable parameters")
                .font(.body.weight(.semibold))
                .foregroundStyle(Color.lwTextSecondary)
            Text("This effect doesn't expose any user-tunable values.")
                .font(.caption)
                .foregroundStyle(Color.lwTextTertiary)
                .multilineTextAlignment(.center)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, Spacing.lg)
    }

    private func errorState(message: String) -> some View {
        VStack(spacing: Spacing.sm) {
            Image(systemName: "exclamationmark.triangle.fill")
                .font(.system(size: 32))
                .foregroundStyle(Color.lwError)
            Text("Couldn't load parameters")
                .font(.body.weight(.semibold))
                .foregroundStyle(Color.lwTextPrimary)
            Text(message)
                .font(.caption)
                .foregroundStyle(Color.lwTextTertiary)
                .multilineTextAlignment(.center)
            Button("Retry") {
                Task { await reload() }
            }
            .foregroundStyle(Color.lwGold)
            .padding(.top, Spacing.xs)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, Spacing.lg)
    }

    // MARK: - Actions

    private func reload() async {
        guard let client = appVM.effects.restClient else {
            loadError = "Not connected to a device."
            isLoading = false
            return
        }
        isLoading = true
        loadError = nil
        await appVM.effects.loadEffectParameters(effectId: effectId, client: client)
        isLoading = false
    }
}

// MARK: - Per-parameter row

/// One row within the sheet — picks its control based on the parameter's
/// `controlKind()`. Holds local state per row so SwiftUI re-renders only the
/// affected control on a change rather than the whole sheet.
private struct ParameterRow: View {
    let parameter: EffectParameter
    let effectId: Int

    @Environment(AppViewModel.self) private var appVM
    @State private var localValue: Double

    init(parameter: EffectParameter, effectId: Int) {
        self.parameter = parameter
        self.effectId = effectId
        _localValue = State(initialValue: Double(parameter.value))
    }

    var body: some View {
        switch parameter.controlKind() {
        case .slider:
            sliderRow
        case .stepper:
            stepperRow
        case .toggle:
            toggleRow
        case .picker:
            pickerRow
        case .hidden:
            EmptyView()
        }
    }

    // MARK: - Common chrome

    private var label: some View {
        Text(parameter.displayName ?? parameter.name)
            .font(.cardLabel)
            .foregroundStyle(Color.lwTextSecondary)
            .textCase(.uppercase)
    }

    private func sendValue(_ newValue: Double) {
        appVM.effects.setRuntimeParameter(
            name: parameter.name,
            value: newValue,
            client: appVM.effects.restClient
        )
    }

    // MARK: - Slider (FLOAT and fallback)

    private var sliderRow: some View {
        LWCard {
            VStack(alignment: .leading, spacing: Spacing.sm) {
                HStack {
                    label
                    Spacer()
                    Text(formattedValue)
                        .font(.sliderValue)
                        .monospacedDigit()
                        .foregroundStyle(Color.lwGold)
                }

                Slider(
                    value: $localValue,
                    in: Double(parameter.min)...Double(parameter.max)
                )
                .tint(Color.lwGold)
                .onChange(of: localValue) { _, newValue in
                    // Debounce inside the view-model, not here. `setRuntimeParameter`
                    // collapses rapid drags into one POST every 150 ms.
                    sendValue(newValue)
                }
            }
        }
    }

    // MARK: - Stepper (INT)

    private var stepperRow: some View {
        LWCard {
            HStack {
                label
                Spacer()
                Stepper(
                    value: $localValue,
                    in: Double(parameter.min)...Double(parameter.max),
                    step: 1
                ) {
                    Text("\(Int(localValue))")
                        .font(.sliderValue)
                        .monospacedDigit()
                        .foregroundStyle(Color.lwGold)
                }
                .labelsHidden()
                .onChange(of: localValue) { _, newValue in
                    // Discrete steps — no debounce required.
                    sendValue(newValue.rounded())
                }
            }
        }
    }

    // MARK: - Toggle (BOOL)

    private var toggleRow: some View {
        LWCard {
            HStack {
                label
                Spacer()
                Toggle(
                    isOn: Binding(
                        get: { localValue >= 0.5 },
                        set: { newOn in
                            let newValue: Double = newOn ? 1.0 : 0.0
                            localValue = newValue
                            sendValue(newValue)
                        }
                    )
                ) { EmptyView() }
                .labelsHidden()
                .tint(Color.lwGold)
            }
        }
    }

    // MARK: - Picker (ENUM)

    private var pickerRow: some View {
        LWCard {
            HStack {
                label
                Spacer()
                Picker(
                    selection: Binding(
                        get: { Int(localValue.rounded()) },
                        set: { newCase in
                            localValue = Double(newCase)
                            sendValue(Double(newCase))
                        }
                    ),
                    label: EmptyView()
                ) {
                    ForEach(enumCases, id: \.self) { caseValue in
                        Text("\(caseValue)").tag(caseValue)
                    }
                }
                .pickerStyle(.menu)
                .tint(Color.lwGold)
            }
        }
    }

    /// Integer cases between `min` and `max` (inclusive). Firmware does not yet
    /// emit human-readable names for enum cases, so we fall back to the integer
    /// value as the visible label. When firmware grows a case-name field, swap
    /// the `Text` above to use it.
    private var enumCases: [Int] {
        let lower = Int(parameter.min.rounded(.up))
        let upper = Int(parameter.max.rounded(.down))
        guard upper >= lower else { return [lower] }
        return Array(lower...upper)
    }

    private var formattedValue: String {
        // Sliders show one decimal place; that's enough resolution for
        // a typical FLOAT range (e.g. 0.0–3.0) while staying glanceable.
        String(format: "%.1f", localValue)
    }
}

// MARK: - Preview

#Preview("Effect Parameter Sheet") {
    struct PreviewWrapper: View {
        @State private var effectId: Int = 4878
        var body: some View {
            EffectParameterSheet(effectId: $effectId)
                .environment(AppViewModel())
        }
    }
    return PreviewWrapper()
}
