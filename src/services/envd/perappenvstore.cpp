#include "perappenvstore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

PerAppEnvStore::PerAppEnvStore(QObject *parent)
    : QObject(parent)
{
}

// static
QString PerAppEnvStore::filePath(const QString &appId)
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/slm/environment.d/apps/")
           + appId
           + QStringLiteral(".json");
}

bool PerAppEnvStore::load(const QString &appId)
{
    m_lastError.clear();
    if (appId.isEmpty()) {
        m_lastError = QStringLiteral("Empty app ID");
        return false;
    }

    const QString path = filePath(appId);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        // No file yet is valid — means no per-app overrides.
        m_store[appId] = {};
        emit appEntriesChanged(appId);
        return true;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        m_lastError = QStringLiteral("Invalid JSON in ") + path;
        return false;
    }

    const QJsonArray arr = doc.object().value(QStringLiteral("entries")).toArray();
    QList<EnvEntry> loaded;
    loaded.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        EnvEntry e = EnvEntry::fromJson(v.toObject());
        if (e.isValid())
            loaded.append(e);
    }
    m_store[appId] = loaded;
    emit appEntriesChanged(appId);
    return true;
}

bool PerAppEnvStore::save(const QString &appId)
{
    m_lastError.clear();
    const QString path = filePath(appId);
    QDir().mkpath(QFileInfo(path).absolutePath());

    const QList<EnvEntry> &list = m_store.value(appId);
    QJsonArray arr;
    for (const EnvEntry &e : list)
        arr.append(e.toJson());

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("app_id")] = appId;
    root[QStringLiteral("scope")]   = QStringLiteral("per-app");
    root[QStringLiteral("entries")] = arr;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_lastError = QStringLiteral("Cannot write to ") + path;
        return false;
    }
    file.write(QJsonDocument(root).toJson());
    return true;
}

QList<EnvEntry> PerAppEnvStore::entries(const QString &appId) const
{
    return m_store.value(appId);
}

bool PerAppEnvStore::addEntry(const QString &appId, const EnvEntry &entry)
{
    QList<EnvEntry> &list = m_store[appId];
    for (const EnvEntry &e : list) {
        if (e.key == entry.key) {
            m_lastError = QStringLiteral("Key already exists: ") + entry.key;
            return false;
        }
    }
    EnvEntry e = entry;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (!e.createdAt.isValid())  e.createdAt  = now;
    if (!e.modifiedAt.isValid()) e.modifiedAt = now;
    list.append(e);
    save(appId);
    emit appEntriesChanged(appId);
    return true;
}

bool PerAppEnvStore::updateEntry(const QString &appId, const QString &key,
                                  const EnvEntry &updated)
{
    QList<EnvEntry> &list = m_store[appId];
    for (EnvEntry &e : list) {
        if (e.key == key) {
            e.value      = updated.value;
            e.comment    = updated.comment;
            e.mergeMode  = updated.mergeMode;
            e.modifiedAt = QDateTime::currentDateTimeUtc();
            save(appId);
            emit appEntriesChanged(appId);
            return true;
        }
    }
    m_lastError = QStringLiteral("Key not found: ") + key;
    return false;
}

bool PerAppEnvStore::removeEntry(const QString &appId, const QString &key)
{
    QList<EnvEntry> &list = m_store[appId];
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].key == key) {
            list.removeAt(i);
            save(appId);
            emit appEntriesChanged(appId);
            return true;
        }
    }
    m_lastError = QStringLiteral("Key not found: ") + key;
    return false;
}

QString PerAppEnvStore::lastError() const
{
    return m_lastError;
}
