#include "StorageManager.h"

#include "clipboardtypes.h"

#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

namespace Slm::Clipboard {

StorageManager::StorageManager()
    : m_connectionName(QStringLiteral("slm-clipboard-") + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

StorageManager::~StorageManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    if (!m_connectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

QString StorageManager::dbPath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share/slm-desktop");
    }
    QDir dir(base);
    dir.mkpath(QStringLiteral("."));
    return dir.filePath(QStringLiteral("clipboard-history.sqlite"));
}

bool StorageManager::open()
{
    if (m_db.isOpen()) {
        return true;
    }
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(dbPath());
    if (!m_db.open()) {
        return false;
    }
    return ensureSchema();
}

bool StorageManager::isOpen() const
{
    return m_db.isOpen();
}

bool StorageManager::ensureSchema()
{
    QSqlQuery q(m_db);
    return q.exec(QStringLiteral(
               "CREATE TABLE IF NOT EXISTS clipboard_items ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "content TEXT,"
               "mimeType TEXT,"
               "type TEXT,"
               "timestamp INTEGER,"
               "source_app TEXT,"
               "preview TEXT,"
               "pinned INTEGER DEFAULT 0"
               ")"))
            && q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_clipboard_timestamp ON clipboard_items(timestamp DESC)"))
            && q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_clipboard_type ON clipboard_items(type)"));
}

qint64 StorageManager::addItem(const QVariantMap &item)
{
    if (!isOpen()) {
        return -1;
    }

    const QString content = item.value(QStringLiteral("content")).toString();
    const QString mimeType = item.value(QStringLiteral("mimeType")).toString();
    const QString type = item.value(QStringLiteral("type")).toString();
    const qint64 nowMs = item.value(QStringLiteral("timestamp")).toLongLong();
    const qint64 ts = nowMs > 0 ? nowMs : QDateTime::currentMSecsSinceEpoch();

    // Lightweight dedupe: refresh latest identical content row instead of growing history.
    QSqlQuery check(m_db);
    check.prepare(QStringLiteral(
        "SELECT id FROM clipboard_items "
        "WHERE content = :content AND mimeType = :mime AND type = :type "
        "ORDER BY id DESC LIMIT 1"));
    check.bindValue(QStringLiteral(":content"), content);
    check.bindValue(QStringLiteral(":mime"), mimeType);
    check.bindValue(QStringLiteral(":type"), type);
    if (check.exec() && check.next()) {
        const qint64 existingId = check.value(0).toLongLong();
        QSqlQuery up(m_db);
        up.prepare(QStringLiteral("UPDATE clipboard_items SET timestamp=:ts, preview=:preview, source_app=:source WHERE id=:id"));
        up.bindValue(QStringLiteral(":ts"), ts);
        up.bindValue(QStringLiteral(":preview"), item.value(QStringLiteral("preview")).toString());
        up.bindValue(QStringLiteral(":source"), item.value(QStringLiteral("sourceApplication")).toString());
        up.bindValue(QStringLiteral(":id"), existingId);
        up.exec();
        enforceLimits();
        return existingId;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO clipboard_items(content,mimeType,type,timestamp,source_app,preview,pinned) "
        "VALUES(:content,:mime,:type,:ts,:source,:preview,:pinned)"));
    q.bindValue(QStringLiteral(":content"), content);
    q.bindValue(QStringLiteral(":mime"), mimeType);
    q.bindValue(QStringLiteral(":type"), type);
    q.bindValue(QStringLiteral(":ts"), ts);
    q.bindValue(QStringLiteral(":source"), item.value(QStringLiteral("sourceApplication")).toString());
    q.bindValue(QStringLiteral(":preview"), item.value(QStringLiteral("preview")).toString());
    q.bindValue(QStringLiteral(":pinned"), item.value(QStringLiteral("isPinned")).toBool() ? 1 : 0);
    if (!q.exec()) {
        return -1;
    }
    enforceLimits();
    return q.lastInsertId().toLongLong();
}

QVariantMap StorageManager::itemById(qint64 id) const
{
    QVariantMap out;
    if (!isOpen() || id <= 0) {
        return out;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id,content,mimeType,type,timestamp,source_app,preview,pinned "
        "FROM clipboard_items WHERE id=:id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec() || !q.next()) {
        return out;
    }
    out.insert(QStringLiteral("id"), q.value(0).toLongLong());
    out.insert(QStringLiteral("content"), q.value(1).toString());
    out.insert(QStringLiteral("mimeType"), q.value(2).toString());
    out.insert(QStringLiteral("type"), q.value(3).toString());
    out.insert(QStringLiteral("timestamp"), q.value(4).toLongLong());
    out.insert(QStringLiteral("sourceApplication"), q.value(5).toString());
    out.insert(QStringLiteral("preview"), q.value(6).toString());
    out.insert(QStringLiteral("isPinned"), q.value(7).toInt() != 0);
    return normalizeRecord(out);
}

