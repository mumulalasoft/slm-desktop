#include "appruntimeregistry.h"

#include <QDateTime>

namespace {
constexpr qint64 kLaunchWarmupMs = 6000;
}

void AppRuntimeRegistry::noteUserLaunch(const QSet<QString> &tokens, qint64 pid)
{
    cleanup();

    QSet<QString> cleanTokens;
    for (const QString &token : tokens) {
        const QString normalized = token.trimmed().toLower();
        if (!normalized.isEmpty()) {
            cleanTokens.insert(normalized);
        }
    }
    if (cleanTokens.isEmpty()) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (LaunchRecord &record : m_records) {
        if (!intersects(record.tokens, cleanTokens)) {
            continue;
        }
        record.tokens.unite(cleanTokens);
        if (pid > 0) {
            record.pids.insert(pid);
        }
        record.expiresAtMs = now + kLaunchWarmupMs;
        return;
    }

    LaunchRecord record;
    record.tokens = cleanTokens;
    record.createdAtMs = now;
    record.expiresAtMs = now + kLaunchWarmupMs;
    if (pid > 0) {
        record.pids.insert(pid);
    }
    m_records.push_back(record);
}

bool AppRuntimeRegistry::hasActiveUserLaunch(const QSet<QString> &tokens) const
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QSet<QString> cleanTokens;
    for (const QString &token : tokens) {
        const QString normalized = token.trimmed().toLower();
        if (!normalized.isEmpty()) {
            cleanTokens.insert(normalized);
        }
    }
    if (cleanTokens.isEmpty()) {
        return false;
    }

    for (const LaunchRecord &record : m_records) {
        if (record.expiresAtMs < now) {
            continue;
        }
        if (record.observedRunning) {
            continue;
        }
        if (intersects(record.tokens, cleanTokens)) {
            return true;
        }
    }
    return false;
}

void AppRuntimeRegistry::refresh(const QSet<qint64> &runningPids)
{
    for (LaunchRecord &record : m_records) {
        if (record.pids.isEmpty()) {
            continue;
        }
        QSet<qint64> alivePids;
        for (qint64 pid : record.pids) {
            if (runningPids.contains(pid)) {
                alivePids.insert(pid);
            }
        }
        record.pids = alivePids;
    }
    cleanup();
}

void AppRuntimeRegistry::noteObservedRunning(const QSet<QString> &tokens)
{
    QSet<QString> cleanTokens;
    for (const QString &token : tokens) {
        const QString normalized = token.trimmed().toLower();
        if (!normalized.isEmpty()) {
            cleanTokens.insert(normalized);
        }
    }
    if (cleanTokens.isEmpty()) {
        return;
    }

    for (LaunchRecord &record : m_records) {
        if (!intersects(record.tokens, cleanTokens)) {
            continue;
        }
        record.observedRunning = true;
    }
}

void AppRuntimeRegistry::cleanup()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_records.begin(); it != m_records.end();) {
        QSet<qint64> alivePids;
        for (qint64 pid : it->pids) {
            if (pid > 0) {
                alivePids.insert(pid);
            }
        }
        it->pids = alivePids;

        if (it->tokens.isEmpty() || it->expiresAtMs < now) {
            it = m_records.erase(it);
        } else {
            ++it;
        }
    }
}

bool AppRuntimeRegistry::intersects(const QSet<QString> &a, const QSet<QString> &b)
{
    if (a.size() > b.size()) {
        return intersects(b, a);
    }
    for (const QString &item : a) {
        if (b.contains(item)) {
            return true;
        }
    }
    return false;
}
