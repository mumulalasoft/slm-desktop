#pragma once

#include <QSet>
#include <QString>
#include <QVector>

class AppRuntimeRegistry {
public:
    void noteUserLaunch(const QSet<QString> &tokens, qint64 pid = -1);
    bool hasActiveUserLaunch(const QSet<QString> &tokens) const;
    void refresh(const QSet<qint64> &runningPids);
    void noteObservedRunning(const QSet<QString> &tokens);

private:
    struct LaunchRecord {
        QSet<QString> tokens;
        QSet<qint64> pids;
        qint64 createdAtMs = 0;
        qint64 expiresAtMs = 0;
        bool observedRunning = false;
    };

    void cleanup();
    static bool intersects(const QSet<QString> &a, const QSet<QString> &b);

    QVector<LaunchRecord> m_records;
};
