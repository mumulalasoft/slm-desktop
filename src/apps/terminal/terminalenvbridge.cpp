#include "terminalenvbridge.h"
#include "../../apps/settings/modules/developer/envserviceclient.h"

#include <QDebug>
#include <QTimer>

TerminalEnvBridge::TerminalEnvBridge(EnvServiceClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_cachedEnv(QProcessEnvironment::systemEnvironment())
{
}

void TerminalEnvBridge::fetchBaseline(EnvCallback callback)
{
    if (m_client->serviceAvailable()) {
        const QStringList list = m_client->resolveEnvList();
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (const QString &pair : list) {
            const int sep = pair.indexOf(QLatin1Char('='));
            if (sep > 0)
                env.insert(pair.left(sep), pair.mid(sep + 1));
        }
        m_cachedEnv = env;
        callback(env);
        emit baselineFetched(env);
        return;
    }

    // Service not yet running — queue retries.
    m_pending     = std::move(callback);
    m_retriesLeft = kMaxRetries;
    qWarning() << "TerminalEnvBridge: service unavailable, retrying" << kMaxRetries << "times";

    QTimer *t = new QTimer(this);
    t->setSingleShot(false);
    t->setInterval(kRetryMs);
    connect(t, &QTimer::timeout, this, &TerminalEnvBridge::onRetryTimer);
    t->start();
}

void TerminalEnvBridge::onRetryTimer()
{
    if (m_client->serviceAvailable()) {
        // Service appeared — fetch now.
        const QStringList list = m_client->resolveEnvList();
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (const QString &pair : list) {
            const int sep = pair.indexOf(QLatin1Char('='));
            if (sep > 0)
                env.insert(pair.left(sep), pair.mid(sep + 1));
        }
        m_cachedEnv = env;
        if (m_pending)
            m_pending(env);
        emit baselineFetched(env);
        m_pending = nullptr;

        // Stop and self-delete the timer.
        QTimer *t = qobject_cast<QTimer *>(sender());
        if (t) {
            t->stop();
            t->deleteLater();
        }
        return;
    }

    if (--m_retriesLeft <= 0) {
        qWarning() << "TerminalEnvBridge: giving up after" << kMaxRetries
                   << "retries; using system environment";
        if (m_pending)
            m_pending(m_cachedEnv);
        m_pending = nullptr;

        QTimer *t = qobject_cast<QTimer *>(sender());
        if (t) {
            t->stop();
            t->deleteLater();
        }
    }
}

QProcessEnvironment TerminalEnvBridge::cachedEnv() const
{
    return m_cachedEnv;
}
