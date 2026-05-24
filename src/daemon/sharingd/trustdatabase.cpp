#include "trustdatabase.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

TrustDatabase::TrustDatabase(QObject *parent)
    : QObject(parent)
{}

TrustDatabase::~TrustDatabase()
{
    if (m_db.isOpen())
        m_db.close();
}

bool TrustDatabase::open()
{
    const QString path = dbPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("slm-sharingd-trust"));
    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        qWarning("slm-sharingd: trust db open failed: %s", qPrintable(m_db.lastError().text()));
        return false;
    }
    return createSchema();
}

bool TrustDatabase::upsertDevice(const QString &deviceId, const QVariantMap &info)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO devices (device_id, display_name, device_type, public_key_fingerprint, "
        "trust_level, paired_at, last_seen_at) "
        "VALUES (:id, :name, :type, :fp, :trust, :paired, datetime('now')) "
        "ON CONFLICT(device_id) DO UPDATE SET "
        "display_name=excluded.display_name, device_type=excluded.device_type, "
        "last_seen_at=excluded.last_seen_at"
    ));
    q.bindValue(QStringLiteral(":id"), deviceId);
    q.bindValue(QStringLiteral(":name"), info.value(QStringLiteral("name")));
    q.bindValue(QStringLiteral(":type"), info.value(QStringLiteral("type"), QStringLiteral("unknown")));
    q.bindValue(QStringLiteral(":fp"), info.value(QStringLiteral("publicKeyFingerprint")));
    q.bindValue(QStringLiteral(":trust"), trustLevelToString(TrustLevel::Untrusted));
    q.bindValue(QStringLiteral(":paired"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return q.exec();
}

bool TrustDatabase::removeDevice(const QString &deviceId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM devices WHERE device_id = :id"));
    q.bindValue(QStringLiteral(":id"), deviceId);
    q.exec();
    QSqlQuery p(m_db);
    p.prepare(QStringLiteral("DELETE FROM permissions WHERE device_id = :id"));
    p.bindValue(QStringLiteral(":id"), deviceId);
    return p.exec();
}

QVariantMap TrustDatabase::deviceInfo(const QString &deviceId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT device_id, display_name, device_type, public_key_fingerprint, "
        "trust_level, paired_at, last_seen_at FROM devices WHERE device_id = :id"
    ));
    q.bindValue(QStringLiteral(":id"), deviceId);
    if (!q.exec() || !q.next())
        return {};

    return {
        {QStringLiteral("deviceId"), q.value(0)},
        {QStringLiteral("name"), q.value(1)},
        {QStringLiteral("type"), q.value(2)},
        {QStringLiteral("publicKeyFingerprint"), q.value(3)},
        {QStringLiteral("trustLevel"), q.value(4)},
        {QStringLiteral("pairedAt"), q.value(5)},
        {QStringLiteral("lastSeenAt"), q.value(6)},
        {QStringLiteral("permissions"), permissions(deviceId)},
    };
}

QVariantList TrustDatabase::allDevices() const
{
    QVariantList result;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT device_id FROM devices ORDER BY last_seen_at DESC")))
        return result;

    while (q.next())
        result.append(deviceInfo(q.value(0).toString()));
    return result;
}

bool TrustDatabase::setTrustLevel(const QString &deviceId, TrustLevel level)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE devices SET trust_level = :level WHERE device_id = :id"));
    q.bindValue(QStringLiteral(":level"), trustLevelToString(level));
    q.bindValue(QStringLiteral(":id"), deviceId);
    return q.exec();
}

TrustDatabase::TrustLevel TrustDatabase::trustLevel(const QString &deviceId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT trust_level FROM devices WHERE device_id = :id"));
    q.bindValue(QStringLiteral(":id"), deviceId);
    if (!q.exec() || !q.next())
        return TrustLevel::Untrusted;
    return trustLevelFromString(q.value(0).toString());
}

QString TrustDatabase::trustLevelToString(TrustLevel level)
{
    switch (level) {
    case TrustLevel::Blocked:   return QStringLiteral("blocked");
    case TrustLevel::Untrusted: return QStringLiteral("untrusted");
    case TrustLevel::Trusted:   return QStringLiteral("trusted");
    case TrustLevel::Full:      return QStringLiteral("full");
    }
    return QStringLiteral("untrusted");
}

TrustDatabase::TrustLevel TrustDatabase::trustLevelFromString(const QString &value)
{
    if (value == QLatin1String("blocked"))   return TrustLevel::Blocked;
    if (value == QLatin1String("trusted"))   return TrustLevel::Trusted;
    if (value == QLatin1String("full"))      return TrustLevel::Full;
    return TrustLevel::Untrusted;
}

bool TrustDatabase::setPermission(const QString &deviceId, const QString &permission, bool allowed)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO permissions (device_id, permission, allowed) "
        "VALUES (:id, :perm, :allowed) "
        "ON CONFLICT(device_id, permission) DO UPDATE SET allowed=excluded.allowed"
    ));
    q.bindValue(QStringLiteral(":id"), deviceId);
    q.bindValue(QStringLiteral(":perm"), permission);
    q.bindValue(QStringLiteral(":allowed"), allowed ? 1 : 0);
    return q.exec();
}

QVariantMap TrustDatabase::permissions(const QString &deviceId) const
{
    QVariantMap result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT permission, allowed FROM permissions WHERE device_id = :id"));
    q.bindValue(QStringLiteral(":id"), deviceId);
    if (!q.exec())
        return result;
    while (q.next())
        result.insert(q.value(0).toString(), q.value(1).toBool());
    return result;
}

bool TrustDatabase::hasPermission(const QString &deviceId, const QString &permission) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT allowed FROM permissions WHERE device_id = :id AND permission = :perm"));
    q.bindValue(QStringLiteral(":id"), deviceId);
    q.bindValue(QStringLiteral(":perm"), permission);
    if (!q.exec() || !q.next())
        return false;
    return q.value(0).toBool();
}

bool TrustDatabase::updateLastSeen(const QString &deviceId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE devices SET last_seen_at = datetime('now') WHERE device_id = :id"));
    q.bindValue(QStringLiteral(":id"), deviceId);
    return q.exec();
}

bool TrustDatabase::createSchema()
{
    QSqlQuery q(m_db);
    const bool ok = q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS devices ("
        "  device_id TEXT PRIMARY KEY,"
        "  display_name TEXT NOT NULL DEFAULT '',"
        "  device_type TEXT NOT NULL DEFAULT 'unknown',"
        "  public_key_fingerprint TEXT,"
        "  trust_level TEXT NOT NULL DEFAULT 'untrusted',"
        "  paired_at TEXT,"
        "  last_seen_at TEXT"
        ")"
    )) && q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS permissions ("
        "  device_id TEXT NOT NULL,"
        "  permission TEXT NOT NULL,"
        "  allowed INTEGER NOT NULL DEFAULT 0,"
        "  PRIMARY KEY (device_id, permission),"
        "  FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE"
        ")"
    ));
    if (!ok)
        qWarning("slm-sharingd: schema creation failed: %s", qPrintable(q.lastError().text()));
    return ok;
}

QString TrustDatabase::dbPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QLatin1String("/slm-sharingd/trust.db");
}
