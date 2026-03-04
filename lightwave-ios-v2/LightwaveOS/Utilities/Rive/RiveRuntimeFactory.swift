//
//  RiveRuntimeFactory.swift
//  LightwaveOS
//
//  Creates the default runtime adapter used by fallback Rive wrappers.
//

import Foundation

enum RiveRuntimeFactory {
    static func makeAdapter(preferred: RiveRuntime = .apple) -> RiveRuntimeAdapter {
        switch preferred {
        case .apple:
            return RiveAppleRuntimeAdapter()
        case .cpp:
            return RiveCppRuntimeAdapter()
        }
    }
}

