#include "filemanagermodel.h"

#include "filemanagerapi.h"
#include "metadataindexserver.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPointer>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <algorithm>
#include <thread>

namespace {

struct DirectoryScanResult
{
    bool ok = false;
    QString path;
    QString error;
    QVector<FileEntry> entries;
};

QString mimeIconForInfo(const QFileInfo &info);

QString fileKindSortKey(const FileEntry &e)
{
    if (e.dir) {
        return QStringLiteral("folder");
    }
    const QString mime = e.mimeType.trimmed().toLower();
    if (!mime.isEmpty()) {
        return mime;
    }
    const QString suffix = e.suffix.trimmed().toLower();
    return suffix.isEmpty() ? QStringLiteral("file") : suffix;
}

QDateTime parseIsoDateLoose(const QString &iso)
{
    const QString value = iso.trimmed();
    if (value.isEmpty()) {
        return {};
    }
    QDateTime dt = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(value, Qt::ISODate);
    }
    return dt;
}

void sortEntries(QVector<FileEntry> &entries,
                 const QString &sortKey,
                 bool sortDescending,
                 bool directoriesFirst)
{
    const QString key = sortKey.trimmed().isEmpty()
            ? QStringLiteral("dateModified")
            : sortKey.trimmed();
    std::sort(entries.begin(), entries.end(),
              [key, sortDescending, directoriesFirst](const FileEntry &a, const FileEntry &b) {
        if (directoriesFirst && a.dir != b.dir) {
            return a.dir;
        }

        auto compareOrder = [sortDescending](int cmp) {
            return sortDescending ? (cmp > 0) : (cmp < 0);
        };

        if (key == QLatin1String("name")) {
            const int cmp = QString::localeAwareCompare(a.name.toLower(), b.name.toLower());
            if (cmp != 0) {
                return compareOrder(-cmp);
            }
        } else if (key == QLatin1String("size")) {
            if (a.size != b.size) {
                return sortDescending ? (a.size > b.size) : (a.size < b.size);
            }
        } else if (key == QLatin1String("kind")) {
            const int cmp = QString::localeAwareCompare(fileKindSortKey(a), fileKindSortKey(b));
            if (cmp != 0) {
                return compareOrder(-cmp);
            }
        } else if (key == QLatin1String("dateAdded")) {
            const QDateTime da = parseIsoDateLoose(a.dateAdded);
            const QDateTime db = parseIsoDateLoose(b.dateAdded);
            if (da.isValid() && db.isValid() && da != db) {
                return sortDescending ? (da > db) : (da < db);
            }
            if (da.isValid() != db.isValid()) {
                return da.isValid();
            }
        } else {
            const QDateTime ma = parseIsoDateLoose(a.lastModified);
            const QDateTime mb = parseIsoDateLoose(b.lastModified);
            if (ma.isValid() && mb.isValid() && ma != mb) {
                return sortDescending ? (ma > mb) : (ma < mb);
            }
            if (ma.isValid() != mb.isValid()) {
                return ma.isValid();
            }
        }

        return QString::localeAwareCompare(a.name.toLower(), b.name.toLower()) < 0;
    });
}

