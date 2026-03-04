//
//  RiveRuntime.swift
//  LightwaveOS
//
//  Lightweight runtime abstraction used by fallback Rive wrappers.
//

import Foundation

protocol RiveRuntimeAdapter: Sendable {}

enum RiveRuntime: String, Sendable {
    case apple
    case cpp
}

