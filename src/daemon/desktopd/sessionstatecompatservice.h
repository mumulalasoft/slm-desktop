#pragma once

#include <QObject>
#include <QVariantMap>

class SessionStateManager;

class SessionStateCompatService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.desktop_shell.SessionState1")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)

public:
    explicit SessionStateCompatService(SessionStateManager *manager, QObject *parent = nullptr);
    ~SessionStateCompatService() override;

    bool serviceRegistered() const;

public slots:
    uint Inhibit(const QString &reason, uint flags);
    void Logout();
    void Shutdown();
    void Reboot();
    void Lock();
    QVariantMap Unlock(const QString &password);

signals:
    void serviceRegisteredChanged();
    void SessionLocked();
    void SessionUnlocked();
    void IdleChanged(bool idle);
    void ActiveAppChanged(const QString &app_id);

private:
    void registerDbusService();

    SessionStateManager *m_manager = nullptr;
    bool m_serviceRegistered = false;
};
