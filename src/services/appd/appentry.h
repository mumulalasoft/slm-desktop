#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

namespace Slm::Appd {

// ── AppState ──────────────────────────────────────────────────────────────────

enum class AppState {
    Created,
    Launching,
    Running,
    Idle,
    Background,
    Unresponsive,
    Crashed,
    Terminated,
};

inline QString appStateString(AppState s)
{
    switch (s) {
    case AppState::Created:      return QStringLiteral("created");
    case AppState::Launching:    return QStringLiteral("launching");
    case AppState::Running:      return QStringLiteral("running");
    case AppState::Idle:         return QStringLiteral("idle");
    case AppState::Background:   return QStringLiteral("background");
    case AppState::Unresponsive: return QStringLiteral("unresponsive");
    case AppState::Crashed:      return QStringLiteral("crashed");
    case AppState::Terminated:   return QStringLiteral("terminated");
    }
    return QStringLiteral("unknown");
}

// ── AppCategory ───────────────────────────────────────────────────────────────

enum class AppCategory {
    Shell,
    GuiApp,
    Gtk,
    Kde,
    QtGeneric,
    QtDesktopFallback,
    CliApp,
    BackgroundAgent,
    SystemIgnore,
    Unknown,
};

inline QString appCategoryString(AppCategory c)
{
    switch (c) {
    case AppCategory::Shell:              return QStringLiteral("shell");
    case AppCategory::GuiApp:            return QStringLiteral("gui-app");
    case AppCategory::Gtk:               return QStringLiteral("gtk");
    case AppCategory::Kde:               return QStringLiteral("kde");
    case AppCategory::QtGeneric:         return QStringLiteral("qt-generic");
    case AppCategory::QtDesktopFallback: return QStringLiteral("qt-desktop-fallback");
    case AppCategory::CliApp:            return QStringLiteral("cli-app");
    case AppCategory::BackgroundAgent:   return QStringLiteral("background-agent");
    case AppCategory::SystemIgnore:      return QStringLiteral("system-ignore");
    case AppCategory::Unknown:           return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

// ── UIExposure ────────────────────────────────────────────────────────────────

struct UIExposure {
    bool dock         = false;
    bool appSwitcher  = false;
    bool launchpad    = false;
    bool systemMonitor = false;
    bool tray         = false;

    QVariantMap toVariantMap() const
    {
        return {
            { QStringLiteral("dock"),          dock },
            { QStringLiteral("appSwitcher"),   appSwitcher },
            { QStringLiteral("launchpad"),     launchpad },
            { QStringLiteral("systemMonitor"), systemMonitor },
            { QStringLiteral("tray"),          tray },
        };
    }
};

// ── WindowInfo ────────────────────────────────────────────────────────────────

struct WindowInfo {
    QString windowId;
    int     workspace = 1;
    QString title;
    bool    focused   = false;
    bool    minimized = false;

    QVariantMap toVariantMap() const
    {
        return {
            { QStringLiteral("windowId"),  windowId },
            { QStringLiteral("workspace"), workspace },
            { QStringLiteral("title"),     title },
            { QStringLiteral("focused"),   focused },
            { QStringLiteral("minimized"), minimized },
        };
    }
};

// ── AppEntry ──────────────────────────────────────────────────────────────────

struct AppEntry {
    QString         appId;
    AppCategory     category    = AppCategory::Unknown;
    AppState        state       = AppState::Created;
    bool            isFocused   = false;
    bool            isVisible   = false;
    bool            isMinimized = false;
    QList<qint64>   pids;
    QList<WindowInfo> windows;
    qint64          lastActiveTs = 0;
    qint64          launchTs     = 0;
    QString         executableHint;
    QString         desktopFile;
    UIExposure      uiExposure;

    int windowCount() const { return windows.size(); }

    QVariantMap toVariantMap() const
    {
        QVariantList pidList;
        for (auto pid : pids) {
            pidList.append(pid);
        }

        QVariantList winList;
        for (const auto &w : windows) {
            winList.append(w.toVariantMap());
        }

        return {
            { QStringLiteral("appId"),        appId },
            { QStringLiteral("category"),     appCategoryString(category) },
            { QStringLiteral("state"),        appStateString(state) },
            { QStringLiteral("isFocused"),    isFocused },
            { QStringLiteral("isVisible"),    isVisible },
            { QStringLiteral("isMinimized"),  isMinimized },
            { QStringLiteral("windowCount"),  windowCount() },
            { QStringLiteral("pids"),         pidList },
            { QStringLiteral("windows"),      winList },
            { QStringLiteral("lastActiveTs"), lastActiveTs },
            { QStringLiteral("uiExposure"),   uiExposure.toVariantMap() },
        };
    }
};

} // namespace Slm::Appd
