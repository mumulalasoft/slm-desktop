#pragma once

#include <QObject>
#include <QVariantMap>

class DockModel;
class LaunchEnvResolver;
class ShortcutModel;
class UIPreferences;

class AppExecutionGate : public QObject {
    Q_OBJECT
public:
    explicit AppExecutionGate(DockModel *dockModel, ShortcutModel *shortcutModel,
                              UIPreferences *uiPreferences, QObject *parent = nullptr);

    Q_INVOKABLE bool launchDesktopEntry(const QString &desktopFilePath, const QString &executable,
                                        const QString &displayName, const QString &iconName = QString(),
                                        const QString &iconSource = QString(),
                                        const QString &source = QStringLiteral("unknown"));
    Q_INVOKABLE bool launchDesktopId(const QString &desktopId,
                                     const QString &source = QStringLiteral("unknown"));
    Q_INVOKABLE bool launchShortcut(int shortcutIndex, const QVariantMap &entry,
                                    const QString &source = QStringLiteral("shell"));
    Q_INVOKABLE bool launchEntryMap(const QVariantMap &entry,
                                    const QString &source = QStringLiteral("generic"));
    Q_INVOKABLE void noteLaunch(const QVariantMap &entry,
                                const QString &source = QStringLiteral("external"));
    Q_INVOKABLE bool launchCommand(const QString &command,
                                   const QString &workingDirectory = QString(),
                                   const QString &source = QStringLiteral("terminal"));
    Q_INVOKABLE bool openTarget(const QString &targetPathOrUrl,
                                const QString &source = QStringLiteral("filemanager"));
    Q_INVOKABLE bool launchFromFileManager(const QString &targetPathOrUrl);
    Q_INVOKABLE bool launchFromTerminal(const QString &command,
                                        const QString &workingDirectory = QString());

    // Attach an env resolver so all subsequent launches inject the effective
    // environment from slm-envd.  Pass nullptr to disable.
    void setLaunchEnvResolver(LaunchEnvResolver *resolver);

signals:
    void appExecutionRecorded(QString source, QString name, QString desktopFile, QString executable, bool success);

private:
    bool verboseLoggingEnabled() const;

    DockModel          *m_dockModel       = nullptr;
    ShortcutModel      *m_shortcutModel   = nullptr;
    UIPreferences      *m_uiPreferences   = nullptr;
    LaunchEnvResolver  *m_envResolver     = nullptr;
};
