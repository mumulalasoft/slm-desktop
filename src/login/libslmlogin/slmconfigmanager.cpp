#include "slmconfigmanager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace Slm::Login {

// ── Static paths ─────────────────────────────────────────────────────────────

QString ConfigManager::configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/slm-desktop");
}

QString ConfigManager::activePath()   { return configDir() + QStringLiteral("/config.json"); }
QString ConfigManager::prevPath()     { return configDir() + QStringLiteral("/config.prev.json"); }
QString ConfigManager::safePath()     { return configDir() + QStringLiteral("/config.safe.json"); }
QString ConfigManager::snapshotDir()  { return configDir() + QStringLiteral("/snapshots"); }

QString ConfigManager::snapshotPath(const QString &id)
{
    return snapshotDir() + QStringLiteral("/") + id + QStringLiteral(".json");
}

// ── Constructor ───────────────────────────────────────────────────────────────

ConfigManager::ConfigManager()
{
    QDir().mkpath(configDir());
    QDir().mkpath(snapshotDir());
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QJsonObject ConfigManager::loadFile(const QString &path)
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return QJsonObject{};
    }
    const QByteArray data = file.readAll();
    file.close();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject()) {
        return doc.object();
    }
    return QJsonObject{};
}

QJsonObject ConfigManager::defaultConfig()
{
    return {
        {QStringLiteral("compositor"),     QStringLiteral("kwin_wayland")},
        {QStringLiteral("compositorArgs"), QJsonArray{}},
        {QStringLiteral("shell"),          QStringLiteral("slm-shell")},
        {QStringLiteral("shellArgs"),      QJsonArray{}},
    };
}

bool ConfigManager::validateConfig(const QJsonObject &config,
                                   QString *error,
                                   QJsonObject *normalized)
{
    QJsonObject out = defaultConfig();
    for (auto it = config.begin(); it != config.end(); ++it) {
        out.insert(it.key(), it.value());
    }

    auto fail = [&](const QString &why) {
        if (error) {
            *error = why;
        }
        return false;
    };

    const auto isCommandLike = [](const QString &value) {
        if (value.trimmed().isEmpty()) {
            return false;
        }
        // Absolute paths are allowed.
        if (value.startsWith(QLatin1Char('/'))) {
            return true;
        }
        static const QRegularExpression bareNameRx(QStringLiteral("^[A-Za-z0-9._+\\-]+$"));
        return bareNameRx.match(value).hasMatch();
    };

    const QJsonValue compositorVal = out.value(QStringLiteral("compositor"));
    if (!compositorVal.isString() || !isCommandLike(compositorVal.toString())) {
        return fail(QStringLiteral("invalid compositor value"));
    }
    const QJsonValue shellVal = out.value(QStringLiteral("shell"));
    if (!shellVal.isString() || !isCommandLike(shellVal.toString())) {
        return fail(QStringLiteral("invalid shell value"));
    }

    const auto validateArgsArray = [&](const char *key) {
        const QJsonValue v = out.value(QLatin1String(key));
        if (!v.isArray()) {
            return fail(QStringLiteral("%1 must be an array").arg(QLatin1String(key)));
        }
        const QJsonArray arr = v.toArray();
        if (arr.size() > 64) {
            return fail(QStringLiteral("%1 has too many entries").arg(QLatin1String(key)));
        }
        for (const QJsonValue &entry : arr) {
            if (!entry.isString()) {
                return fail(QStringLiteral("%1 entries must be strings").arg(QLatin1String(key)));
            }
            if (entry.toString().size() > 512) {
                return fail(QStringLiteral("%1 entry too long").arg(QLatin1String(key)));
            }
        }
        return true;
    };
    if (!validateArgsArray("compositorArgs")) {
        return false;
    }
    if (!validateArgsArray("shellArgs")) {
        return false;
    }

    if (out.contains(QStringLiteral("uiScale"))) {
        const QJsonValue uiScale = out.value(QStringLiteral("uiScale"));
        if (!uiScale.isDouble()) {
            return fail(QStringLiteral("uiScale must be a number"));
        }
        const double scale = uiScale.toDouble();
        if (scale < 0.5 || scale > 2.0) {
            return fail(QStringLiteral("uiScale out of range"));
        }
    }

    if (normalized) {
        *normalized = out;
    }
    return true;
}

bool ConfigManager::atomicWriteJson(const QString &path,
                                    const QJsonObject &obj,
                                    QString *error)
{
    const QString dir = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dir)) {
        if (error) *error = QStringLiteral("cannot create dir: ") + dir;
        return false;
    }
    const QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = QStringLiteral("cannot open for write: ") + path;
        return false;
    }
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        if (error) *error = QStringLiteral("write truncated: ") + path;
        return false;
    }
    if (!file.commit()) {
        if (error) *error = QStringLiteral("atomic commit failed: ") + path;
        return false;
    }
    return true;
}

bool ConfigManager::atomicCopy(const QString &src, const QString &dst, QString *error)
{
    const QJsonObject obj = loadFile(src);
    if (obj.isEmpty() && !QFile::exists(src)) {
        if (error) *error = QStringLiteral("source not found: ") + src;
        return false;
    }
    return atomicWriteJson(dst, obj, error);
}

// ── Public API ────────────────────────────────────────────────────────────────

