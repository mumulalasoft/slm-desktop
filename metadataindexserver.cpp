#include "metadataindexserver.h"

#include "filemanagerapi.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

namespace {

QString normalizeInputPath(const QString &path)
{
    QString out = path.trimmed();
    if (out == QStringLiteral("~")) {
        return QDir::homePath();
    }
    if (out.startsWith(QStringLiteral("~/"))) {
        out = QDir::homePath() + out.mid(1);
    }
    return out;
}

QString iconForInfo(const QFileInfo &info)
{
    if (info.isDir()) {
        return QStringLiteral("folder");
    }
    static QMimeDatabase mimeDb;
    const auto mime = mimeDb.mimeTypeForFile(info, QMimeDatabase::MatchExtension);
    QString icon = mime.iconName().trimmed();
    if (icon.isEmpty()) {
        icon = mime.genericIconName().trimmed();
    }
    if (icon.isEmpty()) {
        icon = QStringLiteral("text-x-generic-symbolic");
    }
    return icon;
}

} // namespace

MetadataIndexServer::MetadataIndexServer(FileManagerApi *fileApi, QObject *parent)
    : QObject(parent)
    , m_fileApi(fileApi)
{
    if (m_fileApi != nullptr) {
        connect(m_fileApi, &FileManagerApi::pathChanged,
                this, &MetadataIndexServer::onPathChanged);
    }
}

MetadataIndexServer::~MetadataIndexServer()
{
    if (m_dbConnectionName.isEmpty()) {
        return;
    }
    if (QSqlDatabase::contains(m_dbConnectionName)) {
        QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
        if (db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(m_dbConnectionName);
}

bool MetadataIndexServer::ready() const
{
    return m_dbReady;
}

int MetadataIndexServer::indexedCount() const
{
    return m_indexedCount;
}

bool MetadataIndexServer::indexing() const
{
    return m_indexing;
}

QString MetadataIndexServer::indexingPath() const
{
    return m_indexingPath;
}

int MetadataIndexServer::indexingCurrent() const
{
    return m_indexingCurrent;
}

int MetadataIndexServer::indexingTotal() const
{
    return m_indexingTotal;
}

void MetadataIndexServer::setIndexedCountInternal(int value)
{
    if (m_indexedCount == value) {
        return;
    }
    m_indexedCount = value;
    emit indexedCountChanged();
}

QString MetadataIndexServer::databasePath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share/DesktopShell");
    }
    QDir dir(base);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return dir.filePath(QStringLiteral("metadata_index.db"));
}

bool MetadataIndexServer::ensureDatabase(QString *error)
{
    if (m_dbReady && !m_dbConnectionName.isEmpty() && QSqlDatabase::contains(m_dbConnectionName)) {
        return true;
    }

    if (m_dbConnectionName.isEmpty()) {
        m_dbConnectionName = QStringLiteral("desktop_shell_metadata_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_dbConnectionName);
    db.setDatabaseName(databasePath());
    if (!db.open()) {
        if (error != nullptr) {
            *error = db.lastError().text();
        }
        return false;
    }

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS metadata_entries ("
            " path TEXT PRIMARY KEY,"
            " path_lc TEXT,"
            " parent_path TEXT,"
            " name TEXT,"
            " name_lc TEXT,"
            " ext TEXT,"
            " icon_name TEXT,"
            " is_dir INTEGER,"
            " size INTEGER,"
            " last_modified TEXT,"
            " mtime_msecs INTEGER,"
            " indexed_at TEXT"
            ")"))) {
        if (error != nullptr) {
            *error = q.lastError().text();
        }
        return false;
    }

    const QStringList indexes{
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_meta_name_lc ON metadata_entries(name_lc)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_meta_path_lc ON metadata_entries(path_lc)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_meta_parent_path ON metadata_entries(parent_path)")
    };
    for (const QString &sql : indexes) {
        if (!q.exec(sql)) {
            if (error != nullptr) {
                *error = q.lastError().text();
            }
            return false;
        }
    }

    if (q.exec(QStringLiteral("SELECT COUNT(1) FROM metadata_entries")) && q.next()) {
        setIndexedCountInternal(q.value(0).toInt());
    }

    m_dbReady = true;
    emit readyChanged();
    return true;
}

QString MetadataIndexServer::normalizePath(const QString &path) const
{
    const QString raw = normalizeInputPath(path);
    if (raw.isEmpty()) {
        return QString();
    }
    return QFileInfo(raw).absoluteFilePath();
}

