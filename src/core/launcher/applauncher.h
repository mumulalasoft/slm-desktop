#pragma once

#include <QProcessEnvironment>
#include <QString>

class LaunchEnvResolver;

// AppLauncher — launch helper that injects the resolved effective environment.
//
// Used by AppExecutionGate when a LaunchEnvResolver is attached.
// Contains the low-level .desktop + QProcess launch implementations that accept
// an explicit QProcessEnvironment so the caller can overlay env vars without
// modifying the parent process environment.
//
// All functions return true on success and log on failure.
//
struct AppLaunchResult {
    bool    ok        = false;
    qint64  pid       = -1;
    bool    privateSessionBus = false;
    QString displayName;
    QString executable;
    QString launchCommand;
    QString iconName;
    QString desktopFilePath;
    QString error;
};

class AppLauncher
{
public:
    // Normalize app process environment before launch while preserving
    // freedesktop/runtime compatibility expectations.
    // In SLM sessions this mainly maps internal WAYLAND_DISPLAY values
    // (e.g. slm-wayland-0) to the compatibility socket name wayland-0.
    static QProcessEnvironment prepareLaunchEnvironment(const QProcessEnvironment &env);

    // Expand a .desktop Exec template for a no-argument launch.
    // Field codes that require a file/URL target are stripped.
    static QString expandDesktopExecCommand(const QString &desktopFilePath);

    // Launch a .desktop file with the supplied environment applied.
    // The normal path reads Exec= and starts it directly; compatibility fallback
    // uses detached `gio launch` so GLib/DBus timeouts cannot block the shell.
    static AppLaunchResult launchDesktopFile(const QString &desktopFilePath,
                                             const QProcessEnvironment &env);

    // Launch a raw shell command via QProcess::startDetached with explicit env.
    static bool launchCommand(const QString &command,
                              const QString &workingDirectory,
                              const QProcessEnvironment &env);

    // Derive an app ID from a .desktop file path.
    // e.g. "/usr/share/applications/firefox.desktop" → "firefox"
    static QString appIdFromDesktopFile(const QString &desktopFilePath);

    // Derive an app ID from a desktop ID string (strips ".desktop" suffix).
    static QString appIdFromDesktopId(const QString &desktopId);
};