DirectoryScanResult scanDirectoryFast(const QString &currentPath,
                                      bool includeHidden,
                                      bool directoriesFirst)
{
    DirectoryScanResult out;
    const QString resolved = QDir(currentPath).absolutePath();
    out.path = resolved;
    QFileInfo dirInfo(resolved);
    if (!dirInfo.exists() || !dirInfo.isDir()) {
        out.error = QStringLiteral("not-a-directory");
        return out;
    }

    QDir dir(resolved);
    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (includeHidden) {
        filters |= QDir::Hidden;
    }
    QFileInfoList rows = dir.entryInfoList(filters, QDir::NoSort);
    std::sort(rows.begin(), rows.end(), [directoriesFirst](const QFileInfo &a, const QFileInfo &b) {
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

    out.entries.reserve(rows.size());
    for (const QFileInfo &info : rows) {
        FileEntry e;
        e.name = info.fileName();
        e.path = info.absoluteFilePath();
        e.thumbnailPath = QString();
        e.suffix = info.suffix();
        e.mimeType = info.isDir()
                ? QStringLiteral("inode/directory")
                : QMimeDatabase().mimeTypeForFile(info.absoluteFilePath(), QMimeDatabase::MatchDefault).name();
        e.iconName = mimeIconForInfo(info);
        e.dateAdded = (info.birthTime().isValid() ? info.birthTime() : info.lastModified())
                .toString(Qt::ISODateWithMs);
        e.lastModified = info.lastModified().toString(Qt::ISODateWithMs);
        e.size = static_cast<qlonglong>(info.size());
        e.dir = info.isDir();
        e.hidden = info.isHidden();
        out.entries.push_back(e);
    }
    out.ok = true;
    return out;
}

QString cacheKeyForPath(const QString &path)
{
    const QString p = path.trimmed();
    if (p.contains(QStringLiteral("://"))) {
        return p;
    }
    return QDir(p).absolutePath();
}

bool isRecentVirtualPath(const QString &path)
{
    return path.trimmed() == QStringLiteral("__recent__");
}

bool isUriPath(const QString &path)
{
    return path.contains(QStringLiteral("://"));
}

QString joinChildPath(const QString &basePath, const QString &childName)
{
    if (!isUriPath(basePath)) {
        return QDir(basePath).filePath(childName);
    }
    QUrl base(basePath);
    if (!base.isValid()) {
        return QString();
    }
    QString p = base.path();
    if (!p.endsWith(QLatin1Char('/'))) {
        p += QLatin1Char('/');
    }
    base.setPath(p);
    QUrl rel;
    rel.setPath(childName);
    return base.resolved(rel).toString(QUrl::FullyEncoded);
}

QString parentPathOf(const QString &path)
{
    if (!isUriPath(path)) {
        QDir dir(path);
        if (!dir.cdUp()) {
            return QDir(path).absolutePath();
        }
        return dir.absolutePath();
    }
    QUrl url(path);
    if (!url.isValid()) {
        return path;
    }
    QString p = url.path();
    while (p.size() > 1 && p.endsWith(QLatin1Char('/'))) {
        p.chop(1);
    }
    const int slash = p.lastIndexOf(QLatin1Char('/'));
    if (slash <= 0) {
        p = QStringLiteral("/");
    } else {
        p = p.left(slash);
    }
    url.setPath(p);
    return url.toString(QUrl::FullyEncoded);
}

QString mimeIconForInfo(const QFileInfo &info)
{
    if (info.isDir()) {
        return QStringLiteral("folder");
    }
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(info.absoluteFilePath(), QMimeDatabase::MatchDefault);
    if (!mime.iconName().isEmpty()) {
        return mime.iconName();
    }
    if (!mime.genericIconName().isEmpty()) {
        return mime.genericIconName();
    }
    return QStringLiteral("text-x-generic-symbolic");
}

QVector<FileEntry> entriesFromApiResult(const QVariantList &rows)
{
    QVector<FileEntry> out;
    out.reserve(rows.size());
    for (const QVariant &v : rows) {
        const QVariantMap row = v.toMap();
        if (row.isEmpty()) {
            continue;
        }
        FileEntry e;
        e.name = row.value(QStringLiteral("name")).toString();
        e.path = row.value(QStringLiteral("path")).toString();
        e.thumbnailPath = row.value(QStringLiteral("thumbnailPath")).toString();
        e.suffix = row.value(QStringLiteral("suffix")).toString();
        e.mimeType = row.value(QStringLiteral("mimeType")).toString();
        e.iconName = row.value(QStringLiteral("iconName")).toString();
        e.dateAdded = row.value(QStringLiteral("dateAdded")).toString();
        e.lastModified = row.value(QStringLiteral("lastModified")).toString();
        e.size = row.value(QStringLiteral("size")).toLongLong();
        if (row.contains(QStringLiteral("dir"))) {
            e.dir = row.value(QStringLiteral("dir")).toBool();
        } else {
            e.dir = row.value(QStringLiteral("isDir")).toBool();
        }
        if (row.contains(QStringLiteral("hidden"))) {
            e.hidden = row.value(QStringLiteral("hidden")).toBool();
        } else {
            e.hidden = row.value(QStringLiteral("isHidden")).toBool();
        }
        if (e.iconName.isEmpty()) {
            QFileInfo fi(e.path);
            e.iconName = mimeIconForInfo(fi);
        }
        if (e.mimeType.isEmpty()) {
            QFileInfo fi(e.path);
            e.mimeType = fi.isDir()
                    ? QStringLiteral("inode/directory")
                    : QMimeDatabase().mimeTypeForFile(fi.absoluteFilePath(), QMimeDatabase::MatchDefault).name();
        }
        if (e.dateAdded.isEmpty()) {
            const QString created = row.value(QStringLiteral("created")).toString();
            const QString lastOpened = row.value(QStringLiteral("lastOpened")).toString();
            e.dateAdded = !created.isEmpty() ? created : (!lastOpened.isEmpty() ? lastOpened : e.lastModified);
        }
        out.push_back(e);
    }
    return out;
}

} // namespace

FileManagerModel::FileManagerModel(FileManagerApi *api,
                                   MetadataIndexServer *indexServer,
                                   QObject *parent)
    : QAbstractListModel(parent)
    , m_api(api)
    , m_indexServer(indexServer)
{
    m_currentPath = QDir::homePath();
    m_refreshDebounce.setSingleShot(true);
    m_refreshDebounce.setInterval(120);
    connect(&m_refreshDebounce, &QTimer::timeout, this, &FileManagerModel::refresh);
    if (m_api != nullptr) {
        connect(m_api, &FileManagerApi::directoryChanged, this, [this](const QString &changedPath) {
            if (m_api != nullptr && m_api->batchOperationActive()) {
                m_refreshDeferredByBatch = true;
                return;
            }
            const QString changed = QDir(changedPath).absolutePath();
            const QString current = QDir(m_currentPath).absolutePath();
            if (changed == current) {
                scheduleRefresh();
            }
        });
        connect(m_api, &FileManagerApi::batchOperationStateChanged, this, [this]() {
            if (m_api == nullptr) {
                return;
            }
            if (m_api->batchOperationActive()) {
                return;
            }
            if (m_refreshDeferredByBatch) {
                m_refreshDeferredByBatch = false;
                scheduleRefresh();
            }
        });
    }
    refresh();
    QTimer::singleShot(700, this, &FileManagerModel::prewarmCommonDirectories);
}

int FileManagerModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant FileManagerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }
    const FileEntry &e = m_entries.at(index.row());
    switch (role) {
    case NameRole: return e.name;
    case PathRole: return e.path;
    case ThumbnailPathRole: return e.thumbnailPath;
    case SuffixRole: return e.suffix;
    case MimeTypeRole: return e.mimeType;
    case IconNameRole: return e.iconName;
    case DateAddedRole: return e.dateAdded;
    case LastModifiedRole: return e.lastModified;
    case SizeRole: return e.size;
    case IsDirRole: return e.dir;
    case HiddenRole: return e.hidden;
    default: return {};
    }
}

