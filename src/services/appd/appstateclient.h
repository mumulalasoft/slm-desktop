#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class QDBusInterface;
class QDBusServiceWatcher;

namespace Slm::Appd {

// AppStateClient — QML-facing bridge to org.desktop.Apps.
//
// Registered as context property "AppStateClient" in main.cpp.
// Provides:
//  - runningAppIds: set of currently running appIds (for dock indicators)
//  - launchingAppIds: set of apps in launching state
//  - focusedAppId: currently focused appId
//  - getAppState(appId): state string for a specific app
//  - getApp(appId): full AppEntry map
//  - isRunning(appId), isLaunching(appId), isFocused(appId): convenience
class AppStateClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY serviceAvailableChanged)
    Q_PROPERTY(QStringList runningAppIds READ runningAppIds NOTIFY runningAppsChanged)
    Q_PROPERTY(QStringList launchingAppIds READ launchingAppIds NOTIFY runningAppsChanged)
    Q_PROPERTY(QString focusedAppId READ focusedAppId NOTIFY focusedAppIdChanged)
    // Opaque revision counter — QML bindings react to any change without
    // needing to deep-compare the full variant list.
    Q_PROPERTY(int _rev READ rev NOTIFY runningAppsChanged)

public:
    explicit AppStateClient(QObject *parent = nullptr);
    ~AppStateClient() override;

    bool serviceAvailable() const;
    QStringList runningAppIds() const;
    QStringList launchingAppIds() const;
    QString focusedAppId() const;
    int rev() const;

    Q_INVOKABLE bool isRunning(const QString &appId) const;
    Q_INVOKABLE bool isLaunching(const QString &appId) const;
    Q_INVOKABLE bool isFocused(const QString &appId) const;
    Q_INVOKABLE QString getAppState(const QString &appId) const;
    Q_INVOKABLE QVariantMap getApp(const QString &appId) const;
    Q_INVOKABLE QVariantList listRunningApps() const;

    // Match by partial identity (desktopFile, executable, name).
    Q_INVOKABLE bool matchesRunning(const QString &desktopFile,
                                    const QString &executable,
                                    const QString &name) const;

signals:
    void serviceAvailableChanged();
    void runningAppsChanged();
    void focusedAppIdChanged();

private slots:
    void onAppAdded(const QString &appId);
    void onAppRemoved(const QString &appId);
    void onAppStateChanged(const QString &appId, const QString &state);
    void onAppFocusChanged(const QString &appId, bool focused);
    void onNameOwnerChanged(const QString &name,
                             const QString &oldOwner,
                             const QString &newOwner);

private:
    void bindSignals();
    void setServiceAvailable(bool available);
    void refresh();

    // Local lightweight state cache (avoids round-trip per query).
    QHash<QString, QString> m_appStates;  // appId → state string
    QSet<QString>           m_focusedIds;
    QString                 m_focusedAppId;
    int                     m_rev = 0;
    bool                    m_serviceAvailable = false;

    QDBusInterface      *m_iface   = nullptr;
    QDBusServiceWatcher *m_watcher = nullptr;
};

} // namespace Slm::Appd