QVariantMap MetadataIndexServer::upsertEntry(const QString &path)
{
    QString err;
    if (!ensureDatabase(&err)) {
        return { { QStringLiteral("ok"), false }, { QStringLiteral("error"), err } };
    }

    const QString normalized = normalizePath(path);
    QFileInfo info(normalized);
    if (!info.exists()) {
        return removePath(normalized);
    }

    QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO metadata_entries("
        " path, path_lc, parent_path, name, name_lc, ext, icon_name, is_dir, size, last_modified, mtime_msecs, indexed_at"
        ") VALUES("
        " :path, :path_lc, :parent_path, :name, :name_lc, :ext, :icon_name, :is_dir, :size, :last_modified, :mtime_msecs, :indexed_at"
        ") ON CONFLICT(path) DO UPDATE SET"
        " path_lc=excluded.path_lc,"
        " parent_path=excluded.parent_path,"
        " name=excluded.name,"
        " name_lc=excluded.name_lc,"
        " ext=excluded.ext,"
        " icon_name=excluded.icon_name,"
        " is_dir=excluded.is_dir,"
        " size=excluded.size,"
        " last_modified=excluded.last_modified,"
        " mtime_msecs=excluded.mtime_msecs,"
        " indexed_at=excluded.indexed_at"));

    q.bindValue(QStringLiteral(":path"), normalized);
    q.bindValue(QStringLiteral(":path_lc"), normalized.toLower());
    q.bindValue(QStringLiteral(":parent_path"), info.absoluteDir().absolutePath());
    q.bindValue(QStringLiteral(":name"), info.fileName());
    q.bindValue(QStringLiteral(":name_lc"), info.fileName().toLower());
    q.bindValue(QStringLiteral(":ext"), info.suffix().toLower());
    q.bindValue(QStringLiteral(":icon_name"), iconForInfo(info));
    q.bindValue(QStringLiteral(":is_dir"), info.isDir() ? 1 : 0);
    q.bindValue(QStringLiteral(":size"), static_cast<qlonglong>(info.size()));
    q.bindValue(QStringLiteral(":last_modified"), info.lastModified().toString(Qt::ISODateWithMs));
    q.bindValue(QStringLiteral(":mtime_msecs"), info.lastModified().toMSecsSinceEpoch());
    q.bindValue(QStringLiteral(":indexed_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    if (!q.exec()) {
        return { { QStringLiteral("ok"), false }, { QStringLiteral("error"), q.lastError().text() } };
    }
    return { { QStringLiteral("ok"), true }, { QStringLiteral("path"), normalized } };
}

QVariantMap MetadataIndexServer::upsertDirectoryChildren(const QString &dirPath, bool recursive)
{
    QString err;
    if (!ensureDatabase(&err)) {
        return { { QStringLiteral("ok"), false }, { QStringLiteral("error"), err } };
    }

    const QString root = normalizePath(dirPath);
    QFileInfo rootInfo(root);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        return { { QStringLiteral("ok"), false }, { QStringLiteral("error"), QStringLiteral("not-a-directory") } };
    }

    emit indexingStarted(root);
    QVariantMap rootRes = upsertEntry(root);
    if (!rootRes.value(QStringLiteral("ok")).toBool()) {
        return rootRes;
    }

    int indexed = 1;
    int progress = 1;
    emit indexingProgress(root, progress, 0);

    QDirIterator::IteratorFlags flags = recursive
        ? QDirIterator::Subdirectories
        : QDirIterator::NoIteratorFlags;
    QDirIterator it(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, flags);
    while (it.hasNext()) {
        const QString path = it.next();
        const QVariantMap r = upsertEntry(path);
        if (!r.value(QStringLiteral("ok")).toBool()) {
            continue;
        }
        ++indexed;
        ++progress;
        if ((progress % 200) == 0) {
            emit indexingProgress(root, progress, 0);
        }
    }

    QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
    QSqlQuery countQuery(db);
    if (countQuery.exec(QStringLiteral("SELECT COUNT(1) FROM metadata_entries")) && countQuery.next()) {
        setIndexedCountInternal(countQuery.value(0).toInt());
    }

    emit indexingProgress(root, progress, progress);
    emit indexingFinished(root, indexed);
    emit indexChanged(root);
    return {
        { QStringLiteral("ok"), true },
        { QStringLiteral("path"), root },
        { QStringLiteral("indexed"), indexed }
    };
}

