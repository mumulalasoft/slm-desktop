#include "filemanagerapi.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QUrl>

#pragma push_macro("signals")
#undef signals
#include <gio/gio.h>
#pragma pop_macro("signals")

namespace {

static QString iconNameFromGIconListing(GIcon *icon)
{
    if (!icon) {
        return QString();
    }
    if (G_IS_THEMED_ICON(icon)) {
        const gchar *const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        if (names && names[0]) {
            return QString::fromUtf8(names[0]);
        }
    }
    if (G_IS_FILE_ICON(icon)) {
        GFile *file = g_file_icon_get_file(G_FILE_ICON(icon));
        if (file) {
            char *path = g_file_get_path(file);
            QString out = path ? QString::fromUtf8(path) : QString();
            g_free(path);
            return out;
        }
    }
    return QString();
}

static QString gfileToPathOrUriListing(GFile *file)
{
    if (!file) {
        return QString();
    }
    char *path = g_file_get_path(file);
    if (path) {
        const QString out = QString::fromUtf8(path);
        g_free(path);
        return out;
    }
    char *uri = g_file_get_uri(file);
    const QString out = uri ? QString::fromUtf8(uri) : QString();
    g_free(uri);
    return out;
}

} // namespace

QVariantMap FileManagerApi::listDirectory(const QString &path,
                                          bool includeHidden,
                                          bool directoriesFirst) const
{
    const QString p = expandPath(path);
    QDir dir(p);
    if (!dir.exists()) {
        return makeResult(false, QStringLiteral("not-found"));
    }

    QDir::Filters filters = QDir::NoDotAndDotDot | QDir::AllEntries;
    if (includeHidden) {
        filters |= QDir::Hidden;
    }
    QFileInfoList list = dir.entryInfoList(filters, QDir::NoSort);
    std::sort(list.begin(), list.end(), [directoriesFirst](const QFileInfo &a, const QFileInfo &b) {
        const QDateTime ma = a.lastModified();
        const QDateTime mb = b.lastModified();
        if (ma.isValid() && mb.isValid() && ma != mb) {
            return ma > mb; // newest first
        }
        if (ma.isValid() != mb.isValid()) {
            return ma.isValid();
        }
        if (directoriesFirst && a.isDir() != b.isDir()) {
            return a.isDir();
        }
        return QString::compare(a.fileName(), b.fileName(), Qt::CaseInsensitive) < 0;
    });
    QVariantList entries;
    entries.reserve(list.size());
    for (const QFileInfo &fi : list) {
        entries.push_back(fileInfoMap(fi));
    }

    return makeResult(true, QString(), {{QStringLiteral("path"), p}, {QStringLiteral("entries"), entries}});
}

