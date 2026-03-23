#pragma once

#include <QObject>
#include <QVariantMap>

class WindowingBackendManager;

class SessionStateManager : public QObject
{
    Q_OBJECT

public:
    explicit SessionStateManager(WindowingBackendManager *windowingBackend,
                                 QObject *compositorStateModel,
                                 QObject *parent = nullptr);

    Q_INVOKABLE quint32 Inhibit(const QString &reason, uint flags);
    Q_INVOKABLE void Logout();
    Q_INVOKABLE void Shutdown();
    Q_INVOKABLE void Reboot();
    Q_INVOKABLE void Lock();
    Q_INVOKABLE void Unlock();

signals:
    void SessionLocked();
    void SessionUnlocked();
    void IdleChanged(bool idle);
    void ActiveAppChanged(const QString &app_id);

private:
    void updateFromEvent(const QString &event, const QVariantMap &payload);
    bool callLogin1(const QString &method, const QVariantList &args = {});
    bool callScreenSaver(const QString &method, const QVariantList &args = {});
    void emitIdle(bool idle);
    void emitActiveApp(const QString &appId);

private slots:
    void onScreenSaverActiveChanged(bool active);

private:
    WindowingBackendManager *m_windowingBackend = nullptr;
    QObject *m_compositorStateModel = nullptr;
    quint32 m_inhibitSeq = 0;
    bool m_idle = false;
    QString m_activeAppId;
};
