//
//  RiveCppRuntimeAdapter.swift
//  LightwaveOS
//
//  Adapter for the stable C++ runtime (RiveViewModel).
//

import SwiftUI
import RiveRuntime

@MainActor
final class RiveCppRuntimeAdapter: RiveRuntime {
    private let viewModel: RiveViewModel
    private var eventHandler: ((String) -> Void)?

    init(asset: RiveAssetDescriptor, bundle: Bundle) {
        self.viewModel = RiveViewModel(
            fileName: asset.fileName,
            in: bundle,
            stateMachineName: asset.stateMachineName,
            fit: .contain,
            alignment: .center,
            autoPlay: true,
            artboardName: asset.artboardName
        )
    }

    var view: AnyView {
        viewModel.view()
    }

    func setBool(_ name: String, value: Bool) {
        viewModel.setInput(name, value: value)
    }

    func setNumber(_ name: String, value: Double) {
        viewModel.setInput(name, value: value)
    }

    func setText(_ name: String, value: String) {
        do {
            try viewModel.setTextRunValue(name, textValue: value)
        } catch {
            // Ignore missing text run until assets are finalised.
        }
    }

    func fireTrigger(_ name: String) {
        viewModel.triggerInput(name)
    }

    func setEventHandler(_ handler: @escaping (String) -> Void) {
        eventHandler = handler
    }
}