QHash<int, QByteArray> FileManagerModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[PathRole] = "path";
    roles[ThumbnailPathRole] = "thumbnailPath";
    roles[SuffixRole] = "suffix";
    roles[MimeTypeRole] = "mimeType";
    roles[IconNameRole] = "iconName";
    roles[DateAddedRole] = "dateAdded";
    roles[LastModifiedRole] = "lastModified";
    roles[SizeRole] = "size";
    roles[IsDirRole] = "isDir";
    roles[HiddenRole] = "hidden";
    return roles;
}

QString FileManagerModel::currentPath() const
{
    return m_currentPath;
}

QString FileManagerModel::resolvePath(const QString &path) const
{
    QString p = path.trimmed();
    if (isRecentVirtualPath(p)) {
        return QStringLiteral("__recent__");
    }
    if (p.contains(QStringLiteral("://"))) {
        return p;
    }
    if (p == QStringLiteral("~")) {
        return QDir::homePath();
    }
    if (p.startsWith(QStringLiteral("~/"))) {
        p = QDir::homePath() + p.mid(1);
    }
    if (p.isEmpty()) {
        p = QDir::homePath();
    }
    return QDir(p).absolutePath();
}

void FileManagerModel::setCurrentPath(const QString &path)
{
    const QString resolved = resolvePath(path);
    if (resolved == m_currentPath) {
        return;
    }
    if (m_api != nullptr) {
        m_api->unwatchDirectory(m_currentPath);
    }
    m_currentPath = resolved;
    emit currentPathChanged();

    QVector<FileEntry> cachedEntries;
    if (cacheLoad(resolved, &cachedEntries)) {
        applyEntries(std::move(cachedEntries));
    } else if (!m_entries.isEmpty()) {
        applyEntries({});
    }

    refresh();
}

