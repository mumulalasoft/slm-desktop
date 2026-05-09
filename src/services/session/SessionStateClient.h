#pragma once

#include <QObject>
#include <QString>

class QDBusInterface;

namespace Slm::Session {

class SessionStateClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY serviceAvailableChanged)
    Q_PROPERTY(bool locked READ locked NOTIFY lockedChanged)
    Q_PROPERTY(QString lockState READ lockState NOTIFY lockStateChanged)
    Q_PROPERTY(QString userName READ userName CONSTANT)
    Q_PROPERTY(QString lastUnlockError READ lastUnlockError NOTIFY lastUnlockResultChanged)
    Q_PROPERTY(int lastRetryAfterSec READ lastRetryAfterSec NOTIFY lastUnlockResultChanged)

public:
    explicit SessionStateClient(QObject *parent = nullptr);
    ~SessionStateClient() override;

    bool serviceAvailable() const;
    bool locked() const;
    QString lockState() const;
    QString userName() const;
    QString lastUnlockError() const;
    int lastRetryAfterSec() const;

    Q_INVOKABLE void lock();
    Q_INVOKABLE bool requestUnlock(const QString &password);

signals:
    void serviceAvailableChanged();
    void lockedChanged();
    void lockStateChanged();
    void lastUnlockResultChanged();
    // Forwarded from desktopd's Resumed DBus signal so QML lockscreen
    // surfaces can re-attach their LayerShell security overlay on wake.
    void resumed();

private slots:
    void onSessionLocked();
    void onSessionUnlocked();
    void onLockStateChanged(const QString &state);
    void onResumed();
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

private:
    void bindSignals();
    void refreshServiceAvailability();
    void setLocked(bool locked);
    void setLockState(const QString &state);

    QDBusInterface *m_iface = nullptr;
    bool m_serviceAvailable = false;
    bool m_locked = false;
    QString m_lockState = QStringLiteral("Active");
    QString m_userName;
    QString m_lastUnlockError;
    int m_lastRetryAfterSec = 0;
};

} // namespace Slm::Session