QVariantList StorageManager::history(int limit) const
{
    QVariantList out;
    if (!isOpen()) {
        return out;
    }
    const int bounded = qBound(1, limit, 1000);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id,content,mimeType,type,timestamp,source_app,preview,pinned "
        "FROM clipboard_items ORDER BY pinned DESC, timestamp DESC LIMIT :limit"));
    q.bindValue(QStringLiteral(":limit"), bounded);
    if (!q.exec()) {
        return out;
    }
    while (q.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), q.value(0).toLongLong());
        row.insert(QStringLiteral("content"), q.value(1).toString());
        row.insert(QStringLiteral("mimeType"), q.value(2).toString());
        row.insert(QStringLiteral("type"), q.value(3).toString());
        row.insert(QStringLiteral("timestamp"), q.value(4).toLongLong());
        row.insert(QStringLiteral("sourceApplication"), q.value(5).toString());
        row.insert(QStringLiteral("preview"), q.value(6).toString());
        row.insert(QStringLiteral("isPinned"), q.value(7).toInt() != 0);
        out.push_back(normalizeRecord(row));
    }
    return out;
}

QVariantList StorageManager::search(const QString &query, int limit) const
{
    QVariantList out;
    if (!isOpen()) {
        return out;
    }
    const QString needle = query.trimmed();
    if (needle.isEmpty()) {
        return history(limit);
    }
    const int bounded = qBound(1, limit, 1000);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id,content,mimeType,type,timestamp,source_app,preview,pinned "
        "FROM clipboard_items "
        "WHERE preview LIKE :needle OR content LIKE :needle "
        "ORDER BY pinned DESC, timestamp DESC LIMIT :limit"));
    q.bindValue(QStringLiteral(":needle"), QStringLiteral("%") + needle + QStringLiteral("%"));
    q.bindValue(QStringLiteral(":limit"), bounded);
    if (!q.exec()) {
        return out;
    }
    while (q.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), q.value(0).toLongLong());
        row.insert(QStringLiteral("content"), q.value(1).toString());
        row.insert(QStringLiteral("mimeType"), q.value(2).toString());
        row.insert(QStringLiteral("type"), q.value(3).toString());
        row.insert(QStringLiteral("timestamp"), q.value(4).toLongLong());
        row.insert(QStringLiteral("sourceApplication"), q.value(5).toString());
        row.insert(QStringLiteral("preview"), q.value(6).toString());
        row.insert(QStringLiteral("isPinned"), q.value(7).toInt() != 0);
        out.push_back(normalizeRecord(row));
    }
    return out;
}

bool StorageManager::setPinned(qint64 id, bool pinned)
{
    if (!isOpen() || id <= 0) {
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE clipboard_items SET pinned=:p WHERE id=:id"));
    q.bindValue(QStringLiteral(":p"), pinned ? 1 : 0);
    q.bindValue(QStringLiteral(":id"), id);
    const bool ok = q.exec();
    if (ok) {
        enforceLimits();
    }
    return ok;
}

bool StorageManager::deleteItem(qint64 id)
{
    if (!isOpen() || id <= 0) {
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM clipboard_items WHERE id=:id"));
    q.bindValue(QStringLiteral(":id"), id);
    return q.exec();
}

bool StorageManager::clearHistory()
{
    if (!isOpen()) {
        return false;
    }
    QSqlQuery q(m_db);
    return q.exec(QStringLiteral("DELETE FROM clipboard_items WHERE pinned=0"));
}

void StorageManager::enforceLimits()
{
    pruneType(QString::fromLatin1(kTypeImage), 100);
    pruneType(QString::fromLatin1(kTypeFile), 50);
}

void StorageManager::pruneType(const QString &type, int maxCount)
{
    if (!isOpen() || maxCount < 1) {
        return;
    }
    QSqlQuery countQ(m_db);
    countQ.prepare(QStringLiteral("SELECT COUNT(1) FROM clipboard_items WHERE type=:type AND pinned=0"));
    countQ.bindValue(QStringLiteral(":type"), type);
    if (!countQ.exec() || !countQ.next()) {
        return;
    }
    const int count = countQ.value(0).toInt();
    if (count <= maxCount) {
        return;
    }
    const int toDelete = count - maxCount;
    QSqlQuery del(m_db);
    del.prepare(QStringLiteral(
        "DELETE FROM clipboard_items WHERE id IN ("
        "SELECT id FROM clipboard_items WHERE type=:type AND pinned=0 "
        "ORDER BY timestamp ASC LIMIT :lim)"));
    del.bindValue(QStringLiteral(":type"), type);
    del.bindValue(QStringLiteral(":lim"), toDelete);
    del.exec();
}

} // namespace Slm::Clipboard