bool FileManagerModel::includeHidden() const
{
    return m_includeHidden;
}

void FileManagerModel::setIncludeHidden(bool include)
{
    if (m_includeHidden == include) {
        return;
    }
    m_includeHidden = include;
    emit includeHiddenChanged();
    refresh();
}

bool FileManagerModel::directoriesFirst() const
{
    return m_directoriesFirst;
}

void FileManagerModel::setDirectoriesFirst(bool enabled)
{
    if (m_directoriesFirst == enabled) {
        return;
    }
    m_directoriesFirst = enabled;
    emit directoriesFirstChanged();
    refresh();
}

QString FileManagerModel::searchText() const
{
    return m_searchText;
}

QString FileManagerModel::sortKey() const
{
    return m_sortKey;
}

void FileManagerModel::setSortKey(const QString &value)
{
    QString next = value.trimmed();
    if (next.isEmpty()) {
        next = QStringLiteral("dateModified");
    }
    if (m_sortKey == next) {
        return;
    }
    m_sortKey = next;
    emit sortKeyChanged();
    refresh();
}

bool FileManagerModel::sortDescending() const
{
    return m_sortDescending;
}

void FileManagerModel::setSortDescending(bool value)
{
    if (m_sortDescending == value) {
        return;
    }
    m_sortDescending = value;
    emit sortDescendingChanged();
    refresh();
}

void FileManagerModel::setSearchText(const QString &value)
{
    const QString next = value;
    const QString nextTrimmed = next.trimmed();
    const QString prevTrimmed = m_searchText.trimmed();
    if (next == m_searchText) {
        return;
    }

    // Instant stage: local filtering first for immediate UX response.
    if (!nextTrimmed.isEmpty()) {
        QVector<FileEntry> source;
        if (!prevTrimmed.isEmpty()
                && nextTrimmed.startsWith(prevTrimmed, Qt::CaseInsensitive)
                && m_lastSearchQuery.compare(prevTrimmed, Qt::CaseInsensitive) == 0
                && !m_lastSearchEntries.isEmpty()) {
            source = m_lastSearchEntries;
        } else if (!prevTrimmed.isEmpty()
                   && prevTrimmed.startsWith(nextTrimmed, Qt::CaseInsensitive)
                   && m_lastSearchQuery.compare(prevTrimmed, Qt::CaseInsensitive) == 0
                   && !m_lastSearchEntries.isEmpty()) {
            source = m_lastSearchEntries;
        } else {
            QVector<FileEntry> cached;
            if (cacheLoad(m_currentPath, &cached)) {
                source = std::move(cached);
            } else {
                source = m_entries;
            }
        }
        QVector<FileEntry> quick;
        quick.reserve(source.size());
        for (const FileEntry &e : source) {
            if (e.name.contains(nextTrimmed, Qt::CaseInsensitive)) {
                quick.push_back(e);
            }
        }
        sortEntries(quick, m_sortKey, m_sortDescending, m_directoriesFirst);
        applyEntries(std::move(quick));
        setLastError(QString());
    }

    m_searchText = next;
    emit searchTextChanged();
    if (nextTrimmed.isEmpty()) {
        m_lastSearchQuery.clear();
        m_lastSearchEntries.clear();
    }
    scheduleRefresh();
}

bool FileManagerModel::deepSearchRunning() const
{
    return m_deepSearchRunning;
}

int FileManagerModel::count() const
{
    return m_entries.size();
}

QString FileManagerModel::lastError() const
{
    return m_lastError;
}

void FileManagerModel::setLastError(const QString &error)
{
    const QString next = error.trimmed();
    if (next == m_lastError) {
        return;
    }
    m_lastError = next;
    emit lastErrorChanged();
}

void FileManagerModel::setDeepSearchRunning(bool running)
{
    if (m_deepSearchRunning == running) {
        return;
    }
    m_deepSearchRunning = running;
    emit deepSearchRunningChanged();
}

