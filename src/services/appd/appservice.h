#pragma once

#include "appentry.h"
#include "appregistry.h"
#include "classificationengine.h"
#include "lifecycleengine.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace Slm::Appd {

// AppService — D-Bus service for org.desktop.Apps.
//
// Exposes the full AppEntry registry to consumers (Dock, Launchpad,
// App Switcher, Global Search, Notification Routing, System Monitor).
class AppService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.desktop.Apps")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)

public:
    explicit AppService(QObject *parent = nullptr);
    ~AppService() override;

    bool start(QString *error = nullptr);
    bool serviceRegistered() const;

    // ── D-Bus Methods ────────────────────────────────────────────────────────

public slots:
    QVariantList ListRunningApps() const;
    QVariantMap  GetApp(const QString &appId) const;
    QVariantList GetAppWindows(const QString &appId) const;
    QString      GetAppState(const QString &appId) const;

    void LaunchApp(const QString &appId);
    void ActivateApp(const QString &appId);
    void MinimizeApp(const QString &appId);
    void CloseApp(const QString &appId);
    void RestartApp(const QString &appId);

    // Test hook: inject a launch event (daemon-internal bridge).
    void NoteLaunch(const QString &appId,
                    const QString &desktopFile,
                    const QString &executable,
                    qint64 pid);

    // Compositor bridge: notify window created/closed/focused.
    void NoteWindowCreated(const QString &appId, qint64 pid,
                           const QString &windowId, const QString &title,
                           int workspace);
    void NoteWindowClosed(const QString &windowId);
    void NoteFocusChanged(const QString &windowId, bool focused);

    // Process monitor bridge: notify exit.
    void NoteProcessExited(qint64 pid, int exitCode);

signals:
    void serviceRegisteredChanged();

    // ── D-Bus Signals ────────────────────────────────────────────────────────
    void AppAdded(const QString &appId);
    void AppRemoved(const QString &appId);
    void AppStateChanged(const QString &appId, const QString &state);
    void AppWindowsChanged(const QString &appId);
    void AppFocusChanged(const QString &appId, bool focused);
    void AppUpdated(const QString &appId);

private:
    void registerDbusService();
    void connectRegistry();

    AppRegistry          m_registry;
    ClassificationEngine m_classifier;
    LifecycleEngine      m_lifecycle;
    QTimer               m_pollTimer;
    bool                 m_serviceRegistered = false;
};

} // namespace Slm::Appd
