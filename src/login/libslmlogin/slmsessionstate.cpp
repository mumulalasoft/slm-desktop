#include "slmsessionstate.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace Slm::Login {

// ── SessionState ────────────────────────────────────────────────────────────

SessionState SessionState::defaults()
{
    SessionState s;
    s.lastUpdated = QDateTime::currentDateTimeUtc();
    return s;
}

QVariantMap SessionState::toMap() const
{
    return {
        {QStringLiteral("crash_count"),        crashCount},
        {QStringLiteral("last_mode"),          startupModeToString(lastMode)},
        {QStringLiteral("last_good_snapshot"), lastGoodSnapshot},
        {QStringLiteral("safe_mode_forced"),   safeModeForced},
        {QStringLiteral("config_pending"),     configPending},
        {QStringLiteral("recovery_reason"),    recoveryReason},
        {QStringLiteral("last_boot_status"),   lastBootStatus},
        {QStringLiteral("last_updated"),       lastUpdated.toString(Qt::ISODate)},
    };
}

SessionState SessionState::fromMap(const QVariantMap &map)
{
    SessionState s;
    s.crashCount       = map.value(QStringLiteral("crash_count"), 0).toInt();
    s.lastMode         = startupModeFromString(
        map.value(QStringLiteral("last_mode"), QStringLiteral("normal")).toString());
    s.lastGoodSnapshot = map.value(QStringLiteral("last_good_snapshot")).toString();
    s.safeModeForced   = map.value(QStringLiteral("safe_mode_forced"), false).toBool();
    s.configPending    = map.value(QStringLiteral("config_pending"), false).toBool();
    s.recoveryReason   = map.value(QStringLiteral("recovery_reason")).toString();
    s.lastBootStatus   = map.value(QStringLiteral("last_boot_status")).toString();
    const QString dt   = map.value(QStringLiteral("last_updated")).toString();
    s.lastUpdated      = dt.isEmpty() ? QDateTime::currentDateTimeUtc()
                                      : QDateTime::fromString(dt, Qt::ISODate);
    return s;
}

// ── SessionStateIO ───────────────────────────────────────────────────────────

QString SessionStateIO::statePath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                         + QStringLiteral("/slm-desktop");
    return base + QStringLiteral("/state.json");
}

bool SessionStateIO::load(SessionState &out, QString &outError)
{
    const QString path = statePath();
    QFile file(path);
    if (!file.exists()) {
        out = SessionState::defaults();
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        outError = QStringLiteral("cannot open state file: ") + path;
        out = SessionState::defaults();
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        outError = QStringLiteral("state parse error: ") + parseError.errorString();
        out = SessionState::defaults();
        return false;
    }
    out = SessionState::fromMap(doc.object().toVariantMap());
    return true;
}

bool SessionStateIO::save(const SessionState &state, QString &outError)
{
    const QString path = statePath();
    const QString dir  = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dir)) {
        outError = QStringLiteral("cannot create config dir: ") + dir;
        return false;
    }

    const QJsonObject obj  = QJsonObject::fromVariantMap(state.toMap());
    const QByteArray  data = QJsonDocument(obj).toJson(QJsonDocument::Indented);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        outError = QStringLiteral("cannot open state file for writing");
        return false;
    }
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        outError = QStringLiteral("state write truncated");
        return false;
    }
    if (!file.commit()) {
        outError = QStringLiteral("state atomic commit failed");
        return false;
    }
    return true;
}

} // namespace Slm::Login
