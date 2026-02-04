//
//  RiveRuntimeFactory.swift
//  LightwaveOS
//
//  Chooses the runtime implementation based on build settings.
//

import Foundation
import RiveRuntime

enum RiveAssetAvailability: Equatable {
    case ok
    case fileMissing
    case artboardMissing(String)
    case stateMachineMissing(String)

    var isAvailable: Bool {
        if case .ok = self { return true }
        return false
    }

    var message: String {
        switch self {
        case .ok:
            return "ok"
        case .fileMissing:
            return "file not found in bundle"
        case .artboardMissing(let name):
            return "artboard not found: \(name)"
        case .stateMachineMissing(let name):
            return "state machine not found: \(name)"
        }
    }
}

@MainActor
enum RiveRuntimeFactory {
    private static var didLogRuntime = false

    static func assetAvailability(for asset: RiveAssetDescriptor, bundle: Bundle = .main) -> RiveAssetAvailability {
        guard bundle.url(forResource: asset.fileName, withExtension: "riv") != nil else {
            return .fileMissing
        }

        do {
            let model = try RiveModel(fileName: asset.fileName, extension: ".riv", in: bundle, loadCdn: false)

            if let artboardName = asset.artboardName {
                let artboardNames = model.riveFile.artboardNames()
                guard artboardNames.contains(artboardName) else {
                    return .artboardMissing(artboardName)
                }

                if let stateMachineName = asset.stateMachineName {
                    guard let artboard = try? model.riveFile.artboard(fromName: artboardName) else {
                        return .artboardMissing(artboardName)
                    }
                    let stateMachineNames = artboard.stateMachineNames()
                    guard stateMachineNames.contains(stateMachineName) else {
                        return .stateMachineMissing(stateMachineName)
                    }
                }
            } else if let stateMachineName = asset.stateMachineName {
                guard let artboard = try? model.riveFile.artboard() else {
                    return .artboardMissing("(default)")
                }
                let stateMachineNames = artboard.stateMachineNames()
                guard stateMachineNames.contains(stateMachineName) else {
                    return .stateMachineMissing(stateMachineName)
                }
            }
        } catch {
            return .fileMissing
        }

        return .ok
    }

    static func logAvailabilityFailure(_ availability: RiveAssetAvailability, asset: RiveAssetDescriptor) {
        guard availability != .ok else { return }
        print("[Rive] \(asset.fileName).riv unavailable: \(availability.message)")
    }

    @MainActor
    static func makeRuntime(
        for asset: RiveAssetDescriptor,
        bundle: Bundle = .main,
        availability: RiveAssetAvailability? = nil
    ) -> (any RiveRuntime)? {
        let availability = availability ?? assetAvailability(for: asset, bundle: bundle)
        guard availability.isAvailable else {
            return nil
        }

        #if RIVE_RUNTIME_CPP
        return RiveCppRuntimeAdapter(asset: asset, bundle: bundle)
        #else
        return RiveAppleRuntimeAdapter(asset: asset, bundle: bundle)
        #endif
    }

    static var runtimeName: String {
        #if RIVE_RUNTIME_CPP
        return "cpp"
        #else
        return "apple"
        #endif
    }

    static var runtimeDisplayName: String {
        switch runtimeName {
        case "cpp":
            return "Cpp"
        default:
            return "Apple"
        }
    }

    static func logRuntimeOnce() {
        guard !didLogRuntime else { return }
        didLogRuntime = true
        print("[Rive] Runtime: \(runtimeDisplayName)")
    }
}
