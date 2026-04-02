#include "PermissionStore.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace Slm::Permissions {

PermissionStore::PermissionStore()
    : m_connectionName(QStringLiteral("slm_permissions_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

PermissionStore::~PermissionStore()
{
    const QString connection = m_connectionName;
    if (m_db.isValid()) {
        m_db.close();
    }
    m_db = QSqlDatabase();
    if (!connection.isEmpty()) {
        QSqlDatabase::removeDatabase(connection);
    }
}

bool PermissionStore::open(const QString &dbPath)
{
    m_databasePath = dbPath.trimmed().isEmpty() ? defaultPath() : dbPath.trimmed();
    return ensureConnection() && ensureSchema();
}

bool PermissionStore::isOpen() const
{
    return m_db.isValid() && m_db.isOpen();
}

QString PermissionStore::databasePath() const
{
    return m_databasePath;
}

bool PermissionStore::ensureConnection()
{
    if (m_databasePath.isEmpty()) {
        m_databasePath = defaultPath();
    }
    QFileInfo fi(m_databasePath);
    QDir().mkpath(fi.absolutePath());

    if (!m_db.isValid()) {
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        m_db.setDatabaseName(m_databasePath);
    }
    if (!m_db.isOpen()) {
        if (!m_db.open()) {
            return false;
        }
    }
    return true;
}

bool PermissionStore::ensureSchema()
{
    if (!ensureConnection()) {
        return false;
    }
    QSqlQuery q(m_db);
    const QStringList ddl{
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS permissions ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "app_id TEXT NOT NULL,"
            "capability TEXT NOT NULL,"
            "decision TEXT NOT NULL,"
            "scope TEXT NOT NULL,"
            "resource_type TEXT,"
            "resource_id TEXT,"
            "created_at INTEGER NOT NULL,"
            "updated_at INTEGER NOT NULL"
            ")"),
        QStringLiteral(
            "CREATE UNIQUE INDEX IF NOT EXISTS idx_permissions_unique "
            "ON permissions(app_id, capability, COALESCE(resource_type, ''), COALESCE(resource_id, ''))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS app_registry ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "app_id TEXT NOT NULL UNIQUE,"
            "desktop_file_id TEXT,"
            "executable_path TEXT,"
            "trust_level TEXT NOT NULL,"
            "is_official INTEGER NOT NULL DEFAULT 0,"
            "updated_at INTEGER NOT NULL"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS audit_log ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "app_id TEXT NOT NULL,"
            "capability TEXT NOT NULL,"
            "decision TEXT NOT NULL,"
            "resource_type TEXT,"
            "timestamp INTEGER NOT NULL,"
            "reason TEXT"
            ")")
    };
    for (const QString &sql : ddl) {
        if (!q.exec(sql)) {
            return false;
        }
    }
    return true;
}

bool PermissionStore::upsertAppRegistry(const CallerIdentity &caller)
{
    if (!ensureConnection()) {
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO app_registry(app_id, desktop_file_id, executable_path, trust_level, is_official, updated_at)"
        "VALUES(:app_id, :desktop_file_id, :executable_path, :trust_level, :is_official, :updated_at)"
        "ON CONFLICT(app_id) DO UPDATE SET "
        "desktop_file_id=excluded.desktop_file_id,"
        "executable_path=excluded.executable_path,"
        "trust_level=excluded.trust_level,"
        "is_official=excluded.is_official,"
        "updated_at=excluded.updated_at"));
    q.bindValue(QStringLiteral(":app_id"), caller.appId);
    q.bindValue(QStringLiteral(":desktop_file_id"), caller.desktopFileId);
    q.bindValue(QStringLiteral(":executable_path"), caller.executablePath);
    q.bindValue(QStringLiteral(":trust_level"), trustLevelToString(caller.trustLevel));
    q.bindValue(QStringLiteral(":is_official"), caller.isOfficialComponent ? 1 : 0);
    q.bindValue(QStringLiteral(":updated_at"), QDateTime::currentMSecsSinceEpoch());
    return q.exec();
}

