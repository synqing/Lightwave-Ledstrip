//
//  RiveAppleRuntimeAdapter.swift
//  LightwaveOS
//
//  Adapter for the experimental Apple runtime (Swift-first).
//

import SwiftUI
@_spi(RiveExperimental) import RiveRuntime

@MainActor
final class RiveAppleRuntimeAdapter: RiveRuntime {
    private let riveView: RiveUIView
    private let riveHolder = RiveHolder()
    private var worker: Worker?
    private var eventHandler: ((String) -> Void)?

    init(asset: RiveAssetDescriptor, bundle: Bundle) {
        let worker = Worker()
        self.worker = worker

        let holder = riveHolder
        self.riveView = RiveUIView(rive: {
            let file = try await File(source: .local(asset.fileName, bundle), worker: worker)
            let artboard = try await file.createArtboard(asset.artboardName)
            let stateMachine = try await artboard.createStateMachine(asset.stateMachineName)
            let rive = try await Rive(file: file, artboard: artboard, stateMachine: stateMachine, dataBind: .auto)
            holder.rive = rive
            return rive
        })
    }

    var view: AnyView {
        AnyView(riveView.view())
    }

    func setBool(_ name: String, value: Bool) {
        guard let instance = riveHolder.rive?.viewModelInstance else { return }
        instance.setValue(of: BoolProperty(path: name), to: value)
    }

    func setNumber(_ name: String, value: Double) {
        guard let instance = riveHolder.rive?.viewModelInstance else { return }
        instance.setValue(of: NumberProperty(path: name), to: Float(value))
    }

    func setText(_ name: String, value: String) {
        guard let instance = riveHolder.rive?.viewModelInstance else { return }
        instance.setValue(of: StringProperty(path: name), to: value)
    }

    func fireTrigger(_ name: String) {
        guard let instance = riveHolder.rive?.viewModelInstance else { return }
        instance.fire(trigger: TriggerProperty(path: name))
    }

    func setEventHandler(_ handler: @escaping (String) -> Void) {
        eventHandler = handler
        // Experimental runtime does not yet expose event callbacks.
    }
}

private final class RiveHolder {
    var rive: Rive?
}
