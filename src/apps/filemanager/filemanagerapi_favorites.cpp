#include "filemanagerapi.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>

namespace {

static QString settingsPath()
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

static QSettings *settingsInstance()
{
    static QSettings s(settingsPath(), QSettings::IniFormat);
    return &s;
}

static QString bookmarkUriFromPath(const QString &pathOrUri)
{
    const QString value = pathOrUri.trimmed();
    if (value.isEmpty()) {
        return QString();
    }
    const QUrl parsed(value);
    if (parsed.isValid() && !parsed.scheme().isEmpty()) {
        return parsed.toString(QUrl::FullyEncoded);
    }
    const QString local = QFileInfo(value).absoluteFilePath();
    return QUrl::fromLocalFile(local).toString(QUrl::FullyEncoded);
}

static QString bookmarkPathFromUri(const QString &uriOrPath)
{
    const QString value = uriOrPath.trimmed();
    if (value.isEmpty()) {
        return QString();
    }
    const QUrl url(value);
    if (url.isValid() && !url.scheme().isEmpty()) {
        if (url.isLocalFile()) {
            return QFileInfo(url.toLocalFile()).absoluteFilePath();
        }
        return value;
    }
    return QFileInfo(value).absoluteFilePath();
}

static QString bookmarkKeyForDedupe(const QString &pathOrUri)
{
    const QString p = bookmarkPathFromUri(pathOrUri);
    if (!p.isEmpty()) {
        return p.toLower();
    }
    return pathOrUri.trimmed().toLower();
}

static QStringList gtkBookmarkFiles()
{
    const QString home = QDir::homePath();
    return {
        home + QStringLiteral("/.config/gtk-3.0/bookmarks"),
        home + QStringLiteral("/.config/gtk-4.0/bookmarks")
    };
}

static QVariantList readGtkBookmarks()
{
    QVariantList out;
    QSet<QString> seen;
    for (const QString &filePath : gtkBookmarkFiles()) {
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        QTextStream ts(&f);
        while (!ts.atEnd()) {
            const QString line = ts.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
                continue;
            }
            const int sep = line.indexOf(QLatin1Char(' '));
            const QString uri = (sep >= 0 ? line.left(sep) : line).trimmed();
            const QString label = (sep >= 0 ? line.mid(sep + 1).trimmed() : QString());
            const QString path = bookmarkPathFromUri(uri);
            const QString key = bookmarkKeyForDedupe(path.isEmpty() ? uri : path);
            if (key.isEmpty() || seen.contains(key)) {
                continue;
            }
            seen.insert(key);
            QVariantMap row;
            row.insert(QStringLiteral("path"), path);
            row.insert(QStringLiteral("label"), label.isEmpty() ? QFileInfo(path).fileName() : label);
            row.insert(QStringLiteral("uri"), uri);
            out.push_back(row);
        }
    }
    return out;
}

static bool writeGtkBookmarks(const QVariantList &rows)
{
    QStringList lines;
    QSet<QString> seen;
    for (const QVariant &v : rows) {
        const QVariantMap m = v.toMap();
        const QString path = m.value(QStringLiteral("path")).toString().trimmed();
        QString uri = m.value(QStringLiteral("uri")).toString().trimmed();
        if (uri.isEmpty()) {
            uri = bookmarkUriFromPath(path);
        }
        if (uri.isEmpty()) {
            continue;
        }
        const QString key = bookmarkKeyForDedupe(path.isEmpty() ? uri : path);
        if (key.isEmpty() || seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        const QString label = m.value(QStringLiteral("label")).toString().trimmed();
        lines.push_back(label.isEmpty() ? uri : (uri + QStringLiteral(" ") + label));
    }

    const QString content = lines.join(QLatin1Char('\n'))
            + (lines.isEmpty() ? QString() : QStringLiteral("\n"));
    for (const QString &filePath : gtkBookmarkFiles()) {
        QFileInfo fi(filePath);
        QDir dir = fi.absoluteDir();
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            return false;
        }
        QSaveFile out(filePath);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        if (out.write(content.toUtf8()) < 0) {
            return false;
        }
        if (!out.commit()) {
            return false;
        }
    }
    return true;
}

static QVariantList mergeBookmarkRows(const QVariantList &primary, const QVariantList &secondary)
{
    QVariantList out;
    QSet<QString> seen;
    auto appendRows = [&](const QVariantList &rows) {
        for (const QVariant &v : rows) {
            QVariantMap m = v.toMap();
            const QString path = bookmarkPathFromUri(m.value(QStringLiteral("path")).toString());
            QString uri = m.value(QStringLiteral("uri")).toString().trimmed();
            if (uri.isEmpty()) {
                uri = bookmarkUriFromPath(path);
            }
            const QString key = bookmarkKeyForDedupe(path.isEmpty() ? uri : path);
            if (key.isEmpty() || seen.contains(key)) {
                continue;
            }
            seen.insert(key);
            m.insert(QStringLiteral("path"), path);
            m.insert(QStringLiteral("uri"), uri);
            if (m.value(QStringLiteral("label")).toString().trimmed().isEmpty()) {
                m.insert(QStringLiteral("label"), QFileInfo(path).fileName());
            }
            out.push_back(m);
        }
    };
    appendRows(primary);
    appendRows(secondary);
    return out;
}

} // namespace

