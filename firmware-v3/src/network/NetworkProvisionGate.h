#pragma once

#include <stddef.h>
#include <stdint.h>

namespace lightwaveos {
namespace network {

enum class NetworkProvisionGate : uint8_t {
    Allowed = 0,
    WifiApOnlyBuild,
    NonValidationBuild,
    NotApMode,
};

constexpr NetworkProvisionGate evaluateNetworkProvisionGate(bool wifiApOnlyBuild,
                                                            bool staValidationBuild,
                                                            bool requestFromApMode) {
    if (wifiApOnlyBuild) {
        return NetworkProvisionGate::WifiApOnlyBuild;
    }
    if (!staValidationBuild) {
        return NetworkProvisionGate::NonValidationBuild;
    }
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
        case NetworkProvisionGate::WifiApOnlyBuild:
            return "Provisioning unavailable in WIFI_AP_ONLY build";
        case NetworkProvisionGate::NonValidationBuild:
            return "Provisioning unavailable outside LW_STA_VALIDATION_BUILD";
        case NetworkProvisionGate::NotApMode:
            return "Provisioning is only available from AP mode";
    }
    return "Provisioning unavailable";
}

} // namespace network
} // namespace lightwaveos