QVariantMap FileManagerModel::refresh()
{
    const QString targetPath = resolvePath(m_currentPath);
    const bool includeHidden = m_includeHidden;
    const bool directoriesFirst = m_directoriesFirst;
    const QString search = m_searchText.trimmed();
    const bool searchActive = !search.isEmpty();
    const bool deepSearchPlanned = searchActive && search.size() >= 2;
    ++m_refreshGeneration;
    const quint64 generation = m_refreshGeneration.load();
    if (deepSearchPlanned) {
        QMetaObject::invokeMethod(this, [this, generation]() {
            if (generation != m_refreshGeneration.load()) {
                return;
            }
            setDeepSearchRunning(true);
        }, Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(this, [this]() {
            setDeepSearchRunning(false);
        }, Qt::QueuedConnection);
    }
    QPointer<FileManagerModel> self(this);
    std::thread([self, generation, targetPath, includeHidden, directoriesFirst, search, searchActive]() {
        DirectoryScanResult scan;
        if (self && self->m_api != nullptr && isRecentVirtualPath(targetPath)) {
            scan.ok = true;
            scan.path = QStringLiteral("__recent__");
            const QVariantList recents = self->m_api->recentFiles(500);
            QVector<FileEntry> entries;
            entries.reserve(recents.size());
            for (const QVariant &rv : recents) {
                const QVariantMap row = rv.toMap();
                const QString p = row.value(QStringLiteral("path")).toString();
                QFileInfo fi(p);
                if (!fi.exists()) {
                    continue;
                }
                FileEntry e;
                e.name = fi.fileName().isEmpty() ? fi.absoluteFilePath() : fi.fileName();
                e.path = fi.absoluteFilePath();
                e.thumbnailPath = QString();
                e.suffix = fi.suffix();
                e.mimeType = fi.isDir()
                        ? QStringLiteral("inode/directory")
                        : QMimeDatabase().mimeTypeForFile(fi.absoluteFilePath(), QMimeDatabase::MatchDefault).name();
                const QString rowIcon = row.value(QStringLiteral("iconName")).toString();
                e.iconName = rowIcon.isEmpty() ? mimeIconForInfo(fi) : rowIcon;
                e.dateAdded = row.value(QStringLiteral("lastOpened")).toString();
                e.lastModified = row.value(QStringLiteral("lastOpened")).toString();
                e.size = static_cast<qlonglong>(fi.size());
                e.dir = fi.isDir();
                e.hidden = fi.isHidden();
                if (searchActive && !e.name.contains(search, Qt::CaseInsensitive)) {
                    continue;
                }
                entries.push_back(e);
            }
            scan.entries = std::move(entries);
        } else if (searchActive && self && self->m_api != nullptr) {
            if (search.size() < 2) {
                // For very short queries, return local filtered entries first.
                scan = scanDirectoryFast(targetPath, includeHidden, directoriesFirst);
                if (scan.ok) {
                    QVector<FileEntry> filtered;
                    filtered.reserve(scan.entries.size());
                    for (const FileEntry &e : scan.entries) {
                        if (e.name.contains(search, Qt::CaseInsensitive)) {
                            filtered.push_back(e);
                        }
                    }
                    scan.entries = std::move(filtered);
                }
            } else {
                const qulonglong searchSession = self->m_api->beginSearchSession();
                const QVariantMap res = self->m_api->searchDirectory(targetPath,
                                                                     search,
                                                                     includeHidden,
                                                                     directoriesFirst,
                                                                     1000,
                                                                     searchSession);
                if (res.value(QStringLiteral("cancelled")).toBool()) {
                    QMetaObject::invokeMethod(self.data(), [self, generation]() {
                        if (!self || generation != self->m_refreshGeneration.load()) {
                            return;
                        }
                        self->setDeepSearchRunning(false);
                    }, Qt::QueuedConnection);
                    return;
                }
                if (res.value(QStringLiteral("ok")).toBool()) {
                    scan.ok = true;
                    scan.path = res.value(QStringLiteral("path")).toString();
                    scan.entries = entriesFromApiResult(res.value(QStringLiteral("entries")).toList());
                } else {
                    scan.ok = false;
                    scan.path = targetPath;
                    scan.error = res.value(QStringLiteral("error")).toString();
                }
            }
        } else {
            if (self && self->m_api != nullptr) {
                self->m_api->beginSearchSession();
            }
            scan = scanDirectoryFast(targetPath, includeHidden, directoriesFirst);
        }
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(), [self, generation, scan = std::move(scan), searchActive, search]() mutable {
            if (!self || generation != self->m_refreshGeneration.load()) {
                return;
            }
            self->setDeepSearchRunning(false);
            if (!scan.ok) {
                self->setLastError(scan.error);
                return;
            }
            if (!scan.path.isEmpty() && scan.path != self->m_currentPath) {
                self->m_currentPath = scan.path;
                emit self->currentPathChanged();
            }
            if (!searchActive) {
                self->cacheStore(self->m_currentPath, scan.entries);
                self->m_lastSearchQuery.clear();
                self->m_lastSearchEntries.clear();
            } else {
                self->m_lastSearchQuery = search.trimmed();
                self->m_lastSearchEntries = scan.entries;
            }
            self->applyEntries(std::move(scan.entries));
            self->setLastError(QString());
            if (self->m_api != nullptr
                    && !self->m_currentPath.contains(QStringLiteral("://"))
                    && !isRecentVirtualPath(self->m_currentPath)
                    && !self->m_currentPath.startsWith(QStringLiteral("__"))) {
                self->m_api->watchDirectory(self->m_currentPath);
            }
        }, Qt::QueuedConnection);
    }).detach();

    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("path"), targetPath},
        {QStringLiteral("scheduled"), true}
    };
}

