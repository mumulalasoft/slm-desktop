#include "daemonservicebootstraprunner.h"

namespace Slm::Bootstrap {

DaemonServiceBootstrapRunner::DaemonServiceBootstrapRunner(const QString &serviceName,
                                                           const QString &localBinary,
                                                           const QString &fallbackBinary,
                                                           const QString &logName,
                                                           ServiceCheckFn serviceCheck,
                                                           StartFn startFn,
                                                           QObject *parent,
                                                           int intervalMs)
    : QObject(parent)
    , m_serviceName(serviceName)
    , m_localBinary(localBinary)
    , m_fallbackBinary(fallbackBinary)
    , m_logName(logName)
    , m_serviceCheck(std::move(serviceCheck))
    , m_startFn(std::move(startFn))
{
    m_timer.setParent(this);
    m_timer.setInterval(intervalMs > 0 ? intervalMs : DaemonServiceBootstrap::kRetryIntervalMs);
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &DaemonServiceBootstrapRunner::onTick);
}

void DaemonServiceBootstrapRunner::start()
{
    if (isRunning()) {
        return;
    }
    onTick();
    if (!isRunning() && m_attempt > 0) {
        return;
    }
    if (m_attempt > 0
        && !DaemonServiceBootstrap::shouldStopRetry(false, m_attempt)
        && !m_timer.isActive()) {
        m_timer.start();
    }
}

void DaemonServiceBootstrapRunner::stop()
{
    m_timer.stop();
}

bool DaemonServiceBootstrapRunner::isRunning() const
{
    return m_timer.isActive();
}

int DaemonServiceBootstrapRunner::attemptCount() const
{
    return m_attempt;
}

int DaemonServiceBootstrapRunner::spawnAttemptCount() const
{
    return m_spawnAttempts;
}

int DaemonServiceBootstrapRunner::spawnStartedCount() const
{
    return m_spawnStarted;
}

int DaemonServiceBootstrapRunner::lastSpawnAttempt() const
{
    return m_lastSpawnAttempt;
}

int DaemonServiceBootstrapRunner::readyAttempt() const
{
    return m_readyAttempt;
}

int DaemonServiceBootstrapRunner::gaveUpAttempt() const
{
    return m_gaveUpAttempt;
}

bool DaemonServiceBootstrapRunner::hasGivenUp() const
{
    return m_gaveUpAttempt > 0;
}

bool DaemonServiceBootstrapRunner::isReady() const
{
    return m_readyAttempt > 0;
}

void DaemonServiceBootstrapRunner::onTick()
{
    ++m_attempt;
    const bool ready = m_serviceCheck ? m_serviceCheck(m_serviceName) : false;
    if (ready) {
        if (m_readyAttempt <= 0) {
            m_readyAttempt = m_attempt;
        }
        emit serviceReady(m_serviceName, m_attempt);
        stop();
        return;
    }

    if (DaemonServiceBootstrap::shouldSpawnOnAttempt(m_attempt)) {
        ++m_spawnAttempts;
        m_lastSpawnAttempt = m_attempt;
        const bool started = m_startFn ? m_startFn(m_localBinary, m_fallbackBinary) : false;
        if (started) {
            ++m_spawnStarted;
        }
        emit spawnAttempted(m_serviceName, m_attempt, started);
    }

    if (DaemonServiceBootstrap::shouldStopRetry(false, m_attempt)) {
        if (m_gaveUpAttempt <= 0) {
            m_gaveUpAttempt = m_attempt;
        }
        emit gaveUp(m_serviceName, m_attempt);
        stop();
        return;
    }

    if (!m_timer.isActive()) {
        m_timer.start();
    }
}

} // namespace Slm::Bootstrap