StoredPermission PermissionStore::findPermission(const QString &appId,
                                                 Capability capability,
                                                 const QString &resourceType,
                                                 const QString &resourceId) const
{
    StoredPermission out;
    if (!isOpen()) {
        return out;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT app_id, capability, decision, scope, resource_type, resource_id, updated_at "
        "FROM permissions "
        "WHERE app_id=:app_id AND capability=:capability "
        "AND COALESCE(resource_type,'')=COALESCE(:resource_type,'') "
        "AND COALESCE(resource_id,'')=COALESCE(:resource_id,'') "
        "LIMIT 1"));
    q.bindValue(QStringLiteral(":app_id"), appId.trimmed());
    q.bindValue(QStringLiteral(":capability"), capabilityToString(capability));
    q.bindValue(QStringLiteral(":resource_type"), resourceType.trimmed());
    q.bindValue(QStringLiteral(":resource_id"), resourceId.trimmed());
    if (!q.exec() || !q.next()) {
        return out;
    }
    out.appId = q.value(0).toString();
    out.capability = capabilityFromString(q.value(1).toString());
    out.decision = decisionTypeFromString(q.value(2).toString());
    out.scope = q.value(3).toString();
    out.resourceType = q.value(4).toString();
    out.resourceId = q.value(5).toString();
    out.updatedAt = QDateTime::fromMSecsSinceEpoch(q.value(6).toLongLong());
    out.valid = true;
    return out;
}

bool PermissionStore::savePermission(const QString &appId,
                                     Capability capability,
                                     DecisionType decision,
                                     const QString &scope,
                                     const QString &resourceType,
                                     const QString &resourceId)
{
    if (!ensureConnection()) {
        return false;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QSqlQuery update(m_db);
    update.prepare(QStringLiteral(
        "UPDATE permissions SET decision=:decision, scope=:scope, updated_at=:updated_at "
        "WHERE app_id=:app_id AND capability=:capability "
        "AND COALESCE(resource_type,'')=COALESCE(:resource_type,'') "
        "AND COALESCE(resource_id,'')=COALESCE(:resource_id,'')"));
    update.bindValue(QStringLiteral(":decision"), decisionTypeToString(decision));
    update.bindValue(QStringLiteral(":scope"), scope.trimmed());
    update.bindValue(QStringLiteral(":updated_at"), now);
    update.bindValue(QStringLiteral(":app_id"), appId.trimmed());
    update.bindValue(QStringLiteral(":capability"), capabilityToString(capability));
    update.bindValue(QStringLiteral(":resource_type"), resourceType.trimmed());
    update.bindValue(QStringLiteral(":resource_id"), resourceId.trimmed());
    if (!update.exec()) {
        return false;
    }
    if (update.numRowsAffected() > 0) {
        return true;
    }

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral(
        "INSERT INTO permissions(app_id, capability, decision, scope, resource_type, resource_id, created_at, updated_at)"
        "VALUES(:app_id, :capability, :decision, :scope, :resource_type, :resource_id, :created_at, :updated_at)"));
    insert.bindValue(QStringLiteral(":app_id"), appId.trimmed());
    insert.bindValue(QStringLiteral(":capability"), capabilityToString(capability));
    insert.bindValue(QStringLiteral(":decision"), decisionTypeToString(decision));
    insert.bindValue(QStringLiteral(":scope"), scope.trimmed());
    insert.bindValue(QStringLiteral(":resource_type"), resourceType.trimmed());
    insert.bindValue(QStringLiteral(":resource_id"), resourceId.trimmed());
    insert.bindValue(QStringLiteral(":created_at"), now);
    insert.bindValue(QStringLiteral(":updated_at"), now);
    return insert.exec();
}