void FileManagerModel::setSort(const QString &key, bool descending)
{
    const QString nextKey = key.trimmed().isEmpty() ? QStringLiteral("dateModified") : key.trimmed();
    const bool changed = (m_sortKey != nextKey) || (m_sortDescending != descending);
    m_sortKey = nextKey;
    m_sortDescending = descending;
    if (!changed) {
        return;
    }
    emit sortKeyChanged();
    emit sortDescendingChanged();
    refresh();
}

void FileManagerModel::applyEntries(QVector<FileEntry> entries)
{
    sortEntries(entries, m_sortKey, m_sortDescending, m_directoriesFirst);
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
    emit countChanged();
}

void FileManagerModel::cacheStore(const QString &path, const QVector<FileEntry> &entries)
{
    const QString key = cacheKeyForPath(path);
    if (key.isEmpty()) {
        return;
    }
    m_dirCache.insert(key, entries);
    m_dirCacheOrder.removeAll(key);
    m_dirCacheOrder.push_back(key);
    static const int kMaxCache = 12;
    while (m_dirCacheOrder.size() > kMaxCache) {
        const QString evict = m_dirCacheOrder.takeFirst();
        m_dirCache.remove(evict);
    }
}

bool FileManagerModel::cacheLoad(const QString &path, QVector<FileEntry> *out) const
{
    if (out == nullptr) {
        return false;
    }
    const QString key = cacheKeyForPath(path);
    const auto it = m_dirCache.constFind(key);
    if (it == m_dirCache.constEnd()) {
        return false;
    }
    *out = it.value();
    return true;
}

void FileManagerModel::scheduleRefresh()
{
    const bool searching = !m_searchText.trimmed().isEmpty();
    const int interval = searching ? 35 : 120;
    if (m_refreshDebounce.interval() != interval) {
        m_refreshDebounce.setInterval(interval);
    }
    m_refreshDebounce.start();
}

void FileManagerModel::prewarmCommonDirectories()
{
    const QStringList candidates = {
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
    };

    QPointer<FileManagerModel> self(this);
    std::thread([self, candidates]() {
        for (const QString &raw : candidates) {
            if (!self) {
                return;
            }
            const QString path = QDir(raw).absolutePath();
            if (path.isEmpty()) {
                continue;
            }
            DirectoryScanResult scan = scanDirectoryFast(path, false, true);
            if (!scan.ok) {
                continue;
            }
            QMetaObject::invokeMethod(self.data(), [self, path, entries = std::move(scan.entries)]() mutable {
                if (!self) {
                    return;
                }
                self->cacheStore(path, entries);
            }, Qt::QueuedConnection);
        }
    }).detach();
}