QVariantMap MetadataIndexServer::ensureDefaultIndex()
{
    if (m_defaultIndexed) {
        return { { QStringLiteral("ok"), true }, { QStringLiteral("indexed"), false } };
    }

    QStringList defaults;
    defaults << QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
             << QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
             << QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
             << QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
             << QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    defaults.removeAll(QString());
    defaults.removeDuplicates();

    int scanned = 0;
    for (const QString &root : defaults) {
        addRoot(root);
        const QVariantMap res = upsertDirectoryChildren(root, true);
        if (res.value(QStringLiteral("ok")).toBool()) {
            ++scanned;
        }
    }
    m_defaultIndexed = true;
    return { { QStringLiteral("ok"), true }, { QStringLiteral("indexedRoots"), scanned } };
}

bool MetadataIndexServer::startDefaultIndexing()
{
    if (m_indexing) {
        return false;
    }
    QStringList defaults;
    defaults << QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
             << QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
             << QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
             << QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
             << QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    defaults.removeAll(QString());
    defaults.removeDuplicates();

    m_defaultIndexQueue.clear();
    for (const QString &root : defaults) {
        if (QFileInfo(root).exists() && QFileInfo(root).isDir()) {
            m_defaultIndexQueue.push_back(root);
        }
    }
    if (m_defaultIndexQueue.isEmpty()) {
        return false;
    }
    m_defaultIndexed = true;
    const QString first = m_defaultIndexQueue.takeFirst();
    return startIndexPath(first, true);
}

bool MetadataIndexServer::startIndexPath(const QString &path, bool recursive)
{
    if (m_indexing) {
        return false;
    }
    QString err;
    if (!ensureDatabase(&err)) {
        return false;
    }

    const QString normalized = normalizePath(path);
    QFileInfo info(normalized);
    if (normalized.isEmpty() || !info.exists()) {
        return false;
    }

    m_indexing = true;
    m_indexingPath = normalized;
    m_indexingCurrent = 0;
    m_indexingTotal = 1;
    m_indexingRecursive = recursive;
    m_pendingPaths.clear();
    m_pendingPaths.push_back(normalized);

    addRoot(normalized);
    emit indexingStateChanged();
    emit indexingStarted(normalized);
    emit indexingProgress(normalized, m_indexingCurrent, m_indexingTotal);
    QTimer::singleShot(0, this, &MetadataIndexServer::processIndexBatch);
    return true;
}

void MetadataIndexServer::cancelIndexing()
{
    m_pendingPaths.clear();
    m_defaultIndexQueue.clear();
    finishIndexingJob(false);
}

void MetadataIndexServer::finishIndexingJob(bool emitFinished)
{
    if (!m_indexing) {
        return;
    }
    const QString finishedPath = m_indexingPath;
    const int finishedCount = m_indexingCurrent;
    m_indexing = false;
    m_indexingPath.clear();
    m_indexingCurrent = 0;
    m_indexingTotal = 0;
    m_pendingPaths.clear();
    emit indexingStateChanged();

    if (emitFinished) {
        emit indexingFinished(finishedPath, finishedCount);
        emit indexChanged(finishedPath);
    }

    if (!m_defaultIndexQueue.isEmpty()) {
        const QString nextRoot = m_defaultIndexQueue.takeFirst();
        startIndexPath(nextRoot, true);
    }
}

void MetadataIndexServer::processIndexBatch()
{
    if (!m_indexing) {
        return;
    }

    static const int kBatchSize = 32;
    int processed = 0;
    while (processed < kBatchSize && !m_pendingPaths.isEmpty()) {
        const QString path = m_pendingPaths.takeFirst();
        QFileInfo info(path);
        if (!info.exists()) {
            ++processed;
            continue;
        }
        const QVariantMap res = upsertEntry(path);
        if (res.value(QStringLiteral("ok")).toBool()) {
            ++m_indexingCurrent;
        }
        if (m_indexingRecursive && info.isDir() && !info.isSymLink()) {
            QDir dir(path);
            const QFileInfoList entries = dir.entryInfoList(
                QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden);
            for (const QFileInfo &entry : entries) {
                m_pendingPaths.push_back(entry.absoluteFilePath());
            }
            m_indexingTotal += entries.size();
        }
        ++processed;
    }

    emit indexingProgress(m_indexingPath, m_indexingCurrent, m_indexingTotal);
    emit indexingStateChanged();

    if (m_pendingPaths.isEmpty()) {
        QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
        QSqlQuery countQuery(db);
        if (countQuery.exec(QStringLiteral("SELECT COUNT(1) FROM metadata_entries")) && countQuery.next()) {
            setIndexedCountInternal(countQuery.value(0).toInt());
        }
        finishIndexingJob(true);
        return;
    }

    QTimer::singleShot(12, this, &MetadataIndexServer::processIndexBatch);
}