bool PermissionStore::removePermission(const QString &appId,
                                       Capability capability,
                                       const QString &resourceType,
                                       const QString &resourceId)
{
    if (!ensureConnection()) {
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "DELETE FROM permissions "
        "WHERE app_id=:app_id AND capability=:capability "
        "AND COALESCE(resource_type,'')=COALESCE(:resource_type,'') "
        "AND COALESCE(resource_id,'')=COALESCE(:resource_id,'')"));
    q.bindValue(QStringLiteral(":app_id"), appId.trimmed());
    q.bindValue(QStringLiteral(":capability"), capabilityToString(capability));
    q.bindValue(QStringLiteral(":resource_type"), resourceType.trimmed());
    q.bindValue(QStringLiteral(":resource_id"), resourceId.trimmed());
    return q.exec();
}

bool PermissionStore::appendAudit(const QString &appId,
                                  Capability capability,
                                  DecisionType decision,
                                  const QString &resourceType,
                                  const QString &reason)
{
    if (!ensureConnection()) {
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO audit_log(app_id, capability, decision, resource_type, timestamp, reason)"
        "VALUES(:app_id, :capability, :decision, :resource_type, :timestamp, :reason)"));
    q.bindValue(QStringLiteral(":app_id"), appId.trimmed());
    q.bindValue(QStringLiteral(":capability"), capabilityToString(capability));
    q.bindValue(QStringLiteral(":decision"), decisionTypeToString(decision));
    q.bindValue(QStringLiteral(":resource_type"), resourceType.trimmed());
    q.bindValue(QStringLiteral(":timestamp"), QDateTime::currentMSecsSinceEpoch());
    q.bindValue(QStringLiteral(":reason"), reason.left(512));
    return q.exec();
}

QVariantList PermissionStore::listPermissions() const
{
    QVariantList out;
    if (!isOpen()) {
        return out;
    }
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT app_id, capability, decision, scope, resource_type, resource_id, updated_at "
            "FROM permissions ORDER BY app_id ASC, capability ASC"))) {
        return out;
    }
    while (q.next()) {
        out.push_back(QVariantMap{
            {QStringLiteral("appId"), q.value(0).toString()},
            {QStringLiteral("capability"), q.value(1).toString()},
            {QStringLiteral("decision"), q.value(2).toString()},
            {QStringLiteral("scope"), q.value(3).toString()},
            {QStringLiteral("resourceType"), q.value(4).toString()},
            {QStringLiteral("resourceId"), q.value(5).toString()},
            {QStringLiteral("updatedAt"), q.value(6).toLongLong()},
        });
    }
    return out;
}

QVariantList PermissionStore::listAuditLogs(int limit) const
{
    QVariantList out;
    if (!isOpen()) {
        return out;
    }
    const int capped = qBound(1, limit, 5000);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT app_id, capability, decision, resource_type, timestamp, reason "
        "FROM audit_log ORDER BY id DESC LIMIT :limit"));
    q.bindValue(QStringLiteral(":limit"), capped);
    if (!q.exec()) {
        return out;
    }
    while (q.next()) {
        out.push_back(QVariantMap{
            {QStringLiteral("appId"), q.value(0).toString()},
            {QStringLiteral("capability"), q.value(1).toString()},
            {QStringLiteral("decision"), q.value(2).toString()},
            {QStringLiteral("resourceType"), q.value(3).toString()},
            {QStringLiteral("timestamp"), q.value(4).toLongLong()},
            {QStringLiteral("reason"), q.value(5).toString()},
        });
    }
    return out;
}

bool PermissionStore::resetPermissionsForApp(const QString &appId)
{
    if (!ensureConnection()) {
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM permissions WHERE app_id=:app_id"));
    q.bindValue(QStringLiteral(":app_id"), appId.trimmed());
    return q.exec();
}

bool PermissionStore::resetAllPermissions()
{
    if (!ensureConnection()) {
        return false;
    }
    QSqlQuery q(m_db);
    return q.exec(QStringLiteral("DELETE FROM permissions"));
}

QString PermissionStore::defaultPath() const
{
    return QDir::home().filePath(QStringLiteral(".local/share/slm-desktop/permissions.db"));
}

} // namespace Slm::Permissions
