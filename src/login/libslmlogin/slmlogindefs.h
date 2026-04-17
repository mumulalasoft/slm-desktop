#pragma once
#include <QString>
#include <QtGlobal>

namespace Slm::Login {

constexpr int kCrashLoopThreshold    = 3;
constexpr int kHealthySessionSeconds = 30;
constexpr int kCompositorSocketTimeoutMs = 10000;

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
