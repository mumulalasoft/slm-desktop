#include "settingssqlstore.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace Slm {

static const QString kConnectionName = QStringLiteral("slm-settingsd");

SettingsSqlStore::SettingsSqlStore() = default;

SettingsSqlStore::~SettingsSqlStore()
{
    if (m_db.isOpen())
        m_db.close();
    // Release our copy of the handle before removing — avoids the "still in use" warning.
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(kConnectionName);
}

// ── Open / Schema ─────────────────────────────────────────────────────────────

bool SettingsSqlStore::open(QString *error)
{
    const QString path = dbPath();
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error)
            *error = QStringLiteral("cannot create settings directory: %1").arg(info.absolutePath());
        return false;
    }

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnectionName);
    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        if (error)
            *error = QStringLiteral("cannot open settings DB: %1").arg(m_db.lastError().text());
        return false;
    }

    if (!enableWal()) {
        // Non-fatal — WAL is a performance hint.
        qWarning("settingsd: WAL mode not available for settings DB");
    }

    if (!initSchema(error)) {
        m_db.close();
        return false;
    }

    loadRevision();
    return true;
}

bool SettingsSqlStore::isOpen() const
{
    return m_db.isValid() && m_db.isOpen();
}

bool SettingsSqlStore::enableWal()
{
    QSqlQuery q(m_db);
    return q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
}

bool SettingsSqlStore::initSchema(QString *error)
{
    QSqlQuery q(m_db);
    const bool ok =
        q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS settings ("
            "  key        TEXT    PRIMARY KEY NOT NULL,"
            "  type       TEXT    NOT NULL,"
            "  value_string TEXT,"
            "  value_int    INTEGER,"
            "  value_real   REAL,"
            "  value_bool   INTEGER,"
            "  source     TEXT    NOT NULL DEFAULT 'user',"
            "  updated_at TEXT    NOT NULL,"
            "  revision   INTEGER NOT NULL DEFAULT 1"
            ")"))
        && q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS meta ("
            "  name  TEXT PRIMARY KEY NOT NULL,"
            "  value TEXT NOT NULL"
            ")"))
        && q.exec(QStringLiteral(
            "INSERT OR IGNORE INTO meta (name, value) VALUES ('db_version', '1')"))
        && q.exec(QStringLiteral(
            "INSERT OR IGNORE INTO meta (name, value) VALUES ('revision', '0')"));

    if (!ok) {
        if (error)
            *error = QStringLiteral("settings DB schema init failed: %1").arg(q.lastError().text());
    }
    return ok;
}

bool SettingsSqlStore::loadRevision()
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT value FROM meta WHERE name = 'revision'"));
    if (!q.exec() || !q.next())
        return false;
    m_revision = q.value(0).toString().toULongLong();
    return true;
}

bool SettingsSqlStore::bumpRevision(QString *error)
{
    ++m_revision;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE meta SET value = ? WHERE name = 'revision'"));
    q.addBindValue(QString::number(m_revision));
    if (!q.exec()) {
        if (error)
            *error = QStringLiteral("cannot update revision: %1").arg(q.lastError().text());
        return false;
    }
    return true;
}

// ── Read ──────────────────────────────────────────────────────────────────────

QVariant SettingsSqlStore::get(const QString &key) const
{
    if (!isOpen())
        return {};

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT type, value_string, value_int, value_real, value_bool"
        " FROM settings WHERE key = ?"));
    q.addBindValue(key);
    if (!q.exec() || !q.next())
        return {};

    const QString type = q.value(0).toString();
    if (type == QLatin1String("bool"))
        return q.value(4).toBool();
    if (type == QLatin1String("int"))
        return q.value(2).toInt();
    if (type == QLatin1String("real"))
        return q.value(3).toDouble();
    return q.value(1).toString();
}

QVariantMap SettingsSqlStore::loadAll() const
{
    QVariantMap result;
    if (!isOpen())
        return result;

    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT key, type, value_string, value_int, value_real, value_bool"
            " FROM settings")))
        return result;

    while (q.next()) {
        const QString key  = q.value(0).toString();
        const QString type = q.value(1).toString();
        QVariant v;
        if (type == QLatin1String("bool"))
            v = q.value(5).toBool();
        else if (type == QLatin1String("int"))
            v = q.value(3).toInt();
        else if (type == QLatin1String("real"))
            v = q.value(4).toDouble();
        else
            v = q.value(2).toString();
        result.insert(key, v);
    }
    return result;
}

quint64 SettingsSqlStore::revision() const
{
    return m_revision;
}

// ── Write ─────────────────────────────────────────────────────────────────────

