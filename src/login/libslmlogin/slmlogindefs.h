#pragma once
#include <QString>
#include <QtGlobal>

namespace Slm::Login {

constexpr int  kCrashLoopThreshold           = 5;
constexpr int  kHealthySessionSeconds        = 30;
constexpr int  kCompositorSocketTimeoutMs    = 5000;   // 5s — actual connect test
constexpr int  kShellFirstFrameTimeoutMs     = 10000;  // 10s — shell must render first frame
constexpr int  kShellFastExitMs              = 3000;   // < 3s exit = hard crash
constexpr int  kRecoveryFirstFrameStableMs   = 5000;   // 5s — recovery UI must be visible and stable

// Compatibility-critical compositor socket name.
// Keep this at wayland-0 so strict sandboxes (Snap/Flatpak) can connect without
// path policy violations.
constexpr const char kDefaultWaylandSocketName[] = "wayland-0";

// Legacy SLM alias kept for internal/backward compatibility.
constexpr const char kLegacySlmWaylandSocketAlias[] = "slm-wayland-0";

enum class StartupMode {
    Normal,
    Safe,
    Recovery,
};

inline QString startupModeToString(StartupMode mode)
{
    switch (mode) {
    case StartupMode::Normal:   return QStringLiteral("normal");
    case StartupMode::Safe:     return QStringLiteral("safe");
    case StartupMode::Recovery: return QStringLiteral("recovery");
    }
    return QStringLiteral("normal");
}

inline StartupMode startupModeFromString(const QString &s)
{
    if (s == QStringLiteral("safe"))     return StartupMode::Safe;
    if (s == QStringLiteral("recovery")) return StartupMode::Recovery;
    return StartupMode::Normal;
}

} // namespace Slm::Login
