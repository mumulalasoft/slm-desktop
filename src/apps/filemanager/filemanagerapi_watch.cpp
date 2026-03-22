#include "filemanagerapi.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {

static QString settingsPathWatch()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share/DesktopShell");
    }
    QDir dir(base);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return dir.filePath(QStringLiteral("filemanager.ini"));
}

} // namespace

bool FileManagerApi::watchDirectory(const QString &path)
{
    const QString p = QDir(expandPath(path)).absolutePath();
    if (!QFileInfo(p).isDir()) {
        return false;
    }
    if (m_watcher.directories().contains(p)) {
        return true;
    }
    return m_watcher.addPath(p);
}

bool FileManagerApi::watchFile(const QString &path)
{
    const QString p = QFileInfo(expandPath(path)).absoluteFilePath();
    if (!QFileInfo(p).isFile()) {
        return false;
    }
    if (m_watcher.files().contains(p)) {
        return true;
    }
    return m_watcher.addPath(p);
}

bool FileManagerApi::watchPath(const QString &path)
{
    const QString p = expandPath(path);
    QFileInfo fi(p);
    if (!fi.exists()) {
        return false;
    }
    return fi.isDir() ? watchDirectory(p) : watchFile(p);
}

bool FileManagerApi::unwatchDirectory(const QString &path)
{
    const QString p = QDir(expandPath(path)).absolutePath();
    if (!m_watcher.directories().contains(p)) {
        return true;
    }
    return m_watcher.removePath(p);
}

bool FileManagerApi::unwatchFile(const QString &path)
{
    const QString p = QFileInfo(expandPath(path)).absoluteFilePath();
    if (!m_watcher.files().contains(p)) {
        return true;
    }
    return m_watcher.removePath(p);
}

bool FileManagerApi::unwatchPath(const QString &path)
{
    const QString p = expandPath(path);
    QFileInfo fi(p);
    return fi.isDir() ? unwatchDirectory(p) : unwatchFile(p);
}

QVariantMap FileManagerApi::watchedPaths() const
{
    QVariantMap out;
    QVariantList dirs;
    for (const QString &d : m_watcher.directories()) {
        dirs.push_back(d);
    }
    QVariantList files;
    for (const QString &f : m_watcher.files()) {
        files.push_back(f);
    }
    out.insert(QStringLiteral("directories"), dirs);
    out.insert(QStringLiteral("files"), files);
    return out;
}

QVariantList FileManagerApi::watchedDirectories() const
{
    QVariantList out;
    for (const QString &d : m_watcher.directories()) {
        out.push_back(d);
    }
    return out;
}

QVariantList FileManagerApi::watchedFiles() const
{
    QVariantList out;
    for (const QString &f : m_watcher.files()) {
        out.push_back(f);
    }
    return out;
}

bool FileManagerApi::ensureDatabase(QString *error) const
{
    if (error) {
        error->clear();
    }
    return true;
}

QString FileManagerApi::databasePath() const
{
    return settingsPathWatch();
}
