#include "launchenvresolver.h"
#include "../../apps/settings/modules/developer/envserviceclient.h"

#include <QDebug>

LaunchEnvResolver::LaunchEnvResolver(EnvServiceClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    // Invalidate all cached entries when the service signals a change.
    connect(m_client, &EnvServiceClient::userVarsChanged,    this, &LaunchEnvResolver::invalidateAll);
    connect(m_client, &EnvServiceClient::sessionVarsChanged, this, &LaunchEnvResolver::invalidateAll);
    connect(m_client, &EnvServiceClient::appVarsChanged,     this, &LaunchEnvResolver::invalidate);
}

QProcessEnvironment LaunchEnvResolver::resolve(const QString &appId)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();

    auto it = m_cache.find(appId);
    if (it != m_cache.end()) {
        if (it->fetchedAt.msecsTo(now) < kTtlMs)
            return it->env;
        m_cache.erase(it);
    }

    QProcessEnvironment env = fetchFromService(appId);
    m_cache.insert(appId, CacheEntry{env, now});
    return env;
}

void LaunchEnvResolver::invalidate(const QString &appId)
{
    m_cache.remove(appId);
}

void LaunchEnvResolver::invalidateAll()
{
    m_cache.clear();
}

QProcessEnvironment LaunchEnvResolver::fetchFromService(const QString &appId)
{
    if (!m_client->serviceAvailable()) {
        qWarning() << "LaunchEnvResolver: service unavailable, using system env for" << appId;
        return QProcessEnvironment::systemEnvironment();
    }

    const QStringList list = m_client->resolveEnvList(appId);
    if (list.isEmpty()) {
        return QProcessEnvironment::systemEnvironment();
    }

    // Start from the current system environment and overlay the resolved vars.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const QString &pair : list) {
        const int sep = pair.indexOf(QLatin1Char('='));
        if (sep > 0) {
            const QString key   = pair.left(sep);
            const QString value = pair.mid(sep + 1);
            env.insert(key, value);
        }
    }
    return env;
}
