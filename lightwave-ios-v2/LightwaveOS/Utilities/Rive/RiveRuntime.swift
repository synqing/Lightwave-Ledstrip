//
//  RiveRuntime.swift
//  LightwaveOS
//
//  Shared abstraction for Rive runtime integration.
//  British English for consistency across the codebase.
//

import SwiftUI

// MARK: - Runtime Protocol

@MainActor
protocol RiveRuntime: AnyObject {
    var view: AnyView { get }

    func setBool(_ name: String, value: Bool)
    func setNumber(_ name: String, value: Double)
    func setText(_ name: String, value: String)
    func fireTrigger(_ name: String)

    func setEventHandler(_ handler: @escaping (String) -> Void)
}

// MARK: - Input Values

struct RiveInputValue: Equatable {
    let name: String
    let kind: RiveInputKind

    static func bool(_ name: String, _ value: Bool) -> RiveInputValue {
        RiveInputValue(name: name, kind: .bool(value))
    }

    static func number(_ name: String, _ value: Double) -> RiveInputValue {
        RiveInputValue(name: name, kind: .number(value))
    }

    static func text(_ name: String, _ value: String) -> RiveInputValue {
        RiveInputValue(name: name, kind: .text(value))
    }

    static func trigger(_ name: String, _ value: Bool) -> RiveInputValue {
        RiveInputValue(name: name, kind: .trigger(value))
    }
}

enum RiveInputKind: Equatable {
    case bool(Bool)
    case number(Double)
    case text(String)
    case trigger(Bool)
}
