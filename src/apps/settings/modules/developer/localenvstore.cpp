#include "localenvstore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

// ── EnvEntry JSON serialization ──────────────────────────────────────────────

QJsonObject EnvEntry::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("key")]        = key;
    obj[QStringLiteral("value")]      = value;
    obj[QStringLiteral("enabled")]    = enabled;
    obj[QStringLiteral("scope")]      = scope;
    obj[QStringLiteral("comment")]    = comment;
    obj[QStringLiteral("merge_mode")] = mergeMode;
    if (createdAt.isValid())
        obj[QStringLiteral("created_at")]  = createdAt.toString(Qt::ISODate);
    if (modifiedAt.isValid())
        obj[QStringLiteral("modified_at")] = modifiedAt.toString(Qt::ISODate);
    return obj;
}

EnvEntry EnvEntry::fromJson(const QJsonObject &obj)
{
    EnvEntry e;
    e.key       = obj.value(QStringLiteral("key")).toString().trimmed();
    e.value     = obj.value(QStringLiteral("value")).toString();
    e.enabled   = obj.value(QStringLiteral("enabled")).toBool(true);
    e.scope     = obj.value(QStringLiteral("scope")).toString(QStringLiteral("user"));
    e.comment   = obj.value(QStringLiteral("comment")).toString();
    e.mergeMode = obj.value(QStringLiteral("merge_mode")).toString(QStringLiteral("replace"));
    const QString cAt = obj.value(QStringLiteral("created_at")).toString();
    const QString mAt = obj.value(QStringLiteral("modified_at")).toString();
    if (!cAt.isEmpty())  e.createdAt  = QDateTime::fromString(cAt, Qt::ISODate);
    if (!mAt.isEmpty())  e.modifiedAt = QDateTime::fromString(mAt, Qt::ISODate);
    return e;
}

// ── LocalEnvStore ─────────────────────────────────────────────────────────────

LocalEnvStore::LocalEnvStore(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &LocalEnvStore::onFileChanged);
}

QString LocalEnvStore::filePath() const
{
    const QString cfg = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return cfg + QStringLiteral("/slm/environment.d/user.json");
}

void LocalEnvStore::ensureDir() const
{
    QDir().mkpath(QFileInfo(filePath()).absolutePath());
}

void LocalEnvStore::watchFile()
{
    const QString path = filePath();
    if (QFile::exists(path) && !m_watcher.files().contains(path))
        m_watcher.addPath(path);
}

bool LocalEnvStore::load()
{
    const QString path = filePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        // No file yet — valid initial state for a new install.
        m_entries.clear();
        watchFile();
        emit entriesChanged();
        return true;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
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
    watchFile();
    emit entriesChanged();
    return true;
}

bool LocalEnvStore::save()
{
    ensureDir();

    QJsonArray arr;
    for (const EnvEntry &e : m_entries)
        arr.append(e.toJson());

    QJsonObject root;
    root[QStringLiteral("version")]  = 1;
    root[QStringLiteral("scope")]    = QStringLiteral("user-persistent");
    root[QStringLiteral("entries")]  = arr;

    const QString path = filePath();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    m_saving = true;
    file.write(QJsonDocument(root).toJson());
    file.close();
    // Re-watch: some watchers drop the path after the file is replaced atomically.
    if (!m_watcher.files().contains(path))
        m_watcher.addPath(path);
    m_saving = false;
    return true;
}

QList<EnvEntry> LocalEnvStore::entries() const
{
    return m_entries;
}

bool LocalEnvStore::addEntry(const EnvEntry &entry)
{
    for (const EnvEntry &e : m_entries) {
        if (e.key == entry.key)
            return false;
    }
    EnvEntry e       = entry;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (!e.createdAt.isValid())  e.createdAt  = now;
    if (!e.modifiedAt.isValid()) e.modifiedAt = now;
    m_entries.append(e);
    save();
    emit entriesChanged();
    return true;
}

bool LocalEnvStore::updateEntry(const QString &key, const EnvEntry &updated)
{
    for (EnvEntry &e : m_entries) {
        if (e.key == key) {
            e.value      = updated.value;
            e.comment    = updated.comment;
            e.mergeMode  = updated.mergeMode;
            e.modifiedAt = QDateTime::currentDateTimeUtc();
            save();
            emit entriesChanged();
            return true;
        }
    }
    return false;
}

bool LocalEnvStore::removeEntry(const QString &key)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].key == key) {
            m_entries.removeAt(i);
            save();
            emit entriesChanged();
            return true;
        }
    }
    return false;
}

bool LocalEnvStore::setEnabled(const QString &key, bool enabled)
{
    for (EnvEntry &e : m_entries) {
        if (e.key == key) {
            if (e.enabled == enabled)
                return true;
            e.enabled    = enabled;
            e.modifiedAt = QDateTime::currentDateTimeUtc();
            save();
            emit entriesChanged();
            return true;
        }
    }
    return false;
}

void LocalEnvStore::onFileChanged(const QString &path)
{
    if (m_saving)
        return;
    Q_UNUSED(path)
    load();
}
