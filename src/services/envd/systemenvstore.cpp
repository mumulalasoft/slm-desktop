#include "systemenvstore.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// static
QString SystemEnvStore::filePath()
{
    return QStringLiteral("/etc/slm/environment.d/system.json");
}

SystemEnvStore::SystemEnvStore(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &SystemEnvStore::onFileChanged);
}

void SystemEnvStore::watchFile()
{
    const QString path = filePath();
    if (QFile::exists(path) && !m_watcher.files().contains(path))
        m_watcher.addPath(path);
}

bool SystemEnvStore::load()
{
    m_lastError.clear();
    const QString path = filePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        // File not present is not an error — system scope simply has no entries.
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
        if (e.isValid()) {
            e.scope = QStringLiteral("system");
            loaded.append(e);
        }
    }
    m_entries = loaded;
    watchFile();
    emit entriesChanged();
    return true;
}

QList<EnvEntry> SystemEnvStore::entries() const
{
    return m_entries;
}

QString SystemEnvStore::lastError() const
{
    return m_lastError;
}

void SystemEnvStore::onFileChanged(const QString &path)
{
    Q_UNUSED(path)
    load();
}