QString SettingsSqlStore::typeString(const QVariant &v)
{
    switch (v.metaType().id()) {
    case QMetaType::Bool:       return QStringLiteral("bool");
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::UInt:
    case QMetaType::ULongLong:  return QStringLiteral("int");
    case QMetaType::Double:
    case QMetaType::Float:      return QStringLiteral("real");
    default:                    return QStringLiteral("string");
    }
}

bool SettingsSqlStore::writeRow(const QString &key, const QVariant &value,
                                quint64 rev, QString *error)
{
    const QString type = typeString(value);
    const QString now  = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO settings"
        "  (key, type, value_string, value_int, value_real, value_bool, source, updated_at, revision)"
        " VALUES (?, ?, ?, ?, ?, ?, 'user', ?, ?)"
        " ON CONFLICT(key) DO UPDATE SET"
        "  type=excluded.type,"
        "  value_string=excluded.value_string,"
        "  value_int=excluded.value_int,"
        "  value_real=excluded.value_real,"
        "  value_bool=excluded.value_bool,"
        "  source=excluded.source,"
        "  updated_at=excluded.updated_at,"
        "  revision=excluded.revision"));

    q.addBindValue(key);
    q.addBindValue(type);

    if (type == QLatin1String("string"))
        q.addBindValue(value.toString());
    else
        q.addBindValue(QVariant{});

    if (type == QLatin1String("int"))
        q.addBindValue(value.toInt());
    else
        q.addBindValue(QVariant{});

    if (type == QLatin1String("real"))
        q.addBindValue(value.toDouble());
    else
        q.addBindValue(QVariant{});

    if (type == QLatin1String("bool"))
        q.addBindValue(value.toBool() ? 1 : 0);
    else
        q.addBindValue(QVariant{});

    q.addBindValue(now);
    q.addBindValue(static_cast<qulonglong>(rev));

    if (!q.exec()) {
        if (error)
            *error = QStringLiteral("write failed for '%1': %2")
                         .arg(key, q.lastError().text());
        return false;
    }
    return true;
}

bool SettingsSqlStore::set(const QString &key, const QVariant &value, QString *error)
{
    if (!isOpen()) {
        if (error) *error = QStringLiteral("DB not open");
        return false;
    }

    m_db.transaction();
    if (!bumpRevision(error) || !writeRow(key, value, m_revision, error)) {
        m_db.rollback();
        --m_revision;
        return false;
    }
    if (!m_db.commit()) {
        if (error)
            *error = QStringLiteral("transaction commit failed: %1")
                         .arg(m_db.lastError().text());
        m_db.rollback();
        --m_revision;
        return false;
    }
    return true;
}

bool SettingsSqlStore::setMany(const QVariantMap &kvs, QStringList *changed,
                               QString *error)
{
    if (!isOpen()) {
        if (error) *error = QStringLiteral("DB not open");
        return false;
    }
    if (kvs.isEmpty())
        return true;

    m_db.transaction();
    if (!bumpRevision(error)) {
        m_db.rollback();
        --m_revision;
        return false;
    }

    QStringList localChanged;
    for (auto it = kvs.constBegin(); it != kvs.constEnd(); ++it) {
        const QVariant current = get(it.key());
        if (current == it.value())
            continue;
        if (!writeRow(it.key(), it.value(), m_revision, error)) {
            m_db.rollback();
            --m_revision;
            return false;
        }
        localChanged.append(it.key());
    }

    if (!m_db.commit()) {
        if (error)
            *error = QStringLiteral("transaction commit failed: %1")
                         .arg(m_db.lastError().text());
        m_db.rollback();
        --m_revision;
        return false;
    }

    if (changed)
        *changed = localChanged;
    return true;
}

bool SettingsSqlStore::importFlat(const QVariantMap &flat, QString *error)
{
    return setMany(flat, nullptr, error);
}

// ── Path ─────────────────────────────────────────────────────────────────────

QString SettingsSqlStore::dbPath()
{
    // Explicit DB path override (production or test).
    const QString dbOverride = qEnvironmentVariable("SLM_SETTINGSD_DB_PATH").trimmed();
    if (!dbOverride.isEmpty())
        return dbOverride;

    // If the legacy JSON env var is set (test environments), derive a sibling .db path
    // so each test case gets its own isolated DB file.
    const QString legacyPath = qEnvironmentVariable("SLM_SETTINGSD_STORE_PATH").trimmed();
    if (!legacyPath.isEmpty()) {
        QString p = legacyPath;
        if (p.endsWith(QStringLiteral(".json")))
            p.chop(5);
        return p + QStringLiteral(".db");
    }

    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                         + QStringLiteral("/slm/config");
    return base + QStringLiteral("/settings.db");
}

} // namespace Slm
