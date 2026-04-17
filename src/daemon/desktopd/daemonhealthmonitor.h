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
    void appendTimelineEvent(const QString &peerKey,
                             const Peer *peer,
                             const QString &code,
                             const QString &severity,
                             const QString &message,
                             const QVariantMap &details = {});
    void loadTimeline();
    void saveTimeline() const;
    QString resolveTimelineFilePath() const;
    static int nextDelayMs(int failures);

    QDBusConnection *m_bus = nullptr;
    Peer *m_fileOps = nullptr;
    Peer *m_devices = nullptr;
    QVariantList m_timeline;
    int m_timelineLimit = 200;
    QString m_timelineFilePath;
};
