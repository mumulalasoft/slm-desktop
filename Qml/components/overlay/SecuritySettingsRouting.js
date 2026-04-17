.pragma library

function deepLinkForCapability(capabilityValue) {
    var capability = String(capabilityValue || "")
    var capabilityLower = capability.toLowerCase()
    if (capabilityLower.indexOf("network") === 0
            || capabilityLower.indexOf("socket") >= 0
            || capabilityLower.indexOf("firewall") >= 0) {
        return "settings://firewall/mode"
    }
    return "settings://permissions/app-secrets"
}