QVariantMap FileManagerApi::searchDirectory(const QString &rootPath,
                                            const QString &query,
                                            bool includeHidden,
                                            bool directoriesFirst,
                                            int maxResults,
                                            qulonglong searchSession) const
{
    const QString basePath = expandPath(rootPath);
    const QString needle = query.trimmed();
    const int cappedMax = qBound(1, maxResults, 5000);
    const qulonglong session = (searchSession > 0)
            ? searchSession
            : m_activeSearchSession.load(std::memory_order_relaxed);
    if (needle.isEmpty()) {
        return makeResult(true, QString(),
                          {{QStringLiteral("path"), basePath},
                           {QStringLiteral("query"), needle},
                           {QStringLiteral("entries"), QVariantList()}});
    }

    GFile *root = g_file_new_for_path(basePath.toUtf8().constData());
    GFileType rootType = g_file_query_file_type(root, G_FILE_QUERY_INFO_NONE, nullptr);
    if (rootType != G_FILE_TYPE_DIRECTORY) {
        g_object_unref(root);
        return makeResult(false, QStringLiteral("not-a-directory"));
    }

    QVariantList entries;
    QList<GFile *> pendingDirs;
    pendingDirs.push_back(root);
    auto clearPendingDirs = [&pendingDirs]() {
        while (!pendingDirs.isEmpty()) {
            GFile *f = pendingDirs.takeFirst();
            if (f) {
                g_object_unref(f);
            }
        }
    };

    const QString attrs = QStringLiteral(
                G_FILE_ATTRIBUTE_STANDARD_NAME ","
                G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
                G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","
                G_FILE_ATTRIBUTE_STANDARD_ICON ","
                G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
                G_FILE_ATTRIBUTE_TIME_MODIFIED);

    while (!pendingDirs.isEmpty() && entries.size() < cappedMax) {
        if (searchSession > 0 && m_activeSearchSession.load(std::memory_order_relaxed) != session) {
            clearPendingDirs();
            return makeResult(true, QString(),
                              {{QStringLiteral("path"), basePath},
                               {QStringLiteral("query"), needle},
                               {QStringLiteral("entries"), QVariantList()},
                               {QStringLiteral("cancelled"), true}});
        }
        GFile *dir = pendingDirs.takeFirst();
        GError *gerr = nullptr;
        GFileEnumerator *enumerator = g_file_enumerate_children(dir,
                                                                attrs.toUtf8().constData(),
                                                                G_FILE_QUERY_INFO_NONE,
                                                                nullptr,
                                                                &gerr);
        if (enumerator == nullptr) {
            if (gerr) {
                g_error_free(gerr);
            }
            g_object_unref(dir);
            continue;
        }

        while (entries.size() < cappedMax) {
            if (searchSession > 0 && m_activeSearchSession.load(std::memory_order_relaxed) != session) {
                g_object_unref(enumerator);
                g_object_unref(dir);
                clearPendingDirs();
                return makeResult(true, QString(),
                                  {{QStringLiteral("path"), basePath},
                                   {QStringLiteral("query"), needle},
                                   {QStringLiteral("entries"), QVariantList()},
                                   {QStringLiteral("cancelled"), true}});
            }
            GFileInfo *info = g_file_enumerator_next_file(enumerator, nullptr, &gerr);
            if (info == nullptr) {
                if (gerr) {
                    g_error_free(gerr);
                    gerr = nullptr;
                }
                break;
            }

            const bool hidden = g_file_info_get_is_hidden(info);
            if (!includeHidden && hidden) {
                g_object_unref(info);
                continue;
            }

            const GFileType type = g_file_info_get_file_type(info);
            const bool isDir = (type == G_FILE_TYPE_DIRECTORY);
            const bool isSymlink = g_file_info_get_is_symlink(info);
            const QString name = QString::fromUtf8(g_file_info_get_name(info));
            const QString displayName = QString::fromUtf8(g_file_info_get_display_name(info));
            GFile *child = g_file_get_child(dir, g_file_info_get_name(info));

            if (displayName.contains(needle, Qt::CaseInsensitive) || name.contains(needle, Qt::CaseInsensitive)) {
                QVariantMap row;
                const QString childPath = gfileToPathOrUriListing(child);
                const QString contentType = QString::fromUtf8(g_file_info_get_content_type(info));
                const bool localPath = childPath.startsWith(QLatin1Char('/'));
                const QMimeType mime = (!isDir && localPath)
                        ? QMimeDatabase().mimeTypeForFile(childPath, QMimeDatabase::MatchDefault)
                        : QMimeType();
                const QString mimeName = isDir
                        ? QStringLiteral("inode/directory")
                        : (!contentType.isEmpty() ? contentType : mime.name());
                QString iconName = iconNameFromGIconListing(g_file_info_get_icon(info));
                if (isDir) {
                    iconName = QStringLiteral("folder");
                } else if (iconName.isEmpty() || iconName == QStringLiteral("text-x-generic")) {
                    const QString mimeIcon = !mime.iconName().isEmpty()
                            ? mime.iconName()
                            : mime.genericIconName();
                    if (!mimeIcon.isEmpty()) {
                        iconName = mimeIcon;
                    } else if (iconName.isEmpty()) {
                        iconName = QStringLiteral("text-x-generic");
                    }
                }
                row.insert(QStringLiteral("ok"), true);
                row.insert(QStringLiteral("name"), displayName.isEmpty() ? name : displayName);
                row.insert(QStringLiteral("path"), childPath);
                row.insert(QStringLiteral("isDir"), isDir);
                row.insert(QStringLiteral("isFile"), !isDir);
                row.insert(QStringLiteral("isHidden"), hidden);
                row.insert(QStringLiteral("size"), static_cast<qlonglong>(g_file_info_get_size(info)));
                row.insert(QStringLiteral("suffix"), QFileInfo(displayName.isEmpty() ? name : displayName).suffix());
                row.insert(QStringLiteral("thumbnailPath"), QString());
                row.insert(QStringLiteral("mimeType"), mimeName);
                row.insert(QStringLiteral("iconName"), iconName);
                const quint64 modifiedUnix = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
                if (modifiedUnix > 0) {
                    row.insert(QStringLiteral("lastModified"),
                               QDateTime::fromSecsSinceEpoch(static_cast<qint64>(modifiedUnix)).toString(Qt::ISODateWithMs));
                } else {
                    row.insert(QStringLiteral("lastModified"), QString());
                }
                entries.push_back(row);
            }

            if (isDir && !isSymlink && entries.size() < cappedMax) {
                pendingDirs.push_back(child);
                child = nullptr;
            }

            if (child != nullptr) {
                g_object_unref(child);
            }
            g_object_unref(info);
        }

        g_object_unref(enumerator);
        g_object_unref(dir);
    }

    std::sort(entries.begin(), entries.end(), [directoriesFirst](const QVariant &a, const QVariant &b) {
        const QVariantMap ma = a.toMap();
        const QVariantMap mb = b.toMap();
        const qint64 tma = QDateTime::fromString(
                    ma.value(QStringLiteral("lastModified")).toString(),
                    Qt::ISODateWithMs).toMSecsSinceEpoch();
        const qint64 tmb = QDateTime::fromString(
                    mb.value(QStringLiteral("lastModified")).toString(),
                    Qt::ISODateWithMs).toMSecsSinceEpoch();
        if (tma != tmb) {
            return tma > tmb; // newest first
        }
        const bool da = ma.value(QStringLiteral("isDir")).toBool();
        const bool db = mb.value(QStringLiteral("isDir")).toBool();
        if (directoriesFirst && da != db) {
            return da && !db;
        }
        const QString na = ma.value(QStringLiteral("name")).toString().toLower();
        const QString nb = mb.value(QStringLiteral("name")).toString().toLower();
        return na < nb;
    });

    return makeResult(true, QString(),
                      {{QStringLiteral("path"), basePath},
                       {QStringLiteral("query"), needle},
                       {QStringLiteral("entries"), entries}});
}

qulonglong FileManagerApi::beginSearchSession()
{
    return m_activeSearchSession.fetch_add(1, std::memory_order_relaxed) + 1;
}
