//
//  RiveViewContainer.swift
//  LightwaveOS
//
//  Compile-safe fallback container that renders provided SwiftUI fallback content.
//

import SwiftUI

enum RiveInputValue: Sendable, Hashable {
    case bool(String, Bool)
    case number(String, Double)
}

struct RiveViewContainer: View {
    let asset: RiveAsset
    let inputs: [RiveInputValue]
    let onEvent: ((String) -> Void)?
    let tapEventName: String?
    let fallback: AnyView

    init(
        asset: RiveAsset,
        inputs: [RiveInputValue] = [],
        onEvent: ((String) -> Void)? = nil,
        tapEventName: String? = nil,
        fallback: AnyView
    ) {
        self.asset = asset
        self.inputs = inputs
        self.onEvent = onEvent
        self.tapEventName = tapEventName
        self.fallback = fallback
    }

    var body: some View {
        fallback
            .contentShape(Rectangle())
            .onTapGesture {
                if let tapEventName {
                    onEvent?(tapEventName)
                }
            }
            .accessibilityLabel(Text(asset.fileName))
    }
}

