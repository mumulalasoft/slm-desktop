#include "helperservice.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// static
QString HelperService::filePath()
{
    return QStringLiteral("/etc/slm/environment.d/system.json");
}

HelperService::HelperService(QObject *parent)
    : QObject(parent)
{
    // Pre-load on start so subsequent reads are cheap.
    load();
}

bool HelperService::load()
{
    m_lastError.clear();
    const QString path = filePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_entries.clear();
        return true; // file not present → empty store
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
    m_entries = loaded;
    return true;
}

bool HelperService::save()
{
    m_lastError.clear();
    const QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonArray arr;
    for (const EnvEntry &e : m_entries)
        arr.append(e.toJson());

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("scope")]   = QStringLiteral("system");
    root[QStringLiteral("entries")] = arr;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_lastError = QStringLiteral("Cannot write to ") + path;
        return false;
    }

    // 644: world-readable so slm-envd can read without elevated privileges.
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                        QFile::ReadGroup | QFile::ReadOther);
    file.write(QJsonDocument(root).toJson());
    return true;
}

bool HelperService::writeEntry(const QString &key, const QString &value,
                                const QString &comment, const QString &mergeMode,
                                bool enabled)
{
    // Reload from disk before mutating so concurrent sessions don't clobber
    // each other's changes.
    if (!load())
        return false;

    const QDateTime now = QDateTime::currentDateTimeUtc();

    for (EnvEntry &e : m_entries) {
        if (e.key == key) {
            e.value      = value;
            e.comment    = comment;
            e.mergeMode  = mergeMode.isEmpty() ? QStringLiteral("replace") : mergeMode;
            e.enabled    = enabled;
            e.modifiedAt = now;
            return save();
        }
    }

    // New entry
    EnvEntry e;
    e.key       = key;
    e.value     = value;
    e.comment   = comment;
    e.mergeMode = mergeMode.isEmpty() ? QStringLiteral("replace") : mergeMode;
    e.enabled   = enabled;
    e.scope     = QStringLiteral("system");
    e.createdAt  = now;
    e.modifiedAt = now;
    m_entries.append(e);
    return save();
}

bool HelperService::deleteEntry(const QString &key)
{
    if (!load())
        return false;

    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].key == key) {
            m_entries.removeAt(i);
            return save();
        }
    }
    // Key not found — treat as success (idempotent delete).
    return true;
}

QList<EnvEntry> HelperService::entries() const
{
    return m_entries;
}

QString HelperService::lastError() const
{
    return m_lastError;
}