QVariantMap FileManagerApi::setFavoriteFile(const QString &path, bool favorite, const QString &thumbnailPath)
{
    QSettings *s = settingsInstance();
    QVariantList favs = s->value(QStringLiteral("favorites")).toList();
    const QString target = expandPath(path);
    QVariantList next;
    bool found = false;
    for (const QVariant &v : favs) {
        QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("path")).toString() == target) {
            found = true;
            if (!favorite) {
                continue;
            }
            m.insert(QStringLiteral("thumbnailPath"), expandPath(thumbnailPath));
            next.push_back(m);
        } else {
            next.push_back(m);
        }
    }
    if (favorite && !found) {
        next.push_back(QVariantMap{{QStringLiteral("path"), target},
                                   {QStringLiteral("thumbnailPath"), expandPath(thumbnailPath)},
                                   {QStringLiteral("addedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}});
    }
    s->setValue(QStringLiteral("favorites"), next);
    s->sync();
    return makeResult(true);
}

QVariantList FileManagerApi::favoriteFiles(int limit) const
{
    QVariantList favs = settingsInstance()->value(QStringLiteral("favorites")).toList();
    if (limit > 0 && favs.size() > limit) {
        favs = favs.mid(0, limit);
    }
    return favs;
}

bool FileManagerApi::isFavoriteFile(const QString &path) const
{
    const QString target = expandPath(path);
    const QVariantList favs = settingsInstance()->value(QStringLiteral("favorites")).toList();
    for (const QVariant &v : favs) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("path")).toString() == target) {
            return true;
        }
    }
    return false;
}

QVariantMap FileManagerApi::addBookmark(const QString &path, const QString &label)
{
    QSettings *s = settingsInstance();
    const QString p = bookmarkPathFromUri(expandPath(path));
    if (p.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }
    QVariantList list = s->value(QStringLiteral("bookmarks")).toList();
    const QString key = bookmarkKeyForDedupe(p);
    QVariantList next;
    bool found = false;
    for (const QVariant &v : list) {
        QVariantMap row = v.toMap();
        if (bookmarkKeyForDedupe(row.value(QStringLiteral("path")).toString()) == key) {
            found = true;
            row.insert(QStringLiteral("path"), p);
            row.insert(QStringLiteral("uri"), bookmarkUriFromPath(p));
            if (!label.trimmed().isEmpty()) {
                row.insert(QStringLiteral("label"), label.trimmed());
            }
            next.push_back(row);
            continue;
        }
        next.push_back(row);
    }
    if (!found) {
        next.push_back(QVariantMap{{QStringLiteral("path"), p},
                                   {QStringLiteral("label"), label.isEmpty() ? QFileInfo(p).fileName() : label},
                                   {QStringLiteral("uri"), bookmarkUriFromPath(p)}});
    }
    const QVariantList merged = mergeBookmarkRows(next, readGtkBookmarks());
    s->setValue(QStringLiteral("bookmarks"), merged);
    s->sync();
    writeGtkBookmarks(merged);
    return makeResult(true);
}

QVariantMap FileManagerApi::removeBookmark(const QString &path)
{
    QSettings *s = settingsInstance();
    QVariantList list = s->value(QStringLiteral("bookmarks")).toList();
    const QString p = bookmarkPathFromUri(expandPath(path));
    const QString key = bookmarkKeyForDedupe(p);
    QVariantList next;
    for (const QVariant &v : list) {
        const QVariantMap m = v.toMap();
        if (bookmarkKeyForDedupe(m.value(QStringLiteral("path")).toString()) == key) {
            continue;
        }
        next.push_back(m);
    }
    const QVariantList gtkRows = readGtkBookmarks();
    QVariantList gtkFiltered;
    for (const QVariant &v : gtkRows) {
        const QVariantMap m = v.toMap();
        if (bookmarkKeyForDedupe(m.value(QStringLiteral("path")).toString()) == key) {
            continue;
        }
        gtkFiltered.push_back(m);
    }
    const QVariantList merged = mergeBookmarkRows(next, gtkFiltered);
    s->setValue(QStringLiteral("bookmarks"), merged);
    s->sync();
    writeGtkBookmarks(merged);
    return makeResult(true);
}

QVariantList FileManagerApi::bookmarks(int limit) const
{
    QVariantList list = mergeBookmarkRows(settingsInstance()->value(QStringLiteral("bookmarks")).toList(),
                                          readGtkBookmarks());
    if (limit > 0 && list.size() > limit) {
        list = list.mid(0, limit);
    }
    return list;
}
