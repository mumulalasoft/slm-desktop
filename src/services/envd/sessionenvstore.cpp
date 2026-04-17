#include "sessionenvstore.h"

#include <QDateTime>

SessionEnvStore::SessionEnvStore(QObject *parent)
    : QObject(parent)
{
}

QList<EnvEntry> SessionEnvStore::entries() const
{
    return m_entries;
}

bool SessionEnvStore::setVar(const QString &key, const QString &value,
                              const QString &mergeMode)
{
    if (key.isEmpty())
        return false;

    for (EnvEntry &e : m_entries) {
        if (e.key == key) {
            e.value     = value;
            e.mergeMode = mergeMode;
            e.modifiedAt = QDateTime::currentDateTimeUtc();
            emit entriesChanged();
            return true;
        }
    }

    EnvEntry e;
    e.key       = key;
    e.value     = value;
    e.mergeMode = mergeMode;
    e.scope     = QStringLiteral("session");
    e.enabled   = true;
    e.createdAt = QDateTime::currentDateTimeUtc();
    e.modifiedAt = e.createdAt;
    m_entries.append(e);
    emit entriesChanged();
    return true;
}

bool SessionEnvStore::unsetVar(const QString &key)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].key == key) {
            m_entries.removeAt(i);
            emit entriesChanged();
            return true;
        }
    }
    return false;
}

bool SessionEnvStore::setEnabled(const QString &key, bool enabled)
{
    for (EnvEntry &e : m_entries) {
        if (e.key == key) {
            if (e.enabled != enabled) {
                e.enabled    = enabled;
                e.modifiedAt = QDateTime::currentDateTimeUtc();
                emit entriesChanged();
            }
            return true;
        }
    }
    return false;
}

void SessionEnvStore::clear()
{
    if (!m_entries.isEmpty()) {
        m_entries.clear();
        emit entriesChanged();
    }
}
