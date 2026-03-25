#pragma once

#include <QProcessEnvironment>
#include <QString>

class LaunchEnvResolver;

// AppLauncher — launch helper that injects the resolved effective environment.
//
// Used by AppExecutionGate when a LaunchEnvResolver is attached.
// Contains the low-level GIO + QProcess launch implementations that accept
// an explicit QProcessEnvironment so the caller can overlay env vars without
// modifying the parent process environment.
//
// All functions return true on success and log on failure.
//
struct AppLaunchResult {
    bool    ok        = false;
    QString displayName;
    QString executable;
    QString iconName;
    QString desktopFilePath;
};

class AppLauncher
{
public:
    // Launch a .desktop file via GIO with the supplied environment applied.
    // Uses GAppLaunchContext::setenv() — does NOT pollute the parent process.
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
