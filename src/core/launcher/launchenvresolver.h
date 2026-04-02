#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

class EnvServiceClient;

// LaunchEnvResolver — fetches and caches the effective launch environment.
//
// Each app-id result is cached for kTtlMs milliseconds to avoid a D-Bus
// round-trip for rapid successive launches of the same app (e.g. dock bounce).
//
// Falls back to QProcessEnvironment::systemEnvironment() when the service is
// not available, so launches always succeed even without slm-envd.
//
class LaunchEnvResolver : public QObject
{
    Q_OBJECT

public:
    static constexpr int kTtlMs = 500;

    explicit LaunchEnvResolver(EnvServiceClient *client, QObject *parent = nullptr);

    // Returns the resolved environment for the given app ID.
    // Pass an empty string to get the session-wide merged view (no per-app layer).
    // Result is cached for kTtlMs ms.
    QProcessEnvironment resolve(const QString &appId);

    // Synchronously invalidate the cache entry for a specific appId.
    void invalidate(const QString &appId);
    void invalidateAll();

private:
    struct CacheEntry {
        QProcessEnvironment env;
        QDateTime           fetchedAt;
    };

    QProcessEnvironment fetchFromService(const QString &appId);

    EnvServiceClient *m_client;
    QHash<QString, CacheEntry> m_cache;
};