QVariantMap FileManagerModel::goUp()
{
    if (isRecentVirtualPath(m_currentPath)) {
        setCurrentPath(QStringLiteral("~"));
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("path"), m_currentPath}
        };
    }
    const QString parent = parentPathOf(m_currentPath);
    if (parent == m_currentPath || parent.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("already-root")},
            {QStringLiteral("path"), m_currentPath}
        };
    }
    setCurrentPath(parent);
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("path"), m_currentPath}
    };
}

QVariantMap FileManagerModel::entryAt(int index) const
{
    if (index < 0 || index >= m_entries.size()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("index-out-of-range")}
        };
    }
    const FileEntry &e = m_entries.at(index);
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("name"), e.name},
        {QStringLiteral("path"), e.path},
        {QStringLiteral("thumbnailPath"), e.thumbnailPath},
        {QStringLiteral("mimeType"), e.mimeType},
        {QStringLiteral("iconName"), e.iconName},
        {QStringLiteral("isDir"), e.dir},
        {QStringLiteral("size"), e.size},
        {QStringLiteral("dateAdded"), e.dateAdded},
        {QStringLiteral("lastModified"), e.lastModified}
    };
}

QVariantMap FileManagerModel::activate(int index)
{
    const QVariantMap entry = entryAt(index);
    if (!entry.value(QStringLiteral("ok")).toBool()) {
        return entry;
    }
    if (entry.value(QStringLiteral("isDir")).toBool()) {
        setCurrentPath(entry.value(QStringLiteral("path")).toString());
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("type"), QStringLiteral("directory")},
            {QStringLiteral("path"), m_currentPath}
        };
    }
    QVariantMap out = entry;
    out.insert(QStringLiteral("type"), QStringLiteral("file"));
    return out;
}

QVariantMap FileManagerModel::createDirectory(const QString &name)
{
    if (m_api == nullptr) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("api-unavailable")}};
    }
    const QString base = name.trimmed();
    if (base.isEmpty()) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("invalid-name")}};
    }
    const QString path = joinChildPath(m_currentPath, base);
    if (path.isEmpty()) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("invalid-path")}};
    }
    const QVariantMap result = m_api->createDirectory(path, true);
    if (result.value(QStringLiteral("ok")).toBool()) {
        refresh();
    }
    return result;
}

QVariantMap FileManagerModel::createFile(const QString &name)
{
    if (m_api == nullptr) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("api-unavailable")}};
    }
    const QString base = name.trimmed();
    if (base.isEmpty()) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("invalid-name")}};
    }
    const QString path = joinChildPath(m_currentPath, base);
    if (path.isEmpty()) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("invalid-path")}};
    }
    const QVariantMap result = m_api->createEmptyFile(path);
    if (result.value(QStringLiteral("ok")).toBool()) {
        refresh();
    }
    return result;
}

QVariantMap FileManagerModel::removeAt(int index, bool recursive)
{
    if (m_api == nullptr) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("api-unavailable")}};
    }
    const QVariantMap entry = entryAt(index);
    if (!entry.value(QStringLiteral("ok")).toBool()) {
        return entry;
    }
    const QVariantMap result = m_api->removePath(entry.value(QStringLiteral("path")).toString(), recursive);
    if (result.value(QStringLiteral("ok")).toBool()) {
        refresh();
    }
    return result;
}

QVariantMap FileManagerModel::renameAt(int index, const QString &newName)
{
    if (m_api == nullptr) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("api-unavailable")}};
    }
    const QVariantMap entry = entryAt(index);
    if (!entry.value(QStringLiteral("ok")).toBool()) {
        return entry;
    }
    const QString cleaned = newName.trimmed();
    if (cleaned.isEmpty()) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("invalid-name")}};
    }
    const QString fromPath = entry.value(QStringLiteral("path")).toString();
    const QString parent = parentPathOf(fromPath);
    const QString toPath = joinChildPath(parent, cleaned);
    if (toPath.isEmpty()) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("invalid-path")}};
    }
    const QVariantMap result = m_api->renamePath(fromPath, toPath);
    if (result.value(QStringLiteral("ok")).toBool()) {
        refresh();
    }
    return result;
}
