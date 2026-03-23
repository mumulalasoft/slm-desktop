#pragma once

#include <QObject>
#include <QVariantMap>

class QDBusConnection;
class QTimer;

class DaemonHealthMonitor : public QObject
{
    Q_OBJECT

public:
    explicit DaemonHealthMonitor(QObject *parent = nullptr);
    ~DaemonHealthMonitor() override;

    QVariantMap snapshot() const;

private slots:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

private:
    struct Peer;
    void setupPeer(Peer &peer);
    void schedule(Peer &peer, int delayMs);
    void checkPeer(Peer &peer);
    static int nextDelayMs(int failures);

    QDBusConnection *m_bus = nullptr;
    Peer *m_fileOps = nullptr;
    Peer *m_devices = nullptr;
};
