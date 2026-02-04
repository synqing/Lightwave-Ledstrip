//
//  RiveViewContainer.swift
//  LightwaveOS
//
//  SwiftUI wrapper that manages lifecycle + inputs for a Rive asset.
//

import SwiftUI

struct RiveViewContainer: View {
    let asset: RiveAssetDescriptor
    var inputs: [RiveInputValue] = []
    var onEvent: ((String) -> Void)? = nil
    var tapEventName: String? = nil
    var fallback: AnyView? = nil

    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @StateObject private var model: RiveViewContainerModel

    init(
        asset: RiveAssetDescriptor,
        inputs: [RiveInputValue] = [],
        onEvent: ((String) -> Void)? = nil,
        tapEventName: String? = nil,
        fallback: AnyView? = nil
    ) {
        self.asset = asset
        self.inputs = inputs
        self.onEvent = onEvent
        self.tapEventName = tapEventName
        self.fallback = fallback
        _model = StateObject(wrappedValue: RiveViewContainerModel(asset: asset))
    }

    var body: some View {
        Group {
            if reduceMotion || model.isUnavailable {
                fallbackView
            } else {
                model.view
                    .opacity(model.isReady ? 1.0 : 0.0)
                    .overlay {
                        if !model.isReady {
                            fallbackView
                        }
                    }
            }
        }
        .contentShape(Rectangle())
        .simultaneousGesture(
            TapGesture().onEnded {
                if let tapEventName, let onEvent {
                    onEvent(tapEventName)
                }
            }
        )
        .task {
            guard !reduceMotion else { return }
            await model.prepare()
            model.setEventHandler(onEvent)
            model.apply(inputs: inputs)
        }
        .onChange(of: reduceMotion) { _, newValue in
            guard !newValue else { return }
            Task { @MainActor in
                await model.prepare()
                model.setEventHandler(onEvent)
                model.apply(inputs: inputs)
            }
        }
        .onChange(of: inputs) { _, newInputs in
            model.apply(inputs: newInputs)
        }
    }

    @ViewBuilder
    private var fallbackView: some View {
        if let fallback {
            fallback
        } else {
            EmptyView()
        }
    }
}

@MainActor
final class RiveViewContainerModel: ObservableObject {
    @Published var view: AnyView = AnyView(EmptyView())
    @Published private(set) var isReady = false
    @Published private(set) var isUnavailable = false

    private let asset: RiveAssetDescriptor
    private var runtime: (any RiveRuntime)?

    init(asset: RiveAssetDescriptor) {
        self.asset = asset
    }

    func prepare() async {
        guard !isReady else { return }
        RiveRuntimeFactory.logRuntimeOnce()

        let availability = RiveRuntimeFactory.assetAvailability(for: asset)
        guard availability.isAvailable else {
            isUnavailable = true
            RiveRuntimeFactory.logAvailabilityFailure(availability, asset: asset)
            return
        }
        isUnavailable = false

        guard let runtime = RiveRuntimeFactory.makeRuntime(for: asset, availability: availability) else {
            isUnavailable = true
            return
        }
        self.runtime = runtime
        self.view = runtime.view
        self.isReady = true
    }

    func apply(inputs: [RiveInputValue]) {
        guard let runtime else { return }
        inputs.forEach { input in
            switch input.kind {
            case .bool(let value):
                runtime.setBool(input.name, value: value)
            case .number(let value):
                runtime.setNumber(input.name, value: value)
            case .text(let value):
                runtime.setText(input.name, value: value)
            case .trigger(let value):
                if value {
                    runtime.fireTrigger(input.name)
                }
            }
        }
    }

    func setEventHandler(_ handler: ((String) -> Void)?) {
        guard let handler else { return }
        runtime?.setEventHandler(handler)
    }
}