QVariantMap MetadataIndexServer::indexPath(const QString &path, bool recursive)
{
    const QString normalized = normalizePath(path);
    if (normalized.isEmpty()) {
        return { { QStringLiteral("ok"), false }, { QStringLiteral("error"), QStringLiteral("invalid-path") } };
    }
    QFileInfo info(normalized);
    if (!info.exists()) {
        return removePath(normalized);
    }
    if (info.isDir()) {
        return upsertDirectoryChildren(normalized, recursive);
    }
    QVariantMap res = upsertEntry(normalized);
    if (res.value(QStringLiteral("ok")).toBool()) {
        emit indexChanged(normalized);
    }
    return res;
}

QVariantMap MetadataIndexServer::removePath(const QString &path)
{
    QString err;
    if (!ensureDatabase(&err)) {
        return { { QStringLiteral("ok"), false }, { QStringLiteral("error"), err } };
    }

    const QString normalized = normalizePath(path);
    if (normalized.isEmpty()) {
        return { { QStringLiteral("ok"), false }, { QStringLiteral("error"), QStringLiteral("invalid-path") } };
    }

    QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "DELETE FROM metadata_entries WHERE path = :path OR path LIKE :prefix"));
    q.bindValue(QStringLiteral(":path"), normalized);
    q.bindValue(QStringLiteral(":prefix"), normalized + QStringLiteral("/%"));
    if (!q.exec()) {
        return { { QStringLiteral("ok"), false }, { QStringLiteral("error"), q.lastError().text() } };
    }

    QSqlQuery countQuery(db);
    if (countQuery.exec(QStringLiteral("SELECT COUNT(1) FROM metadata_entries")) && countQuery.next()) {
        setIndexedCountInternal(countQuery.value(0).toInt());
    }

    emit indexChanged(normalized);
    return { { QStringLiteral("ok"), true }, { QStringLiteral("path"), normalized } };
}

QVariantList MetadataIndexServer::search(const QString &query, int limit, const QString &underPath)
{
    QVariantList out;
    QString err;
    if (!ensureDatabase(&err)) {
        return out;
    }

    QString qText = query.trimmed().toLower();
    if (qText.isEmpty()) {
        return out;
    }
    int maxRows = qBound(1, limit, 500);

    QString sql =
        QStringLiteral("SELECT path, name, icon_name, is_dir, size, last_modified, mtime_msecs, "
                       "CASE "
                       "WHEN name_lc = :exact THEN 500 "
                       "WHEN name_lc LIKE :prefix THEN 300 "
                       "WHEN path_lc LIKE :pathPrefix THEN 180 "
                       "WHEN name_lc LIKE :q THEN 120 "
                       "WHEN path_lc LIKE :q THEN 70 "
                       "ELSE 0 END AS score "
                       "FROM metadata_entries "
                       "WHERE (name_lc LIKE :q OR path_lc LIKE :q)");

    const QString root = normalizePath(underPath).toLower();
    const bool hasRoot = !root.isEmpty();
    if (hasRoot) {
        sql += QStringLiteral(" AND (path_lc = :root OR path_lc LIKE :rootPrefix)");
    }
    sql += QStringLiteral(" ORDER BY score DESC, mtime_msecs DESC, is_dir DESC, name_lc ASC LIMIT :limit");

    QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
    QSqlQuery q(db);
    q.prepare(sql);
    q.bindValue(QStringLiteral(":q"), QStringLiteral("%") + qText + QStringLiteral("%"));
    q.bindValue(QStringLiteral(":exact"), qText);
    q.bindValue(QStringLiteral(":prefix"), qText + QStringLiteral("%"));
    q.bindValue(QStringLiteral(":pathPrefix"), QStringLiteral("%/") + qText + QStringLiteral("%"));
    if (hasRoot) {
        q.bindValue(QStringLiteral(":root"), root);
        q.bindValue(QStringLiteral(":rootPrefix"), root + QStringLiteral("/%"));
    }
    q.bindValue(QStringLiteral(":limit"), maxRows);
    if (!q.exec()) {
        return out;
    }

    while (q.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("ok"), true);
        row.insert(QStringLiteral("path"), q.value(0).toString());
        row.insert(QStringLiteral("name"), q.value(1).toString());
        row.insert(QStringLiteral("iconName"), q.value(2).toString());
        row.insert(QStringLiteral("isDir"), q.value(3).toInt() == 1);
        row.insert(QStringLiteral("size"), q.value(4).toLongLong());
        row.insert(QStringLiteral("lastModified"), q.value(5).toString());
        row.insert(QStringLiteral("mtimeMs"), q.value(6).toLongLong());
        row.insert(QStringLiteral("score"), q.value(7).toInt());
        out.push_back(row);
    }
    return out;
}

