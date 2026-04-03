#include "filemanagerapi.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStandardPaths>
#include <QVector>
#include <algorithm>

#pragma push_macro("signals")
#undef signals
#include <gio/gio.h>
#pragma pop_macro("signals")

namespace {

struct RecentEntryRow {
    QVariantMap row;
    qint64 lastOpenedMs = 0;
};

static QString freedesktopRecentPath()
{
    const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (dataHome.isEmpty()) {
        return QDir::homePath() + QStringLiteral("/.local/share/recently-used.xbel");
    }
    return QDir(dataHome).filePath(QStringLiteral("recently-used.xbel"));
}

static QVariantList readFreedesktopRecents(int limit)
{
    QVariantList out;
    const QString xbelPath = freedesktopRecentPath();
    if (xbelPath.isEmpty()) {
        return out;
    }

    GBookmarkFile *bookmark = g_bookmark_file_new();
    if (!bookmark) {
        return out;
    }

    GError *error = nullptr;
    const QByteArray nativePath = QFile::encodeName(xbelPath);
    const gboolean loaded = g_bookmark_file_load_from_file(bookmark, nativePath.constData(), &error);
    if (!loaded) {
        if (error) {
            g_error_free(error);
        }
        g_bookmark_file_free(bookmark);
        return out;
    }

    gsize uriCount = 0;
    gchar **uris = g_bookmark_file_get_uris(bookmark, &uriCount);
    if (!uris || uriCount == 0) {
        if (uris) {
            g_strfreev(uris);
        }
        g_bookmark_file_free(bookmark);
        return out;
    }

    QVector<RecentEntryRow> rows;
    rows.reserve(static_cast<int>(uriCount));
    QSet<QString> seen;

    for (gsize i = 0; i < uriCount; ++i) {
        const QString uri = QString::fromUtf8(uris[i] ? uris[i] : "");
        if (uri.isEmpty()) {
            continue;
        }
        GFile *file = g_file_new_for_uri(uris[i]);
        if (!file) {
            continue;
        }
        char *localPathRaw = g_file_get_path(file);
        const QString localPath = localPathRaw ? QFileInfo(QString::fromUtf8(localPathRaw)).absoluteFilePath() : QString();
        g_free(localPathRaw);
        g_object_unref(file);
        if (localPath.isEmpty()) {
            continue;
        }

        const QString key = localPath.toLower();
        if (seen.contains(key)) {
            continue;
        }
        seen.insert(key);

        QVariantMap row;
        row.insert(QStringLiteral("path"), localPath);
        row.insert(QStringLiteral("name"), QFileInfo(localPath).fileName());

        const QFileInfo fi(localPath);
        if (fi.isDir()) {
            row.insert(QStringLiteral("mimeType"), QStringLiteral("inode/directory"));
            row.insert(QStringLiteral("iconName"), QStringLiteral("folder"));
        } else {
            const QMimeType mime = QMimeDatabase().mimeTypeForFile(fi.absoluteFilePath(),
                                                                   QMimeDatabase::MatchDefault);
            const QString mimeName = mime.name();
            const QString iconName = !mime.iconName().isEmpty()
                    ? mime.iconName()
                    : (!mime.genericIconName().isEmpty()
                       ? mime.genericIconName()
                       : QStringLiteral("text-x-generic"));
            row.insert(QStringLiteral("mimeType"), mimeName);
            row.insert(QStringLiteral("iconName"), iconName);
        }

        GError *fieldErr = nullptr;
        GDateTime *modified = g_bookmark_file_get_modified_date_time(bookmark, uris[i], &fieldErr);
        if (fieldErr) {
            g_error_free(fieldErr);
            fieldErr = nullptr;
        }
        qint64 modifiedMs = 0;
        if (modified) {
            const gint64 sec = g_date_time_to_unix(modified);
            const gint usec = g_date_time_get_microsecond(modified);
            if (sec > 0) {
                modifiedMs = sec * 1000LL + (usec / 1000);
            }
        }
        if (modifiedMs > 0) {
            row.insert(QStringLiteral("lastOpened"),
                       QDateTime::fromMSecsSinceEpoch(modifiedMs).toUTC().toString(Qt::ISODateWithMs));
        } else {
            row.insert(QStringLiteral("lastOpened"), QString());
        }
        row.insert(QStringLiteral("openCount"), 1);

        RecentEntryRow rec;
        rec.row = row;
        rec.lastOpenedMs = modifiedMs;
        rows.push_back(rec);
    }

    std::sort(rows.begin(), rows.end(), [](const RecentEntryRow &a, const RecentEntryRow &b) {
        return a.lastOpenedMs > b.lastOpenedMs;
    });

    const int maxCount = (limit > 0) ? limit : rows.size();
    for (int i = 0; i < rows.size() && i < maxCount; ++i) {
        out.push_back(rows[i].row);
    }

    g_strfreev(uris);
    g_bookmark_file_free(bookmark);
    return out;
}

static bool clearFreedesktopRecents()
{
    const QString xbelPath = freedesktopRecentPath();
    if (xbelPath.isEmpty()) {
        return false;
    }
    QFile f(xbelPath);
    if (!f.exists()) {
        return true;
    }
    return f.remove();
}

} // namespace

QVariantMap FileManagerApi::clearRecentFiles()
{
    const bool clearedFreedesktop = clearFreedesktopRecents();
    if (!clearedFreedesktop) {
        return makeResult(false, QStringLiteral("failed-to-clear-freedesktop-recent"));
    }
    return makeResult(true);
}

QVariantList FileManagerApi::recentFiles(int limit) const
{
    return readFreedesktopRecents(limit > 0 ? limit : 5000);
}