bool ConfigManager::load()
{
    QString validationError;
    QJsonObject normalized;
    const QJsonObject active = loadFile(activePath());
    if (!active.isEmpty() && validateConfig(active, &validationError, &normalized)) {
        m_config = normalized;
        return true;
    }

    if (!active.isEmpty()) {
        qWarning("slm-configmanager: invalid active config: %s", qUtf8Printable(validationError));
    }

    const QJsonObject safe = loadFile(safePath());
    if (!safe.isEmpty() && validateConfig(safe, &validationError, &normalized)) {
        m_config = normalized;
        QString err;
        atomicWriteJson(activePath(), m_config, &err);
        return true;
    }

    const QJsonObject prev = loadFile(prevPath());
    if (!prev.isEmpty() && validateConfig(prev, &validationError, &normalized)) {
        m_config = normalized;
        QString err;
        atomicWriteJson(activePath(), m_config, &err);
        return true;
    }

    m_config = defaultConfig();
    QString err;
    atomicWriteJson(activePath(), m_config, &err);
    return true; // silent on missing file
}

bool ConfigManager::save(const QJsonObject &config, QString *error)
{
    QJsonObject normalized;
    QString validationError;
    if (!validateConfig(config, &validationError, &normalized)) {
        if (error) {
            *error = validationError;
        }
        return false;
    }

    // Back up current active → prev before overwriting.
    if (QFile::exists(activePath())) {
        QString cpErr;
        if (!atomicCopy(activePath(), prevPath(), &cpErr)) {
            qWarning("slm-configmanager: backup to prev failed: %s", qUtf8Printable(cpErr));
            // Non-fatal — continue with save.
        }
    }
    if (!atomicWriteJson(activePath(), normalized, error)) {
        return false;
    }
    m_config = normalized;
    return true;
}

bool ConfigManager::rollbackToPrevious(QString *error)
{
    if (!hasPreviousConfig()) {
        if (error) *error = QStringLiteral("no previous config available");
        return false;
    }
    if (!atomicCopy(prevPath(), activePath(), error)) {
        return false;
    }
    m_config = loadFile(activePath());
    return true;
}

bool ConfigManager::rollbackToSafe(QString *error)
{
    if (!hasSafeConfig()) {
        // No safe config — use empty object (factory defaults).
        if (!atomicWriteJson(activePath(), QJsonObject{}, error)) {
            return false;
        }
        m_config = QJsonObject{};
        return true;
    }
    if (!atomicCopy(safePath(), activePath(), error)) {
        return false;
    }
    m_config = loadFile(activePath());
    return true;
}

QString ConfigManager::snapshot(QString *error)
{
    const QString id  = QDateTime::currentDateTimeUtc().toString(
        QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString dst = snapshotPath(id);
    if (!atomicCopy(activePath(), dst, error)) {
        return QString{};
    }
    return id;
}

bool ConfigManager::restoreSnapshot(const QString &id, QString *error)
{
    const QString src = snapshotPath(id);
    if (!QFile::exists(src)) {
        if (error) *error = QStringLiteral("snapshot not found: ") + id;
        return false;
    }
    if (!atomicCopy(src, activePath(), error)) {
        return false;
    }
    m_config = loadFile(activePath());
    return true;
}

bool ConfigManager::promoteToLastGood(QString *error)
{
    return atomicCopy(activePath(), safePath(), error);
}

bool ConfigManager::hasPreviousConfig() const { return QFile::exists(prevPath()); }
bool ConfigManager::hasSafeConfig()     const { return QFile::exists(safePath()); }

bool ConfigManager::factoryReset(QString *error)
{
    // 1. Remove all config variants.
    for (const QString &path : {activePath(), prevPath(), safePath()}) {
        if (QFile::exists(path) && !QFile::remove(path)) {
            if (error) *error = QStringLiteral("failed to remove: ") + path;
            return false;
        }
    }

    // 2. Remove all snapshots.
    QDir snapDir(snapshotDir());
    const QStringList snaps = snapDir.entryList(
        QStringList{QStringLiteral("*.json")}, QDir::Files);
    for (const QString &snap : snaps) {
        snapDir.remove(snap);
    }

    // 3. Write a minimal default config as the new active config.
    QJsonObject defaults = defaultConfig();
    defaults.insert(QStringLiteral("factoryReset"), true);
    defaults.insert(QStringLiteral("resetAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!atomicWriteJson(activePath(), defaults, error)) {
        return false;
    }
    m_config = defaults;
    return true;
}

QJsonObject ConfigManager::currentConfig() const { return m_config; }

QString ConfigManager::compositor() const
{
    return m_config.value(QStringLiteral("compositor")).toString(
        QStringLiteral("kwin_wayland"));
}

QStringList ConfigManager::compositorArgs() const
{
    const QJsonArray arr = m_config.value(QStringLiteral("compositorArgs")).toArray();
    QStringList out;
    for (const QJsonValue &v : arr) out.append(v.toString());
    return out;
}

QString ConfigManager::shell() const
{
    return m_config.value(QStringLiteral("shell")).toString(
        QStringLiteral("slm-shell"));
}

QStringList ConfigManager::shellArgs() const
{
    const QJsonArray arr = m_config.value(QStringLiteral("shellArgs")).toArray();
    QStringList out;
    for (const QJsonValue &v : arr) out.append(v.toString());
    return out;
}

} // namespace Slm::Login