QVariantList MetadataIndexServer::searchAdvanced(const QString &query,
                                                 const QVariantMap &options,
                                                 int limit)
{
    QVariantList out;
    QString err;
    if (!ensureDatabase(&err)) {
        return out;
    }

    const QString qText = query.trimmed().toLower();
    if (qText.isEmpty()) {
        return out;
    }
    const int maxRows = qBound(1, limit, 1000);
    const QString kind = options.value(QStringLiteral("kind")).toString().trimmed().toLower(); // any|file|dir
    const QString underPath = normalizePath(options.value(QStringLiteral("underPath")).toString()).toLower();
    const qlonglong sizeMin = options.value(QStringLiteral("sizeMin")).toLongLong();
    const qlonglong sizeMax = options.contains(QStringLiteral("sizeMax"))
        ? options.value(QStringLiteral("sizeMax")).toLongLong()
        : -1;
    const qlonglong modifiedAfterMs = options.value(QStringLiteral("modifiedAfterMs")).toLongLong();
    const qlonglong modifiedBeforeMs = options.contains(QStringLiteral("modifiedBeforeMs"))
        ? options.value(QStringLiteral("modifiedBeforeMs")).toLongLong()
        : -1;

    QString sql =
        QStringLiteral("SELECT path, name, icon_name, is_dir, size, last_modified, mtime_msecs, "
                       "CASE "
                       "WHEN name_lc = :exact THEN 500 "
                       "WHEN name_lc LIKE :prefix THEN 300 "
                       "WHEN path_lc LIKE :pathPrefix THEN 180 "
                       "WHEN name_lc LIKE :q THEN 120 "
                       "WHEN path_lc LIKE :q THEN 70 "
                       "ELSE 0 END AS score "
                       "FROM metadata_entries WHERE (name_lc LIKE :q OR path_lc LIKE :q)");

    if (!underPath.isEmpty()) {
        sql += QStringLiteral(" AND (path_lc = :root OR path_lc LIKE :rootPrefix)");
    }
    if (kind == QStringLiteral("file")) {
        sql += QStringLiteral(" AND is_dir = 0");
    } else if (kind == QStringLiteral("dir") || kind == QStringLiteral("directory")) {
        sql += QStringLiteral(" AND is_dir = 1");
    }
    if (sizeMin > 0) {
        sql += QStringLiteral(" AND size >= :sizeMin");
    }
    if (sizeMax >= 0) {
        sql += QStringLiteral(" AND size <= :sizeMax");
    }
    if (modifiedAfterMs > 0) {
        sql += QStringLiteral(" AND mtime_msecs >= :modifiedAfterMs");
    }
    if (modifiedBeforeMs >= 0) {
        sql += QStringLiteral(" AND mtime_msecs <= :modifiedBeforeMs");
    }

    sql += QStringLiteral(" ORDER BY score DESC, mtime_msecs DESC, is_dir DESC, name_lc ASC LIMIT :limit");

    QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
    QSqlQuery q(db);
    q.prepare(sql);
    q.bindValue(QStringLiteral(":q"), QStringLiteral("%") + qText + QStringLiteral("%"));
    q.bindValue(QStringLiteral(":exact"), qText);
    q.bindValue(QStringLiteral(":prefix"), qText + QStringLiteral("%"));
    q.bindValue(QStringLiteral(":pathPrefix"), QStringLiteral("%/") + qText + QStringLiteral("%"));
    if (!underPath.isEmpty()) {
        q.bindValue(QStringLiteral(":root"), underPath);
        q.bindValue(QStringLiteral(":rootPrefix"), underPath + QStringLiteral("/%"));
    }
    if (sizeMin > 0) {
        q.bindValue(QStringLiteral(":sizeMin"), sizeMin);
    }
    if (sizeMax >= 0) {
        q.bindValue(QStringLiteral(":sizeMax"), sizeMax);
    }
    if (modifiedAfterMs > 0) {
        q.bindValue(QStringLiteral(":modifiedAfterMs"), modifiedAfterMs);
    }
    if (modifiedBeforeMs >= 0) {
        q.bindValue(QStringLiteral(":modifiedBeforeMs"), modifiedBeforeMs);
    }
    q.bindValue(QStringLiteral(":limit"), maxRows);

    if (!q.exec()) {
        return out;
    }

    while (q.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("ok"), true);
        row.insert(QStringLiteral("path"), q.value(0).toString());
        row.insert(QStringLiteral("name"), q.value(1).toString());
        row.insert(QStringLiteral("iconName"), q.value(2).toString());
        row.insert(QStringLiteral("isDir"), q.value(3).toInt() == 1);
        row.insert(QStringLiteral("size"), q.value(4).toLongLong());
        row.insert(QStringLiteral("lastModified"), q.value(5).toString());
        row.insert(QStringLiteral("mtimeMs"), q.value(6).toLongLong());
        row.insert(QStringLiteral("score"), q.value(7).toInt());
        out.push_back(row);
    }
    return out;
}

