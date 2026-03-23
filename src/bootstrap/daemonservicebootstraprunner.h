#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <functional>

#include "daemonservicebootstrap.h"

namespace Slm::Bootstrap {

class DaemonServiceBootstrapRunner : public QObject
{
    Q_OBJECT

public:
    using ServiceCheckFn = std::function<bool(const QString &)>;
    using StartFn = std::function<bool(const QString &, const QString &)>;

    explicit DaemonServiceBootstrapRunner(const QString &serviceName,
                                          const QString &localBinary,
                                          const QString &fallbackBinary,
                                          const QString &logName,
                                          ServiceCheckFn serviceCheck,
                                          StartFn startFn,
                                          QObject *parent = nullptr,
                                          int intervalMs = DaemonServiceBootstrap::kRetryIntervalMs);

    void start();
    void stop();
    bool isRunning() const;
    int attemptCount() const;
    int spawnAttemptCount() const;
    int spawnStartedCount() const;
    int lastSpawnAttempt() const;
    int readyAttempt() const;
    int gaveUpAttempt() const;
    bool hasGivenUp() const;
    bool isReady() const;

signals:
    void serviceReady(const QString &serviceName, int attempt);
    void gaveUp(const QString &serviceName, int attempt);
    void spawnAttempted(const QString &serviceName, int attempt, bool started);

private slots:
    void onTick();

private:
    QString m_serviceName;
    QString m_localBinary;
    QString m_fallbackBinary;
    QString m_logName;
    ServiceCheckFn m_serviceCheck;
    StartFn m_startFn;
    QTimer m_timer;
    int m_attempt = 0;
    int m_spawnAttempts = 0;
    int m_spawnStarted = 0;
    int m_lastSpawnAttempt = 0;
    int m_readyAttempt = 0;
    int m_gaveUpAttempt = 0;
};

} // namespace Slm::Bootstrap
