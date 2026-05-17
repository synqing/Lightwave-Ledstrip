#pragma once

#include <stddef.h>
#include <stdint.h>

namespace lightwaveos {
namespace network {

// K1 supports AP-only OR STA-only modes (never concurrent). Provisioning
// hands off STA credentials from a client joining the K1 AP, so it is only
// meaningful while the device is currently serving as an Access Point.
enum class NetworkProvisionGate : uint8_t {
    Allowed = 0,
    NotApMode,
};

constexpr NetworkProvisionGate evaluateNetworkProvisionGate(bool requestFromApMode) {
    if (!requestFromApMode) {
        return NetworkProvisionGate::NotApMode;
    }
    return NetworkProvisionGate::Allowed;
}

constexpr bool isNetworkProvisionPasswordLengthAllowed(size_t passwordLength) {
    return passwordLength == 0 || (passwordLength >= 8 && passwordLength <= 64);
}

constexpr const char* networkProvisionGateMessage(NetworkProvisionGate gate) {
    switch (gate) {
        case NetworkProvisionGate::Allowed:
            return "";
        case NetworkProvisionGate::NotApMode:
            return "Provisioning is only available from AP mode";
    }
    return "Provisioning unavailable";
}

} // namespace network
} // namespace lightwaveos
