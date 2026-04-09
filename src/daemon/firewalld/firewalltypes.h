#pragma once

#include <QString>

namespace Slm::Firewall {

enum class FirewallMode {
    Home,
    Public,
    Custom,
};

enum class PolicyDecision {
    Allow,
    Deny,
    Prompt,
};

inline QString firewallModeToString(FirewallMode mode)
{
    switch (mode) {
    case FirewallMode::Home:
        return QStringLiteral("home");
    case FirewallMode::Public:
        return QStringLiteral("public");
    case FirewallMode::Custom:
        return QStringLiteral("custom");
    }
    return QStringLiteral("home");
}

inline FirewallMode firewallModeFromString(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QLatin1String("public")) {
        return FirewallMode::Public;
    }
    if (normalized == QLatin1String("custom")) {
        return FirewallMode::Custom;
    }
    return FirewallMode::Home;
}

inline QString policyDecisionToString(PolicyDecision decision)
{
    switch (decision) {
    case PolicyDecision::Allow:
        return QStringLiteral("allow");
    case PolicyDecision::Deny:
        return QStringLiteral("deny");
    case PolicyDecision::Prompt:
        return QStringLiteral("prompt");
    }
    return QStringLiteral("prompt");
}

} // namespace Slm::Firewall
