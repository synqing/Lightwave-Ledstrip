//
//  RiveViewContainer.swift
//  LightwaveOS
//
//  Container that shows Rive animation or fallback. Stub: always shows fallback.
//

import SwiftUI

enum RiveInput {
    case number(String, Double)
    case bool(String, Bool)
}

typealias RiveInputValue = RiveInput

struct RiveViewContainer<Fallback: View>: View {
    let asset: String
    let inputs: [RiveInput]
    let fallback: Fallback

    init(
        asset: String,
        inputs: [RiveInput] = [],
        onEvent: ((String) -> Void)? = nil,
        tapEventName: String? = nil,
        fallback: Fallback
    ) {
        self.asset = asset
        self.inputs = inputs
        self.fallback = fallback
    }

    var body: some View {
        fallback
    }
}
