#include "envstore.h"

#include "../../apps/settings/modules/developer/enventry.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

EnvStore::EnvStore(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &EnvStore::onFileChanged);
}

// static
QString EnvStore::filePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/slm/environment.d/user.json");
}

void EnvStore::watchFile()
{
    const QString path = filePath();
    if (QFile::exists(path) && !m_watcher.files().contains(path))
        m_watcher.addPath(path);
}

bool EnvStore::load()
{
    m_lastError.clear();
    const QString path = filePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_entries.clear();
        watchFile();
        emit entriesChanged();
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
    m_entries = loaded;
    watchFile();
    emit entriesChanged();
    return true;
}

bool EnvStore::save()
{
    m_lastError.clear();

    QDir().mkpath(QFileInfo(filePath()).absolutePath());

    QJsonArray arr;
    for (const EnvEntry &e : m_entries)
        arr.append(e.toJson());

    QJsonObject root;
    root[QStringLiteral("version")]  = 1;
    root[QStringLiteral("scope")]    = QStringLiteral("user-persistent");
    root[QStringLiteral("entries")]  = arr;

    const QString path = filePath();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_lastError = QStringLiteral("Cannot write to ") + path;
        return false;
    }

    m_saving = true;
    file.write(QJsonDocument(root).toJson());
    file.close();
    if (!m_watcher.files().contains(path))
        m_watcher.addPath(path);
    m_saving = false;
    return true;
}

QList<EnvEntry> EnvStore::entries() const
{
    return m_entries;
}

bool EnvStore::addEntry(const EnvEntry &entry)
{
    for (const EnvEntry &e : m_entries) {
        if (e.key == entry.key) {
            m_lastError = QStringLiteral("Key already exists: ") + entry.key;
            return false;
        }
    }
    EnvEntry e = entry;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (!e.createdAt.isValid())  e.createdAt  = now;
    if (!e.modifiedAt.isValid()) e.modifiedAt = now;
    m_entries.append(e);
    save();
    emit entriesChanged();
    return true;
}

bool EnvStore::updateEntry(const QString &key, const EnvEntry &updated)
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
    m_lastError = QStringLiteral("Key not found: ") + key;
    return false;
}

bool EnvStore::removeEntry(const QString &key)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].key == key) {
            m_entries.removeAt(i);
            save();
            emit entriesChanged();
            return true;
        }
    }
    m_lastError = QStringLiteral("Key not found: ") + key;
    return false;
}

bool EnvStore::setEnabled(const QString &key, bool enabled)
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
    m_lastError = QStringLiteral("Key not found: ") + key;
    return false;
}

QString EnvStore::lastError() const
{
    return m_lastError;
}

void EnvStore::onFileChanged(const QString &path)
{
    if (m_saving)
        return;
    Q_UNUSED(path)
    load();
}