QVariantMap MetadataIndexServer::stats()
{
    QString err;
    if (!ensureDatabase(&err)) {
        return { { QStringLiteral("ok"), false }, { QStringLiteral("error"), err } };
    }
    return {
        { QStringLiteral("ok"), true },
        { QStringLiteral("ready"), m_dbReady },
        { QStringLiteral("indexedCount"), m_indexedCount },
        { QStringLiteral("roots"), roots() }
    };
}

bool MetadataIndexServer::addRoot(const QString &path)
{
    const QString root = normalizePath(path);
    QFileInfo info(root);
    if (root.isEmpty() || !info.exists() || !info.isDir()) {
        return false;
    }
    if (!m_roots.contains(root)) {
        m_roots.push_back(root);
    }
    if (m_fileApi != nullptr) {
        m_fileApi->watchDirectory(root);
    }
    return true;
}

bool MetadataIndexServer::removeRoot(const QString &path)
{
    const QString root = normalizePath(path);
    if (root.isEmpty()) {
        return false;
    }
    m_roots.removeAll(root);
    if (m_fileApi != nullptr) {
        m_fileApi->unwatchDirectory(root);
    }
    return true;
}

QVariantList MetadataIndexServer::roots() const
{
    QVariantList out;
    out.reserve(m_roots.size());
    for (const QString &root : m_roots) {
        out.push_back(root);
    }
    return out;
}

void MetadataIndexServer::onPathChanged(const QString &path, const QString &kind)
{
    Q_UNUSED(kind);
    const QString normalized = normalizePath(path);
    if (normalized.isEmpty()) {
        return;
    }
    if (!m_pendingPathChanges.contains(normalized)) {
        m_pendingPathChanges.push_back(normalized);
    }
    if (!m_pathChangeFlushScheduled) {
        m_pathChangeFlushScheduled = true;
        QTimer::singleShot(120, this, &MetadataIndexServer::processPathChangeQueue);
    }
}

void MetadataIndexServer::processPathChangeQueue()
{
    m_pathChangeFlushScheduled = false;
    if (m_pendingPathChanges.isEmpty()) {
        return;
    }
    static const int kBatchSize = 24;
    int processed = 0;
    while (processed < kBatchSize && !m_pendingPathChanges.isEmpty()) {
        const QString normalized = m_pendingPathChanges.takeFirst();
        QFileInfo info(normalized);
        if (!info.exists()) {
            removePath(normalized);
        } else {
            // Keep incremental updates cheap; deep scans are handled by explicit indexing.
            upsertEntry(normalized);
        }
        ++processed;
    }
    if (!m_pendingPathChanges.isEmpty()) {
        m_pathChangeFlushScheduled = true;
        QTimer::singleShot(80, this, &MetadataIndexServer::processPathChangeQueue);
    }
}
